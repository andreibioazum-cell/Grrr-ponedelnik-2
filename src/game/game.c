/* Geometrium — Original 3D Voxel Sandbox & Live Multiplayer Game (Pure C99 + Vulkan).
 * Copyright (c) 2026 Geometrium. All rights reserved. */
#include "game/game.h"
#include "engine/network.h"
#include "engine/runtime.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geo/geo_defs.inc"
#include "geo/geo_math3d.inc"
#include "geo/geo_worldgen.inc"
#include "geo/geo_world_edit.inc"
#include "geo/geo_studio.inc"
#include "geo/geo_script_glue.inc"
#include "geo/geo_render.inc"
#include "geo/geo_hud.inc"
#include "geo/geo_pause_ui.inc"
#include "geo/geo_ui.inc"
#include "geo/geo_anim_ui.inc"
#include "geo/geo_home_ui.inc"
#include "geo/geo_editor_ui.inc"

void game_init(struct AAssetManager *assets) {
    int i;
    char crack_name[24];
    ds_set_asset_manager(assets);
    (void)ds_sound_init(assets);

    net_profile_load();
    {
        int mdl = 0, sk = 0, s;
        net_avatar_load(&mdl, &sk);
        geo.av_model = mdl >= 0 && mdl < GEO_MODEL_COUNT ? mdl : 0;
        for (s = 0; s < SLOT_COUNT; s++) {
            int v = (sk >> (s * 4)) & 15;
            geo.av_skin[s] = v < GEO_SKIN_COUNT ? v : geo_skin_default[s];
        }
        net_custom_anim_load(&geo_custom_poses[0][0], geo_custom_on);
    }
    geo.lang = net_language();
    geo.screen = net_warning_accepted() ? SCR_MENU : SCR_WARNING;
    geo.slot = 0;
    geo.look_tid = -1;
    geo.joy_tid = -1;
    geo.mine_tid = -1;
    geo.place_tid = -1;
    geo.jump_tid = -1;
    geo.up_tid = -1;
    geo.down_tid = -1;
    for (i = 0; i < GEO_HOTBAR_SLOTS; i++)
        geo.hotbar[i] = geo_default_hotbar[i];

    /* Preload all 32x32 block, avatar, and fracture textures + Geometrium sounds */
    for (i = 1; i < GEO_BLOCK_COUNT; i++) {
        (void)png_load(geo_blocks[i].top_tex);
        (void)png_load(geo_blocks[i].side_tex);
        (void)png_load(geo_blocks[i].bot_tex);
    }
    (void)png_load("avatar_head.png");
    (void)png_load("avatar_body.png");
    (void)png_load("avatar_limb.png");
    for (i = 0; i < 10; i++) {
        snprintf(crack_name, sizeof(crack_name), "crack_%d.png", i);
        (void)png_load(crack_name);
    }

    (void)snd_load("sounds/geo_ambient.wav");
    (void)snd_load("sounds/ui_tap.wav");
    (void)snd_load("sounds/block_break.wav");
    (void)snd_load("sounds/block_place.wav");
    (void)snd_load("sounds/step.wav");
    (void)snd_load("sounds/jump.wav");
    (void)snd_load("sounds/charge_boom.wav");
    (void)snd_load("sounds/water_splash.wav");

    if (net_sound_volume() > 0.02f) {
        (void)snd_loop("sounds/geo_ambient.wav");
        snd_volume("sounds/geo_ambient.wav", (double)(net_sound_volume() * 0.45f));
    }

    /* Build background architectural 3D panorama world for the main menu */
    geo_init_world(0, "panorama", 0);
    net_rooms_refresh();
}

void game_update(void) {
    float fdt = (float)dt;
    if (fdt <= 0.0f)
        fdt = 0.016f;
    if (fdt > 0.06f)
        fdt = 0.06f;
    geo.time_sec += fdt;

    if (geo.kb_mode != KB_NONE)
        geo_poll_keyboard();

    if (geo.screen == SCR_PLAY) {
        geo_update_gameplay(fdt);
        geo_script_engine_tick(fdt);   /* user C scripts + animation state */
    }
}

void game_draw(struct Buffer *buffer) {
    (void)buffer;
    if (screen_w < 64)
        screen_w = 1920;
    if (screen_h < 64)
        screen_h = 1080;

    if (geo.screen == SCR_PLAY) {
        geo_render_play_frame();
        if (geo.chat_open)
            geo_draw_chat_overlay();
        else if (geo.pause_open)
            geo_draw_pause_overlay();
        else
            geo_draw_play_hud();
    } else {
        geo_render_menu_background();
        geo_draw_menu_screens();
    }
}

void game_touch(float x, float y, int action, int pointer_id) {
    if (geo.screen == SCR_PLAY)
        geo_touch_play(x, y, action, pointer_id);
    else
        geo_touch_menu(x, y, action, pointer_id);
}

int game_back(void) {
    if (geo.kb_mode != KB_NONE) {
        geo_close_keyboard(0);
        return 1;
    }
    if (geo.screen == SCR_PLAY) {
        if (geo.inv_open) {
            geo.inv_open = 0;
            return 1;
        }
        if (geo.chat_open) {
            geo.chat_open = 0;
            return 1;
        }
        geo.pause_open = !geo.pause_open;
        return 1;
    }
    if (geo.screen != SCR_MENU && geo.screen != SCR_WARNING) {
        geo.screen = SCR_MENU;
        return 1;
    }
    return 0;
}

void game_reset(void) {
    net_disconnect();
    memset(&geo, 0, sizeof(geo));
}

void game_bye(void) {
    if (geo.screen == SCR_PLAY && !geo.online)
        geo_solo_save();
    net_disconnect();
    net_profile_save();
}
