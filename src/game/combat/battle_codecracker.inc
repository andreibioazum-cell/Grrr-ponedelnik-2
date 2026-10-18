/* CodeCracker is a separate, administrator-only class (ID 6), gated by the
 * very same administrator check as Astra: it is visible and
 * selectable only for the admin accounts, and that permission is re-checked
 * whenever the account or the battle changes, so a copied progress file
 * cannot grant it. Online it travels as its own class ID (see the room-slot
 * validation in firebase.rules.json), so every other client sees the real
 * class and applies the same rules.
 *
 * CodeCracker looks like an ordinary cube and plays like one - normal
 * speed, normal punch - except for its small codecracker_hp (12) pool. Its
 * ultimate is a 7-second overclock (codecracker_super_time): about three
 * times the speed (codecracker_speed_mult in player_walk_speed()), half of
 * all incoming damage absorbed (in take_damage()), and a body-check bite:
 * touching an enemy heals CodeCracker by codecracker_touch_heal and bites
 * the enemy for codecracker_touch_damage. The bite only exists while the
 * overclock runs, on both sides of an online match: the attacker's client
 * heals itself, the victim's client takes the bite through
 * take_damage_at(), and the running overclock is published in the room
 * slot (field "ccu", net_publish_codecracker_super) because the victim has
 * to know the ram is live.
 *
 * The touch is resolved on both sides of an online match from the synced
 * positions: the CodeCracker client heals itself, and the victim's client
 * takes the bite through take_damage_at(), which is exactly where the
 * raised block cuts frontal damage - so blocking a CodeCracker works
 * online the same way it works against a punch. A short per-enemy cooldown
 * (codecracker_touch_cd, one timer per room slot plus one for the solo bot)
 * keeps a lasting overlap from firing every frame: one touch, one bite. */

static const double codecracker_tint_rgb = 0x00E676;
static const double codecracker_tint_a = 0.35;

/* Cached permission bit, refreshed together with the developer permission when the
 * account changes (login, profile load, battle start), never per frame. */
static void codecracker_refresh(void) { codecracker_ok = astra_ability_is_dev(); }

static double codecracker_allowed(void) { return codecracker_ok; }

static double codecracker_on(void) { return player_class == CLASS_CODECRACKER && codecracker_allowed() == 1; }

static double codecracker_of_slot(double slot) {
    return arr_get(remotes, slot * remote_fields + 10) == CLASS_CODECRACKER;
}

static double codecracker_touching(double x, double y) {
    return dist(player->x, player->y, x, y) < player->size * 2;
}

static void codecracker_heal(void) {
    player->hp = clamp(player->hp + codecracker_touch_heal, 0, player->max_hp);
}

/* Solo: the bot is the only enemy, so one shared timer is enough. */
static double codecracker_super_active(void) {
    return codecracker_on() == 1 && codecracker_super_t > 0;
}

static void codecracker_tick_solo(void) {
    codecracker_super_t = tick_down(codecracker_super_t);
    codecracker_touch_cd_left = tick_down(codecracker_touch_cd_left);
    if (codecracker_super_active() == 0 || finished != 0 || enemy->hp <= 0) {
        return;
    }
    if (codecracker_touch_cd_left > 0 || codecracker_touching(enemy->x, enemy->y) == 0) {
        return;
    }
    codecracker_touch_cd_left = codecracker_touch_cd;
    codecracker_heal();
    enemy_apply_damage_at(codecracker_touch_damage, player->x, player->y);
}

/* Online: slot i's outbound timer (i) heals me when I am the CodeCracker,
 * slot i's inbound timer (max_players + i) bites me when slot i is one.
 * Both clients see the same synced positions, so the heal and the damage
 * land on the same touch without any extra network field. */
static void codecracker_tick_online(void) {
    codecracker_super_t = tick_down(codecracker_super_t);
    double s = 0;
    while (s < max_players) {
        double out_i = s;
        double in_i = max_players + s;
        arr_set(codecracker_touch_cds, out_i, tick_down(arr_get(codecracker_touch_cds, out_i)));
        arr_set(codecracker_touch_cds, in_i, tick_down(arr_get(codecracker_touch_cds, in_i)));
        s = s + 1;
    }
    if (finished != 0 || player->hp <= 0) {
        return;
    }
    s = 0;
    while (s < max_players) {
        double b = s * remote_fields;
        if (s != net_slot() && arr_get(remotes, b + 5) == 1 &&
            codecracker_touching(arr_get(remotes, b + 1), arr_get(remotes, b + 2)) == 1) {
            if (codecracker_super_active() == 1 && arr_get(codecracker_touch_cds, s) <= 0) {
                arr_set(codecracker_touch_cds, s, codecracker_touch_cd);
                codecracker_heal();
            }
            if (codecracker_of_slot(s) == 1 && net_player_codecracker_super(s) == 1 &&
                arr_get(codecracker_touch_cds, max_players + s) <= 0) {
                arr_set(codecracker_touch_cds, max_players + s, codecracker_touch_cd);
                /* Through take_damage_at, so a raised block facing the
                 * CodeCracker cuts the bite online just like a punch. */
                if (take_damage_at(codecracker_touch_damage, arr_get(remotes, b + 1),
                                   arr_get(remotes, b + 2)) == 1) {
                    return;
                }
            }
        }
        s = s + 1;
    }
}

static double codecracker_tint(void) {
    return floor(codecracker_tint_a * 255) * 0x1000000 + codecracker_tint_rgb;
}

static double codecracker_tint_local(void) {
    if (codecracker_on() == 1) {
        return codecracker_tint();
    }
    return 0;
}

static double codecracker_tint_slot(double slot) {
    if (codecracker_of_slot(slot) == 1) {
        return codecracker_tint();
    }
    return 0;
}

/* Overclock FX: a cool-blue particle stream and rotating energy arcs. It is
 * deterministic instead of random, so every frame is smooth and the same
 * effect can be drawn for a remote CodeCracker without syncing particles. */
static double codecracker_fx_color(double alpha, double rgb) {
    double a = clamp(alpha, 0, 255);
    return (((unsigned)(int)a) << 24) | ((unsigned)(int)rgb);
}

static void codecracker_fx_arc(double cx, double cy, double radius, double start, double sweep,
                               double alpha, double rgb) {
    double i = 0;
    double steps = 9;
    while (i < steps) {
        double p0 = i / steps;
        double p1 = (i + 1) / steps;
        double a0 = start + sweep * p0;
        double a1 = start + sweep * p1;
        double aa = alpha * (0.32 + 0.68 * sin(p0 * pi));
        line(cx + cos(a0) * radius, cy + sin(a0) * radius,
             cx + cos(a1) * radius, cy + sin(a1) * radius, 2.2, codecracker_fx_color(aa, rgb));
        i = i + 1;
    }
}

static void draw_codecracker_fx_at(double cx, double cy, double angle, double phase) {
    double dx = cos(angle);
    double dy = sin(angle);
    double pulse = 0.5 + 0.5 * sin(phase * 8.5);
    double spin = phase * 3.7;
    double blue = 0x0068D8FF;
    double cyan = 0x0000E5FF;
    double ice = 0x00C8F7FF;

    /* No halo disc or ring around the cube (removed on request): the
     * rotating arcs, sparks and orbiting motes carry the effect alone. */
    codecracker_fx_arc(cx, cy, 43 + pulse * 4, spin, 1.55, 190, ice);
    codecracker_fx_arc(cx, cy, 43 + pulse * 4, spin + 2.1, 1.0, 145, cyan);
    codecracker_fx_arc(cx, cy, 37 - pulse * 2, 0 - spin * 1.35, 1.2, 125, blue);

    /* Fast sparks stream out behind the direction the cube is facing. */
    double i = 0;
    while (i < 24) {
        double h1 = hash01(i * 37 + 17);
        double h2 = hash01(i * 61 + 29);
        double h3 = hash01(i * 83 + 43);
        double h4 = hash01(i * 107 + 59);
        double cycle = phase * (1.25 + h1 * 0.8) + h2;
        cycle = cycle - floor(cycle);
        double fade = sin(cycle * pi);
        double back = 25 + cycle * (92 + h3 * 62);
        double side = sin(cycle * pi * (2.0 + h1) + h4 * 2 * pi) * (5 + h2 * 15);
        double px = cx - dx * back - dy * side;
        double py = cy - dy * back + dx * side;
        double tail = 6 + h3 * 15;
        double alpha = fade * (125 + 90 * h4);
        double size = 1.4 + h2 * 2.7 + pulse * 0.7;
        double rgb = h1 > 0.46 ? cyan : blue;
        if (alpha > 1) {
            line(px + dx * 3, py + dy * 3, px - dx * tail, py - dy * tail,
                 1.1 + h2 * 1.8, codecracker_fx_color(alpha * 0.62, rgb));
            circle(px, py, size * 2.4, codecracker_fx_color(alpha * 0.18, ice));
            circle(px, py, size, codecracker_fx_color(alpha, rgb));
            if (h4 > 0.62) {
                circle(px, py, size * 0.38, codecracker_fx_color(alpha + 70, 0x00FFFFFF));
            }
        }
        i = i + 1;
    }

    /* Eight orbiting motes give the aura a jewel-like rim instead of a flat
     * circle. Their tangent streaks make the acceleration read immediately. */
    i = 0;
    while (i < 8) {
        double h = hash01(i * 47 + 101);
        double a = spin * (0.75 + h * 0.18) + i * (2 * pi / 8);
        double r = 34 + 7 * sin(phase * 5 + h * 2 * pi);
        double px = cx + cos(a) * r;
        double py = cy + sin(a) * r;
        double tx = -sin(a) * (7 + h * 8);
        double ty = cos(a) * (7 + h * 8);
        double alpha = 135 + 80 * (0.5 + 0.5 * sin(phase * 7 + h * 6));
        line(px - tx, py - ty, px + tx, py + ty, 1.5, codecracker_fx_color(alpha * 0.72, cyan));
        circle(px, py, 2.2 + h * 1.8, codecracker_fx_color(alpha, ice));
        i = i + 1;
    }
}

static void draw_codecracker_fx_local(void) {
    if (codecracker_super_active() == 0) {
        return;
    }
    double elapsed = codecracker_super_time - codecracker_super_t;
    draw_codecracker_fx_at(player->x, player->y, player->angle, elapsed);
}

static void draw_codecracker_fx_slot(double slot, double x, double y, double angle) {
    if (codecracker_of_slot(slot) == 0 || net_player_codecracker_super(slot) == 0) {
        return;
    }
    draw_codecracker_fx_at(x, y, angle, event_t + slot * 0.31);
}
