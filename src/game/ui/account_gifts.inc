#include "gift_icons.inc"

/* Sticker shop and collection on one page: the shop is the left column, the
 * player's own stickers are the right one. Nothing scrolls - four stickers
 * per side always fit - and there are no permanent buttons: tapping a card
 * shows only the actions that card allows. A shop sticker offers "Buy", an
 * owned one offers "Send" and "Sell" (half of the shop price comes back). */

static double acct_gift_cols_top(void) { return acct_top() + 58; }
static double acct_gift_head_h(void) { return 28; }
/* The page keeps room under the columns for the recipient field, the action
 * row and the message, so the whole thing fits without scrolling. */
static double acct_gift_card_h(void) {
    double top = acct_gift_cols_top() + acct_gift_head_h();
    double bottom = screen_h - screen_margin - 250;
    return clamp((bottom - top - 30) / 4, 40, 156);
}
static double acct_gift_step(void) { return acct_gift_card_h() + 10; }
static double acct_gift_footer_top(void) { return acct_gift_cols_top() + acct_gift_head_h() + 4 * acct_gift_step() + 8; }
static double acct_gift_col_w(void) { return (acct_w() - 12) / 2; }
static double acct_gift_btn_w(void) { return (acct_w() - 12) / 2; }
/* owned == 0 is the shop column on the left, 1 the collection on the right. */
static double acct_gift_x(double owned) { return owned == 1 ? acct_x() + acct_gift_col_w() + 12 : acct_x(); }
static double acct_gift_y(double i) { return acct_gift_cols_top() + acct_gift_head_h() + i * acct_gift_step(); }
static double acct_gift_price(double i) { return net_gift_price(i); }
static double acct_gift_refund(double i) { return net_gift_refund(i); }
static const char *acct_gift_name(double i) {
    if (i == 0) return language == 1 ? "Какашка" : "Poop";
    if (i == 1) return language == 1 ? "Торт" : "Cake";
    if (i == 2) return language == 1 ? "Мишка" : "Teddy Bear";
    return language == 1 ? "Ракета" : "Rocket";
}
static double acct_gift_picked(void) { return acct_gift_choice >= 0 ? 1 : 0; }
static double acct_gift_picked_owned(void) {
    return acct_gift_picked() == 1 && acct_gift_is_owned == 1 ? 1 : 0;
}
static const char *acct_gift_cups_line(double n) {
    return ds_concat(ds_concat(ds_num_to_string(n), " "), tr_cups_unit());
}

static double hit_acct_gift(double x, double y, double owned, double i) {
    if (acct_open != ACCT_GIFTS || i < 0 || i > 3) return 0;
    return hit_rect(x, y, acct_gift_x(owned), acct_gift_y(i), acct_gift_col_w(), acct_gift_card_h()) == 1;
}
static double hit_acct_gift_buy(double x, double y) {
    return acct_open == ACCT_GIFTS && acct_gift_picked() == 1 && acct_gift_is_owned == 0 &&
           hit_button(x, y, acct_x(), acct_below_fields(), acct_w(), btn_h) == 1;
}
static double hit_acct_gift_send(double x, double y) {
    return acct_open == ACCT_GIFTS && acct_gift_picked_owned() == 1 &&
           hit_button(x, y, acct_x(), acct_below_fields(), acct_gift_btn_w(), btn_h) == 1;
}
static double hit_acct_gift_sell(double x, double y) {
    return acct_open == ACCT_GIFTS && acct_gift_picked_owned() == 1 &&
           hit_button(x, y, acct_x() + acct_gift_btn_w() + 12, acct_below_fields(), acct_gift_btn_w(), btn_h) == 1;
}

/* The recipient field shows the nickname as typed: masking it with stars,
 * as the password fields do, hid who the sticker was about to go to. */
static void acct_gift_draw_field(void) {
    double y = acct_field_y(0);
    double caret = acct_field == 0 && keyboard_visible() == 1;
    const char *shown = caret == 1 ? ds_concat(acct_buf[0], "|") : acct_buf[0];
    text(language == 1 ? "Ник получателя" : "Recipient nickname", acct_x() + login_lab_ox, y - login_lab, text_color);
    roundrect(acct_x(), y, acct_w(), login_fh, field_radius, card_bg);
    text_in_field(shown, acct_x() + field_text_pad, y, login_fh, text_color, 0.6);
}

/* Right-aligned text, vertically centred in a row of height h. */
static void acct_gift_text_right(const char *s, double right, double y, double h, uint32_t c, double sc) {
    double tw = text_ink_width(s) * sc;
    double th = text_ink_height(s) * sc;
    double tt = text_ink_top(s) * sc;
    text_scaled(s, right - tw, y + (h - th) / 2 - tt + 1.5, c, sc);
}

/* One sticker as a shop card: the picture on top, the name under it, and the
 * price (shop) or the little heap of owned stickers (collection) at the
 * bottom. A card that cannot be acted on is drawn dim, never hidden. On very
 * short screens the card turns into a single row so nothing overlaps. */
static void acct_gift_card(double i, double owned) {
    double x = acct_gift_x(owned), y = acct_gift_y(i), w = acct_gift_col_w(), h = acct_gift_card_h();
    double count = owned == 1 ? net_gift_count(i) : 0;
    double dim = owned == 1 && count < 1;
    double p = button_press_k(x, y, w, h);
    double sq = 1 - PRESS_SQUEEZE * p;
    double bx = x + w * (1 - sq) / 2, by = y + h * (1 - sq) / 2;
    double bw = w * sq, bh = h * sq;
    uint32_t ink = (uint32_t)shade(text_color, dim ? 0.5 : 1);
    uint32_t face = (uint32_t)shade(ui_purple, dim ? 0.5 : 1 - PRESS_SHADE * p);
    const char *name = acct_gift_name(i);
    const char *value = owned == 1 ? "" : ds_concat(ds_num_to_string(acct_gift_price(i)), language == 1 ? " куб." : " cups");
    button_face(bx, by, bw, bh, face);
    if (bh < 96) {
        /* Short screens: icon, name and the value share one narrow row. */
        double isz = clamp(bh * 0.62, 18, 44);
        double vw = owned == 1 ? gift_pile_w(clamp(bh * 0.24, 12, 22), count) : text_ink_width(value) * 0.44;
        double nx = bx + 8 + isz + 8;
        gift_icon(i, bx + 8, by + (bh - isz) / 2, isz);
        text_in_field(name, nx, by, bh, ink, fit_text_scale(name, bw - (nx - bx) - vw - 14, 0.58));
        if (owned == 1) {
            double psz = clamp(bh * 0.24, 12, 22);
            if (vw > 0) gift_pile(i, bx + bw - 8, by + (bh - psz) / 2, psz, count);
        } else {
            acct_gift_text_right(value, bx + bw - 8, by, bh, ink, 0.44);
        }
    } else {
        double isz = clamp(bh * 0.38, 20, 62);
        gift_icon(i, bx + (bw - isz) / 2, by + bh * 0.13, isz);
        text_in_box(name, bx, by + bh * 0.13 + isz + bh * 0.03, bw, 20, ink,
                    fit_text_scale(name, bw - 12, 0.64));
        if (owned == 1) {
            double psz = clamp(bh * 0.17, 14, 26);
            double pw = gift_pile_w(psz, count);
            if (pw > 0) gift_pile(i, bx + (bw + pw) / 2, by + bh * 0.78, psz, count);
        } else {
            text_in_box(value, bx, by + bh * 0.76, bw, 20, ink, fit_text_scale(value, bw - 12, 0.5));
        }
    }
    if (acct_gift_picked() == 1 && acct_gift_is_owned == owned && acct_gift_choice == i) {
        roundrect_outline(x, y, w, h, btn_radius, 3, 0xFFFFD34D);
    }
}

static void draw_acct_gifts(void) {
    double head_y = acct_gift_cols_top();
    const char *left = language == 1 ? "МАГАЗИН" : "SHOP";
    const char *right = language == 1 ? "МОИ ПОДАРКИ" : "MY GIFTS";
    text_in_box(left, acct_gift_x(0), head_y, acct_gift_col_w(), 20, text_color,
                fit_text_scale(left, acct_gift_col_w() - 8, 0.58));
    text_in_box(right, acct_gift_x(1), head_y, acct_gift_col_w(), 20, text_color,
                fit_text_scale(right, acct_gift_col_w() - 8, 0.58));
    double i = 0;
    while (i < 4) {
        acct_gift_card(i, 0);
        acct_gift_card(i, 1);
        i = i + 1;
    }
    /* The title itself is drawn by draw_account(); only the hint is ours. */
    const char *hint_line = language == 1 ? "Слева - магазин, справа - ваши стикеры"
                                          : "Shop on the left, your stickers on the right";
    ctext_scaled(hint_line, acct_top() + 27, text_color, fit_text_scale(hint_line, fit_w(screen_w), 0.5));
    acct_gift_draw_field();
    if (acct_gift_busy == 1) {
        const char *wait = acct_gift_busy_mode == 2 ? (language == 1 ? "Покупаем..." : "Buying...")
                           : acct_gift_busy_mode == 3 ? (language == 1 ? "Продаём..." : "Selling...")
                                                      : (language == 1 ? "Отправляем..." : "Sending...");
        button_scaled(wait, acct_x(), acct_below_fields(), acct_w(), btn_h, fit_text_scale(wait, acct_w() - 24, 0.6));
    } else if (acct_gift_picked() == 0) {
        /* Nothing chosen: no buttons at all, just a hint where to tap. */
        const char *hint = language == 1 ? "Нажмите на стикер" : "Tap a sticker";
        ctext_scaled(hint, acct_below_fields() + (btn_h - 16) / 2, text_color, 0.55);
    } else if (acct_gift_is_owned == 0) {
        const char *buy = ds_concat(language == 1 ? "Купить · " : "Buy · ",
                                    acct_gift_cups_line(acct_gift_price(acct_gift_choice)));
        if (cups < acct_gift_price(acct_gift_choice)) {
            button_disabled_scaled(buy, acct_x(), acct_below_fields(), acct_w(), btn_h, fit_text_scale(buy, acct_w() - 24, 0.6));
        } else {
            button_scaled(buy, acct_x(), acct_below_fields(), acct_w(), btn_h, fit_text_scale(buy, acct_w() - 24, 0.6));
        }
    } else {
        double bw = acct_gift_btn_w();
        const char *send = language == 1 ? "Подарить" : "Send";
        const char *sell = ds_concat(language == 1 ? "Продать · +" : "Sell · +",
                                     acct_gift_cups_line(acct_gift_refund(acct_gift_choice)));
        if (net_gift_count(acct_gift_choice) < 1) {
            button_disabled_scaled(send, acct_x(), acct_below_fields(), bw, btn_h, fit_text_scale(send, bw - 12, 0.56));
            button_disabled_scaled(sell, acct_x() + bw + 12, acct_below_fields(), bw, btn_h, fit_text_scale(sell, bw - 12, 0.52));
        } else {
            button_scaled(send, acct_x(), acct_below_fields(), bw, btn_h, fit_text_scale(send, bw - 12, 0.56));
            button_scaled(sell, acct_x() + bw + 12, acct_below_fields(), bw, btn_h, fit_text_scale(sell, bw - 12, 0.52));
        }
    }
}

/* A tap on either column picks that sticker and swaps the action row under
 * the columns; it never sends anything by itself. */
static double touch_acct_gift_cards(double x, double y, double action) {
    if (action != 0 || acct_open != ACCT_GIFTS) return 0;
    double i = 0;
    while (i < 4) {
        if (hit_acct_gift(x, y, 0, i) == 1) {
            acct_gift_choice = i; acct_gift_is_owned = 0; return 1;
        }
        if (hit_acct_gift(x, y, 1, i) == 1) {
            acct_gift_choice = i; acct_gift_is_owned = 1; return 1;
        }
        i = i + 1;
    }
    return 0;
}

static void acct_gifts_page(void) {
    acct_gift_choice = -1; acct_gift_busy = 0; acct_gift_is_owned = 0;
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
    if (acct_gift_picked_owned() != 1 || net_gift_count(acct_gift_choice) < 1) {
        acct_msg = language == 1 ? "Выберите свой стикер справа" : "Pick one of your stickers on the right"; return;
    }
    if (!acct_buf[0][0]) { acct_msg = language == 1 ? "Введите ник получателя" : "Enter recipient nickname"; return; }
    acct_gift_busy = 1; acct_gift_busy_mode = 0;
    acct_msg = language == 1 ? "Отправляем стикер..." : "Sending sticker...";
    net_gift_action(acct_gift_choice, acct_buf[0], 0);
}
static void acct_gift_sell(void) {
    keyboard_hide();
    if (acct_gift_picked_owned() != 1 || net_gift_count(acct_gift_choice) < 1) {
        acct_msg = language == 1 ? "Такого стикера нет в инвентаре" : "Sticker is not in your inventory"; return;
    }
    acct_gift_busy = 1; acct_gift_busy_mode = 3;
    acct_msg = language == 1 ? "Продаём стикер..." : "Selling sticker...";
    net_gift_action(acct_gift_choice, "", 3);
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
                    save_progress();
                } else if (acct_gift_busy_mode == 3) {
                    acct_msg = ds_concat(language == 1 ? "Стикер продан: +" : "Sticker sold: +",
                                         acct_gift_cups_line(acct_gift_refund(acct_gift_choice)));
                    cups = cups + acct_gift_refund(acct_gift_choice);
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
                acct_msg = language == 1 ? "Не удалось. Проверьте ник и подключение." : "Failed. Check nickname and connection.";
            }
        }
    }
}
