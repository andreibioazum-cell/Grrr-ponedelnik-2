#include "engine2d/input.h"

#include <string.h>

#define KEY_CAP 128

typedef struct { unsigned char down, pressed, released; } KeyState;
static KeyState keys[KEY_CAP];
static float pointer_x, pointer_y;
static int pointer_down, pointer_pressed, pointer_released;

void p2d_input_key(int key, int down) {
    if (key <= 0 || key >= KEY_CAP) return;
    down = down != 0;
    if (down && !keys[key].down) keys[key].pressed = 1;
    if (!down && keys[key].down) keys[key].released = 1;
    keys[key].down = (unsigned char)down;
}

void p2d_input_touch(float x, float y, int down) {
    pointer_x = x;
    pointer_y = y;
    down = down != 0;
    if (down && !pointer_down) pointer_pressed = 1;
    if (!down && pointer_down) pointer_released = 1;
    pointer_down = down;
}

static int key_code(const char *name) {
    if (!name) return 0;
    if (!strcmp(name, "left")) return P2D_KEY_LEFT;
    if (!strcmp(name, "right")) return P2D_KEY_RIGHT;
    if (!strcmp(name, "up")) return P2D_KEY_UP;
    if (!strcmp(name, "down")) return P2D_KEY_DOWN;
    if (!strcmp(name, "space")) return P2D_KEY_SPACE;
    if (!strcmp(name, "enter")) return P2D_KEY_ENTER;
    if (!strcmp(name, "back")) return P2D_KEY_BACK;
    if (!strcmp(name, "escape")) return P2D_KEY_ESCAPE;
    if (name[0] >= 'a' && name[0] <= 'z' && name[1] == '\0')
        return P2D_KEY_A + name[0] - 'a';
    if (name[0] >= '0' && name[0] <= '9' && name[1] == '\0')
        return P2D_KEY_0 + name[0] - '0';
    return 0;
}

int p2d_input_named(const char *name, int mode) {
    int code = key_code(name);
    if (code) {
        if (mode == 1) return keys[code].pressed;
        if (mode == 2) return keys[code].released;
        return keys[code].down;
    }
    if (name && !strcmp(name, "pointer")) {
        if (mode == 1) return pointer_pressed;
        if (mode == 2) return pointer_released;
        return pointer_down;
    }
    return 0;
}

float p2d_pointer_x(void) { return pointer_x; }
float p2d_pointer_y(void) { return pointer_y; }
int p2d_pointer_down(void) { return pointer_down; }
int p2d_pointer_pressed(void) { return pointer_pressed; }
int p2d_pointer_released(void) { return pointer_released; }

void p2d_input_end_frame(void) {
    for (int i = 0; i < KEY_CAP; i++) {
        keys[i].pressed = 0;
        keys[i].released = 0;
    }
    pointer_pressed = 0;
    pointer_released = 0;
}

void p2d_input_reset(void) {
    memset(keys, 0, sizeof(keys));
    pointer_x = pointer_y = 0.0f;
    pointer_down = pointer_pressed = pointer_released = 0;
}
