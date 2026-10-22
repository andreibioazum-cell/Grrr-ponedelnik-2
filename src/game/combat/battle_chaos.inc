/* "Masterpiece hitboxes": a joke mode switched on from the web panel
 * (root flag /HitboxChaos) or from the in-game admin panel. A punch hitbox
 * immediately teleports to a random place instead of following the player.
 * If a punch nevertheless lands, its ghost teleports away too. There is no
 * flying mascot, no captions and no smooth hitbox movement.
 *
 * The flag is read when a punch starts as well as once a frame. That first
 * read matters: a punch thrown on the same frame as the switch must not keep
 * its normal position. Any use marks the battle, so it cannot pay Cups. */

#define CHAOS_TURN_MIN 58.0      /* smallest turn away from the enemy, degrees */
#define CHAOS_TURN_MAX 88.0      /* stays away from the enemy, never turns back into it */
#define CHAOS_GHOST_TIME 0.65
#define CHAOS_SPARKS 8
#define CHAOS_POINT_TRIES 16

static double chaos_on;
static double chaos_escape_active;
static double chaos_escape_x, chaos_escape_y;
static double chaos_dir_x = 1, chaos_dir_y;
static double chaos_ghost_x, chaos_ghost_y, chaos_ghost_a;
static double chaos_ghost_dx, chaos_ghost_dy;
static double chaos_ghost_shot;          /* shot id of the last ghost */
static double chaos_spark_x[CHAOS_SPARKS], chaos_spark_y[CHAOS_SPARKS];
static double chaos_spark_vx[CHAOS_SPARKS], chaos_spark_vy[CHAOS_SPARKS];
static double chaos_spark_a[CHAOS_SPARKS];

static double chaos_rand(double lo, double hi) {
    if (hi <= lo) {
        return lo;
    }
    return lo + (rand() % 1000) / 1000.0 * (hi - lo);
}

static double chaos_refresh(void) {
    chaos_on = net_hitbox_chaos() == 1 || admin_cheat_chaos() == 1 ? 1 : 0;
    if (chaos_on == 1) {
        /* This includes the global web-panel switch. The mark survives after
         * the switch goes off, just like it does for every other cheat. */
        cheat_stain = 1;
    }
    return chaos_on;
}

/* Every counter back to the start: a fresh fight must not inherit an escaped
 * hitbox, a ghost or sparks from the previous one. */
static void chaos_reset(void) {
    double i = 0;
    chaos_on = 0;
    chaos_escape_active = 0;
    chaos_escape_x = 0;
    chaos_escape_y = 0;
    chaos_ghost_x = 0;
    chaos_ghost_y = 0;
    chaos_ghost_a = 0;
    chaos_ghost_shot = -1;
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
    chaos_escape_active = 0;
    chaos_ghost_a = 0;
    while (i < CHAOS_SPARKS) {
        chaos_spark_a[(int)i] = 0;
        i = i + 1;
    }
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

/* Pick a random arena point, preferring candidates away from both fighters.
 * The fallback is the farthest candidate rather than the player's position,
 * so even a small arena cannot silently turn the joke back into a normal
 * punch. */
static void chaos_pick_point(double *out_x, double *out_y) {
    double mx = punch_width * 0.5;
    double my = punch_width * 0.5;
    double best_x = screen_w * 0.5;
    double best_y = screen_h * 0.5;
    double best_score = -1;
    double i = 0;
    if (mx < 4) {
        mx = 4;
    }
    if (my < 4) {
        my = 4;
    }
    if (mx > screen_w * 0.5) {
        mx = screen_w * 0.5;
    }
    if (my > screen_h * 0.5) {
        my = screen_h * 0.5;
    }
    while (i < CHAOS_POINT_TRIES) {
        double x = chaos_rand(mx, screen_w - mx);
        double y = chaos_rand(my, screen_h - my);
        double player_distance = dist(x, y, player->x, player->y);
        double enemy_distance = dist(x, y, enemy->x, enemy->y);
        double score = player_distance < enemy_distance ? player_distance : enemy_distance;
        if (score > best_score) {
            best_x = x;
            best_y = y;
            best_score = score;
        }
        if (player_distance > punch_reach && enemy_distance > punch_reach + punch_width) {
            best_x = x;
            best_y = y;
            break;
        }
        i = i + 1;
    }
    *out_x = best_x;
    *out_y = best_y;
}

/* Every punch teleports, including every fifth one (which used to keep the
 * normal aim). Set the position here, not on the next frame, so no correctly
 * anchored hitbox can be drawn or published in between. */
static void chaos_punch_escape(void) {
    double ax, ay, len, away, turn;
    chaos_escape_active = 0;
    chaos_ghost_shot = -1;
    if (chaos_refresh() == 0) {
        return;
    }
    chaos_pick_point(&chaos_escape_x, &chaos_escape_y);
    punch->x = chaos_escape_x;
    punch->y = chaos_escape_y;

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
    chaos_escape_active = 1;
}

/* A landed punch leaves a ghost at a new random point immediately. The ghost
 * may fade, but its coordinates never slide across the arena. */
static void chaos_ghost_spawn(void) {
    chaos_pick_point(&chaos_ghost_x, &chaos_ghost_y);
    chaos_ghost_dx = punch->dx;
    chaos_ghost_dy = punch->dy;
    chaos_ghost_a = 1;
    chaos_spark(chaos_ghost_x, chaos_ghost_y);
}

static void chaos_ghost_tick(void) {
    if (chaos_ghost_a <= 0) {
        return;
    }
    chaos_ghost_a = chaos_ghost_a - dt / CHAOS_GHOST_TIME;
    if (chaos_ghost_a < 0) {
        chaos_ghost_a = 0;
    }
}

/* follow_punch() re-anchors the shared punch struct every frame. Reapply the
 * fixed teleported coordinates after it; do not integrate a velocity or an
 * offset. The punch's own lifetime is the only lifetime needed here, avoiding
 * a final frame where two independent timers disagree. */
static void chaos_escape_tick(void) {
    if (chaos_escape_active == 0 || punch->active == 0 || punch_left <= 0) {
        chaos_escape_active = 0;
        return;
    }
    punch->dx = chaos_dir_x;
    punch->dy = chaos_dir_y;
    punch->x = chaos_escape_x;
    punch->y = chaos_escape_y;
}

static void chaos_sparks_tick(void) {
    double i = 0;
    while (i < CHAOS_SPARKS) {
        if (chaos_spark_a[(int)i] > 0) {
            chaos_spark_x[(int)i] = chaos_spark_x[(int)i] + chaos_spark_vx[(int)i] * dt;
            chaos_spark_y[(int)i] = chaos_spark_y[(int)i] + chaos_spark_vy[(int)i] * dt;
            chaos_spark_a[(int)i] = chaos_spark_a[(int)i] - dt / 0.5;
            if (chaos_spark_a[(int)i] < 0) {
                chaos_spark_a[(int)i] = 0;
            }
        }
        i = i + 1;
    }
}

/* Called once per frame from the solo and online updates, after follow_punch. */
static void chaos_tick(void) {
    if (chaos_refresh() == 0) {
        chaos_hide();
        return;
    }
    /* Switching the mode on during an already active punch must teleport that
     * punch too, rather than leaving one ordinary hitbox until the next shot. */
    if (punch->active == 1 && punch_left > 0 && chaos_escape_active == 0) {
        chaos_punch_escape();
    }
    chaos_escape_tick();
    chaos_ghost_tick();
    chaos_sparks_tick();
    if (punch->active == 1 && punch->hit == 1 && chaos_ghost_shot != punch->shot) {
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
    if (chaos_escape_active == 1) {
        chaos_draw_box(punch->x, punch->y, punch->dx, punch->dy, 0x99FF3D7F);
    }
    chaos_draw_sparks();
}
