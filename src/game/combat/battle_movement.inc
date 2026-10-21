/* Walking speed with every bonus and slow applied - the same number move_player
 * moves by, so anything that has to beat a walk (Astra's grab lunge) can read
 * it. */
static double player_walk_speed(void) {
    double spd = joy_speed;
    if (player_class == CLASS_AZUM && player_level >= 2) {
        spd = spd * (1 + azum_speed_bonus);
    } else if (player_class == CLASS_ORDINARY && player_level >= 2) {
        spd = spd * (1 + ordinary_speed_bonus);
    }
    if (player_class == CLASS_CODECRACKER && codecracker_super_t > 0) {
        /* The overclock ultimate: about three times the normal pace. */
        spd = spd * codecracker_speed_mult;
    }
    spd = spd * admin_cheat_speed_mult();
    if (player_freeze > 0) {
        spd = spd * player_freeze_slow;
    }
    if (player_block == 1) {
        /* Holding the guard plants the feet: heavy movement, no dashing. */
        spd = spd * block_speed_k;
    }
    return spd;
}

/* Кубик не переключает скорость мгновенно: стик (или WASD) задаёт желаемую
 * скорость, а настоящая подтягивается к ней. Разгон до полного хода занимает
 * walk_accel_time, а торможение с полного хода до нуля - walk_stop_time
 * (около 0.09 с), поэтому отпущенный стик даёт короткий выкат, а не стоп. */
static void player_walk_move(double spd) {
    double tvx = joy.dx * spd;
    double tvy = joy.dy * spd;
    double want = sqrt(tvx * tvx + tvy * tvy);
    double have = sqrt(player_vx * player_vx + player_vy * player_vy);
    double dvx, dvy, dl, step, rate;
    if (want >= have) {
        /* Разгон (или поворот на полном ходу) идёт от нуля до полного хода
         * за walk_accel_time. */
        player_stop_from = 0;
        rate = (spd > 1 ? spd : 1) / walk_accel_time;
    } else {
        /* Торможение: запоминаем скорость, с которой начали, и гасим её
         * равномерно, чтобы встать ровно за walk_stop_time - и на 30, и на
         * 144 кадрах в секунду. */
        if (player_stop_from <= 0) {
            player_stop_from = have;
        }
        rate = player_stop_from / walk_stop_time;
    }
    dvx = tvx - player_vx;
    dvy = tvy - player_vy;
    dl = sqrt(dvx * dvx + dvy * dvy);
    step = rate * dt;
    if (step < 0) {
        step = 0;
    }
    if (dl <= step || dl < 0.0001) {
        player_vx = tvx;
        player_vy = tvy;
        if (want <= 0) {
            player_stop_from = 0;
        }
    } else {
        player_vx = player_vx + dvx / dl * step;
        player_vy = player_vy + dvy / dl * step;
    }
    if (fabs(player_vx) < 0.01 && want <= 0) {
        player_vx = 0;
    }
    if (fabs(player_vy) < 0.01 && want <= 0) {
        player_vy = 0;
    }
    player->x = player->x + player_vx * dt;
    player->y = player->y + player_vy * dt;
    /* Упёрлись в стену - гасим только этот компонент, чтобы кубик скользил
     * вдоль края, а не «залипал» с накопленной скоростью. */
    if (player->x < player->size) {
        player->x = player->size;
        if (player_vx < 0) player_vx = 0;
    }
    if (player->x > screen_w - player->size) {
        player->x = screen_w - player->size;
        if (player_vx > 0) player_vx = 0;
    }
    if (player->y < player->size) {
        player->y = player->size;
        if (player_vy < 0) player_vy = 0;
    }
    if (player->y > screen_h - player->size) {
        player->y = screen_h - player->size;
        if (player_vy > 0) player_vy = 0;
    }
    double jm = sqrt(joy.dx * joy.dx + joy.dy * joy.dy);
    if (jm > 0.15) {
        player->angle = atan2(joy.dy, joy.dx);
    }
    if (have > 1) {
        /* Пыль идёт от настоящей скорости: на разгоне её меньше, на торможении
         * она пропадает вместе с ходом. */
        tick_dust(player_vx / have, player_vy / have, have / (spd > 1 ? spd : 1));
    }
}

static void move_player(void) {
    if (pthrow_t > 0) {
        player_vx = 0;
        player_vy = 0;
        player_stop_from = 0;
        tick_player_throw();
        return;
    }
    if (player_stun > 0 || plates_lock_me == 1) {
        player_vx = 0;
        player_vy = 0;
        player_stop_from = 0;
        return;
    }
    if (pgrab_active == 1 || astra_locked() == 1) {
        player_vx = 0;
        player_vy = 0;
        player_stop_from = 0;
        return;
    }
    if (dash_active > 0) {
        double jm2 = sqrt(joy.dx * joy.dx + joy.dy * joy.dy);
        if (jm2 > 0.15) {
            player->angle = atan2(joy.dy, joy.dx);
        }
        player->x = player->x + dash_dx * azum_dash_speed * dt;
        player->y = player->y + dash_dy * azum_dash_speed * dt;
        if (player->x < player->size) {
            player->x = player->size;
            dash_t = 0;
        }
        if (player->x > screen_w - player->size) {
            player->x = screen_w - player->size;
            dash_t = 0;
        }
        if (player->y < player->size) {
            player->y = player->size;
            dash_t = 0;
        }
        if (player->y > screen_h - player->size) {
            player->y = screen_h - player->size;
            dash_t = 0;
        }
        double tvx = player->x - dash_x0;
        double tvy = player->y - dash_y0;
        dash_travel = tvx * dash_dx + tvy * dash_dy;
        if (dash_travel < 0) {
            dash_travel = 0;
        }
        if (dash_travel > azum_dash_speed * azum_dash_time) {
            dash_travel = azum_dash_speed * azum_dash_time;
        }
        tick_dust(dash_dx, dash_dy, 1);
        return;
    }
    player_walk_move(player_walk_speed());
}

static double enemy_in_punch(void) {
    return punch_hits_enemy(punch->x, punch->y, punch->dx, punch->dy, punch_reach, punch_width);
}

static void update_aim(void) {
    if (punch_left > 0 && super_windup <= 0 && finished == 0) {
        aim_a = fade_up_a(aim_a, aim_fade_in);
    } else {
        aim_a = fade_down_a(aim_a, aim_fade_out);
    }
    if (punch_cd > 0) {
        punch_cd = tick_down(punch_cd);
        if (admin_cheat_nocd() == 1) {
            punch_cd = 0;
        }
    }
}

static void follow_punch(void) {
    if (punch_left <= 0) {
        return;
    }
    punch->x = player->x;
    punch->y = player->y;
    punch->dx = cos(player->angle);
    punch->dy = sin(player->angle);
}

static double enemy_freeze_factor(void) {
    if (enemy->freeze > 0) {
        return enemy->freeze_slow;
    }
    return 1;
}

static void freeze_enemy(double t, double slow) {
    if (t > enemy->freeze) {
        enemy->freeze = t;
        enemy->freeze_slow = slow;
    }
}

static void enemy_move(double vx, double vy) {
    double f = enemy_freeze_factor() * enemy->speed_var;
    if (enemy_block_left > 0) {
        /* A guarding fighter retreats slowly instead of circling at full speed. */
        f = f * block_speed_k;
    }
    enemy->mvx = vx * f;
    enemy->mvy = vy * f;
    enemy->svx = vx;
    enemy->svy = vy;
    enemy->x = enemy->x + vx * f * dt;
    enemy->y = enemy->y + vy * f * dt;
    if (enemy->x < enemy->size) {
        enemy->x = enemy->size;
    }
    if (enemy->x > screen_w - enemy->size) {
        enemy->x = screen_w - enemy->size;
    }
    if (enemy->y < enemy->size) {
        enemy->y = enemy->size;
    }
    if (enemy->y > screen_h - enemy->size) {
        enemy->y = screen_h - enemy->size;
    }
    tick_enemy_dust(enemy->mvx, enemy->mvy);
}

static void enemy_face_motion(void) {
    double sp = sqrt(enemy->mvx * enemy->mvx + enemy->mvy * enemy->mvy);
    if (sp < 10) {
        return;
    }
    double ta = atan2(enemy->mvy, enemy->mvx);
    double da = ta - enemy->angle;
    if (da > pi) {
        da = da - 2 * pi;
    }
    if (da < 0 - pi) {
        da = da + 2 * pi;
    }
    double turn = da * clamp(dt * enemy_face_speed, 0, 1);
    double maxturn = enemy_face_max_speed * dt;
    enemy->angle = enemy->angle + clamp(turn, 0 - maxturn, maxturn);
}

static void enemy_track_player(void) {
    if (enemy->seen == 0) {
        enemy->px = player->x;
        enemy->py = player->y;
        enemy->pvx = 0;
        enemy->pvy = 0;
        enemy->seen = 1;
        return;
    }
    double vx = 0;
    double vy = 0;
    if (dt > 0) {
        vx = (player->x - enemy->px) / dt;
        vy = (player->y - enemy->py) / dt;
    }
    double sp = sqrt(vx * vx + vy * vy);
    if (sp > joy_speed) {
        vx = vx / sp * joy_speed;
        vy = vy / sp * joy_speed;
    }
    vx = vx + (rand() % 100 - 50) / 100.0 * 12;
    vy = vy + (rand() % 100 - 50) / 100.0 * 12;
    enemy->pvx = lerp(enemy->pvx, vx, 0.25);
    enemy->pvy = lerp(enemy->pvy, vy, 0.25);
    enemy->px = player->x;
    enemy->py = player->y;
}

static void enemy_predict(double lead) {
    double errx = (rand() % 100 - 50) / 100.0 * 18;
    double erry = (rand() % 100 - 50) / 100.0 * 18;
    aim_tx = clamp(player->x + enemy->pvx * lead + errx, player->size, screen_w - player->size);
    aim_ty = clamp(player->y + enemy->pvy * lead + erry, player->size, screen_h - player->size);
}

static void enemy_turn_prediction(void) {
    enemy_predict(enemy_lead);
    double ta = atan2(aim_ty - enemy->y, aim_tx - enemy->x) + enemy->aim_err;
    double da = ta - enemy->angle;
    if (da > pi) {
        da = da - 2 * pi;
    }
    if (da < 0 - pi) {
        da = da + 2 * pi;
    }
    double step = enemy_attack_turn_speed * dt;
    enemy->angle = enemy->angle + clamp(da, 0 - step, step);
}

static double enemy_would_hit(void) {
    enemy_predict(enemy_lead);
    double rx = aim_tx - enemy->x;
    double ry = aim_ty - enemy->y;
    double fd = sqrt(rx * rx + ry * ry);
    double pr = player_half();
    double margin = enemy_hit_margin;
    if (sqrt(enemy->pvx * enemy->pvx + enemy->pvy * enemy->pvy) < enemy_still_speed) {
        margin = 0;
    }
    if (fd <= punch_forward_offset + enemy_punch_reach + pr - margin) {
        return 1;
    }
    return 0;
}

static double enemy_in_danger(void) {
    return punch_hits_enemy(player->x, player->y, cos(player->angle), sin(player->angle), punch_reach, punch_width);
}

static double enemy_free_window(void) {
    if (punch_left <= 0 && punch_cd > enemy_punish_react) {
        return 1;
    }
    return 0;
}

static void enemy_pick_flank(void) {
    double side = (enemy->y - player->y) * cos(player->angle) - (enemy->x - player->x) * sin(player->angle);
    if (side >= 0) {
        enemy->strafe = 1;
    } else {
        enemy->strafe = -1;
    }
    if (rand() % 100 < 22) {
        enemy->strafe = 0 - enemy->strafe;
    }
}

static double enemy_roll_cooldown(void) {
    double r = enemy_cooldown_min + (rand() % 1000) / 1000.0 * (enemy_cooldown_max - enemy_cooldown_min);
    enemy->speed_var = 0.85 + (rand() % 100) / 100.0 * enemy_human_speed_var;
    if (r < enemy_cooldown_min) {
        r = enemy_cooldown_min;
    }
    return r;
}
