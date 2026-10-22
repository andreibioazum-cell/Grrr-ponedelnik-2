/* Button hitboxes and the press feedback. The same shape test decides whether a
 * button fired and whether it squashed, so a press without feedback (or
 * feedback without a press) cannot happen. Kept in its own file so core/ui.inc
 * does not grow. */

/* The exact shape of a rounded rectangle, the same one roundrect() and
 * cb_button_gradient() draw. The radius shrinks the same way as when drawing, so
 * a narrow pill button and a round battle button are tested against their real
 * outline, and the transparent corners press nothing. */
static double hit_round(double px, double py, double x, double y, double w, double h, double rad) {
    if (hit_rect(px, py, x, y, w, h) == 0) {
        return 0;
    }
    if (rad > w / 2) {
        rad = w / 2;
    }
    if (rad > h / 2) {
        rad = h / 2;
    }
    if (rad <= 0) {
        return 1;
    }
    double cx = clamp(px, x + rad, x + w - rad);
    double cy = clamp(py, y + rad, y + h - rad);
    double dx = px - cx;
    double dy = py - cy;
    return dx * dx + dy * dy <= rad * rad ? 1 : 0;
}

/* The hitbox is exactly what is on screen, no invisible border around it. The
 * press effect (button_press_k) uses the same test, so a button either fires and
 * squashes or does neither. */
static double hit_button(double px, double py, double x, double y, double w, double h) {
    return hit_round(px, py, x, y, w, h, btn_radius);
}

/* Press feedback: a button answers the touch itself, not the release. The
 * press squeezes it by 5% and darkens it by 25%, and the squeeze then springs
 * back on its own - the finger does not have to lift, stay put or be let go
 * gently, the pulse always runs to the end. That is what makes a press feel
 * like a press instead of a hold.
 *
 * The pulse is owned by the control the finger landed on (press_btn_*): the
 * first one drawn under the touch claims it, so a card claims it before the
 * pill inside it. press_k ramps 0 -> 1 -> 0 (update_button_press), and every
 * edge is smoothstepped, so the squeeze eases in and eases back out. */
#define PRESS_IN_TIME 0.06
#define PRESS_OUT_TIME 0.13
#define PRESS_SQUEEZE 0.05
#define PRESS_SHADE 0.25

static double press_btn_live;
static double press_btn_x;
static double press_btn_y;
static double press_btn_w;
static double press_btn_h;
static double press_btn_gen;
/* Input layer of the control being drawn: 0 screen, 1 warning/chat,
 * 2 documents, 3 account. Hidden overlays must not steal the press either. */
static double press_overlay;

/* A control hidden under something drawn later never takes the squeeze: the
 * touch belongs to what is on top (the consent warning, the open chat, the
 * intro card or the results). An overlay that has buttons of its own raises
 * press_overlay while it draws them. */
static double press_covered(void) {
    if (acct_open != 0) {
        return press_overlay != 3;
    }
    if (docs_open == 1) {
        return press_overlay != 2;
    }
    if (warn_open == 1) {
        return press_overlay != 1;
    }
    if (press_overlay == 1) {
        return 0;
    }
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
        return finished != 0 || (game_state == ST_ONLINE && chat_open == 1);
    }
    return 0;
}

/* Same pulse, but the control's silhouette has its own corner radius (a round
 * combat pad passes its radius, so its corners do not count). The claim test
 * is the very hitbox the touch handlers use, so a press either fires and
 * squeezes or does neither. A fresh touch always claims, even while an older
 * pulse is still springing back: a quick second tap gets its squeeze too. */
static double button_press_k_round(double x, double y, double w, double h, double rad) {
    if (press_new == 1 && press_covered() == 0 && hit_round(press_x, press_y, x, y, w, h, rad) == 1) {
        double same = press_btn_live == 1 && press_phase != 0 && press_btn_gen == ui_nav_gen && x == press_btn_x &&
                      y == press_btn_y && w == press_btn_w && h == press_btn_h;
        press_new = 0;
        press_btn_live = 1;
        press_btn_x = x;
        press_btn_y = y;
        press_btn_w = w;
        press_btn_h = h;
        press_btn_gen = ui_nav_gen;
        if (same == 0) {
            press_k = 0;
        }
        /* The same control squeezes again from wherever it is now. */
        press_phase = 1;
    }
        /* A pulse belongs to the screen that took it: the next screen often has its
         * own button at the same spot (menu and settings rows line up), and on a low
         * fps a still-moving pulse would flash on that one. */
    if (press_btn_live == 1 && press_phase != 0 && press_btn_gen == ui_nav_gen && x == press_btn_x &&
        y == press_btn_y && w == press_btn_w && h == press_btn_h) {
        /* Smoothstep: the squeeze eases in and the spring back eases out. */
        return press_k * press_k * (3 - 2 * press_k);
    }
    return 0;
}

static double button_press_k(double x, double y, double w, double h) {
    return button_press_k_round(x, y, w, h, btn_radius);
}

static void update_button_press(void) {
    if (press_phase == 1) {
        press_k = press_k + dt / PRESS_IN_TIME;
        if (press_k >= 1) {
            press_k = 1;
            press_phase = 2;
        }
        return;
    }
    if (press_phase == 2) {
        press_k = press_k - dt / PRESS_OUT_TIME;
        if (press_k <= 0) {
            /* Back to rest. */
            press_k = 0;
            press_phase = 0;
            press_btn_live = 0;
        }
    }
}
