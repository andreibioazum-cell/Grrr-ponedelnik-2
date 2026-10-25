/* Perspective 3D math, near-plane polygon clipper, full-texture 32x32 UV mapping,
 * monochrome dark-gray UI geometry primitives, and 3D isometric block icons. */

#define GEO_EYE_H 1.62f
#define GEO_NEAR_Z 0.09f
#define GEO_FOG_START 25.0f
#define GEO_FOG_END 42.0f

typedef struct {
    float cx, cy, cz;
    float u, v;
    float r, g, b, a;
} GeoClipVert;

static void geo_setup_camera(float eye_x, float eye_y, float eye_z, float yaw, float pitch, int third_person) {
    float cy = cosf(yaw), sy = sinf(yaw);
    float cp = cosf(pitch), sp = sinf(pitch);
    geo.fx = sy * cp;
    geo.fy = sp;
    geo.fz = cy * cp;
    geo.rx = -cy;
    geo.ry = 0.0f;
    geo.rz = sy;
    geo.ux = geo.ry * geo.fz - geo.rz * geo.fy;
    geo.uy = geo.rz * geo.fx - geo.rx * geo.fz;
    geo.uz = geo.rx * geo.fy - geo.ry * geo.fx;

    geo.cam_x = eye_x;
    geo.cam_y = eye_y;
    geo.cam_z = eye_z;
    if (third_person) {
        int step, steps = (int)(geo.cam_dist / 0.22f);
        if (steps < 4)
            steps = 4;
        for (step = 1; step <= steps; step++) {
            float d = (float)step * 0.22f;
            float tx = eye_x - geo.fx * d;
            float ty = eye_y - geo.fy * d + 0.18f;
            float tz = eye_z - geo.fz * d;
            if (geo_is_solid((int)floorf(tx), (int)floorf(ty), (int)floorf(tz)))
                break;
            geo.cam_x = tx;
            geo.cam_y = ty;
            geo.cam_z = tz;
        }
    }
    geo.aspect = (float)(screen_w > 1 ? screen_w : 1) / (float)(screen_h > 1 ? screen_h : 1);
    geo.tan_half_vfov = (geo.aspect < 1.0f) ? 0.84f : 0.65f;
    geo.quad_count = 0;
    geo.rec_n = 0;
}

static void geo_world_to_cam(float wx, float wy, float wz, float *cx, float *cy, float *cz) {
    float dx = wx - geo.cam_x, dy = wy - geo.cam_y, dz = wz - geo.cam_z;
    *cx = dx * geo.rx + dy * geo.ry + dz * geo.rz;
    *cy = dx * geo.ux + dy * geo.uy + dz * geo.uz;
    *cz = dx * geo.fx + dy * geo.fy + dz * geo.fz;
}

static int geo_project_point(float wx, float wy, float wz, float *sx, float *sy, float *depth) {
    float cx, cy, cz, inv;
    geo_world_to_cam(wx, wy, wz, &cx, &cy, &cz);
    if (depth)
        *depth = cz;
    if (cz < GEO_NEAR_Z)
        return 0;
    inv = 1.0f / (cz * geo.tan_half_vfov);
    *sx = (0.5f + 0.5f * (cx * inv / geo.aspect)) * (float)screen_w;
    *sy = (0.5f - 0.5f * (cy * inv)) * (float)screen_h;
    return 1;
}

static uint32_t geo_pack_vk_rgba(float r, float g, float b, float a) {
    int ri = (int)(r + 0.5f), gi = (int)(g + 0.5f), bi = (int)(b + 0.5f), ai = (int)(a + 0.5f);
    ri = ri < 0 ? 0 : (ri > 255 ? 255 : ri);
    gi = gi < 0 ? 0 : (gi > 255 ? 255 : gi);
    bi = bi < 0 ? 0 : (bi > 255 ? 255 : bi);
    ai = ai < 0 ? 0 : (ai > 255 ? 255 : ai);
    return (uint32_t)ri | ((uint32_t)gi << 8) | ((uint32_t)bi << 16) | ((uint32_t)ai << 24);
}

static void geo_sky_rgb(float *sr, float *sg, float *sb) {
    *sr = 176.0f;
    *sg = 198.0f;
    *sb = 224.0f;
}

static void geo_emit_screen_quad(const char *tex_name, float x0, float y0, float u0, float v0, uint32_t c0, float x1,
                                 float y1, float u1, float v1, uint32_t c1, float x2, float y2, float u2, float v2,
                                 uint32_t c2, float x3, float y3, float u3, float v3, uint32_t c3) {
    if (geo.record_quads && geo.rec_n < 16384) {
        float mnx = x0, mxx = x0, mny = y0, mxy = y0;
        float xs[3] = {x1, x2, x3}, ys[3] = {y1, y2, y3};
        int k;
        for (k = 0; k < 3; k++) {
            if (xs[k] < mnx)
                mnx = xs[k];
            if (xs[k] > mxx)
                mxx = xs[k];
            if (ys[k] < mny)
                mny = ys[k];
            if (ys[k] > mxy)
                mxy = ys[k];
        }
        geo.rec_is_water[geo.rec_n] = (tex_name && strcmp(tex_name, "azure_water.png") == 0) ? 1 : 0;
        geo.rec_min_x[geo.rec_n] = mnx;
        geo.rec_max_x[geo.rec_n] = mxx;
        geo.rec_min_y[geo.rec_n] = mny;
        geo.rec_max_y[geo.rec_n] = mxy;
        geo.rec_n++;
    }
    ds_gfx_quad(tex_name, x0, y0, u0, v0, c0, x1, y1, u1, v1, c1, x2, y2, u2, v2, c2, x3, y3, u3, v3, c3);
}

static void geo_emit_clipped_quad(const GeoClipVert in_v[4], const char *tex_name) {
    GeoClipVert out_v[8];
    int n = 0, i, vis_any = 0;
    float sx[8], sy[8];
    uint32_t col[8];

    if (geo.quad_count >= 13500)
        return;

    for (i = 0; i < 4; i++) {
        const GeoClipVert *a = &in_v[i];
        const GeoClipVert *b = &in_v[(i + 1) & 3];
        int a_in = a->cz >= GEO_NEAR_Z;
        int b_in = b->cz >= GEO_NEAR_Z;
        if (a_in)
            out_v[n++] = *a;
        if (a_in != b_in) {
            float t = (GEO_NEAR_Z - a->cz) / (b->cz - a->cz);
            GeoClipVert m = {a->cx + (b->cx - a->cx) * t, a->cy + (b->cy - a->cy) * t, GEO_NEAR_Z,
                             a->u + (b->u - a->u) * t,    a->v + (b->v - a->v) * t,    a->r + (b->r - a->r) * t,
                             a->g + (b->g - a->g) * t,    a->b + (b->b - a->b) * t,    a->a + (b->a - a->a) * t};
            out_v[n++] = m;
        }
    }
    if (n < 3)
        return;

    for (i = 0; i < n; i++) {
        float inv = 1.0f / (out_v[i].cz * geo.tan_half_vfov);
        sx[i] = (0.5f + 0.5f * (out_v[i].cx * inv / geo.aspect)) * (float)screen_w;
        sy[i] = (0.5f - 0.5f * (out_v[i].cy * inv)) * (float)screen_h;
        col[i] = geo_pack_vk_rgba(out_v[i].r, out_v[i].g, out_v[i].b, out_v[i].a);
        if (sx[i] >= -240.0f && sx[i] <= (float)screen_w + 240.0f && sy[i] >= -240.0f &&
            sy[i] <= (float)screen_h + 240.0f)
            vis_any = 1;
    }
    if (!vis_any)
        return;

    if (n == 4) {
        geo.quad_count++;
        geo_emit_screen_quad(tex_name, sx[0], sy[0], out_v[0].u, out_v[0].v, col[0], sx[1], sy[1], out_v[1].u,
                             out_v[1].v, col[1], sx[2], sy[2], out_v[2].u, out_v[2].v, col[2], sx[3], sy[3],
                             out_v[3].u, out_v[3].v, col[3]);
        return;
    }
    for (i = 1; i + 1 < n; i++) {
        geo.quad_count++;
        if (i + 2 < n) {
            geo_emit_screen_quad(tex_name, sx[0], sy[0], out_v[0].u, out_v[0].v, col[0], sx[i], sy[i], out_v[i].u,
                                 out_v[i].v, col[i], sx[i + 1], sy[i + 1], out_v[i + 1].u, out_v[i + 1].v, col[i + 1],
                                 sx[i + 2], sy[i + 2], out_v[i + 2].u, out_v[i + 2].v, col[i + 2]);
            i++;
        } else {
            geo_emit_screen_quad(tex_name, sx[0], sy[0], out_v[0].u, out_v[0].v, col[0], sx[i], sy[i], out_v[i].u,
                                 out_v[i].v, col[i], sx[i + 1], sy[i + 1], out_v[i + 1].u, out_v[i + 1].v, col[i + 1],
                                 sx[i + 1], sy[i + 1], out_v[i + 1].u, out_v[i + 1].v, col[i + 1]);
        }
    }
}

/* Draws a 3D world quad using its full 32x32 PNG texture (u,v in [0..1]) with adaptive
 * perspective subdivision on close faces so 32x32 pixels never warp! */
static void geo_draw_quad3d(const float wx[4], const float wy[4], const float wz[4], const char *tex_name,
                            const float vlight[4], float alpha, float fog_k) {
    GeoClipVert c[4];
    const float u0 = 0.008f, v0 = 0.008f, u1 = 0.992f, v1 = 0.992f;
    float sr, sg, sb;
    float min_z = 1e9f, max_z = -1e9f;
    int i, subdiv = 1;

    if (!tex_name || !tex_name[0])
        return;
    geo_sky_rgb(&sr, &sg, &sb);
    for (i = 0; i < 4; i++) {
        float lum = vlight[i] * 255.0f;
        geo_world_to_cam(wx[i], wy[i], wz[i], &c[i].cx, &c[i].cy, &c[i].cz);
        c[i].r = lum + (sr - lum) * fog_k;
        c[i].g = lum + (sg - lum) * fog_k;
        c[i].b = lum + (sb - lum) * fog_k;
        c[i].a = alpha;
        if (c[i].cz < min_z)
            min_z = c[i].cz;
        if (c[i].cz > max_z)
            max_z = c[i].cz;
    }
    if (max_z < GEO_NEAR_Z)
        return;
    c[0].u = u0;
    c[0].v = v0;
    c[1].u = u1;
    c[1].v = v0;
    c[2].u = u1;
    c[2].v = v1;
    c[3].u = u0;
    c[3].v = v1;

    if (min_z < 2.2f)
        subdiv = 4;
    else if (min_z < 5.5f)
        subdiv = 2;

    if (subdiv == 1) {
        geo_emit_clipped_quad(c, tex_name);
        return;
    }
    {
        int gx, gy;
        float inv_n = 1.0f / (float)subdiv;
        for (gy = 0; gy < subdiv; gy++) {
            float t0 = (float)gy * inv_n, t1 = (float)(gy + 1) * inv_n;
            for (gx = 0; gx < subdiv; gx++) {
                float s0 = (float)gx * inv_n, s1 = (float)(gx + 1) * inv_n;
                GeoClipVert sub[4];
                float st[4][2] = {{s0, t0}, {s1, t0}, {s1, t1}, {s0, t1}};
                for (i = 0; i < 4; i++) {
                    float s = st[i][0], t = st[i][1];
                    float w00 = (1.f - s) * (1.f - t), w10 = s * (1.f - t);
                    float w11 = s * t, w01 = (1.f - s) * t;
                    sub[i].cx = c[0].cx * w00 + c[1].cx * w10 + c[2].cx * w11 + c[3].cx * w01;
                    sub[i].cy = c[0].cy * w00 + c[1].cy * w10 + c[2].cy * w11 + c[3].cy * w01;
                    sub[i].cz = c[0].cz * w00 + c[1].cz * w10 + c[2].cz * w11 + c[3].cz * w01;
                    sub[i].u = c[0].u * w00 + c[1].u * w10 + c[2].u * w11 + c[3].u * w01;
                    sub[i].v = c[0].v * w00 + c[1].v * w10 + c[2].v * w11 + c[3].v * w01;
                    sub[i].r = c[0].r * w00 + c[1].r * w10 + c[2].r * w11 + c[3].r * w01;
                    sub[i].g = c[0].g * w00 + c[1].g * w10 + c[2].g * w11 + c[3].g * w01;
                    sub[i].b = c[0].b * w00 + c[1].b * w10 + c[2].b * w11 + c[3].b * w01;
                    sub[i].a = alpha;
                }
                geo_emit_clipped_quad(sub, tex_name);
            }
        }
    }
}

static void geo_text_center(const char *s, float cx, float cy, uint32_t col, float sc) {
    float w;
    if (!s || !s[0])
        return;
    w = (float)text_ink_width(s) * sc;
    text_scaled(s, cx - w * 0.5f, cy - 13.0f * sc, col, sc);
}

/* Monochrome dark-gray stadium capsule / rounded pill button */
static void geo_draw_pill_btn(GeoRect r, const char *label, int active, float font_sc) {
    float rad = r.h * 0.5f;
    roundrect(r.x - 2.0f, r.y - 2.0f, r.w + 4.0f, r.h + 4.0f, rad + 2.0f,
              active ? geo_gray(196, 240) : geo_gray(96, 215));
    roundrect(r.x, r.y, r.w, r.h, rad, active ? geo_gray(66, 238) : geo_gray(36, 224));
    roundrect(r.x + 3.0f, r.y + 3.0f, r.w - 6.0f, r.h * 0.44f, rad * 0.8f,
              active ? geo_gray(84, 90) : geo_gray(56, 70));
    geo_text_center(label, r.x + r.w * 0.5f, r.y + r.h * 0.5f, active ? geo_gray(248, 255) : geo_gray(224, 245),
                    font_sc);
}

/* Large ergonomic full-circle monochrome dark-gray control button */
static void geo_draw_circle_btn(float cx, float cy, float rad, const char *label, int active, int glyph) {
    circle(cx, cy, rad + 3.0f, active ? geo_gray(208, 242) : geo_gray(96, 210));
    circle(cx, cy, rad, active ? geo_gray(68, 236) : geo_gray(34, 216));
    ring(cx, cy, rad - 6.0f, 2.0f, active ? geo_gray(150, 150) : geo_gray(62, 140));

    /* Crisp geometric monochrome symbol above caption */
    if (glyph == 1) { /* Break / Crosshair-Pick */
        line(cx - 14.0f, cy - 22.0f, cx + 14.0f, cy - 6.0f, 3.4f, geo_gray(236, 245));
        line(cx + 14.0f, cy - 22.0f, cx - 14.0f, cy - 6.0f, 3.4f, geo_gray(236, 245));
    } else if (glyph == 2) { /* Place / Isometric Plus Cube */
        line(cx, cy - 26.0f, cx, cy - 4.0f, 3.6f, geo_gray(238, 245));
        line(cx - 11.0f, cy - 15.0f, cx + 11.0f, cy - 15.0f, 3.6f, geo_gray(238, 245));
    } else if (glyph == 3) { /* Up / Jump Chevron */
        line(cx - 13.0f, cy - 10.0f, cx, cy - 23.0f, 3.6f, geo_gray(238, 245));
        line(cx, cy - 23.0f, cx + 13.0f, cy - 10.0f, 3.6f, geo_gray(238, 245));
    } else if (glyph == 4) { /* Down Chevron */
        line(cx - 13.0f, cy - 21.0f, cx, cy - 8.0f, 3.6f, geo_gray(238, 245));
        line(cx, cy - 8.0f, cx + 13.0f, cy - 21.0f, 3.6f, geo_gray(238, 245));
    }
    geo_text_center(label, cx, cy + 16.0f, active ? geo_gray(250, 255) : geo_gray(216, 240), 0.56f);
}

static int geo_hit_rect(float x, float y, GeoRect r) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

static int geo_hit_circle(float x, float y, float cx, float cy, float rad) {
    float dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= rad * rad;
}
