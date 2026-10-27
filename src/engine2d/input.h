#ifndef PONDELINIK_2D_INPUT_H
#define PONDELINIK_2D_INPUT_H

#include <stdint.h>

enum {
    P2D_KEY_LEFT = 1,
    P2D_KEY_RIGHT,
    P2D_KEY_UP,
    P2D_KEY_DOWN,
    P2D_KEY_SPACE,
    P2D_KEY_ENTER,
    P2D_KEY_BACK,
    P2D_KEY_ESCAPE,
    P2D_KEY_A = 32,
    P2D_KEY_Z = P2D_KEY_A + 25,
    P2D_KEY_0 = 64,
    P2D_KEY_9 = P2D_KEY_0 + 9
};

void p2d_input_key(int key, int down);
void p2d_input_touch(float x, float y, int down);
void p2d_input_end_frame(void);
void p2d_input_reset(void);
int p2d_input_named(const char *name, int mode);
float p2d_pointer_x(void);
float p2d_pointer_y(void);
int p2d_pointer_down(void);
int p2d_pointer_pressed(void);
int p2d_pointer_released(void);

#endif
