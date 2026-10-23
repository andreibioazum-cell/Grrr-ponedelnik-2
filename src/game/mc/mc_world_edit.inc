/* 3D AABB collision, Amanatides-Woo 3D raycast, progressive block breaking,
 * block placement, primed TNT explosions, Solo NPC AI, and Online Firebase 3D sync. */

static int mc_aabb_hits_world(float px, float py, float pz, float hw, float h) {
    int x0 = (int)floorf(px - hw), x1 = (int)floorf(px + hw);
    int y0 = (int)floorf(py), y1 = (int)floorf(py + h);
    int z0 = (int)floorf(pz - hw), z1 = (int)floorf(pz + hw);
    int x, y, z;
    for (y = y0; y <= y1; y++)
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++)
                if (mc_is_solid(x, y, z))
                    return 1;
    return 0;
}

static void mc_spawn_block_particles(float wx, float wy, float wz, unsigned char b, int count) {
    int i, spawned = 0;
    for (i = 0; i < MC_MAX_PARTICLES && spawned < count; i++) {
        if (!mc.parts[i].active) {
            unsigned int h = mc_hash3(i, spawned + (int)(mc.time_sec * 100.0f), (int)wx * 7 + (int)wz, 5150u);
            float rx = ((float)(h & 255u) / 255.0f - 0.5f);
            float ry = ((float)((h >> 8) & 255u) / 255.0f);
            float rz = ((float)((h >> 16) & 255u) / 255.0f - 0.5f);
            mc.parts[i].active = 1;
            mc.parts[i].x = wx + rx * 0.6f;
            mc.parts[i].y = wy + ry * 0.6f;
            mc.parts[i].z = wz + rz * 0.6f;
            mc.parts[i].vx = rx * 5.2f;
            mc.parts[i].vy = 2.4f + ry * 4.2f;
            mc.parts[i].vz = rz * 5.2f;
            mc.parts[i].life = 0.45f + ry * 0.45f;
            mc.parts[i].max_life = mc.parts[i].life;
            mc.parts[i].size = 0.14f;
            mc.parts[i].tile = b;
            spawned++;
        }
    }
}

static void mc_set_world_block(int x, int y, int z, unsigned char b, int broadcast) {
    unsigned char prev;
    if (!mc_in_bounds(x, y, z) || y == 0)
        return;
    prev = mc.vox[y][z][x];
    if (prev == b)
        return;
    mc.vox[y][z][x] = b;
    if (prev == MC_GLOWSTONE || b == MC_GLOWSTONE)
        mc_rebuild_glow_lut();
    if (!broadcast)
        return;
    if (game_state == ST_ONLINE) {
        int is_nat = (mc.base_vox[y][z][x] == b) ? 1 : 0;
        mc.local_edit_seq++;
        mc.last_edit_pack = x * 10000 + y * 100 + z;
        mc.last_edit_block = (int)b;
        net_mc_block_write(x, y, z, (int)b, is_nat);
    } else if (game_state == ST_SOLO) {
        mc_solo_save();
    }
}

static void mc_prime_tnt(int x, int y, int z, float fuse) {
    int i;
    mc_set_world_block(x, y, z, MC_AIR, 1);
    for (i = 0; i < MC_MAX_TNT; i++) {
        if (!mc.tnts[i].active) {
            mc.tnts[i].active = 1;
            mc.tnts[i].x = (float)x + 0.5f;
            mc.tnts[i].y = (float)y + 0.02f;
            mc.tnts[i].z = (float)z + 0.5f;
            mc.tnts[i].vx = 0.0f;
            mc.tnts[i].vy = 3.6f;
            mc.tnts[i].vz = 0.0f;
            mc.tnts[i].fuse = fuse;
            play_button_click();
            return;
        }
    }
}

static void mc_explode_at(float ex, float ey, float ez, float radius) {
    int x0 = (int)floorf(ex - radius), x1 = (int)ceilf(ex + radius);
    int y0 = (int)floorf(ey - radius), y1 = (int)ceilf(ey + radius);
    int z0 = (int)floorf(ez - radius), z1 = (int)ceilf(ez + radius);
    int x, y, z, i;
    float dx, dy, dz, d;
    punch_snd();
    for (y = y0; y <= y1; y++)
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++) {
                unsigned char b;
                if (!mc_in_bounds(x, y, z) || y <= 0)
                    continue;
                dx = ((float)x + 0.5f) - ex;
                dy = ((float)y + 0.5f) - ey;
                dz = ((float)z + 0.5f) - ez;
                if (dx * dx + dy * dy + dz * dz > radius * radius)
                    continue;
                b = mc.vox[y][z][x];
                if (b == MC_AIR || b == MC_BEDROCK || b == MC_OBSIDIAN)
                    continue;
                if (b == MC_TNT) {
                    mc_prime_tnt(x, y, z, 0.45f);
                    continue;
                }
                if (((x + y + z) & 3) == 0)
                    mc_spawn_block_particles((float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f, b, 2);
                mc_set_world_block(x, y, z, MC_AIR, 1);
            }
    dx = mc.px - ex;
    dy = (mc.py + 0.9f) - ey;
    dz = mc.pz - ez;
    d = sqrtf(dx * dx + dy * dy + dz * dz);
    if (d < 6.0f && d > 0.01f) {
        float k = (6.0f - d) * 2.2f;
        mc.vx += (dx / d) * k;
        mc.vy += 5.5f;
        mc.vz += (dz / d) * k;
    }
    for (i = 0; i < MC_MAX_NPCS; i++) {
        if (mc.npcs[i].active && mc.npcs[i].hp > 0.0f) {
            dx = mc.npcs[i].x - ex;
            dz = mc.npcs[i].z - ez;
            d = sqrtf(dx * dx + dz * dz);
            if (d < 5.5f) {
                mc.npcs[i].hp -= (5.5f - d) * 2.2f;
                mc.npcs[i].hurt_t = 0.35f;
                mc.npcs[i].vy = 6.0f;
            }
        }
    }
}

/* Exact Amanatides-Woo 3D voxel raycast + 3D player/mob hitbox check */
static void mc_update_raycast(void) {
    float ox = mc.cam_x, oy = mc.cam_y, oz = mc.cam_z;
    float dx = mc.fx, dy = mc.fy, dz = mc.fz;
    int ix = (int)floorf(ox), iy = (int)floorf(oy), iz = (int)floorf(oz);
    int step_x = dx >= 0.0f ? 1 : -1;
    int step_y = dy >= 0.0f ? 1 : -1;
    int step_z = dz >= 0.0f ? 1 : -1;
    float t_dx = fabsf(dx) > 1e-5f ? 1.0f / fabsf(dx) : 1e6f;
    float t_dy = fabsf(dy) > 1e-5f ? 1.0f / fabsf(dy) : 1e6f;
    float t_dz = fabsf(dz) > 1e-5f ? 1.0f / fabsf(dz) : 1e6f;
    float t_max_x = ((step_x > 0 ? ((float)ix + 1.0f - ox) : (ox - (float)ix))) * t_dx;
    float t_max_y = ((step_y > 0 ? ((float)iy + 1.0f - oy) : (oy - (float)iy))) * t_dy;
    float t_max_z = ((step_z > 0 ? ((float)iz + 1.0f - oz) : (oz - (float)iz))) * t_dz;
    float dist = 0.0f, best_ent = MC_REACH;
    int lnx = 0, lny = 0, lnz = 0, steps, i;

    mc.hit = 0;
    mc.hit_npc = -1;
    mc.hit_remote = -1;
    mc.hit_dist = MC_REACH;

    for (steps = 0; steps < 32 && dist <= MC_REACH; steps++) {
        if (mc_in_bounds(ix, iy, iz)) {
            unsigned char b = mc.vox[iy][iz][ix];
            if (b != MC_AIR && !mc_is_liquid_id(b)) {
                mc.hit = 1;
                mc.bx = ix;
                mc.by = iy;
                mc.bz = iz;
                mc.nx = lnx;
                mc.ny = lny;
                mc.nz = lnz;
                mc.hit_dist = dist;
                best_ent = dist;
                break;
            }
        }
        if (t_max_x < t_max_y && t_max_x < t_max_z) {
            dist = t_max_x;
            t_max_x += t_dx;
            ix += step_x;
            lnx = -step_x;
            lny = 0;
            lnz = 0;
        } else if (t_max_y < t_max_z) {
            dist = t_max_y;
            t_max_y += t_dy;
            iy += step_y;
            lnx = 0;
            lny = -step_y;
            lnz = 0;
        } else {
            dist = t_max_z;
            t_max_z += t_dz;
            iz += step_z;
            lnx = 0;
            lny = 0;
            lnz = -step_z;
        }
    }

    /* Check if player is aiming directly at a closer 3D NPC or Remote Player (reach 4.2m) */
    for (i = 0; i < MC_MAX_NPCS; i++) {
        if (mc.npcs[i].active && mc.npcs[i].hp > 0.0f) {
            float ex = mc.npcs[i].x - ox, ey = (mc.npcs[i].y + 0.95f) - oy, ez = mc.npcs[i].z - oz;
            float proj = ex * dx + ey * dy + ez * dz;
            if (proj > 0.2f && proj < 4.2f && proj < best_ent) {
                float px2 = ex - dx * proj, py2 = ey - dy * proj, pz2 = ez - dz * proj;
                if (px2 * px2 + (py2 * 0.55f) * (py2 * 0.55f) + pz2 * pz2 < 0.34f) {
                    best_ent = proj;
                    mc.hit_npc = i;
                    mc.hit_remote = -1;
                }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if (mc.rem[i].active) {
            float ex = mc.rem[i].x - ox, ey = (mc.rem[i].y + 0.95f) - oy, ez = mc.rem[i].z - oz;
            float proj = ex * dx + ey * dy + ez * dz;
            if (proj > 0.2f && proj < 4.2f && proj < best_ent) {
                float px2 = ex - dx * proj, py2 = ey - dy * proj, pz2 = ez - dz * proj;
                if (px2 * px2 + (py2 * 0.55f) * (py2 * 0.55f) + pz2 * pz2 < 0.36f) {
                    best_ent = proj;
                    mc.hit_remote = i;
                    mc.hit_npc = -1;
                }
            }
        }
    }
}

static void mc_try_place_block(void) {
    int tx, ty, tz, chosen = mc.hotbar[mc.slot];
    float hw = MC_PLAYER_W * 0.5f;
    if (!mc.hit || chosen <= 0 || chosen >= MC_TOTAL_TYPES)
        return;
    tx = mc.bx + mc.nx;
    ty = mc.by + mc.ny;
    tz = mc.bz + mc.nz;
    if (!mc_in_bounds(tx, ty, tz) || ty <= 0)
        return;
    if (mc_is_solid(tx, ty, tz))
        return;
    /* Prevent entombing player body inside solid block while allowing pillar-jumping */
    if (mc_is_solid_id((unsigned char)chosen)) {
        int overlap_xz = (mc.px + hw > (float)tx + 0.02f && mc.px - hw < (float)tx + 0.98f &&
                          mc.pz + hw > (float)tz + 0.02f && mc.pz - hw < (float)tz + 0.98f);
        int overlap_y = (mc.py < (float)ty + 0.98f && mc.py + MC_PLAYER_H > (float)ty + 0.02f);
        if (overlap_xz && overlap_y) {
            /* If jumping upward onto top of pillar, gently lift feet cleanly onto block top! */
            if (mc.py > (float)ty + 0.55f && !mc_aabb_hits_world(mc.px, (float)ty + 1.01f, mc.pz, hw, MC_PLAYER_H))
                mc.py = (float)ty + 1.01f;
            else
                return;
        }
    }
    mc_set_world_block(tx, ty, tz, (unsigned char)chosen, 1);
    mc.swing_t = 1.0f;
    mc.place_cd = 0.20f;
    play_button_click();
}

static void mc_melee_swing(void) {
    mc.swing_t = 1.0f;
    mc.local_punch_seq++;
    mc.place_cd = 0.28f;
    if (mc.hit_npc >= 0) {
        int i = mc.hit_npc;
        mc.npcs[i].hp -= (float)(punch_damage > 0.5 ? punch_damage : 2.5);
        mc.npcs[i].hurt_t = 0.32f;
        mc.npcs[i].vx = mc.fx * 6.5f;
        mc.npcs[i].vy = 4.6f;
        mc.npcs[i].vz = mc.fz * 6.5f;
        punch_snd();
        mc_spawn_block_particles(mc.npcs[i].x, mc.npcs[i].y + 1.1f, mc.npcs[i].z,
                                 (unsigned char)mc.npcs[i].held_block, 6);
        if (mc.npcs[i].hp <= 0.0f) {
            mc.npcs[i].respawn_t = 8.0f;
            mc.kills_this_run++;
            cups += 4.0;
            candies += 3.0;
            save_progress();
        }
        return;
    }
    if (mc.hit_remote >= 0) {
        mc.rem[mc.hit_remote].hurt_t = 0.30f;
        punch_snd();
        return;
    }
    punch_snd();
}

