#include "engine2d/api.h"
#include "engine2d/input.h"
#include "engine/runtime.h"

#include <math.h>
#include <string.h>

static float elapsed_time;
static uint32_t random_state = 0x4D595DF4u;

void p2d_clear(uint32_t color) { rect(0.0f, 0.0f, (float)screen_w, (float)screen_h, color); }
void p2d_rect(float x, float y, float width, float height, uint32_t color) {
    rect(x, y, width, height, color);
}
void p2d_roundrect(float x, float y, float width, float height, float radius, uint32_t color) {
    roundrect(x, y, width, height, radius, color);
}
void p2d_circle(float x, float y, float radius, uint32_t color) { circle(x, y, radius, color); }
void p2d_line(float x1, float y1, float x2, float y2, float thickness, uint32_t color) {
    line(x1, y1, x2, y2, thickness, color);
}
void p2d_sprite(PonaObject *asset, float x, float y, float scale) {
    const char *path = pona_string_data(asset);
    if (path[0]) tex(x, y, path, 0.0f, scale);
}
void p2d_text(PonaObject *value, float x, float y, uint32_t color, float scale) {
    text_scaled(pona_string_data(value), x, y, color, scale);
}
int p2d_key_down(PonaObject *name) { return p2d_input_named(pona_string_data(name), 0); }
int p2d_key_pressed(PonaObject *name) { return p2d_input_named(pona_string_data(name), 1); }
int p2d_key_released(PonaObject *name) { return p2d_input_named(pona_string_data(name), 2); }
float p2d_time(void) { return elapsed_time; }
void p2d_set_time(float seconds) { elapsed_time = seconds; }
float p2d_random(void) {
    uint32_t x = random_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    random_state = x ? x : 0x4D595DF4u;
    return (float)(random_state >> 8) / 16777216.0f;
}
void p2d_seed_random(uint32_t seed) { random_state = seed ? seed : 0x4D595DF4u; }
float p2d_screen_width(void) { return (float)screen_w; }
float p2d_screen_height(void) { return (float)screen_h; }
float p2d_clamp(float value, float low, float high) {
    return value < low ? low : value > high ? high : value;
}
float p2d_lerp(float start, float end, float amount) {
    return start + (end - start) * amount;
}
float p2d_pointer_x_value(void) { return p2d_pointer_x(); }
float p2d_pointer_y_value(void) { return p2d_pointer_y(); }
int p2d_pointer_is_down(void) { return p2d_pointer_down(); }
int p2d_pointer_is_pressed(void) { return p2d_pointer_pressed(); }
int p2d_pointer_is_released(void) { return p2d_pointer_released(); }
int32_t p2d_array_length(PonaObject *array) {
    size_t length = pona_array_length(array);
    return length > INT32_MAX ? INT32_MAX : (int32_t)length;
}
int32_t p2d_gc_bytes(void) {
    size_t bytes = pona_gc_live_bytes();
    return bytes > INT32_MAX ? INT32_MAX : (int32_t)bytes;
}
int32_t p2d_gc_objects(void) {
    size_t objects = pona_gc_live_objects();
    return objects > INT32_MAX ? INT32_MAX : (int32_t)objects;
}
