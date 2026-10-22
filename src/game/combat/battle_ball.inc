/* Ebuc has two ults and a small button above the pad to pick one: the letter
 * says what the next tap throws - "Д" for the dispenser, "Ш" for the brick.
 *
 * The brick is aimed by hand. Tapping the pad starts a 3-2-1 countdown above
 * the cube, and while it runs a finger dragged anywhere on the arena points
 * the throw; the brick leaves in that direction when the count hits zero and
 * bounces off the walls. Ten bounces is the limit: after that it just bursts
 * without doing any damage. Lifting the finger early is fine - the last
 * direction stays.
 *
 * Ebuc only: every other class taps the pad and gets its usual super, and
 * nothing in the game charges by holding. */

#define BALL_COUNT_FROM 3.0      /* countdown digits: 3, 2, 1 */
#define BALL_COUNT_STEP 0.7      /* seconds per digit */
#define BALL_SPEED 620.0       /* px per second */
#define BALL_RADIUS 24.0
#define BALL_DAMAGE 6.0
#define BALL_MAX_BOUNCES 10.0
#define BALL_MAX_TIME 8.0      /* safety net: the ball always goes away */
#define BALL_TRAIL 12

typedef struct {
    double active;
    double x, y, vx, vy;
    double bounces, t, spin;
    /* Aiming: the countdown runs in aim_t, the throw follows aim_dx/aim_dy. */
    double aim_on, aim_id, aim_t, aim_dx, aim_dy;
    double trail_x[BALL_TRAIL], trail_y[BALL_TRAIL], trail_a[BALL_TRAIL];
} UltBall;

static UltBall ball;

/* The ult mode of a class that has two of them. Ebuc (the cube called "буК")
 * can throw either his dispenser or this ball, and the small button above the
 * pad switches between them: 0 - dispenser, 1 - ball. */
static double ball_mode;

static double ult_mode_r(void) { return 26; }
static double ult_mode_x(void) { return atk2_x; }
static double ult_mode_y(void) { return atk2_y - battle_pad_r() - ult_mode_r() - 10; }

static double ult_mode_hit(double x, double y) {
    double dx = x - ult_mode_x();
    double dy = y - ult_mode_y();
    return sqrt(dx * dx + dy * dy) <= ult_mode_r() + 14;
}

static void ult_mode_toggle(void) {
    ball_mode = ball_mode == 1 ? 0 : 1;
    play_button_click();
}

/* Shown only where both modes exist, so the pad keeps its meaning everywhere
 * else. The letter says what the next tap will throw. */
static void draw_ult_mode(void) {
    double x = ult_mode_x();
    double y = ult_mode_y();
    double r = ult_mode_r();
    const char *label;
    uint32_t fill;
    double sc = 0.62;
    if (player_class != CLASS_EBUC || game_state != ST_SOLO) {
        return;
    }
    label = ball_mode == 1 ? "Ш" : "Д";
    fill = ball_mode == 1 ? 0xFFFFB300u : 0xFF2F6FEDu;
    circle(x, y, r + 3, 0x66000000u);
    circle(x, y, r, fill);
    ring(x, y, r, 3, 0xCCFFFFFFu);
    text_scaled(label, x - text_ink_width(label) * sc / 2, y - text_ink_height(label) * sc / 2,
                (unsigned)0xFFFFFFFFu, sc);
}

static void ball_reset(void) {
    double i = 0;
    ball.active = 0;
    ball.x = 0;
    ball.y = 0;
    ball.vx = 0;
    ball.vy = 0;
    ball.bounces = 0;
    ball.t = 0;
    ball.aim_on = 0;
    ball.aim_id = -1;
    ball.aim_t = 0;
    ball.aim_dx = 1;
    ball.aim_dy = 0;
    ball_mode = 0;
    while (i < BALL_TRAIL) {
        ball.trail_a[(int)i] = 0;
        i = i + 1;
    }
}

static void ball_trail_push(double x, double y) {
    double i = BALL_TRAIL - 1;
    while (i > 0) {
        ball.trail_x[(int)i] = ball.trail_x[(int)(i - 1)];
        ball.trail_y[(int)i] = ball.trail_y[(int)(i - 1)];
        ball.trail_a[(int)i] = ball.trail_a[(int)(i - 1)];
        i = i - 1;
    }
    /* Sparks spray a little to the sides, otherwise the ball hides its own
     * tail while it flies: at 620 px/s the trail sits inside the ball. */
    ball.trail_x[0] = x + (rand() % 21 - 10);
    ball.trail_y[0] = y + (rand() % 21 - 10);
    ball.trail_a[0] = 1;
}

/* The pad was pressed in brick mode: start the countdown. The finger that
 * started it owns the aim, so a second finger cannot drag the throw away. */
static void ball_aim_start(double id) {
    if (ball.aim_on == 1 || ball.active == 1) {
        return;
    }
    if (finished != 0 || player_block == 1 || player_grabbed() == 1) {
        return;
    }
    if (player_stun > 0 || player_freeze > 0 || super_cd > 0) {
        return;
    }
    ball.aim_on = 1;
    ball.aim_id = id;
    ball.aim_t = BALL_COUNT_FROM * BALL_COUNT_STEP;
    ball.aim_dx = cos(player->angle);
    ball.aim_dy = sin(player->angle);
}

/* The digit the countdown shows right now: 3, 2 or 1. */
static double ball_aim_digit(void) {
    double d = ceil(ball.aim_t / BALL_COUNT_STEP);
    if (d < 1) {
        d = 1;
    }
    if (d > BALL_COUNT_FROM) {
        d = BALL_COUNT_FROM;
    }
    return d;
}

/* The finger moved: the brick goes where it points, from the cube outwards.
 * A touch right on the cube means nothing yet, so the old aim stays. */
static void ball_aim_point(double x, double y) {
    double dx = x - player->x;
    double dy = y - player->y;
    double len = sqrt(dx * dx + dy * dy);
    if (ball.aim_on == 0 || len < 14) {
        return;
    }
    ball.aim_dx = dx / len;
    ball.aim_dy = dy / len;
}

static void ball_aim_cancel(void) {
    ball.aim_on = 0;
    ball.aim_id = -1;
    ball.aim_t = 0;
}

/* The countdown is over: the brick is thrown. Lifting the finger early does
 * not stop it, the aim is already chosen. */
static void ball_aim_tick(void) {
    if (ball.aim_on == 0) {
        return;
    }
    if (finished != 0 || player_block == 1 || player_grabbed() == 1 || player_stun > 0 || player_freeze > 0) {
        ball_aim_cancel();
        return;
    }
    ball.aim_t = ball.aim_t - dt;
    if (ball.aim_t > 0) {
        return;
    }
    ball_aim_cancel();
    ball_fire();
}

static void ball_fire(void) {
    if (finished != 0 || ball.active == 1 || player_block == 1 || player_grabbed() == 1) {
        return;
    }
    if (player_stun > 0 || player_freeze > 0 || super_cd > 0) {
        return;
    }
    ball.x = player->x;
    ball.y = player->y;
    ball.vx = ball.aim_dx * BALL_SPEED;
    ball.vy = ball.aim_dy * BALL_SPEED;
    ball.bounces = 0;
    ball.t = 0;
    ball.spin = 0;
    ball.active = 1;
    super_cd = super_cd_time;
    ability_snd(STATION_SOUND);
    ds_log("brick thrown");
}

static void ball_burst(double damage) {
    ball.active = 0;
    boom_x = ball.x;
    boom_y = ball.y;
    boom_t = boom_time;
    if (damage <= 0) {
        return;
    }
    if (enemy_revive_prot <= 0) {
        enemy_apply_damage_at(BALL_DAMAGE, ball.x, ball.y);
    }
}

static void ball_tick(void) {
    double left, piece;
    if (ball.active == 0) {
        return;
    }
    ball.t = ball.t + dt;
    ball.spin = ball.spin + dt * 6;
    /* The move is split into small pieces: on a slow frame the ball would
     * otherwise fly straight through the enemy without ever touching it. */
    left = 1;
    while (left > 0 && ball.active == 1) {
        piece = left < 0.2 ? left : 0.2;
        left = left - piece;
        ball.x = ball.x + ball.vx * dt * piece;
        ball.y = ball.y + ball.vy * dt * piece;
        if (ball.x < BALL_RADIUS) {
            ball.x = BALL_RADIUS;
            ball.vx = 0 - ball.vx;
            ball.bounces = ball.bounces + 1;
        } else if (ball.x > screen_w - BALL_RADIUS) {
            ball.x = screen_w - BALL_RADIUS;
            ball.vx = 0 - ball.vx;
            ball.bounces = ball.bounces + 1;
        }
        if (ball.y < BALL_RADIUS) {
            ball.y = BALL_RADIUS;
            ball.vy = 0 - ball.vy;
            ball.bounces = ball.bounces + 1;
        } else if (ball.y > screen_h - BALL_RADIUS) {
            ball.y = screen_h - BALL_RADIUS;
            ball.vy = 0 - ball.vy;
            ball.bounces = ball.bounces + 1;
        }
        if (ball.bounces > BALL_MAX_BOUNCES || ball.t > BALL_MAX_TIME) {
            /* Out of bounces: it bursts where it is and nobody gets hurt. */
            ds_log("charged ult: ball used up after %g bounces", ball.bounces);
            ball_burst(0);
            return;
        }
        if (circle_hits_enemy(ball.x, ball.y, BALL_RADIUS) == 1) {
            ball_burst(1);
            return;
        }
    }
    ball_trail_push(ball.x, ball.y);
}

/* Aim preview: a dashed line from the cube the way the brick will go, and the
 * countdown digit over the head. */
static void draw_ball_aim(void) {
    double i = 1;
    double len = punch_reach * 2.4;
    if (ball.aim_on == 0) {
        return;
    }
    while (i <= 5) {
        double k = i / 6;
        circle(player->x + ball.aim_dx * len * k, player->y + ball.aim_dy * len * k, 3 + 4 * (1 - k),
               ((unsigned)(int)(150 + 90 * k) << 24) | 0x00FFD54F);
        i = i + 1;
    }
}

/* The digit of the countdown, drawn with the pads: the fight itself is drawn
 * under the colour correction, and a 3 lost under it is not a warning. */
static void draw_ball_count(void) {
    const char *digit;
    double d = ball_aim_digit();
    double sc = 1.7;
    if (ball.aim_on == 0) {
        return;
    }
    digit = d > 2 ? "3" : (d > 1 ? "2" : "1");
    text_scaled(digit, player->x - text_ink_width(digit) * sc / 2, player->y - 118,
                (unsigned)0xFFFFD54F, sc);
}

static void draw_ball(void) {
    double i = 0;
    double a;
    draw_ult_mode();
    draw_ball_aim();
    if (ball.aim_on == 1) {
        /* The pad is armed: the countdown is running and the aim can be pulled
         * around with the same finger. */
        ring(atk2_x, atk2_y, battle_pad_r() + 9, 5, 0xFFFFD54F);
    }
    while (i < BALL_TRAIL) {
        if (ball.trail_a[(int)i] > 0) {
            a = ball.trail_a[(int)i];
            circle(ball.trail_x[(int)i], ball.trail_y[(int)i], BALL_RADIUS * (0.12 + 0.45 * a),
                   ((unsigned)(int)(a * 170) << 24) | 0x00FFC640);
        }
        i = i + 1;
    }
    if (ball.active == 1) {
        /* A brick, not a ball: a spinning block of fired clay with a mortar
         * line across it, so the flight reads as something thrown. */
        ring(ball.x, ball.y, BALL_RADIUS * 1.25, 4, 0xCCFFD54F);
        rect_rot(ball.x - 23, ball.y - 14, 46, 28, ball.spin, 0xFFD2703F);
        rect_rot(ball.x - 23, ball.y - 8, 46, 2, ball.spin, 0xFFFFE9CC);
        rect_rot(ball.x - 23, ball.y + 6, 46, 2, ball.spin, 0xFFFFE9CC);
        rect_rot(ball.x - 8, ball.y - 14, 2, 8, ball.spin, 0xFFFFE9CC);
    }
}

/* Trail fade runs even between launches, so the dots left by the last ball do
 * not stay on screen forever. */
static void ball_fade_tick(void) {
    double i = 0;
    while (i < BALL_TRAIL) {
        if (ball.trail_a[(int)i] > 0) {
            ball.trail_a[(int)i] = ball.trail_a[(int)i] - dt / 0.45;
            if (ball.trail_a[(int)i] < 0) {
                ball.trail_a[(int)i] = 0;
            }
        }
        i = i + 1;
    }
}
