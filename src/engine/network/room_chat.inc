static int parse_chat_list(const char *json, ChatMsg *tmp, int cap) {
    int tmp_cnt = 0;
    const char *p = skip_ws(json);
    if (!p || !*p || !strncmp(p, "null", 4))
        return 0;
    if (*p != '{')
        return 0;
    for (p = skip_ws(p + 1); p && *p && *p != '}' && tmp_cnt < cap;) {
        if (*p != '\"')
            break;
        const char *kend = skip_str(p);
        if (!kend)
            break;
        char key[28] = {0};
        size_t kl = (size_t)(kend - p - 2);
        if (kl >= sizeof(key))
            kl = sizeof(key) - 1;
        memcpy(key, p + 1, kl);
        key[kl] = 0;
        p = skip_ws(kend);
        if (*p != ':')
            break;
        p = skip_ws(p + 1);
        const char *obj = p;
        p = skip_val(p);
        if (!p)
            break;
        memset(&tmp[tmp_cnt], 0, sizeof(tmp[tmp_cnt]));
        strncpy(tmp[tmp_cnt].key, key, sizeof(tmp[tmp_cnt].key) - 1);
        strv(obj, "uid", tmp[tmp_cnt].uid, sizeof(tmp[tmp_cnt].uid));
        strv(obj, "nick", tmp[tmp_cnt].nick, sizeof(tmp[tmp_cnt].nick));
        strv(obj, "text", tmp[tmp_cnt].text, sizeof(tmp[tmp_cnt].text));
                /* A message with no text (a broken record in the database, an old
                 * command with no body) is not shown at all: it used to leave a bare nick
                 * on the line as "Broken". */
        if (tmp[tmp_cnt].text[0] && !(key[0] == 'b' && key[1] == '_')) {
            tmp[tmp_cnt].valid = 1;
            tmp_cnt++;
        }
        p = skip_ws(p);
        if (p && *p == ',')
            p = skip_ws(p + 1);
    }
    return tmp_cnt;
}
static void parse_and_store_chat(const char *json, unsigned gen) {
    ChatMsg tmp[CHAT_MAX + 16];
    int tmp_cnt, keep, start, i;
    char extra[CHAT_MAX + 16][28];
    int nextra = 0;
    if (!json || !*json)
        return;
    tmp_cnt = parse_chat_list(json, tmp, CHAT_MAX + 16);
    if (tmp_cnt <= 0)
        return;
    lock();
    keep = chat_keep;
    unlock();
    if (keep < 1)
        keep = 1;
    if (keep > CHAT_MAX)
        keep = CHAT_MAX;
    start = 0;
    if (tmp_cnt > keep) {
        int drop = tmp_cnt - keep;
        for (i = 0; i < drop; i++) {
            if (tmp[i].key[0]) {
                strncpy(extra[nextra], tmp[i].key, sizeof(extra[0]) - 1);
                extra[nextra][sizeof(extra[0]) - 1] = 0;
                nextra++;
            }
        }
        start = drop;
    }
    lock();
    if (!net_gen_ok(gen)) {
        unlock();
        return;
    }
    net.chat_count = 0;
    for (i = start; i < tmp_cnt && net.chat_count < CHAT_MAX; i++) net.chats[net.chat_count++] = tmp[i];
    unlock();
    for (i = 0; i < nextra; i++) chat_delete_key_async(extra[i]);
}
static int pull_chat(char *resp, size_t cap) {
    char url[URL], fresh[96];
    int c;
    snprintf(url, sizeof(url), "%s/rooms/%s/chat.json", net.base, net.room);
    fresh[0] = 0;
    if (mc_room_etag[0])
        c = fb_http_ex("GET", url, NULL, resp, cap, "if-none-match", mc_room_etag, fresh, sizeof(fresh));
    else
        c = fb_http_ex("GET", url, NULL, resp, cap, "X-Firebase-ETag", "true", fresh, sizeof(fresh));
    if (fresh[0])
        snprintf(mc_room_etag, sizeof(mc_room_etag), "%s", fresh);
    return c == 200;
}
static void prune_old_chat(unsigned gen) {
    char url[URL], resp[CHAT_RESP];
    ChatMsg oldm[16];
    char keepkeys[CHAT_MAX][28];
    int n, nk = 0, count, keep, i, j;
    lock();
    keep = chat_keep;
    count = net.chat_count;
    for (i = 0; i < net.chat_count; i++) {
        strncpy(keepkeys[nk], net.chats[i].key, sizeof(keepkeys[0]) - 1);
        keepkeys[nk][sizeof(keepkeys[0]) - 1] = 0;
        nk++;
    }
    unlock();
    if (count < keep || keep < 1)
        return;
    snprintf(url, sizeof(url), "%s/rooms/%s/chat.json?orderBy=%%22$key%%22&limitToFirst=16", net.base, net.room);
    if (fb_http("GET", url, NULL, resp, sizeof(resp)) != 200)
        return;
    if (!net_gen_ok(gen))
        return;
    n = parse_chat_list(resp, oldm, 16);
    for (i = 0; i < n; i++) {
        int kept = 0;
        if (!oldm[i].key[0] || (oldm[i].key[0] == 'b' && oldm[i].key[1] == '_'))
            continue;
        for (j = 0; j < nk; j++)
            if (!strcmp(oldm[i].key, keepkeys[j])) {
                kept = 1;
                break;
            }
        if (!kept)
            chat_delete_key_async(oldm[i].key);
    }
}
