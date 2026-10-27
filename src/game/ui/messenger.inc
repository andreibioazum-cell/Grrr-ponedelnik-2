#include <stdio.h>

#define MESSENGER_CONTACT_ROWS 6
#define MESSENGER_INPUT_MAX 320

static double messenger_back_x(void) { return screen_margin; }
static double messenger_back_w(void) { return fit_w(138); }
static double messenger_form_w(void) { return fit_w(680); }
static double messenger_form_x(void) { return center_x(messenger_form_w()); }
static double messenger_input_y(void) { return content_top() + 52; }
static double messenger_open_y(void) { return messenger_input_y() + login_fh + 12; }
static double messenger_contacts_y(void) { return messenger_open_y() + btn_h + 24; }
static double messenger_contact_row_y(double i) { return messenger_contacts_y() + 34 + i * 62; }
static double messenger_chat_top(void) { return back_y + btn_h + 48; }
static double messenger_chat_input_y(void) { return screen_h - 78; }

static void messenger_set_notice(const char *text) {
    size_t n;
    if (!text) text = "";
    n = strlen(text);
    if (n >= sizeof(messenger_notice_buf)) n = sizeof(messenger_notice_buf) - 1;
    memcpy(messenger_notice_buf, text, n);
    messenger_notice_buf[n] = 0;
    messenger_notice = messenger_notice_buf;
    messenger_notice_t = 3.5;
}

static void messenger_sync_input(void) {
    if (!keyboard_visible()) return;
    if (messenger_mode == 0 && messenger_field == 1) {
        keyboard_get_text_to(messenger_peer_buf, sizeof(messenger_peer_buf));
        messenger_peer_input = messenger_peer_buf;
    } else if (messenger_mode == 1 && messenger_field == 2) {
        keyboard_get_text_to(messenger_text_buf, sizeof(messenger_text_buf));
        messenger_text_input = messenger_text_buf;
    }
}

static void messenger_focus(double field) {
    messenger_sync_input();
    messenger_field = field;
    keyboard_clear();
    if (messenger_mode == 0 && field == 1 && messenger_peer_input[0]) keyboard_type(messenger_peer_input);
    if (messenger_mode == 1 && field == 2 && messenger_text_input[0]) keyboard_type(messenger_text_input);
    keyboard_show();
}

static void messenger_open(void) {
    messenger_mode = 0;
    messenger_field = 1;
    messenger_poll_t = 0;
    messenger_send_pending = 0;
    messenger_peer_buf[0] = 0;
    messenger_text_buf[0] = 0;
    messenger_last_send_buf[0] = 0;
    messenger_peer_input = "";
    messenger_text_input = "";
    messenger_notice = "";
    messenger_notice_buf[0] = 0;
    messenger_notice_t = 0;
    keyboard_clear();
    keyboard_hide();
    if (net_login_status() == 2) net_dm_contacts_fetch(FIREBASE);
    else messenger_set_notice(language == 1 ? "Сначала войди в аккаунт" : "Sign in first");
}

static void messenger_leave(void) {
    keyboard_hide();
    keyboard_clear();
    messenger_poll_t = 0;
}

static void messenger_open_peer(const char *peer) {
    messenger_sync_input();
    keyboard_hide();
    keyboard_clear();
    if (!peer || !peer[0]) {
        messenger_set_notice(language == 1 ? "Введи ник собеседника" : "Enter a username");
        return;
    }
    if (str_eq(peer, net_login_nick()) == 1) {
        messenger_set_notice(language == 1 ? "Нельзя открыть чат с собой" : "You cannot message yourself");
        return;
    }
    messenger_mode = 1;
    messenger_field = 2;
    messenger_text_buf[0] = 0;
    messenger_text_input = "";
    messenger_poll_t = 0;
    messenger_set_notice("");
    net_dm_open(FIREBASE, peer);
}

static void messenger_back_to_contacts(void) {
    messenger_sync_input();
    keyboard_hide();
    keyboard_clear();
    messenger_mode = 0;
    messenger_field = 1;
    messenger_text_buf[0] = 0;
    messenger_text_input = "";
    messenger_poll_t = 0;
    messenger_set_notice("");
    if (net_dm_contacts_status() == 0 || net_dm_contacts_status() == 4) net_dm_contacts_fetch(FIREBASE);
}

static void messenger_send(void) {
    size_t n;
    messenger_sync_input();
    n = strlen(messenger_text_input ? messenger_text_input : "");
    if (n == 0) return;
    if (n > MESSENGER_INPUT_MAX) {
        messenger_set_notice(language == 1 ? "Слишком длинное сообщение (лимит 320 байт)" : "Message is too long (320-byte limit)");
        return;
    }
    if (messenger_send_pending == 1) {
        messenger_set_notice(language == 1 ? "Подожди отправку" : "Message is still sending");
        return;
    }
    snprintf(messenger_last_send_buf, sizeof(messenger_last_send_buf), "%s", messenger_text_input);
    net_dm_send(FIREBASE, messenger_last_send_buf);
    messenger_send_pending = 1;
    messenger_text_buf[0] = 0;
    messenger_text_input = "";
    keyboard_clear();
    keyboard_hide();
}

static void messenger_update(void) {
    if (game_state != ST_MESSENGER) return;
    if (keyboard_visible()) messenger_sync_input();
    if (keyboard_enter_pressed()) {
        if (messenger_mode == 0) messenger_open_peer(messenger_peer_input);
        else messenger_send();
    }
    if (messenger_notice_t > 0) messenger_notice_t = clamp(messenger_notice_t - dt, 0, 3.5);
    messenger_poll_t -= dt;
    if (messenger_mode == 0 && messenger_poll_t <= 0 && net_dm_contacts_status() != 1) {
        net_dm_contacts_fetch(FIREBASE);
        messenger_poll_t = 5.0;
    } else if (messenger_mode == 1 && messenger_poll_t <= 0 && net_dm_status() != 1) {
        net_dm_refresh(FIREBASE);
        messenger_poll_t = 2.5;
    }
    if (messenger_send_pending == 1 && net_dm_send_status() != 1) {
        messenger_send_pending = 0;
        if (net_dm_send_status() == 2) {
            messenger_set_notice(language == 1 ? "Сообщение доставлено" : "Message sent");
            messenger_text_buf[0] = 0;
            messenger_text_input = "";
        } else {
            snprintf(messenger_text_buf, sizeof(messenger_text_buf), "%s", messenger_last_send_buf);
            messenger_text_input = messenger_text_buf;
            messenger_set_notice(language == 1 ? "Не удалось отправить — проверь соединение" : "Could not send — check connection");
        }
    }
}

static size_t messenger_utf8_bytes(const unsigned char *p) {
    if ((p[0] & 0x80) == 0) return 1;
    if ((p[0] & 0xE0) == 0xC0 && p[1]) return 2;
    if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) return 3;
    if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) return 4;
    return 1;
}

static int messenger_wrap(const char *src, double max_width, double scale, char lines[5][160]) {
    const unsigned char *p = (const unsigned char *)(src ? src : "");
    int line = 0;
    size_t used = 0;
    memset(lines, 0, 5 * 160);
    while (*p && line < 5) {
        size_t bytes;
        if (*p == '\n') {
            p++;
            line++;
            used = 0;
            continue;
        }
        bytes = messenger_utf8_bytes(p);
        if (used + bytes >= 158) {
            line++;
            used = 0;
            continue;
        }
        memcpy(lines[line] + used, p, bytes);
        lines[line][used + bytes] = 0;
        if (used > 0 && text_ink_width(lines[line]) * scale > max_width) {
            lines[line][used] = 0;
            line++;
            used = 0;
            continue;
        }
        used += bytes;
        p += bytes;
    }
    if (*p && line == 4) snprintf(lines[4], 160, "…");
    return line + 1;
}

static double messenger_draw_bubble(double idx, double bottom) {
    const char *from = net_dm_from(idx);
    const char *body = net_dm_text(idx);
    int mine = str_eq(from, net_login_nick()) == 1;
    double margin = screen_margin;
    double width = fit_w(620);
    double scale = 0.47;
    double inner = width - 28;
    char lines[5][160];
    int line_count = messenger_wrap(body, inner, scale, lines);
    double height = 31 + line_count * 18;
    double x = mine ? screen_w - margin - width : margin;
    double y = bottom - height;
    roundrect(x, y, width, height, 14, mine ? 0xFF4A2E66 : card_bg);
    text_scaled(mine ? (language == 1 ? "Вы" : "You") : from, x + 14, y + 5, mine ? 0xFFD4B6FF : 0xFF82D9FF, 0.39);
    for (int i = 0; i < line_count && i < 5; i++)
        text_scaled(lines[i], x + 14, y + 23 + i * 18, text_color, scale);
    return y - 8;
}

static void messenger_draw_contact_view(void) {
    double x = messenger_form_x(), w = messenger_form_w();
    double iy = messenger_input_y();
    double bw = (w - 12) / 2;
    text_scaled(language == 1 ? "Ник собеседника" : "Recipient username", x + 4, iy - 24, text_color, 0.48);
    roundrect(x, iy, w, login_fh, field_radius, card_bg);
    messenger_sync_input();
    const char *shown = messenger_peer_input;
    if (keyboard_visible() && messenger_field == 1) shown = ds_concat(shown, "|");
    if (!shown[0] && !keyboard_visible()) shown = language == 1 ? "Например: Player2" : "For example: Player2";
    text_in_field(shown, x + field_text_pad, iy, login_fh, text_color, 0.55);
    button_scaled(language == 1 ? "Открыть чат" : "Open chat", x, messenger_open_y(), bw, btn_h, 0.55);
    button_scaled(language == 1 ? "Обновить" : "Refresh", x + bw + 12, messenger_open_y(), bw, btn_h, 0.55);
    text_scaled(language == 1 ? "ЧАТЫ" : "CHATS", x + 4, messenger_contacts_y(), 0xFFB8A9D5, 0.42);
    double status = net_dm_contacts_status();
    double count = net_dm_contacts_count();
    if (status == 1) {
        text_scaled(language == 1 ? "Загружаем список…" : "Loading conversations…", x + 4, messenger_contacts_y() + 30, text_color, 0.48);
    } else if (status == 4) {
        text_scaled(language == 1 ? "Нет соединения с сервером. Нажми «Обновить»." : "Server unavailable. Press Refresh.", x + 4, messenger_contacts_y() + 30, 0xFFFF9D9D, 0.43);
    } else if (count == 0) {
        text_scaled(language == 1 ? "Чатов пока нет. Введи ник выше, чтобы начать." : "No chats yet. Enter a username above to start.", x + 4, messenger_contacts_y() + 30, text_color, 0.43);
    } else {
        double n = count < MESSENGER_CONTACT_ROWS ? count : MESSENGER_CONTACT_ROWS;
        for (double i = 0; i < n; i++) {
            const char *nick = net_dm_contact(i);
            roundrect(x, messenger_contact_row_y(i), w, 54, 12, card_bg);
            circle(x + 26, messenger_contact_row_y(i) + 27, 13, 0xFF5F10A0);
            text_scaled("@", x + 20, messenger_contact_row_y(i) + 15, text_color, 0.45);
            text_scaled(nick, x + 52, messenger_contact_row_y(i) + 15, text_color, 0.55);
            text_scaled(language == 1 ? "Открыть переписку" : "Open conversation", x + w - 178, messenger_contact_row_y(i) + 17, 0xFF9F91B7, 0.36);
        }
    }
}

static void messenger_draw_chat_view(void) {
    double backw = fit_w(142);
    button_scaled(language == 1 ? "< Все чаты" : "< Chats", screen_margin, back_y, backw, btn_h, 0.48);
    const char *peer = net_dm_peer();
    const char *title = ds_concat("@", peer);
    text_scaled(title, screen_margin + backw + 16, back_y + 17, text_color, fit_text_scale(title, screen_w - screen_margin - backw - 24, 0.54));
    double status = net_dm_status();
    if (status == 1 && net_dm_count() == 0) {
        text_scaled(language == 1 ? "Загружаем переписку…" : "Loading conversation…", screen_margin, messenger_chat_top(), text_color, 0.48);
    } else if (status == 3) {
        text_scaled(language == 1 ? "Неверный ник. Используй латиницу, цифры и _" : "Invalid username. Use letters, digits and _", screen_margin, messenger_chat_top(), 0xFFFF9D9D, 0.42);
    } else if (status == 5) {
        text_scaled(language == 1 ? "Такой аккаунт не найден" : "Account not found", screen_margin, messenger_chat_top(), 0xFFFF9D9D, 0.48);
    } else if (status == 4 && net_dm_count() == 0) {
        text_scaled(language == 1 ? "Не удалось подключиться к серверу" : "Could not reach the server", screen_margin, messenger_chat_top(), 0xFFFF9D9D, 0.48);
    } else if (net_dm_count() == 0) {
        text_scaled(language == 1 ? "Напиши первое сообщение" : "Send the first message", screen_margin, messenger_chat_top(), 0xFFAAA0BB, 0.48);
    }
    double bottom = messenger_chat_input_y() - 28;
    double top = messenger_chat_top() + 24;
    for (int i = (int)net_dm_count() - 1; i >= 0 && bottom > top; i--) {
        bottom = messenger_draw_bubble((double)i, bottom);
    }
    double x = screen_margin;
    double y = messenger_chat_input_y();
    double w = screen_w - 2 * screen_margin;
    double inputw = w - 122;
    roundrect(x, y, inputw, 52, 12, card_bg);
    messenger_sync_input();
    const char *shown = messenger_text_input;
    if (keyboard_visible() && messenger_field == 2) shown = ds_concat(shown, "|");
    if (!shown[0] && !keyboard_visible()) shown = language == 1 ? "Сообщение…" : "Message…";
    text_in_field(shown, x + 12, y, 52, text_color, 0.50);
    button_scaled(language == 1 ? "Отправить" : "Send", x + inputw + 10, y, w - inputw - 10, 52, 0.42);
}

static void draw_messenger(void) {
    screen_fill();
    if (messenger_mode == 0) {
        button_scaled(tr_back(), messenger_back_x(), back_y, messenger_back_w(), btn_h, 0.55);
        text_scaled(language == 1 ? "ЛИЧНЫЕ СООБЩЕНИЯ" : "DIRECT MESSAGES", center_x(text_ink_width(language == 1 ? "ЛИЧНЫЕ СООБЩЕНИЯ" : "DIRECT MESSAGES") * 0.55), title_y(), text_color, 0.55);
        messenger_draw_contact_view();
        const char *note = language == 1 ? "HTTPS · сообщения в облаке · сквозного шифрования нет" : "HTTPS · cloud-stored · not end-to-end encrypted";
        text_scaled(note, screen_margin, screen_h - 34, 0xFFAAA0BB, fit_text_scale(note, screen_w - 2 * screen_margin, 0.38));
    } else {
        messenger_draw_chat_view();
        const char *note = language == 1 ? "Доступ участников · сквозного шифрования нет" : "Participant-scoped Firebase access · not end-to-end encrypted";
        text_scaled(note, screen_margin, 14, 0xFFAAA0BB, fit_text_scale(note, screen_w - 2 * screen_margin, 0.36));
    }
    if (messenger_notice_t > 0 && messenger_notice[0]) {
        text_scaled(messenger_notice, screen_margin, screen_h - (messenger_mode == 1 ? 102 : 64), 0xFFFFC76B,
                    fit_text_scale(messenger_notice, screen_w - 2 * screen_margin, 0.42));
    }
}

static double messenger_back_button_hit(double x, double y) {
    if (messenger_mode == 0) return hit_button(x, y, messenger_back_x(), back_y, messenger_back_w(), btn_h);
    return hit_button(x, y, screen_margin, back_y, fit_w(142), btn_h);
}

static double messenger_any_hit(double x, double y) {
    if (messenger_back_button_hit(x, y)) return 1;
    if (messenger_mode == 0) {
        double fx = messenger_form_x(), w = messenger_form_w(), bw = (w - 12) / 2;
        if (hit_rect(x, y, fx, messenger_input_y(), w, login_fh)) return 1;
        if (hit_button(x, y, fx, messenger_open_y(), bw, btn_h) || hit_button(x, y, fx + bw + 12, messenger_open_y(), bw, btn_h)) return 1;
        for (int i = 0; i < MESSENGER_CONTACT_ROWS && i < (int)net_dm_contacts_count(); i++)
            if (hit_button(x, y, fx, messenger_contact_row_y(i), w, 54)) return 1;
        return 0;
    }
    double iy = messenger_chat_input_y(), w = screen_w - 2 * screen_margin, inputw = w - 122;
    return hit_rect(x, y, screen_margin, iy, inputw, 52) || hit_button(x, y, screen_margin + inputw + 10, iy, w - inputw - 10, 52);
}

static void touch_messenger(double x, double y, double action) {
    if (action != 0) return;
    if (messenger_back_button_hit(x, y)) {
        if (messenger_mode == 1) messenger_back_to_contacts();
        else { keyboard_hide(); keyboard_clear(); start_transition(ST_LOBBY); }
        return;
    }
    if (messenger_mode == 0) {
        double fx = messenger_form_x(), w = messenger_form_w(), bw = (w - 12) / 2;
        if (hit_rect(x, y, fx, messenger_input_y(), w, login_fh)) {
            if (!keyboard_visible()) messenger_focus(1);
            return;
        }
        if (hit_button(x, y, fx, messenger_open_y(), bw, btn_h)) {
            messenger_sync_input(); messenger_open_peer(messenger_peer_input); return;
        }
        if (hit_button(x, y, fx + bw + 12, messenger_open_y(), bw, btn_h)) {
            net_dm_contacts_fetch(FIREBASE); messenger_set_notice(language == 1 ? "Обновляем чаты…" : "Refreshing chats…"); return;
        }
        for (int i = 0; i < MESSENGER_CONTACT_ROWS && i < (int)net_dm_contacts_count(); i++) {
            if (hit_button(x, y, fx, messenger_contact_row_y(i), w, 54)) {
                messenger_open_peer(net_dm_contact(i)); return;
            }
        }
        keyboard_hide();
    } else {
        double iy = messenger_chat_input_y(), w = screen_w - 2 * screen_margin, inputw = w - 122;
        if (hit_rect(x, y, screen_margin, iy, inputw, 52)) {
            if (!keyboard_visible()) messenger_focus(2);
            return;
        }
        if (hit_button(x, y, screen_margin + inputw + 10, iy, w - inputw - 10, 52)) {
            messenger_send(); return;
        }
        keyboard_hide();
    }
}

static int messenger_handle_back(void) {
    if (keyboard_visible()) {
        messenger_sync_input();
        keyboard_hide();
        return 1;
    }
    if (messenger_mode == 1) {
        messenger_back_to_contacts();
        return 1;
    }
    keyboard_clear();
    start_transition(ST_LOBBY);
    return 1;
}
