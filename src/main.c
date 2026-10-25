/* main.c - headless demo / smoke run of the pure-C 3D game.
 * Simulates walking, renders a third-person frame, ignites the TNT stash,
 * prints the knockback the player received, renders the crater.
 * Usage: ./game3d [seed] [out_prefix]     (writes <prefix>_before.ppm,
 *                                          <prefix>_after.ppm)        */
#include "game3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void render_scene(Game *g, Frame *f, const char *path) {
    /* Third-person camera behind the player: the character is visible
     * as a 3D model, and blocks are anchored to integer world cells. */
    Vec3 eye = player_eye(&g->player);
    Vec3 back = v3(-sinf(g->player.yaw), 0, cosf(g->player.yaw));
    Camera cam;
    cam.pos = v3(eye.x + back.x * 5.0f, eye.y + 2.0f, eye.z + back.z * 5.0f);
    cam.yaw = g->player.yaw; cam.pitch = -0.30f; cam.fov_deg = 70.0f;
    frame_clear(f, 140, 195, 255);
    render_world(f, &g->world, &cam);
    render_player(f, &g->player, &cam, 1);
    if (!frame_write_ppm(f, path)) fprintf(stderr, "cannot write %s\n", path);
}

int main(int argc, char **argv) {
    int seed = argc > 1 ? atoi(argv[1]) : 7;
    const char *prefix = argc > 2 ? argv[2] : "frame";
    char path[512];
    Game g;
    game_init(&g, seed);

    Frame f;
    if (!frame_init(&f, 320, 200)) { fprintf(stderr, "out of memory\n"); return 1; }

    /* Settle on the ground, then walk forward for a second. */
    Input idle; memset(&idle, 0, sizeof(idle));
    for (int i = 0; i < 30; i++) player_update(&g.player, &g.world, &idle, 1.0f / 30.0f);
    Input fwd = idle; fwd.forward = 1;
    for (int i = 0; i < 30; i++) player_update(&g.player, &g.world, &fwd, 1.0f / 30.0f);

    snprintf(path, sizeof(path), "%s_before.ppm", prefix);
    render_scene(&g, &f, path);
    printf("player at %.2f %.2f %.2f, TNT on map: %d\n",
           g.player.pos.x, g.player.pos.y, g.player.pos.z,
           world_count(&g.world, BLOCK_TNT));

    /* Find the nearest TNT, stand next to it and ignite it (F key path). */
    int best = 1 << 30, bx = -1, by = -1, bz = -1;
    for (int x = 0; x < WORLD_X; x++)
        for (int y = 0; y < WORLD_Y; y++)
            for (int z = 0; z < WORLD_Z; z++) {
                if (g.world.blocks[x][y][z] != BLOCK_TNT) continue;
                int d = abs((int)g.player.pos.x - x) + abs((int)g.player.pos.z - z);
                if (d < best) { best = d; bx = x; by = y; bz = z; }
            }
    if (bx >= 0) {
        g.player.pos = v3(bx - 1.5f, (float)by, bz + 0.5f);
        g.player.vel = v3(0, 0, 0);
        g.player.yaw = 1.5707963f;              /* look +x, at the TNT */
        g.player.pitch = -0.51f;                /* aim down from eye to the block */
        Vec3 before = g.player.vel;
        int rc = game_use(&g, 1, 0);
        printf("ignite rc=%d destroyed=%d vel %.2f %.2f %.2f -> %.2f %.2f %.2f\n",
               rc, g.last_destroyed, before.x, before.y, before.z,
               g.player.vel.x, g.player.vel.y, g.player.vel.z);
        for (int i = 0; i < 20; i++) player_update(&g.player, &g.world, &idle, 1.0f / 30.0f);
        printf("after throw, player at %.2f %.2f %.2f\n",
               g.player.pos.x, g.player.pos.y, g.player.pos.z);
    }
    snprintf(path, sizeof(path), "%s_after.ppm", prefix);
    render_scene(&g, &f, path);
    frame_free(&f);
    return 0;
}
