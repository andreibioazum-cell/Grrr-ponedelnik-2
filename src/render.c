#include "game3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int frame_init(Frame *f, int w, int h) {
    f->w = w; f->h = h;
    f->rgb = (unsigned char *)malloc((size_t)w * h * 3);
    f->zbuf = (float *)malloc((size_t)w * h * sizeof(float));
    if (!f->rgb || !f->zbuf) { frame_free(f); return 0; }
    return 1;
}

void frame_free(Frame *f) { free(f->rgb); free(f->zbuf); f->rgb = NULL; f->zbuf = NULL; }

void frame_clear(Frame *f, unsigned char r, unsigned char g, unsigned char b) {
    for (int i = 0; i < f->w * f->h; i++) {
        f->rgb[i * 3] = r; f->rgb[i * 3 + 1] = g; f->rgb[i * 3 + 2] = b;
        f->zbuf[i] = 1e30f;
    }
}

/* Camera basis. Forward includes pitch; right/up are orthonormal, so the
 * image cannot shear or "swim" when the camera rotates. */
static void camera_basis(const Camera *c, Vec3 *fwd, Vec3 *right, Vec3 *up) {
    float cp = cosf(c->pitch);
    *fwd = v3(sinf(c->yaw) * cp, sinf(c->pitch), -cosf(c->yaw) * cp);
    *right = v3_norm(v3_cross(*fwd, v3(0, 1, 0)));
    *up = v3_cross(*right, *fwd);
}

int project_point(const Camera *cam, int w, int h, Vec3 p,
                  float *sx, float *sy, float *depth) {
    Vec3 fwd, right, up;
    camera_basis(cam, &fwd, &right, &up);
    Vec3 d = v3_sub(p, cam->pos);
    float z = v3_dot(d, fwd);
    if (z < 0.05f) return 0;                    /* behind / too close */
    float focal = (h * 0.5f) / tanf(cam->fov_deg * 0.5f * 3.14159265f / 180.0f);
    *sx = w * 0.5f + v3_dot(d, right) * focal / z;
    *sy = h * 0.5f - v3_dot(d, up) * focal / z;
    *depth = z;
    return 1;
}

/* One shaded triangle with a per-pixel depth test. The depth buffer is
 * what stops distant faces drawing over near ones (flickering blocks). */
static void tri(Frame *f, const Camera *cam,
                Vec3 a, Vec3 b, Vec3 c, float r, float g, float bl) {
    float ax, ay, az, bx, by, bz, cx, cy, cz;
    if (!project_point(cam, f->w, f->h, a, &ax, &ay, &az)) return;
    if (!project_point(cam, f->w, f->h, b, &bx, &by, &bz)) return;
    if (!project_point(cam, f->w, f->h, c, &cx, &cy, &cz)) return;
    float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (area <= 0) return;                      /* back face: cull */
    int x0 = (int)fminf(ax, fminf(bx, cx)), x1 = (int)fmaxf(ax, fmaxf(bx, cx));
    int y0 = (int)fminf(ay, fminf(by, cy)), y1 = (int)fmaxf(ay, fmaxf(by, cy));
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= f->w) x1 = f->w - 1;
    if (y1 >= f->h) y1 = f->h - 1;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float w0 = ((bx - x) * (cy - y) - (by - y) * (cx - x)) / area;
            float w1 = ((cx - x) * (ay - y) - (cy - y) * (ax - x)) / area;
            float w2 = 1.0f - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            /* Perspective-correct depth via 1/z interpolation. */
            float iz = w0 / az + w1 / bz + w2 / cz;
            float z = 1.0f / iz;
            int idx = y * f->w + x;
            if (z >= f->zbuf[idx]) continue;
            f->zbuf[idx] = z;
            /* Cheap distance fog + fixed shading baked into the colour. */
            float fog = 1.0f / (1.0f + z * 0.012f);
            f->rgb[idx * 3]     = (unsigned char)(fminf(1.0f, r * fog + 0.25f * (1 - fog)) * 255);
            f->rgb[idx * 3 + 1] = (unsigned char)(fminf(1.0f, g * fog + 0.45f * (1 - fog)) * 255);
            f->rgb[idx * 3 + 2] = (unsigned char)(fminf(1.0f, bl * fog + 0.75f * (1 - fog)) * 255);
        }
    }
}

static void quad(Frame *f, const Camera *cam, Vec3 a, Vec3 b, Vec3 c, Vec3 d,
                 float r, float g, float bl) {
    tri(f, cam, a, b, c, r, g, bl);
    tri(f, cam, a, c, d, r, g, bl);
}

/* Draw one axis-aligned cube from its integer/min corner. All six faces
 * are emitted with outward winding; per-face shade makes the volume read
 * as 3D. `top_tint` lets TNT draw its fuse face differently. */
static void cube(Frame *f, const Camera *cam, Vec3 mn, Vec3 mx,
                 float r, float g, float b, int is_tnt) {
    Vec3 v[8] = {
        v3(mn.x, mn.y, mn.z), v3(mx.x, mn.y, mn.z), v3(mx.x, mn.y, mx.z), v3(mn.x, mn.y, mx.z),
        v3(mn.x, mx.y, mn.z), v3(mx.x, mx.y, mn.z), v3(mx.x, mx.y, mx.z), v3(mn.x, mx.y, mx.z)
    };
    /* face: 4 corner indices + shade. Order keeps outward winding. */
    static const int F[6][4] = {
        {0,1,2,3}, /* bottom */ {4,7,6,5}, /* top */
        {0,4,5,1}, /* north  */ {3,2,6,7}, /* south */
        {0,3,7,4}, /* west   */ {1,5,6,2}  /* east  */
    };
    static const float SHADE[6] = { 0.45f, 1.0f, 0.70f, 0.80f, 0.60f, 0.90f };
    for (int i = 0; i < 6; i++) {
        float rr = r * SHADE[i], gg = g * SHADE[i], bb = b * SHADE[i];
        if (is_tnt && i == 1) { rr = 0.25f; gg = 0.22f; bb = 0.20f; } /* fuse cap */
        if (is_tnt && i != 1 && i != 0) { /* white "TNT" band on the sides */
            /* drawn as the side face itself; band is a lighter strip via
             * a second thin quad below - kept simple: brighten mid shade */
            rr = fminf(1.0f, rr + 0.12f); gg = fminf(1.0f, gg + 0.12f); bb = fminf(1.0f, bb + 0.12f);
        }
        quad(f, cam, v[F[i][0]], v[F[i][1]], v[F[i][2]], v[F[i][3]], rr, gg, bb);
    }
}

void render_world(Frame *f, const World *w, const Camera *cam) {
    /* Integer block address -> exact world corners. No stored float
     * position anywhere, so a block cannot drift relative to the camera. */
    for (int x = 0; x < WORLD_X; x++)
        for (int y = 0; y < WORLD_Y; y++)
            for (int z = 0; z < WORLD_Z; z++) {
                int b = w->blocks[x][y][z];
                if (!b || !world_block_visible(w, x, y, z)) continue;
                float dx = x + 0.5f - cam->pos.x, dy = y + 0.5f - cam->pos.y,
                      dz = z + 0.5f - cam->pos.z;
                if (dx * dx + dy * dy + dz * dz > 60.0f * 60.0f) continue;
                float r, g, bl;
                block_color(b, &r, &g, &bl);
                cube(f, cam, v3((float)x, (float)y, (float)z),
                     v3(x + 1.0f, y + 1.0f, z + 1.0f), r, g, bl, b == BLOCK_TNT);
            }
}

/* A box part of the player model, rotated by the player's yaw. */
static void part(Frame *f, const Camera *cam, const Player *p, Vec3 feet,
                 Vec3 centre, Vec3 size, float r, float g, float b) {
    float sy = sinf(p->yaw), cy = cosf(p->yaw);
    Vec3 c = v3(feet.x + centre.x * cy + centre.z * sy,
                feet.y + centre.y,
                feet.z - centre.x * sy + centre.z * cy);
    /* Axis-aligned in player space, then approximated as a world AABB:
     * good enough for a blocky character and fully volumetric. */
    float hx = (fabsf(size.x * cy) + fabsf(size.z * sy)) * 0.5f;
    float hz = (fabsf(size.x * sy) + fabsf(size.z * cy)) * 0.5f;
    cube(f, cam, v3(c.x - hx, c.y - size.y * 0.5f, c.z - hz),
         v3(c.x + hx, c.y + size.y * 0.5f, c.z + hz), r, g, b, 0);
}

void render_player(Frame *f, const Player *p, const Camera *cam, int draw_player) {
    if (!draw_player) return;
    float swing = sinf(p->walk_phase) * 0.25f;
    Vec3 feet = p->pos;
    /* Legs, torso, arms (swinging), head with a face shade. All boxes:
     * a real 3D body from every camera angle, never a flat sprite. */
    part(f, cam, p, feet, v3(-0.14f, 0.30f, swing * 0.3f),  v3(0.24f, 0.60f, 0.26f), 0.20f, 0.25f, 0.65f);
    part(f, cam, p, feet, v3( 0.14f, 0.30f, -swing * 0.3f), v3(0.24f, 0.60f, 0.26f), 0.20f, 0.25f, 0.65f);
    part(f, cam, p, feet, v3(0, 0.90f, 0),                  v3(0.56f, 0.62f, 0.30f), 0.75f, 0.20f, 0.20f);
    part(f, cam, p, feet, v3(-0.40f, 0.92f, -swing * 0.35f), v3(0.20f, 0.58f, 0.24f), 0.85f, 0.65f, 0.45f);
    part(f, cam, p, feet, v3( 0.40f, 0.92f,  swing * 0.35f), v3(0.20f, 0.58f, 0.24f), 0.85f, 0.65f, 0.45f);
    part(f, cam, p, feet, v3(0, 1.56f, 0),                  v3(0.46f, 0.46f, 0.46f), 0.92f, 0.72f, 0.52f);
}

void render_highlight(Frame *f, const Camera *cam, const int *hit) {
    if (!hit) return;
    /* Slightly larger wireframe-ish cube: drawn as 4 top edges via thin
     * boxes would cost a lot; instead brighten by drawing a shrunk cube
     * outline using the top face only is skipped - we draw a translucent
     * marker cube scaled 1.02 in a bright colour with depth test on. */
    cube(f, cam, v3(hit[0] - 0.01f, hit[1] - 0.01f, hit[2] - 0.01f),
         v3(hit[0] + 1.01f, hit[1] + 1.01f, hit[2] + 1.01f), 1.0f, 0.9f, 0.2f, 0);
}

int frame_write_ppm(const Frame *f, const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    fprintf(fp, "P6\n%d %d\n255\n", f->w, f->h);
    fwrite(f->rgb, 1, (size_t)f->w * f->h * 3, fp);
    fclose(fp);
    return 1;
}
