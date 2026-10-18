static Actor ps_last;
static long long ps_last_put = 0;
static int ps_have_last = 0;

static int net_gen_ok(unsigned gen) {
    return net.run && net.gen == gen;
}
static int push_state(void) {

    Actor a;
    int slot;
    unsigned long seq;
    char url[URL], body[BODY * 2], enick[64];
    lock();
    a = net.me;
    slot = net.slot;
    seq = ++net.seq;
    unlock();
    if (slot < 0)
        return 0;

    if (ps_have_last && actor_same(&a, &ps_last) && now_ms() - ps_last_put < HEARTBEAT_TICK)
        return 1;
    json_escape(a.nick, enick, sizeof(enick));
    snprintf(url, sizeof(url), "%s/rooms/%s/players/%d.json", net.base, net.room, slot);
    snprintf(body, sizeof(body),
             "{\"uid\":\"%s\",\"nick\":\"%s\",\"x\":%.5f,\"y\":%.5f,\"angle\":%.5f,\"hp\":%.2f,\"alive\":%.0f,\"seq\":%"
             "lu,\"px\":%.5f,\"py\":%.5f,\"pdx\":%.5f,\"pdy\":%.5f,\"punch\":%.0f,\"sx\":%.5f,\"sy\":%.5f,\"sdx\":%.5f,"
             "\"sdy\":%.5f,\"snow\":%.0f,\"stx\":%.5f,\"sty\":%.5f,\"sthp\":%.0f,\"s2x\":%.5f,\"s2y\":%.5f,\"s2hp\":%."
             "0f,\"s3x\":%.5f,\"s3y\":%.5f,\"s3hp\":%.0f,\"station\":%.0f,\"dax\":%.5f,\"day\":%.5f,\"dadx\":%.5f,"
             "\"dady\":%.5f,\"dash\":%.0f,\"gbx\":%.5f,\"gby\":%.5f,\"gbdx\":%.5f,\"gbdy\":%.5f,\"grab\":%.0f,\"uzx\":%"
             ".5f,\"uzy\":%.5f,\"universe\":%.0f,\"thud\":%.0f,\"blk\":%.0f,\"ccu\":%.0f,\"cls\":%.0f,\"level\":%.0f,\"skin\":%.0f}",
             net.uid, enick, clamp01(a.x), clamp01(a.y), safe(a.a), safe(a.hp), safe(a.alive), seq, clamp01(a.punch_x),
             clamp01(a.punch_y), clamp11(a.punch_dx), clamp11(a.punch_dy), safe(a.punch), clamp01(a.snow_x),
             clamp01(a.snow_y), clamp11(a.snow_dx), clamp11(a.snow_dy), safe(a.snow), clamp01(a.station_x),
             clamp01(a.station_y), safe(a.station_hp), clamp01(a.station2_x), clamp01(a.station2_y),
             safe(a.station2_hp), clamp01(a.station3_x), clamp01(a.station3_y), safe(a.station3_hp), safe(a.station),
             clamp01(a.dash_x), clamp01(a.dash_y), clamp11(a.dash_dx), clamp11(a.dash_dy), safe(a.dash),
             clamp01(a.grab_x), clamp01(a.grab_y), clamp11(a.grab_dx), clamp11(a.grab_dy), safe(a.grab),
             clamp01(a.universe_x), clamp01(a.universe_y), safe(a.universe), safe(a.thud), safe(a.block), safe(a.ccu), safe(a.cls), safe(a.level),
             safe(a.skin));
    net_seal_body(body, sizeof(body));
    int c = fb_http("PUT", url, body, NULL, 0);
    if (c != 200 && net_log_ok())
        LOGERR("push state: HTTP %d", c);
    if (c == 200) {
        ps_last = a;
        ps_last_put = now_ms();
        ps_have_last = 1;
    }
    return c == 200;
}
static char state_etag[96];

/* Polling fallback for the players node (used while the SSE stream is
 * down). Conditional GET: once we hold an ETag, unchanged state costs a
 * tiny 304 instead of the full snapshot. Returns 1 = new data in resp,
 * 2 = unchanged (304), 0 = failure. */
static int pull_state(char *resp, size_t cap) {
    char url[URL], fresh[96];
    snprintf(url, sizeof(url), "%s/rooms/%s/players.json", net.base, net.room);
    int c;
    fresh[0] = 0;
    if (state_etag[0])
        c = fb_http_ex("GET", url, NULL, resp, cap, "if-none-match", state_etag, fresh, sizeof(fresh));
    else
        c = fb_http_ex("GET", url, NULL, resp, cap, "X-Firebase-ETag", "true", fresh, sizeof(fresh));
    if (fresh[0])
        snprintf(state_etag, sizeof(state_etag), "%s", fresh);
    if (c == 304)
        return 2;
    if (c != 200 && net_log_ok())
        LOGERR("pull state: HTTP %d", c);
    return c == 200;
}
static int pull_event(char *resp, size_t cap) {
    char url[URL];
    snprintf(url, sizeof(url), "%s/event.json", net.base);
    int c = fb_http("GET", url, NULL, resp, cap);
    if (c != 200 && net_log_ok())
        LOGERR("pull event: HTTP %d", c);
    return c == 200;
}
static void parse_and_store_banner(const char *json, unsigned gen) {
    char text[CHAT_TEXT_MAX], color[BANNER_COLOR_MAX];
    long long ts;
    if (!json || !*json || !strncmp(json, "null", 4)) {
        /* The banner was cleared by its author: propagate the disappearance so
         * every client drops it at the same time instead of each one keeping
         * its own local countdown running. */
        lock();
        if (net_gen_ok(gen)) {
            net.banner_ts = 0;
            net.banner_text[0] = 0;
        }
        unlock();
        return;
    }
    ts = num_ll(json, "ts", 0);
    if (ts <= 0)
        return;
    strv(json, "text", text, sizeof(text));
    strv(json, "color", color, sizeof(color));
    if (!text[0])
        return;
    lock();
    if (!net_gen_ok(gen)) {
        unlock();
        return;
    }
    snprintf(net.banner_text, sizeof(net.banner_text), "%s", text);
    snprintf(net.banner_color, sizeof(net.banner_color), "%s", color[0] ? color : "white");
    net.banner_ts = ts;
    unlock();
}
static int pull_banner(char *resp, size_t cap) {
    char url[URL];
    snprintf(url, sizeof(url), "%s/banner.json", net.base);
    return fb_http("GET", url, NULL, resp, cap) == 200;
}

static void release_slot(int slot) {
    char url[URL];
    if (slot < 0)
        return;
    snprintf(url, sizeof(url), "%s/rooms/%s/players/%d.json", net.base, net.room, slot);
    fb_http("DELETE", url, NULL, NULL, 0);
}
static int claim_slot(unsigned gen) {
    char resp[RESP], url[URL], body[BODY], uid[24], etag[96];
    long long t = now_ms();
    int slot;
    for (slot = 0; slot < NET_SLOTS; slot++) {
        unsigned long seq;
        int claim = 0, code;
        if (!net_gen_ok(gen))
            return -1;
        snprintf(url, sizeof(url), "%s/rooms/%s/players/%d.json", net.base, net.room, slot);
        code = fb_http_ex("GET", url, NULL, resp, sizeof(resp), "X-Firebase-ETag", "true", etag, sizeof(etag));
        if (code != 200) {
            if (net_log_ok())
                LOGERR("claim slot %d: HTTP %d", slot, code);
            continue;
        }
        strv(resp, "uid", uid, sizeof(uid));
        if (uid[0] && !strcmp(uid, net.uid))
            return slot;
        seq = num_ulong(resp, "seq", 0);
        if (!uid[0])
            claim = 1;
        else if (etag[0] && (strcmp(uid, claim_seen_uid[slot]) || seq != claim_seen_seq[slot])) {
            snprintf(claim_seen_uid[slot], sizeof(claim_seen_uid[slot]), "%s", uid);
            claim_seen_seq[slot] = seq;
            claim_seen_at[slot] = t;
        } else if (etag[0] && t - claim_seen_at[slot] >= STALE)
            claim = 1;
        if (!claim)
            continue;
        if (!etag[0]) {
            if (net_log_ok())
                LOG("claim slot %d: no ETag", slot);
        }
        {
            char enick[64];
            json_escape(net.me.nick, enick, sizeof(enick));
            snprintf(
                body, sizeof(body),
                "{\"uid\":\"%s\",\"nick\":\"%s\",\"x\":0,\"y\":0,\"angle\":0,\"hp\":0,\"alive\":0,\"seq\":0,\"px\":0,"
                "\"py\":0,\"pdx\":0,\"pdy\":0,\"punch\":0,\"sx\":0,\"sy\":0,\"sdx\":0,\"sdy\":0,\"snow\":0,\"stx\":0,"
                "\"sty\":0,\"sthp\":0,\"s2x\":0,\"s2y\":0,\"s2hp\":0,\"s3x\":0,\"s3y\":0,\"s3hp\":0,\"station\":0,"
                "\"dax\":0,\"day\":0,\"dadx\":0,\"dady\":0,\"dash\":0,\"gbx\":0,\"gby\":0,\"gbdx\":0,\"gbdy\":0,"
                "\"grab\":0,\"uzx\":0,\"uzy\":0,\"universe\":0,\"thud\":0,\"blk\":0,\"ccu\":0,\"cls\":%.0f,\"level\":%.0f,\"skin\":%.0f}",
                net.uid, enick, safe(net.me.cls), safe(net.me.level), safe(net.me.skin));
        }
        net_seal_body(body, sizeof(body));
        code = fb_http_ex("PUT", url, body, NULL, 0, etag[0] ? "if-match" : NULL, etag[0] ? etag : NULL, NULL, 0);
        if (code == 200) {
            claim_seen_uid[slot][0] = 0;
            claim_seen_seq[slot] = 0;
            claim_seen_at[slot] = 0;
            LOG("slot %d uid %s", slot, net.uid);
            return slot;
        }
        if (net_log_ok())
            LOGERR("claim slot %d: PUT failed, HTTP %d", slot, code);
    }
    return -1;
}
static void read_players(const char *resp, unsigned gen) {
    Actor ps[NET_SLOTS];
    long long t = now_ms();
    int local, count = 0, slot;
    memset(ps, 0, sizeof(ps));
    lock();
    local = net.slot;
    unlock();
    for (slot = 0; slot < NET_SLOTS; slot++) {
        char bp[24], p[40], uid[24];
        unsigned long sq;
        int online;
        if (slot == local) {
            lock();
            ps[slot] = net.me;
            ps[slot].online = local >= 0;
            unlock();
            if (local >= 0)
                count++;
            continue;
        }
        snprintf(bp, sizeof(bp), "%d", slot);
        snprintf(p, sizeof(p), "%s/uid", bp);
        strv(resp, p, uid, sizeof(uid));
        if (!uid[0] || !strcmp(uid, net.uid)) {
            rp_lseq[slot] = 0;
            rp_lch[slot] = 0;
            continue;
        }
        snprintf(p, sizeof(p), "%s/seq", bp);
        sq = num_ulong(resp, p, 0);
        if (sq != rp_lseq[slot]) {
            rp_lseq[slot] = sq;
            rp_lch[slot] = t;
        } else if (!rp_lch[slot])
            rp_lch[slot] = t;
        online = t - rp_lch[slot] < TIMEOUT;
        if (!online)
            continue;
        snprintf(p, sizeof(p), "%s/nick", bp);
        strv(resp, p, ps[slot].nick, sizeof(ps[slot].nick));
        if (ban_cache_has(ps[slot].nick)) {

            if (t - rp_kicked[slot] >= BAN_KICK_TICK) {
                rp_kicked[slot] = t;
                LOG("slot %d: '%s' is banned, kicking", slot, ps[slot].nick);
                room_slot_delete_async(slot);
            }
            rp_lseq[slot] = 0;
            rp_lch[slot] = 0;
            continue;
        }
        snprintf(p, sizeof(p), "%s/x", bp);
        ps[slot].x = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/y", bp);
        ps[slot].y = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/angle", bp);
        ps[slot].a = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/hp", bp);
        ps[slot].hp = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/alive", bp);
        ps[slot].alive = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/px", bp);
        ps[slot].punch_x = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/py", bp);
        ps[slot].punch_y = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/pdx", bp);
        ps[slot].punch_dx = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/pdy", bp);
        ps[slot].punch_dy = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/punch", bp);
        ps[slot].punch = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/sx", bp);
        ps[slot].snow_x = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/sy", bp);
        ps[slot].snow_y = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/sdx", bp);
        ps[slot].snow_dx = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/sdy", bp);
        ps[slot].snow_dy = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/snow", bp);
        ps[slot].snow = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/stx", bp);
        ps[slot].station_x = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/sty", bp);
        ps[slot].station_y = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/sthp", bp);
        ps[slot].station_hp = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/s2x", bp);
        ps[slot].station2_x = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/s2y", bp);
        ps[slot].station2_y = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/s2hp", bp);
        ps[slot].station2_hp = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/s3x", bp);
        ps[slot].station3_x = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/s3y", bp);
        ps[slot].station3_y = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/s3hp", bp);
        ps[slot].station3_hp = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/station", bp);
        ps[slot].station = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/dax", bp);
        ps[slot].dash_x = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/day", bp);
        ps[slot].dash_y = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/dadx", bp);
        ps[slot].dash_dx = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/dady", bp);
        ps[slot].dash_dy = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/dash", bp);
        ps[slot].dash = num(resp, p, 0);

        snprintf(p, sizeof(p), "%s/gbx", bp);
        ps[slot].grab_x = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/gby", bp);
        ps[slot].grab_y = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/gbdx", bp);
        ps[slot].grab_dx = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/gbdy", bp);
        ps[slot].grab_dy = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/grab", bp);
        ps[slot].grab = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/uzx", bp);
        ps[slot].universe_x = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/uzy", bp);
        ps[slot].universe_y = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/universe", bp);
        ps[slot].universe = num(resp, p, 0);

        snprintf(p, sizeof(p), "%s/thud", bp);
        ps[slot].thud = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/blk", bp);
        ps[slot].block = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/ccu", bp);
        ps[slot].ccu = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/cls", bp);
        ps[slot].cls = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/level", bp);
        ps[slot].level = num(resp, p, 0);
        snprintf(p, sizeof(p), "%s/skin", bp);
        ps[slot].skin = num(resp, p, 0);
        ps[slot].online = 1;
        count++;
    }
    lock();
    if (net_gen_ok(gen)) {
        memcpy(net.players, ps, sizeof(ps));
        net.count = count;
    }
    unlock();
}

/* ---- Firebase REST streaming (SSE) for the players node ----
 * One long-lived GET (Accept: text/event-stream) replaces the 60 ms
 * polling of players.json: the server pushes `put` (full snapshot) and
 * `patch` (changed slots only) events. Patches are merged into a cached
 * snapshot with json_object_merge(), then the same read_players() runs,
 * so the streaming and polling paths share every line of game logic.
 * If the stream dies, stream_live_at goes stale and the reader thread
 * falls back to polling until the stream thread reconnects. */
static char stream_players_cache[RESP];
static unsigned stream_gen;

typedef struct {
    char line[RESP];
    size_t line_len;
    char data[RESP];
    size_t data_len;
    char event[24];
} SseParser;
static SseParser sse;

static void stream_mark_live(void) {
    lock();
    net.stream_live_at = now_ms();
    unlock();
}

static int stream_is_live(void) {
    long long at;
    int active;
    lock();
    at = net.stream_live_at;
    active = net.stream_active;
    unlock();
    /* Firebase sends keep-alive events on an idle stream; between them
     * the connection is still the fastest path, polling is only the
     * fallback for a stream that has actually died (flag cleared). */
    return active && at && now_ms() - at < 75000;
}

static void sse_dispatch_players(void) {
    char path[32];
    const char *data, *dend;
    if (!sse.data_len)
        return;
    sse.data[sse.data_len] = 0;
    if (strcmp(sse.event, "keep-alive") == 0 || strcmp(sse.event, "cancel") == 0 ||
        strcmp(sse.event, "auth_revoked") == 0) {
        return;
    }
    strv(sse.data, "path", path, sizeof(path));
    data = member(sse.data, "data");
    if (!data)
        return;
    dend = skip_val(data);
    if (!dend || dend <= data)
        return;
    if (!stream_players_cache[0])
        snprintf(stream_players_cache, sizeof(stream_players_cache), "%s", "{}");
    if (strcmp(path, "/") == 0 || path[0] == 0) {
        if (strcmp(sse.event, "patch") == 0) {
            char patch[RESP];
            size_t n = (size_t)(dend - data);
            if (n >= sizeof(patch))
                return;
            memcpy(patch, data, n);
            patch[n] = 0;
            if (!json_object_merge(stream_players_cache, sizeof(stream_players_cache), patch))
                return;
        } else {
            size_t n = (size_t)(dend - data);
            if (n >= sizeof(stream_players_cache))
                return;
            memcpy(stream_players_cache, data, n);
            stream_players_cache[n] = 0;
            if (stream_players_cache[0] != '{')
                snprintf(stream_players_cache, sizeof(stream_players_cache), "%s", "{}");
        }
    } else {
        /* {"path":"/2","data":{...}} -> patch {"2":{...}} (or delete). */
        char patch[RESP];
        const char *slot = path + 1;
        int w = snprintf(patch, sizeof(patch), "{\"%s\":", slot);
        size_t n = (size_t)(dend - data);
        if (w < 0 || (size_t)w + n + 2 >= sizeof(patch))
            return;
        memcpy(patch + w, data, n);
        patch[w + (int)n] = '}';
        patch[w + (int)n + 1] = 0;
        if (!json_object_merge(stream_players_cache, sizeof(stream_players_cache), patch))
            return;
    }
    if (net_gen_ok(stream_gen))
        read_players(stream_players_cache, stream_gen);
}

static void sse_line(const char *line, size_t len) {
    if (len == 0) {
        if (sse.data_len)
            sse_dispatch_players();
        sse.data_len = 0;
        sse.event[0] = 0;
        return;
    }
    if (line[0] == ':')
        return; /* comment / heartbeat */
    if (len > 6 && strncmp(line, "event:", 6) == 0) {
        const char *v = line + 6;
        while (*v == ' ')
            v++;
        snprintf(sse.event, sizeof(sse.event), "%s", v);
        return;
    }
    if (len > 5 && strncmp(line, "data:", 5) == 0) {
        const char *v = line + 5;
        size_t n;
        while (*v == ' ')
            v++;
        n = strlen(v);
        if (sse.data_len && sse.data_len + 1 < sizeof(sse.data))
            sse.data[sse.data_len++] = '\n';
        if (sse.data_len + n >= sizeof(sse.data))
            n = sizeof(sse.data) - sse.data_len - 1;
        memcpy(sse.data + sse.data_len, v, n);
        sse.data_len += n;
        return;
    }
}

/* Chunk callback for http_stream: reassembles lines across chunks. */
static int stream_players_chunk(void *ctx, const char *chunk, size_t len) {
    size_t i;
    (void)ctx;
    if (!net_gen_ok(stream_gen))
        return 0;
    stream_mark_live();
    for (i = 0; i < len; i++) {
        char c = chunk[i];
        if (c == '\n') {
            sse.line[sse.line_len] = 0;
            /* trim trailing CR */
            if (sse.line_len && sse.line[sse.line_len - 1] == '\r')
                sse.line[--sse.line_len] = 0;
            sse_line(sse.line, sse.line_len);
            sse.line_len = 0;
            if (!net_gen_ok(stream_gen))
                return 0;
        } else if (sse.line_len + 1 < sizeof(sse.line)) {
            sse.line[sse.line_len++] = c;
        } else {
            sse.line_len = 0; /* overlong line: drop it, resync on \n */
        }
    }
    return 1;
}
