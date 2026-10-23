#ifndef CB_GAME_H
#define CB_GAME_H

struct AAssetManager;
struct Buffer;

void game_init(struct AAssetManager *assets);
void game_update(void);
void game_draw(struct Buffer *buffer);
void game_touch(float x, float y, int action, int pointer_id);
int game_back(void);
void game_reset(void);
void game_bye(void);

#endif
