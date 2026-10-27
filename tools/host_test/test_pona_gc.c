#include "engine2d/pona_runtime.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define GC_WORK_BUDGET 7u

static void run_to_idle(void) {
    unsigned int calls = 0;
    size_t work;
    do {
        work = pona_gc_step(GC_WORK_BUDGET);
        assert(work <= GC_WORK_BUDGET);
        assert(pona_gc_work_last() == work);
        assert(++calls < 100000u);
    } while (pona_gc_in_cycle() || work != 0);
}

static void collect_all(void) {
    pona_gc_request();
    run_to_idle();
}

int main(void) {
    PonaObject *root = NULL;
    PonaObject *secondary_root = NULL;
    PonaObject *late_array_child;
    PonaObject *late_root_child;

    pona_gc_init();
    pona_gc_root_register(&root);
    pona_gc_root_register(&secondary_root);
    late_array_child = pona_string_new("barrier-array-child");
    late_root_child = pona_string_new("barrier-root-child");
    pona_gc_root_store(&root, pona_array_new());

    /* Drive the collector into sweep, then add edges to white objects. */
    pona_gc_request();
    assert(pona_gc_step(1) == 1); /* first registered root */
    assert(pona_gc_step(1) == 1); /* second registered root */
    assert(pona_gc_step(1) == 1); /* mark the root array */
    assert(pona_gc_step(1) == 1); /* begin sweeping; preserve the array */
    pona_array_push_str(root, late_array_child);
    pona_gc_root_store(&secondary_root, late_root_child);
    run_to_idle();

    assert(pona_array_length(root) == 1);
    assert(strcmp(pona_string_data(pona_array_get_obj(root, 0)), "barrier-array-child") == 0);
    assert(strcmp(pona_string_data(secondary_root), "barrier-root-child") == 0);

    for (int i = 0; i < 1200; i++) {
        char temporary[40];
        (void)snprintf(temporary, sizeof(temporary), "unreachable-%d", i);
        assert(pona_string_new(temporary) != NULL);
        size_t work = pona_gc_step(GC_WORK_BUDGET);
        assert(work <= GC_WORK_BUDGET);
        assert(pona_gc_work_last() == work);
    }
    for (int i = 0; i < 100; i++) pona_array_push_num(root, (double)i);
    run_to_idle();
    collect_all();
    assert(pona_gc_live_objects() == 3);
    assert(pona_array_length(root) == 101);
    for (int i = 0; i < 100; i++)
        assert(pona_array_get_num(root, (size_t)i + 1u) == (double)i);

    pona_array_clear(root);
    pona_gc_root_store(&root, NULL);
    pona_gc_root_store(&secondary_root, NULL);
    collect_all();
    assert(pona_gc_live_objects() == 0);
    assert(pona_gc_live_bytes() == 0);

    puts("Pona incremental GC and sweep barriers: ok");
    return 0;
}
