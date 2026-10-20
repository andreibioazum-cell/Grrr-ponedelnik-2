/* PC input bridge: the Android build receives every touch straight from the
 * panel, so the pads are invisible to the outside. A keyboard and a mouse
 * need their artwork's coordinates, though, and those live in this
 * translation unit - hence this small read-only window into it. */

double game_pc_state(void) { return game_state; }

int game_pc_in_battle(void) { return game_state == ST_SOLO || game_state == ST_ONLINE; }

void game_pc_goto(double target) { start_transition(target); }

void game_pc_joy(float *x, float *y, float *r) {
    if (x)
        *x = joy.x;
    if (y)
        *y = joy.y;
    if (r)
        *r = joy.r;
}

void game_pc_pads(float *ax, float *ay, float *a2x, float *a2y, float *bx, float *by, float *radius) {
    if (ax)
        *ax = (float)atk_x;
    if (ay)
        *ay = (float)atk_y;
    if (a2x)
        *a2x = (float)atk2_x;
    if (a2y)
        *a2y = (float)atk2_y;
    if (bx)
        *bx = (float)blk_x;
    if (by)
        *by = (float)blk_y;
    if (radius)
        *radius = (float)battle_pad_r();
}

double game_pc_player_hp(void) { return player ? player->hp : 0.0; }

void game_pc_login_fields(float *nick_x, float *nick_y, float *pass_x, float *pass_y, float *btn_x, float *btn_y,
                          float *btn_w, float *button_h) {
    if (nick_x)
        *nick_x = (float)login_x();
    if (nick_y)
        *nick_y = (float)login_nick_y();
    if (pass_x)
        *pass_x = (float)login_x();
    if (pass_y)
        *pass_y = (float)login_pass_y();
    if (btn_x)
        *btn_x = (float)login_btn_x();
    if (btn_y)
        *btn_y = (float)login_btn_y();
    if (btn_w)
        *btn_w = (float)login_btn_w;
    if (button_h)
        *button_h = (float)btn_h;
}

void game_pc_player_xy(double *x, double *y) {
    if (x)
        *x = player ? player->x : 0.0;
    if (y)
        *y = player ? player->y : 0.0;
}

double game_pc_enemy_hp(void) { return enemy ? enemy->hp : 0.0; }

double game_pc_finished(void) { return finished; }

/* The consent checkbox on the login screen is part of the form: a PC player
 * clicks it like any other control. */
void game_pc_login_consent(float *x, float *y, float *size) {
    if (x)
        *x = (float)(login_consent_x() + login_box_sz() * 0.5);
    if (y)
        *y = (float)(login_consent_y() + login_box_sz() * 0.5);
    if (size)
        *size = (float)login_box_sz();
}

double game_pc_login_status(void) { return login_status; }

const char *game_pc_login_nick(void) { return login_nick; }
