#include "game3d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

Vec3 v3(float x, float y, float z) { Vec3 r = { x, y, z }; return r; }
Vec3 v3_add(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
Vec3 v3_sub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
Vec3 v3_scale(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
float v3_len(Vec3 a) { return sqrtf(v3_dot(a, a)); }
float v3_dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 v3_cross(Vec3 a, Vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
Vec3 v3_norm(Vec3 a) {
    float l = v3_len(a);
    return l > 1e-6f ? v3_scale(a, 1.0f / l) : v3(0, 0, 0);
}

/* Small deterministic PRNG so a seed always builds the same map. */
static unsigned rng_state;
static unsigned rng_next(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return (rng_state >> 16) & 0x7fffu;
}

int world_in_bounds(int x, int y, int z) {
    return x >= 0 && x < WORLD_X && y >= 0 && y < WORLD_Y && z >= 0 && z < WORLD_Z;
}

int world_get(const World *w, int x, int y, int z) {
    if (y < 0) return BLOCK_STONE;              /* floor under the world */
    if (!world_in_bounds(x, y, z)) return BLOCK_AIR;
    return w->blocks[x][y][z];
}

void world_set(World *w, int x, int y, int z, int block) {
    if (world_in_bounds(x, y, z)) w->blocks[x][y][z] = (unsigned char)block;
}

void world_generate(World *w, int seed) {
    memset(w, 0, sizeof(*w));
    w->seed = seed;
    rng_state = (unsigned)(seed * 2654435761u + 12345u);
    int cx = WORLD_X / 2, cz = WORLD_Z / 2;
    for (int x = 0; x < WORLD_X; x++) {
        for (int z = 0; z < WORLD_Z; z++) {
            /* Gentle hills from a couple of sine waves + jitter. */
            float h = 6.0f + 2.0f * sinf(x * 0.18f + seed) * cosf(z * 0.15f - seed)
                      + (float)(rng_next() % 3) * 0.4f;
            int ground = (int)h;
            if (ground < 2) ground = 2;
            if (ground > WORLD_Y - 6) ground = WORLD_Y - 6;
            for (int y = 0; y <= ground; y++) {
                int b = (y == ground) ? BLOCK_GRASS
                        : (y > ground - 3) ? BLOCK_DIRT : BLOCK_STONE;
                if (y == ground && ground <= 3) b = BLOCK_SAND;
                w->blocks[x][y][z] = (unsigned char)b;
            }
        }
    }
    /* Flat spawn plaza so the player never spawns inside a hill. */
    int gy = 6;
    for (int x = cx - 4; x <= cx + 4; x++)
        for (int z = cz - 4; z <= cz + 4; z++) {
            for (int y = 0; y < WORLD_Y; y++)
                w->blocks[x][y][z] = (y < gy) ? BLOCK_STONE
                                     : (y == gy) ? BLOCK_GRASS : BLOCK_AIR;
        }
    /* Trees: trunk + leaf blob, kept off the plaza. */
    for (int i = 0; i < 14; i++) {
        int x = 2 + (int)(rng_next() % (WORLD_X - 4));
        int z = 2 + (int)(rng_next() % (WORLD_Z - 4));
        if (abs(x - cx) <= 5 && abs(z - cz) <= 5) continue;
        int top = 0;
        while (top + 1 < WORLD_Y && w->blocks[x][top + 1][z]) top++;
        for (int y = top + 1; y <= top + 4 && y < WORLD_Y; y++)
            w->blocks[x][y][z] = BLOCK_WOOD;
        for (int dx = -1; dx <= 1; dx++)
            for (int dz = -1; dz <= 1; dz++)
                for (int dy = 3; dy <= 5; dy++) {
                    int px = x + dx, py = top + dy, pz = z + dz;
                    if (world_in_bounds(px, py, pz) && !w->blocks[px][py][pz])
                        w->blocks[px][py][pz] = BLOCK_LEAVES;
                }
    }
    /* Dynamite: a small demo stash near spawn + scattered singles.
     * Every one is a complete cube (all faces drawn by the renderer). */
    static const int stash[4][2] = { {3, 0}, {3, 1}, {4, 0}, {4, 1} };
    for (int i = 0; i < 4; i++)
        w->blocks[cx + stash[i][0]][gy + 1][cz + stash[i][1]] = BLOCK_TNT;
    for (int i = 0; i < 6; i++) {
        int x = 2 + (int)(rng_next() % (WORLD_X - 4));
        int z = 2 + (int)(rng_next() % (WORLD_Z - 4));
        int top = 0;
        while (top + 1 < WORLD_Y && w->blocks[x][top + 1][z]) top++;
        if (top + 1 < WORLD_Y) w->blocks[x][top + 1][z] = BLOCK_TNT;
    }
}

int world_count(const World *w, int block) {
    int n = 0;
    for (int x = 0; x < WORLD_X; x++)
        for (int y = 0; y < WORLD_Y; y++)
            for (int z = 0; z < WORLD_Z; z++)
                if (w->blocks[x][y][z] == block) n++;
    return n;
}

static const int DIRS[6][3] = {
    {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}
};

int world_block_visible(const World *w, int x, int y, int z) {
    if (!world_get(w, x, y, z)) return 0;
    for (int i = 0; i < 6; i++)
        if (!world_get(w, x + DIRS[i][0], y + DIRS[i][1], z + DIRS[i][2]))
            return 1;
    return 0;
}

void block_color(int block, float *r, float *g, float *b) {
    switch (block) {
        case BLOCK_GRASS:  *r = 0.35f; *g = 0.75f; *b = 0.30f; break;
        case BLOCK_DIRT:   *r = 0.55f; *g = 0.38f; *b = 0.22f; break;
        case BLOCK_STONE:  *r = 0.55f; *g = 0.56f; *b = 0.58f; break;
        case BLOCK_WOOD:   *r = 0.55f; *g = 0.40f; *b = 0.22f; break;
        case BLOCK_LEAVES: *r = 0.22f; *g = 0.55f; *b = 0.22f; break;
        case BLOCK_SAND:   *r = 0.90f; *g = 0.82f; *b = 0.55f; break;
        case BLOCK_TNT:    *r = 0.90f; *g = 0.18f; *b = 0.12f; break;
        default:           *r = 1.0f;  *g = 0.0f;  *b = 1.0f;  break;
    }
}

const char *block_name(int block) {
    static const char *names[BLOCK_COUNT] = {
        "air", "grass", "dirt", "stone", "wood", "leaves", "sand", "TNT"
    };
    return (block >= 0 && block < BLOCK_COUNT) ? names[block] : "?";
}

int world_raycast(const World *w, Vec3 o, Vec3 d, float max_dist,
                  int *hx, int *hy, int *hz, int *px, int *py, int *pz) {
    int x = (int)floorf(o.x), y = (int)floorf(o.y), z = (int)floorf(o.z);
    int stepx = d.x > 0 ? 1 : -1, stepy = d.y > 0 ? 1 : -1, stepz = d.z > 0 ? 1 : -1;
    float inf = 1e30f;
    float tdx = d.x != 0 ? fabsf(1.0f / d.x) : inf;
    float tdy = d.y != 0 ? fabsf(1.0f / d.y) : inf;
    float tdz = d.z != 0 ? fabsf(1.0f / d.z) : inf;
    float tmx = d.x != 0 ? ((d.x > 0 ? (x + 1 - o.x) : (o.x - x)) * tdx) : inf;
    float tmy = d.y != 0 ? ((d.y > 0 ? (y + 1 - o.y) : (o.y - y)) * tdy) : inf;
    float tmz = d.z != 0 ? ((d.z > 0 ? (z + 1 - o.z) : (o.z - z)) * tdz) : inf;
    *px = x; *py = y; *pz = z;
    float t = 0;
    for (int i = 0; i < 256 && t <= max_dist; i++) {
        if (world_get(w, x, y, z)) {
            *hx = x; *hy = y; *hz = z;
            return 1;
        }
        *px = x; *py = y; *pz = z;
        if (tmx < tmy && tmx < tmz) { x += stepx; t = tmx; tmx += tdx; }
        else if (tmy < tmz)         { y += stepy; t = tmy; tmy += tdy; }
        else                        { z += stepz; t = tmz; tmz += tdz; }
    }
    return 0;
}

Vec3 knockback_impulse(Vec3 blast, Vec3 body_pos) {
    Vec3 d = v3_sub(body_pos, blast);
    d.y = 0;                                   /* horizontal direction */
    float dist = v3_len(d);
    float range = TNT_RADIUS + 1.0f;
    if (dist > range) return v3(0, 0, 0);
    float fall = 1.0f - (dist / range);        /* 1 at centre -> 0 at edge */
    Vec3 dir = dist > 1e-4f ? v3_scale(d, 1.0f / dist) : v3(1, 0, 0);
    Vec3 imp = v3_scale(dir, TNT_POWER * fall);
    imp.y = 4.0f + 6.0f * fall;                /* guaranteed upward pop */
    return imp;
}

int world_detonate(World *w, int x, int y, int z, Vec3 body_pos,
                   Vec3 *out_impulse, int *chained) {
    if (chained) *chained = 0;
    if (out_impulse) *out_impulse = v3(0, 0, 0);
    if (world_get(w, x, y, z) != BLOCK_TNT) return 0;
    Vec3 blast = v3(x + 0.5f, y + 0.5f, z + 0.5f);
    if (out_impulse) *out_impulse = knockback_impulse(blast, body_pos);

    /* Collect the blast first, then clear: chained TNT must not be
     * re-entered while we are still iterating the sphere. */
    int r = (int)ceilf(TNT_RADIUS);
    int destroyed = 0, chain = 0;
    int queue[64][3], qn = 0;
    for (int dx = -r; dx <= r; dx++)
        for (int dy = -r; dy <= r; dy++)
            for (int dz = -r; dz <= r; dz++) {
                Vec3 c = v3(x + dx + 0.5f, y + dy + 0.5f, z + dz + 0.5f);
                if (v3_len(v3_sub(c, blast)) > TNT_RADIUS) continue;
                int bx = x + dx, by = y + dy, bz = z + dz;
                int b = world_get(w, bx, by, bz);
                if (!b) continue;
                if (b == BLOCK_TNT && (dx || dy || dz) && qn < 64) {
                    queue[qn][0] = bx; queue[qn][1] = by; queue[qn][2] = bz; qn++;
                }
                world_set(w, bx, by, bz, BLOCK_AIR);
                destroyed++;
            }
    for (int i = 0; i < qn; i++) {
        /* Neighbour TNT was already cleared above; count it as chained and
         * widen the crater by its own radius without recursing forever. */
        chain++;
        int qx = queue[i][0], qy = queue[i][1], qz = queue[i][2];
        for (int dx = -2; dx <= 2; dx++)
            for (int dy = -2; dy <= 2; dy++)
                for (int dz = -2; dz <= 2; dz++) {
                    if (dx * dx + dy * dy + dz * dz > 6) continue;
                    if (world_get(w, qx + dx, qy + dy, qz + dz)) {
                        world_set(w, qx + dx, qy + dy, qz + dz, BLOCK_AIR);
                        destroyed++;
                    }
                }
    }
    if (chained) *chained = chain;
    return destroyed;
}
