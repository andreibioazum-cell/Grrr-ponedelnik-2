/* geo_home_ui.inc - HOME PAGE: a Roblox-style games catalog.
 * Every live world in Firebase is somebody's game here: tap a card to
 * play it, or WATCH to fly through it as a spectator. Below: create
 * your own game in the editor, your account, settings. */

static void geo_draw_home(float cx, float cy, float w, float h) {
    GeoRoomEntry list[GEO_MAX_ROOMS];
    int n = net_rooms_list(list, 6), i;
    char prof_label[64];
    GeoRect create_b = {cx - 380.0f, cy + 176.0f, 372.0f, 70.0f};
    GeoRect prof_b = {cx + 8.0f, cy + 176.0f, 372.0f, 70.0f};
    GeoRect info_b = {cx - 186.0f, cy + 258.0f, 372.0f, 56.0f};
    (void)w;
    (void)h;

    geo_text_center(geo.lang ? "М И Р Ы  3 D" : "3 D  W O R L D S", cx, cy - 218.0f, geo_gray(248, 255), 1.30f);
    geo_text_center(geo.lang ? "ГЛАВНАЯ  •  ИГРЫ ЛЮДЕЙ, РЕДАКТОР И ОНЛАЙН" : "HOME  •  PLAYER GAMES, EDITOR & ONLINE",
                    cx, cy - 168.0f, geo_gray(198, 238), 0.56f);

    if (n == 0) {
        geo_text_center(geo.lang ? "Пока нет живых игр — создайте первую кнопкой ниже!"
                                  : "No live games yet - create the first one below!",
                        cx, cy - 60.0f, geo_gray(210, 240), 0.58f);
    }
    for (i = 0; i < n; i++) {
        char label[96];
        float ry = cy - 128.0f + (float)i * 76.0f;
        roundrect(cx - 384.0f, ry - 30.0f, 768.0f, 64.0f, 20.0f, geo_gray(32, 235));
        snprintf(label, sizeof(label), geo.lang ? "ИГРАТЬ: %s" : "PLAY: %s", list[i].name);
        geo_text_center(label, cx - 130.0f, ry, geo_gray(244, 255), 0.56f);
        snprintf(label, sizeof(label), geo.lang ? "онлайн: %d" : "online: %d", list[i].players);
        geo_text_center(label, cx - 130.0f, ry + 24.0f, geo_gray(180, 225), 0.44f);
        geo_draw_pill_btn((GeoRect){cx + 176.0f, ry - 24.0f, 196.0f, 52.0f}, geo.lang ? "СМОТРЕТЬ" : "WATCH", 0,
                          0.50f);
    }

    geo_draw_pill_btn(create_b, geo.lang ? "+ СОЗДАТЬ ИГРУ (РЕДАКТОР)" : "+ CREATE GAME (EDITOR)", 1, 0.60f);
    snprintf(prof_label, sizeof(prof_label), "%s", net_nick());
    geo_draw_pill_btn(prof_b, prof_label, net_account_logged_in(), 0.56f);
    geo_draw_pill_btn(info_b, geo.lang ? "НАСТРОЙКИ / ПРАВА" : "SETTINGS / LEGAL", 0, 0.52f);
}

static void geo_touch_home(float x, float y, float cx, float cy) {
    GeoRoomEntry list[GEO_MAX_ROOMS];
    int n = net_rooms_list(list, 6), i;
    for (i = 0; i < n; i++) {
        float ry = cy - 128.0f + (float)i * 76.0f;
        if (geo_hit_rect(x, y, (GeoRect){cx + 176.0f, ry - 24.0f, 196.0f, 52.0f})) {
            geo.spectate_sel = 1;
            geo_close_keyboard(0);
            geo_play_sfx("sounds/ui_tap.wav", 0.85f);
            geo_start_online_room(list[i].name);
            return;
        }
        if (geo_hit_rect(x, y, (GeoRect){cx - 384.0f, ry - 30.0f, 540.0f, 64.0f})) {
            geo.spectate_sel = 0;
            geo_close_keyboard(0);
            geo_play_sfx("sounds/ui_tap.wav", 0.85f);
            geo_start_online_room(list[i].name);
            return;
        }
    }
    if (geo_hit_rect(x, y, (GeoRect){cx - 380.0f, cy + 176.0f, 372.0f, 70.0f})) {
        net_disconnect();
        geo_init_editor_world();
        geo.screen = SCR_PLAY;
        geo_play_sfx("sounds/ui_tap.wav", 0.85f);
    } else if (geo_hit_rect(x, y, (GeoRect){cx + 8.0f, cy + 176.0f, 372.0f, 70.0f})) {
        snprintf(geo.edit_nick, sizeof(geo.edit_nick), "%s", net_nick());
        geo.screen = SCR_PROFILE;
        geo_play_sfx("sounds/ui_tap.wav", 0.8f);
    } else if (geo_hit_rect(x, y, (GeoRect){cx - 186.0f, cy + 258.0f, 372.0f, 56.0f})) {
        geo.screen = SCR_INFO;
        geo_play_sfx("sounds/ui_tap.wav", 0.8f);
    }
}
