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
    return acct_open == ACCT_GIFTS && i >= acct_gift_scroll && i < acct_gift_scroll + acct_gift_visible() &&
           hit_rect(x, y, acct_x(), acct_gift_y(i), acct_w(), acct_gift_card_h()) == 1;
}
static void acct_gifts_page(void) {
    acct_gift_choice = 0; acct_gift_busy = 0; acct_gift_is_owned = 0; acct_gift_scroll = 0;
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
static double acct_gift_card_color(double i) {
    return i == 0 ? 0xFFFFE5ED : i == 1 ? 0xFFFFF0D3 : i == 2 ? 0xFFE2F2FF : 0xFFEFE5FF;
}
static double acct_gift_icon_color(double i) {
    return i == 0 ? 0xFFE65078 : i == 1 ? 0xFFE0A92F : i == 2 ? 0xFF478CC2 : 0xFF8550BE;
}
static void draw_acct_gifts(void) {
    ctext_scaled(language == 1 ? "СТИКЕРНАЯ" : "STICKER SHOP", acct_top() + 27, text_color, 0.58);
    text_in_box(language == 1 ? "Листайте вниз до своих подарков" : "Scroll down to your collection",
                acct_x(), acct_top() + 38, acct_w(), 18, text_color, 0.40);
    double row = acct_gift_scroll;
    while (row < 9 && row < acct_gift_scroll + acct_gift_visible()) {
        double y = acct_gift_y(row), h = acct_gift_card_h();
        if (row == 4) {
            roundrect(acct_x(), y, acct_w(), h, 15, 0xFF252842);
            text_in_box(language == 1 ? "МОИ ПОДАРКИ" : "MY GIFTS", acct_x() + 14, y, acct_w() - 28, h, 0xFFFFFFFF, 0.60);
        } else {
            double idx = row < 4 ? row : row - 5;
            double owned_section = row > 4;
            double selected = idx == acct_gift_choice && owned_section == acct_gift_is_owned;
            const char *label = acct_gift_name(idx);
            roundrect(acct_x(), y, acct_w(), h, 18, acct_gift_card_color(idx));
            roundrect_outline(acct_x(), y, acct_w(), h, 18, selected ? 3 : 1, selected ? 0xFFFFB52E : 0x55808080);
            circle(acct_x() + h * 0.53, y + h / 2, h * 0.34, acct_gift_icon_color(idx));
            text_in_box(acct_gift_icon(idx), acct_x() + h * 0.18, y + h * 0.14, h * 0.7, h * 0.7, 0xFFFFFFFF, 1.0);
            text_in_field(label, acct_x() + h + 16, y + 4, h * 0.42, text_color,
                          fit_text_scale(label, acct_w() - h - 140, 0.62));
            if (owned_section) {
                const char *count = ds_concat("× ", ds_num_to_string(net_gift_count(idx)));
                text_in_box(count, acct_x() + acct_w() - 112, y + 5, 100, h - 10, text_color, 0.58);
            } else {
                const char *price = ds_concat(ds_num_to_string(acct_gift_price(idx)), language == 1 ? " кубков" : " cups");
                text_in_box(price, acct_x() + acct_w() - 132, y + 5, 120, h - 10, text_color, 0.48);
            }
        }
        row = row + 1;
    }
    if (acct_gift_visible() < 9) {
        double track_x = acct_x() + acct_w() - 4;
        double track_y = acct_gift_list_top();
        double track_h = acct_gift_visible() * acct_gift_step() - 8;
        double thumb_h = track_h * acct_gift_visible() / 9;
        double thumb_y = track_y + (track_h - thumb_h) * acct_gift_scroll / (9 - acct_gift_visible());
        roundrect(track_x, track_y, 3, track_h, 2, 0x44333333);
        roundrect(track_x, thumb_y, 3, thumb_h, 2, 0xFF777777);
    }
    draw_acct_field(language == 1 ? "Ник получателя" : "Recipient nickname", 0);
    double bw = acct_gift_btn_w();
    const char *buy = acct_gift_busy == 1 && acct_gift_busy_mode == 2 ? (language == 1 ? "Покупаем..." : "Buying...") :
                      ds_concat(language == 1 ? "Оставить себе · " : "Keep · ", ds_concat(ds_num_to_string(acct_gift_price(acct_gift_choice)), language == 1 ? " куб." : " cups"));
    const char *send = acct_gift_busy == 1 && acct_gift_busy_mode == 0 ? (language == 1 ? "Отправляем..." : "Sending...") : (language == 1 ? "Подарить" : "Send gift");
    button_scaled(buy, acct_x(), acct_below_fields(), bw, btn_h, fit_text_scale(buy, bw - 12, 0.48));
    button_scaled(send, acct_x() + bw + 10, acct_below_fields(), bw, btn_h, fit_text_scale(send, bw - 12, 0.54));
}
