/* game3d.h - pure-C voxel 3D game. No HTML/JS: rendering is a software
 * 3D rasterizer in C, input is fed in by the platform layer as an
 * Input struct (keyboard/mouse state), so the game itself is portable C99.
 *
 * Fixes for the reported bugs:
 *  - "Blocks float when the camera moves": blocks live at fixed integer
 *    world coordinates. Nothing ever accumulates float drift into a block;
 *    each frame a block's corners are projected from its integer address
 *    and the camera position only. A depth buffer + back-face culling
 *    keeps faces stable at oblique angles.
 *  - "Dynamite is broken": TNT is a full cube - all six faces are emitted
 *    with distinct shading (top has the fuse), and it can be placed, hit
 *    by a raycast and detonated like any other block.
 *  - "Dynamite must knock the player back": world_detonate() returns a
 *    radial impulse (horizontal push + upward pop) applied to the player
 *    velocity.
 *  - "Character is a flat 2D figure": the player is a volumetric model
 *    built from boxes (head, torso, arms, legs) rendered by the same 3D
 *    pipeline, with a walk swing.
 */
#ifndef GAME3D_H
#define GAME3D_H

#include <stddef.h>

/* ---------- small vector math ---------- */
typedef struct { float x, y, z; } Vec3;
Vec3 v3(float x, float y, float z);
Vec3 v3_add(Vec3 a, Vec3 b);
Vec3 v3_sub(Vec3 a, Vec3 b);
Vec3 v3_scale(Vec3 a, float s);
float v3_len(Vec3 a);
Vec3 v3_norm(Vec3 a);
float v3_dot(Vec3 a, Vec3 b);
Vec3 v3_cross(Vec3 a, Vec3 b);

/* ---------- voxel world ---------- */
enum {
    BLOCK_AIR = 0, BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE,
    BLOCK_WOOD, BLOCK_LEAVES, BLOCK_SAND, BLOCK_TNT, BLOCK_COUNT
};

#define WORLD_X 48
#define WORLD_Y 24
#define WORLD_Z 48
#define TNT_RADIUS 3.5f
#define TNT_POWER 14.0f

typedef struct {
    unsigned char blocks[WORLD_X][WORLD_Y][WORLD_Z]; /* [x][y][z], y is up */
    int seed;
} World;

/* Deterministic generation (seeded, no rand()). TNT is placed on purpose. */
void world_generate(World *w, int seed);
int  world_in_bounds(int x, int y, int z);
/* Out of bounds horizontally / above = air, below = solid floor. */
int  world_get(const World *w, int x, int y, int z);
void world_set(World *w, int x, int y, int z, int block);
int  world_count(const World *w, int block);
/* A block is visible only if at least one neighbour is air: buried blocks
 * and interior faces are never drawn, so the cube cannot "swim" inside
 * the terrain. */
int  world_block_visible(const World *w, int x, int y, int z);

/* Block base RGB colour (0..1) and a label for HUD/tests. */
void block_color(int block, float *r, float *g, float *b);
const char *block_name(int block);

/* Voxel raycast (Amanatides & Woo DDA over integer cells).
 * Returns 1 and fills hit cell + previous empty cell (for placing). */
int world_raycast(const World *w, Vec3 origin, Vec3 dir, float max_dist,
                  int *hx, int *hy, int *hz, int *px, int *py, int *pz);

/* Detonate the TNT at (x,y,z): clears a sphere, chains other TNT.
 * Fills `out_impulse` with the knockback for a body at `body_pos`
 * (zero if out of range). Returns number of blocks destroyed.
 * `chained` (may be NULL) receives how many extra TNT blocks went off. */
int world_detonate(World *w, int x, int y, int z, Vec3 body_pos,
                   Vec3 *out_impulse, int *chained);

/* Pure knockback curve, exposed for tests: radial falloff, guaranteed
 * upward pop, zero beyond TNT_RADIUS + body slack. */
Vec3 knockback_impulse(Vec3 blast, Vec3 body_pos);

/* ---------- player (volumetric, AABB physics) ---------- */
typedef struct {
    Vec3 pos;      /* feet centre */
    Vec3 vel;
    float yaw, pitch;   /* camera angles, radians */
    float walk_phase;   /* drives arm/leg swing */
    int on_ground;
} Player;

#define PLAYER_HALF 0.3f
#define PLAYER_HEIGHT 1.8f
#define PLAYER_EYE 1.62f

void player_init(Player *p, Vec3 spawn);
Vec3 player_eye(const Player *p);
Vec3 player_look_dir(const Player *p);
/* Input fed by the platform layer: keys + mouse deltas. No JS/Java here. */
typedef struct {
    int forward, back, left, right, jump, sprint;
    float mouse_dx, mouse_dy;
} Input;

void player_update(Player *p, const World *w, const Input *in, float dt);
/* Apply an explosion: adds impulse to velocity (the knockback). */
void player_apply_impulse(Player *p, Vec3 impulse);

/* ---------- software 3D renderer (C only) ---------- */
typedef struct {
    unsigned char *rgb;   /* w*h*3 */
    float *zbuf;          /* w*h, depth buffer: stops face flicker */
    int w, h;
} Frame;

int  frame_init(Frame *f, int w, int h);
void frame_free(Frame *f);
void frame_clear(Frame *f, unsigned char r, unsigned char g, unsigned char b);

/* Camera: position + yaw/pitch. Blocks are projected from integer world
 * coordinates through this camera every frame - they never move. */
typedef struct { Vec3 pos; float yaw, pitch, fov_deg; } Camera;

void render_world(Frame *f, const World *w, const Camera *cam);
/* The player as boxes, skipped in first-person (draw_player == 0). */
void render_player(Frame *f, const Player *p, const Camera *cam, int draw_player);
/* Highlight the block the camera is looking at (may be NULL). */
void render_highlight(Frame *f, const Camera *cam, const int *hit_xyz);

/* Project a world point; returns 0 if behind the camera. */
int project_point(const Camera *cam, int w, int h, Vec3 p,
                  float *sx, float *sy, float *depth);

/* Write a binary PPM (P6) - used by the headless demo/tests. */
int frame_write_ppm(const Frame *f, const char *path);

/* ---------- game glue used by main/tests ---------- */
/* One fixed-step simulation tick: player + pending explosion. */
typedef struct {
    World world;
    Player player;
    int selected_block;
    int tnt_detonated;   /* total TNT exploded this session */
    int last_destroyed;  /* blocks destroyed by the last explosion */
} Game;

void game_init(Game *g, int seed);
/* Place the currently selected block in the cell before the ray hit,
 * or detonate if the hit block is TNT and `ignite` is set.
 * Returns: 0 nothing, 1 placed, 2 broke block, 3 detonated. */
int game_use(Game *g, int ignite, int place);

#endif
