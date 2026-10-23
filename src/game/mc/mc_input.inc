static int mc_handle_touch(float x, float y, int type, int tid) {
    McBtnRect back_b, fly_b, cam_b, inv_b, mine_b, place_b, jump_b, slot_b[MC_HOTBAR_SLOTS];
    int i;
    mc_hud_layout(&back_b, &fly_b, &cam_b, &inv_b, &mine_b, &place_b, &jump_b, slot_b);

    if (type == 0) {
        if (mc_in_rect(x, y, back_b)) {
            if (mc.inv_open)
                mc.inv_open = 0;
            else
                (void)logic_back();
            return 1;
        }
        if (mc.inv_open) {
            float mw = (float)screen_w < 680.0f ? (float)screen_w * 0.90f : 600.0f;
            float mh = mw * 0.72f;
            float mx0 = ((float)screen_w - mw) * 0.5f, my0 = ((float)screen_h - mh) * 0.46f;
            float cell = (mw - 44.0f) / 6.0f;
            for (i = 0; i < 24; i++) {
                float cx = mx0 + 22.0f + (float)(i % 6) * cell;
                float cy = my0 + 54.0f + (float)(i / 6) * (cell * 0.92f);
                if (x >= cx && x <= cx + cell && y >= cy && y <= cy + cell * 0.92f) {
                    mc.hotbar[mc.slot] = mc_creative_list[i];
                    mc.inv_open = 0;
                    mc.banner_t = 2.4f;
                    play_button_click();
                    return 1;
                }
            }
            mc.inv_open = 0;
            return 1;
        }
        if (mc_in_rect(x, y, fly_b)) {
            mc.flying = !mc.flying;
            play_button_click();
            return 1;
        }
        if (mc_in_rect(x, y, cam_b)) {
            mc.third_person = !mc.third_person;
            play_button_click();
            return 1;
        }
        if (mc_in_rect(x, y, inv_b)) {
            mc.inv_open = !mc.inv_open;
            play_button_click();
            return 1;
        }
        for (i = 0; i < MC_HOTBAR_SLOTS; i++) {
            if (mc_in_rect(x, y, slot_b[i])) {
                mc_select_slot(i);
                play_button_click();
                return 1;
            }
        }
        if (mc_in_rect(x, y, mine_b)) {
            mc.pad_mine = 1;
            mc.mouse_mine_click = 1;
            return 1;
        }
        if (mc_in_rect(x, y, place_b)) {
            mc.pad_place_tap = 1;
            return 1;
        }
        if (mc_in_rect(x, y, jump_b)) {
            mc.pad_jump = 1;
            return 1;
        }
        if (x < (float)screen_w * 0.42f && y > (float)screen_h * 0.42f) {
            mc.joy_tid = tid;
            mc.joy_cx = mc.joy_x = x;
            mc.joy_cy = mc.joy_y = y;
            return 1;
        }
        mc.look_tid = tid;
        mc.look_lx = x;
        mc.look_ly = y;
        return 1;
    }
    if (type == 2) {
        if (tid == mc.joy_tid) {
            mc.joy_x = x;
            mc.joy_y = y;
            return 1;
        }
        if (tid == mc.look_tid) {
            mc.yaw -= (x - mc.look_lx) * 0.0052f;
            mc.pitch -= (y - mc.look_ly) * 0.0046f;
            if (mc.pitch > 1.52f)
                mc.pitch = 1.52f;
            if (mc.pitch < -1.52f)
                mc.pitch = -1.52f;
            mc.look_lx = x;
            mc.look_ly = y;
            return 1;
        }
    }
    if (type == 1) {
        if (tid == mc.joy_tid) {
            mc.joy_tid = -1;
            mc.joy_x = mc.joy_cx;
            mc.joy_y = mc.joy_cy;
        }
        if (tid == mc.look_tid)
            mc.look_tid = -999;
        mc.pad_mine = 0;
        mc.pad_jump = 0;
    }
    return 1;
}

static void mc_update_and_draw_lobby_panorama(void) {
    float fdt = (float)(dt > 0.05 ? 0.05 : dt);
    float ang, cx, cz;
    if (!mc.world_ready)
        mc_generate_world("main", 0);
    mc.time_sec += fdt;
    ang = 3.14159f + sinf(mc.time_sec * 0.22f) * 0.52f;
    cx = 32.5f + sinf(mc.time_sec * 0.16f) * 2.4f;
    cz = 31.5f + cosf(mc.time_sec * 0.16f) * 1.4f;
    mc_setup_camera(cx, 15.6f, cz, ang, -0.16f, 0);
    mc.hit = 0;
    mc.mine_on = 0;
    mc_render_world_scene(0);
    /* Subtle dark top/bottom scrim so all original CB4 Lobby cards & buttons pop clearly! */
    rect(0.0f, 0.0f, (float)screen_w, (float)screen_h, 0x360A101Cu);
}

static void mc_run_gameplay_frame(void) {
    float fdt = (float)(dt > 0.05 ? 0.05 : (dt < 0.0005 ? 0.016 : dt));
    const char *want_room = (game_state == ST_ONLINE) ? net_current_room() : "solo";
    float fwd = 0.0f, right = 0.0f, eye_h;
    int want_jump, want_mine, want_place;

    if (!mc.world_ready || mc.is_online_world != (game_state == ST_ONLINE) || strcmp(mc.world_room, want_room) != 0)
        mc_init_session(game_state == ST_ONLINE, want_room);

    mc.time_sec += fdt;
    if (mc.jump_cd > 0.0f)
        mc.jump_cd -= fdt;
    if (mc.hurt_t > 0.0f)
        mc.hurt_t -= fdt;
    if (mc.banner_t > 0.0f)
        mc.banner_t -= fdt;
    if (mc.swing_t > 0.0f) {
        mc.swing_t -= fdt * 4.2f;
        if (mc.swing_t < 0.0f)
            mc.swing_t = 0.0f;
    }

    if (!keyboard_visible() && !chat_open && !mc.inv_open) {
        if (mc.keys['W'] || mc.keys[38])
            fwd += 1.0f;
        if (mc.keys['S'] || mc.keys[40])
            fwd -= 1.0f;
        if (mc.keys['D'] || mc.keys[39])
            right += 1.0f;
        if (mc.keys['A'] || mc.keys[37])
            right -= 1.0f;
        if (mc.joy_tid >= 0) {
            float jdx = (mc.joy_x - mc.joy_cx) / 58.0f;
            float jdy = (mc.joy_y - mc.joy_cy) / 58.0f;
            if (jdx > 1.0f)
                jdx = 1.0f;
            if (jdx < -1.0f)
                jdx = -1.0f;
            if (jdy > 1.0f)
                jdy = 1.0f;
            if (jdy < -1.0f)
                jdy = -1.0f;
            right += jdx;
            fwd -= jdy;
        }
    }
    mc.sneaking = (!keyboard_visible() && !chat_open && mc.keys[16]) ? 1 : 0;
    want_jump = (!keyboard_visible() && !chat_open && !mc.inv_open && (mc.keys[32] || mc.pad_jump)) ? 1 : 0;
    want_mine = (mc.mouse_mine || mc.pad_mine || mc.mouse_mine_click) ? 1 : 0;
    want_place = (mc.mouse_place_click || mc.pad_place_tap) ? 1 : 0;

    mc_step_physics(fdt, fwd, right, want_jump);
    /* Keep legacy 2D player->x / player->y mirrored smoothly for any harnesses inspecting movement! */
    player->x = ((double)mc.px / (double)MC_WX) * (double)screen_w;
    player->y = ((double)mc.pz / (double)MC_WZ) * (double)screen_h;
    player->angle = (double)mc.yaw;

    eye_h = (mc.sneaking ? MC_SNEAK_EYE_H : MC_EYE_H) + sinf(mc.walk_dist * 2.6f) * 0.045f * mc.bob_amp;
    mc_setup_camera(mc.px, mc.py + eye_h, mc.pz, mc.yaw, mc.pitch, mc.third_person);
    mc_update_raycast();
    mc_update_entities_and_mining(fdt, want_mine, want_place);
    if (game_state == ST_ONLINE)
        mc_sync_online_world_and_players(fdt);

    mc_render_world_scene(mc.third_person);
    mc_draw_first_person_hand();
    mc_draw_hud_and_inventory();
    if (game_state == ST_ONLINE) {
        draw_chat();
        draw_banner();
    }
}

static void mc_update_gameplay(void) {
    /* Update online connection lifecycle so room claim/SSE/chat stay 100% live! */
    if (game_state == ST_ONLINE && online_ready == 0) {
        net_set_class(player_class);
        net_connect(FIREBASE, ROOM);
        online_ready = 1;
    }
}

static int mc_is_classic_sim(void) { return mc.classic_sim; }
