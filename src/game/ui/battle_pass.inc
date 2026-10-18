/* New Year battle pass: 7 tiers. Winning battles earns XP (a win is
 * 10 XP, see award_battle_result); reaching a tier's XP unlocks it, and
 * opening (claiming) it costs candies. Tiers are claimed in order:
 *   1:  10 XP,  12 candies -> +10 cups
 *   2:  30 XP,  20 candies -> +20 cups
 *   3:  60 XP,  35 candies -> +30 cups
 *   4: 100 XP,  50 candies -> Elf Ebuc skin
 *   5: 150 XP,  70 candies -> +60 cups
 *   6: 220 XP,  95 candies -> +120 candies
 *   7: 300 XP, 120 candies -> Santa (Дед Мороз) class
 * XP is the cloud profile field "bpx"; claimed tiers ride the existing
 * "bp" profile field (bp_claimed). */

#define BP_TIERS 7
#define BP_XP_PER_WIN 10

static double bp_msg_t;
static char bp_msg_buf[160];
static const char *bp_msg;

static double bp_xp_req(double t) {
    if (t <= 1) return 10;
    if (t == 2) return 30;
    if (t == 3) return 60;
    if (t == 4) return 100;
    if (t == 5) return 150;
    if (t == 6) return 220;
    return 300;
}

static double bp_cost(double t) {
    if (t <= 1) return 12;
    if (t == 2) return 20;
    if (t == 3) return 35;
    if (t == 4) return 50;
    if (t == 5) return 70;
    if (t == 6) return 95;
    return 120;
}

static void bp_set_msg(const char *text) {
    double i = 0;
    if (!text) {
        text = "";
    }
    while (text[(int)i] != 0 && i < (double)(sizeof(bp_msg_buf) - 1)) {
        bp_msg_buf[(int)i] = text[(int)i];
        i = i + 1;
    }
    bp_msg_buf[(int)i] = 0;
    bp_msg = bp_msg_buf;
    bp_msg_t = msg_time;
}

static const char *bp_reward_text(double t) {
    if (t == 1) return ds_concat("+10 ", tr_cups_unit());
    if (t == 2) return ds_concat("+20 ", tr_cups_unit());
    if (t == 3) return ds_concat("+30 ", tr_cups_unit());
    if (t == 4) return language == 1 ? "Скин: Эльф-Эбук" : "Skin: Elf Ebuc";
    if (t == 5) return ds_concat("+60 ", tr_cups_unit());
    if (t == 6) return ds_concat("+120 ", tr_candies_unit());
    return language == 1 ? "Класс: Дед Мороз" : "Class: Santa";
}

static void bp_grant(double t) {
    if (t == 1) {
        cups = cups + 10;
    } else if (t == 2) {
        cups = cups + 20;
    } else if (t == 3) {
        cups = cups + 30;
    } else if (t == 5) {
        cups = cups + 60;
    } else if (t == 6) {
        candies = candies + 120;
    } else if (t == 7) {
        set_class_owned(CLASS_SANTA, 1);
    }
    /* tier 4 is the Elf Ebuc skin itself: skin_unlocked_for() opens it
     * once bp_claimed reaches 4, nothing else to grant. */
}

static void bp_claim(double t) {
    if (t <= bp_claimed) {
        return;
    }
    if (t > bp_claimed + 1) {
        bp_set_msg(language == 1 ? "Сначала открой прошлый уровень" : "Open the previous tier first");
        return;
    }
    if (bp_xp < bp_xp_req(t)) {
        bp_set_msg(language == 1 ? "Не хватает XP - выигрывай бои" : "Not enough XP - win battles");
        return;
    }
    if (candies < bp_cost(t)) {
        bp_set_msg(language == 1 ? "Не хватает конфет" : "Not enough candies");
        return;
    }
    candies = candies - bp_cost(t);
    bp_claimed = t;
    bp_grant(t);
    save_progress();
    check_achievements();
    bp_set_msg(ds_concat(language == 1 ? "Получено: " : "Claimed: ", bp_reward_text(t)));
    ds_log("battle pass: tier %g claimed, xp %g", t, bp_xp);
}

static double bp_row_y(double t) { return content_top() + 92 + (t - 1) * 66; }
static double bp_row_h(void) { return 58; }
static double bp_row_x(void) { return center_x(fit_w(620)); }
static double bp_row_w(void) { return fit_w(620); }

static void draw_bpass(void) {
    screen_fill();
    ctext_scaled(language == 1 ? "Новогодний баттл-пасс" : "New Year Battle Pass", content_top() + 6, newyear_gold + 0xFF000000,
                 fit_text_scale(language == 1 ? "Новогодний баттл-пасс" : "New Year Battle Pass", screen_w - 32, 0.85));
    double next = bp_claimed + 1;
    const char *xp_line;
    if (next > BP_TIERS) {
        xp_line = ds_concat(language == 1 ? "XP: " : "XP: ", ds_num_to_string((double)(int)bp_xp));
    } else {
        xp_line = ds_concat(
            ds_concat(ds_concat(language == 1 ? "XP: " : "XP: ", ds_num_to_string((double)(int)bp_xp)), " / "),
            ds_num_to_string(bp_xp_req(next)));
    }
    ctext_scaled(ds_concat(xp_line, language == 1 ? " (победа = 10 XP)" : " (a win = 10 XP)"), content_top() + 44,
                 text_color, 0.55);
    double t = 1;
    while (t <= BP_TIERS) {
        double x = bp_row_x();
        double y = bp_row_y(t);
        double w = bp_row_w();
        double claimed = t <= bp_claimed;
        card_state(x, y, w, bp_row_h(), claimed, 0, 1);
        const char *title = ds_concat(ds_concat(ds_num_to_string(t), ". "), bp_reward_text(t));
        text_scaled(title, x + 16, y + 8, text_color, fit_text_scale(title, w * 0.55, 0.55));
        const char *status;
        uint32_t sc = text_color;
        if (claimed) {
            status = language == 1 ? "Получено ✓" : "Claimed ✓";
            sc = 0xFF7ED321;
        } else if (bp_xp < bp_xp_req(t)) {
            status = ds_concat(ds_concat(ds_concat("XP ", ds_num_to_string((double)(int)bp_xp)), "/"),
                               ds_num_to_string(bp_xp_req(t)));
        } else if (t > next) {
            status = language == 1 ? "Сначала прошлый" : "Previous first";
        } else {
            status = ds_concat(ds_concat(language == 1 ? "Открыть · " : "Open · ", ds_num_to_string(bp_cost(t))),
                               ds_concat(" ", tr_candies_unit()));
            sc = 0xFFFFD54A;
        }
        double sw = text_ink_width(status) * 0.5;
        text_scaled(status, x + w - sw - 16, y + 14, sc, 0.5);
        t = t + 1;
    }
    bp_msg_t = tick_down(bp_msg_t);
    draw_msg(bp_msg, bp_row_y(BP_TIERS) + bp_row_h() + 16, bp_msg_t);
    draw_back();
}

static void touch_bpass(double x, double y, double action) {
    if (action != 0) {
        return;
    }
    if (back_hit(x, y) == 1) {
        start_transition(ST_LOBBY);
        return;
    }
    double t = 1;
    while (t <= BP_TIERS) {
        if (hit_rect(x, y, bp_row_x(), bp_row_y(t), bp_row_w(), bp_row_h()) == 1) {
            bp_claim(t);
            return;
        }
        t = t + 1;
    }
}

static double bpass_any_hit(double x, double y) {
    if (back_hit(x, y) == 1) {
        return 1;
    }
    double t = 1;
    while (t <= BP_TIERS) {
        if (hit_rect(x, y, bp_row_x(), bp_row_y(t), bp_row_w(), bp_row_h()) == 1) {
            return 1;
        }
        t = t + 1;
    }
    return 0;
}
