/* geo_pause_ui.inc - pause menu + world players (accounts) overlay. */

static void geo_draw_pause_overlay(void) {
    float cx = (float)screen_w * 0.5f, cy = (float)screen_h * 0.5f;
    GeoRect cont_b = {cx - 230.0f, cy - 92.0f, 460.0f, 68.0f};
    GeoRect exit_b = {cx - 230.0f, cy + 62.0f, 460.0f, 68.0f};
    rect(0.0f, 0.0f, (float)screen_w, (float)screen_h, geo_gray(10, 185));
    if (geo.players_open) {
        /* Other accounts in this world: nick, model, current animation */
        int i, row = 0;
        char line[96];
        roundrect(cx - 340.0f, cy - 240.0f, 680.0f, 480.0f, 32.0f, geo_gray(30, 245));
        geo_text_center(geo.lang ? "ИГРОКИ МИРА" : "PLAYERS IN THIS WORLD", cx, cy - 190.0f,
                        geo_gray(244, 255), 0.70f);
        snprintf(line, sizeof(line), "%s — %s / %s", net_nick(),
                 geo.lang ? geo_models[geo.av_model].name_ru : geo_models[geo.av_model].name_en,
                 geo_anim_name(geo.anim_id));
        geo_text_center(line, cx, cy - 140.0f, geo_gray(220, 250), 0.54f);
        for (i = 0; i < GEO_MAX_REMOTES && row < 8; i++) {
            if (!geo.rem[i].active)
                continue;
            snprintf(line, sizeof(line), "%s — %s / %s", geo.rem[i].name,
                     geo.lang ? geo_models[geo.rem[i].model].name_ru : geo_models[geo.rem[i].model].name_en,
                     geo_anim_name(geo.rem[i].anim));
            geo_text_center(line, cx, cy - 96.0f + (float)row * 38.0f, geo_gray(205, 240), 0.52f);
            row++;
        }
        if (!row)
            geo_text_center(geo.lang ? "(пока никого - позовите друзей в эту комнату)" : "(nobody yet - invite friends)",
                            cx, cy - 60.0f, geo_gray(170, 225), 0.52f);
        geo_draw_pill_btn((GeoRect){cx - 160.0f, cy + 168.0f, 320.0f, 60.0f}, geo.lang ? "ГОТОВО" : "DONE", 1, 0.58f);
        return;
    }
    roundrect(cx - 280.0f, cy - 185.0f, 560.0f, 350.0f, 32.0f, geo_gray(30, 245));
    geo_text_center(geo.lang ? "ПАУЗА" : "PAUSED", cx, cy - 135.0f, geo_gray(244, 255), 0.82f);
    geo_draw_pill_btn(cont_b, geo.lang ? "ПРОДОЛЖИТЬ ИГРУ" : "RESUME WORLD", 1, 0.64f);
    geo_draw_pill_btn((GeoRect){cx - 230.0f, cy - 8.0f, 460.0f, 60.0f},
                      geo.lang ? "ИГРОКИ МИРА (АККАУНТЫ)" : "WORLD PLAYERS (ACCOUNTS)", 0, 0.58f);
    geo_draw_pill_btn(exit_b, geo.lang ? "ВЫЙТИ В ГЛАВНОЕ МЕНЮ" : "EXIT TO MAIN MENU", 0, 0.64f);
}
