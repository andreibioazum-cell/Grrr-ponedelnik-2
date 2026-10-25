/* Geometrium Startup Epilepsy / Unrestricted Player Freedom Warning, New Copyrights,
 * Main Menu, Live Firebase Rooms Browser, Profile/Account Screen, and Live Chat Overlay. */

static void geo_close_keyboard(int apply) {
    char buf[96];
    keyboard_get_text_to(buf, sizeof(buf));
    if (apply && buf[0]) {
        if (geo.kb_mode == KB_NICK) {
            snprintf(geo.edit_nick, sizeof(geo.edit_nick), "%s", buf);
            net_set_nick(buf);
        } else if (geo.kb_mode == KB_PASS) {
            snprintf(geo.edit_pass, sizeof(geo.edit_pass), "%s", buf);
        } else if (geo.kb_mode == KB_ROOM) {
            snprintf(geo.edit_room, sizeof(geo.edit_room), "%s", buf);
        } else if (geo.kb_mode == KB_CHAT) {
            net_chat_send(buf);
        }
    }
    geo.kb_mode = KB_NONE;
    keyboard_hide();
}

static void geo_poll_keyboard(void) {
    char buf[96];
    keyboard_get_text_to(buf, sizeof(buf));
    if (geo.kb_mode == KB_NICK && buf[0])
        snprintf(geo.edit_nick, sizeof(geo.edit_nick), "%s", buf);
    else if (geo.kb_mode == KB_PASS && buf[0])
        snprintf(geo.edit_pass, sizeof(geo.edit_pass), "%s", buf);
    else if (geo.kb_mode == KB_ROOM && buf[0])
        snprintf(geo.edit_room, sizeof(geo.edit_room), "%s", buf);

    if (keyboard_enter_pressed())
        geo_close_keyboard(1);
}

static void geo_start_online_room(const char *room_name) {
    const char *r = (room_name && room_name[0]) ? room_name : GEO_DEFAULT_ROOM;
    net_disconnect();
    geo_init_world(1, r, 0);
    net_connect(NULL, r);
    net_publish_local(geo.px, geo.py, geo.pz, geo.yaw, geo.pitch, 0.0f, geo.hotbar[geo.slot], 0,
                      geo.av_model, geo_skin_pack(), geo.anim_id);
    geo.screen = SCR_PLAY;
}

static void geo_draw_chat_overlay(void) {
    float cx = (float)screen_w * 0.5f, cy = (float)screen_h * 0.5f;
    char typed[96];
    int i, total = net_chat_count();
    int from = total > 8 ? total - 8 : 0;
    GeoRect inp_b = {cx - 380.0f, cy + 150.0f, 520.0f, 68.0f};
    GeoRect send_b = {cx + 156.0f, cy + 150.0f, 224.0f, 68.0f};
    GeoRect close_b = {cx + 220.0f, cy - 235.0f, 168.0f, 58.0f};

    keyboard_get_text_to(typed, sizeof(typed));
    rect(0.0f, 0.0f, (float)screen_w, (float)screen_h, geo_gray(10, 175));
    roundrect(cx - 420.0f, cy - 260.0f, 840.0f, 510.0f, 30.0f, geo_gray(28, 244));
    geo_text_center(geo.lang ? "ЖИВОЙ ЧАТ МИРА" : "LIVE WORLD CHAT", cx - 100.0f, cy - 206.0f, geo_gray(244, 255), 0.72f);
    geo_draw_pill_btn(close_b, geo.lang ? "ЗАКРЫТЬ" : "CLOSE", 0, 0.56f);

    for (i = from; i < total; i++) {
        char sn[24], tx[96], line_buf[128];
        float ry = cy - 150.0f + (float)(i - from) * 35.0f;
        net_chat_get(i, sn, sizeof(sn), tx, sizeof(tx));
        snprintf(line_buf, sizeof(line_buf), "%s: %s", sn, tx);
        text_scaled(line_buf, cx - 375.0f, ry, geo_gray(228, 245), 0.56f);
    }

    roundrect(inp_b.x, inp_b.y, inp_b.w, inp_b.h, 34.0f, geo_gray(46, 240));
    geo_text_center(typed[0] ? typed : (geo.lang ? "Нажмите для ввода сообщения..." : "Tap to write a message..."),
                    inp_b.x + inp_b.w * 0.5f, inp_b.y + 34.0f, typed[0] ? geo_gray(245, 255) : geo_gray(150, 220), 0.54f);
    geo_draw_pill_btn(send_b, geo.lang ? "ОТПРАВИТЬ" : "SEND", 1, 0.58f);
}

static void geo_draw_warning_content(float cx, float cy, int is_startup) {
    GeoRect ok_b = {cx - 240.0f, cy + 178.0f, 480.0f, 78.0f};
    roundrect(cx - 492.0f, cy - 272.0f, 984.0f, 548.0f, 34.0f, geo_gray(108, 235));
    roundrect(cx - 490.0f, cy - 270.0f, 980.0f, 544.0f, 32.0f, geo_gray(26, 246));

    geo_text_center("GEOMETRIUM  —  ВАЖНОЕ ПРЕДУПРЕЖДЕНИЕ", cx, cy - 212.0f, geo_gray(246, 255), 0.76f);

    if (geo.lang) {
        geo_text_center("1. Полная свобода действий: в онлайн-мирах Geometrium игроки могут", cx, cy - 140.0f,
                        geo_gray(224, 248), 0.55f);
        geo_text_center("строить, ломать и делать вообще всё что угодно без премодерации.", cx, cy - 104.0f,
                        geo_gray(224, 248), 0.55f);
        geo_text_center("2. Риск фоточувствительной эпилепсии: кто-то из игроков может", cx, cy - 48.0f,
                        geo_gray(236, 252), 0.55f);
        geo_text_center("построить резкие мигающие конструкции, стробоскопические схемы или", cx, cy - 12.0f,
                        geo_gray(236, 252), 0.55f);
        geo_text_center("контрастные оптические узоры («построить эпилепсию»).", cx, cy + 24.0f,
                        geo_gray(236, 252), 0.55f);
        geo_text_center("При головокружении или дискомфорте в глазах немедленно выйдите из мира.", cx, cy + 72.0f,
                        geo_gray(186, 235), 0.51f);
        geo_text_center("© 2026 Geometrium. Все права защищены. Оригинальная игра и движок Geometrium.", cx,
                        cy + 124.0f, geo_gray(154, 230), 0.48f);
    } else {
        geo_text_center("1. Unrestricted freedom: in Geometrium online worlds players can build,", cx, cy - 140.0f,
                        geo_gray(224, 248), 0.55f);
        geo_text_center("destroy, and create anything at all without pre-moderation.", cx, cy - 104.0f,
                        geo_gray(224, 248), 0.55f);
        geo_text_center("2. Photosensitive epilepsy warning: players may construct high-contrast", cx, cy - 48.0f,
                        geo_gray(236, 252), 0.55f);
        geo_text_center("flashing patterns, optical illusions, or strobing light structures.", cx, cy - 12.0f,
                        geo_gray(236, 252), 0.55f);
        geo_text_center("Stop playing immediately if you experience dizziness or visual discomfort.", cx, cy + 44.0f,
                        geo_gray(196, 240), 0.52f);
        geo_text_center("Copyright (c) 2026 Geometrium. All rights reserved.", cx, cy + 120.0f, geo_gray(154, 230),
                        0.50f);
    }

    geo_draw_pill_btn(ok_b,
                      is_startup ? (geo.lang ? "ПРИНИМАЮ И ПРОДОЛЖИТЬ" : "I UNDERSTAND — CONTINUE")
                                 : (geo.lang ? "НАЗАД В МЕНЮ" : "BACK TO MENU"),
                      1, 0.64f);
}

static void geo_draw_menu_screens(void) {
    float w = (float)screen_w, h = (float)screen_h;
    float cx = w * 0.5f, cy = h * 0.5f;

    if (geo.screen == SCR_WARNING) {
        rect(0.0f, 0.0f, w, h, geo_gray(12, 185));
        geo_draw_warning_content(cx, cy, 1);
        return;
    }

    if (geo.screen == SCR_INFO) {
        GeoRect lang_b = {w - 290.0f, 28.0f, 256.0f, 64.0f};
        GeoRect vol_b  = {28.0f, 28.0f, 290.0f, 64.0f};
        char vtxt[64];
        snprintf(vtxt, sizeof(vtxt), geo.lang ? "ЗВУК: %d%%" : "SOUND: %d%%", (int)(net_sound_volume() * 100.0f + 0.5f));
        geo_draw_pill_btn(vol_b, vtxt, net_sound_volume() > 0.05f, 0.56f);
        geo_draw_pill_btn(lang_b, geo.lang ? "ЯЗЫК: РУССКИЙ" : "LANG: ENGLISH", 0, 0.56f);
        geo_draw_warning_content(cx, cy + 22.0f, 0);
        return;
    }

    if (geo.screen == SCR_ROOMS) {
        GeoRoomEntry list[GEO_MAX_ROOMS];
        int n = net_rooms_list(list, 8), i;
        GeoRect back_b = {34.0f, 28.0f, 190.0f, 66.0f};
        GeoRect ref_b  = {w - 234.0f, 28.0f, 200.0f, 66.0f};
        GeoRect inp_b  = {cx - 380.0f, cy + 172.0f, 470.0f, 72.0f};
        GeoRect join_b = {cx + 108.0f, cy + 172.0f, 272.0f, 72.0f};

        roundrect(cx - 430.0f, cy - 235.0f, 860.0f, 510.0f, 32.0f, geo_gray(26, 242));
        geo_draw_pill_btn(back_b, geo.lang ? "НАЗАД" : "BACK", 0, 0.58f);
        geo_draw_pill_btn(ref_b, geo.lang ? "ОБНОВИТЬ" : "REFRESH", 0, 0.58f);
        geo_text_center(geo.lang ? "ОНЛАЙН-МИРЫ И КОМНАТЫ GEOMETRIUM" : "GEOMETRIUM ONLINE WORLDS", cx, cy - 186.0f,
                        geo_gray(246, 255), 0.72f);

        for (i = 0; i < n; i++) {
            char label[96];
            float rx = cx - 380.0f + (float)(i % 2) * 392.0f;
            float ry = cy - 130.0f + (float)(i / 2) * 72.0f;
            snprintf(label, sizeof(label), geo.lang ? "%s   [онлайн: %d]" : "%s   [online: %d]", list[i].name,
                     list[i].players);
            geo_draw_pill_btn((GeoRect){rx, ry, 368.0f, 58.0f}, label, strcmp(list[i].name, GEO_DEFAULT_ROOM) == 0,
                              0.54f);
        }

        roundrect(inp_b.x, inp_b.y, inp_b.w, inp_b.h, 36.0f, geo_gray(46, 240));
        geo_text_center(geo.edit_room[0] ? geo.edit_room
                                         : (geo.lang ? "Имя своей комнаты (нажмите)..." : "Custom room name..."),
                        inp_b.x + inp_b.w * 0.5f, inp_b.y + 36.0f,
                        geo.edit_room[0] ? geo_gray(246, 255) : geo_gray(154, 220), 0.54f);
        geo_draw_pill_btn(join_b, geo.lang ? "ВОЙТИ / СОЗДАТЬ" : "JOIN / CREATE", 1, 0.58f);
        return;
    }

    if (geo.screen == SCR_STUDIO) {
        /* Roblox-style character studio: model + materials + emotes. */
        static const char *slot_ru[SLOT_COUNT] = {"ГОЛОВА", "ТЕЛО", "РУКИ", "НОГИ"};
        static const char *slot_en[SLOT_COUNT] = {"HEAD", "TORSO", "ARMS", "LEGS"};
        GeoRect back_b = {34.0f, 28.0f, 190.0f, 66.0f};
        char label[96];
        int s;
        roundrect(cx - 430.0f, cy - 250.0f, 860.0f, 520.0f, 32.0f, geo_gray(28, 244));
        geo_draw_pill_btn(back_b, geo.lang ? "НАЗАД" : "BACK", 0, 0.58f);
        geo_text_center(geo.lang ? "СТУДИЯ ПЕРСОНАЖА" : "CHARACTER STUDIO", cx, cy - 196.0f,
                        geo_gray(246, 255), 0.74f);
        snprintf(label, sizeof(label), geo.lang ? "МОДЕЛЬ: %s  (нажмите — сменить)" : "MODEL: %s  (tap to change)",
                 geo.lang ? geo_models[geo.av_model].name_ru : geo_models[geo.av_model].name_en);
        geo_draw_pill_btn((GeoRect){cx - 320.0f, cy - 150.0f, 640.0f, 66.0f}, label, 1, 0.58f);
        for (s = 0; s < SLOT_COUNT; s++) {
            float ry = cy - 62.0f + (float)s * 62.0f;
            snprintf(label, sizeof(label), "%s: %s", geo.lang ? slot_ru[s] : slot_en[s],
                     geo_skin_tex[geo.av_skin[s] & 7]);
            geo_draw_pill_btn((GeoRect){cx - 320.0f, ry, 640.0f, 52.0f}, label, 0, 0.52f);
        }
        geo_draw_pill_btn((GeoRect){cx - 320.0f, cy + 196.0f, 310.0f, 58.0f},
                          geo.lang ? "ЭМОЦИЯ: ПРИВЕТ" : "EMOTE: WAVE", geo.emote_id == ANIM_WAVE && geo.emote_t > 0.0f,
                          0.54f);
        geo_draw_pill_btn((GeoRect){cx + 10.0f, cy + 196.0f, 310.0f, 58.0f},
                          geo.lang ? "ЭМОЦИЯ: ТАНЕЦ" : "EMOTE: DANCE", geo.emote_id == ANIM_DANCE && geo.emote_t > 0.0f,
                          0.54f);
        return;
    }

    if (geo.screen == SCR_PROFILE) {
        int st = net_account_status();
        const char *status_line = "";
        GeoRect back_b = {34.0f, 28.0f, 190.0f, 66.0f};
        GeoRect nick_b = {cx - 320.0f, cy - 118.0f, 640.0f, 68.0f};
        GeoRect pass_b = {cx - 320.0f, cy - 34.0f, 640.0f, 68.0f};
        GeoRect auth_b = {cx - 320.0f, cy + 50.0f, 410.0f, 70.0f};
        GeoRect out_b  = {cx + 108.0f, cy + 50.0f, 212.0f, 70.0f};
        GeoRect studio_b = {cx - 320.0f, cy + 138.0f, 640.0f, 64.0f};

        roundrect(cx - 390.0f, cy - 236.0f, 780.0f, 480.0f, 32.0f, geo_gray(28, 244));
        geo_draw_pill_btn(back_b, geo.lang ? "НАЗАД" : "BACK", 0, 0.58f);
        geo_text_center(geo.lang ? "ПРОФИЛЬ И ОБЛАЧНЫЙ АККАУНТ" : "PROFILE & CLOUD ACCOUNT", cx, cy - 182.0f,
                        geo_gray(246, 255), 0.72f);

        {
            char nb[64], pb[64];
            snprintf(nb, sizeof(nb), geo.lang ? "НИКНЕЙМ: %s" : "NICKNAME: %s",
                     geo.edit_nick[0] ? geo.edit_nick : net_nick());
            snprintf(pb, sizeof(pb), geo.lang ? "ПАРОЛЬ: %s" : "PASSWORD: %s",
                     geo.edit_pass[0] ? "••••••••" : (geo.lang ? "(нажмите для ввода)" : "(tap to enter)"));
            geo_draw_pill_btn(nick_b, nb, geo.kb_mode == KB_NICK, 0.58f);
            geo_draw_pill_btn(pass_b, pb, geo.kb_mode == KB_PASS, 0.58f);
        }
        geo_draw_pill_btn(auth_b, geo.lang ? "ВОЙТИ / РЕГИСТРАЦИЯ" : "SIGN IN / REGISTER", 1, 0.58f);
        geo_draw_pill_btn(out_b, geo.lang ? "СБРОСИТЬ" : "SIGN OUT", 0, 0.56f);
        geo_draw_pill_btn(studio_b, geo.lang ? "СТУДИЯ ПЕРСОНАЖА (МОДЕЛИ И АНИМАЦИИ)" : "CHARACTER STUDIO (MODELS & ANIMATION)",
                          0, 0.56f);

        if (st == AUTH_BUSY) status_line = geo.lang ? "Синхронизация с сервером..." : "Syncing with cloud...";
        else if (st == AUTH_OK) status_line = geo.lang ? "Аккаунт синхронизирован!" : "Cloud account active!";
        else if (st == AUTH_WRONG_PASS) status_line = geo.lang ? "Неверный пароль для этого ника." : "Wrong password.";
        else if (st == AUTH_BAD_INPUT) status_line = geo.lang ? "Ник от 2 симв., пароль от 4 симв." : "Min 2 nick / 4 pass.";
        else if (st == AUTH_NET_ERR) status_line = geo.lang ? "Ошибка соединения с базой." : "Database connection error.";
        geo_text_center(status_line, cx, cy + 224.0f, geo_gray(214, 240), 0.54f);
        return;
    }

    /* Main Architectural Title & Large Monochrome Dark-Gray Buttons */
    {
        char prof_label[64];
        GeoRect play_on = {cx - 260.0f, cy - 78.0f, 520.0f, 80.0f};
        GeoRect rooms_b = {cx - 260.0f, cy + 18.0f, 520.0f, 76.0f};
        GeoRect solo_b  = {cx - 260.0f, cy + 110.0f, 520.0f, 76.0f};
        GeoRect prof_b  = {cx - 260.0f, cy + 202.0f, 252.0f, 70.0f};
        GeoRect info_b  = {cx + 8.0f, cy + 202.0f, 252.0f, 70.0f};

        geo_text_center("G E O M E T R I U M", cx, cy - 196.0f, geo_gray(248, 255), 1.25f);
        geo_text_center(geo.lang ? "ВОКСЕЛЬНАЯ ПЕСОЧНИЦА БЕЗ ГРАНИЦ  •  ОНЛАЙН И СТРОИТЕЛЬСТВО"
                                 : "UNRESTRICTED 3D VOXEL SANDBOX  •  LIVE MULTIPLAYER",
                        cx, cy - 134.0f, geo_gray(196, 235), 0.54f);

        snprintf(prof_label, sizeof(prof_label), "%s", net_nick());
        geo_draw_pill_btn(play_on, geo.lang ? "ИГРАТЬ ОНЛАЙН (ОБЩИЙ МИР)" : "PLAY ONLINE (SHARED WORLD)", 1, 0.66f);
        geo_draw_pill_btn(rooms_b, geo.lang ? "СПИСОК МИРОВ И КОМНАТ" : "WORLDS & ROOMS BROWSER", 0, 0.62f);
        geo_draw_pill_btn(solo_b, geo.lang ? "ОДИНОЧНЫЙ МИР" : "SOLO SANDBOX WORLD", 0, 0.62f);
        geo_draw_pill_btn(prof_b, prof_label, net_account_logged_in(), 0.56f);
        geo_draw_pill_btn(info_b, geo.lang ? "НАСТРОЙКИ / ПРАВА" : "SETTINGS / LEGAL", 0, 0.54f);

        geo_text_center("© 2026 Geometrium. Все права защищены.", cx, h - 26.0f, geo_gray(158, 215), 0.46f);
    }
}

static void geo_touch_menu(float x, float y, int action, int tid) {
    float w = (float)screen_w, h = (float)screen_h;
    float cx = w * 0.5f, cy = h * 0.5f;
    (void)tid;
    if (action != 0)
        return;

    if (geo.screen == SCR_WARNING) {
        if (geo_hit_rect(x, y, (GeoRect){cx - 240.0f, cy + 178.0f, 480.0f, 78.0f})) {
            net_accept_warning();
            geo.screen = SCR_MENU;
            geo_play_sfx("sounds/ui_tap.wav", 0.85f);
        }
        return;
    }

    if (geo.screen == SCR_INFO) {
        if (geo_hit_rect(x, y, (GeoRect){28.0f, 28.0f, 290.0f, 64.0f})) {
            float nv = net_sound_volume() > 0.05f ? 0.0f : 0.8f;
            net_set_sound_volume(nv);
            if (nv <= 0.02f)
                snd_stop("sounds/geo_ambient.wav");
            else {
                (void)snd_loop("sounds/geo_ambient.wav");
                snd_volume("sounds/geo_ambient.wav", (double)(nv * 0.45f));
            }
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
        } else if (geo_hit_rect(x, y, (GeoRect){w - 290.0f, 28.0f, 256.0f, 64.0f})) {
            geo.lang = !geo.lang;
            net_set_language(geo.lang);
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
        } else if (geo_hit_rect(x, y, (GeoRect){cx - 240.0f, cy + 200.0f, 480.0f, 78.0f})) {
            geo.screen = SCR_MENU;
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
        }
        return;
    }

    if (geo.screen == SCR_ROOMS) {
        GeoRoomEntry list[GEO_MAX_ROOMS];
        int n = net_rooms_list(list, 8), i;
        if (geo_hit_rect(x, y, (GeoRect){34.0f, 28.0f, 190.0f, 66.0f})) {
            geo_close_keyboard(0);
            geo.screen = SCR_MENU;
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
            return;
        }
        if (geo_hit_rect(x, y, (GeoRect){w - 234.0f, 28.0f, 200.0f, 66.0f})) {
            net_rooms_refresh();
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
            return;
        }
        for (i = 0; i < n; i++) {
            float rx = cx - 380.0f + (float)(i % 2) * 392.0f;
            float ry = cy - 130.0f + (float)(i / 2) * 72.0f;
            if (geo_hit_rect(x, y, (GeoRect){rx, ry, 368.0f, 58.0f})) {
                geo_close_keyboard(0);
                geo_play_sfx("sounds/ui_tap.wav", 0.85f);
                geo_start_online_room(list[i].name);
                return;
            }
        }
        if (geo_hit_rect(x, y, (GeoRect){cx - 380.0f, cy + 172.0f, 470.0f, 72.0f})) {
            geo.kb_mode = KB_ROOM;
            keyboard_clear();
            keyboard_show();
            return;
        }
        if (geo_hit_rect(x, y, (GeoRect){cx + 108.0f, cy + 172.0f, 272.0f, 72.0f})) {
            geo_close_keyboard(1);
            geo_play_sfx("sounds/ui_tap.wav", 0.85f);
            geo_start_online_room(geo.edit_room[0] ? geo.edit_room : GEO_DEFAULT_ROOM);
            return;
        }
        return;
    }

    if (geo.screen == SCR_STUDIO) {
        int s;
        if (geo_hit_rect(x, y, (GeoRect){34.0f, 28.0f, 190.0f, 66.0f})) {
            geo.screen = SCR_PROFILE;
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
        } else if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, cy - 150.0f, 640.0f, 66.0f})) {
            geo.av_model = (geo.av_model + 1) % GEO_MODEL_COUNT;
            net_avatar_save(geo.av_model, geo_skin_pack());
            geo_play_sfx("sounds/ui_tap.wav", 0.85f);
        } else if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, cy + 196.0f, 310.0f, 58.0f})) {
            geo.emote_id = ANIM_WAVE;
            geo.emote_t = 2.4f;
            geo.screen = SCR_MENU;
            geo_play_sfx("sounds/ui_tap.wav", 0.85f);
        } else if (geo_hit_rect(x, y, (GeoRect){cx + 10.0f, cy + 196.0f, 310.0f, 58.0f})) {
            geo.emote_id = ANIM_DANCE;
            geo.emote_t = 3.0f;
            geo.screen = SCR_MENU;
            geo_play_sfx("sounds/ui_tap.wav", 0.85f);
        } else {
            for (s = 0; s < SLOT_COUNT; s++) {
                float ry = cy - 62.0f + (float)s * 62.0f;
                if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, ry, 640.0f, 52.0f})) {
                    geo.av_skin[s] = (geo.av_skin[s] + 1) % GEO_SKIN_COUNT;
                    net_avatar_save(geo.av_model, geo_skin_pack());
                    geo_play_sfx("sounds/ui_tap.wav", 0.85f);
                    break;
                }
            }
        }
        return;
    }

    if (geo.screen == SCR_PROFILE) {
        if (geo_hit_rect(x, y, (GeoRect){34.0f, 28.0f, 190.0f, 66.0f})) {
            geo_close_keyboard(1);
            geo.screen = SCR_MENU;
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
        } else if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, cy - 118.0f, 640.0f, 68.0f})) {
            geo.kb_mode = KB_NICK;
            keyboard_clear();
            keyboard_show();
        } else if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, cy - 34.0f, 640.0f, 68.0f})) {
            geo.kb_mode = KB_PASS;
            keyboard_clear();
            keyboard_show();
        } else if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, cy + 50.0f, 410.0f, 70.0f})) {
            geo_close_keyboard(1);
            net_account_auth(geo.edit_nick[0] ? geo.edit_nick : net_nick(), geo.edit_pass);
            geo_play_sfx("sounds/ui_tap.wav", 0.85f);
        } else if (geo_hit_rect(x, y, (GeoRect){cx + 108.0f, cy + 50.0f, 212.0f, 70.0f})) {
            geo.edit_pass[0] = '\0';
            net_account_logout();
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
        } else if (geo_hit_rect(x, y, (GeoRect){cx - 320.0f, cy + 138.0f, 640.0f, 64.0f})) {
            geo_close_keyboard(1);
            geo.screen = SCR_STUDIO;
            geo_play_sfx("sounds/ui_tap.wav", 0.85f);
        }
        return;
    }

    /* SCR_MENU button actions */
    if (geo_hit_rect(x, y, (GeoRect){cx - 260.0f, cy - 78.0f, 520.0f, 80.0f})) {
        geo_play_sfx("sounds/ui_tap.wav", 0.85f);
        geo_start_online_room(GEO_DEFAULT_ROOM);
    } else if (geo_hit_rect(x, y, (GeoRect){cx - 260.0f, cy + 18.0f, 520.0f, 76.0f})) {
        net_rooms_refresh();
        geo.screen = SCR_ROOMS;
        geo_play_sfx("sounds/ui_tap.wav", 0.8f);
    } else if (geo_hit_rect(x, y, (GeoRect){cx - 260.0f, cy + 110.0f, 520.0f, 76.0f})) {
        net_disconnect();
        geo_init_world(0, "solo", 0);
        geo.screen = SCR_PLAY;
        geo_play_sfx("sounds/ui_tap.wav", 0.85f);
    } else if (geo_hit_rect(x, y, (GeoRect){cx - 260.0f, cy + 202.0f, 252.0f, 70.0f})) {
        snprintf(geo.edit_nick, sizeof(geo.edit_nick), "%s", net_nick());
        geo.screen = SCR_PROFILE;
        geo_play_sfx("sounds/ui_tap.wav", 0.8f);
    } else if (geo_hit_rect(x, y, (GeoRect){cx + 8.0f, cy + 202.0f, 252.0f, 70.0f})) {
        geo.screen = SCR_INFO;
        geo_play_sfx("sounds/ui_tap.wav", 0.8f);
    }
}
