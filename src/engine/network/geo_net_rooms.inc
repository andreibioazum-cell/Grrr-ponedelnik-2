/* Real-time room presence, 16-player 3D sync, persistent blocks.json sync, chat & room browser. */

static void geo_ring_push_in_locked(int x, int y, int z, int id) {
    int next = (net.in_head + 1) % BLOCK_RING_CAP;
    if (next == net.in_tail)
        net.in_tail = (net.in_tail + 1) % BLOCK_RING_CAP;
    net.inbox[net.in_head] = (GeoBlockEdit){x, y, z, id};
    net.in_head = next;
}

static void geo_parse_blocks_json(const char *json, unsigned int gen, unsigned char *shadow, int shadow_len) {
    const char *p = json;
    if (!p || *p != '{')
        return;
    p++;
    while (*p) {
        char key[32];
        int x, y, z, id;
        char *endp = NULL;
        while (*p && *p != '"' && *p != '}')
            p++;
        if (*p != '"')
            break;
        p = geo_json_read_str(p + 1, key, sizeof(key));
        while (*p && *p != ':')
            p++;
        if (*p == ':')
            p++;
        while (*p == ' ' || *p == '\t')
            p++;
        id = (int)strtol(p, &endp, 10);
        if (endp && endp != p && sscanf(key, "%d_%d_%d", &x, &y, &z) == 3) {
            if (x >= 0 && x < 96 && y >= 0 && y < 48 && z >= 0 && z < 96 && id >= 0 && id < 32) {
                int idx = (y * 96 + z) * 96 + x;
                if (idx >= 0 && idx < shadow_len && shadow[idx] != (unsigned char)(id + 1)) {
                    shadow[idx] = (unsigned char)(id + 1);
                    ds_mutex_lock(net_lock);
                    if (net.run && net.gen == gen)
                        geo_ring_push_in_locked(x, y, z, id);
                    ds_mutex_unlock(net_lock);
                }
            }
            p = endp;
        } else if (*p == '{' || *p == '[') {
            p = geo_skip_balanced(p);
        } else {
            while (*p && *p != ',' && *p != '}')
                p++;
        }
    }
}

static void geo_parse_players_json(const char *json, const char *self_uid, unsigned int gen, int *out_total) {
    GeoRemotePeer parsed[GEO_MAX_REMOTES];
    double ts_arr[GEO_MAX_REMOTES];
    double my_ts = 0.0, max_ts = 0.0;
    int count = 0, alive_total = 1, i;
    const char *p = json;

    if (p && *p == '{') {
        p++;
        while (*p && count < GEO_MAX_REMOTES) {
            char uid[24];
            const char *obj_s, *obj_e;
            while (*p && *p != '"' && *p != '}')
                p++;
            if (*p != '"')
                break;
            p = geo_json_read_str(p + 1, uid, sizeof(uid));
            while (*p && *p != ':')
                p++;
            if (*p == ':')
                p++;
            while (*p == ' ')
                p++;
            if (*p != '{') {
                p = (*p == '[') ? geo_skip_balanced(p) : p + 1;
                continue;
            }
            obj_s = p;
            obj_e = geo_skip_balanced(p);
            p = obj_e;

            if (strcmp(uid, self_uid) == 0) {
                my_ts = geo_obj_num(obj_s, obj_e, "ts", 0.0);
                if (my_ts > max_ts)
                    max_ts = my_ts;
                continue;
            }
            if ((int)geo_obj_num(obj_s, obj_e, "live", 0.0) != 1)
                continue;

            memset(&parsed[count], 0, sizeof(parsed[0]));
            parsed[count].active = 1;
            snprintf(parsed[count].uid, sizeof(parsed[0].uid), "%s", uid);
            geo_obj_str(obj_s, obj_e, "name", parsed[count].name, sizeof(parsed[0].name));
            if (!parsed[count].name[0])
                snprintf(parsed[count].name, sizeof(parsed[0].name), "Player");
            parsed[count].x = (float)geo_obj_num(obj_s, obj_e, "x", 48.0);
            parsed[count].y = (float)geo_obj_num(obj_s, obj_e, "y", 20.0);
            parsed[count].z = (float)geo_obj_num(obj_s, obj_e, "z", 48.0);
            parsed[count].yaw = (float)geo_obj_num(obj_s, obj_e, "yaw", 0.0);
            parsed[count].pitch = (float)geo_obj_num(obj_s, obj_e, "pitch", 0.0);
            parsed[count].swing = (float)geo_obj_num(obj_s, obj_e, "swing", 0.0);
            parsed[count].held = (int)geo_obj_num(obj_s, obj_e, "held", 1.0);
            parsed[count].fly = (int)geo_obj_num(obj_s, obj_e, "fly", 0.0);
            ts_arr[count] = geo_obj_num(obj_s, obj_e, "ts", 0.0);
            if (ts_arr[count] > max_ts)
                max_ts = ts_arr[count];
            count++;
        }
    }

    {
        double ref_ts = my_ts > 0.0 ? my_ts : max_ts;
        GeoRemotePeer live_peers[GEO_MAX_REMOTES];
        int live_n = 0;
        for (i = 0; i < count; i++) {
            if (ref_ts > 0.0 && ts_arr[i] > 0.0 && fabs(ref_ts - ts_arr[i]) > 12000.0)
                continue;
            live_peers[live_n++] = parsed[i];
        }
        alive_total = 1 + live_n;
        if (out_total)
            *out_total = alive_total;
        ds_mutex_lock(net_lock);
        if (net.run && net.gen == gen) {
            memcpy(net.peers, live_peers, (size_t)live_n * sizeof(GeoRemotePeer));
            net.peer_n = live_n;
        }
        ds_mutex_unlock(net_lock);
    }
}

static void geo_parse_chat_json(const char *json, unsigned int gen) {
    GeoChatMsg msgs[GEO_MAX_CHAT];
    int n = 0;
    const char *p = json;
    if (p && *p == '{') {
        p++;
        while (*p) {
            char key[40], sender[24], text[96];
            const char *s, *e;
            while (*p && *p != '"' && *p != '}')
                p++;
            if (*p != '"')
                break;
            p = geo_json_read_str(p + 1, key, sizeof(key));
            while (*p && *p != ':')
                p++;
            if (*p == ':')
                p++;
            while (*p == ' ')
                p++;
            if (*p != '{') {
                p = (*p == '[') ? geo_skip_balanced(p) : p + 1;
                continue;
            }
            s = p;
            e = geo_skip_balanced(p);
            p = e;
            geo_obj_str(s, e, "n", sender, sizeof(sender));
            geo_obj_str(s, e, "m", text, sizeof(text));
            if (!text[0])
                continue;
            if (n == GEO_MAX_CHAT) {
                memmove(&msgs[0], &msgs[1], (GEO_MAX_CHAT - 1) * sizeof(GeoChatMsg));
                n = GEO_MAX_CHAT - 1;
            }
            snprintf(msgs[n].sender, sizeof(msgs[n].sender), "%s", sender[0] ? sender : "Player");
            snprintf(msgs[n].text, sizeof(msgs[n].text), "%s", text);
            n++;
        }
    }
    ds_mutex_lock(net_lock);
    if (net.run && net.gen == gen) {
        memcpy(net.chat, msgs, (size_t)n * sizeof(GeoChatMsg));
        net.chat_n = n;
    }
    ds_mutex_unlock(net_lock);
}

static void *writer_thread_fn(void *arg) {
    unsigned int gen = (unsigned int)(uintptr_t)arg;
    char base[256], room[24], uid[24], nick[24], url[384], body[4096];
    char pbody_parts[4096];
    int part_batch = 0, spectate_flag = 0;
    long long last_pos = 0, last_idx = 0;

    for (;;) {
        long long now = geo_now_ms();
        float x, y, z, yaw, pitch, swing;
        int held, fly, batch_n = 0;
        int model = 0, skin = 0, anim = 0;

        ds_mutex_lock(net_lock);
        if (!net.run || net.gen != gen) {
            ds_mutex_unlock(net_lock);
            break;
        }
        snprintf(base, sizeof(base), "%s", net.base);
        snprintf(room, sizeof(room), "%s", net.room);
        snprintf(uid, sizeof(uid), "%s", net.uid);
        snprintf(nick, sizeof(nick), "%s", net.nick);
        x = net.lx;
        y = net.ly;
        z = net.lz;
        yaw = net.lyaw;
        pitch = net.lpitch;
        swing = net.lswing;
        held = net.lheld;
        model = net.lmodel;
        skin = net.lskin;
        anim = net.lanim;
        fly = net.lfly;
        {
            /* Drain queued Part edits into a PATCH body for /parts.json */
            size_t poff = 1;
            pbody_parts[0] = '{';
            pbody_parts[1] = '\0';
            part_batch = 0;
            while (net.part_out_tail != net.part_out_head && part_batch < 24 &&
                   poff + 180 < sizeof(pbody_parts)) {
                GeoPartNet pt = net.part_out[net.part_out_tail];
                net.part_out_tail = (net.part_out_tail + 1) % 256;
                if (pt.block < 0) {
                    poff += (size_t)snprintf(pbody_parts + poff, sizeof(pbody_parts) - poff,
                                             "%s\"%s\":null", part_batch ? "," : "", pt.name);
                } else {
                    poff += (size_t)snprintf(pbody_parts + poff, sizeof(pbody_parts) - poff,
                                             "%s\"%s\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"sx\":%.2f,\"sy\":%.2f,"
                                             "\"sz\":%.2f,\"ry\":%.3f,\"b\":%d}",
                                             part_batch ? "," : "", pt.name, (double)pt.x, (double)pt.y,
                                             (double)pt.z, (double)pt.sx, (double)pt.sy, (double)pt.sz,
                                             (double)pt.yaw, pt.block);
                }
                part_batch++;
            }
            pbody_parts[poff] = '}';
            pbody_parts[poff + 1] = '\0';
            spectate_flag = net.spectate;
        }
        body[0] = '{';
        body[1] = '\0';
        {
            size_t off = 1;
            while (net.out_tail != net.out_head && batch_n < 64 && off + 48 < sizeof(body)) {
                GeoBlockEdit ed = net.outbox[net.out_tail];
                net.out_tail = (net.out_tail + 1) % OUTBOX_CAP;
                off += (size_t)snprintf(body + off, sizeof(body) - off, "%s\"%d_%d_%d\":%d", batch_n ? "," : "", ed.x,
                                        ed.y, ed.z, ed.id);
                batch_n++;
            }
            body[off] = '}';
            body[off + 1] = '\0';
        }
        ds_mutex_unlock(net_lock);

        if (batch_n > 0) {
            snprintf(url, sizeof(url), "%s/geometrium/rooms/%s/blocks.json", base, room);
            (void)geo_http_patch(url, body);
        }
        if (part_batch > 0) {
            snprintf(url, sizeof(url), "%s/geometrium/rooms/%s/parts.json", base, room);
            (void)geo_http_patch(url, pbody_parts);
        }

        if (now - last_pos >= 100 && !spectate_flag) {
            char esc_nick[48], pbody[320];
            long long seq;
            ds_mutex_lock(net_lock);
            seq = ++net.seq;
            ds_mutex_unlock(net_lock);
            geo_json_escape(nick, esc_nick, sizeof(esc_nick));
            snprintf(url, sizeof(url), "%s/geometrium/rooms/%s/players/%s.json", base, room, uid);
            snprintf(pbody, sizeof(pbody),
                     "{\"name\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"yaw\":%.3f,\"pitch\":%.3f,"
                     "\"swing\":%.2f,\"held\":%d,\"fly\":%d,\"m\":%d,\"sk\":%d,\"a\":%d,"
                     "\"live\":1,\"seq\":%lld,\"ts\":{\".sv\":\"timestamp\"}}",
                     esc_nick, (double)x, (double)y, (double)z, (double)yaw, (double)pitch, (double)swing, held, fly,
                     model, skin, anim, seq);
            if (geo_http_put(url, pbody)) {
                ds_mutex_lock(net_lock);
                if (net.run && net.gen == gen)
                    net.state = NET_PLAYING;
                ds_mutex_unlock(net_lock);
            }
            last_pos = now;
        }

        if (now - last_idx >= 3500) {
            int online_n;
            char ibody[128];
            ds_mutex_lock(net_lock);
            online_n = 1 + net.peer_n;
            ds_mutex_unlock(net_lock);
            snprintf(url, sizeof(url), "%s/geometrium/room_index/%s.json", base, room);
            snprintf(ibody, sizeof(ibody), "{\"players\":%d,\"ts\":{\".sv\":\"timestamp\"}}", online_n);
            (void)geo_http_put(url, ibody);
            last_idx = now;
        }
        geo_sleep_ms(20);
    }

    /* Best-effort immediate clean disconnect removal */
    if (base[0] && room[0] && uid[0]) {
        snprintf(url, sizeof(url), "%s/geometrium/rooms/%s/players/%s.json", base, room, uid);
        (void)geo_http_delete(url);
    }
    return NULL;
}

static void *reader_thread_fn(void *arg) {
    unsigned int gen = (unsigned int)(uintptr_t)arg;
    char *blocks_buf = (char *)malloc(BLOCKS_RESP_MAX);
    char *players_buf = (char *)malloc(32768);
    char *chat_buf = (char *)malloc(32768);
    char *parts_buf = (char *)malloc(32768);
    long long last_parts = 0;
    int shadow_len = 96 * 48 * 96;
    unsigned char *shadow = (unsigned char *)calloc((size_t)shadow_len, 1);
    long long last_blocks = 0, last_chat = 0;

    if (!blocks_buf || !players_buf || !chat_buf || !parts_buf || !shadow) {
        free(blocks_buf);
        free(players_buf);
        free(chat_buf);
        free(parts_buf);
        free(shadow);
        return NULL;
    }
    geo_part_shadow_n = 0;

    for (;;) {
        char base[256], room[24], uid[24], url[384];
        long long now = geo_now_ms();
        int total_players = 1;

        ds_mutex_lock(net_lock);
        if (!net.run || net.gen != gen) {
            ds_mutex_unlock(net_lock);
            break;
        }
        snprintf(base, sizeof(base), "%s", net.base);
        snprintf(room, sizeof(room), "%s", net.room);
        snprintf(uid, sizeof(uid), "%s", net.uid);
        ds_mutex_unlock(net_lock);

        if (now - last_blocks >= 320 || last_blocks == 0) {
            snprintf(url, sizeof(url), "%s/geometrium/rooms/%s/blocks.json", base, room);
            if (geo_http_get(url, blocks_buf, BLOCKS_RESP_MAX)) {
                geo_parse_blocks_json(blocks_buf, gen, shadow, shadow_len);
                ds_mutex_lock(net_lock);
                if (net.run && net.gen == gen)
                    net.world_loaded = 1;
                ds_mutex_unlock(net_lock);
            }
            last_blocks = now;
        }

        snprintf(url, sizeof(url), "%s/geometrium/rooms/%s/players.json", base, room);
        if (geo_http_get(url, players_buf, 32768))
            geo_parse_players_json(players_buf, uid, gen, &total_players);

        if (now - last_chat >= 500 || last_chat == 0) {
            snprintf(url, sizeof(url), "%s/geometrium/rooms/%s/chat.json", base, room);
            if (geo_http_get(url, chat_buf, 32768))
                geo_parse_chat_json(chat_buf, gen);
            last_chat = now;
        }

        if (now - last_parts >= 400 || last_parts == 0) {
            snprintf(url, sizeof(url), "%s/geometrium/rooms/%s/parts.json", base, room);
            if (parts_buf && geo_http_get(url, parts_buf, 32768))
                geo_parse_parts_json(parts_buf, gen);
            last_parts = now;
        }

        geo_sleep_ms(100);
    }

    free(blocks_buf);
    free(players_buf);
    free(chat_buf);
    free(parts_buf);
    free(shadow);
    return NULL;
}
