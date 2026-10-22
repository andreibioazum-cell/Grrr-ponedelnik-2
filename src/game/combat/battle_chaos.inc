/* "Masterpiece hitboxes": a joke mode an admin switches on in the web panel
 * (root flag /HitboxChaos). Three things happen in a solo fight, and none of
 * them touch health, cups or the network:
 *   - your punch hitbox turns away from the enemy and runs off, so the punch
 *     almost always whiffs;
 *   - when one does land, the hitbox tears itself off the enemy and flies away;
 *   - a "masterpiece enemy" - a spinning cube with eyes - flies around the
 *     arena, bounces, teleports, leaves sparks and shouts nonsense.
 * The flag is polled once a frame through net_hitbox_chaos(); off means every
 * function here returns at once. */

#define CHAOS_FLEE_SPEED 640.0   /* px/s the escaped hitbox runs */
#define CHAOS_TURN_MIN 58.0      /* smallest turn away from the enemy, degrees */
#define CHAOS_TURN_MAX 92.0      /* biggest one: still never into the enemy */
#define CHAOS_GHOST_SPEED 980.0  /* px/s the box flies off after a hit */
#define CHAOS_GHOST_TIME 0.65
#define CHAOS_FOE_SIZE 34.0
#define CHAOS_FOE_SPEED 280.0
#define CHAOS_SPARKS 8

/* Colours are 0xAARRGGBB, the same packing the rest of the game uses. */
static const uint32_t chaos_foe_rgb[6] = {0xFFFF4081, 0xFF4FC3F7, 0xFFFFD54F,
                                          0xFF69F0AE, 0xFFB388FF, 0xFFFF8A65};

static double chaos_on;
static double chaos_flee_t;              /* time left of the run-away punch */
static double chaos_off_x, chaos_off_y;  /* how far the hitbox ran from the player */
static double chaos_dir_x = 1, chaos_dir_y;  /* where it ran to */
static double chaos_ghost_x, chaos_ghost_y, chaos_ghost_a;
static double chaos_ghost_dx, chaos_ghost_dy, chaos_ghost_vx, chaos_ghost_vy;
static double chaos_ghost_shot;          /* shot id of the last stay */
static double chaos_foe_x, chaos_foe_y, chaos_foe_vx, chaos_foe_vy;
static double chaos_foe_turn, chaos_foe_tele, chaos_foe_spin, chaos_foe_clock;
static double chaos_foe_say_t, chaos_foe_say, chaos_foe_a;
static double chaos_spark_x[CHAOS_SPARKS], chaos_spark_y[CHAOS_SPARKS];
static double chaos_spark_vx[CHAOS_SPARKS], chaos_spark_vy[CHAOS_SPARKS];
static double chaos_spark_a[CHAOS_SPARKS];

static double chaos_rand(double lo, double hi) {
    return lo + (rand() % 1000) / 1000.0 * (hi - lo);
}

/* Every counter back to the start: a fresh fight must not inherit the old
 * runaway hitbox or a foe frozen mid-flight. */
static void chaos_reset(void) {
    double i = 0;
    chaos_on = 0;
    chaos_flee_t = 0;
    chaos_off_x = 0;
    chaos_off_y = 0;
    chaos_ghost_a = 0;
    chaos_ghost_shot = -1;
    chaos_foe_x = screen_w * 0.5;
    chaos_foe_y = screen_h * 0.3;
    chaos_foe_vx = CHAOS_FOE_SPEED * 0.6;
    chaos_foe_vy = CHAOS_FOE_SPEED * 0.4;
    chaos_foe_turn = 0.4;
    chaos_foe_tele = 4.0;
    chaos_foe_spin = 0;
    chaos_foe_clock = 0;
    chaos_foe_say_t = 1.2;
    chaos_foe_say = 0;
    chaos_foe_a = 0;
    while (i < CHAOS_SPARKS) {
        chaos_spark_a[(int)i] = 0;
        chaos_spark_x[(int)i] = 0;
        chaos_spark_y[(int)i] = 0;
        chaos_spark_vx[(int)i] = 0;
        chaos_spark_vy[(int)i] = 0;
        i = i + 1;
    }
}

/* Off switch: everything that was on screen simply disappears. */
static void chaos_hide(void) {
    double i = 0;
    chaos_flee_t = 0;
    chaos_off_x = 0;
    chaos_off_y = 0;
    chaos_ghost_a = 0;
    chaos_foe_a = 0;
    while (i < CHAOS_SPARKS) {
        chaos_spark_a[(int)i] = 0;
        i = i + 1;
    }
}

static const char *chaos_phrase(double n) {
    if (language == 1) {
        if (n == 1) {
            return "я шедевро!";
        }
        if (n == 2) {
            return "хитбокс, беги!";
        }
        if (n == 3) {
            return "я тут просто летаю";
        }
        if (n == 4) {
            return "фигня вышла";
        }
        return "лови меня!";
    }
    if (n == 1) {
        return "I am a masterpiece!";
    }
    if (n == 2) {
        return "run, hitbox, run!";
    }
    if (n == 3) {
        return "just flying about";
    }
    if (n == 4) {
        return "that went wrong";
    }
    return "catch me!";
}

static void chaos_spark(double x, double y) {
    double i = 0;
    while (i < CHAOS_SPARKS) {
        if (chaos_spark_a[(int)i] <= 0) {
            chaos_spark_x[(int)i] = x;
            chaos_spark_y[(int)i] = y;
            chaos_spark_vx[(int)i] = chaos_rand(-260, 260);
            chaos_spark_vy[(int)i] = chaos_rand(-260, 260);
            chaos_spark_a[(int)i] = 1;
            return;
        }
        i = i + 1;
    }
}

/* A punch starts: four punches out of five turn the hitbox away from the enemy
 * and let it run. Every fifth one keeps the aim, and if it lands the box tears
 * off the enemy afterwards - that is the joke the panel turns on. The count
 * goes by the punch number, not by a die roll, so the mode plays the same way
 * every time. */
static void chaos_punch_escape(void) {
    double ax, ay, len, away, turn;
    if (chaos_on == 0) {
        return;
    }
    chaos_flee_t = 0;
    chaos_off_x = 0;
    chaos_off_y = 0;
    chaos_ghost_shot = -1;
    if (punch->shot > 0 && fmod(punch->shot, 5) == 0) {
        return;
    }
    ax = punch->x - enemy->x;
    ay = punch->y - enemy->y;
    len = sqrt(ax * ax + ay * ay);
    away = len < 1 ? chaos_rand(-pi, pi) : atan2(ay, ax);
    turn = chaos_rand(CHAOS_TURN_MIN, CHAOS_TURN_MAX) * pi / 180;
    if (rand() % 2 == 0) {
        turn = 0 - turn;
    }
    chaos_dir_x = cos(away + turn);
    chaos_dir_y = sin(away + turn);
    punch->dx = chaos_dir_x;
    punch->dy = chaos_dir_y;
    chaos_flee_t = punch_time;
}

static void chaos_ghost_spawn(void) {
    double ax = punch->x - enemy->x;
    double ay = punch->y - enemy->y;
    double len = sqrt(ax * ax + ay * ay);
    if (len < 1) {
        ax = cos(punch->dx);
        ay = sin(punch->dy);
        len = 1;
    }
    chaos_ghost_x = punch->x;
    chaos_ghost_y = punch->y;
    chaos_ghost_dx = punch->dx;
    chaos_ghost_dy = punch->dy;
    chaos_ghost_vx = ax / len * CHAOS_GHOST_SPEED;
    chaos_ghost_vy = ay / len * CHAOS_GHOST_SPEED;
    chaos_ghost_a = 1;
    chaos_spark(chaos_ghost_x, chaos_ghost_y);
}

/* The box that left: it slides off the enemy and fades out. */
static void chaos_ghost_tick(void) {
    if (chaos_ghost_a <= 0) {
        return;
    }
    chaos_ghost_x = chaos_ghost_x + chaos_ghost_vx * dt;
    chaos_ghost_y = chaos_ghost_y + chaos_ghost_vy * dt;
    chaos_ghost_a = chaos_ghost_a - dt / CHAOS_GHOST_TIME;
    if (chaos_ghost_a < 0) {
        chaos_ghost_a = 0;
    }
}

/* Runs the punched hitbox away from the player while the punch is alive. The
 * game re-anchors the punch to the player every frame, so the escape is an
 * offset of our own rather than a velocity on the shared struct. */
static void chaos_flee_tick(void) {
    if (punch->active == 0 || chaos_flee_t <= 0) {
        chaos_flee_t = 0;
        chaos_off_x = 0;
        chaos_off_y = 0;
        return;
    }
    chaos_flee_t = chaos_flee_t - dt;
    chaos_off_x = chaos_off_x + chaos_dir_x * CHAOS_FLEE_SPEED * dt;
    chaos_off_y = chaos_off_y + chaos_dir_y * CHAOS_FLEE_SPEED * dt;
    /* follow_punch() has just pointed the box at the player's aim again; the
     * escaped box keeps its own direction, and that is why it never hits. */
    punch->dx = chaos_dir_x;
    punch->dy = chaos_dir_y;
    punch->x = clamp(player->x + chaos_off_x, punch_width, screen_w - punch_width);
    punch->y = clamp(player->y + chaos_off_y, punch_width, screen_h - punch_width);
}

static void chaos_foe_tick(void) {
    double r = CHAOS_FOE_SIZE;
    chaos_foe_a = chaos_foe_a + dt * 3;
    if (chaos_foe_a > 1) {
        chaos_foe_a = 1;
    }
    chaos_foe_clock = chaos_foe_clock + dt;
    chaos_foe_spin = chaos_foe_spin + dt * 5;
    chaos_foe_turn = chaos_foe_turn - dt;
    if (chaos_foe_turn <= 0) {
        /* A new fit of nonsense: a random shove, sometimes straight up for no
         * reason at all. */
        chaos_foe_turn = chaos_rand(0.3, 0.9);
        chaos_foe_vx = chaos_rand(-CHAOS_FOE_SPEED, CHAOS_FOE_SPEED);
        chaos_foe_vy = chaos_rand(-CHAOS_FOE_SPEED, CHAOS_FOE_SPEED);
        if (rand() % 3 == 0) {
            chaos_spark(chaos_foe_x, chaos_foe_y);
        }
    }
    chaos_foe_x = chaos_foe_x + chaos_foe_vx * dt;
    chaos_foe_y = chaos_foe_y + chaos_foe_vy * dt;
    if (chaos_foe_x < r || chaos_foe_x > screen_w - r) {
        chaos_foe_vx = 0 - chaos_foe_vx;
        chaos_foe_x = clamp(chaos_foe_x, r, screen_w - r);
    }
    if (chaos_foe_y < r || chaos_foe_y > screen_h - r) {
        chaos_foe_vy = 0 - chaos_foe_vy;
        chaos_foe_y = clamp(chaos_foe_y, r, screen_h - r);
    }
    chaos_foe_tele = chaos_foe_tele - dt;
    if (chaos_foe_tele <= 0) {
        /* Teleporting across the arena is part of the nonsense. */
        chaos_foe_tele = chaos_rand(3.0, 6.0);
        chaos_spark(chaos_foe_x, chaos_foe_y);
        chaos_foe_x = chaos_rand(r * 2, screen_w - r * 2);
        chaos_foe_y = chaos_rand(r * 2, screen_h * 0.7);
        chaos_spark(chaos_foe_x, chaos_foe_y);
    }
    chaos_foe_say_t = chaos_foe_say_t - dt;
    if (chaos_foe_say_t <= 0) {
        chaos_foe_say_t = 2.4;
        chaos_foe_say = rand() % 5;
    }
    double i = 0;
    while (i < CHAOS_SPARKS) {
        if (chaos_spark_a[(int)i] > 0) {
            chaos_spark_x[(int)i] = chaos_spark_x[(int)i] + chaos_spark_vx[(int)i] * dt;
            chaos_spark_y[(int)i] = chaos_spark_y[(int)i] + chaos_spark_vy[(int)i] * dt;
            chaos_spark_a[(int)i] = chaos_spark_a[(int)i] - dt / 0.5;
        }
        i = i + 1;
    }
}

/* Called once per frame from update_game, right after follow_punch(). */
static void chaos_tick(void) {
    /* Either the web panel flag or the switch in the in-game panel. */
    chaos_on = net_hitbox_chaos() == 1 || admin_cheat_chaos() == 1 ? 1 : 0;
    if (chaos_on == 0) {
        chaos_hide();
        return;
    }
    chaos_flee_tick();
    chaos_ghost_tick();
    chaos_foe_tick();
    if (punch->active == 1 && punch->hit == 1 && chaos_ghost_shot != punch->shot) {
        /* It did land - and the hitbox runs for its life. */
        chaos_ghost_shot = punch->shot;
        chaos_ghost_spawn();
    }
}

static void chaos_draw_box(double x, double y, double dx, double dy, uint32_t colour) {
    double mid = punch_forward_offset + punch_reach / 2;
    rect_rot(x + dx * mid - punch_reach / 2, y + dy * mid - punch_width / 2, punch_reach, punch_width,
             atan2(dy, dx), colour);
}

static void chaos_draw_sparks(void) {
    double i = 0;
    while (i < CHAOS_SPARKS) {
        if (chaos_spark_a[(int)i] > 0) {
            double a = clamp(chaos_spark_a[(int)i], 0, 1) * 235;
            double r = 3 + 5 * chaos_spark_a[(int)i];
            circle(chaos_spark_x[(int)i], chaos_spark_y[(int)i], r, ((unsigned)(int)a << 24) | 0x00FFC107);
        }
        i = i + 1;
    }
}

static void chaos_draw_foe(void) {
    double s = CHAOS_FOE_SIZE * (1 + 0.16 * sin(chaos_foe_clock * 7));
    double a = clamp(chaos_foe_a, 0, 1);
    uint32_t c = chaos_foe_rgb[((int)floor(chaos_foe_clock * 2)) % 6];
    double lx = cos(chaos_foe_spin) * s * 0.22;
    double ly = sin(chaos_foe_spin) * s * 0.22;
    if (a <= 0) {
        return;
    }
    ring(chaos_foe_x, chaos_foe_y, s * 1.3, 4, ((unsigned)(int)(a * 120) << 24) | (c & 0x00FFFFFF));
    rect_rot(chaos_foe_x - s, chaos_foe_y - s, 2 * s, 2 * s, chaos_foe_spin, c);
    circle(chaos_foe_x - s * 0.3 + lx, chaos_foe_y - s * 0.2 + ly, s * 0.2, ((unsigned)(int)(a * 255) << 24) | 0x00FFFFFF);
    circle(chaos_foe_x + s * 0.3 + lx, chaos_foe_y - s * 0.2 + ly, s * 0.2, ((unsigned)(int)(a * 255) << 24) | 0x00FFFFFF);
    circle(chaos_foe_x - s * 0.3 + lx * 1.6, chaos_foe_y - s * 0.2 + ly * 1.6, s * 0.09, ((unsigned)(int)(a * 255) << 24) | 0x00101010);
    circle(chaos_foe_x + s * 0.3 + lx * 1.6, chaos_foe_y - s * 0.2 + ly * 1.6, s * 0.09, ((unsigned)(int)(a * 255) << 24) | 0x00101010);
    ctext_scaled(chaos_phrase(chaos_foe_say), chaos_foe_y - s * 2.4, ((unsigned)(int)(a * 255) << 24) | 0x00FFFFFF, 0.55);
}

/* Whole layer of the joke mode, drawn under the fighters. */
static void chaos_draw(void) {
    if (chaos_on == 0) {
        return;
    }
    if (chaos_ghost_a > 0) {
        double ga = clamp(chaos_ghost_a, 0, 1) * 190;
        chaos_draw_box(chaos_ghost_x, chaos_ghost_y, chaos_ghost_dx, chaos_ghost_dy,
                       ((unsigned)(int)ga << 24) | 0x00FF3D7F);
    }
    if (chaos_flee_t > 0) {
        chaos_draw_box(punch->x, punch->y, punch->dx, punch->dy, 0x99FF3D7F);
    }
    chaos_draw_sparks();
    chaos_draw_foe();
}
