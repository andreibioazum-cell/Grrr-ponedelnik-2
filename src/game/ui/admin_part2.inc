/* Admin panel input and chrome: hit-testing, the touch handler, the "CCP" orb,
 * the panel frame and the cheats grid. Every button here is drawn by
 * admin_btn() from admin.inc, so they all share one flat black-and-white look.
 * Вкладок в панели больше нет - только сетка читов. */

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
            while (t1[(int)i] != 0 && i < (double)(sizeof(admin_passwd_target_buf) - 1)) { admin_passwd_target_buf[(int)i] = t1[(int)i]; i = i + 1; }
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

static void admin_panel_close(void) {
    admin_panel_open = 0;
}

static void admin_panel_press(double x, double y) {
    double px = admin_panel_x();
    double py = admin_panel_y();
    double pw = admin_panel_w();
    double hh = admin_header_h();
    double cxx = px + pw - hh * 0.55;
    double dx = x - cxx;
    double dy = y - (py + hh / 2);
    if (sqrt(dx * dx + dy * dy) <= hh * 0.38 + 8) {
        admin_panel_close();
        play_button_click();
        return;
    }
    if (hit_rect(x, y, px, py, pw, admin_panel_h()) == 0) {
        admin_panel_close();
        return;
    }
    double bw = (pw - 2 * admin_pad() - admin_gap()) / 2;
    double rh = admin_cheat_row_h();
    double row = floor((y - admin_grid_top()) / (rh + admin_gap()));
    if (row < 0 || row > 2 || x < px + admin_pad() || x > px + pw - admin_pad()) {
        return;
    }
    double col = 0;
    if (x >= px + admin_pad() + bw + admin_gap()) {
        col = 1;
    }
    admin_press(row * 2 + col);
}

static double admin_ball_hit(double x, double y) {
    double dx = x - admin_ball_x;
    double dy = y - admin_ball_y;
    return sqrt(dx * dx + dy * dy) <= admin_ball_r() + 14;
}

static double admin_touch(double x, double y, double action, double id) {
    (void)id;
    if (is_admin() == 0) {
        return 0;
    }
    /* The panel is modal: every touch while it is open goes to it, on top of
     * every screen, the transition curtain and the epilepsy warning. */
    if (admin_panel_open == 1) {
        if (action == 0) {
            admin_panel_press(x, y);
        }
        return 1;
    }
    if (admin_ball_grab == 1) {
        if (action == 2 || action == 1) {
            double br = admin_ball_r();
            admin_ball_x = clamp(x - admin_ball_grab_dx, br + 4, screen_w - br - 4);
            admin_ball_y = clamp(y - admin_ball_grab_dy, br + 6, screen_h - br - 6);
        }
        if (action == 1 || action == 3) {
            admin_ball_grab = 0;
            double dx = x - admin_ball_px;
            double dy = y - admin_ball_py;
            if (action == 1 && sqrt(dx * dx + dy * dy) <= 13) {
                admin_panel_open = 1;
                play_button_click();
            }
        }
        return 1;
    }
    if (action == 0 && admin_ball_hit(x, y) == 1) {
        admin_ball_grab = 1;
        admin_ball_px = x;
        admin_ball_py = y;
        admin_ball_grab_dx = x - admin_ball_x;
        admin_ball_grab_dy = y - admin_ball_y;
        return 1;
    }
    return 0;
}

/* Black disc with a white rim and the short panel name - flat, no gradients. */
static void admin_draw_ball(void) {
    const char *lab = admin_orb_label();
    double r = admin_ball_r();
    double cx = admin_ball_x;
    double cy = admin_ball_y;
    double iw = text_ink_width(lab);
    double ih = text_ink_height(lab);
    double sc = 0.5;
    /* Flat black halo instead of a soft shadow: the orb is a button too, and
     * every button in this panel is fully opaque. */
    circle(cx, cy, r + 3, admin_off());
    circle(cx, cy, r, admin_body());
    ring(cx, cy, r, clamp(r * 0.11, 2, 3), admin_frame());
    if (iw > 0 && ih > 0) {
        double sw = (r * 1.5) / iw;
        double sh = (r * 0.95) / ih;
        sc = sw < sh ? sw : sh;
    }
    double tw = text_ink_width(lab) * sc;
    double th = text_ink_height(lab) * sc;
    double tt = text_ink_top(lab) * sc;
    text_scaled(lab, cx - tw / 2, cy - th / 2 - tt, admin_frame(), sc);
}

static void admin_draw_cheats(void) {
    double px = admin_panel_x() + admin_pad();
    double pw = admin_panel_w() - 2 * admin_pad();
    double bw = (pw - admin_gap()) / 2;
    double rh = admin_cheat_row_h();
    double idx = 0;
    while (idx < 6) {
        double row = floor(idx / 2);
        double col = idx - row * 2;
        double bx = px + col * (bw + admin_gap());
        double by = admin_grid_top() + row * (rh + admin_gap());
        const char *label = admin_cheat_label(idx);
        admin_btn(label, bx, by, bw, rh, admin_cheat_value(idx), fit_text_scale(label, bw - 12, rh / 58.0));
        idx = idx + 1;
    }
}

static void admin_draw_panel(void) {
    double px = admin_panel_x();
    double py = admin_panel_y();
    double pw = admin_panel_w();
    double ph = admin_panel_h();
    double hh = admin_header_h();
    const char *title = tr_admin_title();
    double title_w = pw - hh * 1.9 - 16;
    /* The full-screen scrim is the only translucent thing in the panel; every
     * shape of the panel itself is flat and fully opaque. */
    rect(0, 0, screen_w, screen_h, admin_a(0x000000, 0.55));
    roundrect(px, py, pw, ph, 14, admin_body());
    roundrect_outline(px, py, pw, ph, 14, 3, admin_frame());
    /* Header: black band, the full panel name, the close button. */
    roundrect(px, py, pw, hh, 14, admin_header());
    rect(px, py + hh / 2, pw, hh / 2, admin_header());
    line(px + 12, py + hh - 1, px + pw - 12, py + hh - 1, 2, admin_frame());
    /* Left-aligned in the header, as asked, instead of centred in the band. */
    text_in_field(title, px + 16, py, hh, admin_frame(), fit_text_scale(title, title_w, hh / 58.0));
    double cxx = px + pw - hh * 0.55;
    double cr = hh * 0.3;
    circle(cxx, py + hh / 2, cr, admin_off());
    ring(cxx, py + hh / 2, cr, clamp(cr * 0.14, 2, 3), admin_frame());
    text_in_box("X", cxx - cr, py + hh / 2 - cr, cr * 2, cr * 2, admin_frame(), cr / 30.0);
    admin_draw_cheats();
}

static void admin_draw(void) {
    if (is_admin() == 0) {
        return;
    }
    if (admin_ball_x == 0 && admin_ball_y == 0) {
        admin_ball_x = screen_w - (admin_ball_r() + 14);
        admin_ball_y = screen_h * 0.40;
    }
    if (admin_panel_open == 1) {
        admin_draw_panel();
    }
    admin_draw_ball();
}

/* Continuous cheats: ticked from logic_update so anti-stun, the frozen enemy
 * and the zeroed cooldowns survive every re-application the fight makes. */
static void admin_tick(void) {
    admin_ball_t = admin_ball_t + dt;
    if (is_admin() == 0 || admin_in_battle() == 0 || finished != 0) {
        return;
    }
    if (cheat_nocd == 1) {
        /* Everything ability-related refills instantly, solo and online. */
        punch_cd = 0;
        super_cd = 0;
        dash_cd = 0;
        astra_cd = 0;
    }
    if (cheat_cc == 1) {
        player_stun = 0;
        player_freeze = 0;
        player_poison = 0;
    }
    if (cheat_estun == 1 && game_state == ST_SOLO) {
        if (enemy->stun < 0.25) {
            enemy->stun = 0.25;
        }
        /* Этот стан приходит из панели, поэтому кружка за ним нет - даже если
         * чит выключили, а стан от него ещё не истёк. */
        cheat_estun_mark = 1;
    }
}
