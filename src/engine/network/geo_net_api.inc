/* Public thread-safe API for Geometrium real-time multiplayer, chat, rooms & presence. */

void net_connect(const char *url, const char *room) {
    pthread_t wt, rt;
    unsigned int gen;
    char clean_room[24];

    geo_sanitize_id(room, clean_room, sizeof(clean_room), GEO_DEFAULT_ROOM);
    if (url && *url)
        net_set_base_url(url);

    ds_mutex_lock(net_lock);
    geo_ensure_defaults_locked();
    gen = ++net.gen;
    net.run = 1;
    net.state = NET_CONNECTING;
    snprintf(net.room, sizeof(net.room), "%s", clean_room);
    net.peer_n = 0;
    net.out_head = net.out_tail = 0;
    net.part_out_head = net.part_out_tail = 0;
    net.part_in_head = net.part_in_tail = 0;
    net.spectate = 0;
    net.in_head = net.in_tail = 0;
    net.world_loaded = 0;
    net.chat_n = 0;
    ds_mutex_unlock(net_lock);

    if (pthread_create(&wt, NULL, writer_thread_fn, (void *)(uintptr_t)gen) == 0)
        pthread_detach(wt);
    if (pthread_create(&rt, NULL, reader_thread_fn, (void *)(uintptr_t)gen) == 0)
        pthread_detach(rt);
}

void net_disconnect(void) {
    ds_mutex_lock(net_lock);
    net.run = 0;
    net.gen++;
    net.state = NET_OFFLINE;
    net.peer_n = 0;
    net.world_loaded = 0;
    ds_mutex_unlock(net_lock);
}

int net_status(void) {
    int s;
    ds_mutex_lock(net_lock);
    s = net.state;
    ds_mutex_unlock(net_lock);
    return s;
}

const char *net_current_room(void) {
    static char buf[24];
    ds_mutex_lock(net_lock);
    snprintf(buf, sizeof(buf), "%s", net.room[0] ? net.room : GEO_DEFAULT_ROOM);
    ds_mutex_unlock(net_lock);
    return buf;
}

int net_online_count(void) {
    int n;
    ds_mutex_lock(net_lock);
    n = net.run ? (1 + net.peer_n) : 0;
    ds_mutex_unlock(net_lock);
    return n;
}

void net_publish_local(float x, float y, float z, float yaw, float pitch, float swing, int held, int fly,
                       int model, int skin_packed, int anim) {
    ds_mutex_lock(net_lock);
    net.lx = x;
    net.ly = y;
    net.lz = z;
    net.lyaw = yaw;
    net.lpitch = pitch;
    net.lswing = swing;
    net.lheld = held;
    net.lfly = fly;
    net.lmodel = model;
    net.lskin = skin_packed;
    net.lanim = anim;
    ds_mutex_unlock(net_lock);
}

int net_copy_remotes(GeoRemotePeer *out, int max_out) {
    int n;
    ds_mutex_lock(net_lock);
    n = net.peer_n < max_out ? net.peer_n : max_out;
    if (n > 0 && out)
        memcpy(out, net.peers, (size_t)n * sizeof(GeoRemotePeer));
    ds_mutex_unlock(net_lock);
    return n;
}

void net_block_send(int x, int y, int z, int block_id) {
    int next;
    ds_mutex_lock(net_lock);
    if (net.run) {
        next = (net.out_head + 1) % OUTBOX_CAP;
        if (next == net.out_tail)
            net.out_tail = (net.out_tail + 1) % OUTBOX_CAP;
        net.outbox[net.out_head] = (GeoBlockEdit){x, y, z, block_id};
        net.out_head = next;
    }
    ds_mutex_unlock(net_lock);
}

int net_block_poll(int *x, int *y, int *z, int *block_id) {
    int ok = 0;
    ds_mutex_lock(net_lock);
    if (net.in_tail != net.in_head) {
        GeoBlockEdit ed = net.inbox[net.in_tail];
        net.in_tail = (net.in_tail + 1) % BLOCK_RING_CAP;
        *x = ed.x;
        *y = ed.y;
        *z = ed.z;
        *block_id = ed.id;
        ok = 1;
    }
    ds_mutex_unlock(net_lock);
    return ok;
}

void net_part_send(const GeoPartNet *part) {
    int next;
    if (!part || !part->name[0])
        return;
    ds_mutex_lock(net_lock);
    if (net.run) {
        next = (net.part_out_head + 1) % 256;
        if (next == net.part_out_tail)
            net.part_out_tail = (net.part_out_tail + 1) % 256;
        net.part_out[net.part_out_head] = *part;
        net.part_out_head = next;
    }
    ds_mutex_unlock(net_lock);
}

int net_part_poll(GeoPartNet *out) {
    int ok = 0;
    ds_mutex_lock(net_lock);
    if (net.part_in_tail != net.part_in_head) {
        *out = net.part_in[net.part_in_tail];
        net.part_in_tail = (net.part_in_tail + 1) % 256;
        ok = 1;
    }
    ds_mutex_unlock(net_lock);
    return ok;
}

void net_publish_game(void) {
    ds_mutex_lock(net_lock);
    if (net.run)
        net.game_dirty = 1;
    ds_mutex_unlock(net_lock);
}

void net_set_spectator(int spectator) {
    ds_mutex_lock(net_lock);
    net.spectate = spectator ? 1 : 0;
    ds_mutex_unlock(net_lock);
}

int net_world_synced(void) {
    int w;
    ds_mutex_lock(net_lock);
    w = net.world_loaded;
    ds_mutex_unlock(net_lock);
    return w;
}

typedef struct {
    char base[256];
    char room[24];
    char uid[24];
    char sender[24];
    char text[96];
    long long seq;
} ChatSendJob;

static void *chat_send_worker(void *arg) {
    ChatSendJob *j = (ChatSendJob *)arg;
    char url[384], es[48], et[192], body[320];
    geo_json_escape(j->sender, es, sizeof(es));
    geo_json_escape(j->text, et, sizeof(et));
    snprintf(url, sizeof(url), "%s/geometrium/rooms/%s/chat/m_%011lld_%s.json", j->base, j->room,
             (long long)time(NULL) * 100LL + (j->seq % 100LL), j->uid);
    snprintf(body, sizeof(body), "{\"n\":\"%s\",\"m\":\"%s\",\"u\":\"%s\",\"ts\":{\".sv\":\"timestamp\"}}", es, et,
             j->uid);
    (void)geo_http_put(url, body);
    free(j);
    return NULL;
}

void net_chat_send(const char *text) {
    ChatSendJob *j;
    pthread_t th;
    if (!text || !text[0])
        return;
    j = (ChatSendJob *)calloc(1, sizeof(*j));
    if (!j)
        return;
    ds_mutex_lock(net_lock);
    if (!net.run) {
        ds_mutex_unlock(net_lock);
        free(j);
        return;
    }
    snprintf(j->base, sizeof(j->base), "%s", net.base);
    snprintf(j->room, sizeof(j->room), "%s", net.room);
    snprintf(j->uid, sizeof(j->uid), "%s", net.uid);
    snprintf(j->sender, sizeof(j->sender), "%s", net.nick);
    snprintf(j->text, sizeof(j->text), "%s", text);
    j->seq = ++net.seq;
    if (net.chat_n == GEO_MAX_CHAT) {
        memmove(&net.chat[0], &net.chat[1], (GEO_MAX_CHAT - 1) * sizeof(GeoChatMsg));
        net.chat_n = GEO_MAX_CHAT - 1;
    }
    snprintf(net.chat[net.chat_n].sender, sizeof(net.chat[0].sender), "%s", j->sender);
    snprintf(net.chat[net.chat_n].text, sizeof(net.chat[0].text), "%s", j->text);
    net.chat_n++;
    ds_mutex_unlock(net_lock);

    if (pthread_create(&th, NULL, chat_send_worker, j) == 0)
        pthread_detach(th);
    else
        free(j);
}

int net_chat_count(void) {
    int n;
    ds_mutex_lock(net_lock);
    n = net.chat_n;
    ds_mutex_unlock(net_lock);
    return n;
}

void net_chat_get(int idx, char *sender_out, size_t s_cap, char *text_out, size_t t_cap) {
    ds_mutex_lock(net_lock);
    if (idx >= 0 && idx < net.chat_n) {
        snprintf(sender_out, s_cap, "%s", net.chat[idx].sender);
        snprintf(text_out, t_cap, "%s", net.chat[idx].text);
    } else {
        if (s_cap)
            sender_out[0] = '\0';
        if (t_cap)
            text_out[0] = '\0';
    }
    ds_mutex_unlock(net_lock);
}

static void *rooms_refresh_worker(void *arg) {
    char base[256], url[384];
    char *resp = (char *)malloc(16384);
    GeoRoomEntry list[GEO_MAX_ROOMS];
    int n = 0, has_main = 0;
    (void)arg;

    ds_mutex_lock(net_lock);
    geo_ensure_defaults_locked();
    snprintf(base, sizeof(base), "%s", net.base);
    ds_mutex_unlock(net_lock);

    if (resp) {
        snprintf(url, sizeof(url), "%s/geometrium/room_index.json", base);
        if (geo_http_get(url, resp, 16384) && resp[0] == '{') {
            const char *p = resp + 1;
            double max_ts = 0.0;
            double ts_list[GEO_MAX_ROOMS];
            while (*p && n < GEO_MAX_ROOMS) {
                char rname[24];
                const char *s, *e;
                while (*p && *p != '"' && *p != '}')
                    p++;
                if (*p != '"')
                    break;
                p = geo_json_read_str(p + 1, rname, sizeof(rname));
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
                snprintf(list[n].name, sizeof(list[n].name), "%s", rname);
                list[n].players = (int)geo_obj_num(s, e, "players", 0.0);
                ts_list[n] = geo_obj_num(s, e, "ts", 0.0);
                if (ts_list[n] > max_ts)
                    max_ts = ts_list[n];
                if (strcmp(rname, GEO_DEFAULT_ROOM) == 0)
                    has_main = 1;
                n++;
            }
            {
                int i;
                for (i = 0; i < n; i++) {
                    if (max_ts > 0.0 && ts_list[i] > 0.0 && (max_ts - ts_list[i]) > 20000.0)
                        list[i].players = 0;
                }
            }
        }
        free(resp);
    }
    if (!has_main && n < GEO_MAX_ROOMS) {
        memmove(&list[1], &list[0], (size_t)n * sizeof(GeoRoomEntry));
        snprintf(list[0].name, sizeof(list[0].name), "%s", GEO_DEFAULT_ROOM);
        list[0].players = 0;
        n++;
    }
    ds_mutex_lock(net_lock);
    memcpy(net.rooms, list, (size_t)n * sizeof(GeoRoomEntry));
    net.room_n = n;
    net.rooms_busy = 0;
    ds_mutex_unlock(net_lock);
    return NULL;
}

void net_rooms_refresh(void) {
    pthread_t th;
    ds_mutex_lock(net_lock);
    if (net.rooms_busy) {
        ds_mutex_unlock(net_lock);
        return;
    }
    net.rooms_busy = 1;
    ds_mutex_unlock(net_lock);
    if (pthread_create(&th, NULL, rooms_refresh_worker, NULL) == 0)
        pthread_detach(th);
    else {
        ds_mutex_lock(net_lock);
        net.rooms_busy = 0;
        ds_mutex_unlock(net_lock);
    }
}

int net_rooms_list(GeoRoomEntry *out, int max_out) {
    int n;
    ds_mutex_lock(net_lock);
    n = net.room_n < max_out ? net.room_n : max_out;
    if (n > 0 && out)
        memcpy(out, net.rooms, (size_t)n * sizeof(GeoRoomEntry));
    ds_mutex_unlock(net_lock);
    return n;
}

int net_presence_check(const char *data_dir) {
    char url[384], resp[8192];
    if (data_dir && *data_dir)
        net_set_data_path(data_dir);
    net_profile_load();
    snprintf(url, sizeof(url), "%s/geometrium/rooms/%s/players.json", net_get_base_url(), GEO_DEFAULT_ROOM);
    if (!geo_http_get(url, resp, sizeof(resp)))
        return -1;
    if (strstr(resp, "\"live\":1") != NULL)
        return strstr(resp, net.uid) ? 2 : 1;
    return 0;
}
