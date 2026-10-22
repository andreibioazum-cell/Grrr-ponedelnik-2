/* Button hitboxes and the press feedback. The same shape test decides whether a
 * button fired and whether it squashed, so a press without feedback (or
 * feedback without a press) cannot happen. Kept in its own file so core/ui.inc
 * does not grow.
 *
 * Everything here is multi-touch: four fingers can rest on four controls and
 * each one squeezes its own button. The old version kept a single pending
 * touch and a single pulse, so the second finger stole the first one's
 * squeeze and a two-handed grip looked dead. */

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
 * A pulse is owned by the control the finger landed on: the first one drawn
 * under that touch claims it, so a card claims it before the pill inside it.
 * press_k ramps 0 -> 1 -> 0 (update_button_press), and every edge is
 * smoothstepped, so the squeeze eases in and eases back out. */
#define PRESS_IN_TIME 0.06
#define PRESS_OUT_TIME 0.13
#define PRESS_SQUEEZE 0.05
#define PRESS_SHADE 0.25

/* Touches that went down and have not found their button yet, one per finger. */
#define PRESS_POINTS 4
/* Squeezes running right now, one per pressed button. */
#define PRESS_PULSES 4

static double press_point_x[PRESS_POINTS];
static double press_point_y[PRESS_POINTS];
static double press_point_id[PRESS_POINTS];
static double press_point_n;

static double press_k[PRESS_PULSES];
static double press_phase[PRESS_PULSES];
static double press_btn_x[PRESS_PULSES];
static double press_btn_y[PRESS_PULSES];
static double press_btn_w[PRESS_PULSES];
static double press_btn_h[PRESS_PULSES];
static double press_btn_gen[PRESS_PULSES];

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

/* A finger went down, or slid: keep its point in the queue, one entry per
 * pointer, so the controls can claim their own fingers. */
static void press_points_add(double x, double y, double id) {
    double i = 0;
    while (i < press_point_n) {
        if (press_point_id[(int)i] == id) {
            press_point_x[(int)i] = x;
            press_point_y[(int)i] = y;
            return;
        }
        i = i + 1;
    }
    if (press_point_n >= PRESS_POINTS) {
        /* Older fingers keep their buttons: the newest touch is the one to go. */
        press_point_n = PRESS_POINTS - 1;
    }
    press_point_x[(int)press_point_n] = x;
    press_point_y[(int)press_point_n] = y;
    press_point_id[(int)press_point_n] = id;
    press_point_n = press_point_n + 1;
}

/* One finger up: only its own point leaves the queue. */
static void press_points_drop(double id) {
    double i = 0;
    while (i < press_point_n) {
        if (press_point_id[(int)i] == id) {
            double j = i;
            while (j + 1 < press_point_n) {
                press_point_x[(int)j] = press_point_x[(int)(j + 1)];
                press_point_y[(int)j] = press_point_y[(int)(j + 1)];
                press_point_id[(int)j] = press_point_id[(int)(j + 1)];
                j = j + 1;
            }
            press_point_n = press_point_n - 1;
            return;
        }
        i = i + 1;
    }
}

static void press_points_clear(void) { press_point_n = 0; }

/* Drop every squeeze at once: a screen that closes may not leave a half
 * finished pulse that a look-alike button would pick up later. */
static void press_pulses_clear(void) {
    double i = 0;
    while (i < PRESS_PULSES) {
        press_k[(int)i] = 0;
        press_phase[(int)i] = 0;
        i = i + 1;
    }
}

/* Starts a pulse on this control: the same one squeezes again from wherever it
 * is now, otherwise a free slot takes it. */
static void press_pulse_claim(double x, double y, double w, double h) {
    double i = 0;
    double slot = -1;
    while (i < PRESS_PULSES) {
        double same = press_phase[(int)i] != 0 && press_btn_gen[(int)i] == ui_nav_gen && x == press_btn_x[(int)i] &&
                      y == press_btn_y[(int)i] && w == press_btn_w[(int)i] && h == press_btn_h[(int)i];
        if (same == 1) {
            press_k[(int)i] = 0;
            press_phase[(int)i] = 1;
            return;
        }
        if (slot < 0 && press_phase[(int)i] == 0) {
            slot = i;
        }
        i = i + 1;
    }
    if (slot < 0) {
        slot = 0;
    }
    press_btn_x[(int)slot] = x;
    press_btn_y[(int)slot] = y;
    press_btn_w[(int)slot] = w;
    press_btn_h[(int)slot] = h;
    press_btn_gen[(int)slot] = ui_nav_gen;
    press_k[(int)slot] = 0;
    press_phase[(int)slot] = 1;
}

/* Same pulse, but the control's silhouette has its own corner radius (a round
 * combat pad passes its radius, so its corners do not count). The claim test
 * is the very hitbox the touch handlers use, so a press either fires and
 * squeezes or does neither. Two fingers on two controls each get their own
 * pulse; a second finger on the same control squeezes it again. */
static double button_press_k_round(double x, double y, double w, double h, double rad) {
    double i = 0;
    double best = 0;
    if (press_covered() == 0) {
        while (i < press_point_n) {
            if (hit_round(press_point_x[(int)i], press_point_y[(int)i], x, y, w, h, rad) == 1) {
                press_points_drop(press_point_id[(int)i]);
                press_pulse_claim(x, y, w, h);
                continue;
            }
            i = i + 1;
        }
    }
    i = 0;
    while (i < PRESS_PULSES) {
        double same = press_phase[(int)i] != 0 && press_btn_gen[(int)i] == ui_nav_gen && x == press_btn_x[(int)i] &&
                      y == press_btn_y[(int)i] && w == press_btn_w[(int)i] && h == press_btn_h[(int)i];
        if (same == 1) {
            /* Smoothstep: the squeeze eases in and the spring back eases out. */
            double p = press_k[(int)i];
            double eased = p * p * (3 - 2 * p);
            if (eased > best) {
                best = eased;
            }
        }
        i = i + 1;
    }
    return best;
}

static double button_press_k(double x, double y, double w, double h) {
    return button_press_k_round(x, y, w, h, btn_radius);
}

static void update_button_press(void) {
    double i = 0;
    while (i < PRESS_PULSES) {
        if (press_phase[(int)i] == 1) {
            press_k[(int)i] = press_k[(int)i] + dt / PRESS_IN_TIME;
            if (press_k[(int)i] >= 1) {
                press_k[(int)i] = 1;
                press_phase[(int)i] = 2;
            }
        } else if (press_phase[(int)i] == 2) {
            press_k[(int)i] = press_k[(int)i] - dt / PRESS_OUT_TIME;
            if (press_k[(int)i] <= 0) {
                /* Back to rest. */
                press_k[(int)i] = 0;
                press_phase[(int)i] = 0;
            }
        }
        i = i + 1;
    }
}
