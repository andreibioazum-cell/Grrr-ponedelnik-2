#include "game3d.h"
#include <math.h>

void player_init(Player *p, Vec3 spawn) {
    p->pos = spawn; p->vel = v3(0, 0, 0);
    p->yaw = 0; p->pitch = -0.25f; p->walk_phase = 0; p->on_ground = 0;
}

Vec3 player_eye(const Player *p) { return v3(p->pos.x, p->pos.y + PLAYER_EYE, p->pos.z); }

Vec3 player_look_dir(const Player *p) {
    float cp = cosf(p->pitch);
    return v3(sinf(p->yaw) * cp, sinf(p->pitch), -cosf(p->yaw) * cp);
}

void player_apply_impulse(Player *p, Vec3 impulse) {
    /* This is the dynamite knockback: it is added to velocity, so the
     * player is physically thrown and then lands under gravity. */
    p->vel = v3_add(p->vel, impulse);
    if (impulse.y > 0) p->on_ground = 0;
}

static int box_collides(const World *w, Vec3 feet) {
    int x0 = (int)floorf(feet.x - PLAYER_HALF), x1 = (int)floorf(feet.x + PLAYER_HALF);
    int y0 = (int)floorf(feet.y),               y1 = (int)floorf(feet.y + PLAYER_HEIGHT - 0.01f);
    int z0 = (int)floorf(feet.z - PLAYER_HALF), z1 = (int)floorf(feet.z + PLAYER_HALF);
    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++)
            for (int z = z0; z <= z1; z++)
                if (world_get(w, x, y, z)) return 1;
    return 0;
}

/* Move one axis at a time: stable, no tunnelling into blocks. */
static void move_axis(Player *p, const World *w, int axis, float amount) {
    if (amount == 0) return;
    Vec3 next = p->pos;
    if (axis == 0) next.x += amount;
    if (axis == 1) next.y += amount;
    if (axis == 2) next.z += amount;
    if (!box_collides(w, next)) { p->pos = next; return; }
    if (axis == 1) {
        if (amount < 0) p->on_ground = 1;
        p->vel.y = 0;
    }
    if (axis == 0) p->vel.x = 0;
    if (axis == 2) p->vel.z = 0;
}

void player_update(Player *p, const World *w, const Input *in, float dt) {
    if (dt <= 0) return;
    if (dt > 0.05f) dt = 0.05f;               /* fixed-ish step, no spiral */

    p->yaw += in->mouse_dx * 0.0035f;
    p->pitch -= in->mouse_dy * 0.0035f;
    if (p->pitch > 1.45f) p->pitch = 1.45f;
    if (p->pitch < -1.45f) p->pitch = -1.45f;

    float speed = in->sprint ? 8.0f : 4.5f;
    float sy = sinf(p->yaw), cy = cosf(p->yaw);
    Vec3 wish = v3(0, 0, 0);
    if (in->forward) { wish.x += sy;  wish.z -= cy; }
    if (in->back)    { wish.x -= sy;  wish.z += cy; }
    if (in->right)   { wish.x += cy;  wish.z += sy; }
    if (in->left)    { wish.x -= cy;  wish.z -= sy; }
    wish = v3_norm(wish);

    /* Ground control is snappy; air control is weak so an explosion
     * throw cannot be cancelled instantly by holding a key. */
    float accel = p->on_ground ? 1.0f : 0.15f;
    p->vel.x += (wish.x * speed - p->vel.x) * accel;
    p->vel.z += (wish.z * speed - p->vel.z) * accel;

    if (in->jump && p->on_ground) { p->vel.y = 8.0f; p->on_ground = 0; }
    p->vel.y -= 22.0f * dt;                    /* gravity */
    if (p->vel.y < -40.0f) p->vel.y = -40.0f;

    p->on_ground = 0;
    move_axis(p, w, 0, p->vel.x * dt);
    move_axis(p, w, 2, p->vel.z * dt);
    move_axis(p, w, 1, p->vel.y * dt);

    float hs = sqrtf(p->vel.x * p->vel.x + p->vel.z * p->vel.z);
    if (p->on_ground && hs > 0.4f) p->walk_phase += dt * hs * 1.6f;

    /* Never fall out of the world. */
    if (p->pos.y < 1.0f) { p->pos.y = 1.0f; p->vel.y = 0; p->on_ground = 1; }
}
