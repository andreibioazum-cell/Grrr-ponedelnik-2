/* ---- actions ---------------------------------------------------------- */

/* Действие отказали (уже идёт другое): страница не должна остаться в
 * состоянии «Продаём...» навсегда. */
static void acct_gift_refused(void) {
    acct_gift_busy = 0;
    acct_msg = language == 1 ? "Другая операция с подарками ещё идёт"
                              : "Another gift operation is still running";
}

static void acct_gift_clear_pick(void) {
    acct_gift_choice = -1;
    acct_gift_is_owned = 0;
    acct_gift_own_row = -1;
}

static void acct_gifts_page(void) {
    acct_gift_clear_pick();
    acct_gift_busy = 0;
    acct_gift_scroll = 0;
    acct_gift_vel = 0;
    acct_gift_drag = -1;
    net_gift_refresh();
    acct_go(ACCT_GIFTS);
}
static void acct_gift_buy(void) {
    keyboard_hide();
    if (acct_gift_picked() != 1 || acct_gift_is_owned == 1) return;
    /* У админа стикер бесплатный (gifts.inc не трогает кубки в базе). */
    if (admin_playing() == 0 && cups < acct_gift_price(acct_gift_choice)) { acct_msg = language == 1 ? "Не хватает кубков" : "Not enough cups"; return; }
    acct_gift_busy = 1; acct_gift_busy_mode = 2;
    acct_msg = language == 1 ? "Покупаем стикер..." : "Buying sticker...";
    if (net_gift_action(acct_gift_choice, "", 2, 0) == 0) acct_gift_refused();
}
static void acct_gift_send(void) {
    acct_sync(); keyboard_hide();
    if (acct_gift_picked() != 1 || acct_gift_is_owned != 1) {
        acct_msg = language == 1 ? "Выберите свой стикер справа" : "Pick one of your stickers on the right"; return;
    }
    if (!acct_buf[0][0]) { acct_msg = language == 1 ? "Введите ник получателя" : "Enter recipient nickname"; return; }
    acct_gift_busy = 1; acct_gift_busy_mode = 0;
    acct_msg = language == 1 ? "Отправляем стикер..." : "Sending sticker...";
    if (net_gift_action(acct_gift_sel_id(), acct_buf[0], 0, 0) == 0) acct_gift_refused();
}
static void acct_gift_sell(void) {
    keyboard_hide();
    if (acct_gift_picked() != 1 || acct_gift_is_owned != 1) {
        acct_msg = language == 1 ? "Такого стикера нет в инвентаре" : "Sticker is not in your inventory"; return;
    }
    acct_gift_busy = 1; acct_gift_busy_mode = 3;
    acct_msg = language == 1 ? "Продаём стикер..." : "Selling sticker...";
    if (net_gift_action(acct_gift_sel_id(), "", 3, 0) == 0) acct_gift_refused();
}

static void acct_poll_gifts(void) {
    if (acct_gift_busy == 1) {
        double gift_status = net_gift_action_status();
        if (gift_status >= 2) {
            acct_gift_busy = 0;
            if (gift_status == 2) {
                /* The sticker the action was about: bought, sold or sent. */
                double acted = acct_gift_sel_id();
                if (acct_gift_busy_mode == 2) {
                    acct_msg = language == 1 ? "Стикер добавлен в коллекцию!" : "Sticker added to your collection!";
                    /* The purchase already wrote the new balance; take the
                     * server's number instead of subtracting from a local
                     * copy that may have been stale. */
                    double left = net_gift_cups();
                    cups = left >= 0 ? left : cups - acct_gift_price(acct_gift_choice);
                    if (cups < 0) cups = 0;
                    save_progress();
                    net_gift_count_add(acted, 1);
                } else if (acct_gift_busy_mode == 3) {
                    acct_msg = ds_concat(language == 1 ? "Стикер продан: +" : "Sticker sold: +",
                                         acct_gift_cups_line(acct_gift_refund(acted)));
                    double left = net_gift_cups();
                    cups = left >= 0 ? left : cups + acct_gift_refund(acted);
                    save_progress();
                    /* Only the sold sticker leaves the collection; the rest of
                     * it (the emerald, for one) stays on screen. */
                    net_gift_count_add(acted, -1);
                    acct_gift_clear_pick();
                } else {
                    acct_msg = language == 1 ? "Стикер отправлен!" : "Sticker sent!";
                    memset(acct_buf[0], 0, sizeof(acct_buf[0]));
                    acct_field = 0;
                    net_gift_count_add(acted, -1);
                    acct_gift_clear_pick();
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
