/* Profile storage, warning/settings persistence, and cloud account auth on Firebase RTDB. */

static void geo_hash_password(const char *nick, const char *pass, char out_hex[32]) {
    uint64_t h1 = 1469598103934665603ULL;
    uint64_t h2 = 1099511628211987657ULL;
    const char *salt = "::Geometrium_Auth_2026::";
    size_t i;
    for (i = 0; nick && nick[i]; i++) {
        unsigned char c = (unsigned char)tolower((unsigned char)nick[i]);
        h1 ^= c;
        h1 *= 1099511628211ULL;
        h2 = (h2 * 1315423911ULL) ^ ((uint64_t)c + 0x9e3779b97f4a7c15ULL);
    }
    for (i = 0; salt[i]; i++) {
        h1 = (h1 ^ (unsigned char)salt[i]) * 1099511628211ULL;
        h2 ^= (h2 << 7) ^ (unsigned char)salt[i];
    }
    for (i = 0; pass && pass[i]; i++) {
        unsigned char c = (unsigned char)pass[i];
        h1 = (h1 ^ c) * 1099511628211ULL;
        h2 = ((h2 << 11) | (h2 >> 53)) ^ ((uint64_t)c * 2654435761ULL);
    }
    snprintf(out_hex, 32, "%012llx%012llx", (unsigned long long)(h1 & 0xffffffffffffULL),
             (unsigned long long)(h2 & 0xffffffffffffULL));
}

static void geo_ensure_defaults_locked(void) {
    if (!net.base[0])
        snprintf(net.base, sizeof(net.base), "%s", GEO_FIREBASE_URL);
    if (!net.uid[0]) {
        unsigned long a = (unsigned long)time(NULL) ^ (unsigned long)geo_now_ms();
        unsigned long b = (unsigned long)getpid() ^ 0x6e03e7UL;
        snprintf(net.uid, sizeof(net.uid), "g%06lx%06lx", a & 0xffffffUL, b & 0xffffffUL);
    }
    if (!net.nick[0])
        snprintf(net.nick, sizeof(net.nick), "Player_%04u", (unsigned int)(geo_now_ms() % 9000 + 1000));
    if (net.volume <= 0.0f && !net.profile_loaded)
        net.volume = 0.8f;
    if (net.sens <= 0.0f)
        net.sens = 1.0f;
}

void net_set_data_path(const char *path) {
    ds_mutex_lock(net_lock);
    snprintf(net.data_dir, sizeof(net.data_dir), "%s", path ? path : "");
    ds_mutex_unlock(net_lock);
}

void net_set_system_language(const char *code) {
    ds_mutex_lock(net_lock);
    if (!net.profile_loaded)
        net.lang = (!code || strncmp(code, "ru", 2) == 0 || strncmp(code, "uk", 2) == 0 ||
                    strncmp(code, "be", 2) == 0 || !code[0])
                       ? 1
                       : 0;
    ds_mutex_unlock(net_lock);
}

void net_set_base_url(const char *url) {
    size_t n;
    ds_mutex_lock(net_lock);
    snprintf(net.base, sizeof(net.base), "%s", (url && *url) ? url : GEO_FIREBASE_URL);
    n = strlen(net.base);
    while (n > 0 && net.base[n - 1] == '/')
        net.base[--n] = '\0';
    ds_mutex_unlock(net_lock);
}

const char *net_get_base_url(void) {
    static char buf[256];
    ds_mutex_lock(net_lock);
    geo_ensure_defaults_locked();
    snprintf(buf, sizeof(buf), "%s", net.base);
    ds_mutex_unlock(net_lock);
    return buf;
}

void net_solo_world_path(char *out, size_t cap) {
    ds_mutex_lock(net_lock);
    if (net.data_dir[0])
        snprintf(out, cap, "%s/geometrium_solo.dat", net.data_dir);
    else
        snprintf(out, cap, "/tmp/geometrium_solo.dat");
    ds_mutex_unlock(net_lock);
}

void net_profile_save(void) {
    char path[384];
    FILE *f;
    ds_mutex_lock(net_lock);
    geo_ensure_defaults_locked();
    snprintf(path, sizeof(path), "%s/geometrium_profile.dat", net.data_dir[0] ? net.data_dir : "/tmp");
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "uid=%s\nnick=%s\nhash=%s\nlogged=%d\nlang=%d\nwarn=%d\nvol=%.3f\nsens=%.3f\n", net.uid, net.nick,
                net.pass_hash, net.logged_in, net.lang, net.warn_ok, (double)net.volume, (double)net.sens);
        fclose(f);
    }
    ds_mutex_unlock(net_lock);
}

void net_profile_load(void) {
    char path[384], line[160];
    FILE *f;
    ds_mutex_lock(net_lock);
    if (net.lang == 0 && !net.profile_loaded)
        net.lang = 1;
    geo_ensure_defaults_locked();
    snprintf(path, sizeof(path), "%s/geometrium_profile.dat", net.data_dir[0] ? net.data_dir : "/tmp");
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            size_t l = strlen(line);
            while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r'))
                line[--l] = '\0';
            if (strncmp(line, "uid=", 4) == 0 && line[4])
                snprintf(net.uid, sizeof(net.uid), "%s", line + 4);
            else if (strncmp(line, "nick=", 5) == 0 && line[5])
                snprintf(net.nick, sizeof(net.nick), "%s", line + 5);
            else if (strncmp(line, "hash=", 5) == 0)
                snprintf(net.pass_hash, sizeof(net.pass_hash), "%s", line + 5);
            else if (strncmp(line, "logged=", 7) == 0)
                net.logged_in = atoi(line + 7);
            else if (strncmp(line, "lang=", 5) == 0)
                net.lang = atoi(line + 5);
            else if (strncmp(line, "warn=", 5) == 0)
                net.warn_ok = atoi(line + 5);
            else if (strncmp(line, "vol=", 4) == 0)
                net.volume = (float)atof(line + 4);
            else if (strncmp(line, "sens=", 5) == 0)
                net.sens = (float)atof(line + 5);
        }
        fclose(f);
    }
    net.profile_loaded = 1;
    ds_mutex_unlock(net_lock);
}

const char *net_nick(void) {
    static char buf[24];
    ds_mutex_lock(net_lock);
    geo_ensure_defaults_locked();
    snprintf(buf, sizeof(buf), "%s", net.nick);
    ds_mutex_unlock(net_lock);
    return buf;
}

void net_set_nick(const char *nick) {
    char clean[24];
    geo_sanitize_id(nick, clean, sizeof(clean), "Player");
    ds_mutex_lock(net_lock);
    snprintf(net.nick, sizeof(net.nick), "%s", clean);
    ds_mutex_unlock(net_lock);
    net_profile_save();
}

int net_language(void) {
    int v;
    ds_mutex_lock(net_lock);
    v = net.lang;
    ds_mutex_unlock(net_lock);
    return v;
}
void net_set_language(int lang) {
    ds_mutex_lock(net_lock);
    net.lang = lang ? 1 : 0;
    ds_mutex_unlock(net_lock);
    net_profile_save();
}
int net_warning_accepted(void) {
    int v;
    ds_mutex_lock(net_lock);
    v = net.warn_ok;
    ds_mutex_unlock(net_lock);
    return v;
}
void net_accept_warning(void) {
    ds_mutex_lock(net_lock);
    net.warn_ok = 1;
    ds_mutex_unlock(net_lock);
    net_profile_save();
}
float net_sound_volume(void) {
    float v;
    ds_mutex_lock(net_lock);
    v = net.volume;
    ds_mutex_unlock(net_lock);
    return v;
}
void net_set_sound_volume(float vol) {
    if (vol < 0.0f)
        vol = 0.0f;
    if (vol > 1.0f)
        vol = 1.0f;
    ds_mutex_lock(net_lock);
    net.volume = vol;
    ds_mutex_unlock(net_lock);
    net_profile_save();
}
float net_look_sens(void) {
    float v;
    ds_mutex_lock(net_lock);
    v = net.sens > 0.1f ? net.sens : 1.0f;
    ds_mutex_unlock(net_lock);
    return v;
}
void net_set_look_sens(float sens) {
    if (sens < 0.35f)
        sens = 0.35f;
    if (sens > 2.5f)
        sens = 2.5f;
    ds_mutex_lock(net_lock);
    net.sens = sens;
    ds_mutex_unlock(net_lock);
    net_profile_save();
}

typedef struct {
    char nick[24];
    char pass[48];
} AuthJob;

static void *auth_worker(void *arg) {
    AuthJob *job = (AuthJob *)arg;
    char base[256], key[24], url[384], resp[1024], hash[32], stored_hash[32], stored_uid[24], body[256];
    int i;
    geo_sanitize_id(job->nick, key, sizeof(key), "");
    for (i = 0; key[i]; i++)
        key[i] = (char)tolower((unsigned char)key[i]);
    if (strlen(key) < 2 || strlen(job->pass) < 4) {
        ds_mutex_lock(net_lock);
        net.auth_state = AUTH_BAD_INPUT;
        ds_mutex_unlock(net_lock);
        free(job);
        return NULL;
    }
    geo_hash_password(key, job->pass, hash);
    ds_mutex_lock(net_lock);
    geo_ensure_defaults_locked();
    snprintf(base, sizeof(base), "%s", net.base);
    ds_mutex_unlock(net_lock);

    snprintf(url, sizeof(url), "%s/geometrium/accounts/%s.json", base, key);
    if (!geo_http_get(url, resp, sizeof(resp))) {
        ds_mutex_lock(net_lock);
        net.auth_state = AUTH_NET_ERR;
        ds_mutex_unlock(net_lock);
        free(job);
        return NULL;
    }
    if (strcmp(resp, "null") == 0 || resp[0] == '\0') {
        ds_mutex_lock(net_lock);
        snprintf(stored_uid, sizeof(stored_uid), "%s", net.uid);
        ds_mutex_unlock(net_lock);
        snprintf(body, sizeof(body), "{\"nick\":\"%s\",\"hash\":\"%s\",\"uid\":\"%s\",\"created\":{\".sv\":\"timestamp\"}}",
                 key, hash, stored_uid);
        if (!geo_http_put(url, body)) {
            ds_mutex_lock(net_lock);
            net.auth_state = AUTH_NET_ERR;
            ds_mutex_unlock(net_lock);
            free(job);
            return NULL;
        }
    } else {
        geo_obj_str(resp, resp + strlen(resp), "hash", stored_hash, sizeof(stored_hash));
        geo_obj_str(resp, resp + strlen(resp), "uid", stored_uid, sizeof(stored_uid));
        if (!stored_hash[0] || strcmp(stored_hash, hash) != 0) {
            ds_mutex_lock(net_lock);
            net.auth_state = AUTH_WRONG_PASS;
            ds_mutex_unlock(net_lock);
            free(job);
            return NULL;
        }
    }
    ds_mutex_lock(net_lock);
    snprintf(net.nick, sizeof(net.nick), "%s", job->nick);
    geo_sanitize_id(net.nick, net.nick, sizeof(net.nick), key);
    snprintf(net.pass_hash, sizeof(net.pass_hash), "%s", hash);
    if (stored_uid[0])
        snprintf(net.uid, sizeof(net.uid), "%s", stored_uid);
    net.logged_in = 1;
    net.auth_state = AUTH_OK;
    ds_mutex_unlock(net_lock);
    net_profile_save();
    free(job);
    return NULL;
}

void net_account_auth(const char *nick, const char *password) {
    AuthJob *job = (AuthJob *)calloc(1, sizeof(*job));
    pthread_t th;
    if (!job)
        return;
    snprintf(job->nick, sizeof(job->nick), "%s", nick ? nick : "");
    snprintf(job->pass, sizeof(job->pass), "%s", password ? password : "");
    ds_mutex_lock(net_lock);
    net.auth_state = AUTH_BUSY;
    ds_mutex_unlock(net_lock);
    if (pthread_create(&th, NULL, auth_worker, job) == 0)
        pthread_detach(th);
    else
        free(job);
}

int net_account_status(void) {
    int s;
    ds_mutex_lock(net_lock);
    s = net.auth_state;
    ds_mutex_unlock(net_lock);
    return s;
}
int net_account_logged_in(void) {
    int s;
    ds_mutex_lock(net_lock);
    s = net.logged_in;
    ds_mutex_unlock(net_lock);
    return s;
}
void net_account_logout(void) {
    ds_mutex_lock(net_lock);
    net.logged_in = 0;
    net.pass_hash[0] = '\0';
    net.auth_state = AUTH_IDLE;
    net.uid[0] = '\0';
    geo_ensure_defaults_locked();
    ds_mutex_unlock(net_lock);
    net_profile_save();
}
