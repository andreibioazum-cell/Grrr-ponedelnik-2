/* Смена пароля аккаунта (экран «Аккаунт → Сменить пароль»).
 *
 * Идёт в своём потоке:
 *   1. текущий пароль должен совпасть с паролем сессии;
 *   2. свежий вход текущим паролем: Firebase меняет пароль только после
 *      недавнего входа, а сервер заодно ещё раз проверяет пароль;
 *   3. accounts:update ставит новый пароль (с тем же секретным хвостом, что и
 *      при входе) и выдаёт новые токены;
 *   4. сессия и файл сессии переходят на новый пароль, чтобы автовход не
 *      сломался. На других устройствах аккаунт придётся войти заново.
 * В старом режиме без Firebase пароль лежит в профиле: меняется поле pass.
 * Статус: 0 — ничего, 1 — идёт, 2 — изменён, 3 — ошибка сети или сервера,
 * 4 — неверный текущий пароль, 5 — новый пароль не подходит, 6 — не вошли. */

static volatile int s_pass_chg_status = 0;
static char s_pass_chg_old[64], s_pass_chg_new[64];

static int pass_change_firebase(const char *nick, const char *old_pass, const char *new_pass) {
    char url[FB_URL_MAX], body[512], resp[2048], email[96], epass[192], idt[FB_ID_MAX], rt[FB_REFRESH_MAX];
    int code, r;
    fb_email_of(nick, email, sizeof(email));
    snprintf(epass, sizeof(epass), "%s%s", old_pass, net_pass_suffix());
    r = fb_try_sign_in(email, epass, NULL);
    memset(epass, 0, sizeof(epass));
    if (r != 0)
        return r == 1 ? 4 : 3;
    fb_id_token(idt, sizeof(idt));
    if (!idt[0])
        return 3;
    snprintf(epass, sizeof(epass), "%s%s", new_pass, net_pass_suffix());
    fb_auth_endpoint(url, sizeof(url), "/v1/accounts:update");
    snprintf(body, sizeof(body), "{\"idToken\":\"%s\",\"password\":\"%s\",\"returnSecureToken\":true}", idt, epass);
    memset(epass, 0, sizeof(epass));
    code = http("POST", url, body, resp, sizeof(resp));
    memset(body, 0, sizeof(body));
    if (code != 200) {
        if (net_log_ok())
            LOGERR("password change: HTTP %d %.160s", code, resp);
        return fb_err_is(resp, "WEAK_PASSWORD") ? 5 : 3;
    }
    rt[0] = 0;
    idt[0] = 0;
    strv(resp, "idToken", idt, sizeof(idt));
    strv(resp, "refreshToken", rt, sizeof(rt));
    if (idt[0])
        fb_store_tokens(idt, rt, num(resp, "expiresIn", 3600));
    return 2;
}

static int pass_change_legacy(const char *nick, const char *new_pass) {
    char url[URL], body[BODY];
    if (!net.base[0])
        return 2;
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    snprintf(body, sizeof(body), "{\"pass\":\"%s\"}", new_pass);
    net_seal_body(body, sizeof(body));
    return fb_http("PATCH", url, body, NULL, 0) == 200 ? 2 : 3;
}

static void *pass_change_job(void *arg) {
    char nick[LOGIN_NICK_MAX + 2] = "", sess[64] = "", old_pass[64], new_pass[64];
    int result;
    (void)arg;
    snprintf(old_pass, sizeof(old_pass), "%s", s_pass_chg_old);
    snprintf(new_pass, sizeof(new_pass), "%s", s_pass_chg_new);
    memset(s_pass_chg_old, 0, sizeof(s_pass_chg_old));
    memset(s_pass_chg_new, 0, sizeof(s_pass_chg_new));
    lg_lock();
    if (lg.status == NET_LOGIN_OK) {
        snprintf(nick, sizeof(nick), "%s", lg.session_nick);
        snprintf(sess, sizeof(sess), "%s", lg.session_pass);
    }
    lg_unlock();
    if (!nick[0])
        result = 6;
    else if (!sess[0] || strcmp(sess, old_pass) != 0)
        result = 4;
    else if (fb_enabled())
        result = pass_change_firebase(nick, old_pass, new_pass);
    else
        result = pass_change_legacy(nick, new_pass);
    if (result == 2) {
        lg_lock();
        if (strcmp(lg.session_nick, nick) == 0)
            snprintf(lg.session_pass, sizeof(lg.session_pass), "%s", new_pass);
        lg_unlock();
        session_save(nick, new_pass);
        if (net_log_ok())
            LOG("password changed for '%s'", nick);
    }
    memset(old_pass, 0, sizeof(old_pass));
    memset(new_pass, 0, sizeof(new_pass));
    memset(sess, 0, sizeof(sess));
    s_pass_chg_status = result;
    return NULL;
}

void net_change_password(const char *old_pass, const char *new_pass) {
    DSThread t;
    if (s_pass_chg_status == 1)
        return;
    if (!new_pass || !pass_valid(new_pass)) {
        s_pass_chg_status = 5;
        return;
    }
    if (!old_pass || !*old_pass) {
        s_pass_chg_status = 4;
        return;
    }
    snprintf(s_pass_chg_old, sizeof(s_pass_chg_old), "%s", old_pass);
    snprintf(s_pass_chg_new, sizeof(s_pass_chg_new), "%s", new_pass);
    s_pass_chg_status = 1;
    t = ds_thread_start(pass_change_job, NULL);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    memset(s_pass_chg_old, 0, sizeof(s_pass_chg_old));
    memset(s_pass_chg_new, 0, sizeof(s_pass_chg_new));
    s_pass_chg_status = 3;
}

/* Прочитать статус; готовый результат (2 и выше) отдаётся один раз. */
double net_change_password_status(void) {
    int s = s_pass_chg_status;
    if (s >= 2)
        s_pass_chg_status = 0;
    return (double)s;
}

/* ---- Admin reset of another player's password ----
 * The client sends the admin's own Firebase ID token to the
 * adminResetPassword Cloud Function; only the function (Admin SDK) can
 * change another account's password. Statuses: 0 idle, 1 running,
 * 2 changed, 3 error or function not deployed, 4 bad new password,
 * 5 bad target nick (result is handed out once). */
static volatile int s_admin_reset_status = 0;
static char s_admin_reset_nick[24], s_admin_reset_new[64];

static void *admin_reset_job(void *arg) {
    char nick[24], new_pass[64], idt[FB_ID_MAX], bearer[FB_ID_MAX + 16], body[512], resp[2048], enick[64], epass[96];
    int code, result = 3;
    (void)arg;
    snprintf(nick, sizeof(nick), "%s", s_admin_reset_nick);
    snprintf(new_pass, sizeof(new_pass), "%s", s_admin_reset_new);
    memset(s_admin_reset_new, 0, sizeof(s_admin_reset_new));
    if (!fb_enabled() || !fb_ensure_token())
        goto done;
    fb_id_token(idt, sizeof(idt));
    if (!idt[0])
        goto done;
    snprintf(bearer, sizeof(bearer), "Bearer %s", idt);
    json_escape(nick, enick, sizeof(enick));
    json_escape(new_pass, epass, sizeof(epass));
    /* Callable-function protocol: {"data": {...}} + caller ID token. */
    snprintf(body, sizeof(body), "{\"data\":{\"nick\":\"%s\",\"password\":\"%s\"}}", enick, epass);
    code = http_ex("POST", NET_ADMIN_RESET_URL, body, resp, sizeof(resp), "Authorization", bearer, NULL, 0);
    memset(body, 0, sizeof(body));
    memset(bearer, 0, sizeof(bearer));
    if (code == 200 && strstr(resp, "\"result\"") && strstr(resp, "\"ok\":true"))
        result = 2;
    else if (net_log_ok())
        LOGERR("admin password reset for '%s': HTTP %d %.160s", nick, code, resp);
done:
    memset(new_pass, 0, sizeof(new_pass));
    s_admin_reset_status = result;
    return NULL;
}

void net_admin_reset_password(const char *nick, const char *new_pass) {
    DSThread t;
    if (s_admin_reset_status == 1)
        return;
    if (!new_pass || !pass_valid(new_pass)) {
        s_admin_reset_status = 4;
        return;
    }
    if (!nick || !*nick || strlen(nick) >= sizeof(s_admin_reset_nick)) {
        s_admin_reset_status = 5;
        return;
    }
    snprintf(s_admin_reset_nick, sizeof(s_admin_reset_nick), "%s", nick);
    snprintf(s_admin_reset_new, sizeof(s_admin_reset_new), "%s", new_pass);
    s_admin_reset_status = 1;
    t = ds_thread_start(admin_reset_job, NULL);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    memset(s_admin_reset_new, 0, sizeof(s_admin_reset_new));
    s_admin_reset_status = 3;
}

double net_admin_reset_status(void) {
    int s = s_admin_reset_status;
    if (s >= 2)
        s_admin_reset_status = 0;
    return (double)s;
}

static const char *gift_id(double gift) {
    static const char *ids[] = {"rose", "cake", "bear", "rocket"};
    int i = (int)gift;
    return i >= 0 && i < 4 ? ids[i] : "";
}
static volatile int s_gift_status = 0;
static char s_gift_recipient[24];
static int s_gift_id = 0, s_gift_grant = 0;
static int s_gift_counts[4] = {0, 0, 0, 0};
static volatile int s_gift_counts_loaded = 0;
static volatile int s_gift_counts_requested = 0;
static void *gift_action_job(void *arg) {
    char idt[FB_ID_MAX], bearer[FB_ID_MAX + 16], body[320], resp[2048], enick[64], esender[64];
    char nick[24], sender[24], gift[16];
    int code, result = 3;
    (void)arg;
    snprintf(nick, sizeof(nick), "%s", s_gift_recipient);
    lg_lock(); snprintf(sender, sizeof(sender), "%s", lg.session_nick); lg_unlock();
    snprintf(gift, sizeof(gift), "%s", gift_id(s_gift_id));
    if (!gift[0] || !sender[0] || !fb_enabled() || !fb_ensure_token()) goto gift_done;
    fb_id_token(idt, sizeof(idt));
    if (!idt[0]) goto gift_done;
    snprintf(bearer, sizeof(bearer), "Bearer %s", idt);
    json_escape(nick, enick, sizeof(enick));
    json_escape(sender, esender, sizeof(esender));
    const char *action = s_gift_grant == 1 ? "grant" : s_gift_grant == 2 ? "buy" : "send";
    snprintf(body, sizeof(body), "{\"data\":{\"action\":\"%s\",\"gift\":\"%s\",\"sender\":\"%s\",\"recipient\":\"%s\"}}",
             action, gift, esender, enick);
    code = http_ex("POST", NET_GIFT_ACTION_URL, body, resp, sizeof(resp), "Authorization", bearer, NULL, 0);
    if (code == 200 && strstr(resp, "\"result\"") && strstr(resp, "\"ok\":true")) result = 2;
    else {
        if (code == 404 || strstr(resp, "NOT_FOUND")) result = 4; /* function not deployed */
        else if (strstr(resp, "not enough cups")) result = 5;
        else if (strstr(resp, "no gift in inventory")) result = 6;
        else if (strstr(resp, "sender identity mismatch") || strstr(resp, "bad action")) result = 7;
        if (net_log_ok()) LOGERR("gift action: HTTP %d %.160s", code, resp);
    }
    memset(body, 0, sizeof(body)); memset(bearer, 0, sizeof(bearer));
gift_done:
    s_gift_status = result;
    return NULL;
}
void net_gift_action(double gift, const char *recipient, double grant) {
    DSThread t;
    if (s_gift_status == 1 || gift < 0 || gift > 3 || !recipient || strlen(recipient) >= sizeof(s_gift_recipient) ||
        (grant != 2 && !*recipient)) return;
    snprintf(s_gift_recipient, sizeof(s_gift_recipient), "%s", recipient);
    s_gift_id = (int)gift; s_gift_grant = grant == 1;
    s_gift_status = 1;
    t = ds_thread_start(gift_action_job, NULL);
    if (t) { ds_thread_detach(t); return; }
    s_gift_status = 3;
}
double net_gift_action_status(void) {
    int s = s_gift_status;
    if (s >= 2) s_gift_status = 0;
    return (double)s;
}
static void *gift_counts_job(void *arg) {
    char nick[24], plain[URL], url[FB_URL_MAX], resp[8192];
    (void)arg;
    lg_lock(); snprintf(nick, sizeof(nick), "%s", lg.session_nick); lg_unlock();
    if (nick[0] && fb_enabled() && fb_ensure_token()) {
        snprintf(plain, sizeof(plain), "%s/users/%s/gifts.json", net.base, nick);
        fb_url(url, sizeof(url), plain);
        if (http("GET", url, NULL, resp, sizeof(resp)) == 200) {
            s_gift_counts[0] = (int)num(resp, "rose", 0);
            s_gift_counts[1] = (int)num(resp, "cake", 0);
            s_gift_counts[2] = (int)num(resp, "bear", 0);
            s_gift_counts[3] = (int)num(resp, "rocket", 0);
        }
    }
    s_gift_counts_loaded = 1;
    return NULL;
}
void net_gift_refresh(void) {
    s_gift_counts_loaded = 0;
    s_gift_counts_requested = 0;
    memset(s_gift_counts, 0, sizeof(s_gift_counts));
}
double net_gift_count(double gift) {
    if (!s_gift_counts_loaded && !s_gift_counts_requested) {
        DSThread t; s_gift_counts_requested = 1; t = ds_thread_start(gift_counts_job, NULL);
        if (t) ds_thread_detach(t); else s_gift_counts_loaded = 1;
    }
    int i = (int)gift;
    return i >= 0 && i < 4 ? s_gift_counts[i] : 0;
}
