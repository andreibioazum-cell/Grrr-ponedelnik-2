#include "engine2d/input.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    p2d_input_reset();
    assert(!p2d_input_named("left", 0));
    p2d_input_key(P2D_KEY_LEFT, 1);
    assert(p2d_input_named("left", 0));
    assert(p2d_input_named("left", 1));
    assert(!p2d_input_named("left", 2));
    p2d_input_key(P2D_KEY_LEFT, 1); /* key repeat is not another press edge */
    assert(p2d_input_named("left", 1));
    p2d_input_end_frame();
    assert(p2d_input_named("left", 0));
    assert(!p2d_input_named("left", 1));
    p2d_input_key(P2D_KEY_LEFT, 0);
    assert(!p2d_input_named("left", 0));
    assert(p2d_input_named("left", 2));
    p2d_input_end_frame();
    assert(!p2d_input_named("left", 2));

    p2d_input_key(P2D_KEY_Z, 1);
    assert(p2d_input_named("z", 0));
    p2d_input_key(P2D_KEY_0 + 7, 1);
    assert(p2d_input_named("7", 0));
    p2d_input_key(P2D_KEY_SPACE, 1);
    assert(p2d_input_named("space", 0));

    p2d_input_touch(13.5f, 24.25f, 1);
    assert(p2d_input_named("pointer", 0));
    assert(p2d_input_named("pointer", 1));
    assert(p2d_pointer_x() == 13.5f);
    assert(p2d_pointer_y() == 24.25f);
    p2d_input_end_frame();
    assert(p2d_input_named("pointer", 0));
    assert(!p2d_input_named("pointer", 1));
    p2d_input_touch(20.0f, 30.0f, 0);
    assert(!p2d_input_named("pointer", 0));
    assert(p2d_input_named("pointer", 2));
    assert(p2d_pointer_x() == 20.0f);
    assert(p2d_pointer_y() == 30.0f);
    p2d_input_end_frame();
    assert(!p2d_input_named("pointer", 2));

    p2d_input_reset();
    assert(!p2d_input_named("z", 0));
    assert(!p2d_input_named("space", 0));
    assert(!p2d_input_named("pointer", 0));
    puts("Pona input edges and pointer state: ok");
    return 0;
}
