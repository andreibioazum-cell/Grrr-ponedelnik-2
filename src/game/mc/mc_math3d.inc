/* 3D perspective camera, near-plane polygon clipper, smooth AO & fog shading,
 * and adaptive perspective-correct textured quad emission. */

#define MC_NEAR_Z 0.09f
#define MC_FOG_START 23.0f
#define MC_FOG_END 38.0f

typedef struct {
    float cx, cy, cz; /* camera-space coordinates */
    float u, v;
    float r, g, b, a;
} McClipVert;

static void mc_setup_camera(float eye_x, float eye_y, float eye_z, float yaw, float pitch, int allow_third) {
    float cy = cosf(yaw), sy = sinf(yaw);
    float cp = cosf(pitch), sp = sinf(pitch);
    /* Right-handed camera basis: yaw = 0 looks along +Z, yaw = PI looks along -Z,
     * positive right vector is (+cy, 0, -sy) when forward is (sy*cp, sp, cy*cp). */
    mc.fx = sy * cp;
    mc.fy = sp;
    mc.fz = cy * cp;
    mc.rx = -cy;
    mc.ry = 0.0f;
    mc.rz = sy;
    /* up = right x forward -> keeps +Y screen upward! */
    mc.ux = mc.ry * mc.fz - mc.rz * mc.fy;
    mc.uy = mc.rz * mc.fx - mc.rx * mc.fz;
    mc.uz = mc.rx * mc.fy - mc.ry * mc.fx;

    mc.cam_x = eye_x;
    mc.cam_y = eye_y;
    mc.cam_z = eye_z;
    if (allow_third) {
        /* Smooth wall-clipped 3rd-person orbit 3.6 blocks behind head */
        int step;
        for (step = 1; step <= 18; step++) {
            float d = (float)step * 0.2f;
            float tx = eye_x - mc.fx * d;
            float ty = eye_y - mc.fy * d + 0.18f;
            float tz = eye_z - mc.fz * d;
            if (mc_is_solid((int)floorf(tx), (int)floorf(ty), (int)floorf(tz)))
                break;
            mc.cam_x = tx;
            mc.cam_y = ty;
            mc.cam_z = tz;
        }
    }
    mc.aspect = (float)(screen_w > 1 ? screen_w : 1) / (float)(screen_h > 1 ? screen_h : 1);
    mc.tan_half_vfov = (mc.aspect < 1.0f) ? 0.84f : 0.65f;
    mc.quad_count = 0;
}

static void mc_world_to_cam(float wx, float wy, float wz, float *cx, float *cy, float *cz) {
    float dx = wx - mc.cam_x, dy = wy - mc.cam_y, dz = wz - mc.cam_z;
    *cx = dx * mc.rx + dy * mc.ry + dz * mc.rz;
    *cy = dx * mc.ux + dy * mc.uy + dz * mc.uz;
    *cz = dx * mc.fx + dy * mc.fy + dz * mc.fz;
}

static int mc_project_point(float wx, float wy, float wz, float *sx, float *sy, float *depth) {
    float cx, cy, cz, inv;
    mc_world_to_cam(wx, wy, wz, &cx, &cy, &cz);
    if (depth)
        *depth = cz;
    if (cz < MC_NEAR_Z)
        return 0;
    inv = 1.0f / (cz * mc.tan_half_vfov);
    *sx = (0.5f + 0.5f * (cx * inv / mc.aspect)) * (float)screen_w;
    *sy = (0.5f - 0.5f * (cy * inv)) * (float)screen_h;
    return 1;
}

static uint32_t mc_pack_rgba(float r, float g, float b, float a) {
    int ri = (int)(r + 0.5f), gi = (int)(g + 0.5f), bi = (int)(b + 0.5f), ai = (int)(a + 0.5f);
    if (ri < 0)
        ri = 0;
    else if (ri > 255)
        ri = 255;
    if (gi < 0)
        gi = 0;
    else if (gi > 255)
        gi = 255;
    if (bi < 0)
        bi = 0;
    else if (bi > 255)
        bi = 255;
    if (ai < 0)
        ai = 0;
    else if (ai > 255)
        ai = 255;
    return (uint32_t)ri | ((uint32_t)gi << 8) | ((uint32_t)bi << 16) | ((uint32_t)ai << 24);
}

static uint32_t mc_pack_argb(float r, float g, float b, float a) {
    int ri = (int)(r + 0.5f), gi = (int)(g + 0.5f), bi = (int)(b + 0.5f), ai = (int)(a + 0.5f);
    if (ri < 0)
        ri = 0;
    else if (ri > 255)
        ri = 255;
    if (gi < 0)
        gi = 0;
    else if (gi > 255)
        gi = 255;
    if (bi < 0)
        bi = 0;
    else if (bi > 255)
        bi = 255;
    if (ai < 1)
        ai = 1;
    else if (ai > 255)
        ai = 255;
    return ((uint32_t)ai << 24) | ((uint32_t)ri << 16) | ((uint32_t)gi << 8) | (uint32_t)bi;
}

static void mc_sky_rgb(float *sr, float *sg, float *sb) {
    if (net_event() == 1.0) {
        float t = mc.time_sec * 2.4f;
        *sr = 120.0f + 95.0f * sinf(t);
        *sg = 110.0f + 95.0f * sinf(t + 2.09f);
        *sb = 155.0f + 95.0f * sinf(t + 4.18f);
        return;
    }
    *sr = 158.0f;
    *sg = 204.0f;
    *sb = 255.0f;
}

static void mc_emit_clipped_quad(const McClipVert in_v[4], const char *tex_name) {
    McClipVert out_v[8];
    int n = 0, i;
    float sx[8], sy[8];
    uint32_t col[8];
    int vis_any = 0;

    if (mc.quad_count >= 13500)
        return;

    for (i = 0; i < 4; i++) {
        const McClipVert *a = &in_v[i];
        const McClipVert *b = &in_v[(i + 1) & 3];
        int a_in = a->cz >= MC_NEAR_Z;
        int b_in = b->cz >= MC_NEAR_Z;
        if (a_in)
            out_v[n++] = *a;
        if (a_in != b_in) {
            float t = (MC_NEAR_Z - a->cz) / (b->cz - a->cz);
            McClipVert m;
            m.cx = a->cx + (b->cx - a->cx) * t;
            m.cy = a->cy + (b->cy - a->cy) * t;
            m.cz = MC_NEAR_Z;
            m.u = a->u + (b->u - a->u) * t;
            m.v = a->v + (b->v - a->v) * t;
            m.r = a->r + (b->r - a->r) * t;
            m.g = a->g + (b->g - a->g) * t;
            m.b = a->b + (b->b - a->b) * t;
            m.a = a->a + (b->a - a->a) * t;
            out_v[n++] = m;
        }
    }
    if (n < 3)
        return;

    for (i = 0; i < n; i++) {
        float inv = 1.0f / (out_v[i].cz * mc.tan_half_vfov);
        sx[i] = (0.5f + 0.5f * (out_v[i].cx * inv / mc.aspect)) * (float)screen_w;
        sy[i] = (0.5f - 0.5f * (out_v[i].cy * inv)) * (float)screen_h;
        col[i] = mc_pack_rgba(out_v[i].r, out_v[i].g, out_v[i].b, out_v[i].a);
        if (sx[i] >= -240.0f && sx[i] <= (float)screen_w + 240.0f && sy[i] >= -240.0f &&
            sy[i] <= (float)screen_h + 240.0f)
            vis_any = 1;
    }
    if (!vis_any)
        return;

    if (n == 4) {
        mc.quad_count++;
        ds_gfx_quad(tex_name, sx[0], sy[0], out_v[0].u, out_v[0].v, col[0], sx[1], sy[1], out_v[1].u, out_v[1].v,
                    col[1], sx[2], sy[2], out_v[2].u, out_v[2].v, col[2], sx[3], sy[3], out_v[3].u, out_v[3].v,
                    col[3]);
        return;
    }
    for (i = 1; i + 1 < n; i++) {
        mc.quad_count++;
        if (i + 2 < n) {
            ds_gfx_quad(tex_name, sx[0], sy[0], out_v[0].u, out_v[0].v, col[0], sx[i], sy[i], out_v[i].u,
                        out_v[i].v, col[i], sx[i + 1], sy[i + 1], out_v[i + 1].u, out_v[i + 1].v, col[i + 1],
                        sx[i + 2], sy[i + 2], out_v[i + 2].u, out_v[i + 2].v, col[i + 2]);
            i++;
        } else {
            ds_gfx_quad(tex_name, sx[0], sy[0], out_v[0].u, out_v[0].v, col[0], sx[i], sy[i], out_v[i].u,
                        out_v[i].v, col[i], sx[i + 1], sy[i + 1], out_v[i + 1].u, out_v[i + 1].v, col[i + 1],
                        sx[i + 1], sy[i + 1], out_v[i + 1].u, out_v[i + 1].v, col[i + 1]);
        }
    }
}

static void mc_tile_uv(int tile, float *u0, float *v0, float *u1, float *v1) {
    const float inset = 0.03f / 16.0f;
    float tx = (float)(tile & 15) / 16.0f;
    float ty = (float)(tile >> 4) / 16.0f;
    *u0 = tx + inset;
    *v0 = ty + inset;
    *u1 = tx + (1.0f / 16.0f) - inset;
    *v1 = ty + (1.0f / 16.0f) - inset;
}

/* Draws a 3D world quad with adaptive 4x4 / 2x2 / 1x1 perspective UV subdivision
 * so close-up 16x16 Minecraft pixels stay razor-sharp without affine swim! */
static void mc_draw_quad3d(const float wx[4], const float wy[4], const float wz[4], int tile, const float vlight[4],
                           float alpha, float fog_k) {
    McClipVert c[4];
    float u0, v0, u1, v1, sr, sg, sb;
    int subdiv = 1, i, su, sv;
    float min_z = 999.0f, max_z = -999.0f;

    for (i = 0; i < 4; i++) {
        mc_world_to_cam(wx[i], wy[i], wz[i], &c[i].cx, &c[i].cy, &c[i].cz);
        if (c[i].cz < min_z)
            min_z = c[i].cz;
        if (c[i].cz > max_z)
            max_z = c[i].cz;
    }
    if (max_z < MC_NEAR_Z)
        return;

    mc_sky_rgb(&sr, &sg, &sb);
    mc_tile_uv(tile, &u0, &v0, &u1, &v1);
    for (i = 0; i < 4; i++) {
        float l = vlight[i] * 255.0f;
        c[i].r = l + (sr - l) * fog_k;
        c[i].g = l + (sg - l) * fog_k;
        c[i].b = l + (sb - l) * fog_k;
        c[i].a = alpha;
    }
    c[0].u = u0;
    c[0].v = v0;
    c[1].u = u1;
    c[1].v = v0;
    c[2].u = u1;
    c[2].v = v1;
    c[3].u = u0;
    c[3].v = v1;

    if (min_z < 3.6f)
        subdiv = 2;

    if (subdiv == 1) {
        mc_emit_clipped_quad(c, "mc_atlas.png");
        return;
    }

    for (sv = 0; sv < subdiv; sv++) {
        float tv0 = (float)sv / (float)subdiv, tv1 = (float)(sv + 1) / (float)subdiv;
        for (su = 0; su < subdiv; su++) {
            float tu0 = (float)su / (float)subdiv, tu1 = (float)(su + 1) / (float)subdiv;
            McClipVert sub[4];
            float tus[4], tvs[4];
            int k;
            tus[0] = tu0;
            tvs[0] = tv0;
            tus[1] = tu1;
            tvs[1] = tv0;
            tus[2] = tu1;
            tvs[2] = tv1;
            tus[3] = tu0;
            tvs[3] = tv1;
            for (k = 0; k < 4; k++) {
                float a0 = (1.0f - tus[k]) * (1.0f - tvs[k]);
                float a1 = tus[k] * (1.0f - tvs[k]);
                float a2 = tus[k] * tvs[k];
                float a3 = (1.0f - tus[k]) * tvs[k];
                sub[k].cx = c[0].cx * a0 + c[1].cx * a1 + c[2].cx * a2 + c[3].cx * a3;
                sub[k].cy = c[0].cy * a0 + c[1].cy * a1 + c[2].cy * a2 + c[3].cy * a3;
                sub[k].cz = c[0].cz * a0 + c[1].cz * a1 + c[2].cz * a2 + c[3].cz * a3;
                sub[k].u = c[0].u * a0 + c[1].u * a1 + c[2].u * a2 + c[3].u * a3;
                sub[k].v = c[0].v * a0 + c[1].v * a1 + c[2].v * a2 + c[3].v * a3;
                sub[k].r = c[0].r * a0 + c[1].r * a1 + c[2].r * a2 + c[3].r * a3;
                sub[k].g = c[0].g * a0 + c[1].g * a1 + c[2].g * a2 + c[3].g * a3;
                sub[k].b = c[0].b * a0 + c[1].b * a1 + c[2].b * a2 + c[3].b * a3;
                sub[k].a = c[0].a;
            }
            mc_emit_clipped_quad(sub, "mc_atlas.png");
        }
    }
}
