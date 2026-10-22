/* Charged ult: hold the ult pad for 0.6 s and the super turns into a bouncing
 * ball with a particle trail instead of the usual ability.
 *
 * The ball flies from the player towards the enemy, bounces off the arena
 * walls and keeps going until it touches the enemy. Ten bounces is the limit:
 * after that it just bursts without doing any damage. A quick tap still fires
 * the normal super - only a real hold swaps it.
 *
 * Solo only: an online fight keeps the plain super on tap, because the ball
 * would have to be synced to the other players and that is not worth it for a
 * joke that fits a single-player fight. */

#define BALL_HOLD_TIME 0.6     /* how long the pad must be held, seconds */
#define BALL_SPEED 620.0       /* px per second */
#define BALL_RADIUS 24.0
#define BALL_DAMAGE 6.0
#define BALL_MAX_BOUNCES 10.0
#define BALL_MAX_TIME 8.0      /* safety net: the ball always goes away */
#define BALL_TRAIL 12

typedef struct {
    double active;
    double x, y, vx, vy;
    double bounces, t;
    double hold_id, hold_t, charged;
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
    ball.hold_id = -1;
    ball.hold_t = 0;
    ball.charged = 0;
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

/* The pad was pressed: start counting the hold. A second finger on the pad
 * while one is already holding it is ignored. */
static void ball_hold_start(double id) {
    if (ball.hold_id != -1) {
        return;
    }
    ball.hold_id = id;
    ball.hold_t = 0;
    ball.charged = 0;
}

/* Held long enough: the ult is now the ball. Shown by the pad ring. */
static void ball_hold_tick(void) {
    if (ball.hold_id == -1) {
        return;
    }
    ball.hold_t = ball.hold_t + dt;
    if (ball.hold_t >= BALL_HOLD_TIME) {
        ball.charged = 1;
    }
}

/* Finger up: fire whatever was charged. Cancelled means the gesture was taken
 * away (a call came in, the app went away) - then nothing fires. */
static void ball_hold_release(double cancelled) {
    if (ball.hold_id == -1) {
        return;
    }
    double charged = ball.charged;
    ball.hold_id = -1;
    ball.hold_t = 0;
    ball.charged = 0;
    if (cancelled == 1) {
        return;
    }
    if (charged == 1 && game_state == ST_SOLO) {
        ball_fire();
        return;
    }
    fire_super();
}

/* The ball mode of Ebuc is a plain ult: one tap, no holding. */
static void ball_tap_fire(void) {
    if (ball_mode != 1) {
        return;
    }
    ball_fire();
}

static void ball_fire(void) {
    double dx, dy, len;
    if (finished != 0 || ball.active == 1 || player_block == 1 || player_grabbed() == 1) {
        return;
    }
    if (player_stun > 0 || player_freeze > 0 || super_cd > 0) {
        return;
    }
    dx = enemy->x - player->x;
    dy = enemy->y - player->y;
    len = sqrt(dx * dx + dy * dy);
    if (len < 1) {
        dx = cos(player->angle);
        dy = sin(player->angle);
        len = 1;
    }
    ball.x = player->x;
    ball.y = player->y;
    ball.vx = dx / len * BALL_SPEED;
    ball.vy = dy / len * BALL_SPEED;
    ball.bounces = 0;
    ball.t = 0;
    ball.active = 1;
    super_cd = super_cd_time;
    ability_snd(STATION_SOUND);
    ds_log("charged ult: ball away");
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

static void draw_ball(void) {
    double i = 0;
    double a;
    draw_ult_mode();
    if (ball.hold_id != -1) {
        /* The pad shows the charge: a ring grows around it while held. */
        double k = clamp(ball.hold_t / BALL_HOLD_TIME, 0, 1);
        double r = battle_pad_r() * (1.05 + 0.35 * k);
        ring(atk2_x, atk2_y, r, 5, ball.charged == 1 ? 0xFFFFD54F : 0x99FFD54F);
        if (ball.charged == 1) {
            circle(atk2_x, atk2_y, battle_pad_r() * 1.02, 0x33FFD54F);
        }
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
        ring(ball.x, ball.y, BALL_RADIUS * 1.25, 4, 0xCCFFD54F);
        circle(ball.x, ball.y, BALL_RADIUS, 0xFFFFB300);
        circle(ball.x - BALL_RADIUS * 0.3, ball.y - BALL_RADIUS * 0.3, BALL_RADIUS * 0.3, 0xFFFFFFFF);
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
