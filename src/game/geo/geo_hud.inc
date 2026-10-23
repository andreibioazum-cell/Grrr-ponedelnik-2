/* Geometrium In-Game HUD & Multi-Touch Controller:
 * Large ergonomic full-circle action buttons + stadium pills in strict 100% monochrome dark gray. */

typedef struct {
    GeoRect menu_b, fly_b, view_b, pal_b, chat_b;
    float mine_x, mine_y, mine_r;
    float place_x, place_y, place_r;
    float jump_x, jump_y, jump_r;
    float up_x, up_y, up_r;
    float down_x, down_y, down_r;
    float joy_base_x, joy_base_y, joy_r;
    float slot_size, slot_gap, bar_x, bar_y;
} GeoHudLayout;

static void geo_compute_hud(GeoHudLayout *L) {
    float w = (float)screen_w, h = (float)screen_h;
    float sc = h / 1080.0f;
    if (sc < 0.72f)
        sc = 0.72f;
    if (sc > 1.25f)
        sc = 1.25f;

    /* Generous stadium-pill top utility bar */
    L->menu_b = (GeoRect){26.0f * sc, 20.0f * sc, 152.0f * sc, 68.0f * sc};
    L->fly_b  = (GeoRect){192.0f * sc, 20.0f * sc, 168.0f * sc, 68.0f * sc};
    L->view_b = (GeoRect){374.0f * sc, 20.0f * sc, 152.0f * sc, 68.0f * sc};
    L->pal_b  = (GeoRect){540.0f * sc, 20.0f * sc, 174.0f * sc, 68.0f * sc};
    L->chat_b = (GeoRect){728.0f * sc, 20.0f * sc, 148.0f * sc, 68.0f * sc};

    /* Large full-circle right-thumb action cluster (NOT small narrow rectangles!) */
    L->mine_r  = 70.0f * sc;
    L->mine_x  = w - 118.0f * sc;
    L->mine_y  = h - 246.0f * sc;

    L->place_r = 65.0f * sc;
    L->place_x = w - 278.0f * sc;
    L->place_y = h - 188.0f * sc;

    L->jump_r  = 60.0f * sc;
    L->jump_x  = w - 112.0f * sc;
    L->jump_y  = h - 92.0f * sc;

    /* Left-thumb large circular joystick + full-circle vertical flight buttons */
    L->joy_r      = 104.0f * sc;
    L->joy_base_x = 176.0f * sc;
    L->joy_base_y = h - 182.0f * sc;

    L->up_r   = 56.0f * sc;
    L->up_x   = 342.0f * sc;
    L->up_y   = h - 252.0f * sc;

    L->down_r = 56.0f * sc;
    L->down_x = 342.0f * sc;
    L->down_y = h - 118.0f * sc;

    /* Large 74x74 monochrome rounded hotbar slots */
    L->slot_size = 74.0f * sc;
    L->slot_gap  = 8.0f * sc;
    L->bar_x     = (w - (L->slot_size * 9.0f + L->slot_gap * 8.0f)) * 0.5f;
    L->bar_y     = h - L->slot_size - 18.0f * sc;
}

static void geo_draw_play_hud(void) {
    GeoHudLayout L;
    float cx = (float)screen_w * 0.5f, cy = (float)screen_h * 0.5f;
    float jcx, jcy;
    char status_txt[96];
    int i;

    geo_compute_hud(&L);

    /* Crisp monochrome crosshair */
    line(cx - 14.0f, cy, cx + 14.0f, cy, 2.6f, geo_gray(244, 225));
    line(cx, cy - 14.0f, cx, cy + 14.0f, 2.6f, geo_gray(244, 225));

    /* Top utility capsule buttons */
    geo_draw_pill_btn(L.menu_b, geo.lang ? "МЕНЮ" : "MENU", 0, 0.58f);
    geo_draw_pill_btn(L.fly_b, geo.lang ? "ПОЛЁТ" : "FLY", geo.flying, 0.58f);
    geo_draw_pill_btn(L.view_b, geo.lang ? "ВИД" : "VIEW", geo.third_person, 0.58f);
    geo_draw_pill_btn(L.pal_b, geo.lang ? "БЛОКИ" : "BLOCKS", geo.inv_open, 0.58f);
    if (geo.online)
        geo_draw_pill_btn(L.chat_b, geo.lang ? "ЧАТ" : "CHAT", geo.chat_open, 0.58f);

    /* Top-right monochrome world & live player status badge */
    if (geo.online) {
        snprintf(status_txt, sizeof(status_txt),
                 geo.lang ? "%s  •  ИГРОКОВ: %d  •  [%d, %d, %d]" : "%s  •  ONLINE: %d  •  [%d, %d, %d]",
                 net_current_room(), net_online_count(), (int)floorf(geo.px), (int)floorf(geo.py), (int)floorf(geo.pz));
    } else {
        snprintf(status_txt, sizeof(status_txt), geo.lang ? "ОДИНОЧНЫЙ МИР  •  [%d, %d, %d]" : "SOLO WORLD  •  [%d, %d, %d]",
                 (int)floorf(geo.px), (int)floorf(geo.py), (int)floorf(geo.pz));
    }
    {
        float bw = (float)text_ink_width(status_txt) * 0.54f + 38.0f;
        GeoRect badge = {(float)screen_w - bw - 24.0f, 22.0f, bw, 56.0f};
        roundrect(badge.x, badge.y, badge.w, badge.h, 28.0f, geo_gray(30, 215));
        geo_text_center(status_txt, badge.x + badge.w * 0.5f, badge.y + 28.0f, geo_gray(226, 245), 0.54f);
    }

    /* Recent live chat lines in top-right below status pill */
    if (geo.online && net_chat_count() > 0) {
        int total = net_chat_count();
        int from = total > 4 ? total - 4 : 0;
        float ly = 90.0f;
        for (i = from; i < total; i++) {
            char sn[24], tx[96], line_buf[128];
            float lw;
            net_chat_get(i, sn, sizeof(sn), tx, sizeof(tx));
            snprintf(line_buf, sizeof(line_buf), "%s: %s", sn, tx);
            lw = (float)text_ink_width(line_buf) * 0.50f + 28.0f;
            roundrect((float)screen_w - lw - 24.0f, ly, lw, 34.0f, 17.0f, geo_gray(24, 185));
            geo_text_center(line_buf, (float)screen_w - 24.0f - lw * 0.5f, ly + 17.0f, geo_gray(232, 240), 0.50f);
            ly += 40.0f;
        }
    }

    /* Left-thumb large circular dark-gray joystick */
    jcx = (geo.joy_tid >= 0) ? geo.joy_cx : L.joy_base_x;
    jcy = (geo.joy_tid >= 0) ? geo.joy_cy : L.joy_base_y;
    circle(jcx, jcy, L.joy_r + 3.0f, geo_gray(96, 175));
    circle(jcx, jcy, L.joy_r, geo_gray(30, 175));
    ring(jcx, jcy, L.joy_r - 8.0f, 2.0f, geo_gray(68, 150));
    circle(jcx + geo.joy_x * (L.joy_r * 0.54f), jcy + geo.joy_y * (L.joy_r * 0.54f), L.joy_r * 0.42f,
           geo.joy_tid >= 0 ? geo_gray(196, 235) : geo_gray(112, 215));

    if (geo.flying) {
        geo_draw_circle_btn(L.up_x, L.up_y, L.up_r, geo.lang ? "ВВЕРХ" : "UP", geo.up_tid >= 0, 3);
        geo_draw_circle_btn(L.down_x, L.down_y, L.down_r, geo.lang ? "ВНИЗ" : "DOWN", geo.down_tid >= 0, 4);
    }

    /* Right-thumb three big full-circle monochrome dark-gray action buttons */
    geo_draw_circle_btn(L.mine_x, L.mine_y, L.mine_r, geo.lang ? "ЛОМАТЬ" : "BREAK", geo.mine_tid >= 0, 1);
    geo_draw_circle_btn(L.place_x, L.place_y, L.place_r, geo.lang ? "СТАВИТЬ" : "PLACE", geo.place_tid >= 0, 2);
    geo_draw_circle_btn(L.jump_x, L.jump_y, L.jump_r, geo.lang ? "ПРЫЖОК" : "JUMP", geo.jump_tid >= 0, 3);

    /* Selected block name label above hotbar */
    geo_text_center(geo_block_name(geo.hotbar[geo.slot]), cx, L.bar_y - 22.0f, geo_gray(236, 240), 0.58f);

    /* Bottom 9-slot large monochrome dark-gray hotbar */
    roundrect(L.bar_x - 10.0f, L.bar_y - 10.0f, L.slot_size * 9.0f + L.slot_gap * 8.0f + 20.0f, L.slot_size + 20.0f,
              28.0f, geo_gray(24, 215));
    for (i = 0; i < GEO_HOTBAR_SLOTS; i++) {
        float sx = L.bar_x + (float)i * (L.slot_size + L.slot_gap);
        int sel = (i == geo.slot);
        roundrect(sx - 2.0f, L.bar_y - 2.0f, L.slot_size + 4.0f, L.slot_size + 4.0f, 22.0f,
                  sel ? geo_gray(226, 250) : geo_gray(82, 200));
        roundrect(sx, L.bar_y, L.slot_size, L.slot_size, 20.0f, sel ? geo_gray(64, 240) : geo_gray(36, 225));
        geo_draw_block_icon3d(sx + L.slot_size * 0.5f, L.bar_y + L.slot_size * 0.52f, L.slot_size * 0.34f,
                              geo.hotbar[i]);
    }
}

static void geo_draw_palette_overlay(void) {
    float w = (float)screen_w, h = (float)screen_h;
    float pw = w * 0.80f > 1180.0f ? 1180.0f : w * 0.80f;
    float ph = h * 0.78f > 740.0f ? 740.0f : h * 0.78f;
    float px = (w - pw) * 0.5f, py = (h - ph) * 0.5f;
    int cols = 6, i;
    float cell = (pw - 96.0f) / (float)cols;
    GeoRect close_b = {px + pw - 190.0f, py + 20.0f, 166.0f, 60.0f};

    rect(0.0f, 0.0f, w, h, geo_gray(10, 175));
    roundrect(px - 2.0f, py - 2.0f, pw + 4.0f, ph + 4.0f, 32.0f, geo_gray(112, 235));
    roundrect(px, py, pw, ph, 30.0f, geo_gray(28, 245));
    geo_text_center(geo.lang ? "КАТАЛОГ БЛОКОВ GEOMETRIUM (32×32)" : "GEOMETRIUM BLOCK PALETTE (32x32)",
                    px + pw * 0.42f, py + 50.0f, geo_gray(242, 255), 0.72f);
    geo_draw_pill_btn(close_b, geo.lang ? "ГОТОВО" : "CLOSE", 0, 0.58f);

    for (i = 0; i < GEO_PALETTE_COUNT; i++) {
        int col = i % cols, row = i / cols;
        int bid = geo_palette_blocks[i];
        float cx = px + 36.0f + (float)col * (cell + 6.0f);
        float cy = py + 104.0f + (float)row * 144.0f;
        int sel = (geo.hotbar[geo.slot] == bid);
        roundrect(cx - 2.0f, cy - 2.0f, cell - 6.0f + 4.0f, 130.0f, 24.0f, sel ? geo_gray(220, 245) : geo_gray(76, 215));
        roundrect(cx, cy, cell - 6.0f, 126.0f, 22.0f, sel ? geo_gray(62, 240) : geo_gray(40, 230));
        geo_draw_block_icon3d(cx + (cell - 6.0f) * 0.5f, cy + 50.0f, 34.0f, bid);
        geo_text_center(geo_block_name(bid), cx + (cell - 6.0f) * 0.5f, cy + 105.0f, geo_gray(226, 245), 0.44f);
    }
}

static void geo_draw_pause_overlay(void) {
    float cx = (float)screen_w * 0.5f, cy = (float)screen_h * 0.5f;
    GeoRect cont_b = {cx - 230.0f, cy - 60.0f, 460.0f, 76.0f};
    GeoRect exit_b = {cx - 230.0f, cy + 38.0f, 460.0f, 76.0f};
    rect(0.0f, 0.0f, (float)screen_w, (float)screen_h, geo_gray(10, 185));
    roundrect(cx - 280.0f, cy - 165.0f, 560.0f, 315.0f, 32.0f, geo_gray(30, 245));
    geo_text_center(geo.lang ? "ПАУЗА" : "PAUSED", cx, cy - 115.0f, geo_gray(244, 255), 0.82f);
    geo_draw_pill_btn(cont_b, geo.lang ? "ПРОДОЛЖИТЬ ИГРУ" : "RESUME WORLD", 1, 0.64f);
    geo_draw_pill_btn(exit_b, geo.lang ? "ВЫЙТИ В ГЛАВНОЕ МЕНЮ" : "EXIT TO MAIN MENU", 0, 0.64f);
}

static void geo_touch_play(float x, float y, int action, int tid) {
    GeoHudLayout L;
    int i;
    geo_compute_hud(&L);

    if (geo.pause_open) {
        if (action == 0) {
            float cx = (float)screen_w * 0.5f, cy = (float)screen_h * 0.5f;
            GeoRect cont_b = {cx - 230.0f, cy - 60.0f, 460.0f, 76.0f};
            GeoRect exit_b = {cx - 230.0f, cy + 38.0f, 460.0f, 76.0f};
            if (geo_hit_rect(x, y, cont_b)) {
                geo.pause_open = 0;
                geo_play_sfx("sounds/ui_tap.wav", 0.8f);
            } else if (geo_hit_rect(x, y, exit_b)) {
                if (!geo.online)
                    geo_solo_save();
                net_disconnect();
                geo.online = 0;
                geo.pause_open = 0;
                geo.screen = SCR_MENU;
                geo_init_world(0, "panorama", 0);
                net_rooms_refresh();
                geo_play_sfx("sounds/ui_tap.wav", 0.8f);
            }
        }
        return;
    }

    if (geo.inv_open) {
        if (action == 0) {
            float w = (float)screen_w, h = (float)screen_h;
            float pw = w * 0.80f > 1180.0f ? 1180.0f : w * 0.80f;
            float ph = h * 0.78f > 740.0f ? 740.0f : h * 0.78f;
            float px = (w - pw) * 0.5f, py = (h - ph) * 0.5f;
            float cell = (pw - 96.0f) / 6.0f;
            GeoRect close_b = {px + pw - 190.0f, py + 20.0f, 166.0f, 60.0f};
            if (geo_hit_rect(x, y, close_b)) {
                geo.inv_open = 0;
                geo_play_sfx("sounds/ui_tap.wav", 0.8f);
                return;
            }
            for (i = 0; i < GEO_PALETTE_COUNT; i++) {
                float cx = px + 36.0f + (float)(i % 6) * (cell + 6.0f);
                float cy = py + 104.0f + (float)(i / 6) * 144.0f;
                if (geo_hit_rect(x, y, (GeoRect){cx, cy, cell - 6.0f, 126.0f})) {
                    geo.hotbar[geo.slot] = geo_palette_blocks[i];
                    geo.inv_open = 0;
                    geo_play_sfx("sounds/ui_tap.wav", 0.8f);
                    return;
                }
            }
        }
        return;
    }

    if (geo.chat_open) {
        if (action == 0) {
            float cx = (float)screen_w * 0.5f, cy = (float)screen_h * 0.5f;
            GeoRect inp_b = {cx - 380.0f, cy + 150.0f, 520.0f, 68.0f};
            GeoRect send_b = {cx + 156.0f, cy + 150.0f, 224.0f, 68.0f};
            GeoRect close_b = {cx + 220.0f, cy - 235.0f, 168.0f, 58.0f};
            if (geo_hit_rect(x, y, close_b)) {
                geo.chat_open = 0;
                keyboard_hide();
                geo.kb_mode = KB_NONE;
            } else if (geo_hit_rect(x, y, inp_b)) {
                geo.kb_mode = KB_CHAT;
                keyboard_clear();
                keyboard_show();
            } else if (geo_hit_rect(x, y, send_b)) {
                char buf[96];
                keyboard_get_text_to(buf, sizeof(buf));
                if (buf[0])
                    net_chat_send(buf);
                keyboard_clear();
                keyboard_hide();
                geo.kb_mode = KB_NONE;
            }
        }
        return;
    }

    if (action == 0) {
        if (geo_hit_rect(x, y, L.menu_b)) {
            geo.pause_open = 1;
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
            return;
        }
        if (geo_hit_rect(x, y, L.fly_b)) {
            geo.flying = !geo.flying;
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
            return;
        }
        if (geo_hit_rect(x, y, L.view_b)) {
            geo.third_person = !geo.third_person;
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
            return;
        }
        if (geo_hit_rect(x, y, L.pal_b)) {
            geo.inv_open = 1;
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
            return;
        }
        if (geo.online && geo_hit_rect(x, y, L.chat_b)) {
            geo.chat_open = 1;
            geo.kb_mode = KB_CHAT;
            keyboard_clear();
            keyboard_show();
            geo_play_sfx("sounds/ui_tap.wav", 0.8f);
            return;
        }
        for (i = 0; i < GEO_HOTBAR_SLOTS; i++) {
            float sx = L.bar_x + (float)i * (L.slot_size + L.slot_gap);
            if (geo_hit_rect(x, y, (GeoRect){sx, L.bar_y, L.slot_size, L.slot_size})) {
                geo.slot = i;
                geo_play_sfx("sounds/ui_tap.wav", 0.7f);
                return;
            }
        }
        if (geo_hit_circle(x, y, L.mine_x, L.mine_y, L.mine_r * 1.12f)) {
            geo.mine_tid = tid;
            return;
        }
        if (geo_hit_circle(x, y, L.place_x, L.place_y, L.place_r * 1.12f)) {
            geo.place_tid = tid;
            geo.place_tap_queued = 1;
            return;
        }
        if (geo_hit_circle(x, y, L.jump_x, L.jump_y, L.jump_r * 1.12f)) {
            geo.jump_tid = tid;
            return;
        }
        if (geo.flying && geo_hit_circle(x, y, L.up_x, L.up_y, L.up_r * 1.12f)) {
            geo.up_tid = tid;
            return;
        }
        if (geo.flying && geo_hit_circle(x, y, L.down_x, L.down_y, L.down_r * 1.12f)) {
            geo.down_tid = tid;
            return;
        }
        if (x < (float)screen_w * 0.42f && y > (float)screen_h * 0.25f) {
            geo.joy_tid = tid;
            geo.joy_cx = x;
            geo.joy_cy = y;
            geo.joy_x = geo.joy_y = 0.0f;
            return;
        }
        if (geo.look_tid < 0) {
            geo.look_tid = tid;
            geo.look_lx = x;
            geo.look_ly = y;
        }
    } else if (action == 2) {
        if (tid == geo.joy_tid) {
            float dx = (x - geo.joy_cx) / L.joy_r, dy = (y - geo.joy_cy) / L.joy_r;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 1.0f) {
                dx /= len;
                dy /= len;
            }
            geo.joy_x = dx;
            geo.joy_y = dy;
        } else if (tid == geo.look_tid) {
            float sens = 0.0046f * net_look_sens();
            geo.yaw -= (x - geo.look_lx) * sens;
            geo.pitch -= (y - geo.look_ly) * sens;
            if (geo.pitch > 1.50f)
                geo.pitch = 1.50f;
            if (geo.pitch < -1.50f)
                geo.pitch = -1.50f;
            geo.look_lx = x;
            geo.look_ly = y;
        }
    } else {
        if (tid == geo.joy_tid) {
            geo.joy_tid = -1;
            geo.joy_x = geo.joy_y = 0.0f;
        }
        if (tid == geo.look_tid)
            geo.look_tid = -1;
        if (tid == geo.mine_tid)
            geo.mine_tid = -1;
        if (tid == geo.place_tid)
            geo.place_tid = -1;
        if (tid == geo.jump_tid)
            geo.jump_tid = -1;
        if (tid == geo.up_tid)
            geo.up_tid = -1;
        if (tid == geo.down_tid)
            geo.down_tid = -1;
    }
}
