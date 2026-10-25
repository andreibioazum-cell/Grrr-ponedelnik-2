/* geo_editor_ui.inc - Roblox Studio style editor interface:
 * second toolbar row (Character / Animation / Scripts / Play-test /
 * Publish) plus the C scripts manager screen. */

static void geo_editor_row2_rects(GeoRect out[5]) {
    float w = (float)screen_w, y = (float)screen_h - 268.0f;
    float bw = 176.0f, gap = 10.0f, x0 = (w - (bw * 5.0f + gap * 4.0f)) * 0.5f;
    int i;
    for (i = 0; i < 5; i++)
        out[i] = (GeoRect){x0 + (float)i * (bw + gap), y, bw, 56.0f};
}

static void geo_draw_editor_toolbar(void) {
    static const char *ru[5] = {"ПЕРСОНАЖ", "АНИМАЦИЯ", "СКРИПТЫ", "ТЕСТ ▶", "ПУБЛИШ"};
    static const char *en[5] = {"CHARACTER", "ANIMATION", "SCRIPTS", "PLAY ▶", "PUBLISH"};
    GeoRect pr[5];
    int i;
    geo_editor_row2_rects(pr);
    for (i = 0; i < 5; i++)
        geo_draw_pill_btn(pr[i], geo.lang ? ru[i] : en[i], i == 4, 0.50f);
    geo_text_center(geo.lang ? "РЕДАКТОР: стройте Part-ами, персонажем, анимациями и скриптами"
                              : "EDITOR: build with Parts, character, animations and scripts",
                    (float)screen_w * 0.5f, (float)screen_h - 282.0f, geo_gray(235, 245), 0.50f);
}

static void geo_editor_publish(void) {
    char room[24];
    snprintf(room, sizeof(room), "%s", geo.edit_room[0] ? geo.edit_room : "mygame");
    geo.spectate_sel = 0;
    geo_start_online_room(room);
    geo.editor_mode = 1;   /* init_world reset it: stay in the editor */
    geo.build_mode = 1;
    net_publish_game();
    geo_play_sfx("sounds/ui_tap.wav", 0.9f);
}

static void geo_editor_action(int i) {
    if (i == 0) {
        geo.studio_return = 1;
        geo.screen = SCR_STUDIO;
    } else if (i == 1) {
        geo.studio_return = 1;
        geo.screen = SCR_ANIM;
    } else if (i == 2) {
        geo.screen = SCR_SCRIPTS;
    } else if (i == 3) {
        geo.editor_mode = 0;   /* play-test your game */
        geo.build_mode = 0;
    } else if (i == 4) {
        geo_editor_publish();
        return;
    }
    geo_play_sfx("sounds/ui_tap.wav", 0.85f);
}

static int geo_touch_editor_toolbar(float x, float y) {
    GeoRect pr[5];
    int i;
    geo_editor_row2_rects(pr);
    for (i = 0; i < 5; i++) {
        if (geo_hit_rect(x, y, pr[i])) {
            geo_editor_action(i);
            return 1;
        }
    }
    return 0;
}

/* ---- C scripts manager ---- */
static void geo_draw_scripts(float cx, float cy) {
    GeoRect back_b = {34.0f, 28.0f, 190.0f, 66.0f};
    char label[96];
    int i;
    roundrect(cx - 430.0f, cy - 250.0f, 860.0f, 520.0f, 32.0f, geo_gray(28, 244));
    geo_draw_pill_btn(back_b, geo.lang ? "НАЗАД" : "BACK", 0, 0.58f);
    geo_text_center(geo.lang ? "СКРИПТЫ НА C" : "C SCRIPTS", cx, cy - 196.0f, geo_gray(246, 255), 0.74f);
    for (i = 0; i < GEO_SCRIPT_COUNT; i++) {
        float ry = cy - 140.0f + (float)i * 72.0f;
        snprintf(label, sizeof(label), "%s — %s", geo_scripts[i].name,
                 geo_script_enabled[i] ? (geo.lang ? "ВКЛ" : "ON") : (geo.lang ? "ВЫКЛ" : "OFF"));
        geo_draw_pill_btn((GeoRect){cx - 320.0f, ry, 640.0f, 58.0f}, label, geo_script_enabled[i], 0.54f);
    }
    geo_text_center(geo.lang ? "Свои скрипты: функции на C в src/game/geo/geo_user_scripts.inc"
                              : "Your own scripts: C functions in src/game/geo/geo_user_scripts.inc",
                    cx, cy + 130.0f, geo_gray(195, 235), 0.50f);
    geo_text_center(geo.lang ? "API: блоки, взрывы, Part-ы, игрок (GeoScriptApi, docs/scripting.md)"
                              : "API: blocks, explosions, Parts, player (GeoScriptApi, docs/scripting.md)",
                    cx, cy + 166.0f, geo_gray(195, 235), 0.50f);
}

static void geo_touch_scripts(float x, float y, float cx, float cy) {
    int i;
    if (geo_hit_rect(x, y, (GeoRect){34.0f, 28.0f, 190.0f, 66.0f})) {
        geo.screen = SCR_PLAY;
        geo_play_sfx("sounds/ui_tap.wav", 0.8f);
        return;
    }
    for (i = 0; i < GEO_SCRIPT_COUNT; i++) {
        float ry = cy - 140.0f + (float)i * 72.0f;
        if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, ry, 640.0f, 58.0f})) {
            geo_script_enabled[i] = !geo_script_enabled[i];
            if (geo_script_enabled[i] && geo_scripts_started && geo_scripts[i].on_start)
                geo_scripts[i].on_start((GeoScriptApi *)&geo_script_api_impl);
            geo_play_sfx("sounds/ui_tap.wav", 0.85f);
            return;
        }
    }
}
