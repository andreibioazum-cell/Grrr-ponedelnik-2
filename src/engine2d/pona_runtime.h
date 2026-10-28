#ifndef PONA_RUNTIME_H
#define PONA_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

typedef struct PonaObject PonaObject;
typedef struct {
    const char *name;
    const char *type;
    void *address;
} PonaScriptExport;

typedef enum {
    PONA_VALUE_NIL = 0,
    PONA_VALUE_NUMBER,
    PONA_VALUE_BOOL,
    PONA_VALUE_OBJECT
} PonaValueType;

typedef struct {
    PonaValueType type;
    union {
        double number;
        int boolean;
        PonaObject *object;
    } as;
} PonaValue;

void pona_gc_init(void);
void pona_gc_request(void);
size_t pona_gc_step(size_t work_budget);
void pona_gc_root_register(PonaObject **slot);
void pona_gc_root_store(PonaObject **slot, PonaObject *value);
size_t pona_gc_live_bytes(void);
size_t pona_gc_live_objects(void);
size_t pona_gc_work_last(void);
int pona_gc_in_cycle(void);

PonaObject *pona_string_new(const char *text);
PonaObject *pona_string_concat(PonaObject *left, PonaObject *right);
PonaObject *pona_number_string(double value);
const char *pona_string_data(PonaObject *value);
size_t pona_string_length(PonaObject *value);
int pona_string_equals(PonaObject *left, PonaObject *right);

PonaObject *pona_array_new(void);
void pona_array_push(PonaObject *array, PonaValue value);
void pona_array_push_num(PonaObject *array, double number);
void pona_array_push_str(PonaObject *array, PonaObject *string);
PonaValue pona_array_get(PonaObject *array, size_t index);
double pona_array_get_num(PonaObject *array, size_t index);
PonaObject *pona_array_get_obj(PonaObject *array, size_t index);
size_t pona_array_length(PonaObject *array);
void pona_array_clear(PonaObject *array);

PonaValue pona_value_nil(void);
PonaValue pona_value_number(double number);
PonaValue pona_value_bool(int boolean);
PonaValue pona_value_object(PonaObject *object);

#endif
