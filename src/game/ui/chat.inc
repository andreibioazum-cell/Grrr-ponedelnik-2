static double chat_close_x(void) {
    return (screen_w - btn_w) / 2;
}

static double chat_top_y(void) {
    return back_y;
}

static double chat_msg_top(void) {
    return chat_top_y() + btn_h + 4;
}

static double chat_input_y(void) {
    return screen_h - 72;
}

static double chat_visible_limit(void) {
    double ly = chat_msg_top();
    double iy = chat_input_y();
    double lh = iy - ly - 12;
    double line_h = 28;
    double maxl = floor(lh / line_h);
    if (maxl < 1) {
        maxl = 1;
    }
    return maxl;
}

static void chat_sync_input(void) {
    keyboard_get_text_to(chat_input_buf, sizeof(chat_input_buf));
    chat_input = chat_input_buf;
}

static double is_admin(void) {
    if (net_login_status() != 2) {
        return 0;
    }
    const char *nick = net_login_nick();
    const char *pass = net_login_pass();
    if (str_eq(nick, ADMIN_NICK) == 1 && str_eq(pass, ADMIN_PASS) == 1) {
        return 1;
    }
    if (str_eq(nick, ADMIN2_NICK) == 1 && str_eq(pass, ADMIN2_PASS) == 1) {
        return 1;
    }
    return 0;
}

static char admin_passwd_target_buf[24];

static const char *admin_actor_nick(void) {
    return admin_nick_of(admin_slot(net_login_nick()));
}

static double admin_handle(const char *msg) {
    if (is_admin() == 0) {
        return 0;
    }
    const char *by = admin_actor_nick();
    if (str_eq(msg, "event 0") == 1) {
        double cur = net_event();
        if (cur == 0) {
            net_event_set(1);
            net_chat_send(ds_concat(ds_concat("ДИСКО-СОБЫТИЕ от ", by), "!"));
            ds_log("admin: event 0 -> disco ON");
        } else {
            net_event_set(0);
            net_chat_send(ds_concat("СОБЫТИЕ ВЫКЛЮЧИЛ ", by));
            ds_log("admin: event 0 -> OFF");
        }
        return 1;
    }
    if (str_eq(msg, "event 1") == 1) {
        net_event_set(1);
        net_chat_send(ds_concat(ds_concat("ДИСКО-СОБЫТИЕ от ", by), "!"));
        ds_log("admin: event 1 disco");
        return 1;
    }
    if (str_eq(msg, "event 2") == 1 || str_eq(msg, "event 3") == 1) {
        net_event_set(2);
        net_chat_send(ds_concat("СОБЫТИЕ «ПЛИТЫ» от ", by));
        ds_log("admin: event 2 plates");
        return 1;
    }
    if (str_eq(msg, "event 4") == 1) {
        net_event_set(4);
        net_chat_send(ds_concat(ds_concat("СОБЫТИЕ «КУБИК» от ", by), "!"));
        ds_log("admin: event 4 dice");
        return 1;
    }
    if (net_chat_is_passwd(msg) == 1) {
        const char *t1 = net_chat_passwd_target(msg);
        const char *np = net_chat_passwd_new(msg);
        if (str_len(t1) > 0 && str_len(np) > 0) {
            if (admin_slot(t1) != 0) {
                net_chat_send(tr_admin_ban_protected());
                ds_log("admin passwd blocked for protected account %s", t1);
                return 1;
            }
            double i = 0;
            while (t1[(int)i] != 0 && i < (double)(sizeof(admin_passwd_target_buf) - 1)) {
                admin_passwd_target_buf[(int)i] = t1[(int)i];
                i = i + 1;
            }
            admin_passwd_target_buf[(int)i] = 0;
            net_admin_reset_password(t1, np);
            net_chat_send(language == 1 ? "Меняю пароль..." : "Changing password...");
            ds_log("admin passwd reset requested for %s", t1);
            return 1;
        }
    }
    if (net_chat_is_ban(msg) == 1) {
        const char *t1 = net_chat_ban_target(msg);
        if (str_len(t1) > 0) {
            if (admin_slot(t1) != 0) {
                net_chat_send(tr_admin_ban_protected());
                ds_log("admin ban blocked for protected account %s", t1);
                return 1;
            }
            net_ban_set(t1, 1);
            net_chat_send(ds_concat(ds_concat(t1, " ЗАБЛОКИРОВАН, админ "), by));
            ds_log("admin ban %s", t1);
            return 1;
        }
    }
    if (net_chat_is_unban(msg) == 1) {
        const char *t2 = net_chat_unban_target(msg);
        if (str_len(t2) > 0) {
            net_ban_set(t2, 0);
            net_chat_send(ds_concat(ds_concat(t2, " РАЗБЛОКИРОВАН, админ "), by));
            ds_log("admin unban %s", t2);
            return 1;
        }
    }
    if (net_chat_is_text_cmd(msg) == 1) {
        const char *btxt = net_chat_text_cmd_text(msg);
        const char *bcol = net_chat_text_cmd_color(msg);
        if (str_len(btxt) > 0) {
            /* Every banner carries its author's nick: "Dimasi4ek229: текст". */
            net_banner_send(ds_concat(ds_concat(by, ": "), btxt), bcol);
            ds_log("admin text: %s (%s)", ds_concat(ds_concat(by, ": "), btxt), bcol);
            return 1;
        }
    }
    return 0;
}

static void chat_send(void) {
    chat_sync_input();
    if (str_len(chat_input) == 0) {
        return;
    }
    if (is_admin() == 1) {
        if (str_eq(chat_input, "event 0") == 1 || str_eq(chat_input, "event 1") == 1 ||
            str_eq(chat_input, "event 2") == 1 || str_eq(chat_input, "event 3") == 1 ||
            str_eq(chat_input, "event 4") == 1) {
            admin_handle(chat_input);
            chat_input = "";
            keyboard_clear();
            return;
        }
        if (net_chat_is_ban(chat_input) == 1 || net_chat_is_unban(chat_input) == 1 ||
            net_chat_is_passwd(chat_input) == 1) {
            admin_handle(chat_input);
            chat_input = "";
            keyboard_clear();
            return;
        }
        if (net_chat_is_text_cmd(chat_input) == 1) {
            admin_handle(chat_input);
            chat_input = "";
            keyboard_clear();
            return;
        }
    }
    /* Мат, ругань и ссылки уходят в чат звёздочками (игра 12+). */
    net_chat_send(chat_clean(chat_input));
    ds_log("chat: %s", chat_input);
    chat_input = "";
    keyboard_clear();
    net_chat_trim(chat_visible_limit());
}

static const char *chat_sticker_tex(const char *msg) {
    const char *n = "";
    if (str_starts_with(msg, STICKER_CMD) == 1) {
        n = str_sub(msg, 9, str_len(msg) - 9);
        if (str_eq(n, STICKER_LIKE) == 1 || str_eq(n, STICKER_DISLIKE) == 1 || str_eq(n, STICKER_75) == 1) {
            return n;
        }
    }
    return "";
}

static void chat_set_open(double v) {
    chat_open = v;
    sticker_menu_open = 0;
    keyboard_hide();
}

static void chat_send_sticker(const char *texname) {
    net_chat_send(ds_concat(STICKER_CMD, texname));
    ds_log("chat sticker: %s", texname);
    chat_set_open(0);
}

static double sticker_btn_x(void) {
    double lx = 16;
    double lw = screen_w - 32;
    return lx + lw - 112 - 8 - STICKER_BTN;
}

static double sticker_menu_w(void) {
    return 12 + 3 * STICKER_TILE + 2 * 12 + 12;
}

static double sticker_menu_h(void) {
    return 12 + STICKER_TILE + 12;
}

static double sticker_menu_x(void) {
    double lx = 16;
    double lw = screen_w - 32;
    return lx + lw - sticker_menu_w();
}

static double sticker_menu_y(void) {
    return chat_input_y() - sticker_menu_h() - 10;
}

static double sticker_tile_x(double i) {
    return sticker_menu_x() + 12 + i * (STICKER_TILE + 12);
}

static double sticker_tile_y(void) {
    return sticker_menu_y() + 12;
}

static const char *sticker_tex_of(double i) {
    if (i == 0) {
        return STICKER_LIKE;
    }
    if (i == 1) {
        return STICKER_DISLIKE;
    }
    return STICKER_75;
}

static void draw_sticker_face(double cx, double cy) {
    /* Clear sticker-sheet icon: a white smiling sticker with a folded corner,
     * rather than the old mouth made from overlapping circle/rectangle shapes. */
    circle(cx, cy, 15, 0xFFFFFFFF);
    ring(cx, cy, 15, 2, 0xFF292944);
    circle(cx - 5, cy - 4, 2.2, 0xFF292944);
    circle(cx + 5, cy - 4, 2.2, 0xFF292944);
    line(cx - 6, cy + 4, cx - 2, cy + 7, 2.2, 0xFF292944);
    line(cx - 2, cy + 7, cx + 3, cy + 7, 2.2, 0xFF292944);
    line(cx + 3, cy + 7, cx + 7, cy + 3, 2.2, 0xFF292944);
    /* Folded lower-right sticker corner. */
    line(cx + 8, cy + 11, cx + 14, cy + 7, 2, 0xFF292944);
    line(cx + 8, cy + 11, cx + 13, cy + 13, 2, 0xFF292944);
}

static void draw_sticker_tile(double i) {
    double tx = sticker_tile_x(i);
    double ty = sticker_tile_y();
    roundrect(tx, ty, STICKER_TILE, STICKER_TILE, 10, 0xFF3A3A58);
    tex(tx + 8, ty + 8, sticker_tex_of(i), 0, 0.4);
}

static void chat_update(void) {
    if (game_state != 5 || chat_open == 0) {
        return;
    }
    {
        double rs = net_admin_reset_status();
        if (rs == 2) {
            net_chat_send(ds_concat(language == 1 ? "Пароль изменён: " : "Password changed: ",
                                    admin_passwd_target_buf));
        } else if (rs >= 3) {
            net_chat_send(language == 1 ? "Не удалось изменить пароль (функция не развёрнута?)"
                                        : "Password change failed (function not deployed?)");
        }
    }
    if (keyboard_visible() == 1) {
        chat_sync_input();
    }
    if (keyboard_enter_pressed() == 1) {
        chat_send();
    }
    if (net_chat_count() > chat_visible_limit()) {
        net_chat_trim(chat_visible_limit());
    }
}

static double chat_btn_x(void) {
    return 16;
}

static double chat_btn_y(void) {
    return 108;
}

static double chat_btn_w(void) {
    return btn_w;
}

static double chat_btn_h(void) {
    return btn_h;
}

static void draw_chat(void) {
    if (game_state != 5) {
        return;
    }
    if (chat_open == 0) {
        button(tr_chat(), chat_btn_x(), chat_btn_y(), chat_btn_w(), chat_btn_h());
        return;
    }
    screen_fill();
    double cx = chat_close_x();
    /* The open chat covers the fight, so its own buttons claim the press. */
    press_overlay = 1;
    button(tr_close(), cx, chat_top_y(), btn_w, btn_h);
    /* Header: the room this chat belongs to. Taken from the connection
     * itself (net_current_room), so after creating or joining a room the
     * title shows exactly that room's name, never a stale one. */
    const char *room_title = net_current_room();
    if (str_len(room_title) == 0) {
        room_title = ROOM;
    }
    const char *room_header = ds_concat(language == 1 ? "Комната: " : "Room: ", room_title);
    text_scaled(room_header, 16, chat_top_y() + 16, text_color,
                fit_text_scale(room_header, (screen_w - btn_w) / 2 - 24, 0.6));
    double lx = 16;
    double lw = screen_w - 32;
    double iy = chat_input_y();
    double ih = 52;
    double ly = chat_msg_top();
    double line_h = 28;
    double maxl = chat_visible_limit();
    if (net_chat_count() > maxl) {
        net_chat_trim(maxl);
    }
    double total = net_chat_count();
    double start = total - maxl;
    if (start < 0) {
        start = 0;
    }
    if (total == 0) {
        text_scaled(tr_no_messages(), lx, ly, text_color, 0.5);
    } else {
        double idx = start;
        double yy = ly;
        while (idx < total) {
            if (yy + line_h > iy - 8) {
                idx = total;
            } else {
                const char *uid = net_chat_uid(idx);
                const char *st = chat_sticker_tex(net_chat_text(idx));
                if (str_len(st) > 0) {
                    tex(lx + 8, yy + (line_h - 24) / 2, st, 0, 0.24);
                } else {
                    draw_nick_msg(uid, ds_concat(": ", chat_clean(net_chat_text(idx))), lx, yy, text_color, 0.5);
                }
                yy = yy + line_h;
                idx = idx + 1;
            }
        }
    }
    roundrect(lx, iy, lw - 122, ih, 10, card_bg);
    chat_sync_input();
    const char *shown = chat_input;
    if (keyboard_visible() == 1) {
        shown = ds_concat(chat_input, "|");
    }
    if (str_len(chat_input) == 0 && keyboard_visible() == 0) {
        text_in_field(tr_type_message(), lx + 10, iy, ih, text_color, 0.55);
    } else if (str_len(shown) == 0) {
        text_in_field("|", lx + 10, iy, ih, text_color, 0.55);
    } else {
        text_in_field(shown, lx + 10, iy, ih, text_color, 0.55);
    }
    button_scaled(tr_send(), lx + lw - 112, iy, 112, ih, 0.5);
    press_overlay = 0;
    roundrect(sticker_btn_x(), iy, STICKER_BTN, ih, 10, ui_purple);
    draw_sticker_face(sticker_btn_x() + STICKER_BTN / 2, iy + ih / 2);
    if (sticker_menu_open == 1) {
        roundrect(sticker_menu_x(), sticker_menu_y(), sticker_menu_w(), sticker_menu_h(), 12, card_bg);
        double st_i = 0;
        while (st_i < 3) {
            draw_sticker_tile(st_i);
            st_i = st_i + 1;
        }
    }
}

static double touch_chat(double x, double y, double action) {
    if (action != 0) {
        return 0;
    }
    if (chat_open == 0) {
        if (hit_button(x, y, chat_btn_x(), chat_btn_y(), chat_btn_w(), chat_btn_h()) == 1) {
            chat_set_open(1);
            return 1;
        }
        return 0;
    }
    if (hit_button(x, y, chat_close_x(), chat_top_y(), btn_w, btn_h) == 1) {
        chat_set_open(0);
        return 1;
    }
    double lx = 16;
    double lw = screen_w - 32;
    double iy = chat_input_y();
    double ih = 52;
    if (hit_round(x, y, sticker_btn_x(), iy, STICKER_BTN, ih, 10) == 1) {
        if (sticker_menu_open == 1) {
            sticker_menu_open = 0;
        } else {
            sticker_menu_open = 1;
        }
        return 1;
    }
    if (sticker_menu_open == 1) {
        double st_i = 0;
        while (st_i < 3) {
            if (hit_rect(x, y, sticker_tile_x(st_i), sticker_tile_y(), STICKER_TILE, STICKER_TILE) == 1) {
                chat_send_sticker(sticker_tex_of(st_i));
                return 1;
            }
            st_i = st_i + 1;
        }
        if (hit_rect(x, y, sticker_menu_x(), sticker_menu_y(), sticker_menu_w(), sticker_menu_h()) == 1) {
            return 1;
        }
        sticker_menu_open = 0;
        return 1;
    }
    if (hit_rect(x, y, lx, iy, lw - 122, ih) == 1) {
        if (keyboard_visible() == 0) {
            keyboard_show();
        }
        return 1;
    }
    if (hit_button(x, y, lx + lw - 112, iy, 112, ih) == 1) {
        chat_send();
        keyboard_hide();
        return 1;
    }
    keyboard_hide();
    return 1;
}

/* Mirror of touch_chat() used for the button click sound: only the actual
 * controls count, the text field and the message log stay silent. */
static double chat_button_hit(double x, double y) {
    double lx = 16;
    double lw = screen_w - 32;
    double iy = chat_input_y();
    double ih = 52;
    if (chat_open == 0) {
        return hit_button(x, y, chat_btn_x(), chat_btn_y(), chat_btn_w(), chat_btn_h());
    }
    if (hit_button(x, y, chat_close_x(), chat_top_y(), btn_w, btn_h) == 1 ||
        hit_round(x, y, sticker_btn_x(), iy, STICKER_BTN, ih, 10) == 1 ||
        hit_button(x, y, lx + lw - 112, iy, 112, ih) == 1) {
        return 1;
    }
    if (sticker_menu_open == 1) {
        double st_i = 0;
        while (st_i < 3) {
            if (hit_rect(x, y, sticker_tile_x(st_i), sticker_tile_y(), STICKER_TILE, STICKER_TILE) == 1) {
                return 1;
            }
            st_i = st_i + 1;
        }
        return hit_rect(x, y, sticker_menu_x(), sticker_menu_y(), sticker_menu_w(), sticker_menu_h());
    }
    return 0;
}
