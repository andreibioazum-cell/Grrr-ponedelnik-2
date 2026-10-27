/* Authenticated, participant-only direct messages in Realtime Database.
 * Each message is mirrored into both users' inbox paths so either client can
 * read its own conversation without listing the whole database. The account
 * token is appended by fb_http(), and firebase.rules.json checks sender,
 * recipient, message size and the existing Firebase write seal. */
#define DM_CONTACT_MAX 12
#define DM_MESSAGE_MAX 32
#define DM_NICK_MAX 17
#define DM_TEXT_MAX 321
#define DM_RESPONSE_MAX 16384
#define DM_JOB_CONTACTS 1
#define DM_JOB_OPEN 2
#define DM_JOB_REFRESH 3
#define DM_JOB_SEND 4

typedef struct {
    char key[64];
    char from[DM_NICK_MAX];
    char text[DM_TEXT_MAX];
    long long ts;
} DirectMessage;

typedef struct {
    DSMutex lock;
    int contacts_status; /* 0 idle, 1 loading, 2 ready, 4 error */
    int contact_count;
    char contacts[DM_CONTACT_MAX][DM_NICK_MAX];
    int conversation_status; /* 0 idle, 1 loading, 2 ready, 3 invalid, 4 error, 5 no such user */
    int message_count;
    DirectMessage messages[DM_MESSAGE_MAX];
    char self[DM_NICK_MAX];
    char peer[DM_NICK_MAX];
    int send_status; /* 0 idle, 1 sending, 2 sent, 3 error */
    unsigned contacts_gen;
    unsigned conversation_gen;
    unsigned long message_seq;
} DirectMessageState;

static DirectMessageState dm = {.lock = DS_MUTEX_INIT};

typedef struct {
    int kind;
    unsigned gen;
    char base[256];
    char self[DM_NICK_MAX];
    char peer[DM_NICK_MAX];
    char text[DM_TEXT_MAX];
} DirectMessageJob;

static int dm_nick_valid(const char *nick) {
    size_t i, n;
    if (!nick) return 0;
    n = strlen(nick);
    if (n < 1 || n >= DM_NICK_MAX) return 0;
    for (i = 0; i < n; i++) {
        char c = nick[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

static int dm_nick_equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
    }
    return *a == *b;
}

static void dm_copy_base(char *dst, size_t cap, const char *url) {
    size_t n;
    snprintf(dst, cap, "%s", (url && *url) ? url : net.base);
    n = strlen(dst);
    while (n && dst[n - 1] == '/') dst[--n] = 0;
}

static void dm_copy_self(char *dst, size_t cap) {
    lg_lock();
    snprintf(dst, cap, "%s", lg.session_nick);
    lg_unlock();
}

static DirectMessageJob *dm_job_new(int kind, const char *base, const char *self, const char *peer, const char *text) {
    DirectMessageJob *job = (DirectMessageJob *)calloc(1, sizeof(*job));
    if (!job) return NULL;
    job->kind = kind;
    dm_copy_base(job->base, sizeof(job->base), base);
    snprintf(job->self, sizeof(job->self), "%s", self ? self : "");
    snprintf(job->peer, sizeof(job->peer), "%s", peer ? peer : "");
    snprintf(job->text, sizeof(job->text), "%s", text ? text : "");
    return job;
}

static int dm_parse_contacts(const char *json, char out[DM_CONTACT_MAX][DM_NICK_MAX]) {
    const char *p = skip_ws(json);
    int count = 0;
    if (!p || *p != '{') return 0;
    for (p = skip_ws(p + 1); p && *p && *p != '}' && count < DM_CONTACT_MAX;) {
        const char *end = skip_str(p);
        size_t n;
        char nick[DM_NICK_MAX];
        if (!end) break;
        n = (size_t)(end - p - 2);
        if (n >= sizeof(nick)) n = 0;
        memcpy(nick, p + 1, n);
        nick[n] = 0;
        p = skip_ws(end);
        if (*p != ':') break;
        p = skip_val(p + 1);
        if (!p) break;
        if (dm_nick_valid(nick)) {
            int duplicate = 0;
            for (int i = 0; i < count; i++) if (strcmp(out[i], nick) == 0) duplicate = 1;
            if (!duplicate) snprintf(out[count++], DM_NICK_MAX, "%s", nick);
        }
        p = skip_ws(p);
        if (*p == ',') p = skip_ws(p + 1);
    }
    return count;
}

static int dm_parse_messages(const char *json, DirectMessage *out, int cap) {
    const char *p = skip_ws(json);
    DirectMessage parsed[DM_MESSAGE_MAX + 8];
    int count = 0;
    if (!p || *p != '{') return 0;
    for (p = skip_ws(p + 1); p && *p && *p != '}' && count < cap && count < DM_MESSAGE_MAX + 8;) {
        const char *key_start = p + 1;
        const char *key_end = skip_str(p);
        const char *object;
        size_t key_len;
        if (!key_end) break;
        key_len = (size_t)(key_end - p - 2);
        p = skip_ws(key_end);
        if (*p != ':') break;
        object = skip_ws(p + 1);
        p = skip_val(object);
        if (!p) break;
        memset(&parsed[count], 0, sizeof(parsed[count]));
        if (key_len >= sizeof(parsed[count].key)) key_len = sizeof(parsed[count].key) - 1;
        memcpy(parsed[count].key, key_start, key_len);
        parsed[count].key[key_len] = 0;
        strv(object, "from", parsed[count].from, sizeof(parsed[count].from));
        strv(object, "text", parsed[count].text, sizeof(parsed[count].text));
        parsed[count].ts = num_ll(object, "ts", 0);
        if (dm_nick_valid(parsed[count].from) && parsed[count].text[0]) count++;
        p = skip_ws(p);
        if (*p == ',') p = skip_ws(p + 1);
    }
    /* Firebase orders by push key; sort a bounded window by the server timestamp. */
    for (int i = 1; i < count; i++) {
        DirectMessage item = parsed[i];
        int j = i;
        while (j > 0 && parsed[j - 1].ts > item.ts) {
            parsed[j] = parsed[j - 1];
            j--;
        }
        parsed[j] = item;
    }
    if (count > cap) {
        int drop = count - cap;
        memmove(parsed, parsed + drop, (size_t)cap * sizeof(parsed[0]));
        count = cap;
    }
    memcpy(out, parsed, (size_t)count * sizeof(parsed[0]));
    return count;
}

static int dm_fetch_conversation(DirectMessageJob *job, const char *peer, int verify_user) {
    char url[FB_URL_MAX], response[DM_RESPONSE_MAX], canonical[DM_NICK_MAX];
    int code;
    if (verify_user) {
        snprintf(url, sizeof(url), "%s/users/%s.json", job->base, peer);
        response[0] = 0;
        code = fb_http("GET", url, NULL, response, sizeof(response));
        canonical[0] = 0;
        if (code != 200) return 4;
        if (response[0] == 0 || strncmp(skip_ws(response), "null", 4) == 0) return 5;
        strv(response, "nick", canonical, sizeof(canonical));
        if (!dm_nick_valid(canonical) || dm_nick_equal(canonical, job->self)) return 3;
        snprintf(job->peer, sizeof(job->peer), "%s", canonical);
        ds_mutex_lock(dm.lock);
        if (dm.conversation_gen == job->gen) snprintf(dm.peer, sizeof(dm.peer), "%s", canonical);
        ds_mutex_unlock(dm.lock);
    }
    snprintf(url, sizeof(url), "%s/direct/%s/%s.json?orderBy=%%22$key%%22&limitToLast=%d",
             job->base, job->self, job->peer, DM_MESSAGE_MAX);
    response[0] = 0;
    code = fb_http("GET", url, NULL, response, sizeof(response));
    if (code != 200) return 4;
    DirectMessage parsed[DM_MESSAGE_MAX];
    int count = dm_parse_messages(response, parsed, DM_MESSAGE_MAX);
    ds_mutex_lock(dm.lock);
    if (dm.conversation_gen == job->gen && strcmp(dm.peer, job->peer) == 0) {
        memcpy(dm.messages, parsed, (size_t)count * sizeof(parsed[0]));
        dm.message_count = count;
        dm.conversation_status = 2;
    }
    ds_mutex_unlock(dm.lock);
    return 2;
}

static void *dm_job_run(void *arg) {
    DirectMessageJob *job = (DirectMessageJob *)arg;
    if (job->kind == DM_JOB_CONTACTS) {
        char url[FB_URL_MAX], response[DM_RESPONSE_MAX];
        char contacts[DM_CONTACT_MAX][DM_NICK_MAX] = {{0}};
        snprintf(url, sizeof(url), "%s/direct/%s.json?shallow=true", job->base, job->self);
        response[0] = 0;
        int code = fb_http("GET", url, NULL, response, sizeof(response));
        int count = code == 200 ? dm_parse_contacts(response, contacts) : -1;
        ds_mutex_lock(dm.lock);
        if (dm.contacts_gen == job->gen) {
            if (count >= 0) {
                memcpy(dm.contacts, contacts, sizeof(contacts));
                dm.contact_count = count;
                dm.contacts_status = 2;
            } else dm.contacts_status = 4;
        }
        ds_mutex_unlock(dm.lock);
    } else if (job->kind == DM_JOB_OPEN || job->kind == DM_JOB_REFRESH) {
        int status = dm_fetch_conversation(job, job->peer, job->kind == DM_JOB_OPEN);
        ds_mutex_lock(dm.lock);
        if (dm.conversation_gen == job->gen && status != 2) dm.conversation_status = status;
        ds_mutex_unlock(dm.lock);
    } else if (job->kind == DM_JOB_SEND) {
        char url1[FB_URL_MAX], url2[FB_URL_MAX], body[1200], escaped[DM_TEXT_MAX * 2];
        char uid[24], key[64];
        unsigned long seq;
        long long timestamp = (long long)time(NULL) * 1000;
        json_escape(job->text, escaped, sizeof(escaped));
        ds_mutex_lock(net.lock);
        if (!net.uid[0]) make_uid();
        snprintf(uid, sizeof(uid), "%s", net.uid);
        ds_mutex_unlock(net.lock);
        ds_mutex_lock(dm.lock);
        seq = ++dm.message_seq;
        ds_mutex_unlock(dm.lock);
        snprintf(key, sizeof(key), "m%llx_%s_%lx", (unsigned long long)timestamp, uid, seq);
        snprintf(body, sizeof(body), "{\"from\":\"%s\",\"to\":\"%s\",\"text\":\"%s\",\"ts\":%lld}",
                 job->self, job->peer, escaped, timestamp);
        if (!net_seal_body(body, sizeof(body))) {
            ds_mutex_lock(dm.lock); dm.send_status = 3; ds_mutex_unlock(dm.lock);
            free(job);
            return NULL;
        }
        snprintf(url1, sizeof(url1), "%s/direct/%s/%s/%s.json", job->base, job->self, job->peer, key);
        snprintf(url2, sizeof(url2), "%s/direct/%s/%s/%s.json", job->base, job->peer, job->self, key);
        int first = fb_http("PUT", url1, body, NULL, 0);
        int second = first == 200 ? fb_http("PUT", url2, body, NULL, 0) : 0;
        ds_mutex_lock(dm.lock);
        dm.send_status = (first == 200 && second == 200) ? 2 : 3;
        ds_mutex_unlock(dm.lock);
        if (first == 200 && second == 200) {
            dm_fetch_conversation(job, job->peer, 0);
        }
    }
    free(job);
    return NULL;
}

static int dm_start_job(DirectMessageJob *job) {
    DSThread thread;
    if (!job) return 0;
    thread = ds_thread_start(dm_job_run, job);
    if (!thread) {
        free(job);
        return 0;
    }
    ds_thread_detach(thread);
    return 1;
}

void net_dm_contacts_fetch(const char *url) {
    char self[DM_NICK_MAX];
    unsigned gen;
    DirectMessageJob *job;
    dm_copy_self(self, sizeof(self));
    if (!dm_nick_valid(self)) {
        ds_mutex_lock(dm.lock); dm.contacts_status = 4; dm.contact_count = 0; ds_mutex_unlock(dm.lock);
        return;
    }
    ds_mutex_lock(dm.lock);
    snprintf(dm.self, sizeof(dm.self), "%s", self);
    dm.contacts_status = 1;
    gen = ++dm.contacts_gen;
    ds_mutex_unlock(dm.lock);
    job = dm_job_new(DM_JOB_CONTACTS, url, self, NULL, NULL);
    if (!job) {
        ds_mutex_lock(dm.lock); dm.contacts_status = 4; ds_mutex_unlock(dm.lock);
        return;
    }
    job->gen = gen;
    if (!dm_start_job(job)) {
        ds_mutex_lock(dm.lock); dm.contacts_status = 4; ds_mutex_unlock(dm.lock);
    }
}

void net_dm_open(const char *url, const char *peer) {
    char self[DM_NICK_MAX];
    unsigned gen;
    DirectMessageJob *job;
    dm_copy_self(self, sizeof(self));
    if (!dm_nick_valid(self) || !dm_nick_valid(peer) || dm_nick_equal(self, peer)) {
        ds_mutex_lock(dm.lock);
        ++dm.conversation_gen;
        dm.conversation_status = 3;
        dm.message_count = 0;
        dm.peer[0] = 0;
        ds_mutex_unlock(dm.lock);
        return;
    }
    ds_mutex_lock(dm.lock);
    snprintf(dm.self, sizeof(dm.self), "%s", self);
    snprintf(dm.peer, sizeof(dm.peer), "%s", peer);
    dm.conversation_status = 1;
    dm.message_count = 0;
    gen = ++dm.conversation_gen;
    ds_mutex_unlock(dm.lock);
    job = dm_job_new(DM_JOB_OPEN, url, self, peer, NULL);
    if (!job) {
        ds_mutex_lock(dm.lock); dm.conversation_status = 4; ds_mutex_unlock(dm.lock);
        return;
    }
    job->gen = gen;
    if (!dm_start_job(job)) {
        ds_mutex_lock(dm.lock); dm.conversation_status = 4; ds_mutex_unlock(dm.lock);
    }
}

void net_dm_refresh(const char *url) {
    char self[DM_NICK_MAX], peer[DM_NICK_MAX];
    unsigned gen;
    DirectMessageJob *job;
    ds_mutex_lock(dm.lock);
    snprintf(self, sizeof(self), "%s", dm.self);
    snprintf(peer, sizeof(peer), "%s", dm.peer);
    if (dm.conversation_status == 1 || !peer[0]) {
        ds_mutex_unlock(dm.lock);
        return;
    }
    dm.conversation_status = 1;
    gen = ++dm.conversation_gen;
    ds_mutex_unlock(dm.lock);
    job = dm_job_new(DM_JOB_REFRESH, url, self, peer, NULL);
    if (!job) {
        ds_mutex_lock(dm.lock); dm.conversation_status = 4; ds_mutex_unlock(dm.lock);
        return;
    }
    job->gen = gen;
    if (!dm_start_job(job)) {
        ds_mutex_lock(dm.lock); dm.conversation_status = 4; ds_mutex_unlock(dm.lock);
    }
}

void net_dm_send(const char *url, const char *text) {
    char self[DM_NICK_MAX], peer[DM_NICK_MAX];
    unsigned gen;
    DirectMessageJob *job;
    size_t n;
    ds_mutex_lock(dm.lock);
    snprintf(self, sizeof(self), "%s", dm.self);
    snprintf(peer, sizeof(peer), "%s", dm.peer);
    gen = dm.conversation_gen;
    ds_mutex_unlock(dm.lock);
    n = text ? strlen(text) : 0;
    if (!dm_nick_valid(self) || !dm_nick_valid(peer) || !text || n == 0 || n >= DM_TEXT_MAX) {
        ds_mutex_lock(dm.lock); dm.send_status = 3; ds_mutex_unlock(dm.lock);
        return;
    }
    ds_mutex_lock(dm.lock);
    if (dm.send_status == 1) { ds_mutex_unlock(dm.lock); return; }
    dm.send_status = 1;
    ds_mutex_unlock(dm.lock);
    job = dm_job_new(DM_JOB_SEND, url, self, peer, text);
    if (job) job->gen = gen;
    if (!dm_start_job(job)) {
        ds_mutex_lock(dm.lock); dm.send_status = 3; ds_mutex_unlock(dm.lock);
    }
}

double net_dm_contacts_status(void) { double s; ds_mutex_lock(dm.lock); s = dm.contacts_status; ds_mutex_unlock(dm.lock); return s; }
double net_dm_contacts_count(void) { double n; ds_mutex_lock(dm.lock); n = dm.contact_count; ds_mutex_unlock(dm.lock); return n; }
const char *net_dm_contact(double index) {
    static char value[DM_NICK_MAX]; int i = (int)index;
    ds_mutex_lock(dm.lock);
    if (i < 0 || i >= dm.contact_count) value[0] = 0;
    else snprintf(value, sizeof(value), "%s", dm.contacts[i]);
    ds_mutex_unlock(dm.lock); return value;
}
double net_dm_status(void) { double s; ds_mutex_lock(dm.lock); s = dm.conversation_status; ds_mutex_unlock(dm.lock); return s; }
double net_dm_count(void) { double n; ds_mutex_lock(dm.lock); n = dm.message_count; ds_mutex_unlock(dm.lock); return n; }
const char *net_dm_peer(void) { static char value[DM_NICK_MAX]; ds_mutex_lock(dm.lock); snprintf(value, sizeof(value), "%s", dm.peer); ds_mutex_unlock(dm.lock); return value; }
const char *net_dm_from(double index) {
    static char value[DM_NICK_MAX]; int i = (int)index;
    ds_mutex_lock(dm.lock);
    if (i < 0 || i >= dm.message_count) value[0] = 0;
    else snprintf(value, sizeof(value), "%s", dm.messages[i].from);
    ds_mutex_unlock(dm.lock); return value;
}
const char *net_dm_text(double index) {
    static char value[DM_TEXT_MAX]; int i = (int)index;
    ds_mutex_lock(dm.lock);
    if (i < 0 || i >= dm.message_count) value[0] = 0;
    else snprintf(value, sizeof(value), "%s", dm.messages[i].text);
    ds_mutex_unlock(dm.lock); return value;
}
double net_dm_timestamp(double index) {
    double value = 0; int i = (int)index;
    ds_mutex_lock(dm.lock);
    if (i >= 0 && i < dm.message_count) value = (double)dm.messages[i].ts;
    ds_mutex_unlock(dm.lock); return value;
}
double net_dm_send_status(void) { double s; ds_mutex_lock(dm.lock); s = dm.send_status; ds_mutex_unlock(dm.lock); return s; }
