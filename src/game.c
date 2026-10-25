#include "game3d.h"
#include <math.h>

void game_init(Game *g, int seed) {
    world_generate(&g->world, seed);
    player_init(&g->player, v3(WORLD_X / 2 + 0.5f, 8.0f, WORLD_Z / 2 + 0.5f));
    g->selected_block = BLOCK_TNT;
    g->tnt_detonated = 0;
    g->last_destroyed = 0;
}

int game_use(Game *g, int ignite, int place) {
    Vec3 eye = player_eye(&g->player);
    Vec3 dir = player_look_dir(&g->player);
    int hx, hy, hz, px, py, pz;
    if (!world_raycast(&g->world, eye, dir, 7.0f, &hx, &hy, &hz, &px, &py, &pz))
        return 0;
    int hit = world_get(&g->world, hx, hy, hz);
    if (hit == BLOCK_TNT && ignite) {
        Vec3 body = v3(g->player.pos.x, g->player.pos.y + 0.9f, g->player.pos.z);
        Vec3 impulse;
        int chained = 0;
        g->last_destroyed = world_detonate(&g->world, hx, hy, hz, body, &impulse, &chained);
        player_apply_impulse(&g->player, impulse);   /* <-- the knockback */
        g->tnt_detonated += 1 + chained;
        return 3;
    }
    if (place) {
        if (!world_get(&g->world, px, py, pz)) {
            world_set(&g->world, px, py, pz, g->selected_block);
            return 1;
        }
        return 0;
    }
    world_set(&g->world, hx, hy, hz, BLOCK_AIR);     /* break the block */
    return 2;
}
