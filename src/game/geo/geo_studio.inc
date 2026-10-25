/* geo_studio.inc - Roblox-style studio layer for Geometrium, pure C99.
 *
 * Three systems, all data-driven and all in C:
 *
 * 1. ANIMATIONS: clips are keyframe tables (GeoAnimClip). A clip is a list
 *    of keys; each key holds one angle per joint channel. Sampling lerps
 *    between keys (wrapping for looping clips), so new animations are
 *    just new tables - no code changes. Built-ins: idle, walk, run,
 *    jump, fall, swim, wave, dance. Add yours in geo_anim_clips[].
 *
 * 2. CHARACTER MODELS: a model is a table of box parts (GeoModelPart):
 *    joint it hangs on, offset, size, pivot and material slot. Four
 *    built-in models (Classic, Slim, Robot, Knight) plus 4 material
 *    slots the player can restyle (head/torso/arms/legs). The renderer
 *    walks the table, so a new character model is only new data.
 *
 * 3. C SCRIPTING ("GeoScript"): scripts are plain C functions taking a
 *    GeoScriptApi* - the same API Roblox scripts would call (spawn/move
 *    parts, read/set blocks, bounce the player, print). Register with
 *    GEO_SCRIPT(name) in geo_user_scripts.inc and the engine runs
 *    on_start once per world and on_tick every frame. Dynamic parts
 *    (GeoPart) are script-spawned physical boxes rendered in the 3D
 *    world, like Roblox Workspace parts.
 *
 * This file is intentionally free of engine globals so host tests can
 * include it standalone with a recording GeoScriptApi stub.
 */
#ifndef GEO_STUDIO_INC
#define GEO_STUDIO_INC

#include <math.h>
#include <string.h>

/* ---------------- animation ---------------- */
enum {
    GJ_LEG_L = 0, GJ_LEG_R, GJ_ARM_L, GJ_ARM_R, GJ_HEAD, GJ_BODY, GJ_BOB, GJ_COUNT
};
enum {
    ANIM_IDLE = 0, ANIM_WALK, ANIM_RUN, ANIM_JUMP, ANIM_FALL,
    ANIM_SWIM, ANIM_WAVE, ANIM_DANCE, ANIM_CUSTOM, ANIM_COUNT
};

/* The player's own animation, built joint-by-joint in the Anim Editor.
 * Played as: neutral -> this pose -> neutral, 1 second loop. */
static float geo_custom_pose[GJ_COUNT];
static int geo_custom_joint;   /* joint currently edited in the UI */

#define GEO_ANIM_MAX_KEYS 8
typedef struct {
    float t;                 /* seconds */
    float a[GJ_COUNT];       /* joint angles (rad), GJ_BOB = vertical bob */
} GeoAnimKey;
typedef struct {
    const char *name;
    float duration;
    int loop;
    int nkeys;
    GeoAnimKey keys[GEO_ANIM_MAX_KEYS];
} GeoAnimClip;

static const GeoAnimClip geo_anim_clips[ANIM_COUNT] = {
    /* idle: gentle breathing */
    {"idle", 2.0f, 1, 2, {{0.0f, {0, 0, 0.05f, -0.05f, 0, 0, 0.0f}},
                          {1.0f, {0, 0, -0.05f, 0.05f, 0, 0, 0.02f}}}},
    /* walk */
    {"walk", 0.72f, 1, 2, {{0.0f, {0.62f, -0.62f, -0.48f, 0.48f, 0, 0, 0.03f}},
                           {0.36f, {-0.62f, 0.62f, 0.48f, -0.48f, 0, 0, 0.03f}}}},
    /* run: bigger swing + body lean via GJ_BODY */
    {"run", 0.48f, 1, 2, {{0.0f, {0.95f, -0.95f, -0.85f, 0.85f, 0, 0.16f, 0.06f}},
                          {0.24f, {-0.95f, 0.95f, 0.85f, -0.85f, 0, 0.16f, 0.06f}}}},
    /* jump: legs tucked, arms up (held pose) */
    {"jump", 0.4f, 0, 1, {{0.0f, {0.5f, 0.35f, -2.6f, -2.6f, -0.2f, 0, 0.0f}}}},
    /* fall: legs trailing, arms out */
    {"fall", 0.4f, 0, 1, {{0.0f, {-0.3f, -0.45f, -1.4f, -1.7f, 0.25f, 0, 0.0f}}}},
    /* swim: alternating arm strokes */
    {"swim", 1.0f, 1, 2, {{0.0f, {0.3f, -0.3f, -2.8f, 0.4f, 0, 0.1f, 0.02f}},
                          {0.5f, {-0.3f, 0.3f, 0.4f, -2.8f, 0, 0.1f, 0.05f}}}},
    /* wave: right arm up waving */
    {"wave", 1.2f, 1, 3, {{0.0f, {0, 0, 0, -2.9f, 0, 0, 0.0f}},
                          {0.3f, {0, 0, 0, -2.4f, 0, 0, 0.0f}},
                          {0.6f, {0, 0, 0, -2.9f, 0, 0, 0.0f}}}},
    /* dance: bounce + arms alternating */
    {"dance", 1.0f, 1, 4, {{0.0f, {0.4f, -0.4f, -2.2f, -0.6f, 0, 0.12f, 0.10f}},
                           {0.25f, {-0.4f, 0.4f, -0.6f, -2.2f, 0, -0.12f, 0.0f}},
                           {0.5f, {0.4f, -0.4f, -2.2f, -0.6f, 0, 0.12f, 0.10f}},
                           {0.75f, {-0.4f, 0.4f, -0.6f, -2.2f, 0, -0.12f, 0.0f}}}},
};

static const char *geo_anim_name(int id) {
    if (id == ANIM_CUSTOM)
        return "custom";
    if (id < 0 || id >= ANIM_CUSTOM)
        return "idle";
    return geo_anim_clips[id].name;
}
static const char *geo_joint_name(int j) {
    static const char *names[GJ_COUNT] = {"leg L", "leg R", "arm L", "arm R", "head", "body", "bob"};
    return (j >= 0 && j < GJ_COUNT) ? names[j] : "?";
}

/* Sample a clip at time t (seconds) into out[GJ_COUNT]. */
static void geo_anim_sample(int clip_id, float t, float out[GJ_COUNT]) {
    const GeoAnimClip *c;
    int i, k;
    if (clip_id == ANIM_CUSTOM) {
        float ph = fmodf(t, 1.0f), f;
        if (ph < 0.0f)
            ph += 1.0f;
        f = ph < 0.5f ? ph * 2.0f : (1.0f - ph) * 2.0f;
        for (i = 0; i < GJ_COUNT; i++)
            out[i] = geo_custom_pose[i] * f;
        return;
    }
    if (clip_id < 0 || clip_id >= ANIM_CUSTOM)
        clip_id = ANIM_IDLE;
    c = &geo_anim_clips[clip_id];
    if (c->loop && c->duration > 0.0f) {
        t = fmodf(t, c->duration);
        if (t < 0.0f)
            t += c->duration;
    } else if (t > c->duration) {
        t = c->duration;
    }
    if (t < 0.0f)
        t = 0.0f;
    for (i = 0; i < GJ_COUNT; i++)
        out[i] = c->keys[0].a[i];
    for (k = 0; k + 1 < c->nkeys; k++) {
        float t0 = c->keys[k].t, t1 = c->keys[k + 1].t;
        if (t >= t0 && t <= t1) {
            float f = t1 > t0 ? (t - t0) / (t1 - t0) : 0.0f;
            for (i = 0; i < GJ_COUNT; i++)
                out[i] = c->keys[k].a[i] + (c->keys[k + 1].a[i] - c->keys[k].a[i]) * f;
            return;
        }
    }
    if (c->nkeys > 0)
        for (i = 0; i < GJ_COUNT; i++)
            out[i] = c->keys[c->nkeys - 1].a[i];
}

/* ---------------- character models ---------------- */
enum { GEO_MODEL_CLASSIC = 0, GEO_MODEL_SLIM, GEO_MODEL_ROBOT, GEO_MODEL_KNIGHT, GEO_MODEL_COUNT };
enum { SLOT_HEAD = 0, SLOT_TORSO, SLOT_ARMS, SLOT_LEGS, SLOT_COUNT };

typedef struct {
    const char *name_ru;
    const char *name_en;
    float head;        /* head box half-size */
    float torso_w;     /* torso half width */
    float leg_h;       /* leg length */
    float arm_len;
    int hat;           /* 0 none, 1 cap (torso mat), 2 helm (steel), 3 antenna */
} GeoModelDef;

static const GeoModelDef geo_models[GEO_MODEL_COUNT] = {
    {"Классика", "Classic", 0.24f, 0.25f, 0.72f, 0.66f, 0},
    {"Стройный", "Slim",    0.21f, 0.20f, 0.80f, 0.72f, 1},
    {"Робот",    "Robot",   0.28f, 0.28f, 0.66f, 0.62f, 3},
    {"Рыцарь",   "Knight",  0.25f, 0.29f, 0.70f, 0.64f, 2},
};

/* Material palette for the 4 slots (existing 32x32 game textures). */
#define GEO_SKIN_COUNT 8
static const char *geo_skin_tex[GEO_SKIN_COUNT] = {
    "avatar_head.png", "avatar_body.png", "avatar_limb.png", "marble_top.png",
    "steel_plate.png", "obsidian_grid.png", "amber_vein.png", "cobalt_mosaic.png",
};
/* Default skin index per slot: head, torso, arms, legs. */
static const int geo_skin_default[SLOT_COUNT] = {0, 1, 2, 2};

/* ---------------- dynamic parts (Workspace) ---------------- */
#define GEO_MAX_PARTS 64
typedef struct {
    int active;
    char name[24];
    float x, y, z;         /* centre */
    float sx, sy, sz;      /* full size */
    float yaw;             /* rotation, animated by scripts */
    int block_id;          /* voxel block whose texture skins the part */
    int script_id;         /* owning script index, -1 = static */
} GeoPart;

static GeoPart geo_parts[GEO_MAX_PARTS];

static int geo_part_spawn(const char *name, float x, float y, float z,
                          float sx, float sy, float sz, int block_id, int script_id) {
    int i;
    for (i = 0; i < GEO_MAX_PARTS; i++) {
        if (!geo_parts[i].active) {
            GeoPart *p = &geo_parts[i];
            memset(p, 0, sizeof(*p));
            p->active = 1;
            snprintf(p->name, sizeof(p->name), "%s", name ? name : "Part");
            p->x = x; p->y = y; p->z = z;
            p->sx = sx; p->sy = sy; p->sz = sz;
            p->block_id = block_id;
            p->script_id = script_id;
            return i;
        }
    }
    return -1;
}

static GeoPart *geo_part_find(const char *name) {
    int i;
    for (i = 0; i < GEO_MAX_PARTS; i++)
        if (geo_parts[i].active && strcmp(geo_parts[i].name, name) == 0)
            return &geo_parts[i];
    return NULL;
}

static void geo_parts_clear(void) { memset(geo_parts, 0, sizeof(geo_parts)); }

/* ---------------- C scripting ---------------- */
typedef struct GeoScriptApi {
    /* world */
    int  (*get_block)(int x, int y, int z);
    void (*set_block)(int x, int y, int z, int block_id);
    void (*explode)(float x, float y, float z, float radius);
    /* workspace parts */
    int  (*spawn_part)(const char *name, float x, float y, float z,
                       float sx, float sy, float sz, int block_id);
    GeoPart *(*find_part)(const char *name);
    /* player */
    void (*player_pos)(float *x, float *y, float *z);
    void (*player_bounce)(float power);
    int  (*player_on_part)(const char *part_name);
    /* misc */
    float (*time)(void);
    void (*print)(const char *msg);
} GeoScriptApi;

typedef struct {
    const char *name;
    void (*on_start)(GeoScriptApi *api);
    void (*on_tick)(GeoScriptApi *api, float dt);
} GeoScriptDef;

#include "geo_user_scripts.inc"

static const GeoScriptApi *geo_script_api;
static int geo_scripts_started;

static void geo_scripts_start(const GeoScriptApi *api) {
    int i;
    geo_parts_clear();
    geo_script_api = api;
    geo_scripts_started = 1;
    for (i = 0; i < GEO_SCRIPT_COUNT; i++)
        if (geo_scripts[i].on_start)
            geo_scripts[i].on_start((GeoScriptApi *)api);
}

static void geo_scripts_tick(const GeoScriptApi *api, float dt) {
    int i;
    if (!geo_scripts_started)
        geo_scripts_start(api);
    for (i = 0; i < GEO_SCRIPT_COUNT; i++)
        if (geo_scripts[i].on_tick)
            geo_scripts[i].on_tick((GeoScriptApi *)api, dt);
}

#endif /* GEO_STUDIO_INC */
