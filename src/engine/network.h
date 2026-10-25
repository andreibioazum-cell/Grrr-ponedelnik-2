#ifndef GEO_NETWORK_H
#define GEO_NETWORK_H

#include <stddef.h>
#ifdef __ANDROID__
#include <jni.h>
#endif

#ifndef GEO_FIREBASE_URL
#define GEO_FIREBASE_URL "https://nasksel-9104c-default-rtdb.firebaseio.com"
#endif

#define GEO_DEFAULT_ROOM "main"
#define GEO_MAX_REMOTES 16
#define GEO_MAX_ROOMS 24
#define GEO_MAX_CHAT 24

#define NET_OFFLINE 0
#define NET_CONNECTING 1
#define NET_PLAYING 3
#define NET_ERROR 4

#define AUTH_IDLE 0
#define AUTH_BUSY 1
#define AUTH_OK 2
#define AUTH_BAD_INPUT 3
#define AUTH_WRONG_PASS 4
#define AUTH_NET_ERR 5

typedef struct {
    int active;
    char uid[24];
    char name[24];
    float x, y, z;
    float yaw, pitch;
    float swing;
    int held;
    int fly;
    /* Roblox-style appearance sync: model, packed materials, animation */
    int model;
    int skin;   /* 4 slots x 4 bits */
    int anim;
} GeoRemotePeer;

typedef struct {
    char name[24];
    int players;
} GeoRoomEntry;

#ifdef __ANDROID__
void net_set_java_vm(JavaVM *vm);
#endif

void net_set_data_path(const char *path);
void net_set_system_language(const char *code);
void net_set_base_url(const char *url);
const char *net_get_base_url(void);

/* Local & Cloud Profile / Copyright Warning / Settings */
void net_profile_load(void);
void net_profile_save(void);
const char *net_nick(void);
void net_set_nick(const char *nick);
int net_language(void);
void net_set_language(int lang);
int net_warning_accepted(void);
void net_accept_warning(void);
float net_sound_volume(void);
void net_set_sound_volume(float vol);
float net_look_sens(void);
void net_set_look_sens(float sens);
void net_solo_world_path(char *out, size_t cap);

/* Optional Cloud Account on Firebase RTDB (/geometrium/accounts/<nick>.json) */
void net_account_auth(const char *nick, const char *password);
int net_account_status(void);
int net_account_logged_in(void);
void net_account_logout(void);

/* Live Multiplayer Session on Firebase RTDB (/geometrium/rooms/<room>/...) */
void net_connect(const char *url, const char *room);
void net_disconnect(void);
int net_status(void);
const char *net_current_room(void);
int net_online_count(void);

void net_publish_local(float x, float y, float z, float yaw, float pitch, float swing, int held, int fly,
                       int model, int skin_packed, int anim);
/* Avatar (model + packed materials) persisted in the local profile */
void net_avatar_load(int *model, int *skin_packed);
void net_avatar_save(int model, int skin_packed);
/* Custom animations (3 poses x 7 joints + walk/run override flags) */
void net_custom_anim_load(float *poses, int *on);
void net_custom_anim_save(const float *poses, const int *on);
int net_copy_remotes(GeoRemotePeer *out, int max_out);

/* Dynamic Parts sync (/geometrium/rooms/<room>/parts.json): Roblox-style
 * script/player-created 3D objects that everyone in the room can see. */
typedef struct {
    char name[24];
    float x, y, z, sx, sy, sz, yaw;
    int block;   /* <0 = deleted */
} GeoPartNet;
void net_part_send(const GeoPartNet *part);
int net_part_poll(GeoPartNet *out);
/* Spectators watch other people's games without appearing as a player. */
void net_set_spectator(int spectator);
/* Publish the current room as a playable game in the catalog
 * (/geometrium/games/<room>.json: title, author, online count). */
void net_publish_game(void);

/* Real-time Persistent Shared Voxel Sync (/geometrium/rooms/<room>/blocks.json) */
void net_block_send(int x, int y, int z, int block_id);
int net_block_poll(int *x, int *y, int *z, int *block_id);
int net_world_synced(void);

/* Live UTF-8 Chat & Rooms Directory */
void net_chat_send(const char *text);
int net_chat_count(void);
void net_chat_get(int idx, char *sender_out, size_t s_cap, char *text_out, size_t t_cap);
void net_rooms_refresh(void);
int net_rooms_list(GeoRoomEntry *out, int max_out);

/* Android background presence job hook */
int net_presence_check(const char *data_dir);

#endif
