#include "engine2d/api.h"
#include "engine2d/input.h"
#include "engine/runtime.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

extern void pona_script_init(void);
extern void pona_script_update(float delta);
extern void pona_script_draw(void);
extern void pona_script_stop(void);
extern const PonaScriptExport pona_script_exports[];
extern const size_t pona_script_export_count;
extern float pona_global_actor_x;
extern PonaObject *pona_global_trail;

int screen_w = 400;
int screen_h = 300;
double dt;
static unsigned int rect_count;
static unsigned int circle_count;
static unsigned int line_count;
static unsigned int text_count;

void rect(float x, float y, float width, float height, uint32_t color) {
    (void)x; (void)y; (void)width; (void)height; (void)color;
    rect_count++;
}
void roundrect(float x, float y, float width, float height, float radius, uint32_t color) {
    (void)x; (void)y; (void)width; (void)height; (void)radius; (void)color;
    rect_count++;
}
void circle(float x, float y, float radius, uint32_t color) {
    (void)x; (void)y; (void)radius; (void)color;
    circle_count++;
}
void line(float x1, float y1, float x2, float y2, float thickness, uint32_t color) {
    (void)x1; (void)y1; (void)x2; (void)y2; (void)thickness; (void)color;
    line_count++;
}
void tex(float x, float y, const char *name, float angle, float scale) {
    (void)x; (void)y; (void)name; (void)angle; (void)scale;
}
void text_scaled(const char *value, float x, float y, uint32_t color, float scale) {
    (void)x; (void)y; (void)color; (void)scale;
    assert(value != NULL);
    text_count++;
}

int main(void) {
    pona_script_init();
    assert(pona_script_export_count == 1);
    assert(strcmp(pona_script_exports[0].name, "move_speed") == 0);
    assert(strcmp(pona_script_exports[0].type, "f32") == 0);
    assert(*(float *)pona_script_exports[0].address == 210.0f);

    p2d_input_reset();
    p2d_seed_random(1234u);
    float first_random = p2d_random();
    p2d_seed_random(1234u);
    assert(p2d_random() == first_random);
    assert(first_random >= 0.0f && first_random < 1.0f);
    p2d_input_key(P2D_KEY_RIGHT, 1);
    pona_script_update(0.1f);
    assert(fabsf(pona_global_actor_x - 271.0f) < 0.01f);
    p2d_input_key(P2D_KEY_RIGHT, 0);
    p2d_input_end_frame();

    p2d_input_touch(77.0f, 88.0f, 1);
    pona_script_update(0.016f);
    assert(pona_array_length(pona_global_trail) == 2);
    assert(pona_array_get_num(pona_global_trail, 0) == 77.0);
    assert(pona_array_get_num(pona_global_trail, 1) == 88.0);
    p2d_input_end_frame();

    pona_script_draw();
    assert(rect_count >= 2);
    assert(circle_count >= 3);
    assert(line_count >= 20);
    assert(text_count == 4);
    pona_script_stop();
    puts("Pona generated script + native API integration: ok");
    return 0;
}
