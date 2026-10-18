static double acct_gift_list_top(void) { return acct_top() + 58; }
static double acct_gift_card_h(void) { return clamp((screen_h - acct_top() - 300) / 4, 48, 104); }
static double acct_gift_step(void) { return acct_gift_card_h() + 8; }
static double acct_gift_visible(void) {
    double n = floor((screen_h - acct_gift_list_top() - 228) / acct_gift_step());
    return clamp(n, 1, 4);
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
static void acct_gifts_page(void) { acct_gift_choice = 0; acct_gift_busy = 0; acct_gift_scroll = 0; net_gift_refresh(); acct_go(ACCT_GIFTS); }
static void acct_gift_buy(void) {
    keyboard_hide();
    if (cups < acct_gift_price(acct_gift_choice)) { acct_msg = language == 1 ? "Не хватает кубков" : "Not enough cups"; return; }
    acct_gift_busy = 1; acct_gift_busy_mode = 2;
    acct_msg = language == 1 ? "Покупаем стикер..." : "Buying sticker...";
    net_gift_action(acct_gift_choice, "", 2);
}
static void acct_gift_send(void) {
    acct_sync(); keyboard_hide();
    if (net_gift_count(acct_gift_choice) < 1) { acct_msg = language == 1 ? "Сначала купите стикер себе" : "Buy one for yourself first"; return; }
    if (!acct_buf[0][0]) { acct_msg = language == 1 ? "Введите ник получателя" : "Enter recipient nickname"; return; }
    acct_gift_busy = 1; acct_gift_busy_mode = 0;
    acct_msg = language == 1 ? "Отправляем стикер..." : "Sending sticker...";
    net_gift_action(acct_gift_choice, acct_buf[0], 0);
}
static void draw_acct_gifts(void) {

        ctext_scaled(language == 1 ? "СТИКЕРНАЯ" : "STICKER SHOP", acct_top() + 27, text_color, 0.58);
        const char *hint = acct_gift_visible() < 4 ? (language == 1 ? "Листайте список вверх и вниз" : "Swipe the collection up or down") :
                                                    (language == 1 ? "Купите себе — подарок останется в коллекции" : "Buy for yourself — it stays in your collection");
        text_in_box(hint, acct_x(), acct_top() + 38, acct_w(), 18, text_color, 0.40);
        double gi = acct_gift_scroll;
        while (gi < 4 && gi < acct_gift_scroll + acct_gift_visible()) {
            double cy = acct_gift_y(gi), ch = acct_gift_card_h();
            double card_col = gi == 0 ? 0xFFFFE5ED : gi == 1 ? 0xFFFFF0D3 : gi == 2 ? 0xFFE2F2FF : 0xFFEFE5FF;
            double icon_col = gi == 0 ? 0xFFE65078 : gi == 1 ? 0xFFE0A92F : gi == 2 ? 0xFF478CC2 : 0xFF8550BE;
            roundrect(acct_x(), cy, acct_w(), ch, 18, card_col);
            roundrect_outline(acct_x(), cy, acct_w(), ch, 18, gi == acct_gift_choice ? 3 : 1, gi == acct_gift_choice ? 0xFFFFB52E : 0x66808080);
            circle(acct_x() + ch * 0.53, cy + ch / 2, ch * 0.34, icon_col);
            text_in_box(acct_gift_icon(gi), acct_x() + ch * 0.18, cy + ch * 0.14, ch * 0.7, ch * 0.7, 0xFFFFFFFF, 1.0);
            text_in_field(acct_gift_name(gi), acct_x() + ch + 16, cy + 4, ch * 0.42, text_color, fit_text_scale(acct_gift_name(gi), acct_w() - ch - 140, 0.62));
            const char *owned = ds_concat(language == 1 ? "У вас: " : "OWNED: ", ds_num_to_string(net_gift_count(gi)));
            text_in_box(owned, acct_x() + ch + 16, cy + ch * 0.53, acct_w() - ch - 26, ch * 0.32, text_color, 0.42);
            const char *cost = ds_concat(ds_num_to_string(acct_gift_price(gi)), language == 1 ? " кубков" : " cups");
            text_in_box(cost, acct_x() + acct_w() - 122, cy + 5, 112, ch - 10, text_color, 0.48);
            gi = gi + 1;
        }
        draw_acct_field(language == 1 ? "Ник получателя" : "Recipient nickname", 0);
        double bw = acct_gift_btn_w();
        const char *buy = acct_gift_busy == 1 && acct_gift_busy_mode == 2 ? (language == 1 ? "Покупаем..." : "Buying...") : (language == 1 ? "Купить себе" : "Buy for me");
        const char *send = acct_gift_busy == 1 && acct_gift_busy_mode == 0 ? (language == 1 ? "Отправляем..." : "Sending...") : (language == 1 ? "Подарить" : "Gift owned");
        button_scaled(buy, acct_x(), acct_below_fields(), bw, btn_h, fit_text_scale(buy, bw - 12, 0.54));
        button_scaled(send, acct_x() + bw + 10, acct_below_fields(), bw, btn_h, fit_text_scale(send, bw - 12, 0.54));
}
