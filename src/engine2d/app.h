#ifndef PONDELINIK_2D_APP_H
#define PONDELINIK_2D_APP_H

#include "engine/runtime.h"

void pond_app_init(AAssetManager *assets);
void pond_app_update(void);
void pond_app_draw(Buffer *buffer);
void pond_app_touch(float x, float y, int down);
void pond_app_key(int key, int down);
void pond_app_end_frame(void);
void pond_app_stop(void);

#endif
