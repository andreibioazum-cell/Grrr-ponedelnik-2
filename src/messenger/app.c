#include "messenger/app.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define EVENT_CAP 32
#define CHAT_CAP 32
#define CHAT_LINES 6
#define TEXT_CAP 512
#define ICON "icons/ponedelnik_messenger.png"

typedef struct { int type; char peer[40]; char data[TEXT_CAP]; } AppEvent;
typedef struct { int mine; unsigned line_count; char lines[CHAT_LINES][TEXT_CAP]; } ChatLine;

static pthread_mutex_t event_lock = PTHREAD_MUTEX_INITIALIZER;
static AppEvent events[EVENT_CAP];
static unsigned event_head, event_tail;
static ChatLine chat[CHAT_CAP];
static unsigned chat_count;
static int page, focus, network_started, peer_connected, peer_verified, incoming;
static double notice_t, anim_t;
static char server_url[256] = "http://10.0.2.2:8765";
static char nickname[128] = "";
static char own_id[40] = "";
static char own_fingerprint[80] = "";
static char peer_id[40] = "";
static char peer_name[128] = "";
static char peer_fingerprint[80] = "";
static char incoming_id[40] = "";
static char incoming_name[128] = "";
static char incoming_fingerprint[80] = "";
static char message_text[TEXT_CAP] = "";
static char verify_text[80] = "";
static char status_text[128] = "Сигналинг не подключён";
static char notice_text[160] = "";

static float sc(void) { return screen_w > 0 ? (float)screen_w / 540.0f : 1.0f; }
static float u(float value) { return value * sc(); }
static float margin(void) { return u(20); }
static float content_w(void) { return screen_w - margin() * 2; }
static void note(const char *s) {
    snprintf(notice_text, sizeof(notice_text), "%s", s ? s : "");
    notice_t = 3.0;
}
static void copy_str(char *dst, size_t cap, const char *src) {
    if (!dst || !cap) return;
    snprintf(dst, cap, "%s", src ? src : "");
}
static void split_peer_info(const char *value, char *fingerprint, size_t fingerprint_cap,
                            char *name, size_t name_cap) {
    const char *separator = value ? strchr(value, '\n') : NULL;
    if (!separator) {
        copy_str(fingerprint, fingerprint_cap, value);
        if (name && name_cap) name[0] = 0;
        return;
    }
    size_t n = (size_t)(separator - value);
    if (n >= fingerprint_cap) n = fingerprint_cap - 1;
    memcpy(fingerprint, value, n);
    fingerprint[n] = 0;
    copy_str(name, name_cap, separator + 1);
}
#include "messenger/chat_layout.inc"
static void queue_event(int type, const char *peer, const char *data) {
    unsigned next;
    pthread_mutex_lock(&event_lock);
    next = (event_head + 1u) % EVENT_CAP;
    if (next == event_tail) event_tail = (event_tail + 1u) % EVENT_CAP;
    events[event_head].type = type;
    copy_str(events[event_head].peer, sizeof(events[event_head].peer), peer);
    copy_str(events[event_head].data, sizeof(events[event_head].data), data);
    event_head = next;
    pthread_mutex_unlock(&event_lock);
}
static int dequeue_event(AppEvent *out) {
    int have = 0;
    pthread_mutex_lock(&event_lock);
    if (event_tail != event_head) {
        *out = events[event_tail];
        event_tail = (event_tail + 1u) % EVENT_CAP;
        have = 1;
    }
    pthread_mutex_unlock(&event_lock);
    return have;
}
static void add_line(int mine, const char *body) {
    unsigned i;
    if (chat_count == CHAT_CAP) {
        for (i = 1; i < CHAT_CAP; i++) chat[i - 1] = chat[i];
        chat_count--;
    }
    chat[chat_count].mine = mine;
    wrap_chat_text(&chat[chat_count], body);
    chat_count++;
}
static void apply_event(const AppEvent *e) {
    if (e->type == PND_EVENT_IDENTITY) {
        copy_str(own_id, sizeof(own_id), e->peer);
        copy_str(own_fingerprint, sizeof(own_fingerprint), e->data);
        network_started = 1;
        copy_str(status_text, sizeof(status_text), "Сигналинг готов · сообщения не проходят через сервер");
    } else if (e->type == PND_EVENT_STATUS) {
        copy_str(status_text, sizeof(status_text), e->data);
    } else if (e->type == PND_EVENT_INCOMING) {
        copy_str(incoming_id, sizeof(incoming_id), e->peer);
        split_peer_info(e->data, incoming_fingerprint, sizeof(incoming_fingerprint),
                        incoming_name, sizeof(incoming_name));
        incoming = 1;
    } else if (e->type == PND_EVENT_PEER_INFO) {
        copy_str(peer_id, sizeof(peer_id), e->peer);
        split_peer_info(e->data, peer_fingerprint, sizeof(peer_fingerprint), peer_name, sizeof(peer_name));
        peer_connected = 0;
        peer_verified = 0;
        page = 1;
    } else if (e->type == PND_EVENT_CONNECTED) {
        copy_str(peer_id, sizeof(peer_id), e->peer);
        split_peer_info(e->data, peer_fingerprint, sizeof(peer_fingerprint), peer_name, sizeof(peer_name));
        peer_connected = 1;
        copy_str(status_text, sizeof(status_text), "Прямое P2P-соединение установлено");
    } else if (e->type == PND_EVENT_MESSAGE) {
        if (!peer_id[0] || strcmp(peer_id, e->peer) == 0) {
            copy_str(peer_id, sizeof(peer_id), e->peer);
            add_line(0, e->data);
            page = 1;
        }
    } else if (e->type == PND_EVENT_SENT) {
        add_line(1, e->data);
    } else if (e->type == PND_EVENT_VERIFIED) {
        peer_verified = 1;
        note("Ключ собеседника закреплён на этом устройстве");
    } else if (e->type == PND_EVENT_ERROR) {
        copy_str(status_text, sizeof(status_text), e->data);
        note(e->data);
    }
}
static void sync_field(void) {
    if (!keyboard_visible()) return;
    if (focus == 1) keyboard_get_text_to(server_url, sizeof(server_url));
    else if (focus == 2) keyboard_get_text_to(nickname, sizeof(nickname));
    else if (focus == 3) keyboard_get_text_to(peer_id, sizeof(peer_id));
    else if (focus == 4) keyboard_get_text_to(message_text, sizeof(message_text));
    else if (focus == 5) keyboard_get_text_to(verify_text, sizeof(verify_text));
}
static void focus_field(int which, char *value) {
    sync_field();
    focus = which;
    keyboard_clear();
    if (value && value[0]) keyboard_type(value);
    keyboard_show();
}
static void blur_keyboard(void) { keyboard_hide(); keyboard_clear(); focus = 0; }
static void start_network(void) {
    sync_field();
    if (!server_url[0]) { note("Введи URL сервера сигналинга"); return; }
    if (!nickname[0]) copy_str(nickname, sizeof(nickname), "Guest");
    blur_keyboard();
    copy_str(status_text, sizeof(status_text), "Подключаемся к сигналингу…");
    messenger_android_start(server_url, nickname);
}
static void connect_peer(const char *id) {
    sync_field();
    if (!network_started) { note("Сначала подключись к сигналингу"); return; }
    if (!id || !id[0]) { note("Введи ID собеседника"); return; }
    copy_str(peer_id, sizeof(peer_id), id);
    peer_name[0] = 0;
    peer_fingerprint[0] = 0;
    blur_keyboard();
    page = 1;
    peer_connected = peer_verified = 0;
    incoming = 0;
    chat_count = 0;
    copy_str(status_text, sizeof(status_text), "Запрашиваем прямое соединение…");
    messenger_android_connect(peer_id);
}
static void send_message(void) {
    sync_field();
    if (!message_text[0]) return;
    if (!peer_connected) { note("Собеседник пока не подключён напрямую"); return; }
    if (!peer_verified) { note("Сначала сверь и подтверди отпечаток ключа"); return; }
    messenger_android_send(peer_id, message_text);
    copy_str(message_text, sizeof(message_text), "");
    blur_keyboard();
}
static void draw_button(const char *label, float x, float y, float w, float h, int bright) {
    roundrect(x, y, w, h, u(18), bright ? 0xFF55D8C2u : 0xFF242B45u);
    text_scaled(label, x + (w - text_ink_width(label) * u(0.48f)) * 0.5f,
                y + h * 0.32f, 0xFFFFFFFFu, u(0.48f));
}
static void draw_field(const char *label, const char *value, const char *hint, float x, float y, float w, int field) {
    text_scaled(label, x + u(4), y - u(21), 0xFFB7C3D9u, u(0.38f));
    roundrect(x, y, w, u(48), u(14), focus == field ? 0xFF2C3857u : 0xFF171D32u);
    const char *s = value && value[0] ? value : hint;
    char shown[TEXT_CAP];
    snprintf(shown, sizeof(shown), "%s%s", s ? s : "", focus == field && keyboard_visible() ? "|" : "");
    text_scaled(shown, x + u(14), y + u(13), value && value[0] ? 0xFFFFFFFFu : 0xFF7D8AA4u, u(0.42f));
}
static void draw_backdrop(void) {
    rect(0, 0, (float)screen_w, (float)screen_h, 0xFF0B1020u);
    circle(screen_w * 0.84f, u(80), u(150), 0xFF111C35u);
    circle(screen_w * 0.05f, screen_h * 0.88f, u(190), 0xFF111A30u);
}
static void draw_header(int chat_page) {
    float s = sc(), x = margin();
    roundrect(x, u(18), content_w(), u(74), u(22), 0xFF151B30u);
    tex_round(x + u(38), u(55), ICON, u(48) / 1024.0f, u(12));
    if (chat_page) {
        roundrect(x + u(76), u(31), u(42), u(42), u(14), 0xFF252D46u);
        text_scaled("‹", x + u(91), u(31), 0xFFFFFFFFu, u(0.9f));
        text_scaled(peer_name[0] ? peer_name : peer_id, x + u(132), u(31), 0xFFFFFFFFu, u(0.54f));
        text_scaled(peer_connected ? "DIRECT P2P" : "CONNECTING", x + u(132), u(59), 0xFF60D9C3u, u(0.32f));
    } else {
        text_scaled("Ponedelnik", x + u(76), u(30), 0xFFFFFFFFu, u(0.64f));
        text_scaled("PEER-TO-PEER CHAT", x + u(77), u(58), 0xFF98A6C0u, u(0.32f));
        circle(screen_w - x - u(27), u(55), u(6), network_started ? 0xFF60D9C3u : 0xFF67718A);
    }
    (void)s;
}
static void draw_home(void) {
    float x = margin(), w = content_w(), y = u(112), h = u(248);
    draw_header(0);
    roundrect(x, y, w, h, u(24), 0xFF151B30u);
    text_scaled("Прямой чат", x + u(20), y + u(18), 0xFFFFFFFFu, u(0.62f));
    text_scaled("Текст передаётся только между устройствами.", x + u(20), y + u(52), 0xFFB7C3D9u, u(0.39f));
    draw_field("Сервер только для сигналинга · локальный адрес", server_url, "http://10.0.2.2:8765", x + u(16), y + u(96), w - u(32), 1);
    draw_field("Имя", nickname, "Например, Alex", x + u(16), y + u(160), w - u(32), 2);
    draw_button(network_started ? "Переподключить" : "Подключить сигналинг", x + u(16), y + u(210), w - u(32), u(44), 1);
    text_scaled(status_text, x + u(18), y + h + u(12), 0xFF91A0BAu, u(0.34f));

    y = u(396);
    roundrect(x, y, w, u(112), u(22), 0xFF151B30u);
    text_scaled("ВАШ PEER ID", x + u(18), y + u(14), 0xFF98A6C0u, u(0.34f));
    text_scaled(own_id[0] ? own_id : "Подключитесь, чтобы получить ID", x + u(18), y + u(38), 0xFFFFFFFFu, u(0.47f));
    text_scaled(own_fingerprint[0] ? own_fingerprint : "Ключ устройства появится здесь", x + u(18), y + u(70), 0xFF61D8C2u, u(0.35f));
    draw_button("Поделиться ID", x + w - u(138), y + u(58), u(120), u(38), 0);

    y = u(526);
    roundrect(x, y, w, incoming ? u(224) : u(192), u(22), 0xFF151B30u);
    text_scaled("Начать P2P-чат", x + u(18), y + u(15), 0xFFFFFFFFu, u(0.55f));
    draw_field("Peer ID собеседника", peer_id, "32-символьный ID", x + u(16), y + u(61), w - u(32), 3);
    draw_button("Соединиться напрямую", x + u(16), y + u(116), w - u(32), u(44), 1);
    if (incoming) {
        char line[96];
        snprintf(line, sizeof(line), "Запрос от %s", incoming_name[0] ? incoming_name : incoming_id);
        text_scaled(line, x + u(18), y + u(170), 0xFFFFD68A, u(0.37f));
        draw_button("Принять запрос", x + u(16), y + u(190), w - u(32), u(38), 0);
    }
    text_scaled("Сигналинг/STUN видит IP и ID, но не сообщения.\n"
                "Без TURN: некоторые NAT-сети не соединятся.", x + u(6), y + u(incoming ? 238 : 206),
                0xFF77849D, u(0.32f));
}
static void draw_chat(void) {
    float x = margin(), w = content_w(), bottom = (float)screen_h - u(126);
    unsigned i;
    draw_header(1);
    roundrect(x, u(108), w, u(112), u(20), 0xFF151B30u);
    text_scaled(peer_fingerprint[0] ? "ОТПЕЧАТОК КЛЮЧА СОБЕСЕДНИКА" : "ОЖИДАЕМ ОТПЕЧАТОК КЛЮЧА",
                x + u(16), u(123), 0xFF98A6C0u, u(0.32f));
    text_scaled(peer_fingerprint[0] ? peer_fingerprint : "Ожидание ответа…", x + u(16), u(151), 0xFF61D8C2u, u(0.42f));
    if (!peer_verified) {
        draw_field("Сверьте код по другому каналу", verify_text, "Введите отпечаток точно", x + u(12), u(237), w - u(24), 5);
        draw_button("Подтвердить ключ", x + u(12), u(294), w - u(24), u(42), 0);
    } else {
        text_scaled("Сквозное шифрование · только память устройства", x + u(12), u(232), 0xFF61D8C2u, u(0.34f));
    }
    float top = u(peer_verified ? 268 : 346);
    float cursor = bottom;
    for (i = chat_count; i > 0; i--) {
        const ChatLine *m = &chat[i - 1];
        unsigned rows = m->line_count ? m->line_count : 1;
        float bubble_h = u((float)(rows * 19u + 16u));
        float by = cursor - bubble_h;
        if (by < top) break;
        float bw = w * 0.82f;
        float bx = m->mine ? x + w - bw : x;
        roundrect(bx, by, bw, bubble_h, u(16), m->mine ? 0xFF1D6A67u : 0xFF222A42u);
        for (unsigned row = 0; row < rows; row++)
            text_scaled(m->lines[row], bx + u(12), by + u(8 + row * 19),
                        0xFFFFFFFFu, u(0.42f));
        cursor = by - u(8);
    }
    roundrect(x, (float)screen_h - u(98), w - u(76), u(58), u(18), 0xFF171D32u);
    text_scaled(message_text[0] ? message_text : "Сообщение…", x + u(14), (float)screen_h - u(80),
                message_text[0] ? 0xFFFFFFFFu : 0xFF7D8AA4u, u(0.42f));
    draw_button("➤", x + w - u(64), (float)screen_h - u(98), u(64), u(58), 1);
}
static void draw_notice(void) {
    if (notice_t <= 0 || !notice_text[0]) return;
    float w = content_w() - u(24), x = margin() + u(12), y = (float)screen_h - u(158);
    roundrect(x, y, w, u(40), u(14), 0xFF35304Au);
    text_scaled(notice_text, x + u(12), y + u(11), 0xFFFFFFFFu, u(0.34f));
}

void messenger_app_init(AAssetManager *assets) {
    ds_set_asset_manager(assets);
    (void)png_load(ICON);
    page = focus = network_started = peer_connected = peer_verified = incoming = 0;
    chat_count = 0;
    own_id[0] = own_fingerprint[0] = peer_id[0] = peer_name[0] = peer_fingerprint[0] = 0;
    incoming_id[0] = incoming_name[0] = incoming_fingerprint[0] = 0;
    copy_str(status_text, sizeof(status_text), "Сигналинг не подключён");
    copy_str(notice_text, sizeof(notice_text), "");
    keyboard_clear();
    keyboard_hide();
}
void messenger_app_update(void) {
    AppEvent e;
    while (dequeue_event(&e)) apply_event(&e);
    if (keyboard_visible()) sync_field();
    if (keyboard_enter_pressed()) {
        if (page == 1) send_message();
        else if (focus == 3) connect_peer(peer_id);
        else if (focus == 1 || focus == 2) start_network();
    }
    if (notice_t > 0) notice_t = clamp(notice_t - dt, 0, 3.0);
    anim_t += dt;
}
void messenger_app_draw(Buffer *buffer) {
    (void)buffer;
    draw_backdrop();
    if (page == 1) draw_chat(); else draw_home();
    draw_notice();
}
void messenger_app_touch(float x, float y, int action, int pointer_id) {
    float s = sc(), mx = margin(), yy = y / s, xx = x / s;
    (void)pointer_id;
    if (action != 0) return;
    sync_field();
    if (page == 0) {
        if (yy >= 322 && yy <= 366) { start_network(); return; }
        if (yy >= 208 && yy <= 256) { focus_field(1, server_url); return; }
        if (yy >= 272 && yy <= 320) { focus_field(2, nickname); return; }
        if (yy >= 454 && yy <= 498 && own_id[0]) { messenger_android_share(own_id, own_fingerprint); return; }
        if (yy >= 587 && yy <= 635) { focus_field(3, peer_id); return; }
        if (yy >= 642 && yy <= 688) { connect_peer(peer_id); return; }
        if (incoming && yy >= 716 && yy <= 762) {
            blur_keyboard();
            copy_str(peer_id, sizeof(peer_id), incoming_id);
            copy_str(peer_name, sizeof(peer_name), incoming_name);
            copy_str(peer_fingerprint, sizeof(peer_fingerprint), incoming_fingerprint);
            page = 1;
            messenger_android_accept(peer_id);
            note("Сверь отпечаток с собеседником перед отправкой");
            return;
        }
    } else {
        if (yy >= 25 && yy <= 83 && xx < mx + 130) { blur_keyboard(); page = 0; return; }
        if (!peer_verified && yy >= 236 && yy <= 284) { focus_field(5, verify_text); return; }
        if (!peer_verified && yy >= 294 && yy <= 338) {
            blur_keyboard();
            messenger_android_verify(peer_id, verify_text);
            return;
        }
        if (yy >= screen_h / s - 102 && xx >= 540 - 92) { send_message(); return; }
        if (yy >= screen_h / s - 106) { focus_field(4, message_text); return; }
        if (yy < 215) messenger_android_share(peer_id, peer_fingerprint);
    }
}
int messenger_app_back(void) {
    if (keyboard_visible()) { blur_keyboard(); return 1; }
    if (page == 1) { page = 0; incoming = 0; return 1; }
    return 0;
}
void messenger_app_stop(void) {
    blur_keyboard();
    messenger_android_stop();
}
void messenger_app_set_activity(void *activity) { messenger_android_set_activity(activity); }
void messenger_app_on_java_event(int type, const char *peer, const char *data) { queue_event(type, peer, data); }

#ifdef __ANDROID__
#include <jni.h>
static void java_string_to_utf8(JNIEnv *env, jstring value, char *output, size_t capacity) {
    if (!output || capacity == 0) return;
    output[0] = 0;
    if (!value) return;
    jsize length = (*env)->GetStringLength(env, value);
    const jchar *chars = (*env)->GetStringChars(env, value, NULL);
    if (!chars) return;
    size_t used = 0;
    for (jsize i = 0; i < length; i++) {
        uint32_t cp = chars[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < length
                && chars[i + 1] >= 0xDC00 && chars[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (chars[++i] - 0xDC00);
        } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            cp = 0xFFFD;
        }
        unsigned char encoded[4];
        size_t count;
        if (cp < 0x80) { encoded[0] = (unsigned char)cp; count = 1; }
        else if (cp < 0x800) {
            encoded[0] = (unsigned char)(0xC0 | (cp >> 6));
            encoded[1] = (unsigned char)(0x80 | (cp & 0x3F)); count = 2;
        } else if (cp < 0x10000) {
            encoded[0] = (unsigned char)(0xE0 | (cp >> 12));
            encoded[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            encoded[2] = (unsigned char)(0x80 | (cp & 0x3F)); count = 3;
        } else {
            encoded[0] = (unsigned char)(0xF0 | (cp >> 18));
            encoded[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
            encoded[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            encoded[3] = (unsigned char)(0x80 | (cp & 0x3F)); count = 4;
        }
        if (used + count >= capacity) break;
        memcpy(output + used, encoded, count);
        used += count;
    }
    output[used] = 0;
    (*env)->ReleaseStringChars(env, value, chars);
}
JNIEXPORT void JNICALL Java_com_ponedelnik_messenger_MessengerActivity_nativeMessengerEvent(JNIEnv *env, jobject obj, jint type,
                                                                       jstring peer, jstring data) {
    char peer_utf8[40], data_utf8[TEXT_CAP];
    (void)obj;
    java_string_to_utf8(env, peer, peer_utf8, sizeof(peer_utf8));
    java_string_to_utf8(env, data, data_utf8, sizeof(data_utf8));
    messenger_app_on_java_event((int)type, peer_utf8, data_utf8);
}
#endif
