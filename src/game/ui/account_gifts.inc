#include "gift_icons.inc"

/* Sticker shop and collection on one page: the shop is the left column,
 * the player's own stickers are the right one. The shop always shows the
 * whole catalogue; the collection lists every owned sticker as its own card
 * (two poops are two cards) and scrolls smoothly, without snapping. A card
 * is drawn once - one PNG sprite, the name and a rarity caption, both in the
 * rarity colour - and has no buttons on it: a tap selects the card and the
 * action row under the columns buys it or sends/sells the selected sticker. */

#define ACCT_GIFT_GAP 10
#define ACCT_GIFT_COL_GAP 12
#define ACCT_GIFT_PANEL_PAD 8

static double acct_gift_cols_top(void) { return acct_top() + 58; }
static double acct_gift_head_h(void) { return 30; }
static double acct_gift_view_top(void) { return acct_gift_cols_top() + acct_gift_head_h(); }
static double acct_gift_shop_rows(void) {
    double n = net_gift_types();
    return n > 1 ? n : 1;
}
/* Five catalogue cards have to fit the window without scrolling, so the card
 * height follows the free space; the collection then scrolls through that
 * same window, and the room under it stays free for the field and buttons. */
static double acct_gift_card_h(void) {
    double avail = screen_h - screen_margin - 250 - acct_gift_view_top();
    double rows = acct_gift_shop_rows();
    return clamp((avail - (rows - 1) * ACCT_GIFT_GAP) / rows, 40, 150);
}
static double acct_gift_step(void) { return acct_gift_card_h() + ACCT_GIFT_GAP; }
static double acct_gift_view_h(void) { return acct_gift_shop_rows() * acct_gift_step() - ACCT_GIFT_GAP; }
static double acct_gift_view_bottom(void) { return acct_gift_view_top() + acct_gift_view_h(); }
static double acct_gift_footer_top(void) { return acct_gift_view_bottom(); }
static double acct_gift_col_w(void) { return (acct_w() - ACCT_GIFT_COL_GAP) / 2; }
static double acct_gift_btn_w(void) { return (acct_w() - ACCT_GIFT_COL_GAP) / 2; }
/* owned == 0 is the shop column on the left, 1 the collection on the right. */
static double acct_gift_x(double owned) { return owned == 1 ? acct_x() + acct_gift_col_w() + ACCT_GIFT_COL_GAP : acct_x(); }
static double acct_gift_panel_x(double owned) { return acct_gift_x(owned) - ACCT_GIFT_PANEL_PAD; }
static double acct_gift_panel_y(void) { return acct_gift_cols_top() - 6; }
static double acct_gift_panel_w(void) { return acct_gift_col_w() + 2 * ACCT_GIFT_PANEL_PAD; }
static double acct_gift_panel_h(void) { return acct_gift_head_h() + acct_gift_view_h() + 12; }
static double acct_gift_price(double i) { return net_gift_price(i); }
static double acct_gift_refund(double i) { return net_gift_refund(i); }
static const char *acct_gift_name(double i) {
    if (i == 0) return language == 1 ? "Какашка" : "Poop";
    if (i == 1) return language == 1 ? "Торт" : "Cake";
    if (i == 2) return language == 1 ? "Мишка" : "Teddy Bear";
    if (i == 3) return language == 1 ? "Ракета" : "Rocket";
    return language == 1 ? "Изумруд" : "Emerald";
}
static const char *acct_gift_cups_line(double n) {
    return ds_concat(ds_concat(ds_num_to_string(n), " "), tr_cups_unit());
}

/* ---- collection: one row per owned sticker ---------------------------- */

static double acct_gift_own_total(void) {
    double n = 0, i = 0;
    while (i < acct_gift_shop_rows()) {
        n = n + net_gift_count(i);
        i = i + 1;
    }
    return n;
}
/* Which sticker sits in a row; -1 when the row is past the end. */
static double acct_gift_own_id(double row) {
    double base = 0, i = 0;
    if (row < 0) return -1;
    while (i < acct_gift_shop_rows()) {
        double c = net_gift_count(i);
        if (row < base + c) return i;
        base = base + c;
        i = i + 1;
    }
    return -1;
}
static double acct_gift_scroll_max(void) {
    double extra = acct_gift_own_total() * acct_gift_step() - ACCT_GIFT_GAP - acct_gift_view_h();
    return extra > 0 ? extra : 0;
}
static double acct_gift_own_sel(void) {
    if (acct_gift_is_owned != 1) return -1;
    if (acct_gift_own_row < 0 || acct_gift_own_row >= acct_gift_own_total()) return -1;
    return acct_gift_own_row;
}
static double acct_gift_sel_id(void) {
    if (acct_gift_is_owned == 1) return acct_gift_own_id(acct_gift_own_sel());
    return acct_gift_choice;
}
static double acct_gift_picked(void) {
    if (acct_gift_is_owned == 1) return acct_gift_own_sel() >= 0 ? 1 : 0;
    return acct_gift_choice >= 0 && acct_gift_choice < acct_gift_shop_rows() ? 1 : 0;
}
/* Smooth glide after the finger is lifted; no snapping to whole rows, and a
 * gentle decay so the list does not fly away on a short flick. */
static void acct_gift_scroll_tick(void) {
    if (acct_open != ACCT_GIFTS || acct_gift_drag >= 0) return;
    /* The list can shrink under the scroll (a sticker was just sold), so the
     * position is clamped on every tick, not only while it is gliding. */
    double mx = acct_gift_scroll_max();
    if (acct_gift_scroll > mx) acct_gift_scroll = mx;
    if (acct_gift_scroll < 0) acct_gift_scroll = 0;
    if (fabs(acct_gift_vel) < 0.6) {
        acct_gift_vel = 0;
        return;
    }
    acct_gift_scroll = acct_gift_scroll + acct_gift_vel * dt;
    acct_gift_vel = acct_gift_vel * exp(-6.0 * dt);
    if (acct_gift_scroll <= 0) {
        acct_gift_scroll = 0;
        acct_gift_vel = 0;
    } else if (acct_gift_scroll >= mx) {
        acct_gift_scroll = mx;
        acct_gift_vel = 0;
    }
}

/* ---- input ------------------------------------------------------------ */

static double acct_gift_hit_row(double y) {
    return floor((y - acct_gift_view_top() + acct_gift_scroll) / acct_gift_step());
}
static double hit_acct_gift(double x, double y, double i) {
    if (acct_open != ACCT_GIFTS || i < 0 || i >= acct_gift_shop_rows()) return 0;
    double cy = acct_gift_view_top() + i * acct_gift_step();
    return hit_rect(x, y, acct_gift_x(0), cy, acct_gift_col_w(), acct_gift_card_h()) == 1;
}
static double hit_acct_gift_any(double x, double y) {
    double i = 0;
    while (i < acct_gift_shop_rows()) {
        if (hit_acct_gift(x, y, i) == 1) return 1;
        i = i + 1;
    }
    return 0;
}
static double hit_acct_gift_buy(double x, double y) {
    return acct_open == ACCT_GIFTS && acct_gift_picked() == 1 && acct_gift_is_owned == 0 &&
           hit_button(x, y, acct_x(), acct_below_fields(), acct_w(), btn_h) == 1;
}
static double hit_acct_gift_send(double x, double y) {
    return acct_open == ACCT_GIFTS && acct_gift_picked() == 1 && acct_gift_is_owned == 1 &&
           hit_button(x, y, acct_x(), acct_below_fields(), acct_gift_btn_w(), btn_h) == 1;
}
static double hit_acct_gift_sell(double x, double y) {
    return acct_open == ACCT_GIFTS && acct_gift_picked() == 1 && acct_gift_is_owned == 1 &&
           hit_button(x, y, acct_x() + acct_gift_btn_w() + ACCT_GIFT_COL_GAP, acct_below_fields(), acct_gift_btn_w(), btn_h) == 1;
}

/* The collection keeps its own down/move/up machine: a drag scrolls, a short
 * press selects the sticker under the finger. The shop never scrolls, so a
 * touch on it selects right away. Selecting never sends or sells by itself. */
static double touch_acct_gift_list(double x, double y, double action, double id) {
    if (acct_open != ACCT_GIFTS) return 0;
    double ox = acct_gift_x(1), oy = acct_gift_view_top();
    double ow = acct_gift_col_w(), oh = acct_gift_view_h();
    double inside_list = x >= ox && x <= ox + ow && y >= oy && y <= oy + oh;
    if (action == 2) {
        if (acct_gift_drag < 0 || id != acct_gift_drag) return 0;
        double dy = y - acct_gift_drag_y;
        acct_gift_scroll = clamp(acct_gift_scroll - dy, 0, acct_gift_scroll_max());
        /* Slower than the raw finger speed: a short flick used to throw the
         * list far past the sticker the player was aiming at. */
        acct_gift_vel = clamp(-dy / fmax(dt, 0.016) * 0.55, -22, 22);
        acct_gift_drag_y = y;
        acct_gift_drag_move = acct_gift_drag_move + fabs(dy);
        return 1;
    }
    if (action == 1 || action == 3) {
        if (acct_gift_drag < 0 || id != acct_gift_drag) return 0;
        if (action == 1 && acct_gift_drag_move < 14) {
            double row = acct_gift_hit_row(y);
            double picked = acct_gift_own_id(row);
            if (picked >= 0) {
                acct_gift_choice = picked;
                acct_gift_is_owned = 1;
                acct_gift_own_row = row;
                play_button_click();
            }
        }
        acct_gift_drag = -1;
        return 1;
    }
    if (action != 0) return 0;
    if (inside_list) {
        acct_gift_drag = id;
        acct_gift_drag_y = y;
        acct_gift_drag_move = 0;
        acct_gift_vel = 0;
        return 1;
    }
    double i = 0;
    while (i < acct_gift_shop_rows()) {
        if (hit_acct_gift(x, y, i) == 1) {
            acct_gift_choice = i;
            acct_gift_is_owned = 0;
            acct_gift_own_row = -1;
            acct_gift_vel = 0;
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

/* ---- drawing ---------------------------------------------------------- */

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

/* One card: one picture, the name and the rarity caption. The face is cut by
 * hand to [clip_top, clip_bottom] - squared off where the window slices it -
 * and an element is drawn only when it lies completely inside, so nothing can
 * spill over the header or the field below. */
static void acct_gift_card(double id, double owned, double card_y, double clip_top, double clip_bottom, double selected) {
    double full = acct_gift_card_h();
    double x = acct_gift_x(owned), w = acct_gift_col_w();
    double y = card_y > clip_top ? card_y : clip_top;
    double bottom = card_y + full < clip_bottom ? card_y + full : clip_bottom;
    double h = bottom - y;
    if (h < 2) return;
    double cut_top = card_y < clip_top ? 1 : 0;
    double cut_bottom = card_y + full > clip_bottom ? 1 : 0;
    double p = cut_top == 0 && cut_bottom == 0 ? button_press_k(x, card_y, w, full) : 0;
    uint32_t ink = text_color;
    uint32_t face = (uint32_t)shade(ui_purple, 1 - PRESS_SHADE * p);
    const char *name = acct_gift_name(id);
    const char *rarity = gift_rarity_name(id);
    const char *value = owned == 1 ? "" : ds_concat(ds_num_to_string(acct_gift_price(id)), language == 1 ? " куб." : " cups");
    double r = btn_radius;
    roundrect(x, y, w, h, r, face);
    if (cut_top == 1) rect(x, y, w, fmin(r, h), face);
    if (cut_bottom == 1) rect(x, y + h - fmin(r, h), w, fmin(r, h), face);
    if (full < 96) {
        /* Short cards: picture, name and the price share one narrow row. */
        double isz = clamp(full * 0.62, 18, 44);
        double nx = x + 8 + isz + 8;
        double vw = owned == 1 ? 0 : text_ink_width(value) * 0.44;
        double iy = card_y + (full - isz) / 2;
        if (iy >= y && iy + isz <= bottom) gift_icon(id, x + 8, iy, isz);
        if (card_y + 2 >= y && card_y + full - 2 <= bottom) {
            gift_text_rarity_left(name, nx, card_y, full, id, fit_text_scale(name, w - (nx - x) - vw - 14, 0.58));
            if (owned == 0) acct_gift_text_right(value, x + w - 8, card_y, full, ink, 0.44);
        }
    } else {
        double isz = clamp(full * 0.36, 20, 56);
        double iy = card_y + full * 0.10;
        double ny = iy + isz + full * 0.02;
        double ry = ny + 21;
        double py = card_y + full * 0.80;
        if (iy >= y && iy + isz <= bottom) gift_icon(id, x + (w - isz) / 2, iy, isz);
        if (ny >= y && ny + 19 <= bottom) gift_text_rarity(name, x, ny, w, 19, id, fit_text_scale(name, w - 12, 0.6));
        if (ry >= y && ry + 14 <= bottom) gift_text_rarity(rarity, x, ry, w, 14, id, fit_text_scale(rarity, w - 16, 0.34));
        if (owned == 0 && py >= y && py + 18 <= bottom) text_in_box(value, x, py, w, 18, ink, fit_text_scale(value, w - 12, 0.48));
    }
    if (selected == 1 && cut_top == 0 && cut_bottom == 0) {
        roundrect_outline(x, y, w, full, r, 3, 0xFFFFD34D);
    }
}

static void acct_gift_panel(double owned) {
    roundrect(acct_gift_panel_x(owned), acct_gift_panel_y(), acct_gift_panel_w(), acct_gift_panel_h(), 14, card_bg);
}

static void draw_acct_gifts(void) {
    double vt = acct_gift_view_top(), vb = acct_gift_view_bottom();
    acct_gift_panel(0);
    acct_gift_panel(1);
    double i = 0;
    while (i < acct_gift_shop_rows()) {
        double picked = acct_gift_picked() == 1 && acct_gift_is_owned == 0 && acct_gift_choice == i;
        acct_gift_card(i, 0, vt + i * acct_gift_step(), vt, vb, picked);
        i = i + 1;
    }
    double total = acct_gift_own_total();
    if (total < 1) {
        const char *empty = language == 1 ? "Пока нет подарков" : "No stickers yet";
        ctext_scaled(empty, vt + acct_gift_view_h() / 2 - 8, text_color, fit_text_scale(empty, acct_gift_col_w() - 16, 0.55));
    } else {
        double row = floor(acct_gift_scroll / acct_gift_step());
        double cy = vt + row * acct_gift_step() - acct_gift_scroll;
        while (row < total && cy < vb) {
            double id = acct_gift_own_id(row);
            double picked = acct_gift_own_sel() == row ? 1 : 0;
            acct_gift_card(id, 1, cy, vt, vb, picked);
            row = row + 1;
            cy = cy + acct_gift_step();
        }
    }
    const char *left = language == 1 ? "МАГАЗИН" : "SHOP";
    const char *right = language == 1 ? "МОИ ПОДАРКИ" : "MY GIFTS";
    text_in_box(left, acct_gift_x(0), acct_gift_cols_top(), acct_gift_col_w(), 22, text_color,
                fit_text_scale(left, acct_gift_col_w() - 8, 0.58));
    text_in_box(right, acct_gift_x(1), acct_gift_cols_top(), acct_gift_col_w(), 22, text_color,
                fit_text_scale(right, acct_gift_col_w() - 8, 0.58));
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
        const char *hint = language == 1 ? "Нажмите на стикер" : "Tap a sticker";
        ctext_scaled(hint, acct_below_fields() + (btn_h - 16) / 2, text_color, 0.55);
    } else if (acct_gift_is_owned == 0) {
        const char *buy = ds_concat(language == 1 ? "Купить · " : "Buy · ",
                                    acct_gift_cups_line(acct_gift_price(acct_gift_sel_id())));
        if (cups < acct_gift_price(acct_gift_sel_id())) {
            button_disabled_scaled(buy, acct_x(), acct_below_fields(), acct_w(), btn_h, fit_text_scale(buy, acct_w() - 24, 0.6));
        } else {
            button_scaled(buy, acct_x(), acct_below_fields(), acct_w(), btn_h, fit_text_scale(buy, acct_w() - 24, 0.6));
        }
    } else {
        double bw = acct_gift_btn_w();
        const char *send = language == 1 ? "Подарить" : "Send";
        const char *sell = ds_concat(language == 1 ? "Продать · +" : "Sell · +",
                                     acct_gift_cups_line(acct_gift_refund(acct_gift_sel_id())));
        button_scaled(send, acct_x(), acct_below_fields(), bw, btn_h, fit_text_scale(send, bw - 12, 0.56));
        button_scaled(sell, acct_x() + bw + ACCT_GIFT_COL_GAP, acct_below_fields(), bw, btn_h, fit_text_scale(sell, bw - 12, 0.52));
    }
}
