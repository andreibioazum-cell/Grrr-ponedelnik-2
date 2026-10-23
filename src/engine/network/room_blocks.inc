/* Persistent 3D world delta sync for online rooms.
 *
 * Stores modified blocks at /rooms/<room>/chat/b_<x>_<y>_<z>.json using the
 * exact authenticated {uid, nick, text, w} schema already validated by the
 * live firebase.rules.json and admin panel. Restoring a block to its natural
 * worldgen state deletes the key cleanly so room storage stays compact. */

#define MC_BLOCK_QUEUE_CAP 4096
#define MC_CHAT_BUF_SIZE (256 * 1024)

typedef struct {
    int x, y, z, id;
} NetBlockEdit;

typedef struct {
    char url[FB_URL_MAX];
    char body[256];
    int is_delete;
} BlockWriteJob;

static NetBlockEdit mc_edit_q[MC_BLOCK_QUEUE_CAP];
static int mc_edit_head = 0;
static int mc_edit_tail = 0;
static unsigned char mc_seen_block[64 * 36 * 64];
static char mc_room_etag[96];

static void mc_blocks_reset_state(void) {
    lock();
    mc_edit_head = 0;
    mc_edit_tail = 0;
    memset(mc_seen_block, 0xFF, sizeof(mc_seen_block));
    mc_room_etag[0] = 0;
    unlock();
}

static void mc_queue_push_locked(int x, int y, int z, int id) {
    size_t idx;
    if (x < 0 || x >= 64 || y < 0 || y >= 36 || z < 0 || z >= 64)
        return;
    if (id < 0 || id > 63)
        return;
    idx = ((size_t)y * 64u + (size_t)z) * 64u + (size_t)x;
    if (mc_seen_block[idx] == (unsigned char)id)
        return;
    mc_seen_block[idx] = (unsigned char)id;
    {
        int next = (mc_edit_tail + 1) % MC_BLOCK_QUEUE_CAP;
        if (next == mc_edit_head)
            mc_edit_head = (mc_edit_head + 1) % MC_BLOCK_QUEUE_CAP;
        mc_edit_q[mc_edit_tail].x = x;
        mc_edit_q[mc_edit_tail].y = y;
        mc_edit_q[mc_edit_tail].z = z;
        mc_edit_q[mc_edit_tail].id = id;
        mc_edit_tail = next;
    }
}

static void *mc_block_write_worker(void *arg) {
    BlockWriteJob *j = (BlockWriteJob *)arg;
    if (!j)
        return NULL;
    if (j->is_delete)
        fb_http("DELETE", j->url, NULL, NULL, 0);
    else
        fb_http("PUT", j->url, j->body, NULL, 0);
    free(j);
    return NULL;
}

void net_mc_block_write(int x, int y, int z, int block_id, int is_natural) {
    BlockWriteJob *j;
    char enick[64];
    const char *nick;
    size_t idx;
    DSThread t;
    if (!net.started || !net.run || !net.base[0])
        return;
    if (x < 0 || x >= 64 || y < 0 || y >= 36 || z < 0 || z >= 64)
        return;
    idx = ((size_t)y * 64u + (size_t)z) * 64u + (size_t)x;
    j = (BlockWriteJob *)malloc(sizeof(*j));
    if (!j)
        return;
    memset(j, 0, sizeof(*j));
    lock();
    mc_seen_block[idx] = (unsigned char)(block_id & 0xFF);
    nick = net.me.nick[0] ? net.me.nick : net.uid;
    json_escape(nick, enick, sizeof(enick));
    snprintf(j->url, sizeof(j->url), "%s/rooms/%s/chat/b_%d_%d_%d.json", net.base, net.room, x, y, z);
    unlock();
    if (is_natural) {
        j->is_delete = 1;
    } else {
        j->is_delete = 0;
        snprintf(j->body, sizeof(j->body), "{\"uid\":\"%s\",\"nick\":\"%s\",\"text\":\"@B:%d\"}", net.uid, enick,
                 block_id);
        net_seal_body(j->body, sizeof(j->body));
    }
    t = ds_thread_start(mc_block_write_worker, j);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    free(j);
}

int net_mc_poll_edit(int *x, int *y, int *z, int *block_id) {
    int ok = 0;
    lock();
    if (mc_edit_head != mc_edit_tail) {
        NetBlockEdit e = mc_edit_q[mc_edit_head];
        mc_edit_head = (mc_edit_head + 1) % MC_BLOCK_QUEUE_CAP;
        if (x)
            *x = e.x;
        if (y)
            *y = e.y;
        if (z)
            *z = e.z;
        if (block_id)
            *block_id = e.id;
        ok = 1;
    }
    unlock();
    return ok;
}

void net_mc_solo_path(char *out, size_t cap) { data_file_path(out, cap, "mc_solo.dat"); }

static void mc_blocks_ingest_json(const char *json, unsigned gen) {
    const char *p = skip_ws(json);
    if (!p || !*p || !strncmp(p, "null", 4) || *p != '{')
        return;
    lock();
    if (!net_gen_ok(gen)) {
        unlock();
        return;
    }
    for (p = skip_ws(p + 1); p && *p && *p != '}';) {
        if (*p != '\"')
            break;
        const char *kend = skip_str(p);
        char key[32] = {0};
        size_t kl;
        const char *obj;
        if (!kend)
            break;
        kl = (size_t)(kend - p - 2);
        if (kl >= sizeof(key))
            kl = sizeof(key) - 1;
        memcpy(key, p + 1, kl);
        key[kl] = 0;
        p = skip_ws(kend);
        if (*p != ':')
            break;
        p = skip_ws(p + 1);
        obj = p;
        p = skip_val(p);
        if (!p)
            break;
        if (key[0] == 'b' && key[1] == '_') {
            int bx = -1, by = -1, bz = -1, bid = -1;
            char txt[32] = {0};
            if (sscanf(key + 2, "%d_%d_%d", &bx, &by, &bz) == 3) {
                strv(obj, "text", txt, sizeof(txt));
                if (txt[0] == '@' && txt[1] == 'B' && txt[2] == ':' && sscanf(txt + 3, "%d", &bid) == 1) {
                    mc_queue_push_locked(bx, by, bz, bid);
                }
            }
        }
        p = skip_ws(p);
        if (p && *p == ',')
            p = skip_ws(p + 1);
    }
    unlock();
}
