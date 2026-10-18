#include "gift_icons.inc"

/* Sticker shop and collection. The list snaps to whole rows: a half-scrolled
 * card used to be drawn above the list and showed through the header, so now
 * the visible window always starts on a row boundary and nothing spills out
 * of the rounded card. */

static double acct_gift_list_top(void) { return acct_top() + 58; }
static double acct_gift_card_h(void) { return clamp((screen_h - acct_top() - 300) / 4, 48, 104); }
static double acct_gift_step(void) { return acct_gift_card_h() + 8; }
static double acct_gift_visible(void) {
    double n = floor((screen_h - acct_gift_list_top() - 228) / acct_gift_step());
    return clamp(n, 1, 5);
}
static double acct_gift_scroll_max(void) { return 9 - acct_gift_visible(); }
/* Row shown at the top of the window: whole rows only. */
static double acct_gift_row0(void) { return clamp(floor(acct_gift_scroll + 0.5), 0, acct_gift_scroll_max()); }
static double acct_gift_footer_top(void) { return acct_gift_list_top() + acct_gift_visible() * acct_gift_step() + 2; }
static double acct_gift_btn_w(void) { return (acct_w() - 10) / 2; }
static double acct_gift_y(double i) { return acct_gift_list_top() + (i - acct_gift_row0()) * acct_gift_step(); }
static double hit_acct_gift_buy(double x, double y) {
    return acct_open == ACCT_GIFTS && hit_button(x, y, acct_x(), acct_below_fields(), acct_gift_btn_w(), btn_h) == 1;
}
static double hit_acct_gift_send(double x, double y) {
    return acct_open == ACCT_GIFTS && hit_button(x, y, acct_x() + acct_gift_btn_w() + 10, acct_below_fields(), acct_gift_btn_w(), btn_h) == 1;
}
static const char *acct_gift_name(double i) {
    if (i == 0) return language == 1 ? "Какашка" : "Poop";
    if (i == 1) return language == 1 ? "Торт" : "Cake";
    if (i == 2) return language == 1 ? "Мишка" : "Teddy Bear";
    return language == 1 ? "Ракета" : "Rocket";
}
static double acct_gift_price(double i) { return net_gift_price(i); }
static double hit_acct_gift(double x, double y, double i) {
    double iy = acct_gift_y(i);
    return acct_open == ACCT_GIFTS && i >= acct_gift_row0() && i < acct_gift_row0() + acct_gift_visible() &&
           iy >= acct_gift_list_top() && iy + acct_gift_card_h() <= acct_gift_list_top() + acct_gift_visible() * acct_gift_step() - 8 &&
           hit_rect(x, y, acct_x(), iy, acct_w(), acct_gift_card_h()) == 1;
}
static double acct_gift_bar_x(void) { return acct_x() - 20; }
static double acct_gift_bar_h(void) { return acct_gift_visible() * acct_gift_step() - 8; }
static double acct_gift_thumb_h(void) { return clamp(acct_gift_bar_h() * acct_gift_visible() / 9, 40, acct_gift_bar_h()); }
static double acct_gift_thumb_y(void) {
    double travel = acct_gift_bar_h() - acct_gift_thumb_h();
    return acct_gift_list_top() + (acct_gift_scroll_max() > 0 ? travel * acct_gift_row0() / acct_gift_scroll_max() : 0);
}
static double hit_acct_gift_scrollbar(double x, double y) {
    return acct_open == ACCT_GIFTS && hit_rect(x, y, acct_gift_bar_x() - 6, acct_gift_list_top(), 26, acct_gift_bar_h()) == 1;
}
static void acct_gift_scroll_to(double y) {
    double travel = acct_gift_bar_h() - acct_gift_thumb_h();
    if (travel > 0) acct_gift_scroll = clamp((y - acct_gift_bar_grab - acct_gift_list_top()) / travel * acct_gift_scroll_max(), 0, acct_gift_scroll_max());
}
static double touch_acct_gift_scroll(double x, double y, double action, double id) {
    if (action == 2 && acct_open == ACCT_GIFTS && id == acct_gift_bar_drag_id) {
        acct_gift_velocity = 0; acct_gift_scroll_to(y); return 1;
    }
    if (action == 2 && acct_open == ACCT_GIFTS && id == acct_gift_drag_id) {
        double delta = y - acct_gift_drag_y;
        acct_gift_scroll = clamp(acct_gift_scroll - delta / acct_gift_step(), 0, acct_gift_scroll_max());
        acct_gift_velocity = clamp(-delta / acct_gift_step() / fmax(dt, 0.016), -18, 18);
        acct_gift_drag_y = y; return 1;
    }
    if ((action == 1 || action == 3) && id == acct_gift_bar_drag_id) { acct_gift_scroll = acct_gift_row0(); acct_gift_bar_drag_id = -1; return 1; }
    if ((action == 1 || action == 3) && id == acct_gift_drag_id) {
        if (action == 1 && fabs(y - acct_gift_drag_start_y) < 12) {
            double row = floor((y - acct_gift_list_top()) / acct_gift_step() + acct_gift_row0());
            if (row >= 0 && row < 4) { acct_gift_choice = row; acct_gift_is_owned = 0; }
            else if (row > 4 && row < 9) { acct_gift_choice = row - 5; acct_gift_is_owned = 1; }
        }
        acct_gift_scroll = clamp(acct_gift_row0(), 0, acct_gift_scroll_max());
        acct_gift_drag_id = -1; return 1;
    }
    if (action != 0 || acct_open != ACCT_GIFTS) return 0;
    if (hit_acct_gift_scrollbar(x, y)) {
        acct_gift_velocity = 0; acct_gift_bar_drag_id = id;
        if (y >= acct_gift_thumb_y() && y <= acct_gift_thumb_y() + acct_gift_thumb_h()) acct_gift_bar_grab = y - acct_gift_thumb_y();
        else { acct_gift_bar_grab = acct_gift_thumb_h() / 2; acct_gift_scroll_to(y); }
        return 1;
    }
    if (hit_rect(x, y, acct_x(), acct_gift_list_top(), acct_w(), acct_gift_visible() * acct_gift_step() - 8)) {
        acct_gift_velocity = 0; acct_gift_drag_id = id; acct_gift_drag_start_y = y; acct_gift_drag_y = y; return 1;
    }
    return 0;
}
static void acct_gifts_page(void) {
    acct_gift_choice = 0; acct_gift_busy = 0; acct_gift_is_owned = 0; acct_gift_scroll = 0; acct_gift_velocity = 0;
    net_gift_refresh(); acct_go(ACCT_GIFTS);
}
static void acct_gift_buy(void) {
    keyboard_hide();
    if (cups < acct_gift_price(acct_gift_choice)) { acct_msg = language == 1 ? "Не хватает кубков" : "Not enough cups"; return; }
    acct_gift_busy = 1; acct_gift_busy_mode = 2;
    acct_msg = language == 1 ? "Покупаем стикер..." : "Buying sticker...";
    net_gift_action(acct_gift_choice, "", 2);
}
static void acct_gift_send(void) {
    acct_sync(); keyboard_hide();
    if (acct_gift_is_owned != 1 || net_gift_count(acct_gift_choice) < 1) {
        acct_msg = language == 1 ? "Выберите стикер из раздела «Мои подарки»" : "Select a sticker from My gifts below"; return;
    }
    if (!acct_buf[0][0]) { acct_msg = language == 1 ? "Введите ник получателя" : "Enter recipient nickname"; return; }
    acct_gift_busy = 1; acct_gift_busy_mode = 0;
    acct_msg = language == 1 ? "Отправляем стикер..." : "Sending sticker...";
    net_gift_action(acct_gift_choice, acct_buf[0], 0);
}

/* Right-aligned text, vertically centred in a row of height h. */
static void acct_gift_text_right(const char *s, double right, double y, double h, uint32_t c, double sc) {
    double tw = text_ink_width(s) * sc;
    double th = text_ink_height(s) * sc;
    double tt = text_ink_top(s) * sc;
    text_scaled(s, right - tw, y + (h - th) / 2 - tt + 1.5, c, sc);
}

/* One list row: the drawn sticker, its name, then either the price (shop) or
 * a little heap of owned stickers (collection). No counters anywhere. */
static void acct_gift_row(double row, double y) {
    double x = acct_x() + 8, w = acct_w() - 16, h = acct_gift_card_h();
    int owned = row > 4;
    int i = owned ? (int)row - 5 : (int)row;
    double count = owned ? net_gift_count(i) : 0;
    int dim = owned && count < 1;
    double p = button_press_k(x, y, w, h);
    double sq = 1 - PRESS_SQUEEZE * p;
    double bx = x + w * (1 - sq) / 2, by = y + h * (1 - sq) / 2;
    double bw = w * sq, bh = h * sq;
    uint32_t ink = (uint32_t)shade(text_color, dim ? 0.55 : 1);
    uint32_t face = (uint32_t)shade(ui_purple, dim ? 0.55 : 1 - PRESS_SHADE * p);
    const char *name = acct_gift_name(i);
    const char *price = ds_concat(ds_num_to_string(acct_gift_price(i)), language == 1 ? " куб." : " cups");
    double isz = clamp(bh * 0.62, 22, 52);
    double psz = clamp(bh * 0.48, 18, 40);
    double price_sc = 0.55;
    double price_w = owned ? 0 : text_ink_width(price) * price_sc;
    double pile_w = owned ? gift_pile_w(psz, count) : 0;
    double name_x = bx + 10 + isz + 10;
    double name_w = bx + bw - 12 - name_x - (owned ? pile_w + 10 : price_w + 12);
    if (name_w < 24) name_w = 24;
    button_face(bx, by, bw, bh, face);
    gift_icon(i, bx + 10, by + (bh - isz) / 2, isz);
    text_in_field(name, name_x, by, bh, ink, fit_text_scale(name, name_w, 0.64));
    if (owned) {
        gift_pile(i, bx + bw - 12, by + (bh - psz) / 2, psz, count);
    } else {
        acct_gift_text_right(price, bx + bw - 12, by, bh, ink, price_sc);
    }
    if (acct_gift_choice == i && acct_gift_is_owned == (owned ? 1 : 0)) {
        roundrect_outline(x, y, w, h, btn_radius, 3, 0xFFFFD34D);
    }
}

static void draw_acct_gifts(void) {
    double top = acct_gift_list_top();
    double view_h = acct_gift_visible() * acct_gift_step() - 8;
    double row = acct_gift_row0();
    double last = row + acct_gift_visible();
    roundrect(acct_x(), top, acct_w(), view_h, 14, card_bg);
    while (row < 9 && row < last) {
        double y = acct_gift_y(row);
        if (row == 4) {
            button_scaled(language == 1 ? "МОИ ПОДАРКИ" : "MY GIFTS", acct_x() + 8, y, acct_w() - 16,
                          acct_gift_card_h(), 0.62);
        } else {
            acct_gift_row(row, y);
        }
        row = row + 1;
    }
    ctext_scaled(language == 1 ? "Подарки" : "Gifts", acct_top(), text_color,
                 fit_text_scale(language == 1 ? "Подарки" : "Gifts", fit_w(screen_w), 0.8));
    ctext_scaled(language == 1 ? "СТИКЕРНАЯ" : "STICKER SHOP", acct_top() + 27, text_color, 0.58);
    text_in_box(language == 1 ? "Листайте вниз до своих подарков" : "Scroll down to your collection",
                acct_x(), acct_top() + 38, acct_w(), 18, text_color, 0.40);
    roundrect(acct_gift_bar_x(), top, 14, view_h, 7, 0x55303030);
    roundrect(acct_gift_bar_x(), acct_gift_thumb_y(), 14, acct_gift_thumb_h(), 7, ui_purple);
    draw_acct_field(language == 1 ? "Ник получателя" : "Recipient nickname", 0);
    double bw = acct_gift_btn_w();
    const char *buy = acct_gift_busy == 1 && acct_gift_busy_mode == 2 ? (language == 1 ? "Покупаем..." : "Buying...") :
                      ds_concat(language == 1 ? "Оставить себе - " : "Keep - ", ds_concat(ds_num_to_string(acct_gift_price(acct_gift_choice)), language == 1 ? " куб." : " cups"));
    const char *send = acct_gift_busy == 1 && acct_gift_busy_mode == 0 ? (language == 1 ? "Отправляем..." : "Sending...") : (language == 1 ? "Подарить" : "Send gift");
    button_scaled(buy, acct_x(), acct_below_fields(), bw, btn_h, fit_text_scale(buy, bw - 12, 0.48));
    button_scaled(send, acct_x() + bw + 10, acct_below_fields(), bw, btn_h, fit_text_scale(send, bw - 12, 0.54));
}

/* Inertia while the finger is off the list, then settle on a whole row so the
 * window never rests between two cards. */
static void acct_gift_tick(void) {
    if (acct_open != ACCT_GIFTS) {
        return;
    }
    if (acct_gift_drag_id == -1 && acct_gift_bar_drag_id == -1 && fabs(acct_gift_velocity) > 0.01) {
        acct_gift_scroll = clamp(acct_gift_scroll + acct_gift_velocity * dt, 0, acct_gift_scroll_max());
        acct_gift_velocity = acct_gift_velocity * exp(-5.2 * dt);
        if (acct_gift_scroll <= 0 || acct_gift_scroll >= acct_gift_scroll_max()) acct_gift_velocity = 0;
    }
    if (acct_gift_drag_id == -1 && acct_gift_bar_drag_id == -1 && fabs(acct_gift_velocity) < 0.05) {
        acct_gift_scroll = acct_gift_row0();
    }
}

static void acct_poll_gifts(void) {
    if (acct_gift_busy == 1) {
        double gift_status = net_gift_action_status();
        if (gift_status >= 2) {
            acct_gift_busy = 0;
            if (gift_status == 2) {
                if (acct_gift_busy_mode == 2) {
                    acct_msg = language == 1 ? "Стикер добавлен в коллекцию!" : "Sticker added to your collection!";
                    cups = cups - acct_gift_price(acct_gift_choice);
                    if (cups < 0) cups = 0;
                    acct_gift_is_owned = 1;
                    acct_gift_scroll = clamp(5 + acct_gift_choice - floor(acct_gift_visible() / 2), 0, 9 - acct_gift_visible());
                    save_progress();
                } else {
                    acct_msg = language == 1 ? "Стикер отправлен!" : "Sticker sent!";
                    memset(acct_buf[0], 0, sizeof(acct_buf[0]));
                    acct_field = 0;
                }
                net_gift_refresh();
            } else if (gift_status == 4) {
                acct_msg = language == 1 ? "Игрок с таким ником не найден" : "No player with that nickname";
            } else if (gift_status == 5) {
                acct_msg = language == 1 ? "Не хватает кубков" : "Not enough cups";
            } else if (gift_status == 6) {
                acct_msg = language == 1 ? "Такого стикера нет в инвентаре" : "Sticker is not in your inventory";
            } else if (gift_status == 7) {
                acct_msg = language == 1 ? "Проверьте ник: нельзя дарить самому себе" : "Check the nickname: you cannot gift yourself";
            } else if (gift_status == 8) {
                acct_msg = language == 1 ? "Коллекция заполнена (999)" : "Collection is full (999)";
            } else {
                acct_msg = language == 1 ? "Не удалось отправить. Проверьте ник и подключение." : "Gift failed. Check nickname and connection.";
            }
        }
    }
}
