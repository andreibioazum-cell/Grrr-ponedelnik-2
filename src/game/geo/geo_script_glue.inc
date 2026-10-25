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

/* Publish local state & interpolate remote players (called each tick). */
static void geo_sync_remote_players(float fdt) {
if (geo.online) {
    GeoRemotePeer peers[GEO_MAX_REMOTES];
    int pn, r, i;
    net_publish_local(geo.px, geo.py, geo.pz, geo.face_yaw, geo.pitch, (float)geo.swing_seq, geo.hotbar[geo.slot],
                      geo.flying, geo.av_model, geo_skin_pack(), geo.anim_id);
    pn = net_copy_remotes(peers, GEO_MAX_REMOTES);
    for (r = 0; r < GEO_MAX_REMOTES; r++)
        geo.rem[r].active = 0;
    for (i = 0; i < pn; i++) {
        GeoRemoteAvatar *av = &geo.rem[i];
        float dx = peers[i].x - av->x, dy = peers[i].y - av->y, dz = peers[i].z - av->z;
        int fresh = strcmp(av->uid, peers[i].uid) != 0;
        snprintf(av->uid, sizeof(av->uid), "%s", peers[i].uid);
        snprintf(av->name, sizeof(av->name), "%s", peers[i].name);
        av->active = 1;
        av->held = peers[i].held;
        av->model = peers[i].model >= 0 && peers[i].model < 4 ? peers[i].model : 0;
        geo_skin_unpack(peers[i].skin, av->skin);
        if (peers[i].anim != av->anim) {
            av->anim = peers[i].anim;
            av->anim_t = 0.0f;
        } else {
            av->anim_t += fdt;
        }
        if (fresh || (dx * dx + dy * dy + dz * dz) > 64.0f) {
            av->x = peers[i].x;
            av->y = peers[i].y;
            av->z = peers[i].z;
            av->yaw = peers[i].yaw;
            av->pitch = peers[i].pitch;
            av->last_swing = peers[i].swing;
        } else {
            float k = fdt * 10.5f > 1.0f ? 1.0f : fdt * 10.5f;
            float dyaw = peers[i].yaw - av->yaw;
            while (dyaw > 3.14159265f)
                dyaw -= 6.2831853f;
            while (dyaw < -3.14159265f)
                dyaw += 6.2831853f;
            if (fabsf(dx) + fabsf(dz) > 0.015f)
                av->walk_t += fdt * 8.5f;
            av->x += dx * k;
            av->y += dy * k;
            av->z += dz * k;
            av->yaw += dyaw * k;
            av->pitch += (peers[i].pitch - av->pitch) * k;
            if (peers[i].swing != av->last_swing) {
                av->last_swing = peers[i].swing;
                av->swing_t = 1.0f;
            }
        }
        if (av->swing_t > 0.0f) {
            av->swing_t -= fdt * 4.0f;
            if (av->swing_t < 0.0f)
                av->swing_t = 0.0f;
        }
    }
}
}

/* ---- Roblox Studio-style Part builder (used by the in-game panel) ---- */
static int geo_part_counter;

static void geo_part_broadcast(GeoPart *p) {
    GeoPartNet np;
    if (!geo.online || !p)
        return;
    memset(&np, 0, sizeof(np));
    snprintf(np.name, sizeof(np.name), "%s", p->name);
    np.x = p->x; np.y = p->y; np.z = p->z;
    np.sx = p->sx; np.sy = p->sy; np.sz = p->sz; np.yaw = p->yaw;
    np.block = p->active ? p->block_id : -1;
    net_part_send(&np);
}

static void geo_build_create(void) {
    char name[24];
    float px, py, pz;
    int idx;
    snprintf(name, sizeof(name), "Part%d", ++geo_part_counter);
    if (geo.hit) {
        px = (float)geo.bx + 0.5f;
        py = (float)geo.by + 1.5f;
        pz = (float)geo.bz + 0.5f;
    } else {
        px = geo.px + sinf(geo.yaw) * 3.0f;
        py = geo.py + 1.0f;
        pz = geo.pz + cosf(geo.yaw) * 3.0f;
    }
    idx = geo_part_spawn(name, px, py, pz, 1.4f, 0.7f, 1.4f, geo.hotbar[geo.slot], -1);
    if (idx >= 0) {
        geo.part_sel = idx;
        geo_part_broadcast(&geo_parts[idx]);
        geo_play_sfx("sounds/block_place.wav", 0.8f);
    }
}

static GeoPart *geo_build_selected(void) {
    if (geo.part_sel >= 0 && geo.part_sel < GEO_MAX_PARTS && geo_parts[geo.part_sel].active)
        return &geo_parts[geo.part_sel];
    return NULL;
}

static void geo_build_rotate(void) {
    GeoPart *p = geo_build_selected();
    if (p) {
        p->yaw += 0.261799f;   /* 15 degrees: turn parts any way you like */
        geo_part_broadcast(p);
    }
}

static void geo_build_resize(void) {
    GeoPart *p = geo_build_selected();
    if (p) {
        float v = p->sx < 1.8f ? 2.8f : (p->sx < 3.4f ? 0.9f : 1.4f);
        p->sx = p->sz = v;
        p->sy = v * 0.5f;
        geo_part_broadcast(p);
    }
}

static void geo_build_material(void) {
    GeoPart *p = geo_build_selected();
    if (p) {
        p->block_id = geo_palette_blocks[(geo.slot + 1) % GEO_PALETTE_COUNT];
        geo_part_broadcast(p);
    }
}

static void geo_build_delete(void) {
    GeoPart *p = geo_build_selected();
    if (p) {
        p->active = 0;
        geo_part_broadcast(p);
        geo.part_sel = -1;
    }
}

/* Shared geometry for the 5-button builder panel (draw + touch). */
static void geo_build_panel_rects(GeoRect out[5]) {
    float w = (float)screen_w, y = (float)screen_h - 196.0f;
    float bw = 176.0f, gap = 10.0f, x0 = (w - (bw * 5.0f + gap * 4.0f)) * 0.5f;
    int i;
    for (i = 0; i < 5; i++)
        out[i] = (GeoRect){x0 + (float)i * (bw + gap), y, bw, 56.0f};
}
static void geo_build_action(int i) {
    if (i == 0)
        geo_build_create();
    else if (i == 1)
        geo_build_rotate();
    else if (i == 2)
        geo_build_resize();
    else if (i == 3)
        geo_build_material();
    else if (i == 4)
        geo_build_delete();
    geo_play_sfx("sounds/ui_tap.wav", 0.8f);
}

/* Apply Part edits arriving from the room (other players' creations). */
static void geo_apply_remote_parts(void) {
    GeoPartNet np;
    int guard = 0;
    while (guard++ < 64 && net_part_poll(&np)) {
        GeoPart *p = geo_part_find(np.name);
        if (np.block < 0) {
            if (p)
                p->active = 0;
            continue;
        }
        if (!p) {
            int idx = geo_part_spawn(np.name, np.x, np.y, np.z, np.sx, np.sy, np.sz, np.block, -2);
            if (idx < 0)
                continue;
            p = &geo_parts[idx];
        }
        p->x = np.x; p->y = np.y; p->z = np.z;
        p->sx = np.sx; p->sy = np.sy; p->sz = np.sz;
        p->yaw = np.yaw;
        p->block_id = np.block;
    }
}

static void geo_script_engine_tick(float dt) {
    geo_scripts_tick(&geo_script_api_impl, dt);
    geo_update_anim_state(dt);
}
