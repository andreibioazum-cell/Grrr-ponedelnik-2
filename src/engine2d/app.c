#include "engine2d/app.h"
#include "engine2d/api.h"
#include "engine2d/input.h"
#include "engine2d/pona_runtime.h"

#include <time.h>

extern void pona_script_init(void);
extern void pona_script_update(float delta);
extern void pona_script_draw(void);
extern void pona_script_stop(void);

void pond_app_init(AAssetManager *assets) {
    ds_set_asset_manager(assets);
    pona_gc_init();
    p2d_set_time(0.0f);
    p2d_input_reset();
    uint32_t seed = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)assets;
    seed ^= ((uint32_t)screen_w << 16) ^ (uint32_t)screen_h;
    p2d_seed_random(seed);
    pona_script_init();
}

void pond_app_update(void) {
    float delta = (float)dt;
    if (!(delta >= 0.0f) || delta > 0.1f) delta = delta < 0.0f ? 0.0f : 0.1f;
    p2d_set_time(p2d_time() + delta);
    pona_script_update(delta);
}

void pond_app_draw(Buffer *buffer) {
    (void)buffer;
    pona_script_draw();
}

void pond_app_touch(float x, float y, int down) { p2d_input_touch(x, y, down); }
void pond_app_key(int key, int down) { p2d_input_key(key, down); }
void pond_app_end_frame(void) {
    (void)pona_gc_step(64);
    p2d_input_end_frame();
}

void pond_app_stop(void) {
    pona_script_stop();
    p2d_input_reset();
}
