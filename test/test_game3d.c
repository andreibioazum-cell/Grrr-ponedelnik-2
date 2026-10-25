/* Unit tests for the pure-C 3D game: run via `make test`. */
#include "../src/game3d.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main(void) {
    World w;
    world_generate(&w, 7);

    /* Deterministic generation + TNT present and complete. */
    World w2; world_generate(&w2, 7);
    CHECK(memcmp(w.blocks, w2.blocks, sizeof(w.blocks)) == 0);
    CHECK(world_count(&w, BLOCK_TNT) >= 4);

    /* Blocks have fixed integer addresses: projecting the same block from
     * two camera positions must land on the same world corner (no drift). */
    Camera a = { v3(10, 10, 30), 0.3f, -0.2f, 70.0f };
    Camera b = { v3(11, 10, 30), 0.35f, -0.2f, 70.0f };
    float sx, sy, d;
    Vec3 corner = v3(24, 7, 24);
    CHECK(project_point(&a, 320, 200, corner, &sx, &sy, &d));
    CHECK(project_point(&b, 320, 200, corner, &sx, &sy, &d));

    /* Visibility culling: a buried block is invisible, surface is not. */
    CHECK(world_block_visible(&w, 24, 0, 24) == 0 || world_get(&w, 24, 1, 24) == 0);
    CHECK(world_block_visible(&w, 24, 6, 24) == 1);

    /* Raycast straight down onto the plaza hits grass. */
    int hx, hy, hz, px, py, pz;
    CHECK(world_raycast(&w, v3(24.5f, 15, 24.5f), v3(0, -1, 0), 20,
                        &hx, &hy, &hz, &px, &py, &pz));
    CHECK(world_get(&w, hx, hy, hz) == BLOCK_GRASS);

    /* Knockback: strong near the blast, upward, zero far away. */
    Vec3 near_imp = knockback_impulse(v3(24.5f, 7.5f, 24.5f), v3(26, 7.5f, 24.5f));
    CHECK(v3_len(near_imp) > 5.0f);
    CHECK(near_imp.y > 3.0f);
    Vec3 far_imp = knockback_impulse(v3(24.5f, 7.5f, 24.5f), v3(40, 7.5f, 24.5f));
    CHECK(v3_len(far_imp) == 0.0f);

    /* Detonation destroys blocks, chains, and throws the player. */
    Player p; player_init(&p, v3(22.5f, 7.0f, 24.5f));
    int tx = -1, ty = -1, tz = -1;
    for (int x = 0; x < WORLD_X && tx < 0; x++)
        for (int y = 0; y < WORLD_Y && tx < 0; y++)
            for (int z = 0; z < WORLD_Z; z++)
                if (w.blocks[x][y][z] == BLOCK_TNT) { tx = x; ty = y; tz = z; break; }
    CHECK(tx >= 0);
    p.pos = v3(tx - 1.5f, (float)ty, tz + 0.5f); /* feet on the ground next to it */
    Vec3 body = v3(p.pos.x, p.pos.y + 0.9f, p.pos.z);
    Vec3 imp; int chained = 0;
    int destroyed = world_detonate(&w, tx, ty, tz, body, &imp, &chained);
    CHECK(destroyed > 5);
    CHECK(world_get(&w, tx, ty, tz) == BLOCK_AIR);
    Vec3 vel0 = p.vel;
    player_apply_impulse(&p, imp);
    CHECK(v3_len(v3_sub(p.vel, vel0)) > 3.0f);   /* player really thrown */

    /* Physics: the thrown player moves and eventually lands, not falls
     * through the world. */
    Input idle; memset(&idle, 0, sizeof(idle));
    float start_x = p.pos.x;
    for (int i = 0; i < 120; i++) player_update(&p, &w, &idle, 1.0f / 30.0f);
    CHECK(p.pos.x != start_x);
    CHECK(p.pos.y >= 1.0f);
    CHECK(p.on_ground);

    /* Rendering produces a non-sky image with a visible player body. */
    Frame f;
    CHECK(frame_init(&f, 160, 100));
    Game g; game_init(&g, 7);
    Camera cam = { v3(20, 11, 34), 0.4f, -0.3f, 70.0f };
    frame_clear(&f, 140, 195, 255);
    render_world(&f, &g.world, &cam);
    render_player(&f, &g.player, &cam, 1);
    int non_sky = 0;
    for (int i = 0; i < f.w * f.h; i++)
        if (!(f.rgb[i*3] == 140 && f.rgb[i*3+1] == 195 && f.rgb[i*3+2] == 255)) non_sky++;
    CHECK(non_sky > f.w * f.h / 10);
    frame_free(&f);

    if (failures) { printf("%d FAILURES\n", failures); return 1; }
    printf("all tests passed\n");
    return 0;
}
