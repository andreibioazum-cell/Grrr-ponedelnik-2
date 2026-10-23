/* 3D Minecraft HUD (isometric 3D block hotbar, 24-block inventory, 10 hearts,
 * XYZ HUD), multi-touch + PC/Wasm FPS input, and Solo/Online/Lobby 3D loop. */

typedef struct {
    float x, y, w, h;
} McBtnRect;

static void mc_hud_layout(McBtnRect *back_b, McBtnRect *fly_b, McBtnRect *cam_b, McBtnRect *inv_b,
                          McBtnRect *mine_b, McBtnRect *place_b, McBtnRect *jump_b, McBtnRect *slot_b) {
    float sw = (float)screen_w, sh = (float)screen_h;
    float bar_h = sw < sh ? 68.0f : 64.0f;
    float total_w = bar_h * 10.0f + 18.0f;
    float bx0, by0;
    int i;
    if (total_w > sw * 0.86f) {
        total_w = sw * 0.86f;
        bar_h = (total_w - 18.0f) / 10.0f;
    }
    bx0 = (sw - total_w) * 0.5f;
    by0 = sh - bar_h - 14.0f;

    *back_b = (McBtnRect){18.0f, 16.0f, 62.0f, 52.0f};
    *fly_b = (McBtnRect){sw - 224.0f, 16.0f, 96.0f, 46.0f};
    *cam_b = (McBtnRect){sw - 118.0f, 16.0f, 100.0f, 46.0f};

    for (i = 0; i < MC_HOTBAR_SLOTS; i++)
        slot_b[i] = (McBtnRect){bx0 + (float)i * (bar_h + 2.0f), by0, bar_h, bar_h};
    *inv_b = (McBtnRect){bx0 + 9.0f * (bar_h + 2.0f) + 2.0f, by0, bar_h, bar_h};

    *mine_b = (McBtnRect){sw - 196.0f, sh - 236.0f, 84.0f, 74.0f};
    *place_b = (McBtnRect){sw - 102.0f, sh - 236.0f, 84.0f, 74.0f};
    *jump_b = (McBtnRect){sw - 102.0f, sh - 150.0f, 84.0f, 64.0f};
}

static int mc_in_rect(float x, float y, McBtnRect r) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

/* Draws a crisp 3-face isometric 3D Minecraft block icon inside HUD / Inventory! */
static void mc_draw_icon3d(float cx, float cy, float rad, int block_id) {
    float u0, v0, u1, v1;
    uint32_t c_top = 0xFFFFFFFFu, c_left = 0xFFD4D4D4u, c_right = 0xFFA6A6A6u;
    float dx = rad * 0.86f, dy = rad * 0.46f, hz = rad * 0.92f;
    if (block_id <= 0 || block_id >= MC_TOTAL_TYPES)
        return;
    /* Top diamond */
    mc_tile_uv(mc_face_tile((unsigned char)block_id, 0), &u0, &v0, &u1, &v1);
    ds_gfx_quad("mc_atlas.png", cx, cy - hz, u0, v0, c_top, cx + dx, cy - hz + dy, u1, v0, c_top, cx,
                cy - hz + dy * 2.0f, u1, v1, c_top, cx - dx, cy - hz + dy, u0, v1, c_top);
    /* Left side face */
    mc_tile_uv(mc_face_tile((unsigned char)block_id, 2), &u0, &v0, &u1, &v1);
    ds_gfx_quad("mc_atlas.png", cx - dx, cy - hz + dy, u0, v0, c_left, cx, cy - hz + dy * 2.0f, u1, v0, c_left, cx,
                cy + dy * 1.15f, u1, v1, c_left, cx - dx, cy + dy * 0.15f, u0, v1, c_left);
    /* Right side face */
    mc_tile_uv(mc_face_tile((unsigned char)block_id, 4), &u0, &v0, &u1, &v1);
    ds_gfx_quad("mc_atlas.png", cx, cy - hz + dy * 2.0f, u0, v0, c_right, cx + dx, cy - hz + dy, u1, v0, c_right,
                cx + dx, cy + dy * 0.15f, u1, v1, c_right, cx, cy + dy * 1.15f, u0, v1, c_right);
}

static void mc_select_slot(int s) {
    if (s < 0)
        s = MC_HOTBAR_SLOTS - 1;
    if (s >= MC_HOTBAR_SLOTS)
        s = 0;
    mc.slot = s;
    mc.banner_t = 2.2f;
}

static void mc_update_entities_and_mining(float fdt, int want_mine, int want_place) {
    int i;
    /* Update 3D debris particles */
    for (i = 0; i < MC_MAX_PARTICLES; i++) {
        if (!mc.parts[i].active)
            continue;
        mc.parts[i].life -= fdt;
        if (mc.parts[i].life <= 0.0f) {
            mc.parts[i].active = 0;
            continue;
        }
        mc.parts[i].vy -= 18.0f * fdt;
        mc.parts[i].x += mc.parts[i].vx * fdt;
        mc.parts[i].y += mc.parts[i].vy * fdt;
        mc.parts[i].z += mc.parts[i].vz * fdt;
        if (mc_is_solid((int)floorf(mc.parts[i].x), (int)floorf(mc.parts[i].y), (int)floorf(mc.parts[i].z))) {
            mc.parts[i].vy = 0.0f;
            mc.parts[i].vx *= 0.7f;
            mc.parts[i].vz *= 0.7f;
        }
    }
    /* Update primed 3D TNT */
    for (i = 0; i < MC_MAX_TNT; i++) {
        if (!mc.tnts[i].active)
            continue;
        mc.tnts[i].fuse -= fdt;
        mc.tnts[i].vy -= 20.0f * fdt;
        mc.tnts[i].x += mc.tnts[i].vx * fdt;
        if (!mc_is_solid((int)floorf(mc.tnts[i].x), (int)floorf(mc.tnts[i].y + mc.tnts[i].vy * fdt),
                         (int)floorf(mc.tnts[i].z)))
            mc.tnts[i].y += mc.tnts[i].vy * fdt;
        else
            mc.tnts[i].vy = 0.0f;
        mc.tnts[i].z += mc.tnts[i].vz * fdt;
        if (mc.tnts[i].fuse <= 0.0f) {
            mc.tnts[i].active = 0;
            mc_explode_at(mc.tnts[i].x, mc.tnts[i].y + 0.4f, mc.tnts[i].z, 3.3f);
        }
    }
    /* Update Solo 3D NPC characters */
    if (game_state == ST_SOLO) {
        for (i = 0; i < MC_MAX_NPCS; i++) {
            McNpc *n = &mc.npcs[i];
            if (!n->active)
                continue;
            if (n->hp <= 0.0f) {
                n->respawn_t -= fdt;
                if (n->respawn_t <= 0.0f) {
                    n->hp = n->max_hp;
                    n->y = (float)(mc_top_solid_y((int)n->x, (int)n->z) + 1);
                }
                continue;
            }
            {
                float dx = mc.px - n->x, dz = mc.pz - n->z;
                float d = sqrtf(dx * dx + dz * dz);
                n->yaw = atan2f(dx, dz);
                if (d > 1.7f && d < 12.0f && i == 0) {
                    float nx = n->x + (dx / d) * 1.9f * fdt;
                    float nz = n->z + (dz / d) * 1.9f * fdt;
                    if (!mc_aabb_hits_world(nx, n->y, n->z, 0.3f, 1.8f))
                        n->x = nx;
                    else if (n->on_ground) {
                        n->vy = 7.4f;
                        n->on_ground = 0;
                    }
                    if (!mc_aabb_hits_world(n->x, n->y, nz, 0.3f, 1.8f))
                        n->z = nz;
                    n->walk_t += fdt;
                } else {
                    n->yaw = sinf(mc.time_sec * 0.7f + (float)i * 1.9f) * 1.6f;
                    n->walk_t = 0.0f;
                }
                n->vy -= MC_GRAVITY * fdt;
                if (!mc_aabb_hits_world(n->x, n->y + n->vy * fdt, n->z, 0.3f, 1.8f)) {
                    n->y += n->vy * fdt;
                    n->on_ground = 0;
                } else {
                    if (n->vy < 0.0f) {
                        n->y = floorf(n->y + 0.05f) + 0.001f;
                        n->on_ground = 1;
                    }
                    n->vy = 0.0f;
                }
                if (n->hurt_t > 0.0f)
                    n->hurt_t -= fdt;
                if (n->swing_t > 0.0f)
                    n->swing_t -= fdt * 3.5f;
                if (n->atk_cd > 0.0f)
                    n->atk_cd -= fdt;
                if (i == 0 && d < 1.95f && fabsf(mc.py - n->y) < 1.8f && n->atk_cd <= 0.0f) {
                    n->atk_cd = 1.25f;
                    n->swing_t = 1.0f;
                    mc.hurt_t = 0.28f;
                    punch_snd();
                    player->hp -= 1.0;
                    if (player->hp <= 0.0) {
                        player->hp = player->max_hp;
                        mc.px = 32.5f;
                        mc.pz = 29.5f;
                        mc.py = (float)(mc_top_solid_y(32, 29) + 1) + 0.02f;
                    }
                }
            }
        }
    }

    /* Mining / Melee Attack / TNT Ignition */
    if (mc.place_cd > 0.0f)
        mc.place_cd -= fdt;
    if (want_mine && !mc.inv_open) {
        if (mc.hit_npc >= 0 || mc.hit_remote >= 0) {
            if (mc.place_cd <= 0.0f)
                mc_melee_swing();
            mc.mine_on = 0;
        } else if (mc.hit) {
            unsigned char b = mc_get(mc.bx, mc.by, mc.bz);
            if (b == MC_TNT) {
                mc_prime_tnt(mc.bx, mc.by, mc.bz, 2.2f);
                mc.swing_t = 1.0f;
                mc.mine_on = 0;
                mc.mouse_mine_click = 0;
            } else if ((mc_blocks[b].flags & MC_F_UNBREAK) == 0 && b != MC_AIR) {
                if (!mc.mine_on || mc.mx != mc.bx || mc.my != mc.by || mc.mz != mc.bz) {
                    mc.mine_on = 1;
                    mc.mx = mc.bx;
                    mc.my = mc.by;
                    mc.mz = mc.bz;
                    mc.mine_prog = 0.0f;
                }
                if (mc.swing_t <= 0.05f)
                    mc.swing_t = 1.0f;
                mc.mine_prog += fdt / (mc_blocks[b].hardness > 0.06f ? mc_blocks[b].hardness : 0.1f);
                if (((int)(mc.mine_prog * 20.0f) % 5) == 0)
                    mc_spawn_block_particles((float)mc.bx + 0.5f, (float)mc.by + 0.5f, (float)mc.bz + 0.5f, b, 1);
                if (mc.mine_prog >= 1.0f) {
                    mc_spawn_block_particles((float)mc.bx + 0.5f, (float)mc.by + 0.5f, (float)mc.bz + 0.5f, b, 12);
                    mc_set_world_block(mc.bx, mc.by, mc.bz, MC_AIR, 1);
                    play_button_click();
                    mc.mine_on = 0;
                    mc.mine_prog = 0.0f;
                }
            }
        } else if (mc.mouse_mine_click) {
            mc_melee_swing();
        }
    } else {
        mc.mine_on = 0;
        mc.mine_prog = 0.0f;
    }
    mc.mouse_mine_click = 0;

    if (want_place && !mc.inv_open && mc.place_cd <= 0.0f)
        mc_try_place_block();
    mc.mouse_place_click = 0;
    mc.pad_place_tap = 0;
}

static void mc_draw_hud_and_inventory(void) {
    McBtnRect back_b, fly_b, cam_b, inv_b, mine_b, place_b, jump_b, slot_b[MC_HOTBAR_SLOTS];
    float sw = (float)screen_w, sh = (float)screen_h;
    int i, hearts;
    char info[128];

    mc_hud_layout(&back_b, &fly_b, &cam_b, &inv_b, &mine_b, &place_b, &jump_b, slot_b);

    /* Red vignette flash on damage */
    if (mc.hurt_t > 0.0f)
        rect(0.0f, 0.0f, sw, sh, mc_pack_argb(220.0f, 24.0f, 24.0f, mc.hurt_t * 210.0f));

    /* Center Minecraft Crosshair */
    if (!mc.inv_open) {
        uint32_t cc = (mc.hit_npc >= 0 || mc.hit_remote >= 0) ? 0xFFFF4B4Bu : 0xEBFFFFFFu;
        rect(sw * 0.5f - 11.0f, sh * 0.5f - 1.5f, 22.0f, 3.0f, cc);
        rect(sw * 0.5f - 1.5f, sh * 0.5f - 11.0f, 3.0f, 22.0f, cc);
    }

    /* Top-left Back button + XYZ Coordinates & Room pill */
    roundrect(back_b.x, back_b.y, back_b.w, back_b.h, 12.0f, 0xB8141820u);
    text_scaled("<", back_b.x + 21.0f, back_b.y + 8.0f, 0xFFFFFFFFu, 0.85f);

    if (game_state == ST_ONLINE)
        snprintf(info, sizeof(info), "XYZ: %d, %d, %d  |  %s (%d/4)", (int)floorf(mc.px), (int)floorf(mc.py),
                 (int)floorf(mc.pz), net_current_room(), 1 + (mc.rem[0].active + mc.rem[1].active + mc.rem[2].active + mc.rem[3].active));
    else
        snprintf(info, sizeof(info), "XYZ: %d, %d, %d  |  3D MINECRAFT", (int)floorf(mc.px), (int)floorf(mc.py),
                 (int)floorf(mc.pz));
    roundrect(92.0f, 18.0f, (float)strlen(info) * 8.2f + 24.0f, 40.0f, 10.0f, 0x9C10141Cu);
    text_scaled(info, 104.0f, 26.0f, 0xFFEAF4FFu, 0.52f);

    /* Top-right Fly [F] & 3rd-Person [V] toggles */
    roundrect(fly_b.x, fly_b.y, fly_b.w, fly_b.h, 11.0f, mc.flying ? 0xD82EA043u : 0xAA161B24u);
    text_scaled(mc.flying ? "FLY ON" : "FLY [F]", fly_b.x + 12.0f, fly_b.y + 12.0f, 0xFFFFFFFFu, 0.50f);
    roundrect(cam_b.x, cam_b.y, cam_b.w, cam_b.h, 11.0f, mc.third_person ? 0xD82F81F7u : 0xAA161B24u);
    text_scaled(mc.third_person ? "3RD [V]" : "1ST [V]", cam_b.x + 13.0f, cam_b.y + 12.0f, 0xFFFFFFFFu, 0.50f);

    /* Touch / quick-click Break, Place, and Jump action pads */
    if (!mc.inv_open) {
        roundrect(mine_b.x, mine_b.y, mine_b.w, mine_b.h, 16.0f, mc.mine_on ? 0xD8E05246u : 0x92181E28u);
        text_scaled(language == 1 ? "ЛОМАТЬ" : "BREAK", mine_b.x + 9.0f, mine_b.y + 26.0f, 0xFFFFFFFFu, 0.45f);
        roundrect(place_b.x, place_b.y, place_b.w, place_b.h, 16.0f, 0x9E1E56A0u);
        text_scaled(language == 1 ? "СТАВИТЬ" : "PLACE", place_b.x + 7.0f, place_b.y + 26.0f, 0xFFFFFFFFu, 0.44f);
        roundrect(jump_b.x, jump_b.y, jump_b.w, jump_b.h, 16.0f, 0x92181E28u);
        text_scaled(language == 1 ? "ПРЫЖОК" : "JUMP", jump_b.x + 9.0f, jump_b.y + 21.0f, 0xFFFFFFFFu, 0.44f);
        /* Clean left-thumb 3D virtual joystick */
        {
            float jcx = mc.joy_tid >= 0 ? mc.joy_cx : 112.0f;
            float jcy = mc.joy_tid >= 0 ? mc.joy_cy : (sh - 178.0f);
            float jkx = mc.joy_tid >= 0 ? mc.joy_x : jcx;
            float jky = mc.joy_tid >= 0 ? mc.joy_y : jcy;
            circle(jcx, jcy, 58.0f, 0x44FFFFFFu);
            ring(jcx, jcy, 58.0f, 2.5f, 0x88FFFFFFu);
            circle(jkx, jky, 25.0f, 0xAAFFFFFFu);
        }
    }

    /* 10 Minecraft Hearts above left side of hotbar */
    hearts = (int)ceil((player->hp / (player->max_hp > 0.1 ? player->max_hp : 10.0)) * 10.0);
    for (i = 0; i < 10; i++) {
        float hx = slot_b[0].x + (float)i * 22.0f, hy = slot_b[0].y - 24.0f;
        uint32_t hc = i < hearts ? 0xFFE53935u : 0x9028282Eu;
        rect(hx + 2.0f, hy, 5.0f, 5.0f, hc);
        rect(hx + 9.0f, hy, 5.0f, 5.0f, hc);
        rect(hx, hy + 3.0f, 16.0f, 6.0f, hc);
        rect(hx + 3.0f, hy + 9.0f, 10.0f, 4.0f, hc);
        rect(hx + 6.0f, hy + 13.0f, 4.0f, 3.0f, hc);
    }

    /* Selected block name above hotbar */
    if (mc.banner_t > 0.0f) {
        const char *lbl = mc_block_label(mc.hotbar[mc.slot]);
        float lw = (float)strlen(lbl) * 9.0f * 0.56f + 22.0f;
        roundrect((sw - lw) * 0.5f, slot_b[0].y - 58.0f, lw, 28.0f, 8.0f, 0xB410141Cu);
        text_scaled(lbl, (sw - lw) * 0.5f + 11.0f, slot_b[0].y - 54.0f, 0xFFFFF4B8u, 0.56f);
    }

    /* Classic 9-Slot Minecraft Hotbar with real Isometric 3D Block Icons + [...] Inventory button */
    rect(slot_b[0].x - 5.0f, slot_b[0].y - 5.0f, (inv_b.x + inv_b.w) - slot_b[0].x + 10.0f, slot_b[0].h + 10.0f,
         0xCC181A20u);
    for (i = 0; i < MC_HOTBAR_SLOTS; i++) {
        McBtnRect sb = slot_b[i];
        int sel = (i == mc.slot);
        char num[4];
        rect(sb.x, sb.y, sb.w, sb.h, sel ? 0xFFF2F6FAu : 0xCC3A3E48u);
        rect(sb.x + 3.0f, sb.y + 3.0f, sb.w - 6.0f, sb.h - 6.0f, sel ? 0xFF565E6Eu : 0xEE22252Cu);
        mc_draw_icon3d(sb.x + sb.w * 0.5f, sb.y + sb.h * 0.58f, sb.w * 0.36f, mc.hotbar[i]);
        snprintf(num, sizeof(num), "%d", i + 1);
        text_scaled(num, sb.x + 5.0f, sb.y + 3.0f, 0xCCFFFFFFu, 0.40f);
    }
    rect(inv_b.x, inv_b.y, inv_b.w, inv_b.h, mc.inv_open ? 0xFF42D674u : 0xCC3A3E48u);
    rect(inv_b.x + 3.0f, inv_b.y + 3.0f, inv_b.w - 6.0f, inv_b.h - 6.0f, 0xEE22252Cu);
    text_scaled("[E]", inv_b.x + inv_b.w * 0.22f, inv_b.y + inv_b.h * 0.30f, 0xFFFFE680u, 0.52f);

    /* Full 24-Block Creative Inventory Modal ([E]) */
    if (mc.inv_open) {
        float mw = sw < 680.0f ? sw * 0.90f : 600.0f;
        float mh = mw * 0.72f;
        float mx0 = (sw - mw) * 0.5f, my0 = (sh - mh) * 0.46f;
        float cell = (mw - 44.0f) / 6.0f;
        rect(0.0f, 0.0f, sw, sh, 0x8C080B10u);
        roundrect(mx0, my0, mw, mh, 18.0f, 0xF41E222Bu);
        text_scaled(language == 1 ? "ИНВЕНТАРЬ БЛОКОВ [E]" : "CREATIVE BLOCKS [E]", mx0 + 22.0f, my0 + 14.0f,
                    0xFFFFFFFFu, 0.62f);
        for (i = 0; i < 24; i++) {
            int col = i % 6, row = i / 6;
            float cx = mx0 + 22.0f + (float)col * cell;
            float cy = my0 + 54.0f + (float)row * (cell * 0.92f);
            int bid = mc_creative_list[i];
            int active_b = (mc.hotbar[mc.slot] == bid);
            roundrect(cx + 3.0f, cy + 3.0f, cell - 6.0f, cell * 0.92f - 6.0f, 10.0f,
                      active_b ? 0xFF3B82F6u : 0xFF2D3340u);
            mc_draw_icon3d(cx + cell * 0.5f, cy + cell * 0.52f, cell * 0.34f, bid);
        }
    }
}

