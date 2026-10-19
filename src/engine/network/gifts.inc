/* ---- Подарки без Cloud Functions ----
 * Инвентарь и покупки идут обычными запросами к Realtime Database:
 *   users/<ник>/gifts/<стикер> — счётчик 0..999;
 *   users/<ник>/cups           — кубки.
 * Пять стикеров: poop («какашка»), cake, bear, rocket и самый дорогой —
 * emerald (изумруд, 500 кубков); старые счётчики rose остаются в базе, но
 * больше не показываются.
 * Купить — одна запись users/<ник>.json: кубки минус цена и +1 стикер, так
 * покупка не рассыпается на две.
 * Подарить — сначала -1 у себя, затем +1 получателю; если вторая запись не
 * прошла, свой счётчик возвращается. Продать — одна запись: -1 стикер и
 * +половина цены в кубки. firebase.rules.json разрешает чужому игроку
 * только +1 в gifts (и не даёт трогать остальной профиль или удалять чужие
 * стикеры); владелец может писать свои счётчики свободно, поэтому продажа
 * проходит теми же правилами, что и покупка.
 * Статусы: 0 — ничего, 1 — идёт, 2 — готово, 3 — сеть или сервер,
 * 4 — получатель не найден, 5 — не хватает кубков, 6 — нет стикера
 * в инвентаре, 7 — плохой ник или подарок самому себе,
 * 8 — коллекция переполнена (999). */

#define GIFT_N 5

static volatile int s_gift_status = 0;
static char s_gift_recipient[24];
static int s_gift_id = 0, s_gift_grant = 0;
static int s_gift_counts[GIFT_N] = {0, 0, 0, 0, 0};
static volatile int s_gift_cups = -1; /* balance the last buy/sell left in the DB */
static volatile int s_gift_counts_loaded = 0;
static volatile int s_gift_counts_requested = 0;

static const char *gift_id(double gift) {
    static const char *ids[GIFT_N] = {"poop", "cake", "bear", "rocket", "emerald"};
    int i = (int)gift;
    return i >= 0 && i < GIFT_N ? ids[i] : "";
}

static int gift_price(int i) {
    static const int prices[GIFT_N] = {25, 100, 250, 400, 500};
    return i >= 0 && i < GIFT_N ? prices[i] : 0;
}

/* Selling returns half of the shop price: the shop stays the cheaper way to
 * get a sticker, while a duplicate from an event is still worth something. */
static int gift_refund_for(int i) { return gift_price(i) / 2; }

static int gift_nick_ok(const char *nick) {
    size_t i, n;
    if (!nick || !(n = strlen(nick)) || n > 16)
        return 0;
    for (i = 0; i < n; i++) {
        char c = nick[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
            return 0;
    }
    return 1;
}

static int gift_nick_same(const char *a, const char *b) {
    size_t i;
    if (!a || !b)
        return 0;
    for (i = 0; a[i] && b[i]; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z')
            x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z')
            y = (char)(y - 'A' + 'a');
        if (x != y)
            return 0;
    }
    return a[i] == 0 && b[i] == 0;
}

/* Счётчики из users/<ник>/gifts.json; отсутствующий узел ("null") — нули. */
static int gift_get_counts(const char *nick, int out[GIFT_N]) {
    char url[URL], resp[2048];
    int i;
    for (i = 0; i < GIFT_N; i++)
        out[i] = 0;
    snprintf(url, sizeof(url), "%s/users/%s/gifts.json", net.base, nick);
    if (fb_http("GET", url, NULL, resp, sizeof(resp)) != 200)
        return 0;
    out[0] = num_int(resp, "poop", 0);
    out[1] = num_int(resp, "cake", 0);
    out[2] = num_int(resp, "bear", 0);
    out[3] = num_int(resp, "rocket", 0);
    out[4] = num_int(resp, "emerald", 0);
    for (i = 0; i < GIFT_N; i++) {
        if (out[i] < 0)
            out[i] = 0;
        if (out[i] > 999)
            out[i] = 999;
    }
    return 1;
}

/* Один стикер — одна запись: чужие счётчики не перезаписываются и старые
 * ключи (например, rose) в профиле не мешают. */
static int gift_put_one(const char *nick, int id, int value) {
    char url[URL], body[32];
    snprintf(url, sizeof(url), "%s/users/%s/gifts/%s.json", net.base, nick, gift_id(id));
    snprintf(body, sizeof(body), "%d", value);
    return fb_http("PUT", url, body, NULL, 0);
}

static int gift_recipient_exists(const char *nick) {
    char url[URL], resp[1024];
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    if (fb_http("GET", url, NULL, resp, sizeof(resp)) != 200)
        return 0;
    return resp[0] == '{';
}

static int gift_buy(const char *nick, int id) {
    char url[URL], body[BODY], resp[1024];
    int counts[GIFT_N], cups, code, price = gift_price(id);
    snprintf(url, sizeof(url), "%s/users/%s/cups.json", net.base, nick);
    if (fb_http("GET", url, NULL, resp, sizeof(resp)) != 200)
        return 3;
    cups = num_int(resp, "", 0);
    if (cups < price)
        return 5;
    if (!gift_get_counts(nick, counts))
        return 3;
    if (counts[id] >= 999)
        return 8;
    counts[id]++;
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    snprintf(body, sizeof(body), "{\"cups\":%d,\"gifts\":{\"%s\":%d}}", cups - price, gift_id(id), counts[id]);
    net_seal_body(body, sizeof(body));
    code = fb_http("PATCH", url, body, NULL, 0);
    if (code != 200) {
        if (net_log_ok())
            LOGERR("gift buy for '%s': HTTP %d", nick, code);
        return 3;
    }
    /* The purchase wrote the balance itself; the shop takes this exact number
     * instead of subtracting from a local copy that may be stale. */
    s_gift_cups = cups - price;
    return 2;
}

static int gift_sell(const char *nick, int id) {
    char url[URL], body[BODY], resp[1024];
    int counts[GIFT_N], cups, code;
    snprintf(url, sizeof(url), "%s/users/%s/cups.json", net.base, nick);
    if (fb_http("GET", url, NULL, resp, sizeof(resp)) != 200)
        return 3;
    cups = num_int(resp, "", 0);
    if (!gift_get_counts(nick, counts))
        return 3;
    if (counts[id] < 1)
        return 6;
    counts[id]--;
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    snprintf(body, sizeof(body), "{\"cups\":%d,\"gifts\":{\"%s\":%d}}", cups + gift_refund_for(id), gift_id(id),
             counts[id]);
    net_seal_body(body, sizeof(body));
    code = fb_http("PATCH", url, body, NULL, 0);
    if (code != 200) {
        if (net_log_ok())
            LOGERR("gift sell for '%s': HTTP %d", nick, code);
        return 3;
    }
    s_gift_cups = cups + gift_refund_for(id);
    return 2;
}

static int gift_send(const char *sender, const char *recipient, int id) {
    int mine[GIFT_N], theirs[GIFT_N];
    if (!gift_nick_ok(recipient) || gift_nick_same(sender, recipient))
        return 7;
    if (!gift_recipient_exists(recipient))
        return 4;
    if (!gift_get_counts(sender, mine))
        return 3;
    if (mine[id] < 1)
        return 6;
    if (!gift_get_counts(recipient, theirs))
        return 3;
    if (theirs[id] >= 999)
        return 8;
    mine[id]--;
    if (gift_put_one(sender, id, mine[id]) != 200) {
        if (net_log_ok())
            LOGERR("gift send: cannot take the sticker from '%s'", sender);
        return 3;
    }
    theirs[id]++;
    if (gift_put_one(recipient, id, theirs[id]) != 200) {
        /* Не смогли положить получателю — возвращаем стикер себе. */
        mine[id]++;
        gift_put_one(sender, id, mine[id]);
        if (net_log_ok())
            LOGERR("gift send: cannot deliver to '%s'", recipient);
        return 3;
    }
    return 2;
}

static int gift_grant(const char *recipient, int id) {
    int theirs[GIFT_N];
    if (!gift_nick_ok(recipient))
        return 4;
    if (!gift_recipient_exists(recipient))
        return 4;
    if (!gift_get_counts(recipient, theirs))
        return 3;
    if (theirs[id] >= 999)
        return 8;
    theirs[id]++;
    if (gift_put_one(recipient, id, theirs[id]) != 200) {
        if (net_log_ok())
            LOGERR("gift grant for '%s': write failed", recipient);
        return 3;
    }
    return 2;
}

static void *gift_action_job(void *arg) {
    char nick[24], sender[24], gift[16];
    int id, mode, result = 3;
    (void)arg;
    snprintf(nick, sizeof(nick), "%s", s_gift_recipient);
    lg_lock();
    snprintf(sender, sizeof(sender), "%s", lg.session_nick);
    lg_unlock();
    id = s_gift_id;
    mode = s_gift_grant;
    snprintf(gift, sizeof(gift), "%s", gift_id(id));
    if (!gift[0] || !sender[0] || !fb_enabled() || !fb_ensure_token())
        goto gift_done;
    if (mode == 1)
        result = gift_grant(nick, id);
    else if (mode == 2)
        result = gift_buy(sender, id);
    else if (mode == 3)
        result = gift_sell(sender, id);
    else
        result = gift_send(sender, nick, id);
gift_done:
    s_gift_status = result;
    return NULL;
}

void net_gift_action(double gift, const char *recipient, double grant) {
    DSThread t;
    int mode = (int)grant;
    if (s_gift_status == 1 || gift < 0 || gift > GIFT_N - 1 || (mode != 1 && mode != 2 && mode != 3 && mode != 0) || !recipient ||
        strlen(recipient) >= sizeof(s_gift_recipient) || (mode == 0 && !*recipient))
        return;
    snprintf(s_gift_recipient, sizeof(s_gift_recipient), "%s", recipient);
    s_gift_id = (int)gift;
    s_gift_grant = mode;
    s_gift_status = 1;
    t = ds_thread_start(gift_action_job, NULL);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    s_gift_status = 3;
}

double net_gift_action_status(void) {
    int s = s_gift_status;
    if (s >= 2)
        s_gift_status = 0;
    return (double)s;
}

/* Balance the database holds after the last successful buy/sell, or -1 when
 * the current save is not one. The shop never guesses this number locally. */
double net_gift_cups(void) {
    double v = (double)s_gift_cups;
    s_gift_cups = -1;
    return v;
}

/* How many sticker kinds the catalogue has (poop, cake, bear, rocket,
 * emerald). The shop and the collection both lay themselves out from this. */
double net_gift_types(void) { return (double)GIFT_N; }

double net_gift_price(double gift) {
    return (double)gift_price((int)gift);
}

double net_gift_refund(double gift) {
    return (double)gift_refund_for((int)gift);
}

static void *gift_counts_job(void *arg) {
    char nick[24], plain[URL], url[FB_URL_MAX], resp[8192];
    (void)arg;
    lg_lock();
    snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    lg_unlock();
    if (nick[0] && fb_enabled() && fb_ensure_token()) {
        snprintf(plain, sizeof(plain), "%s/users/%s/gifts.json", net.base, nick);
        fb_url(url, sizeof(url), plain);
        if (http("GET", url, NULL, resp, sizeof(resp)) == 200) {
            s_gift_counts[0] = num_int(resp, "poop", 0);
            s_gift_counts[1] = num_int(resp, "cake", 0);
            s_gift_counts[2] = num_int(resp, "bear", 0);
            s_gift_counts[3] = num_int(resp, "rocket", 0);
            s_gift_counts[4] = num_int(resp, "emerald", 0);
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
        DSThread t;
        s_gift_counts_requested = 1;
        t = ds_thread_start(gift_counts_job, NULL);
        if (t)
            ds_thread_detach(t);
        else
            s_gift_counts_loaded = 1;
    }
    int i = (int)gift;
    return i >= 0 && i < GIFT_N ? s_gift_counts[i] : 0;
}
