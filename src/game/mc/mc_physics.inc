static void mc_step_physics(float fdt, float move_fwd, float move_right, int want_jump) {
    float hw = MC_PLAYER_W * 0.5f;
    float spd = mc.flying ? 10.5f : (mc.sneaking ? 2.4f : (mc.in_water ? 3.4f : 5.4f));
    float wish_x = sinf(mc.yaw) * move_fwd - cosf(mc.yaw) * move_right;
    float wish_z = cosf(mc.yaw) * move_fwd + sinf(mc.yaw) * move_right;
    float wl = sqrtf(wish_x * wish_x + wish_z * wish_z);
    float nx, ny, nz;

    if (wl > 1.0f) {
        wish_x /= wl;
        wish_z /= wl;
    }
    mc.vx = wish_x * spd;
    mc.vz = wish_z * spd;
    mc.in_water = mc_is_liquid_id(mc_get((int)floorf(mc.px), (int)floorf(mc.py + 0.5f), (int)floorf(mc.pz)));

    if (mc.flying) {
        mc.vy = 0.0f;
        if (want_jump)
            mc.vy = 7.5f;
        if (mc.sneaking)
            mc.vy = -7.5f;
    } else if (mc.in_water) {
        mc.vy -= MC_GRAVITY * 0.25f * fdt;
        if (mc.vy < -2.6f)
            mc.vy = -2.6f;
        if (want_jump)
            mc.vy = 4.6f;
    } else {
        mc.vy -= MC_GRAVITY * fdt;
        if (mc.vy < -34.0f)
            mc.vy = -34.0f;
        if (want_jump && mc.on_ground && mc.jump_cd <= 0.0f) {
            mc.vy = MC_JUMP_VEL;
            mc.on_ground = 0;
            mc.jump_cd = 0.16f;
        }
    }

    /* Slide X with sneak edge-fall protection */
    nx = mc.px + mc.vx * fdt;
    if (nx < 1.0f)
        nx = 1.0f;
    if (nx > (float)MC_WX - 1.0f)
        nx = (float)MC_WX - 1.0f;
    if (!mc_aabb_hits_world(nx, mc.py, mc.pz, hw, MC_PLAYER_H)) {
        if (!(mc.sneaking && mc.on_ground && !mc.flying &&
              !mc_aabb_hits_world(nx, mc.py - 0.15f, mc.pz, hw * 0.85f, 0.12f)))
            mc.px = nx;
    }
    /* Slide Z with sneak edge-fall protection */
    nz = mc.pz + mc.vz * fdt;
    if (nz < 1.0f)
        nz = 1.0f;
    if (nz > (float)MC_WZ - 1.0f)
        nz = (float)MC_WZ - 1.0f;
    if (!mc_aabb_hits_world(mc.px, mc.py, nz, hw, MC_PLAYER_H)) {
        if (!(mc.sneaking && mc.on_ground && !mc.flying &&
              !mc_aabb_hits_world(mc.px, mc.py - 0.15f, nz, hw * 0.85f, 0.12f)))
            mc.pz = nz;
    }
    /* Slide Y */
    ny = mc.py + mc.vy * fdt;
    if (!mc_aabb_hits_world(mc.px, ny, mc.pz, hw, MC_PLAYER_H)) {
        mc.py = ny;
        mc.on_ground = 0;
    } else {
        if (mc.vy < 0.0f) {
            mc.py = floorf(mc.py + 0.05f) + 0.001f;
            mc.on_ground = 1;
            if (mc.vy < -17.0f && !mc.flying)
                mc.hurt_t = 0.25f;
        }
        mc.vy = 0.0f;
    }
    if (mc.py < 1.0f)
        mc.py = (float)(mc_top_solid_y((int)mc.px, (int)mc.pz) + 1) + 0.02f;

    /* Stride view bobbing */
    {
        float hspd = sqrtf(wish_x * wish_x + wish_z * wish_z);
        float want_bob = (mc.on_ground && !mc.flying && hspd > 0.1f) ? 1.0f : 0.0f;
        mc.bob_amp += (want_bob - mc.bob_amp) * (fdt * 10.0f > 1.0f ? 1.0f : fdt * 10.0f);
        if (want_bob > 0.0f)
            mc.walk_dist += hspd * spd * fdt;
    }
}

static void mc_sync_online_world_and_players(float fdt) {
    int bx, by, bz, bid, s;
    /* 1. Drain persistent world block delta queue from Firebase /rooms/<room>/chat/b_x_y_z */
    while (net_mc_poll_edit(&bx, &by, &bz, &bid)) {
        if (mc_in_bounds(bx, by, bz) && bid >= 0 && bid < MC_TOTAL_TYPES)
            mc.vox[by][bz][bx] = (unsigned char)bid;
    }
    /* 2. Publish our 3D state into existing Firebase rooms/<room>/players/<slot> schema! */
    net_publish((double)mc.px / (double)MC_WX, (double)mc.pz / (double)MC_WZ, (double)mc.yaw, player->hp,
                player->hp > 0 ? 1.0 : 0.0);
    net_publish_punch(0.0, 0.0, 0.0, 0.0, (double)mc.local_punch_seq);
    net_publish_block((double)mc.sneaking);
    net_publish_turrets(0.0, 0.0, (double)mc.hotbar[mc.slot], 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    net_publish_snow(0.0, 0.0, 0.0, 0.0, (double)mc.last_edit_pack);
    net_publish_universe((double)(mc.py / (float)MC_WY), (double)(mc.pitch * 0.3f + 0.5f), (double)mc.last_edit_block);
    net_publish_brick(0.0, 0.0, 0.0, 0.0, (double)mc.local_edit_seq);

    /* 3. Update up to 4 live 3D players in the room + apply instant <60ms SSE block edits! */
    for (s = 0; s < 4; s++) {
        double ds = (double)s;
        if (s == (int)net_slot() || net_player_online(ds) != 1.0 || net_player_alive(ds) != 1.0) {
            mc.rem[s].active = 0;
            continue;
        }
        {
            float tx = (float)(net_player_x(ds) * (double)MC_WX);
            float tz = (float)(net_player_y(ds) * (double)MC_WZ);
            float ty = (float)(net_player_universe_x(ds) * (double)MC_WY);
            int pseq = (int)net_player_punch(ds);
            int eseq = (int)net_player_brick(ds);
            const char *rnick = net_player_nick(ds);
            if (rnick && strcmp(rnick, "mov eax, 0x229") == 0)
                grant_achievement(ACH_EAX229);
            if (ty < 1.0f)
                ty = (float)(mc_top_solid_y((int)tx, (int)tz) + 1);
            if (!mc.rem[s].active) {
                mc.rem[s].active = 1;
                mc.rem[s].x = tx;
                mc.rem[s].y = ty;
                mc.rem[s].z = tz;
                mc.rem[s].last_punch = pseq;
                mc.rem[s].last_edit_seq = eseq;
            } else {
                float dx = tx - mc.rem[s].x, dz = tz - mc.rem[s].z;
                float k = fdt * 12.0f > 1.0f ? 1.0f : fdt * 12.0f;
                if (dx * dx + dz * dz > 0.0004f)
                    mc.rem[s].walk_t += fdt * 1.4f;
                else
                    mc.rem[s].walk_t = 0.0f;
                mc.rem[s].x += dx * k;
                mc.rem[s].y += (ty - mc.rem[s].y) * k;
                mc.rem[s].z += dz * k;
            }
            mc.rem[s].yaw = (float)net_player_angle(ds);
            mc.rem[s].pitch = ((float)net_player_universe_y(ds) - 0.5f) / 0.3f;
            mc.rem[s].sneaking = net_player_block(ds) > 0.5;
            mc.rem[s].held_block = (int)net_player_station_hp(ds);
            if (mc.rem[s].held_block <= 0 || mc.rem[s].held_block >= MC_TOTAL_TYPES)
                mc.rem[s].held_block = MC_GRASS;

            /* Fast-path SSE block edit from co-player (< 60ms latency!) */
            if (eseq > mc.rem[s].last_edit_seq) {
                int pack = (int)(net_player_snow(ds) + 0.5);
                int rbid = (int)(net_player_universe(ds) + 0.5);
                int rx = pack / 10000, ry = (pack / 100) % 100, rz = pack % 100;
                mc.rem[s].last_edit_seq = eseq;
                mc.rem[s].swing_t = 1.0f;
                if (mc_in_bounds(rx, ry, rz) && rbid >= 0 && rbid < MC_TOTAL_TYPES) {
                    unsigned char prev = mc.vox[ry][rz][rx];
                    if (rbid == MC_AIR && prev != MC_AIR)
                        mc_spawn_block_particles((float)rx + 0.5f, (float)ry + 0.5f, (float)rz + 0.5f, prev, 8);
                    mc.vox[ry][rz][rx] = (unsigned char)rbid;
                }
            }
            /* PvP melee hit check when remote player swings toward us within 3.8 blocks */
            if (pseq > mc.rem[s].last_punch) {
                float dx = mc.px - mc.rem[s].x, dy = mc.py - mc.rem[s].y, dz = mc.pz - mc.rem[s].z;
                mc.rem[s].last_punch = pseq;
                mc.rem[s].swing_t = 1.0f;
                if (dx * dx + dy * dy + dz * dz < 14.4f && player->hp > 0) {
                    player->hp -= 1.5;
                    mc.hurt_t = 0.30f;
                    punch_snd();
                    if (player->hp <= 0) {
                        player->hp = player->max_hp;
                        mc.px = 32.5f;
                        mc.pz = 29.5f;
                        mc.py = (float)(mc_top_solid_y(32, 29) + 1) + 0.02f;
                    }
                }
            }
            if (mc.rem[s].swing_t > 0.0f)
                mc.rem[s].swing_t -= fdt * 3.8f;
            if (mc.rem[s].hurt_t > 0.0f)
                mc.rem[s].hurt_t -= fdt;
        }
    }
}
