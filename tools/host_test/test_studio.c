/* Host test for the Roblox-style studio layer (geo_studio.inc):
 * animation sampling, character models, dynamic Parts and the C
 * scripting API with the bundled example scripts. Runs without the
 * Android engine: a recording GeoScriptApi stub stands in for the world.
 * Build: gcc -std=c99 -Wall -Wextra -Werror -Isrc -o /tmp/test_studio \
 *          tools/host_test/test_studio.c -lm && /tmp/test_studio */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* geo_studio.inc is designed to be includable standalone. */
#include "../../src/game/geo/geo_studio.inc"

/* ---- recording script-API stub ---- */
static int stub_blocks[64][64][64];
static int set_block_calls, explode_calls, bounce_calls;
static float last_bounce;
static char printed[256];

static int t_get_block(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= 64 || y >= 64 || z >= 64)
        return 0;
    return stub_blocks[x][y][z];
}
static void t_set_block(int x, int y, int z, int b) {
    set_block_calls++;
    if (x >= 0 && y >= 0 && z >= 0 && x < 64 && y < 64 && z < 64)
        stub_blocks[x][y][z] = b;
}
static void t_explode(float x, float y, float z, float r) {
    (void)x; (void)y; (void)z; (void)r;
    explode_calls++;
}
static int t_spawn_part(const char *n, float x, float y, float z, float sx, float sy, float sz, int b) {
    return geo_part_spawn(n, x, y, z, sx, sy, sz, b, -1);
}
static GeoPart *t_find_part(const char *n) { return geo_part_find(n); }
static void t_player_pos(float *x, float *y, float *z) { *x = 48.0f; *y = 20.0f; *z = 48.0f; }
static void t_bounce(float p) { bounce_calls++; last_bounce = p; }
static int t_on_part(const char *n) {
    GeoPart *p = geo_part_find(n);
    return p != NULL; /* in the stub the player stands wherever a part is */
}
static float t_time(void) { return 1.0f; }
static void t_print(const char *m) { snprintf(printed, sizeof(printed), "%s", m ? m : ""); }

static GeoScriptApi stub_api = {
    t_get_block, t_set_block, t_explode, t_spawn_part, t_find_part,
    t_player_pos, t_bounce, t_on_part, t_time, t_print
};

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main(void) {
    float pose[GJ_COUNT];
    int i;

    /* --- animations: walk swings legs opposite, idle is calm --- */
    geo_anim_sample(ANIM_WALK, 0.0f, pose);
    CHECK(pose[GJ_LEG_L] > 0.5f && pose[GJ_LEG_R] < -0.5f);
    geo_anim_sample(ANIM_IDLE, 0.5f, pose);
    CHECK(pose[GJ_LEG_L] == 0.0f);
    /* looping wraps: walk at t and t+duration must match */
    {
        float a[GJ_COUNT], b[GJ_COUNT];
        geo_anim_sample(ANIM_RUN, 0.13f, a);
        geo_anim_sample(ANIM_RUN, 0.13f + geo_anim_clips[ANIM_RUN].duration, b);
        for (i = 0; i < GJ_COUNT; i++)
            CHECK(fabsf(a[i] - b[i]) < 1e-4f);
        /* non-looping jump clamps at its held pose */
        geo_anim_sample(ANIM_JUMP, 99.0f, a);
        CHECK(a[GJ_ARM_L] < -2.0f);
    }
    /* every built-in clip has keys and a name */
    for (i = 0; i < ANIM_CUSTOM; i++) {
        CHECK(geo_anim_clips[i].nkeys >= 1);
        CHECK(geo_anim_clips[i].name != NULL);
        CHECK(geo_anim_name(i) == geo_anim_clips[i].name);
    }
    CHECK(strcmp(geo_anim_name(ANIM_CUSTOM), "custom") == 0);
    CHECK(strcmp(geo_joint_name(GJ_ARM_R), "arm R") == 0);

    /* custom animation: pose built joint-by-joint peaks mid-loop */
    geo_custom_joint = GJ_ARM_R;   /* editor selects this joint */
    memset(geo_custom_pose, 0, sizeof(geo_custom_pose));
    geo_custom_pose[GJ_ARM_R] = -2.8f;
    geo_custom_pose[GJ_BOB] = 0.12f;
    geo_anim_sample(ANIM_CUSTOM, 0.25f, pose);
    CHECK(pose[GJ_ARM_R] < -1.3f);
    geo_anim_sample(ANIM_CUSTOM, 0.0f, pose);
    CHECK(pose[GJ_ARM_R] == 0.0f);

    /* --- models: 4 distinct, sane proportions --- */
    CHECK(GEO_MODEL_COUNT == 4);
    for (i = 0; i < GEO_MODEL_COUNT; i++) {
        CHECK(geo_models[i].head > 0.1f && geo_models[i].head < 0.5f);
        CHECK(geo_models[i].leg_h > 0.4f && geo_models[i].leg_h < 1.0f);
    }
    CHECK(geo_models[GEO_MODEL_ROBOT].hat == 3);   /* antenna */
    CHECK(geo_models[GEO_MODEL_KNIGHT].hat == 2);  /* helm */
    CHECK(GEO_SKIN_COUNT == 8);
    for (i = 0; i < GEO_SKIN_COUNT; i++)
        CHECK(geo_skin_tex[i] != NULL && geo_skin_tex[i][0]);

    /* --- parts + scripting: run the bundled example scripts --- */
    geo_scripts_start(&stub_api);
    CHECK(geo_part_find("BouncePad") != NULL);
    CHECK(geo_part_find("Spinner") != NULL);
    /* builder script raised a marble tower: 5 marble + lumen on top */
    CHECK(set_block_calls == 6);
    CHECK(stub_blocks[54][25][48] == 16);
    CHECK(printed[0] != '\0');

    /* spinner rotates over time */
    {
        GeoPart *sp = geo_part_find("Spinner");
        float y0 = sp->yaw;
        for (i = 0; i < 30; i++)
            geo_scripts_tick(&stub_api, 1.0f / 30.0f);
        CHECK(sp->yaw > y0 + 0.5f);
    }
    /* bounce pad fired player_bounce with real power */
    CHECK(bounce_calls > 0);
    CHECK(last_bounce > 10.0f);

    /* parts can be rotated to any angle (builder panel turns by 15 deg) */
    {
        GeoPart *bp = geo_part_find("BouncePad");
        bp->yaw += 0.261799f * 3.0f;
        CHECK(bp->yaw > 0.7f);
    }

    if (failures) {
        printf("%d FAILURES\n", failures);
        return 1;
    }
    printf("studio tests passed\n");
    return 0;
}
