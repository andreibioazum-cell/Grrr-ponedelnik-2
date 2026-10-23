/* Draws an oriented 3D rectangular prism rotated by `yaw` + local pitch around `piv`.
 * Used for articulated 6-box 3D Minecraft characters, held 3D blocks, and primed TNT! */
static void mc_draw_oriented_box(float px, float py, float pz, float yaw, float pitch_loc, float ox, float oy,
                                 float oz, float sx, float sy, float sz, int top_t, int front_t, int side_t,
                                 float tint_red) {
    float cy = cosf(yaw), syw = sinf(yaw), cp = cosf(pitch_loc), sp = sinf(pitch_loc);
    float corners[8][3];
    static const int faces[6][4] = {
        {4, 5, 6, 7}, /* +Y top */
        {3, 2, 1, 0}, /* -Y bot */
        {7, 6, 2, 3}, /* +Z front */
        {5, 4, 0, 1}, /* -Z back */
        {6, 5, 1, 2}, /* +X side */
        {4, 7, 3, 0}  /* -X side */
    };
    static const float shade[6] = {1.0f, 0.52f, 0.88f, 0.74f, 0.70f, 0.70f};
    int i, f;
    for (i = 0; i < 8; i++) {
        float lx = ox + ((i & 1) ? sx * 0.5f : -sx * 0.5f);
        float ly = oy + ((i & 4) ? sy : 0.0f);
        float lz = oz + ((i & 2) ? sz * 0.5f : -sz * 0.5f);
        /* Local X-axis limb/head pitch rotation */
        float ry_p = ly * cp - lz * sp;
        float rz_p = ly * sp + lz * cp;
        corners[i][0] = px + lx * cy + rz_p * syw;
        corners[i][1] = py + ry_p;
        corners[i][2] = pz - lx * syw + rz_p * cy;
    }

    for (f = 0; f < 6; f++) {
        const int *fv = faces[f];
        /* Back-face cull in world space against camera position: exact convex box visibility! */
        float ax = corners[fv[1]][0] - corners[fv[0]][0], ay = corners[fv[1]][1] - corners[fv[0]][1],
              az = corners[fv[1]][2] - corners[fv[0]][2];
        float bx2 = corners[fv[3]][0] - corners[fv[0]][0], by2 = corners[fv[3]][1] - corners[fv[0]][1],
              bz2 = corners[fv[3]][2] - corners[fv[0]][2];
        float nx = ay * bz2 - az * by2, ny = az * bx2 - ax * bz2, nz = ax * by2 - ay * bx2;
        float vx = mc.cam_x - corners[fv[0]][0], vy = mc.cam_y - corners[fv[0]][1], vz = mc.cam_z - corners[fv[0]][2];
        float wx[4], wy[4], wz[4], vl[4];
        int tile = (f <= 1) ? top_t : (f == 2 ? front_t : side_t);
        if (nx * vx + ny * vy + nz * vz >= 0.0f)
            continue;
        for (i = 0; i < 4; i++) {
            wx[i] = corners[fv[i]][0];
            wy[i] = corners[fv[i]][1];
            wz[i] = corners[fv[i]][2];
            vl[i] = shade[f] * (tint_red > 0.0f ? 1.15f : 1.0f);
        }
        mc_draw_quad3d(wx, wy, wz, tile, vl, 255.0f, tint_red > 0.0f ? -0.35f : 0.0f);
    }
}

static void mc_draw_character3d(float x, float y, float z, float yaw, float head_pitch, float walk_t, float swing_t,
                                int sneaking, int cls, int skin, int held_block, float hurt_t, const char *nick,
                                float hp, float max_hp, const char *bubble_txt) {
    typedef struct {
        float px, py, pz, yw, pt, ox, oy, oz, sx, sy, sz, d2;
        int tt, ft, st;
        float hurt;
    } McBoxPart;
    McBoxPart parts[7];
    int pn = 0, i, j, face_t, body_t;
    float leg_ang = sinf(walk_t * 6.5f) * 0.65f * ((fabsf(walk_t) > 0.001f) ? 1.0f : 0.0f);
    float arm_r = -leg_ang - sinf(swing_t * 3.14159f) * 1.25f;
    float crouch = sneaking ? 0.16f : 0.0f;
    float sx, sy, depth;

    mc_skin_tiles_for(cls, skin, &face_t, &body_t);
    parts[pn++] = (McBoxPart){x, y + 0.72f - crouch, z, yaw, leg_ang, -0.13f, -0.72f, 0.0f, 0.24f, 0.72f, 0.24f, 0.f,
                              body_t, body_t, body_t, hurt_t};
    parts[pn++] = (McBoxPart){x, y + 0.72f - crouch, z, yaw, -leg_ang, 0.13f, -0.72f, 0.0f, 0.24f, 0.72f, 0.24f, 0.f,
                              body_t, body_t, body_t, hurt_t};
    parts[pn++] = (McBoxPart){x, y + 0.72f - crouch, z, yaw, sneaking ? 0.20f : 0.0f, 0.0f, 0.0f, 0.0f, 0.50f, 0.70f,
                              0.26f, 0.f, body_t, body_t, body_t, hurt_t};
    parts[pn++] = (McBoxPart){x, y + 1.40f - crouch, z, yaw, -leg_ang, -0.37f, -0.68f, 0.0f, 0.22f, 0.68f, 0.22f, 0.f,
                              body_t, body_t, body_t, hurt_t};
    parts[pn++] = (McBoxPart){x, y + 1.40f - crouch, z, yaw, arm_r, 0.37f, -0.68f, 0.0f, 0.22f, 0.68f, 0.22f, 0.f,
                              body_t, body_t, body_t, hurt_t};
    parts[pn++] = (McBoxPart){x, y + 1.42f - crouch, z, yaw, -head_pitch, 0.0f, 0.0f, 0.0f, 0.46f, 0.46f, 0.46f, 0.f,
                              body_t, face_t, body_t, hurt_t};
    if (held_block > 0 && held_block < MC_TOTAL_TYPES) {
        float hx = x - 0.38f * cosf(yaw) + 0.38f * sinf(yaw);
        float hz = z + 0.38f * sinf(yaw) + 0.38f * cosf(yaw);
        parts[pn++] = (McBoxPart){hx, y + 0.78f - crouch, hz, yaw + 0.45f, 0.0f, 0.0f, 0.0f, 0.0f, 0.26f, 0.26f, 0.26f,
                                  0.f, mc_face_tile((unsigned char)held_block, 0),
                                  mc_face_tile((unsigned char)held_block, 2),
                                  mc_face_tile((unsigned char)held_block, 4), 0.0f};
    }
    for (i = 0; i < pn; i++) {
        float cyw = cosf(parts[i].yw), syw = sinf(parts[i].yw);
        float lyc = parts[i].oy + parts[i].sy * 0.5f;
        float wcx = parts[i].px + parts[i].ox * cyw + parts[i].oz * syw;
        float wcy = parts[i].py + lyc;
        float wcz = parts[i].pz - parts[i].ox * syw + parts[i].oz * cyw;
        float dx = wcx - mc.cam_x, dy = wcy - mc.cam_y, dz = wcz - mc.cam_z;
        parts[i].d2 = dx * dx + dy * dy + dz * dz;
    }
    for (i = 1; i < pn; i++) {
        McBoxPart k = parts[i];
        for (j = i - 1; j >= 0 && parts[j].d2 < k.d2; j--)
            parts[j + 1] = parts[j];
        parts[j + 1] = k;
    }
    for (i = 0; i < pn; i++)
        mc_draw_oriented_box(parts[i].px, parts[i].py, parts[i].pz, parts[i].yw, parts[i].pt, parts[i].ox, parts[i].oy,
                             parts[i].oz, parts[i].sx, parts[i].sy, parts[i].sz, parts[i].tt, parts[i].ft, parts[i].st,
                             parts[i].hurt);

    /* Overhead 3D projected nameplate + HP bar + chat speech bubble */
    if (nick && nick[0] && !sneaking && mc_project_point(x, y + 2.16f, z, &sx, &sy, &depth) && depth < 26.0f) {
        float sc = 0.58f;
        float tw = (float)strlen(nick) * 11.0f * sc + 16.0f;
        rect(sx - tw * 0.5f, sy - 20.0f, tw, 24.0f, 0x96000000u);
        text_scaled(nick, sx - tw * 0.5f + 8.0f, sy - 18.0f,
                    strcmp(nick, ADMIN_NICK) == 0 ? 0xFF42F57Bu : 0xFFFFFFFFu, sc);
        if (max_hp > 0.0f) {
            float ratio = hp / max_hp;
            if (ratio < 0.0f)
                ratio = 0.0f;
            if (ratio > 1.0f)
                ratio = 1.0f;
            rect(sx - 34.0f, sy + 7.0f, 68.0f, 7.0f, 0xCC181818u);
            rect(sx - 32.0f, sy + 8.5f, 64.0f * ratio, 4.0f, ratio > 0.4f ? 0xFF38E056u : 0xFFFF4646u);
        }
        if (bubble_txt && bubble_txt[0]) {
            float bw = (float)strlen(bubble_txt) * 9.5f * 0.52f + 18.0f;
            roundrect(sx - bw * 0.5f, sy - 50.0f, bw, 25.0f, 7.0f, 0xEAFFFFFFu);
            text_scaled(bubble_txt, sx - bw * 0.5f + 9.0f, sy - 47.0f, 0xFF141820u, 0.52f);
        }
    }
}

static void mc_draw_sky_and_clouds(void) {
    float sr, sg, sb, sx, sy;
    int band, i;
    mc_sky_rgb(&sr, &sg, &sb);
    /* Crisp multi-band atmospheric sky gradient from deep zenith azure to mist horizon */
    for (band = 0; band < 8; band++) {
        float t = (float)band / 7.0f;
        float r = (sr * 0.62f) + (sr - sr * 0.62f) * t;
        float g = (sg * 0.72f) + (sg - sg * 0.72f) * t;
        float b = (sb * 0.90f) + (sb - sb * 0.90f) * t;
        float y0 = (float)screen_h * (float)band / 8.0f;
        rect(0.0f, y0, (float)screen_w, (float)screen_h / 8.0f + 2.0f, mc_pack_argb(r, g, b, 255.0f));
    }
    /* Golden 3D Minecraft Sun high in the sky */
    if (mc_project_point(mc.cam_x + 42.0f, mc.cam_y + 48.0f, mc.cam_z - 52.0f, &sx, &sy, NULL)) {
        rect(sx - 44.0f, sy - 44.0f, 88.0f, 88.0f, 0x38FFF4A8u);
        rect(sx - 26.0f, sy - 26.0f, 52.0f, 52.0f, 0xFFFFF8C4u);
    }
    /* Event 4: Giant floating Obsidian/Gold Monolith Cube in the sky! */
    if (net_event() == 4.0) {
        mc_draw_oriented_box(32.0f, 27.0f + sinf(mc.time_sec * 1.4f) * 1.2f, 22.0f, mc.time_sec * 0.45f, 0.0f, 0.0f,
                             0.0f, 0.0f, 6.0f, 6.0f, 6.0f, 20, 24, 22, 0.0f);
    }
    /* Drifting 3D white Minecraft cloud slabs */
    for (i = 0; i < 7; i++) {
        float cx = fmodf((float)(i * 19) + mc.time_sec * 0.55f, 96.0f) - 16.0f;
        float cz = (float)((i * 23) % 72) - 4.0f;
        float wx[4], wy[4], wz[4];
        float cl[4] = {0.96f, 0.96f, 0.92f, 0.92f};
        wx[0] = cx;         wy[0] = 33.5f; wz[0] = cz + 8.0f;
        wx[1] = cx + 11.0f; wy[1] = 33.5f; wz[1] = cz + 8.0f;
        wx[2] = cx + 11.0f; wy[2] = 33.5f; wz[2] = cz;
        wx[3] = cx;         wy[3] = 33.5f; wz[3] = cz;
        mc_draw_quad3d(wx, wy, wz, 15, cl, 205.0f, 0.0f);
    }
}

typedef struct {
    int kind; /* 0 = remote, 1 = npc, 2 = self 3rd person, 3 = TNT, 4 = particle */
    int idx;
    float dist2;
} McEntSort;

static void mc_render_world_scene(int draw_self_3rd) {
    int cam_ix = (int)floorf(mc.cam_x), cam_iy = (int)floorf(mc.cam_y), cam_iz = (int)floorf(mc.cam_z);
    int min_x = cam_ix - MC_RENDER_DIST, max_x = cam_ix + MC_RENDER_DIST;
    int min_z = cam_iz - MC_RENDER_DIST, max_z = cam_iz + MC_RENDER_DIST;
    int min_y = cam_iy - 16, max_y = cam_iy + 18;
    int oct, pass, i, j, crack_stage = -1;
    McEntSort ents[128];
    int ent_n = 0;

    if (min_x < 0)
        min_x = 0;
    if (max_x >= MC_WX)
        max_x = MC_WX - 1;
    if (min_z < 0)
        min_z = 0;
    if (max_z >= MC_WZ)
        max_z = MC_WZ - 1;
    if (min_y < 0)
        min_y = 0;
    if (max_y >= MC_WY)
        max_y = MC_WY - 1;

    if (mc.mine_on) {
        crack_stage = (int)(mc.mine_prog * 10.0f);
        if (crack_stage > 9)
            crack_stage = 9;
    }

    mc_draw_sky_and_clouds();

    /* Pass 0: Exact 8-octant back-to-front solid/cutout blocks; Pass 1: Translucent Water & Ice */
    for (pass = 0; pass < 2; pass++) {
        for (oct = 0; oct < 8; oct++) {
            int x_start = (oct & 1) ? min_x : max_x;
            int x_end = (oct & 1) ? cam_ix : (cam_ix + 1);
            int x_step = (oct & 1) ? 1 : -1;
            int z_start = (oct & 2) ? min_z : max_z;
            int z_end = (oct & 2) ? cam_iz : (cam_iz + 1);
            int z_step = (oct & 2) ? 1 : -1;
            int y_start = (oct & 4) ? min_y : max_y;
            int y_end = (oct & 4) ? cam_iy : (cam_iy + 1);
            int y_step = (oct & 4) ? 1 : -1;
            int x, y, z;

            for (x = x_start; (x_step > 0) ? (x <= x_end) : (x >= x_end); x += x_step) {
                for (z = z_start; (z_step > 0) ? (z <= z_end) : (z >= z_end); z += z_step) {
                    float cdx = ((float)x + 0.5f) - mc.cam_x;
                    float cdz = ((float)z + 0.5f) - mc.cam_z;
                    float d2_xz = cdx * cdx + cdz * cdz;
                    if (d2_xz > (float)(MC_RENDER_DIST * MC_RENDER_DIST))
                        continue;
                    /* Fast 3D column frustum cull when column is > 3 blocks away */
                    if (d2_xz > 9.0f) {
                        float fwd_xz = cdx * mc.fx + cdz * mc.fz;
                        if (fwd_xz < -2.4f - fabsf(mc.fy) * 18.0f)
                            continue;
                    }
                    for (y = y_start; (y_step > 0) ? (y <= y_end) : (y >= y_end); y += y_step) {
                        unsigned char b = mc.vox[y][z][x];
                        int is_trans, cs;
                        float dy, dist, fog_k;
                        if (b == MC_AIR)
                            continue;
                        is_trans = (b == MC_WATER || b == MC_ICE);
                        if (pass != is_trans)
                            continue;
                        dy = ((float)y + 0.5f) - mc.cam_y;
                        dist = sqrtf(d2_xz + dy * dy);
                        fog_k = (dist - MC_FOG_START) / (MC_FOG_END - MC_FOG_START);
                        if (fog_k < 0.0f)
                            fog_k = 0.0f;
                        else if (fog_k > 0.92f)
                            fog_k = 0.92f;
                        cs = (pass == 0 && mc.mine_on && x == mc.mx && y == mc.my && z == mc.mz) ? crack_stage : -1;

                        if (mc.cam_y > (float)y + 1.0f && mc_face_visible(b, x, y + 1, z))
                            mc_draw_block_face(x, y, z, b, 0, fog_k, cs);
                        else if (mc.cam_y < (float)y && mc_face_visible(b, x, y - 1, z))
                            mc_draw_block_face(x, y, z, b, 1, fog_k, cs);

                        if (mc.cam_z > (float)z + 1.0f && mc_face_visible(b, x, y, z + 1))
                            mc_draw_block_face(x, y, z, b, 2, fog_k, cs);
                        else if (mc.cam_z < (float)z && mc_face_visible(b, x, y, z - 1))
                            mc_draw_block_face(x, y, z, b, 3, fog_k, cs);

                        if (mc.cam_x > (float)x + 1.0f && mc_face_visible(b, x + 1, y, z))
                            mc_draw_block_face(x, y, z, b, 4, fog_k, cs);
                        else if (mc.cam_x < (float)x && mc_face_visible(b, x - 1, y, z))
                            mc_draw_block_face(x, y, z, b, 5, fog_k, cs);
                    }
                }
            }
        }

        /* Draw 3D selection wireframe + sorted 3D entities right after opaque world, BEFORE water translucency! */
        if (pass == 0) {
            if (mc.hit && game_state != ST_LOBBY)
                mc_draw_wire_cube(mc.bx, mc.by, mc.bz, 0xDF101014u);

            for (i = 0; i < 4 && ent_n < 120; i++)
                if (mc.rem[i].active) {
                    float dx = mc.rem[i].x - mc.cam_x, dy = mc.rem[i].y - mc.cam_y, dz = mc.rem[i].z - mc.cam_z;
                    ents[ent_n++] = (McEntSort){0, i, dx * dx + dy * dy + dz * dz};
                }
            for (i = 0; i < MC_MAX_NPCS && ent_n < 120; i++)
                if (mc.npcs[i].active && mc.npcs[i].hp > 0.0f) {
                    float dx = mc.npcs[i].x - mc.cam_x, dy = mc.npcs[i].y - mc.cam_y, dz = mc.npcs[i].z - mc.cam_z;
                    ents[ent_n++] = (McEntSort){1, i, dx * dx + dy * dy + dz * dz};
                }
            if (draw_self_3rd && ent_n < 120) {
                float dx = mc.px - mc.cam_x, dy = mc.py - mc.cam_y, dz = mc.pz - mc.cam_z;
                ents[ent_n++] = (McEntSort){2, 0, dx * dx + dy * dy + dz * dz};
            }
            for (i = 0; i < MC_MAX_TNT && ent_n < 120; i++)
                if (mc.tnts[i].active) {
                    float dx = mc.tnts[i].x - mc.cam_x, dy = mc.tnts[i].y - mc.cam_y, dz = mc.tnts[i].z - mc.cam_z;
                    ents[ent_n++] = (McEntSort){3, i, dx * dx + dy * dy + dz * dz};
                }
            for (i = 0; i < MC_MAX_PARTICLES && ent_n < 126; i++)
                if (mc.parts[i].active) {
                    float dx = mc.parts[i].x - mc.cam_x, dy = mc.parts[i].y - mc.cam_y, dz = mc.parts[i].z - mc.cam_z;
                    ents[ent_n++] = (McEntSort){4, i, dx * dx + dy * dy + dz * dz};
                }
            /* Sort back-to-front by squared distance */
            for (i = 1; i < ent_n; i++) {
                McEntSort key = ents[i];
                for (j = i - 1; j >= 0 && ents[j].dist2 < key.dist2; j--)
                    ents[j + 1] = ents[j];
                ents[j + 1] = key;
            }
            for (i = 0; i < ent_n; i++) {
                int idx = ents[i].idx;
                if (ents[i].kind == 0) {
                    const char *bub = NULL;
                    int cnt = (int)net_chat_count();
                    if (cnt > 0)
                        bub = net_chat_text((double)(cnt - 1));
                    mc_draw_character3d(mc.rem[idx].x, mc.rem[idx].y, mc.rem[idx].z, mc.rem[idx].yaw,
                                        mc.rem[idx].pitch, mc.rem[idx].walk_t, mc.rem[idx].swing_t,
                                        mc.rem[idx].sneaking, (int)net_player_class((double)idx),
                                        (int)net_player_skin((double)idx), mc.rem[idx].held_block,
                                        mc.rem[idx].hurt_t, net_player_nick((double)idx),
                                        (float)net_player_hp((double)idx), 10.0f, bub);
                } else if (ents[i].kind == 1) {
                    mc_draw_character3d(mc.npcs[idx].x, mc.npcs[idx].y, mc.npcs[idx].z, mc.npcs[idx].yaw, 0.0f,
                                        mc.npcs[idx].walk_t, mc.npcs[idx].swing_t, 0, mc.npcs[idx].skin_idx / 10,
                                        mc.npcs[idx].skin_idx % 10, mc.npcs[idx].held_block, mc.npcs[idx].hurt_t,
                                        mc.npcs[idx].name, mc.npcs[idx].hp, mc.npcs[idx].max_hp, NULL);
                } else if (ents[i].kind == 2) {
                    mc_draw_character3d(mc.px, mc.py, mc.pz, mc.yaw, mc.pitch, mc.bob_amp > 0.05f ? mc.walk_dist : 0.0f,
                                        mc.swing_t, mc.sneaking, (int)player_class, (int)current_skin(),
                                        mc.hotbar[mc.slot], mc.hurt_t, net_login_nick(), (float)player->hp,
                                        (float)player->max_hp, NULL);
                } else if (ents[i].kind == 3) {
                    int flash = ((int)(mc.tnts[idx].fuse * 10.0f) & 1) == 0;
                    float swell = 1.0f + (mc.tnts[idx].fuse < 0.8f ? (0.8f - mc.tnts[idx].fuse) * 0.28f : 0.0f);
                    mc_draw_oriented_box(mc.tnts[idx].x, mc.tnts[idx].y, mc.tnts[idx].z, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         0.98f * swell, 0.98f * swell, 0.98f * swell, flash ? 35 : 29, flash ? 35 : 30,
                                         flash ? 35 : 30, 0.0f);
                } else {
                    float sxp, syp, dep;
                    if (mc_project_point(mc.parts[idx].x, mc.parts[idx].y, mc.parts[idx].z, &sxp, &syp, &dep) &&
                        dep > 0.1f) {
                        float psz = (mc.parts[idx].size * (float)screen_h) / (dep * 2.2f);
                        uint32_t pcol = mc_blocks[mc.parts[idx].tile < MC_TOTAL_TYPES ? mc.parts[idx].tile : 1].particle_col;
                        if (psz > 2.0f && psz < 42.0f)
                            rect(sxp - psz * 0.5f, syp - psz * 0.5f, psz, psz, pcol);
                    }
                }
            }
        }
    }
}

/* Draws classic 1st-person 3D held block in bottom-right corner with stride bob & swing! */
static void mc_draw_first_person_hand(void) {
    int b = mc.hotbar[mc.slot];
    float swing = sinf(mc.swing_t * 3.14159f);
    float bob_x = sinf(mc.walk_dist * 2.6f) * 0.035f * mc.bob_amp;
    float bob_y = fabsf(cosf(mc.walk_dist * 2.6f)) * 0.032f * mc.bob_amp;
    float hx, hy, hz;
    if (mc.third_person || b <= 0 || b >= MC_TOTAL_TYPES)
        return;
    hx = mc.cam_x + mc.fx * (0.76f - swing * 0.14f) + mc.rx * (0.42f + bob_x - swing * 0.12f) +
         mc.ux * (-0.28f - bob_y + swing * 0.06f);
    hy = mc.cam_y + mc.fy * (0.76f - swing * 0.14f) + mc.ry * (0.42f + bob_x - swing * 0.12f) +
         mc.uy * (-0.28f - bob_y + swing * 0.06f);
    hz = mc.cam_z + mc.fz * (0.76f - swing * 0.14f) + mc.rz * (0.42f + bob_x - swing * 0.12f) +
         mc.uz * (-0.28f - bob_y + swing * 0.06f);
    mc_draw_oriented_box(hx, hy, hz, mc.yaw + 0.72f - swing * 0.45f, -mc.pitch * 0.5f + 0.22f + swing * 0.55f, 0.0f,
                         -0.15f, 0.0f, 0.28f, 0.28f, 0.28f, mc_face_tile((unsigned char)b, 0),
                         mc_face_tile((unsigned char)b, 2), mc_face_tile((unsigned char)b, 4), 0.0f);
}
