#include "engine2d/pona_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PONA_GC_MIN_THRESHOLD (32u * 1024u)

typedef enum { GC_OBJECT_STRING = 1, GC_OBJECT_ARRAY = 2 } GcObjectKind;
typedef enum { GC_IDLE = 0, GC_ROOTS, GC_MARK, GC_SWEEP } GcPhase;
typedef enum { GC_WHITE = 0, GC_GRAY, GC_BLACK } GcColor;

struct PonaObject {
    struct PonaObject *next;
    struct PonaObject *gray_next;
    size_t bytes;
    size_t scan_index;
    uint64_t mark_epoch;
    unsigned char kind;
    unsigned char color;
};

typedef struct {
    PonaObject base;
    size_t length;
    char data[];
} PonaString;

typedef struct {
    PonaObject base;
    size_t count;
    size_t capacity;
    PonaValue *items;
} PonaArray;

typedef struct {
    PonaObject *objects;
    PonaObject *gray;
    PonaObject *current;
    PonaObject **sweep;
    PonaObject ***roots;
    size_t root_count;
    size_t root_capacity;
    size_t root_cursor;
    size_t live_bytes;
    size_t live_objects;
    size_t threshold;
    size_t last_work;
    uint64_t epoch;
    GcPhase phase;
    int requested;
    int resume_sweep;
    int initialized;
} PonaGc;

static PonaGc gc;

void pona_gc_init(void) {
    if (gc.initialized) return;
    memset(&gc, 0, sizeof(gc));
    gc.threshold = PONA_GC_MIN_THRESHOLD;
    gc.epoch = 1;
    gc.initialized = 1;
}

static void mark_object(PonaObject *object) {
    if (!object || object->mark_epoch == gc.epoch) return;
    object->mark_epoch = gc.epoch;
    object->color = GC_GRAY;
    object->scan_index = 0;
    object->gray_next = gc.gray;
    gc.gray = object;
}

static void barrier_mark(PonaObject *object) {
    if (!object || gc.phase == GC_IDLE) return;
    if (gc.phase == GC_SWEEP && object->mark_epoch != gc.epoch) {
        gc.phase = GC_MARK;
        gc.current = NULL;
        gc.gray = NULL;
        gc.resume_sweep = 1;
    }
    mark_object(object);
}

static PonaObject *allocate_object(size_t bytes, GcObjectKind kind) {
    pona_gc_init();
    if (bytes < sizeof(PonaObject) || bytes > (size_t)-1 - gc.live_bytes) return NULL;
    PonaObject *object = (PonaObject *)calloc(1, bytes);
    if (!object) return NULL;
    object->bytes = bytes;
    object->kind = (unsigned char)kind;
    if (gc.phase != GC_IDLE) {
        object->mark_epoch = gc.epoch;
        object->color = GC_BLACK;
    }
    object->next = gc.objects;
    gc.objects = object;
    gc.live_bytes += bytes;
    gc.live_objects++;
    if (gc.live_bytes >= gc.threshold) gc.requested = 1;
    return object;
}

static PonaArray *as_array(PonaObject *object) {
    return object && object->kind == GC_OBJECT_ARRAY ? (PonaArray *)object : NULL;
}

static PonaString *as_string(PonaObject *object) {
    return object && object->kind == GC_OBJECT_STRING ? (PonaString *)object : NULL;
}

PonaValue pona_value_nil(void) {
    PonaValue value = {0};
    value.type = PONA_VALUE_NIL;
    return value;
}

PonaValue pona_value_number(double number) {
    PonaValue value = {0};
    value.type = PONA_VALUE_NUMBER;
    value.as.number = number;
    return value;
}

PonaValue pona_value_bool(int boolean) {
    PonaValue value = {0};
    value.type = PONA_VALUE_BOOL;
    value.as.boolean = boolean != 0;
    return value;
}

PonaValue pona_value_object(PonaObject *object) {
    PonaValue value = {0};
    value.type = object ? PONA_VALUE_OBJECT : PONA_VALUE_NIL;
    value.as.object = object;
    return value;
}

void pona_gc_root_register(PonaObject **slot) {
    if (!slot) return;
    pona_gc_init();
    for (size_t i = 0; i < gc.root_count; i++)
        if (gc.roots[i] == slot) return;
    if (gc.root_count == gc.root_capacity) {
        size_t capacity = gc.root_capacity ? gc.root_capacity * 2 : 16;
        if (capacity < gc.root_capacity || capacity > (size_t)-1 / sizeof(*gc.roots)) return;
        PonaObject ***roots = (PonaObject ***)realloc(gc.roots, capacity * sizeof(*roots));
        if (!roots) return;
        gc.roots = roots;
        gc.root_capacity = capacity;
    }
    gc.roots[gc.root_count++] = slot;
    barrier_mark(*slot);
}

void pona_gc_root_store(PonaObject **slot, PonaObject *value) {
    if (!slot) return;
    pona_gc_init();
    *slot = value;
    barrier_mark(value);
}

void pona_gc_request(void) {
    pona_gc_init();
    gc.requested = 1;
}

static void start_cycle(void) {
    gc.epoch++;
    if (gc.epoch == 0) {
        for (PonaObject *object = gc.objects; object; object = object->next)
            object->mark_epoch = 0;
        gc.epoch = 1;
    }
    gc.gray = NULL;
    gc.current = NULL;
    gc.root_cursor = 0;
    gc.resume_sweep = 0;
    gc.phase = GC_ROOTS;
    gc.requested = 0;
}

static void free_object(PonaObject *object) {
    if (object->kind == GC_OBJECT_ARRAY) {
        PonaArray *array = (PonaArray *)object;
        size_t buffer_bytes = array->capacity * sizeof(*array->items);
        if (buffer_bytes <= gc.live_bytes) gc.live_bytes -= buffer_bytes;
        free(array->items);
    }
    if (object->bytes <= gc.live_bytes) gc.live_bytes -= object->bytes;
    if (gc.live_objects) gc.live_objects--;
    free(object);
}

static void finish_cycle(void) {
    size_t next = gc.live_bytes > (size_t)-1 / 2 ? (size_t)-1 : gc.live_bytes * 2;
    gc.threshold = next < PONA_GC_MIN_THRESHOLD ? PONA_GC_MIN_THRESHOLD : next;
    gc.requested = 0;
    gc.phase = GC_IDLE;
    gc.current = NULL;
    gc.sweep = NULL;
}

size_t pona_gc_step(size_t work_budget) {
    pona_gc_init();
    gc.last_work = 0;
    if (gc.phase == GC_IDLE) {
        if (!gc.requested && gc.live_bytes < gc.threshold) return 0;
        start_cycle();
    }
    while (gc.last_work < work_budget) {
        if (gc.phase == GC_ROOTS) {
            if (gc.root_cursor < gc.root_count) {
                mark_object(*gc.roots[gc.root_cursor++]);
                gc.last_work++;
            } else {
                gc.phase = GC_MARK;
            }
            continue;
        }
        if (gc.phase == GC_MARK) {
            if (!gc.current) {
                if (!gc.gray) {
                    gc.phase = GC_SWEEP;
                    if (gc.resume_sweep)
                        gc.resume_sweep = 0;
                    else
                        gc.sweep = &gc.objects;
                    continue;
                }
                gc.current = gc.gray;
                gc.gray = gc.current->gray_next;
                gc.current->gray_next = NULL;
            }
            if (gc.current->kind == GC_OBJECT_ARRAY) {
                PonaArray *array = (PonaArray *)gc.current;
                if (gc.current->scan_index < array->count) {
                    PonaValue value = array->items[gc.current->scan_index++];
                    if (value.type == PONA_VALUE_OBJECT) mark_object(value.as.object);
                    gc.last_work++;
                    continue;
                }
            }
            gc.current->color = GC_BLACK;
            gc.current = NULL;
            gc.last_work++;
            continue;
        }
        if (gc.phase == GC_SWEEP) {
            if (!*gc.sweep) {
                finish_cycle();
                return gc.last_work;
            }
            PonaObject *object = *gc.sweep;
            if (object->mark_epoch != gc.epoch) {
                *gc.sweep = object->next;
                free_object(object);
            } else {
                gc.sweep = &object->next;
            }
            gc.last_work++;
        }
    }
    return gc.last_work;
}

size_t pona_gc_live_bytes(void) { return gc.live_bytes; }
size_t pona_gc_live_objects(void) { return gc.live_objects; }
size_t pona_gc_work_last(void) { return gc.last_work; }
int pona_gc_in_cycle(void) { return gc.phase != GC_IDLE; }

PonaObject *pona_string_new(const char *text) {
    const char *source = text ? text : "";
    size_t length = strlen(source);
    if (length > (size_t)-1 - sizeof(PonaString) - 1) return NULL;
    PonaString *string = (PonaString *)allocate_object(sizeof(*string) + length + 1, GC_OBJECT_STRING);
    if (!string) return NULL;
    string->length = length;
    memcpy(string->data, source, length + 1);
    return (PonaObject *)string;
}

PonaObject *pona_string_concat(PonaObject *left, PonaObject *right) {
    const char *a = pona_string_data(left), *b = pona_string_data(right);
    size_t an = strlen(a), bn = strlen(b);
    if (an > (size_t)-1 - bn - sizeof(PonaString) - 1) return NULL;
    PonaString *result = (PonaString *)allocate_object(sizeof(*result) + an + bn + 1, GC_OBJECT_STRING);
    if (!result) return NULL;
    result->length = an + bn;
    memcpy(result->data, a, an);
    memcpy(result->data + an, b, bn + 1);
    return (PonaObject *)result;
}

PonaObject *pona_number_string(double value) {
    char buffer[48];
    (void)snprintf(buffer, sizeof(buffer), "%.7g", value);
    return pona_string_new(buffer);
}

const char *pona_string_data(PonaObject *value) {
    PonaString *string = as_string(value);
    return string ? string->data : "";
}

size_t pona_string_length(PonaObject *value) {
    PonaString *string = as_string(value);
    return string ? string->length : 0;
}

int pona_string_equals(PonaObject *left, PonaObject *right) {
    const char *a = pona_string_data(left), *b = pona_string_data(right);
    return strcmp(a, b) == 0;
}

PonaObject *pona_array_new(void) {
    PonaArray *array = (PonaArray *)allocate_object(sizeof(*array), GC_OBJECT_ARRAY);
    return (PonaObject *)array;
}

static void write_barrier(PonaObject *owner, PonaValue value) {
    if (owner && owner->mark_epoch == gc.epoch && value.type == PONA_VALUE_OBJECT)
        barrier_mark(value.as.object);
}

void pona_array_push(PonaObject *object, PonaValue value) {
    PonaArray *array = as_array(object);
    if (!array) return;
    if (array->count == array->capacity) {
        size_t capacity = array->capacity ? array->capacity * 2 : 8;
        if (capacity < array->capacity || capacity > (size_t)-1 / sizeof(*array->items)) return;
        size_t old_bytes = array->capacity * sizeof(*array->items);
        size_t new_bytes = capacity * sizeof(*array->items);
        PonaValue *items = (PonaValue *)realloc(array->items, new_bytes);
        if (!items) return;
        array->items = items;
        array->capacity = capacity;
        gc.live_bytes += new_bytes - old_bytes;
        if (gc.live_bytes >= gc.threshold) gc.requested = 1;
    }
    array->items[array->count++] = value;
    write_barrier(object, value);
}

void pona_array_push_num(PonaObject *array, double number) {
    pona_array_push(array, pona_value_number(number));
}

void pona_array_push_str(PonaObject *array, PonaObject *string) {
    pona_array_push(array, pona_value_object(string));
}

PonaValue pona_array_get(PonaObject *object, size_t index) {
    PonaArray *array = as_array(object);
    return array && index < array->count ? array->items[index] : pona_value_nil();
}

double pona_array_get_num(PonaObject *array, size_t index) {
    PonaValue value = pona_array_get(array, index);
    return value.type == PONA_VALUE_NUMBER ? value.as.number : 0.0;
}

PonaObject *pona_array_get_obj(PonaObject *array, size_t index) {
    PonaValue value = pona_array_get(array, index);
    return value.type == PONA_VALUE_OBJECT ? value.as.object : NULL;
}

size_t pona_array_length(PonaObject *object) {
    PonaArray *array = as_array(object);
    return array ? array->count : 0;
}

void pona_array_clear(PonaObject *object) {
    PonaArray *array = as_array(object);
    if (array) array->count = 0;
}
