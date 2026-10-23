/* "Masterpiece hitboxes": a joke mode an admin switches on in the web panel
 * (root flag /HitboxChaos). When it is on:
 *   - your punch hitbox instantly teleports to a random place, not smoothly
 *     flying away;
 *   - when a hit does land, its ghost also teleports to a random place;
 * No flying foe, no phrases, no sparks. The flag is polled once a frame
 * through net_hitbox_chaos(); off means every function here returns at once. */

#define CHAOS_GHOST_TIME 0.65

static double chaos_on;
static double chaos_flee_t;
static double chaos_off_x, chaos_off_y;
static double chaos_dir_x = 1, chaos_dir_y;
static double chaos_ghost_x, chaos_ghost_y, chaos_ghost_a;
static double chaos_ghost_dx, chaos_ghost_dy;
static double chaos_ghost_shot;

static double chaos_rand(double lo, double hi) {
    return lo + (rand() % 1000) / 1000.0 * (hi - lo);
}

static void chaos_reset(void) {
    chaos_on = 0;
    chaos_flee_t = 0;
    chaos_off_x = 0;
    chaos_off_y = 0;
    chaos_ghost_a = 0;
    chaos_ghost_shot = -1;
}

static void chaos_hide(void) {
    chaos_flee_t = 0;
    chaos_off_x = 0;
    chaos_off_y = 0;
    chaos_ghost_a = 0;
}

/* Every punch instantly teleports the hitbox to a random place, not smoothly
 * flying away. No 1-in-5 exception – previously every 5th stayed, which was the
 * “sometimes appears where needed” bug. */
static void chaos_punch_escape(void) {
    if (chaos_on == 0) {
        return;
    }
    chaos_flee_t = 0;
    chaos_off_x = 0;
    chaos_off_y = 0;
    chaos_ghost_shot = -1;
    /* Instant random teleport, not a smooth flee. */
    double rx = chaos_rand(punch_width + 8, screen_w - punch_width - 8);
    double ry = chaos_rand(punch_width + 8, screen_h - punch_width - 8);
    double ang = chaos_rand(-pi, pi);
    chaos_dir_x = cos(ang);
    chaos_dir_y = sin(ang);
    punch->dx = chaos_dir_x;
    punch->dy = chaos_dir_y;
    chaos_off_x = rx - player->x;
    chaos_off_y = ry - player->y;
    punch->x = clamp(player->x + chaos_off_x, punch_width, screen_w - punch_width);
    punch->y = clamp(player->y + chaos_off_y, punch_width, screen_h - punch_width);
    chaos_flee_t = punch_time;
}

static void chaos_ghost_spawn(void) {
    /* Ghost also instantly appears at a random place, not flying off smoothly. */
    chaos_ghost_x = chaos_rand(punch_width + 8, screen_w - punch_width - 8);
    chaos_ghost_y = chaos_rand(punch_width + 8, screen_h - punch_width - 8);
    double ang = chaos_rand(-pi, pi);
    chaos_ghost_dx = cos(ang);
    chaos_ghost_dy = sin(ang);
    chaos_ghost_a = 1;
}

static void chaos_ghost_tick(void) {
    if (chaos_ghost_a <= 0) {
        return;
    }
    /* No movement – just fade. Previously it slid with velocity. */
    chaos_ghost_a = chaos_ghost_a - dt / CHAOS_GHOST_TIME;
    if (chaos_ghost_a < 0) {
        chaos_ghost_a = 0;
    }
}

static void chaos_flee_tick(void) {
    if (punch->active == 0 || chaos_flee_t <= 0) {
        chaos_flee_t = 0;
        chaos_off_x = 0;
        chaos_off_y = 0;
        return;
    }
    chaos_flee_t = chaos_flee_t - dt;
    /* Keep the already teleported random offset – no incremental movement. */
    punch->dx = chaos_dir_x;
    punch->dy = chaos_dir_y;
    punch->x = clamp(player->x + chaos_off_x, punch_width, screen_w - punch_width);
    punch->y = clamp(player->y + chaos_off_y, punch_width, screen_h - punch_width);
}

static void chaos_tick(void) {
    chaos_on = net_hitbox_chaos() == 1 || admin_cheat_chaos() == 1 ? 1 : 0;
    if (chaos_on == 0) {
        chaos_hide();
        return;
    }
    chaos_flee_tick();
    chaos_ghost_tick();
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
}
