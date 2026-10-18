#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/game.c"

/* Test-only views over the game rules. The game itself no longer needs these
 * shortcuts, so they live here instead of in the shipped code. */
static double astra_grab_total_fraction(void) { return astra_hit_fraction * astra_hit_count + astra_final_fraction; }

static const char *fighter_sprite(double cls, double punching, double skin) {
    double look = class_look(cls, skin);
    if (look_ready(look) != 1) {
        return "";
    }
    return look_tex(look, punching == 1);
}

static double class_hp_of(double cls) { return astra_ability_max_hp_for(cls, astra_ability_of(cls, class_skin_of(cls))); }

static double class_power_of(double cls) {
    return astra_ability_punch_damage_for(cls, class_level_of(cls), astra_ability_of(cls, class_skin_of(cls)));
}

static double class_visible_index(double cls) {
    double n = 0;
    double i = 0;
    while (i < class_count) {
        double c = class_at_order(i);
        if (class_visible(c) == 1) {
            if (c == cls) {
                return n;
            }
            n = n + 1;
        }
        i = i + 1;
    }
    return -1;
}

int screen_w = 320;
int screen_h = 180;
double dt = 1.0 / 60.0;
int mouse_clicked;
double ds_mouse_x;
double ds_mouse_y;
Joy joy;

/* --- admin panel test instrumentation -------------------------------- */
#define ADM_COLOR_MAX 1024
static int adm_color_n;
static uint32_t adm_colors[ADM_COLOR_MAX];
static int adm_text_n;
static char adm_text[64][80];
static int adm_rr_n;
static double adm_rr_x, adm_rr_y, adm_rr_w, adm_rr_h;
static double adm_last_circle_r;
static double adm_last_circle_x, adm_last_circle_y;
static void adm_record_color(uint32_t c) {
    if (adm_color_n < ADM_COLOR_MAX)
        adm_colors[adm_color_n++] = c;
}
static void adm_record_text(const char *s) {
    if (adm_text_n < 64 && s)
        snprintf(adm_text[adm_text_n++], sizeof(adm_text[0]), "%s", s);
}
#define ADM_RR_COLOR_MAX 512
static int adm_rr_color_n;
static uint32_t adm_rr_colors[ADM_RR_COLOR_MAX];
static void adm_record_rr_color(uint32_t c) {
    if (adm_rr_color_n < ADM_RR_COLOR_MAX)
        adm_rr_colors[adm_rr_color_n++] = c;
}
static void adm_reset_log(void) {
    adm_color_n = 0;
    adm_text_n = 0;
    adm_rr_n = 0;
    adm_rr_color_n = 0;
}
static int adm_has_text(const char *needle) {
    for (int i = 0; i < adm_text_n; i++)
        if (strstr(adm_text[i], needle))
            return 1;
    return 0;
}
/* The panel paints no half-transparent shapes: fills and outlines are either
 * fully opaque (alpha 0xFF) or the plain no-alpha colour the game uses. */
static int adm_all_flat(void) {
    for (int i = 0; i < adm_rr_color_n; i++) {
        uint32_t a = adm_rr_colors[i] >> 24;
        if (a != 0 && a != 0xFF)
            return 0;
    }
    return 1;
}
/* Enabled / selected buttons may only turn grey: nothing in the panel may be
 * painted with a white fill, and the fill count says how many buttons are on. */
static int adm_no_white_fill(void) {
    for (int i = 0; i < adm_rr_color_n; i++)
        if ((adm_rr_colors[i] & 0xFFFFFF) == 0xFFFFFF)
            return 0;
    return 1;
}
static int adm_fill_count(uint32_t c) {
    int n = 0;
    for (int i = 0; i < adm_rr_color_n; i++)
        if ((adm_rr_colors[i] & 0xFFFFFF) == c)
            n++;
    return n;
}
static int adm_all_greyscale(void) {
    for (int i = 0; i < adm_color_n; i++) {
        uint32_t c = adm_colors[i];
        if (((c >> 16) & 255) != ((c >> 8) & 255) || ((c >> 8) & 255) != (c & 255))
            return 0;
    }
    return 1;
}

/* The Players tab reads the ban list through these: a tiny in-memory list. */
static char stub_bans[8][24];
static int stub_ban_n;
static char stub_nicks[4][24];
static char stub_last_chat[128];
static uint32_t stub_circle_color;

static int ground_texture_missing;
static int ground_tiles;
static int ground_fills;
static uint32_t last_ground_fill;
static const char *last_ground;
static int lobby_playing;
static int winter_playing;
static int winter_loop_calls;
static int snowflake_draws;
static int snowflake_tinted;
static uint32_t snowflake_tint_color;
static double snowflake_centres[64];
static int snowflake_centre_n;
static int star_line_count;
static double star_max_y;

struct DSArray {
    double *data;
    size_t length;
    size_t capacity;
};

DSArray *arr_new(void) {
    DSArray *array = calloc(1, sizeof(*array));
    assert(array);
    return array;
}

void arr_push(DSArray *array, double value) {
    if (array->length == array->capacity) {
        size_t capacity = array->capacity ? array->capacity * 2 : 8;
        double *data = realloc(array->data, capacity * sizeof(*data));
        assert(data);
        array->data = data;
        array->capacity = capacity;
    }
    array->data[array->length++] = value;
}

double arr_get(DSArray *array, double index) {
    size_t position = (size_t)index;
    return array && position < array->length ? array->data[position] : 0;
}

void arr_set(DSArray *array, double index, double value) {
    size_t position = (size_t)index;
    while (array->length <= position)
        arr_push(array, 0);
    array->data[position] = value;
}

double arr_len(DSArray *array) {
    return array ? (double)array->length : 0;
}

double clamp(double value, double low, double high) {
    return value < low ? low : value > high ? high : value;
}

void arr_clear(DSArray *array) {
    if (array)
        array->length = 0;
}

void arr_free(DSArray *array) {
    if (!array)
        return;
    free(array->data);
    free(array);
}

void ds_runtime_error(const char *format, ...) {
    (void)format;
    abort();
}

void ds_log(const char *format, ...) {
    (void)format;
}
void ds_gfx_gray(double on) {
    (void)on;
}
void ds_gfx_shake(double amp, double seconds) {
    (void)amp;
    (void)seconds;
}
double dist(double x, double y, double a, double b) {
    return hypot(x - a, y - b);
}
double ds_mod(double value, double divisor) {
    return (double)((int)value % (int)divisor);
}
int str_eq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0 ? 1 : 0;
}
double str_len(const char *s) {
    return s ? (double)strlen(s) : 0;
}
/* Склейка строк, как в рантайме: два чередующихся буфера, чтобы вызов
 * ds_concat(ds_concat(a, b), c) не затирал сам себя. */
char *ds_concat(const char *left, const char *right) {
    static char buffers[4][512];
    static int turn = 0;
    turn = (turn + 1) & 3;
    snprintf(buffers[turn], sizeof(buffers[turn]), "%s%s", left ? left : "", right ? right : "");
    return buffers[turn];
}
/* Keyboard stub with a real buffer: the admin nick field is driven through the
 * same calls the platform keyboard uses. */
static char stub_kb[128];
static int stub_kb_visible;
void keyboard_hide(void) { stub_kb_visible = 0; }
void keyboard_clear(void) { stub_kb[0] = 0; }
void keyboard_show(void) { stub_kb_visible = 1; }
int keyboard_visible(void) { return stub_kb_visible; }
void keyboard_get_text_to(char *dst, size_t cap) {
    if (dst && cap)
        snprintf(dst, cap, "%s", stub_kb);
}
void keyboard_type(const char *text) { snprintf(stub_kb, sizeof(stub_kb), "%s", text ? text : ""); }
void keyboard_backspace(void) {
    size_t len = strlen(stub_kb);
    if (len > 0)
        stub_kb[len - 1] = 0;
}
int keyboard_enter_pressed(void) { return 0; }
int keyboard_uses_editor(void) { return 0; }
int keyboard_handle_key(int keycode, int action, int meta) {
    (void)keycode;
    (void)action;
    (void)meta;
    return 0;
}
static void stub_kb_set(const char *text) { snprintf(stub_kb, sizeof(stub_kb), "%s", text ? text : ""); }
static double stub_net_slot = -1;
static double stub_online[4];
static double stub_chat_count;
static char stub_chat_keys[4][28];
static char stub_chat_uids[4][24];
static char stub_chat_texts[4][64];
static double stub_level[4];
static double stub_grab[4];
static double stub_gx[4];
static double stub_gy[4];
static double stub_gdx[4];
static double stub_gdy[4];
double net_slot(void) {
    return stub_net_slot;
}
double net_player_online(double slot) {
    return stub_online[(int)slot];
}
double net_player_level(double slot) {
    return stub_level[(int)slot];
}
double net_player_grab(double slot) {
    return stub_grab[(int)slot];
}
double net_player_grab_x(double slot) {
    return stub_gx[(int)slot];
}
double net_player_grab_y(double slot) {
    return stub_gy[(int)slot];
}
double net_player_grab_dx(double slot) {
    return stub_gdx[(int)slot];
}
double net_player_grab_dy(double slot) {
    return stub_gdy[(int)slot];
}
void net_mark_achievement_flag(double flag) {
    (void)flag;
}
void net_save_azum_revives(double value) {
    (void)value;
}
double stub_bpx = 0;
double net_load_bp_level(void) {
    return bp_claimed;
}
void net_set_bp_xp(double v) {
    if (v > stub_bpx)
        stub_bpx = v;
}
double net_load_bp_xp(void) {
    return stub_bpx;
}
void net_set_class(double value) {
    (void)value;
}
void net_set_level(double value) {
    (void)value;
}
void net_set_skin(double value) {
    (void)value;
}
void net_save_astra(double owned, double level, double unlocked) {
    (void)owned;
    (void)level;
    (void)unlocked;
}
static double stub_login_status;
static char stub_login_nick[24];
static char stub_login_pass[24];
const char *net_player_nick(double slot) {
    int i = (int)slot;
    return (i >= 0 && i < 4) ? stub_nicks[i] : "";
}
void net_ban_set(const char *nick, double banned) {
    int i, j = 0;
    if (!nick || !*nick)
        return;
    for (i = 0; i < stub_ban_n; i++) {
        if (strcmp(stub_bans[i], nick) == 0)
            continue;
        if (j != i)
            memcpy(stub_bans[j], stub_bans[i], sizeof(stub_bans[0]));
        j++;
    }
    stub_ban_n = j;
    if (banned >= 0.5 && stub_ban_n < 8) {
        memset(stub_bans[stub_ban_n], 0, sizeof(stub_bans[0]));
        snprintf(stub_bans[stub_ban_n], sizeof(stub_bans[0]), "%s", nick);
        stub_ban_n++;
    }
}
char *ds_num_to_string(double number) {
    static char buffers[4][24];
    static int turn = 0;
    turn = (turn + 1) & 3;
    snprintf(buffers[turn], sizeof(buffers[turn]), "%.0f", number);
    return buffers[turn];
}
void net_chat_send(const char *text) {
    snprintf(stub_last_chat, sizeof(stub_last_chat), "%s", text ? text : "");
}
double net_login_status(void) {
    return stub_login_status;
}
const char *net_login_nick(void) {
    return stub_login_nick;
}
const char *net_login_pass(void) {
    return stub_login_pass;
}
static char last_bye[256];
static int bye_calls;
void bye_notice_show(const char *text) {
    bye_calls++;
    snprintf(last_bye, sizeof(last_bye), "%s", text ? text : "");
}
static int fx_ring_count;
static int fx_big_circle_count;
void ring(float x, float y, float r, float t, uint32_t color) {
    fx_ring_count++;
    (void)x;
    (void)y;
    (void)r;
    (void)t;
    adm_record_color(color);
}
void tex_tint(float x, float y, const char *name, float angle, float scale, uint32_t color) {
    (void)y;
    (void)angle;
    (void)scale;
    ground_tiles++;
    last_ground = name;
    if (name && SNOWFLAKE_TEX && strcmp(name, SNOWFLAKE_TEX) == 0) {
        snowflake_tinted++;
        snowflake_tint_color = color;
        snowflake_draws++;
        if (snowflake_centre_n < 64) {
            snowflake_centres[snowflake_centre_n++] = x + 26.0 * scale;
        }
    }
}
void net_save_progress_all(double a, double b, double c, double d, double e, double f, double g, double h, double i,
                           double j, double k, double l, double m, double n, double o, double p, double q, double r) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)i;
    (void)j;
    (void)k;
    (void)l;
    (void)m;
    (void)n;
    (void)o;
    (void)p;
    (void)q;
    (void)r;
}

int snd_load(const char *name) {
    return name && *name;
}
int snd_play(const char *name) {
    (void)name;
    return 1;
}
int snd_loop(const char *name) {
    if (name && LOBBY_MUSIC && strcmp(name, LOBBY_MUSIC) == 0) {
        lobby_playing = 1;
    }
    if (name && WINTER_JINGLE && strcmp(name, WINTER_JINGLE) == 0) {
        winter_playing = 1;
        winter_loop_calls++;
    }
    return 1;
}
int snd_playing(const char *name) {
    if (name && LOBBY_MUSIC && strcmp(name, LOBBY_MUSIC) == 0)
        return lobby_playing;
    if (name && WINTER_JINGLE && strcmp(name, WINTER_JINGLE) == 0)
        return winter_playing;
    return 0;
}
void snd_stop(const char *name) {
    if (name && LOBBY_MUSIC && strcmp(name, LOBBY_MUSIC) == 0)
        lobby_playing = 0;
    if (name && WINTER_JINGLE && strcmp(name, WINTER_JINGLE) == 0)
        winter_playing = 0;
}
void snd_volume(const char *name, double volume) {
    (void)name;
    (void)volume;
}

int tex_ready(const char *name) {
    return name && *name && !ground_texture_missing;
}

void rect(float x, float y, float width, float height, uint32_t color) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    last_ground_fill = color;
    ground_fills++;
    adm_record_color(color);
}

/* Added for the admin panel: the game only needs these on screen, the host
 * test just records what was drawn. */
void roundrect(float x, float y, float w, float h, float rad, uint32_t color) {
    (void)rad;
    adm_record_color(color);
    adm_record_rr_color(color);
    if (adm_rr_n == 0) {
        adm_rr_x = x;
        adm_rr_y = y;
        adm_rr_w = w;
        adm_rr_h = h;
    }
    adm_rr_n++;
}

void text_scaled(const char *s, float x, float y, uint32_t c, float scale) {
    (void)x;
    (void)y;
    (void)scale;
    adm_record_text(s);
    adm_record_color(c);
}

int text_ink_width(const char *s) {
    return s ? (int)(9 * strlen(s)) : 0;
}

int text_ink_height(const char *s) {
    (void)s;
    return 40;
}

int text_ink_top(const char *s) {
    (void)s;
    return -34;
}

void line(float x1, float y1, float x2, float y2, float thickness, uint32_t color) {
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)thickness;
    star_line_count++;
    if (y1 > star_max_y) star_max_y = y1;
    if (y2 > star_max_y) star_max_y = y2;
    adm_record_color(color);
}

static int candy_draws;
static double candy_alpha_drawn;
static double candy_scale_drawn;
static double candy_x_drawn;
static double candy_y_drawn;
static uint32_t candy_circle_color;
void circle(float x, float y, float r, uint32_t color) {
    if (r > 20) fx_big_circle_count++;
    candy_circle_color = color;
    candy_x_drawn = x;
    candy_y_drawn = y;
    stub_circle_color = color;
    adm_last_circle_r = r;
    adm_last_circle_x = x;
    adm_last_circle_y = y;
    adm_record_color(color);
}
/* The gradient buttons live in C++ (src/game/cpp/ui_fx.cpp); the state test
 * never draws them, so a stub keeps the host link self-contained. */
void cb_button_gradient(float x, float y, float w, float h, float rad, uint32_t color) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)rad;
    (void)color;
}
void tex_alpha(float x, float y, const char *name, float angle, float scale, double alpha) {
    (void)angle;
    if (name && CANDY_TEX && strcmp(name, CANDY_TEX) == 0) {
        candy_draws++;
        candy_scale_drawn = scale;
        candy_alpha_drawn = alpha;
        /* tex_alpha() получает левый верхний угол, центр на 25*scale правее и ниже. */
        candy_x_drawn = x + 25.0 * scale;
        candy_y_drawn = y + 25.0 * scale;
    }
}

void tex(float x, float y, const char *name, float angle, float scale) {
    (void)x;
    (void)y;
    (void)angle;
    (void)scale;
    ground_tiles++;
    last_ground = name;
    if (name && SNOWFLAKE_TEX && strcmp(name, SNOWFLAKE_TEX) == 0) {
        snowflake_draws++;
        if (snowflake_centre_n < 64) {
            /* tex() receives the top-left corner; the game sets hw = 26*scale, so the centre is 26*scale further right. */
            snowflake_centres[snowflake_centre_n++] = x + 26.0 * scale;
        }
    }
}

/* --- Engine API pulled in by the full touch dispatch: the release-tap test
 * reaches logic_touch -> touch_menu -> login -> progress_from_storage and
 * friends. These paths are not exercised here; the stubs only satisfy the
 * linker (with --gc-sections these used to be stripped as dead code). ----- */
double net_auth(const char *url, const char *nick, const char *pass) {
    (void)url;
    (void)nick;
    (void)pass;
    return 2;
}
static double stub_azum_buff;
double net_azum_buff(void) { return stub_azum_buff; }
static double stub_flash_sale;
static int stub_live_refreshes;
double net_flash_sale(void) { return stub_flash_sale; }
void net_live_flags_refresh(void) { stub_live_refreshes++; }
double net_event(void) { return 0; }
void net_event_set(double mode) { (void)mode; }
double net_load_cups(void) { return 0; }
double net_load_candies(void) { return 0; }
double net_load_class(void) { return 0; }
double net_load_azum(void) { return 0; }
double net_load_santa(void) { return 0; }
double net_load_ebuc(void) { return 0; }
double net_load_level(void) { return 0; }
double net_load_levels_unlocked(void) { return 0; }
double net_load_ordinary_level(void) { return 0; }
double net_load_ordinary_levels_unlocked(void) { return 0; }
double net_load_azum_level(void) { return 0; }
double net_load_azum_levels_unlocked(void) { return 0; }
double net_load_santa_level(void) { return 0; }
double net_load_santa_levels_unlocked(void) { return 0; }
double net_load_ebuc_level(void) { return 0; }
double net_load_ebuc_levels_unlocked(void) { return 0; }
double net_load_azum_skin(void) { return 0; }
double net_load_astra(void) { return 0; }
double net_load_astra_level(void) { return 0; }
double net_load_astra_levels_unlocked(void) { return 0; }
double net_load_achievement_flags(void) { return 0; }
double net_load_azum_revives(void) { return 0; }
void net_connect(const char *url, const char *room) { (void)url; (void)room; }
void net_disconnect(void) {}
static int stub_promo2027_used;
double net_promo_2027_used(void) { return stub_promo2027_used; }
void net_promo_2027_mark_used(void) { stub_promo2027_used = 1; }
void net_leaderboard_fetch(const char *url) { (void)url; }
double net_load_language(void) { return 0; }
double net_load_hitboxes(void) { return 0; }
void net_save_settings(double language, double hitboxes) {
    (void)language;
    (void)hitboxes;
}
double net_load_music_volume(void) { return 0; }
void net_save_music_volume(double volume) { (void)volume; }
double net_load_winter_theme(void) { return 0; }
void net_save_winter_theme(double on) { (void)on; }
double net_load_new_theme(void) { return 1; }
void net_save_new_theme(double on) { (void)on; }
void settings_mark_legal(void) {}
/* The game-state harness does not link the runtime pool tick; the promo
 * test only needs the calls to exist so it can prove the message text is
 * an owned copy, not a pooled pointer. */
void ds_string_pool_tick(void) {}
static double stub_legal_ts;
double settings_legal_ts(void) { return stub_legal_ts; }
/* Согласие на обработку ПД и удаление аккаунта: по умолчанию согласие дано,
 * чтобы старые проверки лобби не упирались в окно вопроса. */
static double stub_pd_ver = LEGAL_DOCS_VERSION;
static int stub_pd_uploads;
static int stub_delete_calls;
static double stub_delete_status;
static int stub_logout_calls;
void settings_set_pd_consent(double version) { stub_pd_ver = version; }
double settings_pd_consent_version(void) { return stub_pd_ver; }
void net_pd_consent_upload(void) { stub_pd_uploads++; }
static char stub_delete_pass[64];
static int stub_pass_calls;
static double stub_pass_status;
static char stub_pass_old[64], stub_pass_new[64];
void net_delete_account(const char *password) {
    stub_delete_calls++;
    snprintf(stub_delete_pass, sizeof(stub_delete_pass), "%s", password ? password : "");
    stub_delete_status = 1;
}
void net_change_password(const char *old_pass, const char *new_pass) {
    stub_pass_calls++;
    snprintf(stub_pass_old, sizeof(stub_pass_old), "%s", old_pass ? old_pass : "");
    snprintf(stub_pass_new, sizeof(stub_pass_new), "%s", new_pass ? new_pass : "");
    stub_pass_status = 1;
}
double net_change_password_status(void) {
    double v = stub_pass_status;
    if (v >= 2) {
        stub_pass_status = 0;
    }
    return v;
}
double net_delete_account_status(void) {
    double v = stub_delete_status;
    if (v >= 2) {
        stub_delete_status = 0;
    }
    return v;
}
void net_logout(void) {
    stub_logout_calls++;
    stub_login_status = 0;
}
double net_is_banned(const char *nick) {
    (void)nick;
    return 0;
}
double net_chat_is_ban(const char *msg) {
    (void)msg;
    return 0;
}
double net_chat_is_unban(const char *msg) {
    (void)msg;
    return 0;
}
const char *net_chat_ban_target(const char *msg) {
    (void)msg;
    return "";
}
const char *net_chat_unban_target(const char *msg) {
    (void)msg;
    return "";
}
static int stub_admin_reset_calls;
static char stub_admin_reset_nick[24], stub_admin_reset_new[64];
static double stub_admin_reset_status;
void net_admin_reset_password(const char *nick, const char *new_pass) {
    stub_admin_reset_calls++;
    snprintf(stub_admin_reset_nick, sizeof(stub_admin_reset_nick), "%s", nick ? nick : "");
    snprintf(stub_admin_reset_new, sizeof(stub_admin_reset_new), "%s", new_pass ? new_pass : "");
    stub_admin_reset_status = 1;
}
double net_admin_reset_status(void) {
    double v = stub_admin_reset_status;
    if (v >= 2) stub_admin_reset_status = 0;
    return v;
}
double net_gift_count(double gift) { (void)gift; return 0; }
void net_gift_refresh(void) {}
void net_gift_action(double gift, const char *recipient, double grant) {
    (void)gift; (void)recipient; (void)grant;
}
double net_gift_action_status(void) { return 0; }
double net_chat_is_passwd(const char *msg) {
    if (!msg || strncmp(msg, "passwd ", 7) != 0) return 0;
    const char *t = msg + 7;
    while (*t == ' ' || *t == '\t') t++;
    if (!*t) return 0;
    while (*t && *t != ' ' && *t != '\t') t++;
    while (*t == ' ' || *t == '\t') t++;
    return *t ? 1 : 0;
}
static char stub_passwd_target[24], stub_passwd_new[64];
const char *net_chat_passwd_target(const char *msg) {
    const char *t = msg ? msg + 7 : "";
    size_t i = 0;
    while (*t == ' ' || *t == '\t') t++;
    while (t[i] && i < sizeof(stub_passwd_target) - 1 && t[i] != ' ' && t[i] != '\t') { stub_passwd_target[i] = t[i]; i++; }
    stub_passwd_target[i] = 0;
    return stub_passwd_target;
}
const char *net_chat_passwd_new(const char *msg) {
    const char *t = msg ? msg + 7 : "";
    size_t i = 0;
    while (*t == ' ' || *t == '\t') t++;
    while (*t && *t != ' ' && *t != '\t') t++;
    while (*t == ' ' || *t == '\t') t++;
    while (t[i] && i < sizeof(stub_passwd_new) - 1 && t[i] != ' ' && t[i] != '\t') { stub_passwd_new[i] = t[i]; i++; }
    stub_passwd_new[i] = 0;
    return stub_passwd_new;
}
double net_chat_is_text_cmd(const char *msg) {
    (void)msg;
    return 0;
}
const char *net_chat_text_cmd_text(const char *msg) {
    (void)msg;
    return "";
}
const char *net_chat_text_cmd_color(const char *msg) {
    (void)msg;
    return "";
}
void net_banner_send(const char *text, const char *color) {
    (void)text;
    (void)color;
}
void net_chat_trim(double keep) { (void)keep; }
int png_load(const char *name) {
    (void)name;
    return 0;
}
double str_index_of(const char *hay, const char *needle) {
    (void)hay;
    (void)needle;
    return -1;
}
const char *str_sub(const char *s, double start, double len) {
    (void)s;
    (void)start;
    (void)len;
    return "";
}
/* Faithful little versions (the runtime ones are not linked into this
 * harness): trim + uppercase over rotating static buffers. */
const char *str_trim(const char *s) {
    static char bufs[4][128];
    static int slot;
    if (s == NULL) s = "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' || s[n - 1] == '\r')) n--;
    if (n > 127) n = 127;
    char *out = bufs[slot];
    slot = (slot + 1) % 4;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}
const char *str_upper(const char *s) {
    static char bufs[4][128];
    static int slot;
    if (s == NULL) s = "";
    char *out = bufs[slot];
    slot = (slot + 1) % 4;
    size_t i = 0;
    while (s[i] != '\0' && i < 127) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[i] = c;
        i++;
    }
    out[i] = '\0';
    return out;
}

static void assert_clean(const char *in, const char *want) {
    char out[CF_OUT_MAX];
    chat_clean_into(out, sizeof(out), in);
    if (strcmp(out, want) != 0) {
        fprintf(stderr, "chat filter: '%s' -> '%s', want '%s'\n", in, out, want);
        assert(0);
    }
}

/* Свайп ползунка удаления пальцем id: от ползунка на долю frac дорожки. */
static void swipe_delete_knob(double frac, double id) {
    double kx = acct_knob_x() + acct_knob_w() / 2;
    double ky = acct_slider_y() + acct_slider_h() / 2;
    double span = acct_w() - acct_knob_w() - 8;
    logic_touch(kx, ky, 0, id);
    press_new = 0;
    logic_touch(kx + span * frac * 0.5, ky, 2, id);
    logic_touch(kx + span * frac, ky, 2, id);
    logic_touch(kx + span * frac, ky, 1, id);
}

/* Удаление: без пароля и без свайпа до конца ничего не уходит на сервер. */
static void test_account_delete(void) {
    int calls = stub_delete_calls;
    /* Свайп без пароля: просьба ввести пароль, ползунок уезжает назад. */
    swipe_delete_knob(1.2, 60);
    assert(stub_delete_calls == calls && str_eq(acct_msg, tr_acct_need_pass()) == 1);
    /* Пароль введён, но ползунок отпущен на полпути - не удаляем. */
    logic_touch(acct_x() + acct_w() / 2, acct_field_y(0) + login_fh / 2, 0, 61);
    press_new = 0;
    assert(keyboard_visible() == 1 && acct_field == 0);
    keyboard_type("secret1");
    acct_tick();
    assert(strcmp(acct_buf[0], "secret1") == 0 && strcmp(acct_mask(0), "*******") == 0);
    acct_knob = 0;
    swipe_delete_knob(0.5, 62);
    assert(stub_delete_calls == calls && acct_knob > 0.3);
    double k = acct_knob;
    dt = 0.05;
    acct_tick();
    assert(acct_knob < k); /* springs back */
    acct_knob = 0;
    /* Обычное касание (без свайпа) по ползунку тоже ничего не делает. */
    logic_touch(acct_knob_x() + acct_knob_w() / 2, acct_slider_y() + acct_slider_h() / 2, 0, 63);
    press_new = 0;
    logic_touch(acct_knob_x() + acct_knob_w() / 2, acct_slider_y() + acct_slider_h() / 2, 1, 63);
    assert(stub_delete_calls == calls);
    /* Свайп до конца с паролем: удаление с этим паролем, поле очищено. */
    swipe_delete_knob(1.2, 64);
    assert(stub_delete_calls == calls + 1 && strcmp(stub_delete_pass, "secret1") == 0);
    assert(acct_busy == 1 && acct_buf[0][0] == 0);
    /* Неверный пароль: сообщение, можно повторить. */
    stub_delete_status = 4;
    acct_tick();
    assert(acct_busy == 0 && acct_done == 0 && str_eq(acct_msg, tr_acct_wrong()) == 1);
    acct_buf[0][0] = 's';
    acct_buf[0][1] = 0;
    acct_knob = 0;
    swipe_delete_knob(1.2, 65);
    assert(stub_delete_calls == calls + 2);
    stub_delete_status = 2;
    acct_tick();
    assert(acct_busy == 0 && acct_done == 1 && str_eq(acct_msg, tr_delete_done()) == 1);
}

/* Advance only the transition clock, without polling login or gameplay. */
static void finish_transition(void) {
    double saved_dt = dt;
    dt = 0.05;
    for (int step = 0; step < 100 && t_dir != 0; step++) update_transition();
    dt = saved_dt;
    assert(t_dir == 0 && t_fade == 0 && t_action == NULL);
}

/* Экран аккаунта из лобби: меню, смена пароля, выход. */
static void test_account_screen(void) {
    char saved_nick[24];
    snprintf(saved_nick, sizeof(saved_nick), "%s", stub_login_nick);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    stub_login_status = 2;
    game_state = ST_LOBBY;
    t_dir = 0;
    docs_open = 0;
    acct_open = 0;
    logic_touch(menu_x() + menu_btn_w() / 2, menu_row(3) + btn_h / 2, 0, 70);
    press_new = 0;
    /* The account opens through the screen transition, like every other
     * screen: nothing yet, the account at the end of the fade. */
    assert(acct_open == 0 && t_dir == 1 && acct_pending == 1);
    {
        double saved_dt = dt;
        dt = 0.1;
        for (int step = 0; step < 60 && acct_open == 0; step++) update_transition();
        dt = saved_dt;
    }
    assert(acct_open == ACCT_MENU);
    t_dir = 0;
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(0) + btn_h / 2, 0, 71);
    press_new = 0;
    assert(acct_open == ACCT_MENU && t_dir == 1);
    finish_transition();
    assert(acct_open == ACCT_PASS);
    /* Проверки до сервера: пустые поля, несовпадение, тот же пароль. */
    logic_touch(acct_x() + acct_w() / 2, acct_below_fields() + btn_h / 2, 0, 72);
    press_new = 0;
    assert(str_eq(acct_msg, tr_acct_need_pass()) == 1 && stub_pass_calls == 0);
    snprintf(acct_buf[0], ACCT_BUF, "%s", "old1");
    snprintf(acct_buf[1], ACCT_BUF, "%s", "new1");
    snprintf(acct_buf[2], ACCT_BUF, "%s", "new2");
    acct_pass_try();
    assert(str_eq(acct_msg, tr_acct_mismatch()) == 1 && stub_pass_calls == 0);
    snprintf(acct_buf[1], ACCT_BUF, "%s", "old1");
    snprintf(acct_buf[2], ACCT_BUF, "%s", "old1");
    acct_pass_try();
    assert(str_eq(acct_msg, tr_acct_same()) == 1 && stub_pass_calls == 0);
    /* Пароль админа задан в коде: его смена отняла бы права админа. */
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(acct_buf[1], ACCT_BUF, "%s", "new1");
    snprintf(acct_buf[2], ACCT_BUF, "%s", "new1");
    acct_pass_try();
    assert(str_eq(acct_msg, tr_acct_admin()) == 1 && stub_pass_calls == 0);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    /* Ввод через клавиатуру: Enter переходит к следующему полю. */
    logic_touch(acct_x() + acct_w() / 2, acct_field_y(1) + login_fh / 2, 0, 73);
    press_new = 0;
    assert(acct_field == 1 && keyboard_visible() == 1);
    keyboard_type("new1");
    acct_tick();
    logic_touch(acct_x() + acct_w() / 2, acct_field_y(2) + login_fh / 2, 0, 74);
    press_new = 0;
    keyboard_type("new1");
    acct_tick();
    assert(strcmp(acct_buf[1], "new1") == 0 && strcmp(acct_buf[2], "new1") == 0);
    logic_touch(acct_x() + acct_w() / 2, acct_below_fields() + btn_h / 2, 0, 75);
    press_new = 0;
    assert(stub_pass_calls == 1 && strcmp(stub_pass_old, "old1") == 0 && strcmp(stub_pass_new, "new1") == 0);
    assert(acct_busy == 2 && acct_buf[0][0] == 0 && acct_buf[1][0] == 0);
    stub_pass_status = 4;
    acct_tick();
    assert(acct_busy == 0 && str_eq(acct_msg, tr_acct_wrong()) == 1);
    acct_busy = 2;
    stub_pass_status = 2;
    acct_tick();
    assert(acct_busy == 0 && str_eq(acct_msg, tr_acct_changed()) == 1);
    /* «Назад» - в меню аккаунта, ещё раз - закрыть. */
    assert(logic_back() == 1 && t_dir == 1 && acct_open == ACCT_PASS);
    finish_transition();
    assert(acct_open == ACCT_MENU);
    /* Выход из аккаунта. */
    int logouts = stub_logout_calls;
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(2) + btn_h / 2, 0, 76);
    press_new = 0;
    assert(acct_open == ACCT_MENU && stub_logout_calls == logouts && t_dir == 1);
    finish_transition();
    assert(acct_open == 0 && stub_logout_calls == logouts + 1);
    /* Sign-out zeroes the local progress: cups, candies, classes, levels. */
    cups = 500;
    candies = 90;
    set_class_owned(CLASS_AZUM, 1);
    set_class_progress(CLASS_AZUM, 2, 3);
    player_class = CLASS_AZUM;
    progress_reset_on_signout();
    assert(cups == 0 && candies == 0);
    assert(class_owned_of(CLASS_AZUM) == 0 && player_class == CLASS_ORDINARY);
    assert(class_level_of(CLASS_AZUM) == 0 && class_unlocked_of(CLASS_AZUM) == 0);
    assert(achievement_mask == 0 && azum_revive_count == 0);
    /* Без входа «Аккаунт» ведёт на экран входа. */
    stub_login_status = 0;
    logic_touch(menu_x() + menu_btn_w() / 2, menu_row(3) + btn_h / 2, 0, 77);
    press_new = 0;
    assert(acct_open == 0 && t_target == ST_LOGIN);
    t_dir = 0;
    t_target = 0;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick);
    puts("account: menu, password change checks, sign-out, delete needs password + full swipe right");
}

/* Правовая часть: фильтр чата, согласие при входе, вопрос в лобби, окно
 * документов поверх предупреждения и удаление аккаунта. */
static void test_legal(void) {
    double saved_lang = language;
    double saved_w = screen_w, saved_h = screen_h;
    screen_w = 1280;
    screen_h = 720;
    language = 1;

    /* Мат, его латинские и «зашумлённые» записи и ссылки - звёздочки. */
    assert_clean("привет хуй", "привет ***");
    assert_clean("Пиздец!", "***");
    assert_clean("cyka blyat", "*** ***");
    assert_clean("huy", "***");
    assert_clean("х.у.й", "***");
    assert_clean("xyй", "***");
    assert_clean("заебал уже", "*** уже");
    assert_clean("fuck you", "*** you");
    assert_clean("заходи на site.ru", "заходи на ***");
    assert_clean("https://x.com/a", "***");
    assert_clean("t.me/chan", "***");
    /* Обычные слова, похожие на запрещённые, не трогаем. */
    assert_clean("себе хлеб небо тебе веб вебинар психуй застрахуй", "себе хлеб небо тебе веб вебинар психуй застрахуй");
    assert_clean("спидран мандарин сукно употреблять упорный оскорблять", "спидран мандарин сукно употреблять упорный оскорблять");
    assert_clean("ebuC web ebay hello pizza 12:00 1.5", "ebuC web ebay hello pizza 12:00 1.5");
    assert_clean("ebat", "***");
    assert(nick_blocked("huy228") == 1);
    assert(nick_blocked("Dimasi4ek229") == 0);
    assert(nick_blocked("ebuc_fan") == 0);
    assert(nick_blocked("tester") == 0);

    /* Вход: без галочки согласия нельзя, галочка заранее не стоит. */
    stub_pd_ver = 0;
    stub_login_status = 0;
    game_state = ST_LOGIN;
    t_dir = 0;
    warn_open = 0;
    studio_open = 0;
    login_open();
    assert(login_consent == 0);
    login_nick = "tester";
    login_pass = "secret1";
    login_do();
    assert(login_status == 8 && str_eq(login_error_text(), tr_consent_needed()) == 1);
    login_nick = "huy";
    login_do();
    assert(login_status == 9);
    login_nick = "tester";
    login_status = 8;
    logic_touch(login_consent_x() + 10, login_consent_y() + 10, 0, 40);
    press_new = 0;
    assert(login_consent == 1 && login_status == 0);
    int uploads = stub_pd_uploads;
    login_do();
    assert(login_status == 0 && stub_pd_ver == LEGAL_DOCS_VERSION && stub_pd_uploads == uploads + 1);
    t_dir = 0;
    t_target = 0;
    /* Ссылка под галочкой открывает документы, ник при этом не теряется. */
    logic_touch(center_x(login_more_w()) + login_more_w() / 2, login_more_y() + btn_h * 0.4, 0, 41);
    press_new = 0;
    assert(docs_open == 0 && t_dir == 1);
    finish_transition();
    assert(docs_open == 1 && docs_page == LEGAL_PAGE_CONSENT - 2 && str_eq(login_nick, "tester") == 1);
    docs_close();
    login_open();
    /* The checkbox resets on every entry, even after consent was given. */
    assert(login_consent == 0);

    /* Вошедшего до появления согласия спрашивают в лобби, один раз. */
    stub_pd_ver = 0;
    stub_login_status = 2;
    game_state = ST_LOBBY;
    docs_asked = 0;
    docs_tick();
    assert(docs_open == 1 && docs_ask == 1 && docs_page == LEGAL_PAGE_CONSENT);
    logic_touch(docs_mid_x() + docs_mid_w() / 2, docs_row_y() + btn_h / 2, 0, 42);
    press_new = 0;
    assert(docs_open == 1 && t_dir == 1);
    finish_transition();
    assert(docs_open == 0 && stub_pd_ver == LEGAL_DOCS_VERSION);
    docs_tick();
    assert(docs_open == 0);
    /* Отказ: игрок выходит из аккаунта. */
    stub_pd_ver = 0;
    docs_show(LEGAL_PAGE_CONSENT, 1);
    logic_touch(docs_mid_x() + docs_mid_w() / 2, docs_refuse_y() + btn_h / 2, 0, 43);
    press_new = 0;
    finish_transition();
    assert(docs_open == 0 && stub_logout_calls == 1 && stub_login_status == 0);
    /* Онлайн без согласия не открывается - вместо него вопрос. */
    stub_login_status = 2;
    open_online();
    assert(docs_open == 0 && t_dir == 1);
    finish_transition();
    assert(docs_open == 1 && docs_ask == 1 && t_dir == 0);
    docs_close();
    stub_pd_ver = LEGAL_DOCS_VERSION;

    /* Листание; кнопка удаления открывает экран аккаунта поверх документов. */
    docs_show(0, 0);
    assert(hit_docs_prev(docs_prev_x() + docs_nav_w() / 2, docs_row_y() + btn_h / 2) == 0);
    logic_touch(docs_next_x() + docs_nav_w() / 2, docs_row_y() + btn_h / 2, 0, 44);
    press_new = 0;
    assert(docs_page == 0 && t_dir == 1);
    finish_transition();
    assert(docs_page == 1);
    docs_show(LEGAL_PAGE_RIGHTS, 0);
    assert(hit_docs_next(docs_next_x() + docs_nav_w() / 2, docs_row_y() + btn_h / 2) == 0);
    logic_touch(docs_mid_x() + docs_mid_w() / 2, docs_row_y() + btn_h / 2, 0, 45);
    press_new = 0;
    finish_transition();
    assert(acct_open == ACCT_DELETE && docs_open == 1 && stub_delete_calls == 0);
    test_account_delete();
    /* «Назад» закрывает экран аккаунта и оставляет документы. */
    assert(logic_back() == 1 && t_dir == 1 && acct_open == ACCT_DELETE);
    finish_transition();
    assert(acct_open == 0 && docs_open == 1);
    docs_close();
    /* Без входа кнопки удаления нет. */
    stub_login_status = 0;
    docs_show(LEGAL_PAGE_RIGHTS, 0);
    assert(docs_has_mid() == 0);
    docs_close();

    /* Предупреждение при запуске: «Документы» открывают окно поверх него. */
    warn_open = 1;
    warn_closing = 0;
    logic_touch(warn_docs_x() + warn_docs_w() / 2, warn_btn_y() + btn_h / 2, 0, 47);
    press_new = 0;
    assert(docs_open == 0 && t_dir == 1);
    finish_transition();
    assert(docs_open == 1 && warn_open == 1);
    assert(logic_back() == 1 && docs_open == 1);
    finish_transition();
    assert(docs_open == 0);
    warn_open = 0;

    test_account_screen();
    stub_login_status = 2;

    /* Описания персонажей - авторские фразы, не переводятся; HP остаётся. */
    language = 1;
    assert(str_eq(tr_class_ordinary_desc(), "I was the first") == 1);
    assert(strstr(tr_achievement_last_hp_desc(), "HP") != NULL);

    game_state = ST_LOBBY;
    language = saved_lang;
    screen_w = saved_w;
    screen_h = saved_h;
    puts("legal: chat filter, sign-in consent, lobby consent ask, documents window, account deletion");
}

/* Стартовый экран: предупреждение об эпилепсии только при первом заходе,
 * «нажми, чтобы начать» - при каждом, касание плавно уводит в лобби. */
static void test_start_gate(void) {
    game_reset();
    game_state = ST_LOBBY;
    dt = 0.1;

    /* First launch: no stored acceptance -> warning up, start screen armed. */
    stub_legal_ts = 0;
    start_gate_init();
    assert(warn_open == 1 && start_open == 1 && start_closing == 0);
    /* The warning owns touches while it is up: no tap-to-start yet. */
    logic_touch(screen_w / 2, screen_h / 2, 0, 71);
    assert(start_closing == 0 && start_open == 1);
    warn_open = 0;
    warn_a = 0;

    /* Returning player: the warning is shown on EVERY launch again (the
     * stored acceptance no longer skips it), start screen still up. */
    stub_legal_ts = 1700000000;
    start_gate_init();
    assert(warn_open == 1 && start_open == 1);
    warn_open = 0;
    warn_a = 0;

    /* A touch starts the fade but keeps the lobby covered until it ends. */
    logic_touch(screen_w / 2, screen_h / 2, 0, 72);
    assert(start_closing == 1 && start_open == 1 && game_state == ST_LOBBY);
    for (int i = 0; i < 40 && start_open == 1; i++) {
        update_start_screen();
    }
    assert(start_open == 0 && start_a == 0 && game_state == ST_LOBBY);

    /* The lobby has appeared but is not tappable yet: the settle lock
     * swallows the first taps, Play unlocks only after it runs out. */
    assert(lobby_lock_t > 0);
    logic_touch(menu_x() + menu_btn_w() / 2, menu_row(1) + btn_h / 2, 0, 73);
    assert(t_dir == 0);
    lobby_lock_t = 0;
    logic_touch(menu_x() + menu_btn_w() / 2, menu_row(1) + btn_h / 2, 0, 74);
    assert(t_dir == 1 && t_target == ST_CLASSES);
    t_dir = 0;
    t_target = 0;
    press_new = 0;
    stub_legal_ts = 0;
    puts("start gate: epilepsy warning and tap-to-start every launch, lobby unlocks after a settle");
}

/* Промокод 2027: сообщение об успехе - собственная копия текста, а не
 * указатель в строковый пул, поэтому после тиков пула оно не превращается
 * в мусор и знак «?». */
static void test_promo_message(void) {
    game_state = ST_PROMO;
    language = 0;
    stub_login_status = 2;
    stub_promo2027_used = 0;
    promo_focus = 0;
    promo_input = "2027";
    double cups_before = cups;
    promo_submit();
    assert(cups == cups_before + 27 && stub_promo2027_used == 1);
    assert(promo_msg_t > 0);
    for (int i = 0; i < 4; i++) {
        ds_string_pool_tick();
        (void)ds_concat("frame noise ", ds_num_to_string((double)i));
    }
    assert(str_eq(promo_msg_text, "+27 cups!") == 1);
    /* Second submit says "already redeemed" and also survives the pool. */
    promo_submit();
    for (int i = 0; i < 4; i++)
        ds_string_pool_tick();
    assert(str_eq(promo_msg_text, "Promo code already redeemed") == 1);
    promo_msg_t = 0;
    stub_login_status = 0;
    game_state = ST_LOBBY;
    puts("promo: 2027 message survives the string pool, no garbage or ? glyph");
}

static void test_azum_buff(void) {
    game_reset();
    assert(azum_buff_active == 0);
    assert(strcmp(tr_class_azum(), "Азум") == 0);
    player_class = CLASS_AZUM;
    reset_battle();
    enemy_class = CLASS_AZUM;
    enemy->max_hp = player_max_hp_for(CLASS_AZUM);
    enemy->hp = enemy->max_hp / 2;
    player->hp = player->max_hp / 2;
    stub_azum_buff = 1;
    update_azum_buff();
    assert(fabs(player->max_hp - azum_hp * 1.10) < 1e-9);
    assert(fabs(player->hp / player->max_hp - 0.5) < 1e-9);
    assert(fabs(enemy->max_hp - azum_hp * 1.10) < 1e-9);
    assert(fabs(enemy->hp / enemy->max_hp - 0.5) < 1e-9);
    assert(fabs(class_punch_damage_for(CLASS_AZUM, 0) - punch_damage * 1.10) < 1e-9);
    assert(fabs(azum_dash_damage_for() - azum_dash_damage * 1.10) < 1e-9);
    assert(fabs(enemy_azum_dash_damage_for() - enemy_dash_damage * 1.10) < 1e-9);
    assert(player_max_hp_for(CLASS_ORDINARY) == 20);
    assert(class_punch_damage_for(CLASS_SANTA, 0) == santa_damage);
    assert(strcmp(tr_class_azum(), "@z&m") == 0);
    assert(strcmp(tr_class_azum_desc(), "Суббота-17") == 0);
    language = 0;
    assert(strcmp(tr_class_azum_desc(), "Saturday-17") == 0);
    for (int i = 0; i < 2; i++) {
        player->hp = 0;
        enemy->hp = 0;
        assert(try_revive() == 1 && enemy_try_revive() == 1);
        assert(player->hp == player->max_hp && enemy->hp == enemy->max_hp);
    }
    player->hp = 0;
    enemy->hp = 0;
    assert(try_revive() == 0 && enemy_try_revive() == 0);
    assert(azum_revived == 2 && enemy_revived == 2);
    stub_azum_buff = 0;
    update_azum_buff();
    assert(fabs(player->max_hp - azum_hp) < 1e-9);
    assert(fabs(enemy->max_hp - azum_hp) < 1e-9);
    assert(strcmp(tr_class_azum(), "Azum") == 0);
    assert(strcmp(tr_class_azum_desc(), "Lived once, buried twice...") == 0);
    assert(class_punch_damage_for(CLASS_AZUM, 0) == punch_damage);
    assert(azum_dash_damage_for() == azum_dash_damage);
    /* Disabling the flag never grants another revive after two were spent. */
    assert(try_revive() == 0 && enemy_try_revive() == 0);
    azum_revived = 0;
    enemy_revived = 0;
    assert(try_revive() == 1 && enemy_try_revive() == 1);
    player->hp = 0;
    enemy->hp = 0;
    assert(try_revive() == 0 && enemy_try_revive() == 0);

    /* FlashSale: off keeps full prices, on cuts EVERY price by 10%
     * (rounded to whole units), and buying charges the discounted price. */
    assert(flash_sale_active == 0);
    assert(class_cost_of(CLASS_AZUM) == azum_cost);
    assert(level_cost(1) == level_base_cost);
    stub_flash_sale = 1;
    update_flash_sale();
    assert(flash_sale_active == 1);
    assert(class_cost_of(CLASS_AZUM) == 59);  /* 65 - 10% -> 58.5 -> 59 */
    assert(class_cost_of(CLASS_SANTA) == 90); /* 100 candies - 10% */
    assert(class_cost_of(CLASS_ASTRA) == 122); /* 135 - 10% -> 121.5 -> 122 */
    assert(level_cost(1) == 27);              /* 30 - 10% */
    assert(level_cost(3) == 45);              /* 50 - 10% */
    assert(class_cost_of(CLASS_ORDINARY) == 0);
    cups = 59;
    pick_class(CLASS_AZUM);
    assert(class_owned_of(CLASS_AZUM) == 1 && cups == 0);
    stub_flash_sale = 0;
    update_flash_sale();
    assert(flash_sale_active == 0 && class_cost_of(CLASS_EBUC) == ebuc_cost);
    assert(level_cost(1) == level_base_cost);
    puts("flash sale: -10% on all prices and purchase OK");
}

/* Ульта CodeCracker: без кружка вокруг куба; звёздочки - гладкие. */
static void test_codecracker_fx(void) {
    fx_ring_count = 0;
    fx_big_circle_count = 0;
    draw_codecracker_fx_at(200, 200, 0.3, 0.7);
    assert(fx_ring_count == 0);
    assert(fx_big_circle_count == 0);

    /* A smooth star needs many more strokes than its diameter in pixels:
     * the old pixel step produced at most ~2*r strokes. */
    double r = stat_star_r();
    star_line_count = 0;
    draw_star(0, 0, r, 0xFFF7C948);
    assert(star_line_count > (int)(3 * r));
    puts("codecracker fx: no halo circle in the ult; stars drawn smooth, not pixelated");
}

static int stub_roomlist_status;
static int stub_roomlist_count;
static char stub_room_names[6][25];
static int stub_prep_status;
static char stub_prep_name[25];
static char stub_prep_pass[64];
static int stub_prep_create;
static int stub_prep_calls;
void net_roomlist_fetch(const char *url) { (void)url; stub_roomlist_status = 2; }
double net_roomlist_status(void) { return stub_roomlist_status; }
double net_roomlist_count(void) { return stub_roomlist_count; }
const char *net_roomlist_name(double idx) {
    int i = (int)idx;
    return (i >= 0 && i < stub_roomlist_count) ? stub_room_names[i] : "";
}
void net_room_prepare(const char *url, const char *name, const char *pass, double create) {
    (void)url;
    snprintf(stub_prep_name, sizeof(stub_prep_name), "%s", name ? name : "");
    snprintf(stub_prep_pass, sizeof(stub_prep_pass), "%s", pass ? pass : "");
    stub_prep_create = create ? 1 : 0;
    stub_prep_calls++;
    stub_prep_status = 1;
}
double net_room_prep_status(void) { return stub_prep_status; }
static char stub_current_room[25];
const char *net_current_room(void) { return stub_current_room; }
void net_room_prep_reset(void) { stub_prep_status = 0; }

/* Ачивка mov eax, 0x229 и экран комнат с паролями. */

static void test_dice_alone_and_bubbles(void) {
    /* Dice event: prizes, admin-only cubes, throw + collect. */
    assert(dice_prize_of(DICE_CUBE_ORDINARY) == 2);
    assert(dice_prize_of(DICE_CUBE_LEGENDARY) == 95);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "x");
    stub_login_status = 2;
    astra_ability_refresh();
    assert(dev_access() == 0);
    srand(12345);
    double legendary = 0;
    double saw_santa = 0;
    double saw_cc = 0;
    double i = 0;
    while (i < 30000) {
        double f = dice_roll_cube();
        /* Every class rolls for everyone, admins included or not. */
        if (f == DICE_CUBE_SANTA) saw_santa = 1;
        if (f == DICE_CUBE_CODECRACKER) saw_cc = 1;
        if (f == DICE_CUBE_LEGENDARY) legendary = legendary + 1;
        i = i + 1;
    }
    assert(legendary > 0 && legendary < 80); /* 0.08% of 30000 = ~24 */
    assert(saw_santa == 1 && saw_cc == 1);

    screen_w = 800;
    screen_h = 600;
    event_mode = 4;
    dice_event_reset();
    dt = 0.7;
    update_dice_event(); /* first cube is thrown */
    assert(arr_get(dice_cubes, 0) == 1);
    double cube = arr_get(dice_cubes, 1);
    arr_set(dice_cubes, 2, dice_flight); /* landed */
    player->x = 300;
    player->y = 300;
    player->size = 50;
    arr_set(dice_cubes, 5, 300);
    arr_set(dice_cubes, 6, 300);
    double candies_before = candies;
    update_dice_event();
    assert(candies == candies_before + dice_prize_of(cube));
    assert(arr_get(dice_cubes, 0) == 0);
    event_mode = 0;
    update_dice_event();
    assert(dice_active == 0);

    /* Alone in a room: 40 s of waiting, then out to the room list. */
    stub_net_slot = 0;
    double s = 0;
    while (s < 4) { stub_online[(int)s] = 0; s = s + 1; }
    s = 0;
    while (s < max_players * remote_fields) { arr_set(remotes, s, 0); s = s + 1; }
    online_ready = 1;
    finished = 0;
    game_state = ST_ONLINE;
    match_seen_foe = 0;
    foes_gone_t = 0;
    alone_t = 0;
    dt = 1;
    t_dir = 0;
    i = 0;
    while (i < 39) { check_online_finish(); i = i + 1; }
    assert(alone_t == 39 && t_dir == 0); /* still waiting, not kicked */
    check_online_finish();
    assert(t_target == ST_ROOMS && t_dir != 0); /* 40 s: out */
    t_dir = 0;
    alone_t = 5;
    arr_set(remotes, remote_fields + 5, 1); /* someone joins alive */
    check_online_finish();
    assert(alone_t == 0);
    arr_set(remotes, remote_fields + 5, 0);
    online_ready = 0;
    game_state = ST_LOBBY;

    /* Admin passwd command: resets a normal account, never an admin. */
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", ADMIN_PASS);
    stub_login_status = 2;
    stub_admin_reset_calls = 0;
    assert(admin_handle("passwd bob secret1") == 1);
    assert(stub_admin_reset_calls == 1);
    assert(strcmp(stub_admin_reset_nick, "bob") == 0);
    assert(strcmp(stub_admin_reset_new, "secret1") == 0);
    assert(admin_handle("passwd Dimasi4ek229 secret1") == 1);
    assert(stub_admin_reset_calls == 1); /* protected admin account */
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "x");

    /* Chat bubbles: a second message must replace the bubble even though
     * net_chat_key() reuses one static buffer (the old aliasing bug). */
    snprintf(stub_nicks[1], sizeof(stub_nicks[1]), "%s", "bob");
    stub_chat_count = 1;
    snprintf(stub_chat_keys[0], sizeof(stub_chat_keys[0]), "%s", "k1");
    snprintf(stub_chat_uids[0], sizeof(stub_chat_uids[0]), "%s", "bob");
    snprintf(stub_chat_texts[0], sizeof(stub_chat_texts[0]), "%s", "first");
    chat_bubbles_reset();
    dt = 0.05;
    chat_bubbles_watch();
    assert(strcmp(bubble_msg(1), "first") == 0);
    stub_chat_count = 2;
    snprintf(stub_chat_keys[1], sizeof(stub_chat_keys[1]), "%s", "k2");
    snprintf(stub_chat_uids[1], sizeof(stub_chat_uids[1]), "%s", "bob");
    snprintf(stub_chat_texts[1], sizeof(stub_chat_texts[1]), "%s", "second");
    chat_bubbles_watch();
    assert(strcmp(bubble_msg(1), "second") == 0);
    stub_chat_count = 0;
    puts("dice event + alone wait + chat bubbles: OK");
}

static void test_eax229_and_rooms(void) {
    game_reset();
    screen_w = 1280;
    screen_h = 720;
    assert(achievement_count() == 5);
    assert(strcmp(tr_achievement_eax229(), "mov eax, 0x229") == 0);
    assert(strcmp(tr_achievement_eax229_desc(), "???") == 0);

    /* Dimasi4ek229 himself: granted automatically (case-insensitive). */
    achievement_mask = 0;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "dimasi4ek229");
    check_meet_eax229();
    assert(has_achievement(ACH_EAX229) == 1);

    /* Anyone else: only when Dimasi4ek229 is in the same online room. */
    achievement_mask = 0;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    game_state = ST_LOBBY;
    check_meet_eax229();
    assert(has_achievement(ACH_EAX229) == 0);
    game_state = ST_ONLINE;
    stub_net_slot = 0;
    stub_online[1] = 1;
    snprintf(stub_nicks[1], sizeof(stub_nicks[1]), "%s", "Dimasi4ek229");
    check_meet_eax229();
    assert(has_achievement(ACH_EAX229) == 1);
    stub_online[1] = 0;
    stub_nicks[1][0] = 0;
    game_state = ST_LOBBY;

    /* Rooms screen: join an open room, a password room, and create one. */
    achievement_mask = 0;
    game_state = ST_ROOMS;
    t_dir = 0;
    warn_open = 0;
    studio_open = 0;
    start_open = 0;
    snprintf(stub_room_names[0], sizeof(stub_room_names[0]), "%s", "main");
    snprintf(stub_room_names[1], sizeof(stub_room_names[1]), "%s", "closed-club");
    stub_roomlist_count = 2;
    stub_roomlist_status = 2;
    rooms_open();
    assert(rooms_mode == 0);
    logic_touch(rooms_x() + rooms_w() / 2, rooms_row_y(0) + rooms_row_h() / 2, 0, 81);
    press_new = 0;
    assert(stub_prep_calls == 1 && stub_prep_create == 0 && strcmp(stub_prep_name, "main") == 0);
    stub_prep_status = 2; /* open room: allowed in */
    update_rooms();
    assert(t_dir == 1 && t_target == ST_ONLINE && strcmp(ROOM, "main") == 0);
    t_dir = 0;
    t_target = 0;
    game_state = ST_ROOMS;
    rooms_open();
    logic_touch(rooms_x() + rooms_w() / 2, rooms_row_y(1) + rooms_row_h() / 2, 0, 82);
    press_new = 0;
    stub_prep_status = 3; /* password required */
    update_rooms();
    assert(rooms_mode == 2 && str_len(rooms_msg) > 0);
    rooms_pass = "s3cret";
    rooms_submit_join();
    assert(stub_prep_create == 0 && strcmp(stub_prep_pass, "s3cret") == 0);
    stub_prep_status = 2;
    update_rooms();
    assert(t_target == ST_ONLINE && strcmp(ROOM, "closed-club") == 0);
    t_dir = 0;
    t_target = 0;
    game_state = ST_ROOMS;
    rooms_open();
    rooms_mode = 1;
    rooms_name = "my-room";
    rooms_pass = "";
    rooms_submit_create();
    assert(stub_prep_create == 1 && strcmp(stub_prep_name, "my-room") == 0);
    stub_prep_status = 2;
    update_rooms();
    assert(t_target == ST_ONLINE && strcmp(ROOM, "my-room") == 0);
    t_dir = 0;
    t_target = 0;
    game_state = ST_LOBBY;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");

    /* Chat header: after creating a room, the chat title is that room. */
    snprintf(stub_current_room, sizeof(stub_current_room), "%s", "my-room");
    language = 0;
    game_state = ST_ONLINE;
    chat_open = 1;
    adm_reset_log();
    draw_chat();
    assert(adm_has_text("Room: my-room") == 1);
    chat_open = 0;
    game_state = ST_LOBBY;
    puts("eax229 + rooms: meet-admin achievement, room list, password join, create");
}

/* Drawing the complete UI retains every screen through draw_screen(). */
void text(const char *s, float x, float y, uint32_t color) { text_scaled(s, x, y, color, 1); }
void tex_round(float x, float y, const char *name, float scale, float radius) {
    (void)radius;
    tex(x, y, name, 0, scale);
}
void rect_rot(float x, float y, float w, float h, float angle, uint32_t color) {
    (void)angle;
    rect(x, y, w, h, color);
}
double lerp(double a, double b, double t) { return a + (b - a) * t; }
int str_starts_with(const char *s, const char *pref) { return strncmp(s, pref, strlen(pref)) == 0; }
double net_chat_count(void) { return stub_chat_count; }
const char *net_chat_uid(double idx) { return stub_chat_uids[(int)idx]; }
const char *net_chat_text(double idx) { return stub_chat_texts[(int)idx]; }
/* Like the real one: ONE shared static buffer for every call. */
const char *net_chat_key(double idx) {
    static char ret[28];
    snprintf(ret, sizeof(ret), "%s", stub_chat_keys[(int)idx]);
    return ret;
}
double net_status(void) { return net_st_playing; }
double net_leaderboard_status(void) { return 0; }
double net_leaderboard_count(void) { return 0; }
const char *net_leaderboard_nick(double idx) { (void)idx; return ""; }
double net_leaderboard_cups(double idx) { (void)idx; return 0; }
double net_player_codecracker_super(double slot) { (void)slot; return 0; }
double net_player_block(double slot) { (void)slot; return 0; }
const char *net_banner_text(void) { return ""; }
const char *net_banner_color(void) { return "white"; }

static void test_class_stars(void) {
    game_reset();
    screen_w = 1280;
    screen_h = 720;
    assert(class_imbalance_of(CLASS_ORDINARY) == 1);
    assert(class_imbalance_of(CLASS_SANTA) == 2);
    assert(class_imbalance_of(CLASS_AZUM) == 3);
    assert(class_imbalance_of(CLASS_EBUC) == 3);
    assert(class_imbalance_of(CLASS_ASTRA) == 4);
    assert(class_imbalance_of(CLASS_CODECRACKER) == 5);
    double r = stat_star_r();
    star_line_count = 0;
    draw_star(0, 0, r * 1.1, 0xFFF7C948);
    int filled = star_line_count;
    star_line_count = 0;
    draw_star(0, 0, r, 0xFF0B0B12);
    int empty = star_line_count;
    for (int i = 0; i < 6; i++) {
        player_class = class_at_order(i);
        int rating = (int)class_imbalance_of(player_class);
        star_line_count = 0;
        star_max_y = 0;
        adm_reset_log();
        draw_class_stats();
        assert(star_line_count == rating * filled + (5 - rating) * empty);
        assert(star_max_y < stat_motto_y());
        assert(adm_has_text(class_desc_of(player_class)));
    }
    puts("classes: all five rating stars restored above the description");
}

static void assert_curtain_on_top(void) {
    double fade = t_fade;
    t_fade = 1;
    adm_reset_log();
    logic_draw();
    assert(adm_color_n > 0 && adm_color_n < ADM_COLOR_MAX);
    assert(adm_colors[adm_color_n - 1] == 0xFF000000);
    t_fade = fade;
}

static void test_overlay_transitions(void) {
    game_reset();
    screen_w = 1280;
    screen_h = 720;
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    warn_open = 0;
    studio_open = 0;
    /* The account stays behind the curtain, including opening from lobby. */
    logic_touch(menu_x() + menu_btn_w() / 2, menu_row(3) + btn_h / 2, 0, 90);
    assert(t_dir == 1 && acct_open == 0);
    finish_transition();
    assert(acct_open == ACCT_MENU);
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(0) + btn_h / 2, 0, 91);
    assert(t_dir == 1 && acct_open == ACCT_MENU);
    double generation = ui_nav_gen;
    logic_draw();
    update_button_press();
    press_overlay = 3;
    assert(button_press_k(acct_x(), acct_menu_y(0), acct_w(), btn_h) > 0);
    press_overlay = 0;
    assert(ui_nav_gen == generation);
    /* A second tap or Back cannot overwrite a pending navigation action. */
    assert(logic_back() == 1 && acct_open == ACCT_MENU);
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(3) + btn_h / 2, 0, 92);
    assert(acct_open == ACCT_MENU);
    assert_curtain_on_top();
    finish_transition();
    assert(acct_open == ACCT_PASS);
    assert(logic_back() == 1 && acct_open == ACCT_PASS);
    finish_transition();
    assert(acct_open == ACCT_MENU);
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(3) + btn_h / 2, 0, 93);
    assert(acct_open == ACCT_MENU && t_dir == 1);
    finish_transition();
    assert(acct_open == ACCT_DELETE);
    assert(logic_back() == 1);
    finish_transition();
    /* Switch-account used to hide the old window before any fade. */
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(1) + btn_h / 2, 0, 94);
    assert(acct_open == ACCT_MENU && t_dir == 1);
    assert_curtain_on_top();
    finish_transition();
    assert(acct_open == 0 && game_state == ST_LOGIN);
    /* Opening, paging and closing documents share that very same curtain. */
    docs_transition(0, 0);
    assert(docs_open == 0 && t_dir == 1);
    finish_transition();
    assert(docs_open == 1 && game_state == ST_LOGIN);
    touch_docs(docs_next_x() + docs_nav_w() / 2, docs_row_y() + btn_h / 2, 0);
    assert(docs_page == 0 && t_dir == 1);
    assert_curtain_on_top();
    finish_transition();
    assert(docs_page == 1);
    touch_docs(docs_prev_x() + docs_nav_w() / 2, docs_row_y() + btn_h / 2, 0);
    finish_transition();
    assert(docs_page == 0);
    assert(logic_back() == 1 && docs_open == 1);
    finish_transition();
    assert(docs_open == 0 && game_state == ST_LOGIN);
    /* A covered documents button may not claim an account's press. */
    docs_show(0, 0);
    acct_show(ACCT_MENU);
    press_new = 1;
    press_x = acct_x() + acct_w() / 2;
    press_y = acct_menu_y(0) + btn_h / 2;
    press_btn_live = 0;
    press_phase = 0;
    press_overlay = 2;
    assert(button_press_k(acct_x(), acct_menu_y(0), acct_w(), btn_h) == 0);
    assert(press_new == 1);
    press_overlay = 3;
    (void)button_press_k(acct_x(), acct_menu_y(0), acct_w(), btn_h);
    assert(press_new == 0 && press_btn_live == 1);
    press_overlay = 0;
    acct_close();
    docs_close();
    /* Reset must discard callbacks as well as the transition clock. */
    docs_transition(0, 0);
    assert(t_action != NULL);
    game_reset();
    assert(t_action == NULL && t_dir == 0);
    puts("navigation: deferred overlays, outgoing press, topmost curtain and input lock OK");
}

int main(void) {
    state_create();
    state_ready = 1;
    assert(player && enemy && punch && gift && enemy_gift);
    assert(ST_LOBBY == 0 && ST_SOLO == 1 && ST_ONLINE == 5);
    assert(player->hp == 20 && enemy->hp == 20);
    assert(class_level_tbl && remote_punches && plates_candies && flake_cache);

    /* The game starts in Russian when the phone language is unknown (168-FZ). */
    assert(language == 1);
    assert(strcmp(tr_play(), "Играть") == 0);
    language = 0;
    assert(strcmp(tr_play(), "Play") == 0);
    language = 1;
    assert(strcmp(tr_play(), "Играть") == 0);
    language = 0;

    assert(class_count == 6 && CLASS_ASTRA == 4 && CLASS_CODECRACKER == 6);
    assert(winter_theme == 0 && winter_theme_on() == 0); /* the normal theme is the default */
    assert(strcmp(tr_class_level_effect(CLASS_ASTRA, 1), tr_class_level_effect(CLASS_ASTRA, 1)) == 0);
    assert(strcmp(tr_class_level_effect(CLASS_ASTRA, 2), tr_class_level_effect(CLASS_ASTRA, 2)) == 0);
    assert(strcmp(tr_class_level_effect(CLASS_ASTRA, 3), tr_class_level_effect(CLASS_ASTRA, 3)) == 0);
    assert(punch_forward_offset == 19 && astra_grab_forward_offset == 19);
    assert(fabs(astra_hit_interval - 0.2) < 1e-9);
    assert(fabs(astra_final_delay - 0.85) < 1e-9);
    assert(fabs(astra_ability_final_time_for(0) - astra_ability_beat_time_for(astra_hit_count, 0) - astra_final_delay) < 1e-9);
    assert(class_cost_of(CLASS_AZUM) == 65);
    assert(class_cost_of(CLASS_SANTA) == 100);
    assert(class_cost_of(CLASS_EBUC) == 120);
    assert(class_cost_of(CLASS_ASTRA) == 135);
    assert(class_has_super(CLASS_ASTRA) == 1);
    assert(astra_cd_for(0) == 8);
    assert(astra_cd_for(1) < astra_cd_for(0));
    assert(astra_zone_reach(2) > astra_zone_reach(1));
    assert(astra_throw_dist_for(3) > astra_throw_dist_for(2));
    assert(fabs(astra_grab_total_fraction() - 0.45) < 1e-9);
    assert(strcmp(WINTER_JINGLE, "winter_jingle.wav") == 0);
    assert(strcmp(fighter_sprite(CLASS_ORDINARY, 0, SKIN_NORMAL), ORDINARY_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_ORDINARY, 1, SKIN_NORMAL), PUNCH_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_ASTRA, 0, SKIN_NORMAL), ORDINARY_TEX) == 0);
    assert(strcmp(class_card_tex(CLASS_ASTRA, SKIN_NORMAL), ASTRA_CARD_TEX) == 0);
    assert(class_has_super(CLASS_ASTRA) == 1 && player_max_hp_for(CLASS_ASTRA) == astra_hp);
    /* Long-fight balance: every pool doubled (ordinary/santa 20, azum 13,
     * astra 18, ebuc 24) and the ability numbers scaled with them, while the
     * basic attacks kept their old damage - that is what makes fights last. */
    assert(player_max_hp_for(CLASS_ORDINARY) == 20 && player_max_hp_for(CLASS_SANTA) == 20);
    assert(azum_hp == 13 && astra_hp == 18 && ebuc_hp == 24 && enemy_base_hp == 20);
    assert(punch_damage == 1 && santa_damage == 0.25 && ebuc_damage == 1.0);
    assert(ebuc_universe_damage >= ebuc_hp);
    azum_tex_ok = azum_punch_tex_ok = 1;
    azum_zombie_tex_ok = azum_zombie_punch_tex_ok = 1;
    assert(strcmp(fighter_sprite(CLASS_AZUM, 0, SKIN_NORMAL), AZUM_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_AZUM, 1, SKIN_NORMAL), AZUM_PUNCH_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_AZUM, 0, SKIN_ZOMBIE), AZUM_ZOMBIE_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_AZUM, 1, SKIN_ZOMBIE), AZUM_ZOMBIE_PUNCH_TEX) == 0);
    FighterSpritePair azum_pair = look_sprite_pair(LOOK_AZUM);
    FighterSpritePair zombie_pair = look_sprite_pair(LOOK_ZOMBIE);
    assert(strcmp(azum_pair.idle, AZUM_TEX) == 0 && strcmp(azum_pair.punch, AZUM_PUNCH_TEX) == 0);
    assert(strcmp(zombie_pair.idle, AZUM_ZOMBIE_TEX) == 0 &&
           strcmp(zombie_pair.punch, AZUM_ZOMBIE_PUNCH_TEX) == 0);
    resolve_fighter_look(FLOOK_ME, CLASS_ORDINARY, SKIN_NORMAL);
    assert(fighter_look_of(FLOOK_ME) == LOOK_ORDINARY);
    assert(strcmp(look_tex(fighter_look_of(FLOOK_ME), fighter_pose_of(FLOOK_ME, 1)), PUNCH_TEX) == 0);
    resolve_fighter_look(FLOOK_ME, CLASS_AZUM, SKIN_NORMAL);
    assert(fighter_look_of(FLOOK_ME) == LOOK_AZUM);
    assert(strcmp(look_tex(fighter_look_of(FLOOK_ME), fighter_pose_of(FLOOK_ME, 1)), AZUM_PUNCH_TEX) == 0);
    assert(remote_fields == 12);

    assert(rects_overlap(0, 0, 1, 0, 10, 10, 15, 0, 1, 0, 10, 10) == 1);
    assert(rects_overlap(0, 0, 1, 0, 10, 10, 25, 0, 1, 0, 10, 10) == 0);
    assert(circle_hits_box(0, 0, 5, 8, 0, 0, 4) == 1);
    assert(circle_hits_box(0, 0, 2, 8, 0, 0, 4) == 0);

    /* The virtual stick now gives full directional speed beyond its small
     * deadzone: near-center and rim positions travel the same distance. */
    joy.x = 120;
    joy.y = 90;
    joy.r = 70;
    joy_id = -1;
    player_class = CLASS_ORDINARY;
    player->x = 160;
    player->y = 90;
    joy_touch(joy.x + 14, joy.y, 0, 7);
    assert(fabs(joy.dx - 1) < 1e-9 && fabs(joy.dy) < 1e-9);
    move_player();
    double near_stick_move = player->x - 160;
    player->x = 160;
    joy_touch(joy.x + joy.r, joy.y, 2, 7);
    assert(fabs(joy.dx - 1) < 1e-9 && fabs(joy.dy) < 1e-9);
    move_player();
    assert(fabs((player->x - 160) - near_stick_move) < 1e-9);
    player->x = 160;
    joy_touch(joy.x + 5, joy.y, 2, 7);
    assert(joy.dx == 0 && joy.dy == 0);
    move_player();
    assert(fabs(player->x - 160) < 1e-9);
    joy_touch(joy.x + 5, joy.y, 1, 7);

    player->x = 400;
    player->y = 400;
    enemy->x = 399;
    enemy->y = 400;
    enemy->angle = 0.75;
    assert(punch_hits_enemy(player->x, player->y, 1, 0, punch_reach, punch_width) == 0);
    assert(astra_zone_hits_enemy(player->x, player->y, 1, 0, 0) == 0);
    enemy->x = 470;
    assert(punch_hits_enemy(player->x, player->y, 1, 0, punch_reach, punch_width) == 1);
    assert(astra_zone_hits_enemy(player->x, player->y, 1, 0, 0) == 1);

    winter_theme = 0;
    snow_tex_ok = 1;
    game_state = ST_SOLO;
    ground_tiles = ground_fills = 0;
    draw_arena_background();
    assert(ground_tiles > 0 && ground_fills == 1);
    assert(last_ground_fill == (uint32_t)grass_ground_rgb);
    assert(strcmp(last_ground, GRASS) == 0);

    ground_texture_missing = 1;
    ground_tiles = ground_fills = 0;
    draw_arena_background();
    assert(ground_tiles == 0 && ground_fills == 1);
    ground_texture_missing = 0;

    /* Winter theme (and Santa, and candies) are developer-only for now:
     * a saved winter setting alone does not turn anything on. */
    winter_theme = 1;
    astra_ability_ok = 0;
    assert(dev_access() == 0 && winter_theme_on() == 0 && candies_on() == 0);
    assert(class_visible(CLASS_SANTA) == 0);
    ground_tiles = ground_fills = 0;
    draw_arena_background();
    assert(strcmp(last_ground, GRASS) == 0);
    astra_ability_ok = 1;
    assert(dev_access() == 1 && winter_theme_on() == 1 && candies_on() == 1);
    assert(class_visible(CLASS_SANTA) == 1);
    ground_tiles = ground_fills = 0;
    draw_arena_background();
    assert(ground_tiles > 0 && ground_fills == 1);
    assert(strcmp(last_ground, SNOW_TEX) == 0);
    assert(snow_active() == 1);
    assert(newyear_active() == 1);
    assert(newyear_menu_active() == 0);
    /* Developer access stays on through the winter/candy sections below;
     * the admin-login section further down refreshes it from credentials. */

    /* The lobby gradient is one smooth wash in every theme: hundreds of ~2 px
     * bands instead of the old thirty 14 px strips. */
    double saved_w = screen_w;
    double saved_h = screen_h;
    double saved_winter = winter_theme;
    screen_w = 1080;
    screen_h = 2400;
    winter_theme = 0;
    ground_tiles = ground_fills = 0;
    screen_fill();
    assert(ground_fills >= 1 + bg_gradient_h / 2);
    winter_theme = 1;
    ground_tiles = ground_fills = 0;
    screen_fill();
    assert(ground_fills >= 1 + bg_gradient_h / 2);
    winter_theme = saved_winter;
    screen_w = saved_w;
    screen_h = saved_h;

    game_state = ST_LOBBY;
    warn_open = 1;
    studio_open = 0;
    newyear_jingle_ok = 1;
    assert(snow_active() == 0);
    /* The startup notice and transitions do not pause the snow clock. */
    assert(newyear_menu_active() == 1);
    assert(newyear_music_active() == 1);
    newyear_snow_t = 0;
    update_newyear_snow();
    assert(newyear_snow_t > 0);
    t_fade = 0.5;
    t_dir = 1;
    update_newyear_snow();
    t_fade = 0;
    t_dir = 0;
    assert(newyear_snow_t >= 2 * dt);
    assert(newyear_menu_flakes == 22);

    /* Startup notice open and fully opaque: the epilepsy warning keeps every
     * flake under its panel, and nothing is ever drawn above its text. */
    snowflake_draws = 0;
    snowflake_tinted = 0;
    warn_open = 1;
    warn_a = 1;
    assert(newyear_snow_under_notice_active() == 0 && newyear_snow_over_notice_active() == 0);
    draw_newyear_under_notices();
    assert(snowflake_draws == 0);
    draw_newyear_over_notices();
    assert(snowflake_draws == 0);

    /* The panel is fading: the lobby - and its snow - shows through beneath it,
     * still never on top of the legal text, and still in the lobby layout. */
    warn_a = 0.4;
    assert(newyear_snow_under_notice_active() == 1 && newyear_snow_over_notice_active() == 0);
    snowflake_draws = 0;
    snowflake_tinted = 0;
    draw_newyear_under_notices();
    /* The bundled flake texture is blue; the lobby snow has to be tinted white
     * so the winter menu shows white flakes instead of blue blobs. */
    assert(snowflake_draws == (int)newyear_menu_flakes && snowflake_tinted == (int)newyear_menu_flakes);
    assert(snowflake_tint_color == 0xFFFFFFFF);
    /* The over-pass is inactive while the warning still shows, so the count
     * does not change. */
    draw_newyear_over_notices();
    assert(snowflake_draws == (int)newyear_menu_flakes);
    warn_a = 1;

    /* Snow falls across the full width in both passes - the button column
     * included, since the snow is drawn behind the buttons now. Closing the
     * splash must not move any flake: the over-pass at zero opacity has to
     * match the lobby layout exactly. */
    {
        int saved_w = screen_w;
        int saved_h = screen_h;
        double saved_t = newyear_snow_t;
        double lo = 0;
        double hi = 0;
        double lobby_x[64];
        int centre_over = 0;
        int centre_lobby = 0;
        int handoff_mismatch = 0;
        int k = 0;

        assert(newyear_menu_flakes <= 64);
        screen_w = 1080;
        screen_h = 2400;
        lo = menu_x() - 30;
        hi = menu_x() + btn_w + 30;
        for (k = 0; k < 200; k++) {
            int j = 0;
            newyear_snow_t = 1.0 + k * 0.173;

            studio_open = 0;
            studio_a = 0;
            warn_open = 0;
            warn_a = 0;
            snowflake_centre_n = 0;
            draw_newyear_under_notices();
            assert(snowflake_centre_n == (int)newyear_menu_flakes);
            for (j = 0; j < snowflake_centre_n; j++) {
                lobby_x[j] = snowflake_centres[j];
                if (lobby_x[j] >= lo && lobby_x[j] <= hi) {
                    centre_lobby++;
                }
            }

            studio_open = 1;
            studio_a = 0;
            snowflake_centre_n = 0;
            draw_newyear_over_notices();
            assert(snowflake_centre_n == (int)newyear_menu_flakes);
            for (j = 0; j < snowflake_centre_n; j++) {
                double d = snowflake_centres[j] - lobby_x[j];
                if (d > 1e-3 || d < -1e-3) {
                    handoff_mismatch++;
                }
            }

            studio_a = 1;
            snowflake_centre_n = 0;
            draw_newyear_over_notices();
            assert(snowflake_centre_n == (int)newyear_menu_flakes);
            for (j = 0; j < snowflake_centre_n; j++) {
                if (snowflake_centres[j] >= lo && snowflake_centres[j] <= hi) {
                    centre_over++;
                }
            }
        }
        /* The button column is about 31% of a 1080 px screen; flakes reach it
         * in both passes, so require at least 10% of draws there. */
        assert(centre_lobby >= 200 * (int)newyear_menu_flakes / 10);
        assert(handoff_mismatch == 0);
        assert(centre_over >= 200 * (int)newyear_menu_flakes / 10);

        screen_w = saved_w;
        screen_h = saved_h;
        newyear_snow_t = saved_t;
        studio_open = 0;
        studio_a = 0;
        warn_open = 1;
        warn_a = 1;
    }

    /* A transition curtain over the studio splash: no flakes are drawn above
     * black, and the epilepsy warning is never snowed over either. */
    studio_open = 1;
    studio_a = 1;
    t_fade = 0.5;
    t_dir = 1;
    snowflake_draws = 0;
    assert(newyear_snow_over_notice_active() == 0);
    assert(newyear_snow_under_notice_active() == 0);
    draw_newyear_over_notices();
    assert(snowflake_draws == 0);
    studio_open = 0;
    studio_a = 0;
    warn_open = 1;
    warn_a = 1;
    snowflake_draws = 0;
    draw_newyear_under_notices();
    assert(snowflake_draws == 0);
    t_fade = 0;
    t_dir = 0;

    /* Notice closed: flakes draw beneath the curtain and keep drawing through a fade. */
    warn_open = 0;
    assert(newyear_snow_under_notice_active() == 1 && newyear_snow_over_notice_active() == 0);
    snowflake_draws = 0;
    draw_newyear_under_notices();
    assert(snowflake_draws == (int)newyear_menu_flakes);
    t_fade = 0.5;
    t_dir = 1;
    snowflake_draws = 0;
    draw_newyear_under_notices();
    assert(snowflake_draws == (int)newyear_menu_flakes);
    t_fade = 0;
    t_dir = 0;

    /* The studio splash is an opaque notice too. */
    studio_open = 1;
    assert(newyear_snow_under_notice_active() == 0 && newyear_snow_over_notice_active() == 1);
    studio_open = 0;

    t_fade = 0;
    t_dir = 0;
    studio_open = 0;
    warn_open = 1;
    /* The jingle must take over even while the startup notice is visible; do
     * not let the ordinary lobby loop block it. */
    music_ok = 1;
    music_level = 1;
    lobby_playing = 1;
    winter_playing = 0;
    winter_loop_calls = 0;
    update_newyear_jingle();
    assert(lobby_playing == 0 && winter_playing == 1);
    assert(winter_loop_calls == 1 && music_level == 0);
    update_music();
    assert(lobby_playing == 0);
    winter_theme = 0;
    update_newyear_jingle();
    assert(winter_playing == 0);
    winter_theme = 1;
    warn_open = 0;

    /* Candies appear by fading in only: same place, same size, alpha 0 -> 1. */
    {
        double cx = 0;
        double cy = 0;
        double x0 = 0;
        double y0 = 0;
        candy_enabled = 1;
        arr_set(candy_pick, 0, 0);
        cx = arr_get(candy_x, 0);
        cy = arr_get(candy_y, 0);
        candy_tex_ok = 1;
        arr_set(candy_t, 0, 0);
        candy_draws = 0;
        draw_candies();
        assert(candy_draws == 0);

        arr_set(candy_t, 0, candy_pop_time * 0.05);
        candy_alpha_drawn = -1;
        candy_scale_drawn = -1;
        draw_candies();
        assert(candy_draws == 1);
        /* scale приходит как float, сравниваем с допуском */
        assert(candy_scale_drawn > candy_scale - 1e-5 && candy_scale_drawn < candy_scale + 1e-5);
        assert(candy_alpha_drawn > 0 && candy_alpha_drawn < 0.2);
        x0 = candy_x_drawn;
        y0 = candy_y_drawn;
        assert(fabs(x0 - cx) < 1e-3 && fabs(y0 - cy) < 1e-3);

        arr_set(candy_t, 0, candy_pop_time * 0.5);
        candy_scale_drawn = -1;
        draw_candies();
        assert(candy_scale_drawn > candy_scale - 1e-5 && candy_scale_drawn < candy_scale + 1e-5);
        assert(candy_alpha_drawn > 0.4 && candy_alpha_drawn < 1);
        /* Ни роста, ни подлёта: место то же самое, что в начале появления. */
        assert(fabs(candy_x_drawn - x0) < 1e-3 && fabs(candy_y_drawn - y0) < 1e-3);

        arr_set(candy_t, 0, candy_pop_time);
        candy_scale_drawn = -1;
        draw_candies();
        assert(candy_scale_drawn > candy_scale - 1e-5 && candy_scale_drawn < candy_scale + 1e-5);
        assert(candy_alpha_drawn > 0.99);
        assert(fabs(candy_x_drawn - x0) < 1e-3 && fabs(candy_y_drawn - y0) < 1e-3);

        /* Без текстуры - тот же альфа-канал в цвете круга. */
        candy_tex_ok = 0;
        arr_set(candy_t, 0, candy_pop_time * 0.05);
        candy_circle_color = 0;
        draw_candies();
        assert(((candy_circle_color >> 24) & 0xff) > 0 && ((candy_circle_color >> 24) & 0xff) < 60);
        assert((candy_circle_color & 0x00FFFFFF) == 0x00FF4081);
        assert(fabs(candy_x_drawn - cx) < 1e-3 && fabs(candy_y_drawn - cy) < 1e-3);
        arr_set(candy_t, 0, candy_pop_time);
        candy_circle_color = 0;
        draw_candies();
        assert(((candy_circle_color >> 24) & 0xff) > 250);
        candy_tex_ok = 0;
    }

    /* Punch boxes stay close to the body: 86 px long and 86 px wide. */
    assert(punch_reach == 86 && punch_width == 86);
    assert(enemy_punch_reach == 86 && enemy_punch_width == 86);

    /* Flawless victory requires full health and no incoming hit. */
    {
        game_state = ST_SOLO;
        finished = 0;
        cups_awarded = 0;
        achievement_mask = 0;
        player_class = CLASS_ORDINARY;
        player->max_hp = 10;
        player->hp = 10;
        player_was_hit = 0;
        enemy->hp = 0;
        finish_game(1);
        assert(has_achievement(ACH_FIRST_WIN) == 1);
        assert(has_achievement(ACH_LAST_HP) == 1);

        /* The former one-HP condition no longer qualifies. */
        finished = 0;
        cups_awarded = 0;
        achievement_mask = 0;
        player->hp = 1;
        player_was_hit = 0;
        finish_game(1);
        assert(has_achievement(ACH_LAST_HP) == 0);

        /* Being healed back to full does not erase damage taken earlier. */
        finished = 0;
        cups_awarded = 0;
        achievement_mask = 0;
        player->hp = 10;
        player_was_hit = 0;
        assert(take_damage(1) == 0);
        assert(player_was_hit == 1 && player->hp < player->max_hp);
        player->hp = player->max_hp;
        finish_game(1);
        assert(player->hp == player->max_hp);
        assert(has_achievement(ACH_LAST_HP) == 0);

        /* Ebuc's turret shield leaves player HP untouched, but the blocked
         * hit still disqualifies the achievement. */
        finished = 0;
        cups_awarded = 0;
        achievement_mask = 0;
        player_class = CLASS_EBUC;
        player->max_hp = ebuc_hp;
        player->hp = player->max_hp;
        player_was_hit = 0;
        arr_clear(turret_x);
        arr_clear(turret_y);
        arr_clear(turret_hp);
        arr_clear(turret_maxhp);
        arr_clear(turret_spawn_t);
        arr_push(turret_x, player->x);
        arr_push(turret_y, player->y);
        arr_push(turret_hp, 5);
        arr_push(turret_maxhp, 5);
        arr_push(turret_spawn_t, 0);
        assert(take_damage(1) == 0);
        assert(player->hp == player->max_hp);
        assert(arr_get(turret_hp, 0) == 4);
        assert(player_was_hit == 1);
        finish_game(1);
        assert(has_achievement(ACH_LAST_HP) == 0);

        assert(achievement_count() == 5);
        assert(achievement_bit(3) == ACH_LAST_HP);
        finished = 0;
        cups_awarded = 0;
        achievement_mask = 0;
        player_class = CLASS_ORDINARY;
        player->max_hp = 10;
        player->hp = 10;
        player_was_hit = 0;
        enemy->hp = 10;
        arr_clear(turret_x);
        arr_clear(turret_y);
        arr_clear(turret_hp);
        arr_clear(turret_maxhp);
        arr_clear(turret_spawn_t);
    }

    /* Leaving the game (game_bye, called when the activity pauses or stops):
     * half of the exits stay silent, the rest say one of the three farewells,
     * and the nick line needs a logged-in account. */
    {
        int i = 0;
        int silent = 0;
        int sorry = 0;
        int cubes = 0;
        int nick_line = 0;
        int empty_nick = 0;
        int other = 0;
        game_state = ST_LOBBY;
        warn_open = 0;
        studio_open = 0;
        t_dir = 0;
        t_fade = 0;
        language = 1;
        stub_login_status = 2;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "Дима");
        for (i = 0; i < 4000; i++) {
            bye_calls = 0;
            last_bye[0] = 0;
            game_bye();
            if (bye_calls == 0) {
                silent++;
                continue;
            }
            assert(bye_calls == 1);
            if (strcmp(last_bye, "Нам жаль что вы вышли, удачи!") == 0) {
                sorry++;
            } else if (strcmp(last_bye, "Пока, кубы будут вас ждать!") == 0) {
                cubes++;
            } else if (strcmp(last_bye, "Пока, Дима, удачи!") == 0) {
                nick_line++;
            } else {
                other++;
            }
        }
        assert(other == 0);
        /* A 50/50 roll over 4000 exits, with a wide band for the random draw. */
        assert(silent > 1700 && silent < 2300);
        assert(sorry > 400 && cubes > 400 && nick_line > 400);

        /* Without an account the nick line is never picked: no "Пока, , удачи!". */
        stub_login_nick[0] = 0;
        empty_nick = 0;
        for (i = 0; i < 4000; i++) {
            bye_calls = 0;
            last_bye[0] = 0;
            game_bye();
            if (bye_calls == 1 && strstr(last_bye, ", , ") != NULL) {
                empty_nick++;
            }
        }
        assert(empty_nick == 0);

        /* The English wording follows the language setting. */
        language = 0;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "Dima");
        other = 0;
        for (i = 0; i < 4000; i++) {
            bye_calls = 0;
            last_bye[0] = 0;
            game_bye();
            if (bye_calls == 0) {
                continue;
            }
            if (strcmp(last_bye, "Sorry that you left, good luck!") != 0 &&
                strcmp(last_bye, "Bye, the cubes will be waiting for you!") != 0 &&
                strcmp(last_bye, "Bye, Dima, good luck!") != 0) {
                other++;
            }
        }
        assert(other == 0);

        /* Back in the lobby closes the game, but the farewell itself is said by
         * the lifecycle hook (game_bye): one goodbye per exit, whichever way the
         * player leaves - back button, swipe-up gesture, swipe out of recents.
         * An ordinary back from a sub-screen only returns to the lobby. */
        language = 1;
        game_state = ST_LOBBY;
        bye_calls = 0;
        assert(logic_back() == 0);
        assert(bye_calls == 0);
        game_state = ST_SETTINGS;
        assert(logic_back() == 1);
        assert(bye_calls == 0);
        t_dir = 0;
        t_target = ST_LOBBY;
        game_state = ST_LOBBY;
    }

    /* The ordinary Astra class now always uses the fast ability kit. */
    assert(astra_ability_of(CLASS_ASTRA, SKIN_NORMAL) == 1);
    assert(astra_ability_of(CLASS_ASTRA, SKIN_SPECIAL) == 1);
    assert(astra_ability_of(CLASS_AZUM, SKIN_SPECIAL) == 0);
    assert(admin_slot("DIMASI4EK229") == 1);
    assert(admin_slot("QWERTYUIOPAJ1234") == 2);
    assert(admin_slot("regular_player") == 0);
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", ADMIN_PASS);
    astra_ability_refresh();
    assert(astra_ability_allowed() == 1);
    assert(astra_ability_is_dev() == 1);
    stub_login_pass[0] = 'x';
    stub_login_pass[1] = 0;
    astra_ability_refresh();
    assert(astra_ability_allowed() == 0);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", ADMIN_PASS);
    astra_ability_refresh();
    assert(astra_ability_allowed() == 1);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN2_NICK);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", ADMIN2_PASS);
    astra_ability_refresh();
    assert(astra_ability_allowed() == 1 && astra_ability_is_dev() == 1);
    stub_login_pass[0] = 'x';
    astra_ability_refresh();
    assert(astra_ability_allowed() == 0);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", ADMIN_PASS);
    astra_ability_refresh();
    assert(astra_ability_allowed() == 1);

    player_class = CLASS_ASTRA;
    player_level = 0;
    set_class_owned(CLASS_ASTRA, 1);
    assert(astra_ability_on() == 1 && astra_ability_local() == 1);
    assert(class_skin_of(CLASS_ASTRA) == SKIN_NORMAL && current_skin() == SKIN_NORMAL);
    assert(class_owned_of(CLASS_ASTRA) == 1 && class_visible(CLASS_ASTRA) == 1);
    assert(strcmp(class_name_of(CLASS_ASTRA), tr_class_astra()) == 0);
    assert(fabs(astra_ability_max_hp_for(CLASS_ASTRA, 1) - astra_hp * 0.95) < 1e-9);
    assert(astra_ability_max_hp_for(CLASS_ASTRA, 1) < astra_hp);
    assert(astra_ability_max_hp_for(CLASS_AZUM, 1) == azum_hp);
    assert(fabs(astra_ability_punch_damage_for(CLASS_ASTRA, 0, 1) - punch_damage * 0.93) < 1e-9);
    assert(astra_ability_punch_damage_for(CLASS_ASTRA, 0, 1) <
           astra_ability_punch_damage_for(CLASS_ASTRA, 0, 0));
    assert(fabs(astra_ability_punch_damage_net(CLASS_ASTRA, 0, SKIN_NORMAL) -
                astra_ability_punch_damage_for(CLASS_ASTRA, 0, 1)) < 1e-9);
    assert(fabs(class_hp_of(CLASS_ASTRA) - astra_hp * 0.95) < 1e-9);
    assert(fabs(class_power_of(CLASS_ASTRA) - punch_damage * 0.93) < 1e-9);
    classes_scroll = 0;
    codecracker_refresh();
    assert(codecracker_allowed() == 1 && codecracker_allowed() == astra_ability_allowed());
    assert(player_max_hp_for(CLASS_CODECRACKER) == 12 && codecracker_hp == 12);
    assert(codecracker_speed_mult == 3 && codecracker_touch_damage == 1.55 && codecracker_touch_heal == 0.63);
    assert(codecracker_super_time == 7 && codecracker_super_absorb == 0.5);
    assert(class_has_super(CLASS_CODECRACKER) == 1 && codecracker_super_active() == 0);
    /* The overclock ultimate: 7 seconds of triple speed and half damage,
     * then it is just an ordinary cube with a small HP pool again. */
    player_class = CLASS_CODECRACKER;
    codecracker_super_t = codecracker_super_time;
    assert(codecracker_super_active() == 1);
    assert(fabs(player_walk_speed() - joy_speed * codecracker_speed_mult * admin_cheat_speed_mult()) < 1e-6);
    {
        double hp_before = player->hp;
        player->max_hp = codecracker_hp;
        player->hp = codecracker_hp;
        take_damage(2);
        assert(fabs(player->hp - (codecracker_hp - 1)) < 1e-9); /* half absorbed */
        player->hp = hp_before;
    }
    codecracker_super_t = 0;
    assert(fabs(player_walk_speed() - joy_speed * admin_cheat_speed_mult()) < 1e-6);
    player_class = CLASS_ORDINARY;
    assert(class_owned_of(CLASS_CODECRACKER) == 1 && class_visible(CLASS_CODECRACKER) == 1);
    assert(visible_class_at(5) == CLASS_CODECRACKER);
    screen_w = 320;
    assert(classes_page_size() == 1 && classes_scroll_needed() == 1);
    assert(classes_scroll == 0 && visible_class_at(5) == CLASS_CODECRACKER);
    classes_scroll_by(100000);
    assert(classes_scroll == classes_scroll_max() && classes_scroll > 0);
    classes_scroll_by(-100000);
    assert(classes_scroll == 0);
    screen_w = 600;
    assert(classes_page_size() == 3 && classes_scroll_needed() == 1);
    assert(visible_class_at(5) == CLASS_CODECRACKER);
    classes_scroll = 0;
    screen_w = 900;
    assert(classes_page_size() == 5 && classes_scroll_needed() == 1);
    double swipe_y = classes_top() + class_card_h / 2;
    double swipe_x = classes_card_x(0) + classes_card_w() / 2;
    double selected_before_swipe = player_class;
    assert(classes_swipe_zone(swipe_x, swipe_y) == 1);
    touch_classes(swipe_x, swipe_y, 0, 7);
    touch_classes(swipe_x - 100, swipe_y, 2, 7);
    touch_classes(swipe_x - 100, swipe_y, 1, 7);
    assert(classes_scroll > 0 && player_class == selected_before_swipe);
    double scroll_after_drag = classes_scroll;
    swipe_x = classes_card_x(1) + classes_card_w() / 2;
    touch_classes(swipe_x, swipe_y, 0, 7);
    touch_classes(swipe_x + 100, swipe_y, 2, 7);
    touch_classes(swipe_x + 100, swipe_y, 1, 7);
    assert(classes_scroll < scroll_after_drag && player_class == selected_before_swipe);
    double had_astra_access = astra_ability_ok;
    astra_ability_ok = 0;
    codecracker_ok = 0;
    assert(classes_visible_count() == 4 && classes_scroll_needed() == 0); /* no Santa either: developer-only */
    assert(classes_swipe_zone(swipe_x, swipe_y) == 0);
    astra_ability_ok = had_astra_access;
    codecracker_refresh();
    classes_scroll = 0;
    screen_w = 320;
    player_class = CLASS_ASTRA;
    progress_normalize();
    assert(astra_ability_hit_interval_for(1) < astra_ability_hit_interval_for(0));
    /* the first three beats land sooner... */
    assert(astra_ability_beat_time_for(1, 1) < astra_ability_beat_time_for(1, 0));
    assert(astra_ability_beat_time_for(3, 1) < astra_ability_beat_time_for(3, 0));
    assert(astra_ability_beats_done_for(astra_ability_beat_time_for(3, 1), 1) == 3);
    /* ...and then the throw waits only ~0.4 s after the last beat, so the
     * whole ultimate reads as fast hits, a short pause, the throw. */
    assert(fabs(astra_ability_final_delay_for(1) - 0.4) < 1e-9);
    assert(fabs(astra_ability_final_time_for(1) - astra_ability_beat_time_for(3, 1) - 0.4) < 1e-9);
    assert(astra_ability_final_time_for(1) < astra_ability_final_time_for(0));
    assert(astra_ability_grab_time_for(1) < astra_ability_grab_time_for(0));
    assert(fabs(astra_ability_throw_dist_for(0, 1) - astra_throw_dist_for(0) * 1.10) < 1e-9);
    assert(astra_ability_throw_dist_for(0, 1) < astra_throw_dist_for(0) * 1.35);
    assert(astra_ability_throw_stun_for(0, 1) == astra_ability_throw_stun_for(0, 0));
    /* pressing the ultimate takes a small forward step: the grab origin is
     * the end of the step, while the body slides there smoothly frame by
     * frame instead of jumping in one go */
    player->angle = 0;
    player->size = 45;
    player->x = 150;
    player->y = 90;
    astra_punch_t = 0.05;
    start_grab_now();
    assert(astra_punch_t == 0);
    assert(fabs(player->x - 150) < 1e-9);
    assert(astra_x > 150 && astra_x - 150 <= astra_ability_lunge_dist + 1e-9);
    assert(fabs(astra_y - 90) < 1e-9);
    double lunge_prev = player->x;
    astra_ability_lunge_tick();
    assert(player->x > lunge_prev && player->x < astra_x);
    lunge_prev = player->x;
    int lunge_frames = 1;
    while (lunge_frames < 60 && player->x < astra_x) {
        astra_ability_lunge_tick();
        assert(player->x >= lunge_prev);
        lunge_prev = player->x;
        lunge_frames = lunge_frames + 1;
    }
    assert(fabs(player->x - astra_x) < 1e-9);
    assert(fabs(player->y - astra_y) < 1e-9);
    assert(lunge_frames * dt >= astra_ability_lunge_time);
    assert(astra_ability_zone_live_for(astra_ability_lunge_time / 2, 0, 1) == 0);
    assert(astra_ability_zone_live_for(astra_ability_lunge_time, 0, 1) == 1);
    astra_state = 0;
    astra_cd = 0;
    /* On the move the lunge still beats the walk: it lands a full step
     * further than walking would have carried her, and it bursts off faster
     * than walking pace from the very first frame. */
    {
        double saved_jdx = joy.dx;
        double saved_jdy = joy.dy;
        joy.dx = 1;
        joy.dy = 0;
        player->x = 150;
        player->y = 90;
        start_grab_now();
        assert(astra_x - 150 > player_walk_speed() * astra_ability_lunge_time + astra_ability_lunge_dist - 1e-6);
        double before = player->x;
        astra_ability_lunge_tick();
        assert((player->x - before) / dt > player_walk_speed());
        int k = 0;
        while (k < 60) {
            astra_ability_lunge_tick();
            k = k + 1;
        }
        assert(fabs(player->x - astra_x) < 1e-9);
        joy.dx = saved_jdx;
        joy.dy = saved_jdy;
        astra_state = 0;
        astra_cd = 0;
    }
    /* The grab/catch waits for the visual dash to finish; its first hit is
     * still scheduled before Astra's first beat. */
    player->x = 100;
    player->y = 90;
    player->hp = 10;
    enemy->hp = enemy->max_hp = 10;
    enemy_revive_prot = 0;
    enemy->y = player->y;
    game_state = ST_SOLO;
    start_grab_now();
    enemy->x = astra_x + 70;
    for (int frame = 0; frame < 9; frame++) {
        tick_astra();
        assert(astra_caught == 0 && astra_hits_done == 0);
    }
    assert(astra_caught == 0 && enemy->hp == 10);
    tick_astra();
    assert(astra_caught == 1 && astra_hits_done == 0);
    for (int frame = 10; frame < 22; frame++)
        tick_astra();
    assert(astra_hits_done == 0);
    tick_astra();
    assert(astra_hits_done == 1 && astra_punch_t > 0);
    astra_release(0);
    astra_cd = 0;
    /* Astra has no separate skin or developer variant: one ordinary card,
     * with the current fast ability kit. */
    player_class = CLASS_ASTRA;
    assert(class_has_skins(CLASS_ASTRA) == 0 && class_has_skins(CLASS_ASTRA) == 0);
    assert(skins_btn_visible() == 0 && current_skin() == SKIN_NORMAL);
    pick_skin(SKIN_SPECIAL);
    assert(strcmp(class_card_tex(CLASS_ASTRA, SKIN_NORMAL), ASTRA_CARD_TEX) == 0);
    assert(strcmp(class_card_tex(CLASS_ORDINARY, SKIN_NORMAL), ORDINARY_CARD_TEX) == 0);
    assert(fabs(class_hp_of(CLASS_ASTRA) - astra_hp * 0.95) < 1e-9);
    /* The card keeps only its description; no rating graphic is drawn. */
    /* No developer account still hides only developer-only classes. */
    stub_login_nick[0] = 'x';
    stub_login_nick[1] = 0;
    astra_ability_refresh();
    codecracker_refresh();
    assert(astra_ability_allowed() == 0 && codecracker_allowed() == 0);
    assert(class_visible(CLASS_ASTRA) == 1);
    assert(class_owned_of(CLASS_CODECRACKER) == 0 && class_visible(CLASS_CODECRACKER) == 0);
    assert(class_visible_index(CLASS_ASTRA) >= 0 && classes_visible_count() == 4);
    assert(class_visible_index(CLASS_CODECRACKER) == -1);
    assert(class_skin_of(CLASS_ASTRA) == SKIN_NORMAL && current_skin() == SKIN_NORMAL);
    /* Candies and Santa are only hidden while they are developer-only, never
     * taken away: an earlier balance and an earlier purchase survive in the
     * save, and both come back the moment access opens for everyone. */
    candies = 37;
    set_class_owned(CLASS_SANTA, 1);
    player_class = CLASS_SANTA;
    progress_normalize();
    assert(player_class == CLASS_ORDINARY); /* cannot play Santa for now */
    assert(candies == 37 && class_owned_of(CLASS_SANTA) == 1);
    assert(class_visible(CLASS_SANTA) == 0 && candies_on() == 0);
    astra_ability_ok = 1; /* the future "for everyone" update, via the dev gate */
    assert(class_visible(CLASS_SANTA) == 1 && candies_on() == 1 && candies == 37);
    select_class(CLASS_SANTA);
    assert(player_class == CLASS_SANTA);
    astra_ability_ok = 0;
    player_class = CLASS_ORDINARY;
    progress_normalize();
    assert(player_class == CLASS_ORDINARY);
    player->angle = 0;
    player->x = 150;
    player->y = 90;
    double plain_x = player->x;
    start_grab_now();
    assert(fabs(player->x - plain_x) < 1e-9);
    astra_state = 0;
    astra_cd = 0;
    player_class = CLASS_ORDINARY;

    stub_net_slot = -1;
    online_ready = 0;
    screen_w = 1080;
    screen_h = 1920;

    player_class = CLASS_ASTRA;
    player_level = 0;
    game_state = ST_SOLO;
    finished = 0;
    player->x = 400;
    player->y = 400;
    player->angle = 0;
    player->hp = player->max_hp = 10;
    enemy_class = CLASS_ORDINARY;
    enemy->x = 470;
    enemy->y = 400;
    enemy->hp = enemy->max_hp = 10;
    astra_cd = 0;
    start_grab_now();
    for (int frame = 0; frame < 400 && astra_state != 0; frame++)
        tick_astra();
    assert(fabs(enemy->hp - 5.5) < 1e-6);
    assert(enemy_throw_t > 0);

    astra_state = 0;
    enemy_throw_t = 0;
    player_class = CLASS_ORDINARY;
    game_state = ST_SOLO;
    finished = 0;
    player->x = 470;
    player->y = 400;
    player->hp = player->max_hp = 10;
    enemy_class = CLASS_ASTRA;
    enemy_level = 0;
    enemy->x = 400;
    enemy->y = 400;
    enemy->angle = 0;
    enemy->hp = enemy->max_hp = 10;
    pgrab_active = 0;
    pthrow_t = 0;
    enemy_start_grab();
    for (int frame = 0; frame < 400 && egrab_state != 0; frame++)
        tick_enemy_grab();
    assert(fabs(player->hp - 5.5) < 1e-6);
    assert(pthrow_t > 0);

    egrab_state = 0;
    pgrab_active = 0;
    pthrow_t = 0;
    game_state = ST_ONLINE;
    finished = 0;
    player->x = 470;
    player->y = 400;
    player->hp = player->max_hp = 10;
    stub_net_slot = 0;
    stub_online[1] = 1;
    stub_level[1] = 0;
    stub_gx[1] = 400.0 / screen_w;
    stub_gy[1] = 400.0 / screen_h;
    stub_gdx[1] = 1;
    stub_gdy[1] = 0;
    stub_grab[1] = 1;
    for (int frame = 0; frame < 400; frame++)
        update_remote_grabs();
    assert(fabs(player->hp - 5.5) < 1e-6);
    assert(pthrow_t > 0);
    double plain_throw = pthrow_speed * astra_throw_time;

    /* The same grab from an Astra attacker uses the class snapshot; the three
     * hits land sooner, the throw waits only ~0.4 s after the last beat and
     * goes farther; the total damage of the grab is unchanged. */
    pgrab_active = 0;
    pgrab_slot = -1;
    pthrow_t = 0;
    finished = 0;
    player->x = 470;
    player->y = 400;
    player->hp = player->max_hp = 10;
    arr_set(remotes, 1 * remote_fields + 10, CLASS_ASTRA);
    arr_set(remotes, 1 * remote_fields + 11, SKIN_SPECIAL);
    stub_grab[1] = 2;
    for (int frame = 0; frame < 9; frame++)
        update_remote_grabs();
    assert(pgrab_active == 0 && player->hp == 10);
    for (int frame = 9; frame < 45; frame++)
        update_remote_grabs();
    assert(player->hp < 10);
    assert(astra_ability_beat_time_for(3, 1) < astra_ability_beat_time_for(3, 0));
    assert(45 * dt > astra_ability_beat_time_for(3, 1) && 45 * dt < astra_ability_beat_time_for(3, 0) + 1e-9);
    for (int frame = 0; frame < 400 && pthrow_t <= 0; frame++)
        update_remote_grabs();
    assert(fabs(player->hp - 5.5) < 1e-6);
    assert(pthrow_t > 0);
    assert(pthrow_speed * astra_throw_time > plain_throw);
    assert(fabs(pthrow_speed * astra_throw_time - plain_throw * 1.10) < 1e-6);
    pgrab_active = 0;
    pgrab_slot = -1;
    pthrow_t = 0;
    stub_grab[1] = 0;
    arr_set(remotes, 1 * remote_fields + 10, CLASS_ORDINARY);
    arr_set(remotes, 1 * remote_fields + 11, SKIN_NORMAL);
    stub_net_slot = -1;

    player->hp = 1;
    game_state = ST_ONLINE;
    cups = 999;
    game_reset();
    assert(player->hp == 20);
    assert(game_state == ST_LOBBY);
    assert(cups == 0);

    /* --- admin panel -------------------------------------------------- */
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", ADMIN_PASS);
    assert(is_admin() == 1);
    screen_w = 1080;
    screen_h = 2400;
    game_state = ST_LOBBY;
    warn_open = 1; /* the epilepsy warning is up... */
    t_dir = 1;     /* ...and a screen transition is running */
    t_fade = 0.5;
    admin_ball_x = 0;
    admin_ball_y = 0;
    admin_panel_open = 0;
    stub_online[1] = 1;
    snprintf(stub_nicks[1], sizeof(stub_nicks[1]), "remote_guy");
    stub_online[2] = 1;
    snprintf(stub_nicks[2], sizeof(stub_nicks[2]), "%s", ADMIN2_NICK);
    stub_ban_n = 1;
    snprintf(stub_bans[0], sizeof(stub_bans[0]), "old_cheater");
    stub_last_chat[0] = 0;

    /* The orb is drawn even while the warning and the curtain are up, and it
     * is the last shape drawn, so nothing can cover it. */
    adm_reset_log();
    admin_draw();
    assert(admin_ball_x > 0 && admin_ball_y > 0);
    assert(fabs(adm_last_circle_x - admin_ball_x) < 0.001);
    assert(fabs(adm_last_circle_y - admin_ball_y) < 0.001);
    assert(fabs(adm_last_circle_r - admin_ball_r()) < 0.001);
    /* The menu button got smaller, wears the short panel name and is a black
     * disc with a white rim. */
    assert(admin_ball_r() >= 15 && admin_ball_r() <= 26);
    assert(strcmp(admin_orb_label(), "CCP") == 0);
    assert(strcmp(tr_admin_title(), "CONTENT CREATOR PANEL") == 0);
    /* Plain opaque black disc; the rim and the "CCP" are plain white. */
    assert((stub_circle_color & 0xFFFFFF) == 0);
    for (int i = 0; i < adm_color_n; i++) {
        uint32_t c = adm_colors[i];
        assert(((c >> 16) & 255) == ((c >> 8) & 255) && ((c >> 8) & 255) == (c & 255));
    }

    /* Tapping the orb opens the panel; it draws over the warning and the
     * transition instead of hiding itself. */
    assert(admin_touch(admin_ball_x, admin_ball_y, 0, 1) == 1);
    assert(admin_touch(admin_ball_x, admin_ball_y, 1, 1) == 1);
    assert(admin_panel_open == 1);
    adm_reset_log();
    admin_draw();
    assert(adm_rr_n > 0);
    assert(fabs(adm_rr_x - admin_panel_x()) < 0.001 && fabs(adm_rr_y - admin_panel_y()) < 0.001);
    assert(fabs(adm_rr_w - admin_panel_w()) < 0.001 && fabs(adm_rr_h - admin_panel_h()) < 0.001);
    assert(adm_all_greyscale() == 1);
    /* Flat and opaque: no button in the panel fades, glows or uses alpha. */
    assert(adm_all_flat() == 1);
    /* Compact, but a fifth wider than the old panel: no more full-screen panel. */
    assert(admin_panel_w() < screen_w * 0.75 && admin_panel_h() < screen_h * 0.55);
    /* The panel calls itself a content creator panel and the orb wears the
     * short name; the admin nick and the old "label: on/off" status line under
     * the panel are gone. */
    /* The title, close button, orb, two tabs and six cheat labels. */
    assert(adm_text_n == 11);
    assert(adm_has_text(tr_admin_title()) == 1);
    assert(adm_has_text(admin_orb_label()) == 1);
    assert(adm_has_text(language == 1 ? "Читы" : "Cheats") == 1);
    assert(adm_has_text(language == 1 ? "Подарки" : "Gifts") == 1);
    assert(adm_has_text("ADMIN") == 0);
    assert(adm_has_text(ADMIN_NICK) == 0);
    assert(adm_has_text(": ") == 0);
    for (int i = 0; i < 6; i++)
        assert(adm_has_text(admin_cheat_label(i)) == 1);

    /* Панель содержит отдельные вкладки читов и подарков. */
    adm_reset_log();
    admin_draw();
    assert(adm_all_greyscale() == 1);
    assert(adm_all_flat() == 1);
    for (int i = 0; i < 6; i++)
        assert(adm_has_text(admin_cheat_label(i)) == 1);
    /* The cheat grid starts below the tabs and remains inside the panel. */
    assert(fabs(admin_grid_top() - admin_content_top() - admin_row_h() - admin_gap()) < 0.001);
    assert(admin_grid_top() + 3 * admin_cheat_row_h() + 2 * admin_gap() <=
               admin_panel_y() + admin_panel_h() - admin_pad() + 0.01);

    /* Клавиатура отдаёт текст в буфер самого поля: два разных поля не обязаны
     * делить одну строку на всех. */
    stub_kb_set("typed_nick");
    {
        char fa[32];
        char fb[32];
        keyboard_get_text_to(fa, sizeof(fa));
        keyboard_get_text_to(fb, sizeof(fb));
        assert(strcmp(fa, "typed_nick") == 0 && strcmp(fb, "typed_nick") == 0);
        assert(&fa[0] != &fb[0]);
    }
    stub_kb_set("");

    /* The compact layout has to hold on every screen size. */
    double sizes[4][2] = {{360, 640}, {720, 1280}, {1080, 2400}, {1440, 3200}};
    for (int k = 0; k < 4; k++) {
        screen_w = sizes[k][0];
        screen_h = sizes[k][1];
        assert(admin_panel_w() <= screen_w - 16);
        assert(admin_panel_h() <= screen_h - 16);
        assert(admin_panel_x() >= 0 && admin_panel_y() >= 0);
        assert(admin_panel_w() <= screen_w - 16 && admin_panel_h() <= screen_h - 16);
        assert(admin_panel_w() < screen_w * 0.75 && admin_panel_h() < screen_h * 0.55);
        /* the panel is a fifth wider than the old one: 20% up */
        assert(admin_panel_w() >= screen_w * 0.58);
        assert(admin_content_top() < admin_panel_y() + admin_panel_h() - admin_pad());
        /* six cheat buttons always fit between the header and the bottom edge */
        assert(admin_grid_top() + 3 * admin_cheat_row_h() + 2 * admin_gap() <=
               admin_panel_y() + admin_panel_h() - admin_pad() + 0.01);
        assert(admin_cheat_row_h() >= 26);
        adm_reset_log();
        admin_draw();
        assert(adm_all_greyscale() == 1);
        assert(adm_all_flat() == 1);
    }
    screen_w = 1080;
    screen_h = 2400;

    /* Any cheat switched on stains the battle: the win pays no Cups, and the
     * mark stays even when the switch goes straight back off. */
    net_ban_set("typed_cheater", 0);
    assert(cheat_god == 0 && cheat_stain == 0);
    admin_press(0); /* god mode on */
    assert(cheat_god == 1 && cheat_stain == 1);
    /* The enabled switch - like the selected tab - only turns grey: the panel
     * never paints a white button, and the fill count says what is on. */
    adm_reset_log();
    admin_draw();
    assert(adm_no_white_fill() == 1);
    assert(adm_fill_count(0x333333) == 2); /* selected tab and enabled cheat */
    admin_press(0); /* ...and straight back off */
    assert(cheat_god == 0 && cheat_stain == 1);
    adm_reset_log();
    admin_draw();
    assert(adm_no_white_fill() == 1);
    assert(adm_fill_count(0x333333) == 1); /* only selected tab remains grey */
    assert(adm_has_text(admin_cheat_label(0)) == 1);
    assert(admin_cups_allowed() == 0);
    cups = 0;
    cup_win = 50;
    player->hp = player->max_hp;
    player_was_hit = 0;
    award_battle_result(1);
    assert(cups == 0);
    /* A fresh battle with every switch off pays normally again. */
    admin_battle_stain_reset();
    assert(admin_cups_allowed() == 1);
    cups = 0;
    award_battle_result(1);
    assert(cups == 50);
    /* Entering a battle with a cheat already on stains it as well. */
    cheat_stain = 0;
    cheat_speed = 1;
    admin_battle_stain_reset();
    assert(admin_cups_allowed() == 0);
    cheat_speed = 0;
    admin_battle_stain_reset();
    assert(admin_cups_allowed() == 1);
    cups = 0;

    net_ban_set("remote_guy", 0);
    net_ban_set("old_cheater", 0);
    stub_online[1] = 0;
    stub_online[2] = 0;
    stub_nicks[1][0] = 0;
    stub_nicks[2][0] = 0;
    game_state = ST_LOBBY;
    warn_open = 0;
    t_dir = 0;
    admin_panel_close();
    admin_panel_open = 0;
    screen_w = 320;
    screen_h = 180;

    /* Buttons fire on the press itself: the click, the squeeze and the action
     * all happen on touch down, and the squeeze springs back on its own - the
     * finger never has to lift, stay put or be let go gently. Nothing is
     * fired twice by a drag, and the shop follows the same rule for its pill,
     * while the card body waits for the release so a swipe can still page. */
    {
        int saved_h = screen_h;
        screen_h = 2400;
        warn_open = 0;
        studio_open = 0;
        game_state = ST_LOBBY;
        t_dir = 0;
        t_target = 0;

        /* Press the Play row: it goes on the touch itself. */
        double tx = menu_x() + btn_w / 2;
        double ty = menu_row(0) + btn_h / 2;
        logic_touch(tx, ty, 0, 1);
        assert(t_dir == 1 && t_target == ST_MODES);
        /* The squeeze is claimed by the button the draw passes under it. */
        assert(press_new == 1);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_new == 0 && press_phase == 1);
        update_button_press();
        assert(press_k > 0 && press_k < 1); /* easing in */
        assert(button_press_k(menu_x(), menu_row(0), btn_w, btn_h) > 0);
        int n = 0;
        while (n < 3) {
            update_button_press();
            n = n + 1;
        }
        assert(press_k > 0.95); /* fully squeezed and darkened */
        /* The finger is still down - and the button springs back anyway. */
        n = 0;
        while (n < 30) {
            update_button_press();
            n = n + 1;
        }
        assert(press_k == 0 && press_phase == 0);
        assert(button_press_k(menu_x(), menu_row(0), btn_w, btn_h) == 0);
        /* Letting go changes nothing: the row already fired. */
        logic_touch(tx + 4, ty, 2, 1);
        logic_touch(tx + 4, ty, 1, 1);
        assert(t_dir == 1 && t_target == ST_MODES);
        t_dir = 0;

        /* A drag off a pressed button does not fire it a second time, and a
         * release in the middle of nowhere fires nothing at all. */
        t_target = 0;
        logic_touch(tx, ty, 0, 1);
        assert(t_dir == 1 && t_target == ST_MODES);
        logic_touch(tx + 200, ty, 2, 1);
        logic_touch(tx + 200, ty, 1, 1);
        assert(t_target == ST_MODES);
        t_dir = 0;
        t_target = 0;
        logic_touch(menu_x() + btn_w / 2, menu_row(0) - 200, 0, 1);
        assert(t_dir == 0);
        logic_touch(menu_x() + btn_w / 2, menu_row(0) - 200, 1, 1);
        assert(t_dir == 0);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase == 0);

        /* The NPC cube opens its dialogue on the press as well. */
        cube_npc_d0 = "d0";
        cube_npc_d1 = "d1";
        cube_npc_d2 = "d2";
        cube_npc_d3 = "d3";
        cube_npc_d4 = "d4";
        draw_lobby_npc_cube();
        logic_touch(cube_npc_x + 30, cube_npc_y + 30, 0, 1);
        assert(cube_npc_dialogue_active == 1);
        /* Tapping the screen outside the dialogue is its skip action: the talk
         * advances and the menu rows behind it never fire. */
        double ch0 = cube_npc_dialogue_chars;
        logic_touch(tx, ty, 0, 1);
        assert(t_dir == 0); /* the Play row did not fire */
        assert(cube_npc_dialogue_active == 1 && cube_npc_dialogue_chars > ch0);
        n = 0;
        while (cube_npc_dialogue_active == 1 && n < 20) {
            logic_touch(tx, ty, 0, 1);
            n = n + 1;
        }
        assert(cube_npc_dialogue_active == 0 && t_dir == 0);
        cube_npc_dialogue_line = 0;
        cube_npc_dialogue_chars = 0;

        /* The shop buttons fire on the press; a drag no longer cancels them. */
        game_state = ST_CLASSES;
        player_class = CLASS_ORDINARY;
        double lx = levels_btn_x() + class_action_w() / 2;
        double ly = class_actions_y() + btn_h / 2;
        logic_touch(lx, ly, 0, 2);
        assert(t_dir == 1 && t_target == ST_LEVELS);
        t_dir = 0;
        t_target = 0;
        logic_touch(lx, ly, 0, 2);
        logic_touch(lx + 200, ly, 2, 2);
        logic_touch(lx + 200, ly, 1, 2);
        assert(t_dir == 1 && t_target == ST_LEVELS);
        t_dir = 0;

        /* One shop card, sold once: the pill buys on the press, the rest of
         * the card on the release, and a swipe pages instead of buying. */
        cups = 1000;
        classes_scroll = 0; /* the second card: Azum, 65 cups */
        double v0 = 1;
        double cls = visible_class_at(v0);
        assert(cls != CLASS_ORDINARY && class_owned_of(cls) == 0);
        double card_x = classes_card_x(v0) + classes_card_w() / 2;
        double card_y = classes_top() + class_card_h / 2;
        double pill_x = classes_card_x(v0) + class_pick_inset() + class_pick_w() / 2;
        double pill_y = class_pick_y() + class_pick_h / 2;

        /* The pill is a button: it buys the moment the finger lands. */
        logic_touch(pill_x, pill_y, 0, 3);
        assert(class_owned_of(cls) == 1 && player_class == cls && cups == 1000 - class_cost_of(cls));
        /* The squeeze of a press on the pill belongs to the whole card. */
        assert(press_new == 1);
        (void)button_press_k(classes_card_x(v0), classes_top(), classes_card_w(), class_card_h);
        assert(press_new == 0 && press_phase == 1);
        n = 0;
        while (n < 3) {
            update_button_press();
            n = n + 1;
        }
        assert(button_press_k(classes_card_x(v0), classes_top(), classes_card_w(), class_card_h) > 0.8);
        assert(button_press_k(classes_card_x(v0) + class_pick_inset(), class_pick_y(), class_pick_w(),
                              class_pick_h) == 0);
        n = 0;
        while (n < 30) {
            update_button_press();
            n = n + 1;
        }
        assert(press_k == 0 && press_phase == 0);

        /* The card body takes the press as feedback only: nothing is bought
         * until the finger lifts on the same card. */
        set_class_owned(cls, 0);
        player_class = CLASS_ORDINARY;
        cups = 1000;
        logic_touch(card_x, card_y, 0, 4);
        assert(class_owned_of(cls) == 0 && cups == 1000);
        (void)button_press_k(classes_card_x(v0), classes_top(), classes_card_w(), class_card_h);
        assert(press_phase == 1); /* the card itself is what darkens */
        n = 0;
        while (n < 30) {
            update_button_press();
            n = n + 1;
        }
        assert(class_owned_of(cls) == 0);
        logic_touch(card_x, card_y, 1, 4);
        assert(class_owned_of(cls) == 1 && player_class == cls && cups == 1000 - class_cost_of(cls));

        /* A sideways drag over a card scrolls the strip instead of buying -
         * provided the strip overflows at all in this test's screen size. */
        set_class_owned(cls, 0);
        player_class = CLASS_ORDINARY;
        cups = 1000;
        classes_scroll = 0;
        logic_touch(card_x, card_y, 0, 5);
        logic_touch(card_x - classes_swipe_threshold() - 10, card_y, 2, 5);
        assert(classes_scroll == classes_scroll_needed() * (classes_swipe_threshold() + 10) ||
               classes_scroll == classes_scroll_max());
        logic_touch(card_x - classes_swipe_threshold() - 10, card_y, 1, 5);
        assert(class_owned_of(cls) == 0 && cups == 1000);
        classes_scroll = 0;

        /* Button hitboxes are exactly the drawn shape, and the squeeze is
         * claimed by the very same test: a press either fires and squeezes,
         * or does neither. */
        game_state = ST_LOBBY;
        press_phase = 0;
        press_k = 0;
        press_new = 0;
        t_dir = 0;
        t_target = 0;
        /* Just beside the button (the old invisible touch margin): nothing. */
        logic_touch(menu_x() - 6, menu_row(0) + btn_h / 2, 0, 6);
        assert(t_dir == 0);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase == 0);
        press_new = 0;
        logic_touch(menu_x() + btn_w / 2, menu_row(0) + btn_h + 4, 0, 6);
        assert(t_dir == 0);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase == 0);
        press_new = 0;
        /* The transparent rounded corner is not part of the button either. */
        logic_touch(menu_x() + 2, menu_row(0) + 2, 0, 6);
        assert(t_dir == 0);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase == 0);
        press_new = 0;
        /* The very edge of the visible shape fires - and squeezes. */
        logic_touch(menu_x() + btn_radius, menu_row(0) + 1, 0, 6);
        assert(t_dir == 1 && t_target == ST_MODES);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase == 1);
        n = 0;
        while (n < 30) {
            update_button_press();
            n = n + 1;
        }
        t_dir = 0;
        t_target = 0;
        /* The shape test itself: rounded corners and round pads. */
        assert(hit_round(3, 3, 0, 0, 100, 40, 20) == 0);
        assert(hit_round(20, 1, 0, 0, 100, 40, 20) == 1);
        assert(hit_round(50, 20, 0, 0, 100, 40, 20) == 1);
        assert(hit_round(101, 20, 0, 0, 100, 40, 20) == 0);
        assert(hit_round(110, 110, 0, 0, 120, 120, 60) == 0);
        assert(hit_round(60 + 59, 60, 0, 0, 120, 120, 60) == 1);

        /* A quick second tap while the first squeeze is still springing back
         * gets its own squeeze instead of firing silently. */
        logic_touch(menu_x() + btn_w / 2, menu_row(0) + btn_h / 2, 0, 7);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        n = 0;
        while (press_phase != 2 && n < 30) {
            update_button_press();
            n = n + 1;
        }
        update_button_press();
        assert(press_phase == 2);
        t_dir = 0;
        press_new = 1; /* the second tap */
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase == 1 && press_new == 0);
        n = 0;
        while (n < 30) {
            update_button_press();
            n = n + 1;
        }
        assert(press_phase == 0);

        /* A touch that lands mid-transition is ignored, so it squeezes nothing. */
        t_dir = 1;
        logic_touch(menu_x() + btn_w / 2, menu_row(1) + btn_h / 2, 0, 8);
        assert(press_new == 0);
        t_dir = 0;
        /* Another finger sliding does not drag the press point away. */
        logic_touch(menu_x() + btn_w / 2, menu_row(1) + btn_h / 2, 0, 9);
        t_dir = 0;
        t_target = 0;
        logic_touch(5, 5, 2, 10);
        assert(press_x == menu_x() + btn_w / 2 && press_y == menu_row(1) + btn_h / 2);
        press_new = 0;

        game_state = ST_LOBBY;
        screen_h = saved_h;
    }

    /* The old theme is back as a settings toggle (row 4): rose colours, a
     * left-aligned menu and no NPC cube; tapping again brings the new one. */
    {
        assert(new_theme == 1);
        assert(menu_x() == center_x(btn_w));
        game_state = ST_SETTINGS;
        t_dir = 0;
        t_target = 0;
        logic_touch(menu_x() + menu_btn_w() / 2, settings_row_y(4) + settings_btn_h() / 2, 0, 20);
        assert(new_theme == 0 && t_dir == 0);
        assert(ui_purple == 0xFFD4508A && bg_screen == 0xFF3A1530);
        assert(menu_x() == screen_margin);
        press_new = 0;
        logic_touch(menu_x() + menu_btn_w() / 2, settings_row_y(4) + settings_btn_h() / 2, 0, 21);
        assert(new_theme == 1 && ui_purple == 0xFF5F10A0);
        press_new = 0;
        /* Row 5 opens the legal documents window over the settings. */
        logic_touch(menu_x() + menu_btn_w() / 2, settings_row_y(5) + settings_btn_h() / 2, 0, 22);
        assert(t_dir == 1 && docs_open == 0);
        finish_transition();
        assert(docs_open == 1 && docs_page == 0 && docs_ask == 0);
        assert(logic_back() == 1 && docs_open == 1);
        finish_transition();
        assert(docs_open == 0 && game_state == ST_SETTINGS);
        press_new = 0;

        /* The promo code sits in the bottom-right corner of the modes screen
         * (on a landscape phone-sized screen, as the game runs). */
        double saved_mw = screen_w;
        double saved_mh = screen_h;
        screen_w = 1280;
        screen_h = 720;
        game_state = ST_MODES;
        assert(promo_corner_x() + promo_corner_w() == screen_w - screen_margin);
        assert(promo_corner_y() + btn_h == screen_h - screen_margin);
        assert(promo_corner_x() > menu_x() + menu_btn_w());
        logic_touch(promo_corner_x() + promo_corner_w() / 2, promo_corner_y() + btn_h / 2, 0, 23);
        assert(t_dir == 1 && t_target == ST_PROMO);
        t_dir = 0;
        t_target = 0;

        /* Промокод 2027: +27 кубков, один раз; мусорный код ничего не даёт. */
        {
            stub_login_status = 2;
            game_state = ST_PROMO;
            double cups_before = cups;
            promo_focus = 0;
            promo_input = "2027";
            promo_submit();
            assert(cups == cups_before + 27 && stub_promo2027_used == 1);
            promo_submit();
            assert(cups == cups_before + 27);
            promo_input = "1234";
            promo_submit();
            assert(cups == cups_before + 27);
            stub_login_status = 0;
            game_state = ST_MODES;
        }
        press_new = 0;
        /* Row 4 is the winter toggle: grey and dead without developer
         * access, a working toggle with it. */
        double saved_winter2 = winter_theme;
        double saved_dev = astra_ability_ok;
        astra_ability_ok = 0;
        logic_touch(menu_x() + menu_btn_w() / 2, modes_row(4) + btn_h / 2, 0, 24);
        assert(winter_theme == saved_winter2 && t_dir == 0);
        press_new = 0;
        astra_ability_ok = 1;
        logic_touch(menu_x() + menu_btn_w() / 2, modes_row(4) + btn_h / 2, 0, 25);
        assert(winter_theme == 1 - saved_winter2 && t_dir == 0);
        press_new = 0;
        logic_touch(menu_x() + menu_btn_w() / 2, modes_row(4) + btn_h / 2, 0, 26);
        assert(winter_theme == saved_winter2);
        astra_ability_ok = saved_dev;
        press_new = 0;
        screen_w = saved_mw;
        screen_h = saved_mh;
        game_state = ST_LOBBY;
    }

    test_legal();
    test_start_gate();
    test_promo_message();
    test_azum_buff();
    test_class_stars();
    test_codecracker_fx();
    test_eax229_and_rooms();
    test_dice_alone_and_bubbles();
    test_overlay_transitions();

    /* New Year battle pass: wins give XP, tiers claim in order for
     * candies; tier 4 unlocks the Elf Ebuc skin, tier 7 grants Santa. */
    {
        bp_xp = 0;
        bp_claimed = 0;
        candies = 1000;
        cups = 0;
        stub_bpx = 0;
        award_battle_result(1);
        assert(bp_xp == 10 && stub_bpx == 10);
        double cups_after_win = cups;
        assert(skin_unlocked_for(CLASS_EBUC, SKIN_SPECIAL) == 0);
        bp_claim(2); /* out of order: nothing happens */
        assert(bp_claimed == 0 && candies == 1000);
        bp_claim(1);
        assert(bp_claimed == 1 && cups == cups_after_win + 10 && candies == 1000 - 12);
        bp_xp = 300;
        bp_claim(2);
        bp_claim(3);
        assert(bp_claimed == 3 && skin_unlocked_for(CLASS_EBUC, SKIN_SPECIAL) == 0);
        bp_claim(4);
        assert(bp_claimed == 4 && skin_unlocked_for(CLASS_EBUC, SKIN_SPECIAL) == 1);
        assert(class_owned_of(CLASS_SANTA) == 0);
        bp_claim(5);
        bp_claim(6);
        bp_claim(7);
        assert(bp_claimed == 7 && class_owned_of(CLASS_SANTA) == 1);
        assert(class_visible(CLASS_SANTA) == 1);
        bp_xp = 0;
        bp_claimed = 0;
        stub_bpx = 0;
    }

    state_destroy();
    state_ready = 0;
    puts("Состояние полной C-версии: норма");
    return 0;
}
