/* geo_script_glue.inc - binds the GeoScript C API (geo_studio.inc) to the
 * live world: blocks, dynamic Parts, the player. Also owns the local
 * animation state machine (idle/walk/run/jump/fall/swim + emotes). */

static int gs_get_block(int x, int y, int z) { return (int)geo_get(x, y, z); }
static void gs_set_block(int x, int y, int z, int b) {
    if (b >= 0 && b < GEO_BLOCK_COUNT)
        geo_set_block(x, y, z, (unsigned char)b, 1);
}
static void gs_explode(float x, float y, float z, float r) { geo_detonate_at(x, y, z, r); }
static int gs_spawn_part(const char *name, float x, float y, float z, float sx, float sy, float sz, int block_id) {
    return geo_part_spawn(name, x, y, z, sx, sy, sz, block_id, -1);
}
static GeoPart *gs_find_part(const char *name) { return geo_part_find(name); }
static void gs_player_pos(float *x, float *y, float *z) {
    *x = geo.px; *y = geo.py; *z = geo.pz;
}
static void gs_player_bounce(float power) {
    if (geo.on_ground || geo.vy <= 0.5f) {
        geo.vy = power;
        geo.on_ground = 0;
        geo_play_sfx("sounds/jump.wav", 0.8f);
    }
}
static int gs_player_on_part(const char *part_name) {
    GeoPart *p = geo_part_find(part_name);
    if (!p)
        return 0;
    return fabsf(geo.px - p->x) < p->sx * 0.5f + 0.28f &&
           fabsf(geo.pz - p->z) < p->sz * 0.5f + 0.28f &&
           fabsf(geo.py - (p->y + p->sy * 0.5f)) < 0.35f;
}
static float gs_time(void) { return geo.time_sec; }
static void gs_print(const char *msg) { ds_log("GeoScript: %s", msg ? msg : ""); }

static const GeoScriptApi geo_script_api_impl = {
    gs_get_block, gs_set_block, gs_explode,
    gs_spawn_part, gs_find_part,
    gs_player_pos, gs_player_bounce, gs_player_on_part,
    gs_time, gs_print
};

/* Pack the 4 material slots into one int for profile/network sync. */
static int geo_skin_pack(void) {
    int s, v = 0;
    for (s = 0; s < SLOT_COUNT; s++)
        v |= (geo.av_skin[s] & 15) << (s * 4);
    return v;
}
static void geo_skin_unpack(int packed, int skin[4]) {
    int s;
    for (s = 0; s < SLOT_COUNT; s++) {
        int v = (packed >> (s * 4)) & 15;
        skin[s] = v < GEO_SKIN_COUNT ? v : geo_skin_default[s];
    }
}

/* Animation state machine: emotes win, then air, water, movement. */
static void geo_update_anim_state(float dt) {
    float hs = sqrtf(geo.vx * geo.vx + geo.vz * geo.vz);
    int want;
    if (geo.emote_t > 0.0f) {
        geo.emote_t -= dt;
        want = geo.emote_id;
    } else if (geo.in_water) {
        want = ANIM_SWIM;
    } else if (!geo.on_ground && !geo.flying) {
        want = geo.vy > 0.5f ? ANIM_JUMP : ANIM_FALL;
    } else if (hs > 6.5f) {
        want = ANIM_RUN;
    } else if (hs > 0.5f) {
        want = ANIM_WALK;
    } else {
        want = ANIM_IDLE;
    }
    if (want != geo.anim_id) {
        geo.anim_id = want;
        geo.anim_t = 0.0f;
    } else {
        geo.anim_t += dt;
    }
}

static void geo_script_engine_tick(float dt) {
    geo_scripts_tick(&geo_script_api_impl, dt);
    geo_update_anim_state(dt);
}
