/* Unified Single-Pass True 3D Back-to-Front Painter's Renderer for Geometrium.
 * Every voxel (opaque blocks, water, glass, prism ice), every 3D player avatar,
 * every demolition charge, and every particle is drawn strictly from farthest cell
 * to nearest cell in one unified 8-octant pass — eliminating water/entity X-ray completely! */

static int geo_ao_occ(int x, int y, int z) { return geo_is_opaque_id(geo_get(x, y, z)); }

static float geo_vertex_ao(int s1, int s2, int corner) {
    if (s1 && s2)
        return 0.44f;
    return 1.0f - (float)(s1 + s2 + corner) * 0.16f;
}

static float geo_point_light(int x, int y, int z) {
    if (!geo_in_bounds(x, y, z))
        return 0.0f;
    return (float)geo.light_warm[y][z][x] / 255.0f * 0.62f;
}

static int geo_face_visible(unsigned char self_b, int nx, int ny, int nz) {
    unsigned char nb = geo_get(nx, ny, nz);
    if (nb == GEO_AIR)
        return 1;
    if (nb == self_b && (self_b == GEO_WATER || self_b == GEO_GLASS || self_b == GEO_ICE))
        return 0;
    return !geo_is_opaque_id(nb);
}

static void geo_draw_block_face(int x, int y, int z, unsigned char b, int face, float fog_k, int crack_stage) {
    float fx = (float)x, fy = (float)y, fz = (float)z;
    float y_top = (b == GEO_WATER) ? (fy + 0.88f) : (fy + 1.0f);
    const char *tex_name = geo_face_tex(b, face);
    float wx[4], wy[4], wz[4], vl[4];
    float alpha = (b == GEO_WATER) ? 182.0f : (b == GEO_GLASS ? 215.0f : (b == GEO_ICE ? 222.0f : 255.0f));
    float base_sun = 1.0f;
    float glow = ((geo_blocks[b].flags & (GEO_F_WARM_LIGHT | GEO_F_COOL_LIGHT)) != 0) ? 1.0f : 0.0f;
    int i, ox = 0, oy = 0, oz = 0;

    if (face == 0) {
        oy = 1;
        base_sun = 1.0f;
        wx[0] = fx;       wy[0] = y_top; wz[0] = fz;
        wx[1] = fx + 1.f; wy[1] = y_top; wz[1] = fz;
        wx[2] = fx + 1.f; wy[2] = y_top; wz[2] = fz + 1.f;
        wx[3] = fx;       wy[3] = y_top; wz[3] = fz + 1.f;
        vl[0] = geo_vertex_ao(geo_ao_occ(x - 1, y + 1, z), geo_ao_occ(x, y + 1, z - 1), geo_ao_occ(x - 1, y + 1, z - 1));
        vl[1] = geo_vertex_ao(geo_ao_occ(x + 1, y + 1, z), geo_ao_occ(x, y + 1, z - 1), geo_ao_occ(x + 1, y + 1, z - 1));
        vl[2] = geo_vertex_ao(geo_ao_occ(x + 1, y + 1, z), geo_ao_occ(x, y + 1, z + 1), geo_ao_occ(x + 1, y + 1, z + 1));
        vl[3] = geo_vertex_ao(geo_ao_occ(x - 1, y + 1, z), geo_ao_occ(x, y + 1, z + 1), geo_ao_occ(x - 1, y + 1, z + 1));
    } else if (face == 1) {
        oy = -1;
        base_sun = 0.50f;
        wx[0] = fx;       wy[0] = fy; wz[0] = fz + 1.f;
        wx[1] = fx + 1.f; wy[1] = fy; wz[1] = fz + 1.f;
        wx[2] = fx + 1.f; wy[2] = fy; wz[2] = fz;
        wx[3] = fx;       wy[3] = fy; wz[3] = fz;
        vl[0] = vl[1] = vl[2] = vl[3] = 0.90f;
    } else if (face == 2) {
        oz = 1;
        base_sun = 0.86f;
        wx[0] = fx;       wy[0] = y_top; wz[0] = fz + 1.f;
        wx[1] = fx + 1.f; wy[1] = y_top; wz[1] = fz + 1.f;
        wx[2] = fx + 1.f; wy[2] = fy;    wz[2] = fz + 1.f;
        wx[3] = fx;       wy[3] = fy;    wz[3] = fz + 1.f;
        vl[0] = geo_vertex_ao(geo_ao_occ(x - 1, y, z + 1), geo_ao_occ(x, y + 1, z + 1), geo_ao_occ(x - 1, y + 1, z + 1));
        vl[1] = geo_vertex_ao(geo_ao_occ(x + 1, y, z + 1), geo_ao_occ(x, y + 1, z + 1), geo_ao_occ(x + 1, y + 1, z + 1));
        vl[2] = geo_vertex_ao(geo_ao_occ(x + 1, y, z + 1), geo_ao_occ(x, y - 1, z + 1), geo_ao_occ(x + 1, y - 1, z + 1));
        vl[3] = geo_vertex_ao(geo_ao_occ(x - 1, y, z + 1), geo_ao_occ(x, y - 1, z + 1), geo_ao_occ(x - 1, y - 1, z + 1));
    } else if (face == 3) {
        oz = -1;
        base_sun = 0.78f;
        wx[0] = fx + 1.f; wy[0] = y_top; wz[0] = fz;
        wx[1] = fx;       wy[1] = y_top; wz[1] = fz;
        wx[2] = fx;       wy[2] = fy;    wz[2] = fz;
        wx[3] = fx + 1.f; wy[3] = fy;    wz[3] = fz;
        vl[0] = geo_vertex_ao(geo_ao_occ(x + 1, y, z - 1), geo_ao_occ(x, y + 1, z - 1), geo_ao_occ(x + 1, y + 1, z - 1));
        vl[1] = geo_vertex_ao(geo_ao_occ(x - 1, y, z - 1), geo_ao_occ(x, y + 1, z - 1), geo_ao_occ(x - 1, y + 1, z - 1));
        vl[2] = geo_vertex_ao(geo_ao_occ(x - 1, y, z - 1), geo_ao_occ(x, y - 1, z - 1), geo_ao_occ(x - 1, y - 1, z - 1));
        vl[3] = geo_vertex_ao(geo_ao_occ(x + 1, y, z - 1), geo_ao_occ(x, y - 1, z - 1), geo_ao_occ(x + 1, y - 1, z - 1));
    } else if (face == 4) {
        ox = 1;
        base_sun = 0.70f;
        wx[0] = fx + 1.f; wy[0] = y_top; wz[0] = fz + 1.f;
        wx[1] = fx + 1.f; wy[1] = y_top; wz[1] = fz;
        wx[2] = fx + 1.f; wy[2] = fy;    wz[2] = fz;
        wx[3] = fx + 1.f; wy[3] = fy;    wz[3] = fz + 1.f;
        vl[0] = geo_vertex_ao(geo_ao_occ(x + 1, y, z + 1), geo_ao_occ(x + 1, y + 1, z), geo_ao_occ(x + 1, y + 1, z + 1));
        vl[1] = geo_vertex_ao(geo_ao_occ(x + 1, y, z - 1), geo_ao_occ(x + 1, y + 1, z), geo_ao_occ(x + 1, y + 1, z - 1));
        vl[2] = geo_vertex_ao(geo_ao_occ(x + 1, y, z - 1), geo_ao_occ(x + 1, y - 1, z), geo_ao_occ(x + 1, y - 1, z - 1));
        vl[3] = geo_vertex_ao(geo_ao_occ(x + 1, y, z + 1), geo_ao_occ(x + 1, y - 1, z), geo_ao_occ(x + 1, y - 1, z + 1));
    } else {
        ox = -1;
        base_sun = 0.66f;
        wx[0] = fx; wy[0] = y_top; wz[0] = fz;
        wx[1] = fx; wy[1] = y_top; wz[1] = fz + 1.f;
        wx[2] = fx; wy[2] = fy;    wz[2] = fz + 1.f;
        wx[3] = fx; wy[3] = fy;    wz[3] = fz;
        vl[0] = geo_vertex_ao(geo_ao_occ(x - 1, y, z - 1), geo_ao_occ(x - 1, y + 1, z), geo_ao_occ(x - 1, y + 1, z - 1));
        vl[1] = geo_vertex_ao(geo_ao_occ(x - 1, y, z + 1), geo_ao_occ(x - 1, y + 1, z), geo_ao_occ(x - 1, y + 1, z + 1));
        vl[2] = geo_vertex_ao(geo_ao_occ(x - 1, y, z + 1), geo_ao_occ(x - 1, y - 1, z), geo_ao_occ(x - 1, y - 1, z - 1));
        vl[3] = geo_vertex_ao(geo_ao_occ(x - 1, y, z - 1), geo_ao_occ(x - 1, y - 1, z), geo_ao_occ(x - 1, y - 1, z - 1));
    }

    if (glow == 0.0f && fog_k < 0.6f)
        glow = geo_point_light(x + ox, y + oy, z + oz);
    for (i = 0; i < 4; i++) {
        float lit = vl[i] * base_sun + glow;
        vl[i] = lit > 1.0f ? 1.0f : lit;
    }
    geo_draw_quad3d(wx, wy, wz, tex_name, vl, alpha, fog_k);

    if (crack_stage >= 0 && crack_stage <= 9) {
        char crack_tex[24];
        float cl[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        snprintf(crack_tex, sizeof(crack_tex), "crack_%d.png", crack_stage);
        geo_draw_quad3d(wx, wy, wz, crack_tex, cl, 235.0f, 0.0f);
    }
}

static void geo_draw_line3d(float x0, float y0, float z0, float x1, float y1, float z1, float thick, uint32_t col) {
    float sx0, sy0, sx1, sy1, d0, d1;
    if (geo_project_point(x0, y0, z0, &sx0, &sy0, &d0) && geo_project_point(x1, y1, z1, &sx1, &sy1, &d1))
        line(sx0, sy0, sx1, sy1, thick, col);
}

static void geo_draw_wire_cube(int bx, int by, int bz) {
    static const int edges[12][2] = {{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{3,7},{2,6}};
    float e = 0.008f, lo[3], hi[3];
    int i, a, b;
    lo[0] = (float)bx - e; lo[1] = (float)by - e; lo[2] = (float)bz - e;
    hi[0] = (float)bx + 1.0f + e; hi[1] = (float)by + 1.0f + e; hi[2] = (float)bz + 1.0f + e;
    for (i = 0; i < 12; i++) {
        a = edges[i][0]; b = edges[i][1];
        geo_draw_line3d((a & 1) ? hi[0] : lo[0], (a & 2) ? hi[1] : lo[1], (a & 4) ? hi[2] : lo[2],
                        (b & 1) ? hi[0] : lo[0], (b & 2) ? hi[1] : lo[1], (b & 4) ? hi[2] : lo[2],
                        2.2f, geo_gray(240, 210));
    }
}

static void geo_draw_oriented_box(float px, float py, float pz, float yaw, float pitch_loc, float ox, float oy,
                                  float oz, float hx, float hy, float hz, const char *tex_name, float fog_k) {
    static const int fv[6][4] = {{0, 1, 2, 3}, {7, 6, 5, 4}, {3, 2, 6, 7},
                                 {1, 0, 4, 5}, {2, 1, 5, 6}, {0, 3, 7, 4}};
    static const float fshade[6] = {1.0f, 0.55f, 0.88f, 0.78f, 0.72f, 0.68f};
    float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch_loc), sp = sinf(pitch_loc);
    float corners[8][3];
    int i, f, k;

    for (i = 0; i < 8; i++) {
        float lx = (i & 1) ? hx : -hx;
        float ly = (i & 4) ? 0.0f : (2.0f * hy);
        float lz = (i & 2) ? hz : -hz;
        float ry = (ly - hy) * cp - lz * sp;
        float rz = (ly - hy) * sp + lz * cp;
        float tx = ox + lx, ty = oy + ry, tz = oz + rz;
        corners[i][0] = px + tx * cy + tz * sy;
        corners[i][1] = py + ty;
        corners[i][2] = pz - tx * sy + tz * cy;
    }
    for (f = 0; f < 6; f++) {
        float wx[4], wy[4], wz[4], vl[4];
        float ax, ay, az, bx, by, bz, nx, ny, nz, vx, vy, vz;
        for (k = 0; k < 4; k++) {
            int vi = fv[f][k];
            wx[k] = corners[vi][0];
            wy[k] = corners[vi][1];
            wz[k] = corners[vi][2];
            vl[k] = fshade[f];
        }
        ax = wx[1] - wx[0]; ay = wy[1] - wy[0]; az = wz[1] - wz[0];
        bx = wx[3] - wx[0]; by = wy[3] - wy[0]; bz = wz[3] - wz[0];
        nx = ay * bz - az * by;
        ny = az * bx - ax * bz;
        nz = ax * by - ay * bx;
        vx = wx[0] - geo.cam_x;
        vy = wy[0] - geo.cam_y;
        vz = wz[0] - geo.cam_z;
        /* Inward normal from (v1-v0)x(v3-v0) requires dot(N_in, P - cam) >= 0 for front-facing faces! */
        if (nx * vx + ny * vy + nz * vz >= 0.0f)
            geo_draw_quad3d(wx, wy, wz, tex_name, vl, 255.0f, fog_k);
    }
}

/* Roblox-style avatar: data-driven model (geo_models[]) posed by an animation
 * clip (geo_anim_sample); materials come from the player's 4 slots. */
static void geo_draw_avatar3d(float x, float y, float z, float yaw, float pitch, float walk_t, float swing_t,
                              int held_block, const char *name_tag, const char *bubble,
                              int model_id, const int skin[4], int anim_id, float anim_t) {
    float dx = x - geo.cam_x, dy = y - geo.cam_y, dz = z - geo.cam_z;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    float fog_k = (dist - GEO_FOG_START) / (GEO_FOG_END - GEO_FOG_START);
    float pose[GJ_COUNT];
    const GeoModelDef *m;
    float leg_l, leg_r, arm_l, arm_r, bob, torso_y, head_y;
    float sx, sy, depth;
    (void)walk_t;

    fog_k = fog_k < 0.0f ? 0.0f : (fog_k > 0.90f ? 0.90f : fog_k);
    if (model_id < 0 || model_id >= GEO_MODEL_COUNT)
        model_id = GEO_MODEL_CLASSIC;
    m = &geo_models[model_id];

    geo_anim_sample(anim_id, anim_t, pose);
    /* A mining swing overrides the right arm, like before. */
    if (swing_t > 0.0f)
        pose[GJ_ARM_R] = -sinf(swing_t * 3.14159f) * 1.35f - pose[GJ_LEG_L] * 0.4f;
    leg_l = pose[GJ_LEG_L];
    leg_r = pose[GJ_LEG_R];
    arm_l = pose[GJ_ARM_L];
    arm_r = pose[GJ_ARM_R];
    bob = pose[GJ_BOB];
    torso_y = m->leg_h + 0.33f + bob;
    head_y = m->leg_h + 0.72f + m->head + bob;

    geo_draw_oriented_box(x, y, z, yaw, leg_l, -0.14f, m->leg_h * 0.5f + bob, 0.0f, 0.12f, m->leg_h * 0.5f, 0.12f,
                          geo_skin_tex[skin[SLOT_LEGS] & 7], fog_k);
    geo_draw_oriented_box(x, y, z, yaw, leg_r, 0.14f, m->leg_h * 0.5f + bob, 0.0f, 0.12f, m->leg_h * 0.5f, 0.12f,
                          geo_skin_tex[skin[SLOT_LEGS] & 7], fog_k);
    geo_draw_oriented_box(x, y, z, yaw, pose[GJ_BODY], 0.0f, torso_y, 0.0f, m->torso_w, 0.34f, 0.14f,
                          geo_skin_tex[skin[SLOT_TORSO] & 7], fog_k);
    geo_draw_oriented_box(x, y, z, yaw, arm_l, -(m->torso_w + 0.12f), torso_y + 0.06f, 0.0f, 0.11f, m->arm_len * 0.5f,
                          0.11f, geo_skin_tex[skin[SLOT_ARMS] & 7], fog_k);
    geo_draw_oriented_box(x, y, z, yaw, arm_r, (m->torso_w + 0.12f), torso_y + 0.06f, 0.0f, 0.11f, m->arm_len * 0.5f,
                          0.11f, geo_skin_tex[skin[SLOT_ARMS] & 7], fog_k);
    if (held_block > 0 && held_block < GEO_BLOCK_COUNT) {
        geo_draw_oriented_box(x, y, z, yaw, arm_r, (m->torso_w + 0.13f), torso_y - 0.28f, 0.22f, 0.14f, 0.14f, 0.14f,
                              geo_face_tex((unsigned char)held_block, 2), fog_k);
    }
    geo_draw_oriented_box(x, y, z, yaw, -pitch + pose[GJ_HEAD], 0.0f, head_y, 0.0f, m->head, m->head, m->head,
                          geo_skin_tex[skin[SLOT_HEAD] & 7], fog_k);
    if (m->hat == 1) {          /* cap */
        geo_draw_oriented_box(x, y, z, yaw, -pitch, 0.0f, head_y + m->head + 0.05f, 0.0f, m->head + 0.05f, 0.05f,
                              m->head + 0.05f, geo_skin_tex[skin[SLOT_TORSO] & 7], fog_k);
    } else if (m->hat == 2) {   /* knight helm */
        geo_draw_oriented_box(x, y, z, yaw, -pitch, 0.0f, head_y + m->head + 0.07f, 0.0f, m->head + 0.03f, 0.07f,
                              m->head + 0.03f, "steel_plate.png", fog_k);
    } else if (m->hat == 3) {   /* robot antenna */
        geo_draw_oriented_box(x, y, z, yaw, 0.0f, 0.0f, head_y + m->head + 0.16f, 0.0f, 0.03f, 0.16f, 0.03f,
                              "cyan_lamp.png", fog_k);
    }

    if (name_tag && name_tag[0] && dist < 34.0f && geo_project_point(x, y + 2.15f, z, &sx, &sy, &depth)) {
        float nw = (float)text_ink_width(name_tag) * 0.56f + 22.0f;
        roundrect(sx - nw * 0.5f, sy - 17.0f, nw, 32.0f, 16.0f, geo_gray(24, 195));
        geo_text_center(name_tag, sx, sy, geo_gray(240, 245), 0.56f);
        if (bubble && bubble[0]) {
            float bw = (float)text_ink_width(bubble) * 0.52f + 26.0f;
            roundrect(sx - bw * 0.5f, sy - 56.0f, bw, 32.0f, 16.0f, geo_gray(46, 228));
            geo_text_center(bubble, sx, sy - 40.0f, geo_gray(248, 255), 0.52f);
        }
    }
}

static void geo_draw_sky_dome(void) {
    float w = (float)screen_w, h = (float)screen_h;
    int i;
    for (i = 0; i < 6; i++) {
        float t = (float)i / 5.0f;
        unsigned int r = (unsigned int)(116.0f + 62.0f * t), g = (unsigned int)(152.0f + 48.0f * t);
        unsigned int b = (unsigned int)(204.0f + 24.0f * t);
        rect(0.0f, h * (float)i / 6.0f, w, h / 6.0f + 2.0f, 0xFF000000u | (r << 16) | (g << 8) | b);
    }
}

typedef struct {
    int kind; /* 0 = remote player, 1 = self 3rd person, 2 = charge, 3 = particle */
    int idx;
    int cx, cy, cz;
} GeoCellEnt;

static void geo_render_scene3d(int draw_self_3rd) {
    int cam_ix = (int)floorf(geo.cam_x), cam_iy = (int)floorf(geo.cam_y), cam_iz = (int)floorf(geo.cam_z);
    int min_x = cam_ix - GEO_RENDER_DIST, max_x = cam_ix + GEO_RENDER_DIST;
    int min_z = cam_iz - GEO_RENDER_DIST, max_z = cam_iz + GEO_RENDER_DIST;
    int min_y = cam_iy - 18, max_y = cam_iy + 20;
    int crack_stage = -1, oct, i;
    GeoCellEnt ents[160];
    int ent_n = 0;
    char latest_sender[24] = "", latest_msg[96] = "";

    if (min_x < 0) min_x = 0;
    if (max_x >= GEO_WX) max_x = GEO_WX - 1;
    if (min_z < 0) min_z = 0;
    if (max_z >= GEO_WZ) max_z = GEO_WZ - 1;
    if (min_y < 0) min_y = 0;
    if (max_y >= GEO_WY) max_y = GEO_WY - 1;

    if (geo.mine_on) {
        crack_stage = (int)(geo.mine_prog * 10.0f);
        if (crack_stage > 9)
            crack_stage = 9;
    }
    if (geo.online && net_chat_count() > 0)
        net_chat_get(net_chat_count() - 1, latest_sender, sizeof(latest_sender), latest_msg, sizeof(latest_msg));

    /* Collect all dynamic 3D entities with their integer voxel anchor so they obey exact 3D wall occlusion! */
    for (i = 0; i < GEO_MAX_REMOTES && ent_n < 150; i++)
        if (geo.rem[i].active)
            ents[ent_n++] = (GeoCellEnt){0, i, (int)floorf(geo.rem[i].x), (int)floorf(geo.rem[i].y + 0.9f),
                                         (int)floorf(geo.rem[i].z)};
    if (draw_self_3rd && ent_n < 152)
        ents[ent_n++] = (GeoCellEnt){1, 0, (int)floorf(geo.px), (int)floorf(geo.py + 0.9f), (int)floorf(geo.pz)};
    for (i = 0; i < GEO_MAX_CHARGES && ent_n < 156; i++)
        if (geo.charges[i].active)
            ents[ent_n++] = (GeoCellEnt){2, i, (int)floorf(geo.charges[i].x), (int)floorf(geo.charges[i].y + 0.4f),
                                         (int)floorf(geo.charges[i].z)};
    for (i = 0; i < GEO_MAX_PARTICLES && ent_n < 160; i++)
        if (geo.parts[i].active)
            ents[ent_n++] = (GeoCellEnt){3, i, (int)floorf(geo.parts[i].x), (int)floorf(geo.parts[i].y),
                                         (int)floorf(geo.parts[i].z)};
    /* Script-spawned Workspace Parts join the same occlusion traversal */
    for (i = 0; i < GEO_MAX_PARTS && ent_n < 160; i++)
        if (geo_parts[i].active)
            ents[ent_n++] = (GeoCellEnt){4, i, (int)floorf(geo_parts[i].x), (int)floorf(geo_parts[i].y),
                                         (int)floorf(geo_parts[i].z)};

    geo_draw_sky_dome();

    /* SINGLE UNIFIED 8-OCTANT EXACT 3D PAINTER'S TRAVERSAL (oct = 0..7):
     * Every solid block, crystal water block, clear glass block, prism ice block,
     * 3D player avatar, charge, and particle is drawn strictly from farthest voxel to nearest voxel! */
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
                float cdx = ((float)x + 0.5f) - geo.cam_x;
                float cdz = ((float)z + 0.5f) - geo.cam_z;
                float d2_xz = cdx * cdx + cdz * cdz;
                if (d2_xz > (float)(GEO_RENDER_DIST * GEO_RENDER_DIST))
                    continue;
                if (d2_xz > 9.0f) {
                    float fwd_xz = cdx * geo.fx + cdz * geo.fz;
                    if (fwd_xz < -2.6f - fabsf(geo.fy) * 18.0f)
                        continue;
                }
                for (y = y_start; (y_step > 0) ? (y <= y_end) : (y >= y_end); y += y_step) {
                    unsigned char b = geo.vox[y][z][x];
                    if (b != GEO_AIR) {
                        float dy = ((float)y + 0.5f) - geo.cam_y;
                        float dist = sqrtf(d2_xz + dy * dy);
                        float fog_k = (dist - GEO_FOG_START) / (GEO_FOG_END - GEO_FOG_START);
                        float y_top = (b == GEO_WATER) ? ((float)y + 0.88f) : ((float)y + 1.0f);
                        int cs = (geo.mine_on && x == geo.mx && y == geo.my && z == geo.mz) ? crack_stage : -1;

                        if (fog_k < 0.0f) fog_k = 0.0f;
                        else if (fog_k > 0.92f) fog_k = 0.92f;

                        if (geo.cam_y > y_top && geo_face_visible(b, x, y + 1, z))
                            geo_draw_block_face(x, y, z, b, 0, fog_k, cs);
                        else if (geo.cam_y < (float)y && geo_face_visible(b, x, y - 1, z))
                            geo_draw_block_face(x, y, z, b, 1, fog_k, cs);

                        if (geo.cam_z > (float)z + 1.0f && geo_face_visible(b, x, y, z + 1))
                            geo_draw_block_face(x, y, z, b, 2, fog_k, cs);
                        else if (geo.cam_z < (float)z && geo_face_visible(b, x, y, z - 1))
                            geo_draw_block_face(x, y, z, b, 3, fog_k, cs);

                        if (geo.cam_x > (float)x + 1.0f && geo_face_visible(b, x + 1, y, z))
                            geo_draw_block_face(x, y, z, b, 4, fog_k, cs);
                        else if (geo.cam_x < (float)x && geo_face_visible(b, x - 1, y, z))
                            geo_draw_block_face(x, y, z, b, 5, fog_k, cs);
                    }

                    if (geo.hit && geo.screen == SCR_PLAY && x == geo.bx && y == geo.by && z == geo.bz)
                        geo_draw_wire_cube(x, y, z);

                    for (i = 0; i < ent_n; i++) {
                        if (ents[i].cx == x && ents[i].cy == y && ents[i].cz == z) {
                            int idx = ents[i].idx;
                            if (ents[i].kind == 0) {
                                const char *bub = (latest_sender[0] && strcmp(latest_sender, geo.rem[idx].name) == 0)
                                                      ? latest_msg
                                                      : NULL;
                                geo_draw_avatar3d(geo.rem[idx].x, geo.rem[idx].y, geo.rem[idx].z, geo.rem[idx].yaw,
                                                  geo.rem[idx].pitch, geo.rem[idx].walk_t, geo.rem[idx].swing_t,
                                                  geo.rem[idx].held, geo.rem[idx].name, bub,
                                                  geo.rem[idx].model, geo.rem[idx].skin, geo.rem[idx].anim,
                                                  geo.rem[idx].anim_t);
                            } else if (ents[i].kind == 1) {
                                geo_draw_avatar3d(geo.px, geo.py, geo.pz, geo.face_yaw, geo.pitch, geo.walk_phase,
                                                  geo.swing_anim, geo.hotbar[geo.slot], net_nick(), NULL,
                                                  geo.av_model, geo.av_skin, geo.anim_id, geo.anim_t);
                            } else if (ents[i].kind == 4) {
                                GeoPart *pt = &geo_parts[idx];
                                const char *ptex = (pt->block_id > 0 && pt->block_id < GEO_BLOCK_COUNT)
                                                       ? geo_face_tex((unsigned char)pt->block_id, 2)
                                                       : "marble_top.png";
                                geo_draw_oriented_box(pt->x, pt->y, pt->z, pt->yaw, 0.0f, 0.0f, 0.0f, 0.0f,
                                                      pt->sx * 0.5f, pt->sy * 0.5f, pt->sz * 0.5f, ptex, 0.0f);
                            } else if (ents[i].kind == 2) {
                                const char *ctex = ((int)(geo.charges[idx].fuse * 7.0f) & 1) ? "lumen_core.png"
                                                                                             : "charge_side.png";
                                geo_draw_oriented_box(geo.charges[idx].x, geo.charges[idx].y + 0.48f, geo.charges[idx].z,
                                                      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.48f, 0.48f, 0.48f, ctex, 0.0f);
                            } else if (ents[i].kind == 3) {
                                geo_draw_oriented_box(geo.parts[idx].x, geo.parts[idx].y, geo.parts[idx].z,
                                                      geo.parts[idx].life * 4.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.09f, 0.09f,
                                                      0.09f, geo_face_tex(geo.parts[idx].block_id, 2), 0.0f);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void geo_draw_first_person_hand(void) {
    float swing = sinf(geo.swing_anim * 3.14159f);
    float bob = sinf(geo.walk_phase) * ((fabsf(geo.vx) + fabsf(geo.vz)) > 0.2f ? 0.025f : 0.0f);
    float hx = geo.cam_x + geo.fx * (0.64f - swing * 0.14f) + geo.rx * (0.40f - swing * 0.14f) +
               geo.ux * (-0.30f + bob - swing * 0.08f);
    float hy = geo.cam_y + geo.fy * (0.64f - swing * 0.14f) + geo.ry * (0.40f - swing * 0.14f) +
               geo.uy * (-0.30f + bob - swing * 0.08f);
    float hz = geo.cam_z + geo.fz * (0.64f - swing * 0.14f) + geo.rz * (0.40f - swing * 0.14f) +
               geo.uz * (-0.30f + bob - swing * 0.08f);
    int held = geo.hotbar[geo.slot];
    if (held > 0 && held < GEO_BLOCK_COUNT) {
        geo_draw_oriented_box(hx, hy, hz, -geo.yaw + 0.60f, -0.35f + swing * 0.7f, 0.0f, 0.0f, 0.0f, 0.13f, 0.13f,
                              0.13f, geo_face_tex((unsigned char)held, 2), 0.0f);
    }
}

static void geo_render_play_frame(void) {
    geo_setup_camera(geo.px, geo.py + GEO_EYE_H, geo.pz, geo.yaw, geo.pitch, geo.third_person);
    geo_render_scene3d(geo.third_person);
    if (!geo.third_person)
        geo_draw_first_person_hand();
    if (geo.in_water)
        rect(0.0f, 0.0f, (float)screen_w, (float)screen_h, geo_gray(24, 70));
}

static void geo_render_menu_background(void) {
    float ang = geo.time_sec * 0.18f;
    float cx = (float)(GEO_WX / 2) + 0.5f, cz = (float)(GEO_WZ / 2) + 0.5f;
    float ex = cx + sinf(ang) * 14.0f, ez = cz + cosf(ang) * 14.0f;
    geo_setup_camera(ex, 24.5f, ez, ang + 3.14159265f, -0.36f, 0);
    geo_render_scene3d(0);
    rect(0.0f, 0.0f, (float)screen_w, (float)screen_h, geo_gray(14, 118));
}
