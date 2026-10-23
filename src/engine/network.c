/* Geometrium Real-Time Firebase RTDB Multiplayer & Cloud Profile Engine.
 * Default database: https://nasksel-9104c-default-rtdb.firebaseio.com */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "engine/network.h"

#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "GeometriumNet", __VA_ARGS__)
#define LOGERR(...) __android_log_print(ANDROID_LOG_ERROR, "GeometriumNet", __VA_ARGS__)
#else
#define LOG(...) ((void)0)
#define LOGERR(...) ((void)0)
#endif

typedef pthread_mutex_t DSMutex;
#define DS_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#define ds_mutex_lock(m) pthread_mutex_lock(&(m))
#define ds_mutex_unlock(m) pthread_mutex_unlock(&(m))

#define TIMEOUT 5000
#define BLOCK_RING_CAP 8192
#define OUTBOX_CAP 2048
#define BLOCKS_RESP_MAX 524288

typedef struct {
    int x, y, z, id;
} GeoBlockEdit;

typedef struct {
    char sender[24];
    char text[96];
} GeoChatMsg;

typedef struct {
#ifdef __ANDROID__
    JavaVM *vm;
#endif
    char data_dir[320];
    char base[256];
    char uid[24];
    char nick[24];
    char pass_hash[32];
    int logged_in;
    int auth_state;
    int lang;
    int warn_ok;
    float volume;
    float sens;
    int profile_loaded;

    int run;
    unsigned int gen;
    int state;
    char room[24];
    long long seq;

    float lx, ly, lz, lyaw, lpitch, lswing;
    int lheld, lfly;

    GeoRemotePeer peers[GEO_MAX_REMOTES];
    int peer_n;

    GeoBlockEdit outbox[OUTBOX_CAP];
    int out_head, out_tail;

    GeoBlockEdit inbox[BLOCK_RING_CAP];
    int in_head, in_tail;
    int world_loaded;

    GeoChatMsg chat[GEO_MAX_CHAT];
    int chat_n;

    GeoRoomEntry rooms[GEO_MAX_ROOMS];
    int room_n;
    int rooms_busy;
} NetState;

static DSMutex net_lock = DS_MUTEX_INIT;
static NetState net;
static int net_fast = 0;
static int net_log_ok(void) { return 1; }

static long long geo_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000L);
}

static void geo_sleep_ms(int ms) {
    struct timespec t;
    t.tv_sec = ms / 1000;
    t.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&t, NULL);
}

#ifdef __ANDROID__
#include "platform/android/network_http.inc"
#else
#include "engine/network/geo_net_posix.inc"
#endif

#include "engine/network/geo_net_json.inc"
#include "engine/network/geo_net_profile.inc"
#include "engine/network/geo_net_rooms.inc"
#include "engine/network/geo_net_api.inc"

void geo_net_keep_transport_symbols(void);
void geo_net_keep_transport_symbols(void) {
    (void)net_fast;
    (void)net_log_ok;
    (void)http_stream;
}
