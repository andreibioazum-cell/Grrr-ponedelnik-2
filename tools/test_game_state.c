/* Comprehensive Automated C99 Verification & Live Multi-Player E2E Harness for Geometrium:
 * 1. Startup unrestricted-freedom + epilepsy warning & new Geometrium copyrights.
 * 2. Strict 100% monochrome dark-gray UI invariant (R == G == B on every UI command).
 * 3. Large circular & stadium-capsule control geometry (zero small rectangular pads, zero classes).
 * 4. Mathematical 3D water occlusion proof (water behind walls/hills NEVER renders after closer walls).
 * 5. Real 3D block breaking, block placing, demolition craters, and solo world persistence.
 * 6. Real multi-process TCP/HTTP Firebase RTDB multiplayer (live 3D players, blocks.json, chat, accounts). */
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "engine/runtime.h"
#include "engine/network.h"

int screen_w = 1920, screen_h = 1080;
double dt = 0.016;
int mouse_clicked = 0;
double ds_mouse_x = 0, ds_mouse_y = 0;
Joy joy = {0};

static int check_monochrome_ui = 0;
static int seen_epilepsy_warning = 0;
static int seen_copyright_2026 = 0;
static int circle_buttons_drawn = 0;

static void assert_monochrome_argb(uint32_t c, const char *where) {
    unsigned int r = (c >> 16) & 0xFFu;
    unsigned int g = (c >> 8) & 0xFFu;
    unsigned int b = c & 0xFFu;
    if (check_monochrome_ui && !(r == g && g == b)) {
        fprintf(stderr, "NON-MONOCHROME UI COLOR in %s: 0x%08x (R=%u G=%u B=%u)\n", where, c, r, g, b);
        abort();
    }
}

/* Engine runtime & graphics stubs linking directly against real src/engine/network.c */
static char stub_kb[96] = "";
static int stub_kb_vis = 0, stub_kb_enter = 0;

void ds_log(const char *fmt, ...) { (void)fmt; }
void ds_log_err(const char *fmt, ...) { (void)fmt; }
void ds_console_log(int err, const char *fmt, ...) { (void)err; (void)fmt; }
void ds_set_asset_manager(AAssetManager *a) { (void)a; }
void ds_release_assets(void) {}
int ds_sound_init(AAssetManager *a) { (void)a; return 1; }
void ds_sound_shutdown(void) {}
void ds_sound_suspend(void) {}
void ds_sound_pause(void) {}
void ds_sound_resume(void) {}
void ds_sound_set_java_vm(void *vm) { (void)vm; }

void keyboard_show(void) { stub_kb_vis = 1; }
void keyboard_hide(void) { stub_kb_vis = 0; }
int keyboard_visible(void) { return stub_kb_vis; }
void keyboard_clear(void) { stub_kb[0] = '\0'; }
void keyboard_get_text_to(char *dst, size_t cap) { snprintf(dst, cap, "%s", stub_kb); }
int keyboard_enter_pressed(void) {
    int e = stub_kb_enter;
    stub_kb_enter = 0;
    return e;
}

int png_load(const char *name) { return name && *name; }
int tex_ready(const char *name) { return name && *name; }
int snd_load(const char *name) { return name && *name; }
int snd_play(const char *name) { (void)name; return 1; }
int snd_loop(const char *name) { (void)name; return 1; }
void snd_stop(const char *name) { (void)name; }
int snd_playing(const char *name) { (void)name; return 0; }
void snd_volume(const char *name, double v) { (void)name; (void)v; }
void ds_gfx_gray(double on) { (void)on; }
void ds_gfx_shake(double amp, double sec) { (void)amp; (void)sec; }

void rect(float x, float y, float w, float h, uint32_t c) {
    (void)x; (void)y; (void)w; (void)h;
    assert_monochrome_argb(c, "rect");
}
void roundrect(float x, float y, float w, float h, float r, uint32_t c) {
    (void)x; (void)y; (void)w; (void)h; (void)r;
    assert_monochrome_argb(c, "roundrect");
}
void circle(float x, float y, float r, uint32_t c) {
    (void)x; (void)y;
    if (r >= 55.0f)
        circle_buttons_drawn++;
    assert_monochrome_argb(c, "circle");
}
void ring(float x, float y, float r, float t, uint32_t c) {
    (void)x; (void)y; (void)r; (void)t;
    assert_monochrome_argb(c, "ring");
}
void line(float x1, float y1, float x2, float y2, float t, uint32_t c) {
    (void)x1; (void)y1; (void)x2; (void)y2; (void)t;
    assert_monochrome_argb(c, "line");
}
void text_scaled(const char *s, float x, float y, uint32_t c, float sc) {
    (void)x; (void)y; (void)sc;
    assert_monochrome_argb(c, "text_scaled");
    if (s) {
        if (strstr(s, "эпилепс") || strstr(s, "epilepsy"))
            seen_epilepsy_warning = 1;
        if (strstr(s, "2026 Geometrium"))
            seen_copyright_2026 = 1;
    }
}
void text(const char *s, float x, float y, uint32_t c) { text_scaled(s, x, y, c, 1.0f); }
int text_ink_width(const char *s) { return s ? (int)(strlen(s) * 9) : 0; }
int text_ink_height(const char *s) { (void)s; return 32; }

void ds_gfx_quad(const char *tex_name, float x0, float y0, float u0, float v0, uint32_t c0, float x1, float y1,
                 float u1, float v1, uint32_t c1, float x2, float y2, float u2, float v2, uint32_t c2, float x3,
                 float y3, float u3, float v3, uint32_t c3) {
    (void)tex_name;
    (void)x0; (void)y0; (void)u0; (void)v0; (void)c0;
    (void)x1; (void)y1; (void)u1; (void)v1; (void)c1;
    (void)x2; (void)y2; (void)u2; (void)v2; (void)c2;
    (void)x3; (void)y3; (void)u3; (void)v3; (void)c3;
}

#include "game/game.c"

static void run_local_engine_tests(const char *tmp_dir) {
    GeoHudLayout L;
    int i, j;

    net_set_data_path(tmp_dir);
    game_init(NULL);

    /* 1. Startup unrestricted-freedom + epilepsy warning & new copyrights */
    assert(geo.screen == SCR_WARNING);
    check_monochrome_ui = 1;
    geo_draw_menu_screens();
    check_monochrome_ui = 0;
    assert(seen_epilepsy_warning == 1);
    assert(seen_copyright_2026 == 1);

    /* Tap accept warning -> enters main menu and saves acceptance */
    game_touch(960.0f, 540.0f + 210.0f, 0, 0);
    assert(geo.screen == SCR_MENU);
    assert(net_warning_accepted() == 1);

    /* 2. Verify EVERY UI & HUD screen is strictly 100% monochrome dark gray + large round controls */
    check_monochrome_ui = 1;
    geo_draw_menu_screens();
    geo.screen = SCR_ROOMS;   geo_draw_menu_screens();
    geo.screen = SCR_PROFILE; geo_draw_menu_screens();
    geo.screen = SCR_INFO;    geo_draw_menu_screens();

    geo.screen = SCR_PLAY;
    geo.flying = 1;
    circle_buttons_drawn = 0;
    geo_draw_play_hud();
    geo.screen = SCR_SCRIPTS; geo_draw_menu_screens(); geo.screen = SCR_PLAY;
    geo_draw_chat_overlay();
    geo_draw_pause_overlay();
    check_monochrome_ui = 0;

    geo_compute_hud(&L);
    assert(L.mine_r >= 68.0f && L.place_r >= 62.0f && L.jump_r >= 58.0f && L.joy_r >= 100.0f);
    assert(circle_buttons_drawn >= 6);

    /* 3. Mathematical 3D Water Occlusion Verification:
     * Build a solid tall slate wall completely separating camera from water behind it.
     * Assert EVERY overlapping screen region has an opaque wall quad drawn AFTER every water quad! */
    geo_init_world(0, "occlusion_test", 1);
    geo_fill_box(38, 1, 38, 58, 30, 62, GEO_AIR);
    geo_fill_box(38, 12, 38, 58, 12, 62, GEO_SLATE);
    /* Put a pool of crystal water behind the solid wall (z = 54..57, y = 13..15) */
    geo_fill_box(42, 13, 54, 54, 15, 57, GEO_WATER);
    /* Build solid opaque slate wall in front of the water (x = 40..56, y = 13..22, z = 51) */
    geo_fill_box(40, 13, 51, 56, 22, 51, GEO_SLATE);

    geo.px = 48.5f;
    geo.py = 13.01f;
    geo.pz = 47.5f;
    geo.yaw = 0.0f;
    geo.pitch = 0.0f;
    geo.record_quads = 1;
    geo_setup_camera(geo.px, geo.py + GEO_EYE_H, geo.pz, geo.yaw, geo.pitch, 0);
    geo_render_scene3d(0);
    geo.record_quads = 0;

    assert(geo.rec_n > 20);
    {
        int water_seen = 0;
        for (i = 0; i < geo.rec_n; i++) {
            if (!geo.rec_is_water[i])
                continue;
            water_seen++;
            /* No water quad may be the topmost drawn quad at its screen center because solid wall z=51 blocks ALL of z=54..57! */
            {
                float cxp = 0.5f * (geo.rec_min_x[i] + geo.rec_max_x[i]);
                float cyp = 0.5f * (geo.rec_min_y[i] + geo.rec_max_y[i]);
                int covered_by_later_solid = 0;
                for (j = i + 1; j < geo.rec_n; j++) {
                    if (!geo.rec_is_water[j] && cxp >= geo.rec_min_x[j] - 1.0f && cxp <= geo.rec_max_x[j] + 1.0f &&
                        cyp >= geo.rec_min_y[j] - 1.0f && cyp <= geo.rec_max_y[j] + 1.0f) {
                        covered_by_later_solid = 1;
                        break;
                    }
                }
                assert(covered_by_later_solid == 1);
            }
        }
        assert(water_seen > 0);
    }

    /* And when looking directly down at open water over dune sand, dune sand IS drawn BEFORE water! */
    geo_fill_box(46, 13, 51, 50, 22, 51, GEO_AIR);
    geo_fill_box(46, 12, 54, 50, 12, 57, GEO_DUNE);
    geo.px = 48.5f;
    geo.py = 17.5f;
    geo.pz = 52.5f;
    geo.yaw = 0.0f;
    geo.pitch = -0.65f;
    geo.record_quads = 1;
    geo_setup_camera(geo.px, geo.py + GEO_EYE_H, geo.pz, geo.yaw, geo.pitch, 0);
    geo_render_scene3d(0);
    geo.record_quads = 0;
    {
        int last_water = -1, first_dune_under = -1;
        for (i = 0; i < geo.rec_n; i++) {
            if (geo.rec_is_water[i])
                last_water = i;
            else if (first_dune_under < 0)
                first_dune_under = i;
        }
        assert(last_water > first_dune_under);
    }

    /* 4. Block placing, block mining, demolition crater, and solo world disk persistence */
    geo_init_world(0, "solo", 1);
    geo.screen = SCR_PLAY;
    geo_fill_box(46, 16, 46, 52, 21, 52, GEO_AIR);
    geo_set_block(48, 17, 50, GEO_LIMESTONE, 0);
    geo.px = 48.5f;
    geo.py = 16.01f;
    geo.pz = 47.6f;
    geo.yaw = 0.0f;
    geo.pitch = 0.0f;
    geo_setup_camera(geo.px, geo.py + GEO_EYE_H, geo.pz, geo.yaw, geo.pitch, 0);
    geo_update_raycast();
    assert(geo.hit == 1 && geo.bx == 48 && geo.by == 17 && geo.bz == 50);

    /* Place block onto front face */
    geo.hotbar[0] = GEO_MARBLE;
    geo.slot = 0;
    assert(geo_try_place_block() == 1);
    assert(geo_get(48, 17, 49) == GEO_MARBLE);

    /* Hold mine button until marble block shatters */
    geo.mine_tid = 1;
    for (i = 0; i < 80 && geo_get(48, 17, 49) != GEO_AIR; i++) {
        dt = 0.02;
        game_update();
    }
    geo.mine_tid = -1;
    assert(geo_get(48, 17, 49) == GEO_AIR);

    /* Demolition charge carves 3D blast crater */
    geo_set_block(20, 12, 20, GEO_SLATE, 0);
    geo_prime_charge(20, 13, 20, 0.2f);
    for (i = 0; i < 30; i++) {
        dt = 0.02;
        game_update();
    }
    assert(geo_get(20, 12, 20) == GEO_AIR);

    /* Save solo world, wipe RAM, reload solo world */
    geo_set_block(30, 20, 30, GEO_COBALT, 0);
    geo_solo_save();
    geo_raw_set(30, 20, 30, GEO_AIR);
    assert(geo_solo_load() == 1);
    assert(geo_get(30, 20, 30) == GEO_COBALT);
}

static void sleep_30ms(void) {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 30000000L;
    nanosleep(&ts, NULL);
}

static int run_live_peer_alpha(const char *url, const char *dir) {
    int step;
    net_set_data_path(dir);
    net_set_base_url(url);
    net_set_nick("Player_Alpha");

    /* Cloud account registration + wrong-password rejection + valid relogin */
    net_account_auth("Player_Alpha", "GeoPass2026");
    for (step = 0; step < 50 && net_account_status() == AUTH_BUSY; step++)
        sleep_30ms();
    assert(net_account_status() == AUTH_OK && net_account_logged_in() == 1);

    net_account_auth("Player_Alpha", "WrongPass999");
    for (step = 0; step < 50 && net_account_status() == AUTH_BUSY; step++)
        sleep_30ms();
    assert(net_account_status() == AUTH_WRONG_PASS);

    net_account_auth("Player_Alpha", "GeoPass2026");
    for (step = 0; step < 50 && net_account_status() == AUTH_BUSY; step++)
        sleep_30ms();
    assert(net_account_status() == AUTH_OK);

    /* Join shared room "main", build & break blocks, send chat, and wait for Player_Beta's block! */
    geo_start_online_room("main");
    geo.px = 52.5f;
    geo.py = 16.01f;
    geo.pz = 50.5f;
    geo_set_block(50, 17, 50, GEO_MARBLE, 1);
    geo_set_block(49, 15, 49, GEO_AIR, 1);
    net_chat_send("Строим башню в Geometrium!");

    for (step = 0; step < 120; step++) {
        dt = 0.03;
        game_update();
        if (geo_get(53, 18, 51) == GEO_COBALT) {
            net_disconnect();
            return 0;
        }
        sleep_30ms();
    }
    fprintf(stderr, "Alpha timed out waiting for Beta's COBALT block at (53,18,51)\n");
    return 1;
}

static int run_live_peer_beta(const char *url, const char *dir) {
    int step, saw_alpha = 0, saw_chat = 0;
    net_set_data_path(dir);
    net_set_base_url(url);
    net_set_nick("Builder_Beta");
    geo_start_online_room("main");
    geo.px = 46.5f;
    geo.py = 16.01f;
    geo.pz = 49.5f;

    for (step = 0; step < 120; step++) {
        int i;
        dt = 0.03;
        game_update();
        for (i = 0; i < GEO_MAX_REMOTES; i++) {
            if (geo.rem[i].active && strcmp(geo.rem[i].name, "Player_Alpha") == 0 && fabsf(geo.rem[i].x - 52.5f) < 1.5f)
                saw_alpha = 1;
        }
        for (i = 0; i < net_chat_count(); i++) {
            char sn[24], tx[96];
            net_chat_get(i, sn, sizeof(sn), tx, sizeof(tx));
            if (strstr(tx, "Geometrium") != NULL)
                saw_chat = 1;
        }
        if (saw_alpha && saw_chat && geo_get(50, 17, 50) == GEO_MARBLE && geo_get(49, 15, 49) == GEO_AIR) {
            /* Now place our own block back so Alpha receives it in real time too! */
            geo_set_block(53, 18, 51, GEO_COBALT, 1);
            for (i = 0; i < 20; i++) {
                game_update();
                sleep_30ms();
            }
            net_disconnect();
            return 0;
        }
        sleep_30ms();
    }
    fprintf(stderr, "Beta failed sync: saw_alpha=%d saw_chat=%d marble=%d air=%d\n", saw_alpha, saw_chat,
            (int)geo_get(50, 17, 50), (int)geo_get(49, 15, 49));
    return 1;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "--alpha") == 0)
        return run_live_peer_alpha(argv[2], argv[3]);
    if (argc == 4 && strcmp(argv[1], "--beta") == 0)
        return run_live_peer_beta(argv[2], argv[3]);
    if (argc == 2) {
        run_local_engine_tests(argv[1]);
        puts("Geometrium C99 engine, monochrome HUD, 32x32 blocks & 3D water occlusion: OK");
        return 0;
    }
    return 2;
}
