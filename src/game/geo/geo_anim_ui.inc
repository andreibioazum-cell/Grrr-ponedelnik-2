/* geo_anim_ui.inc - custom animation editor screen (draw + touch). */

static void geo_draw_anim(float cx, float cy) {
    /* Custom animation editor: pick a joint, bend it, play it. */
    GeoRect back_b = {34.0f, 28.0f, 190.0f, 66.0f};
    char label[96];
    int j = geo_custom_joint;
    roundrect(cx - 430.0f, cy - 250.0f, 860.0f, 520.0f, 32.0f, geo_gray(28, 244));
    geo_draw_pill_btn(back_b, geo.lang ? "НАЗАД" : "BACK", 0, 0.58f);
    geo_text_center(geo.lang ? "РЕДАКТОР АНИМАЦИИ" : "ANIMATION EDITOR", cx, cy - 196.0f,
                    geo_gray(246, 255), 0.74f);
    snprintf(label, sizeof(label), geo.lang ? "СУСТАВ: %s  (нажмите — сменить)" : "JOINT: %s  (tap to change)",
             geo_joint_name(j));
    geo_draw_pill_btn((GeoRect){cx - 320.0f, cy - 140.0f, 640.0f, 64.0f}, label, 1, 0.56f);
    snprintf(label, sizeof(label), geo.lang ? "УГОЛ: %.1f" : "ANGLE: %.1f", (double)geo_custom_pose[j]);
    geo_text_center(label, cx, cy - 40.0f, geo_gray(240, 255), 0.62f);
    geo_draw_pill_btn((GeoRect){cx - 320.0f, cy + 6.0f, 310.0f, 64.0f}, "- 0.2", 0, 0.60f);
    geo_draw_pill_btn((GeoRect){cx + 10.0f, cy + 6.0f, 310.0f, 64.0f}, "+ 0.2", 0, 0.60f);
    geo_draw_pill_btn((GeoRect){cx - 320.0f, cy + 96.0f, 310.0f, 64.0f},
                      geo.lang ? "ТАНЕЦ (ЭМОЦИЯ)" : "DANCE (EMOTE)", geo.emote_id == ANIM_DANCE && geo.emote_t > 0.0f,
                      0.54f);
    geo_draw_pill_btn((GeoRect){cx + 10.0f, cy + 96.0f, 310.0f, 64.0f},
                      geo.lang ? "ИГРАТЬ СВОЮ В МИРЕ" : "PLAY MINE IN WORLD", geo.emote_id == ANIM_CUSTOM && geo.emote_t > 0.0f,
                      0.54f);
    geo_text_center(geo.lang ? "Своя анимация сохраняется в профиль и видна другим игрокам"
                              : "Your animation is saved to your profile and synced to others",
                    cx, cy + 204.0f, geo_gray(190, 235), 0.50f);
    return;
}

static void geo_touch_anim(float x, float y, float cx, float cy) {
    int j = geo_custom_joint;
    if (geo_hit_rect(x, y, (GeoRect){34.0f, 28.0f, 190.0f, 66.0f})) {
        geo.screen = SCR_STUDIO;
        geo_play_sfx("sounds/ui_tap.wav", 0.8f);
    } else if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, cy - 140.0f, 640.0f, 64.0f})) {
        geo_custom_joint = (j + 1) % GJ_COUNT;
        geo_play_sfx("sounds/ui_tap.wav", 0.85f);
    } else if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, cy + 6.0f, 310.0f, 64.0f})) {
        geo_custom_pose[j] -= 0.2f;
        if (geo_custom_pose[j] < -3.0f)
            geo_custom_pose[j] = -3.0f;
        net_custom_anim_save(geo_custom_pose);
        geo_play_sfx("sounds/ui_tap.wav", 0.85f);
    } else if (geo_hit_rect(x, y, (GeoRect){cx + 10.0f, cy + 6.0f, 310.0f, 64.0f})) {
        geo_custom_pose[j] += 0.2f;
        if (geo_custom_pose[j] > 3.0f)
            geo_custom_pose[j] = 3.0f;
        net_custom_anim_save(geo_custom_pose);
        geo_play_sfx("sounds/ui_tap.wav", 0.85f);
    } else if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, cy + 96.0f, 310.0f, 64.0f})) {
        geo.emote_id = ANIM_DANCE;
        geo.emote_t = 3.0f;
        geo.screen = SCR_MENU;
        geo_play_sfx("sounds/ui_tap.wav", 0.85f);
    } else if (geo_hit_rect(x, y, (GeoRect){cx + 10.0f, cy + 96.0f, 310.0f, 64.0f})) {
        geo.emote_id = ANIM_CUSTOM;
        geo.emote_t = 3.0f;
        geo.screen = SCR_MENU;
        geo_play_sfx("sounds/ui_tap.wav", 0.85f);
    }
    return;
}
