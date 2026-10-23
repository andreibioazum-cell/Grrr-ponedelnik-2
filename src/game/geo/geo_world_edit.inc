/* 3D voxel DDA raycast, block mining & placing, demolition charge physics,
 * AABB sliding movement, buoyancy, creative flight, and real-time Firebase sync. */

static void geo_play_sfx(const char *name, float scale) {
    float v = net_sound_volume() * scale;
    if (v <= 0.02f)
        return;
    snd_volume(name, (double)(v > 1.0f ? 1.0f : v));
    (void)snd_play(name);
}

static int geo_aabb_hits_solid(float px, float py, float pz, float hw, float h) {
    int x0 = (int)floorf(px - hw), x1 = (int)floorf(px + hw);
    int y0 = (int)floorf(py), y1 = (int)floorf(py + h);
    int z0 = (int)floorf(pz - hw), z1 = (int)floorf(pz + hw);
    int x, y, z;
    for (y = y0; y <= y1; y++)
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++)
                if (geo_is_solid(x, y, z))
                    return 1;
    return 0;
}

static void geo_spawn_particles(float wx, float wy, float wz, unsigned char b, int count) {
    int i, spawned = 0;
    if (b == GEO_AIR)
        return;
    for (i = 0; i < GEO_MAX_PARTICLES && spawned < count; i++) {
        if (!geo.parts[i].active) {
            unsigned int h = geo_hash3(i, spawned + (int)(geo.time_sec * 100.0f), (int)(wx * 13.0f), 0x51A7u);
            geo.parts[i].active = 1;
            geo.parts[i].block_id = b;
            geo.parts[i].x = wx + ((float)(h & 255u) / 255.0f - 0.5f) * 0.6f;
            geo.parts[i].y = wy + ((float)((h >> 8) & 255u) / 255.0f - 0.5f) * 0.6f;
            geo.parts[i].z = wz + ((float)((h >> 16) & 255u) / 255.0f - 0.5f) * 0.6f;
            geo.parts[i].vx = ((float)(h & 63u) / 31.5f - 1.0f) * 2.6f;
            geo.parts[i].vy = 1.6f + (float)((h >> 6) & 63u) / 24.0f;
            geo.parts[i].vz = ((float)((h >> 12) & 63u) / 31.5f - 1.0f) * 2.6f;
            geo.parts[i].life = 0.45f + (float)((h >> 18) & 31u) / 60.0f;
            spawned++;
        }
    }
}

static void geo_set_block(int x, int y, int z, unsigned char b, int broadcast) {
    unsigned char old;
    if (!geo_in_bounds(x, y, z) || y == 0)
        return;
    old = geo.vox[y][z][x];
    if (old == b)
        return;
    geo.vox[y][z][x] = b;
    if (old == GEO_LUMEN || old == GEO_CYAN_LAMP || b == GEO_LUMEN || b == GEO_CYAN_LAMP)
        geo_rebuild_lights();
    if (broadcast && geo.online)
        net_block_send(x, y, z, (int)b);
}

static void geo_prime_charge(int x, int y, int z, float fuse) {
    int i;
    geo_set_block(x, y, z, GEO_AIR, 1);
    for (i = 0; i < GEO_MAX_CHARGES; i++) {
        if (!geo.charges[i].active) {
            geo.charges[i].active = 1;
            geo.charges[i].x = (float)x + 0.5f;
            geo.charges[i].y = (float)y + 0.02f;
            geo.charges[i].z = (float)z + 0.5f;
            geo.charges[i].vy = 2.6f;
            geo.charges[i].fuse = fuse;
            break;
        }
    }
}

static void geo_detonate_at(float ex, float ey, float ez, float radius) {
    int r = (int)ceilf(radius);
    int ix = (int)floorf(ex), iy = (int)floorf(ey), iz = (int)floorf(ez);
    int x, y, z;
    geo_play_sfx("sounds/charge_boom.wav", 1.0f);
    ds_gfx_shake(9.0, 0.32);
    for (y = iy - r; y <= iy + r; y++) {
        for (z = iz - r; z <= iz + r; z++) {
            for (x = ix - r; x <= ix + r; x++) {
                float dx = ((float)x + 0.5f) - ex, dy = ((float)y + 0.5f) - ey, dz = ((float)z + 0.5f) - ez;
                unsigned char b;
                if (!geo_in_bounds(x, y, z) || y <= 0 || dx * dx + dy * dy + dz * dz > radius * radius)
                    continue;
                b = geo.vox[y][z][x];
                if (b == GEO_AIR || b == GEO_MONOLITH)
                    continue;
                if (b == GEO_CHARGE)
                    geo_prime_charge(x, y, z, 0.35f);
                else {
                    geo_spawn_particles((float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f, b, 2);
                    geo_set_block(x, y, z, GEO_AIR, 1);
                }
            }
        }
    }
}

/* Exact Amanatides & Woo 3D DDA voxel traversal along camera forward ray */
static void geo_update_raycast(void) {
    float ox = geo.px, oy = geo.py + GEO_EYE_H, oz = geo.pz;
    float dx = geo.fx, dy = geo.fy, dz = geo.fz;
    int ix = (int)floorf(ox), iy = (int)floorf(oy), iz = (int)floorf(oz);
    int step_x = dx >= 0.0f ? 1 : -1;
    int step_y = dy >= 0.0f ? 1 : -1;
    int step_z = dz >= 0.0f ? 1 : -1;
    float inv_x = fabsf(dx) > 1e-5f ? fabsf(1.0f / dx) : 1e6f;
    float inv_y = fabsf(dy) > 1e-5f ? fabsf(1.0f / dy) : 1e6f;
    float inv_z = fabsf(dz) > 1e-5f ? fabsf(1.0f / dz) : 1e6f;
    float tmax_x = ((dx >= 0.0f ? ((float)(ix + 1) - ox) : (ox - (float)ix))) * inv_x;
    float tmax_y = ((dy >= 0.0f ? ((float)(iy + 1) - oy) : (oy - (float)iy))) * inv_y;
    float tmax_z = ((dz >= 0.0f ? ((float)(iz + 1) - oz) : (oz - (float)iz))) * inv_z;
    int last_nx = 0, last_ny = 1, last_nz = 0;
    float t = 0.0f;
    int iter;

    geo.hit = 0;
    for (iter = 0; iter < 32 && t <= 6.8f; iter++) {
        if (geo_in_bounds(ix, iy, iz)) {
            unsigned char b = geo.vox[iy][iz][ix];
            if (b != GEO_AIR && b != GEO_WATER) {
                geo.hit = 1;
                geo.bx = ix;
                geo.by = iy;
                geo.bz = iz;
                geo.nx = last_nx;
                geo.ny = last_ny;
                geo.nz = last_nz;
                geo.hit_dist = t;
                return;
            }
        }
        if (tmax_x < tmax_y && tmax_x < tmax_z) {
            t = tmax_x;
            tmax_x += inv_x;
            ix += step_x;
            last_nx = -step_x;
            last_ny = 0;
            last_nz = 0;
        } else if (tmax_y < tmax_z) {
            t = tmax_y;
            tmax_y += inv_y;
            iy += step_y;
            last_nx = 0;
            last_ny = -step_y;
            last_nz = 0;
        } else {
            t = tmax_z;
            tmax_z += inv_z;
            iz += step_z;
            last_nx = 0;
            last_ny = 0;
            last_nz = -step_z;
        }
    }
}

static int geo_try_place_block(void) {
    int tx, ty, tz, held;
    if (!geo.hit)
        return 0;
    held = geo.hotbar[geo.slot];
    if (held <= 0 || held >= GEO_BLOCK_COUNT)
        return 0;

    /* Tapping place on an existing demolition charge ignites its fuse! */
    if (geo_get(geo.bx, geo.by, geo.bz) == GEO_CHARGE && held != GEO_CHARGE) {
        geo_prime_charge(geo.bx, geo.by, geo.bz, 2.2f);
        geo_play_sfx("sounds/ui_tap.wav", 0.9f);
        return 1;
    }

    tx = geo.bx + geo.nx;
    ty = geo.by + geo.ny;
    tz = geo.bz + geo.nz;
    if (!geo_in_bounds(tx, ty, tz) || ty <= 0)
        return 0;
    if (geo_is_solid(tx, ty, tz))
        return 0;

    if (geo_is_solid_id((unsigned char)held)) {
        /* Never suffocate/entomb the local player inside a newly placed solid block */
        float hw = 0.29f;
        int over_x = (geo.px + hw > (float)tx) && (geo.px - hw < (float)(tx + 1));
        int over_y = (geo.py + 1.78f > (float)ty) && (geo.py < (float)(ty + 1));
        int over_z = (geo.pz + hw > (float)tz) && (geo.pz - hw < (float)(tz + 1));
        if (over_x && over_y && over_z)
            return 0;
    }

    geo_set_block(tx, ty, tz, (unsigned char)held, 1);
    geo.swing_anim = 1.0f;
    geo.swing_seq++;
    geo_play_sfx(held == GEO_WATER ? "sounds/water_splash.wav" : "sounds/block_place.wav", 0.85f);
    if (!geo.online)
        geo_solo_save();
    return 1;
}

static void geo_update_gameplay(float fdt) {
    float fwd = 0.0f, right = 0.0f, sy, cy, speed, nx, ny, nz;
    const float hw = 0.30f, ph = 1.78f;
    int bx, by, bz, bid, i;
    int want_mine = (geo.mine_tid >= 0);
    int want_place = geo.place_tap_queued || (geo.place_tid >= 0 && geo.place_cd <= 0.0f);
    int want_jump = (geo.jump_tid >= 0);

    /* Apply incoming live block deltas from Firebase RTDB */
    if (geo.online) {
        int guard = 0;
        while (guard++ < 512 && net_block_poll(&bx, &by, &bz, &bid))
            geo_set_block(bx, by, bz, (unsigned char)bid, 0);
    }

    if (geo.joy_tid >= 0) {
        right = geo.joy_x;
        fwd = -geo.joy_y;
    }
    sy = sinf(geo.yaw);
    cy = cosf(geo.yaw);
    speed = geo.flying ? 9.4f : (geo.in_water ? 3.8f : 5.4f);
    geo.vx = (sy * fwd - cy * right) * speed;
    geo.vz = (cy * fwd + sy * right) * speed;

    geo.in_water = geo_is_liquid_id(geo_get((int)floorf(geo.px), (int)floorf(geo.py + 0.7f), (int)floorf(geo.pz)));
    if (geo.flying) {
        geo.vy = 0.0f;
        if (geo.up_tid >= 0 || want_jump)
            geo.vy += 6.8f;
        if (geo.down_tid >= 0)
            geo.vy -= 6.8f;
    } else if (geo.in_water) {
        geo.vy -= 5.5f * fdt;
        if (want_jump)
            geo.vy = 3.8f;
        if (geo.vy < -2.6f)
            geo.vy = -2.6f;
    } else {
        geo.vy -= 24.0f * fdt;
        if (geo.vy < -32.0f)
            geo.vy = -32.0f;
        if (want_jump && geo.on_ground) {
            geo.vy = 8.4f;
            geo.on_ground = 0;
            geo_play_sfx("sounds/jump.wav", 0.7f);
        }
    }

    nx = geo.px + geo.vx * fdt;
    if (!geo_aabb_hits_solid(nx, geo.py, geo.pz, hw, ph))
        geo.px = nx;
    nz = geo.pz + geo.vz * fdt;
    if (!geo_aabb_hits_solid(geo.px, geo.py, nz, hw, ph))
        geo.pz = nz;
    ny = geo.py + geo.vy * fdt;
    if (!geo_aabb_hits_solid(geo.px, ny, geo.pz, hw, ph)) {
        geo.py = ny;
        geo.on_ground = 0;
    } else {
        if (geo.vy < 0.0f) {
            geo.py = floorf(geo.py) + 0.001f;
            geo.on_ground = 1;
        }
        geo.vy = 0.0f;
    }
    if (geo.px < 1.2f)
        geo.px = 1.2f;
    if (geo.px > (float)GEO_WX - 1.2f)
        geo.px = (float)GEO_WX - 1.2f;
    if (geo.pz < 1.2f)
        geo.pz = 1.2f;
    if (geo.pz > (float)GEO_WZ - 1.2f)
        geo.pz = (float)GEO_WZ - 1.2f;
    if (geo.py < 1.0f) {
        geo.py = (float)geo_top_solid_y((int)geo.px, (int)geo.pz) + 1.02f;
        geo.vy = 0.0f;
    }

    if ((fabsf(geo.vx) + fabsf(geo.vz)) > 0.4f) {
        geo.walk_phase += fdt * 8.5f;
        geo.step_cd -= fdt;
        if (geo.on_ground && geo.step_cd <= 0.0f) {
            geo.step_cd = 0.34f;
            geo_play_sfx("sounds/step.wav", 0.55f);
        }
    }
    if (geo.swing_anim > 0.0f) {
        geo.swing_anim -= fdt * 4.2f;
        if (geo.swing_anim < 0.0f)
            geo.swing_anim = 0.0f;
    }
    if (geo.place_cd > 0.0f)
        geo.place_cd -= fdt;

    /* Update camera basis and raycast */
    geo_setup_camera(geo.px, geo.py + GEO_EYE_H, geo.pz, geo.yaw, geo.pitch, geo.third_person);
    geo_update_raycast();

    if (want_mine && !geo.inv_open && !geo.chat_open && !geo.pause_open) {
        if (geo.swing_anim <= 0.05f) {
            geo.swing_anim = 1.0f;
            geo.swing_seq++;
        }
        if (geo.hit) {
            unsigned char b = geo_get(geo.bx, geo.by, geo.bz);
            if (b == GEO_CHARGE) {
                geo_prime_charge(geo.bx, geo.by, geo.bz, 2.0f);
                geo.mine_on = 0;
            } else if (b != GEO_MONOLITH && b != GEO_AIR) {
                if (!geo.mine_on || geo.mx != geo.bx || geo.my != geo.by || geo.mz != geo.bz) {
                    geo.mine_on = 1;
                    geo.mx = geo.bx;
                    geo.my = geo.by;
                    geo.mz = geo.bz;
                    geo.mine_prog = 0.0f;
                }
                geo.mine_prog += fdt / (geo_blocks[b].hardness > 0.1f ? geo_blocks[b].hardness : 0.25f);
                if (geo.mine_prog >= 1.0f) {
                    geo_spawn_particles((float)geo.bx + 0.5f, (float)geo.by + 0.5f, (float)geo.bz + 0.5f, b, 14);
                    geo_set_block(geo.bx, geo.by, geo.bz, GEO_AIR, 1);
                    geo_play_sfx("sounds/block_break.wav", 0.9f);
                    geo.mine_on = 0;
                    geo.mine_prog = 0.0f;
                    if (!geo.online)
                        geo_solo_save();
                }
            }
        } else {
            geo.mine_on = 0;
        }
    } else {
        geo.mine_on = 0;
        geo.mine_prog = 0.0f;
    }

    if (want_place && !geo.inv_open && !geo.chat_open && !geo.pause_open) {
        geo.place_tap_queued = 0;
        geo.place_cd = 0.24f;
        (void)geo_try_place_block();
    }

    /* Update primed demolition charges & particles */
    for (i = 0; i < GEO_MAX_CHARGES; i++) {
        if (!geo.charges[i].active)
            continue;
        geo.charges[i].vy -= 18.0f * fdt;
        ny = geo.charges[i].y + geo.charges[i].vy * fdt;
        if (!geo_aabb_hits_solid(geo.charges[i].x, ny, geo.charges[i].z, 0.44f, 0.92f))
            geo.charges[i].y = ny;
        else
            geo.charges[i].vy = 0.0f;
        geo.charges[i].fuse -= fdt;
        if (geo.charges[i].fuse <= 0.0f) {
            float ex = geo.charges[i].x, ey = geo.charges[i].y + 0.5f, ez = geo.charges[i].z;
            geo.charges[i].active = 0;
            geo_detonate_at(ex, ey, ez, 3.4f);
            if (!geo.online)
                geo_solo_save();
        }
    }
    for (i = 0; i < GEO_MAX_PARTICLES; i++) {
        if (!geo.parts[i].active)
            continue;
        geo.parts[i].vy -= 16.0f * fdt;
        geo.parts[i].x += geo.parts[i].vx * fdt;
        geo.parts[i].y += geo.parts[i].vy * fdt;
        geo.parts[i].z += geo.parts[i].vz * fdt;
        geo.parts[i].life -= fdt;
        if (geo.parts[i].life <= 0.0f)
            geo.parts[i].active = 0;
    }

    /* Publish local 3D player state & smoothly interpolate all remote players */
    if (geo.online) {
        GeoRemotePeer peers[GEO_MAX_REMOTES];
        int pn, r;
        net_publish_local(geo.px, geo.py, geo.pz, geo.yaw, geo.pitch, (float)geo.swing_seq, geo.hotbar[geo.slot],
                          geo.flying);
        pn = net_copy_remotes(peers, GEO_MAX_REMOTES);
        for (r = 0; r < GEO_MAX_REMOTES; r++)
            geo.rem[r].active = 0;
        for (i = 0; i < pn; i++) {
            GeoRemoteAvatar *av = &geo.rem[i];
            float dx = peers[i].x - av->x, dy = peers[i].y - av->y, dz = peers[i].z - av->z;
            int fresh = strcmp(av->uid, peers[i].uid) != 0;
            snprintf(av->uid, sizeof(av->uid), "%s", peers[i].uid);
            snprintf(av->name, sizeof(av->name), "%s", peers[i].name);
            av->active = 1;
            av->held = peers[i].held;
            if (fresh || (dx * dx + dy * dy + dz * dz) > 64.0f) {
                av->x = peers[i].x;
                av->y = peers[i].y;
                av->z = peers[i].z;
                av->yaw = peers[i].yaw;
                av->pitch = peers[i].pitch;
                av->last_swing = peers[i].swing;
            } else {
                float k = fdt * 10.5f > 1.0f ? 1.0f : fdt * 10.5f;
                float dyaw = peers[i].yaw - av->yaw;
                while (dyaw > 3.14159265f)
                    dyaw -= 6.2831853f;
                while (dyaw < -3.14159265f)
                    dyaw += 6.2831853f;
                if (fabsf(dx) + fabsf(dz) > 0.015f)
                    av->walk_t += fdt * 8.5f;
                av->x += dx * k;
                av->y += dy * k;
                av->z += dz * k;
                av->yaw += dyaw * k;
                av->pitch += (peers[i].pitch - av->pitch) * k;
                if (peers[i].swing != av->last_swing) {
                    av->last_swing = peers[i].swing;
                    av->swing_t = 1.0f;
                }
            }
            if (av->swing_t > 0.0f) {
                av->swing_t -= fdt * 4.0f;
                if (av->swing_t < 0.0f)
                    av->swing_t = 0.0f;
            }
        }
    }
}
