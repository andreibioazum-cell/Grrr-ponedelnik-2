#ifndef PONEDLNIK_MESSENGER_APP_H
#define PONEDLNIK_MESSENGER_APP_H

#include <android/asset_manager.h>
#include "engine/runtime.h"

#define PND_EVENT_IDENTITY 1
#define PND_EVENT_STATUS 2
#define PND_EVENT_INCOMING 3
#define PND_EVENT_PEER_INFO 4
#define PND_EVENT_CONNECTED 5
#define PND_EVENT_MESSAGE 6
#define PND_EVENT_SENT 7
#define PND_EVENT_VERIFIED 8
#define PND_EVENT_ERROR 9
#define PND_EVENT_CALL_INCOMING 10
#define PND_EVENT_CALL_STATE 11
#define PND_EVENT_CALL_ERROR 12

void messenger_app_init(AAssetManager *assets);
void messenger_app_update(void);
void messenger_app_draw(Buffer *buffer);
void messenger_app_touch(float x, float y, int action, int pointer_id);
int messenger_app_back(void);
void messenger_app_stop(void);
void messenger_app_set_activity(void *activity);

void messenger_android_set_activity(void *activity);
void messenger_android_start(const char *url, const char *nickname);
void messenger_android_connect(const char *peer_id);
void messenger_android_accept(const char *peer_id);
void messenger_android_verify(const char *peer_id, const char *fingerprint);
void messenger_android_send(const char *peer_id, const char *message);
void messenger_android_share(const char *peer_id, const char *fingerprint);
void messenger_android_call(const char *peer_id);
void messenger_android_accept_call(const char *peer_id);
void messenger_android_decline_call(const char *peer_id);
void messenger_android_end_call(const char *peer_id);
void messenger_android_mute_call(const char *peer_id, int muted);
void messenger_android_stop(void);

#endif
