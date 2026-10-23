/* Cubic Battle 4 — WebAssembly + WebGL2 60 FPS Browser Platform Layer.
 * Compiles the exact C game (game.c, mc, graphics.c, network.c, audio.c)
 * to WebAssembly, rendering 3D Minecraft & UI via WebGL2 GPU batches and talking
 * directly to the live Firebase Auth & Realtime Database. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine/runtime.h"
#include "engine/network.h"
#include "game/game.h"
#include "platform/pc/pc_input.h"

#define WASM_EXP(name) __attribute__((export_name(name)))

typedef struct {
    int pipeline;
    uint32_t tex_ptr;
    int tw, th;
    uint32_t first, count;
} WebGpuBatch;

#define WEB_MAX_BATCHES 1024
static WebGpuBatch g_web_batches[WEB_MAX_BATCHES];
static int g_web_batch_n = 0;
static int g_web_w = 960, g_web_h = 600;
static Buffer g_web_buf = {0};
static int g_web_inited = 0;
static double g_web_dt = 0.016;
static double g_net_accum = 0.0;

__attribute__((import_module("env"), import_name("js_gl_submit"))) extern void
js_gl_submit(int w, int h, const void *verts, int vn, const uint16_t *idx, int in_cnt, const void *batches, int bn,
             int gray);
__attribute__((import_module("env"), import_name("js_log"))) extern void js_log(int is_err, const char *msg);

extern void net_wasm_room_step(void);
void web_mix_audio_s16(int16_t *dst, int frames);

static void pc_exe_dir(char *dst, size_t cap) {
    if (dst && cap > 0)
        snprintf(dst, cap, ".");
}

double ds_frame_seconds(void) { return g_web_dt; }

static void web_gpu_frame_begin(int w, int h) {
    g_web_w = w;
    g_web_h = h;
    g_web_batch_n = 0;
}

static void web_gpu_draw_range(int pipeline, const uint32_t *tex, int tw, int th, size_t first, size_t count) {
    uint32_t tp = (uint32_t)(uintptr_t)tex;
    if (!count)
        return;
    if (g_web_batch_n > 0) {
        WebGpuBatch *prev = &g_web_batches[g_web_batch_n - 1];
        if (prev->pipeline == pipeline && prev->tex_ptr == tp && prev->first + prev->count == (uint32_t)first) {
            prev->count += (uint32_t)count;
            return;
        }
    }
    if (g_web_batch_n < WEB_MAX_BATCHES) {
        WebGpuBatch *b = &g_web_batches[g_web_batch_n++];
        b->pipeline = pipeline;
        b->tex_ptr = tp;
        b->tw = tw;
        b->th = th;
        b->first = (uint32_t)first;
        b->count = (uint32_t)count;
    }
}

static int gfx_gray_on;

static void web_gpu_frame_end(const void *verts, size_t vn, const uint16_t *idx, size_t in_cnt) {
    js_gl_submit(g_web_w, g_web_h, verts, (int)vn, idx, (int)in_cnt, g_web_batches, g_web_batch_n,
                 gfx_gray_on ? 1 : 0);
}

#include "platform/pc/pc_assets.inc"

WASM_EXP("web_alloc") void *web_alloc(int size) { return malloc((size_t)(size > 0 ? size : 1)); }
WASM_EXP("web_free") void web_free(void *p) { free(p); }

WASM_EXP("web_init") int web_init(int w, int h) {
    g_web_w = w > 200 ? w : 960;
    g_web_h = h > 200 ? h : 600;
    screen_w = g_web_w;
    screen_h = g_web_h;
    ds_set_asset_manager(pc_asset_manager());
    pc_assets_set_root("assets");
    net_set_data_path("cb4_data");
    net_set_system_language("ru");
    if (!ds_graphics_init(pc_asset_manager(), NULL))
        return 0;
    ds_sound_init(pc_asset_manager());
    game_init(pc_asset_manager());
    game_reset();
    g_web_inited = 1;
    return 1;
}

WASM_EXP("web_resize") void web_resize(int w, int h) {
    if (w < 200 || h < 200)
        return;
    g_web_w = w;
    g_web_h = h;
    screen_w = w;
    screen_h = h;
}

WASM_EXP("web_frame") void web_frame(double step_sec) {
    if (!g_web_inited)
        return;
    if (step_sec < 0.001)
        step_sec = 0.001;
    if (step_sec > 0.05)
        step_sec = 0.05;
    g_web_dt = step_sec;
    dt = step_sec;
    screen_w = g_web_w;
    screen_h = g_web_h;

    if (ds_game_restart_requested()) {
        ds_clear_game_restart();
        ds_clear_runtime_error();
        game_bye();
        game_init(pc_asset_manager());
        game_reset();
    }

    game_pc_room_tick();
    g_net_accum += step_sec;
    if (g_net_accum >= 0.09) {
        g_net_accum = 0.0;
        net_wasm_room_step();
    }

    game_update();
    ds_string_pool_tick();

    g_web_buf.width = g_web_w;
    g_web_buf.height = g_web_h;
    g_web_buf.stride = g_web_w;
    if (ds_graphics_begin_frame(&g_web_buf)) {
        game_draw(&g_web_buf);
        ds_graphics_end_frame();
    }
    mouse_clicked = 0;
}

WASM_EXP("web_touch") void web_touch(float x, float y, int action, int id) {
    ds_mouse_x = x;
    ds_mouse_y = y;
    if (action == 0)
        mouse_clicked = 1;
    if (g_web_inited)
        game_touch(x, y, action, id);
}

WASM_EXP("web_key") void web_key(int vk, int down) {
    if (!g_web_inited)
        return;
    game_mc_key(vk, down);
    if (down && keyboard_visible()) {
        if (vk == 8)
            keyboard_backspace();
        else if (vk == 13)
            (void)keyboard_handle_key(66, 0, 0);
    }
}

WASM_EXP("web_text") void web_text(const char *s, int submit_enter) {
    if (!g_web_inited || !s)
        return;
    keyboard_clear();
    keyboard_type(s);
    if (submit_enter)
        (void)keyboard_handle_key(66, 0, 0);
}

WASM_EXP("web_mouse_delta") void web_mouse_delta(float dx, float dy) {
    if (g_web_inited)
        game_mc_mouse_delta(dx, dy);
}

WASM_EXP("web_mouse_btn") void web_mouse_btn(int btn, int down) {
    if (g_web_inited)
        game_mc_mouse_btn(btn, down);
}

WASM_EXP("web_wheel") void web_wheel(int delta) {
    if (g_web_inited)
        game_mc_wheel(delta);
}

WASM_EXP("web_wants_lock") int web_wants_lock(void) { return g_web_inited ? game_mc_wants_cursor_lock() : 0; }
WASM_EXP("web_wants_kb") int web_wants_kb(void) { return g_web_inited ? keyboard_visible() : 0; }
WASM_EXP("web_mix_audio") void web_mix_audio(int16_t *dst, int frames) { web_mix_audio_s16(dst, frames); }

#include "engine/graphics.c"
#include "engine/network.c"
#include "engine/audio.c"
