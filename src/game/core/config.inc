static double admin_nick_equal(const char *a, const char *b) {
    if (!a || !b) {
        return 0;
    }
    int i = 0;
    while (a[i] && b[i]) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') {
            ca = ca + ('a' - 'A');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = cb + ('a' - 'A');
        }
        if (ca != cb) {
            return 0;
        }
        i = i + 1;
    }
    return a[i] == b[i];
}

static double admin_slot(const char *nick) {
    if (admin_nick_equal(nick, ADMIN_NICK) == 1) {
        return 1;
    }
    if (admin_nick_equal(nick, ADMIN2_NICK) == 1) {
        return 2;
    }
    return 0;
}

/* Админ по нику вошедшего аккаунта. Кубки такого игрока игра не тратит:
 * классы и уровни бесплатны, поражение ничего не отнимает, а стикеры в
 * магазине покупаются без списания (движок gifts.inc держит тот же список). */
static double admin_playing(void) {
    if (net_login_status() != 2) {
        return 0;
    }
    return admin_slot(net_login_nick()) != 0 ? 1 : 0;
}

static const char *admin_nick_of(double slot) {
    if (slot == 2) {
        return ADMIN2_NICK;
    }
    return ADMIN_NICK;
}

static double admin_color_of(const char *nick, double base) {
    if (admin_nick_equal(nick, ADMIN_NICK) == 1) {
        return admin_color;
    }
    if (admin_nick_equal(nick, ADMIN2_NICK) == 1) {
        return admin2_color;
    }
    return base;
}

static double admin_head_color_of(const char *nick, double base) {
    if (admin_nick_equal(nick, ADMIN_NICK) == 1) {
        return admin_head_color;
    }
    if (admin_nick_equal(nick, ADMIN2_NICK) == 1) {
        return admin2_head_color;
    }
    return base;
}
