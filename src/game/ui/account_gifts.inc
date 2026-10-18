static double acct_gift_list_top(void) { return acct_top() + 58; }
static double acct_gift_card_h(void) { return clamp((screen_h - acct_top() - 300) / 4, 48, 104); }
static double acct_gift_step(void) { return acct_gift_card_h() + 8; }
static double acct_gift_visible(void) {
    double n = floor((screen_h - acct_gift_list_top() - 228) / acct_gift_step());
    return clamp(n, 1, 5);
}
static double acct_gift_footer_top(void) { return acct_gift_list_top() + acct_gift_visible() * acct_gift_step() + 2; }
static double acct_gift_btn_w(void) { return (acct_w() - 10) / 2; }
static double hit_acct_gift_buy(double x, double y) {
    return acct_open == ACCT_GIFTS && hit_button(x, y, acct_x(), acct_below_fields(), acct_gift_btn_w(), btn_h) == 1;
}
static double hit_acct_gift_send(double x, double y) {
    return acct_open == ACCT_GIFTS && hit_button(x, y, acct_x() + acct_gift_btn_w() + 10, acct_below_fields(), acct_gift_btn_w(), btn_h) == 1;
}
static const char *acct_gift_name(double i) {
    if (i == 0) return language == 1 ? "Роза" : "Rose";
    if (i == 1) return language == 1 ? "Торт" : "Cake";
    if (i == 2) return language == 1 ? "Мишка" : "Teddy Bear";
    return language == 1 ? "Ракета" : "Rocket";
}
static const char *acct_gift_icon(double i) { return i == 0 ? "✿" : i == 1 ? "★" : i == 2 ? "♥" : "➤"; }
static double acct_gift_price(double i) { return i == 0 ? 25 : i == 1 ? 100 : i == 2 ? 250 : 500; }
static double acct_gift_y(double i) { return acct_gift_list_top() + (i - acct_gift_scroll) * acct_gift_step(); }
static double hit_acct_gift(double x, double y, double i) {
    double iy = acct_gift_y(i);
    return acct_open == ACCT_GIFTS && i >= acct_gift_scroll && i < acct_gift_scroll + acct_gift_visible() &&
           iy >= acct_gift_list_top() && iy + acct_gift_card_h() <= acct_gift_list_top() + acct_gift_visible() * acct_gift_step() - 8 &&
           hit_rect(x, y, acct_x(), iy, acct_w(), acct_gift_card_h()) == 1;
}
static double acct_gift_scroll_max(void) { return 9 - acct_gift_visible(); }
static double acct_gift_bar_x(void) { return acct_x() - 20; }
static double acct_gift_bar_h(void) { return acct_gift_visible() * acct_gift_step() - 8; }
static double acct_gift_thumb_h(void) { return clamp(acct_gift_bar_h() * acct_gift_visible() / 9, 40, acct_gift_bar_h()); }
static double acct_gift_thumb_y(void) {
    double travel = acct_gift_bar_h() - acct_gift_thumb_h();
    return acct_gift_list_top() + (acct_gift_scroll_max() > 0 ? travel * acct_gift_scroll / acct_gift_scroll_max() : 0);
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
    if ((action == 1 || action == 3) && id == acct_gift_bar_drag_id) { acct_gift_bar_drag_id = -1; return 1; }
    if ((action == 1 || action == 3) && id == acct_gift_drag_id) {
        if (action == 1 && fabs(y - acct_gift_drag_start_y) < 12) {
            double row = floor((y - acct_gift_list_top()) / acct_gift_step() + acct_gift_scroll);
            if (row >= 0 && row < 4) { acct_gift_choice = row; acct_gift_is_owned = 0; }
            else if (row > 4 && row < 9) { acct_gift_choice = row - 5; acct_gift_is_owned = 1; }
        }
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
static const char *acct_gift_row_label(double row) {
    double i = row < 4 ? row : row - 5;
    const char *base = ds_concat(acct_gift_icon(i), ds_concat("  ", acct_gift_name(i)));
    if (row < 4) return ds_concat(base, ds_concat("     ·     ", ds_concat(ds_num_to_string(acct_gift_price(i)), language == 1 ? " куб." : " cups")));
    return ds_concat(base, ds_concat("     × ", ds_num_to_string(net_gift_count(i))));
}
static void draw_acct_gifts(void) {
    double top = acct_gift_list_top();
    double view_h = acct_gift_visible() * acct_gift_step() - 8;
    ctext_scaled(language == 1 ? "СТИКЕРНАЯ" : "STICKER SHOP", acct_top() + 27, text_color, 0.58);
    text_in_box(language == 1 ? "Листайте вниз до своих подарков" : "Scroll down to your collection",
                acct_x(), acct_top() + 38, acct_w(), 18, text_color, 0.40);
    roundrect(acct_x(), top, acct_w(), view_h, 14, card_bg);
    double row = floor(acct_gift_scroll);
    while (row < 9 && row < ceil(acct_gift_scroll + acct_gift_visible())) {
        double y = acct_gift_y(row), h = acct_gift_card_h();
        if (y >= top && y + h <= top + view_h) {
            if (row == 4) {
                button_scaled(language == 1 ? "МОИ ПОДАРКИ" : "MY GIFTS", acct_x() + 8, y, acct_w() - 16, h, 0.62);
            } else {
                double i = row < 4 ? row : row - 5;
                double owned = row > 4;
                double selected = i == acct_gift_choice && owned == acct_gift_is_owned;
                const char *label = acct_gift_row_label(row);
                button_scaled(label, acct_x() + 8, y, acct_w() - 16, h, fit_text_scale(label, acct_w() - 40, 0.62));
                if (selected) roundrect_outline(acct_x() + 8, y, acct_w() - 16, h, btn_radius, 3, 0xFFFFD34D);
            }
        }
        row = row + 1;
    }
    roundrect(acct_gift_bar_x(), top, 14, view_h, 7, 0x55303030);
    roundrect(acct_gift_bar_x(), acct_gift_thumb_y(), 14, acct_gift_thumb_h(), 7, ui_purple);
    draw_acct_field(language == 1 ? "Ник получателя" : "Recipient nickname", 0);
    double bw = acct_gift_btn_w();
    const char *buy = acct_gift_busy == 1 && acct_gift_busy_mode == 2 ? (language == 1 ? "Покупаем..." : "Buying...") :
                      ds_concat(language == 1 ? "Оставить себе · " : "Keep · ", ds_concat(ds_num_to_string(acct_gift_price(acct_gift_choice)), language == 1 ? " куб." : " cups"));
    const char *send = acct_gift_busy == 1 && acct_gift_busy_mode == 0 ? (language == 1 ? "Отправляем..." : "Sending...") : (language == 1 ? "Подарить" : "Send gift");
    button_scaled(buy, acct_x(), acct_below_fields(), bw, btn_h, fit_text_scale(buy, bw - 12, 0.48));
    button_scaled(send, acct_x() + bw + 10, acct_below_fields(), bw, btn_h, fit_text_scale(send, bw - 12, 0.54));
}
