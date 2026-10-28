#ifndef PONDELINIK_2D_API_H
#define PONDELINIK_2D_API_H

#include "engine2d/pona_runtime.h"
#include <stdint.h>

void p2d_clear(uint32_t color);
void p2d_rect(float x, float y, float width, float height, uint32_t color);
void p2d_roundrect(float x, float y, float width, float height, float radius, uint32_t color);
void p2d_circle(float x, float y, float radius, uint32_t color);
void p2d_line(float x1, float y1, float x2, float y2, float thickness, uint32_t color);
void p2d_sprite(PonaObject *asset, float x, float y, float scale);
void p2d_text(PonaObject *value, float x, float y, uint32_t color, float scale);
int p2d_key_down(PonaObject *name);
int p2d_key_pressed(PonaObject *name);
int p2d_key_released(PonaObject *name);
float p2d_time(void);
float p2d_random(void);
void p2d_seed_random(uint32_t seed);
float p2d_screen_width(void);
float p2d_screen_height(void);
float p2d_clamp(float value, float low, float high);
float p2d_lerp(float start, float end, float amount);
float p2d_pointer_x_value(void);
float p2d_pointer_y_value(void);
int p2d_pointer_is_down(void);
int p2d_pointer_is_pressed(void);
int p2d_pointer_is_released(void);
int32_t p2d_array_length(PonaObject *array);
int32_t p2d_gc_bytes(void);
int32_t p2d_gc_objects(void);
void p2d_set_time(float seconds);

#endif
