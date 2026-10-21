/* Keyboard/gamepad-to-touch bridge shared by the window loop and the
 * headless harness. The game itself only knows touches, so a PC "move" is a
 * second finger on the virtual stick and a PC "punch" is a tap on the pad
 * that battle_render.inc draws at that very moment - the same taps a phone
 * sends. Keeping this in one place means the harness exercises exactly the
 * code the keyboard drives. */
#define PC_TOUCH_STICK 900
#define PC_TOUCH_ATK 901
#define PC_TOUCH_BLK 902
#define PC_TOUCH_SUPER 903

static int pc_ctl_stick_on = 0;
static int pc_ctl_atk_down = 0;
static int pc_ctl_blk_down = 0;
static int pc_ctl_super_down = 0;

#ifdef _WIN32
/* Called when the window loses focus: the game must not keep a key held. */
static void pc_ctl_release_all(void) {
    float x = 0, y = 0;
    if (pc_ctl_stick_on) {
        game_pc_joy(&x, &y, NULL);
        pc_touch(x, y, 1, PC_TOUCH_STICK);
        pc_ctl_stick_on = 0;
    }
    if (pc_ctl_atk_down) {
        game_pc_pads(&x, &y, NULL, NULL, NULL, NULL, NULL);
        pc_touch(x, y, 1, PC_TOUCH_ATK);
        pc_ctl_atk_down = 0;
    }
    if (pc_ctl_blk_down) {
        game_pc_pads(NULL, NULL, NULL, NULL, &x, &y, NULL);
        pc_touch(x, y, 1, PC_TOUCH_BLK);
        pc_ctl_blk_down = 0;
    }
    if (pc_ctl_super_down) {
        game_pc_pads(NULL, NULL, &x, &y, NULL, NULL, NULL);
        pc_touch(x, y, 1, PC_TOUCH_SUPER);
        pc_ctl_super_down = 0;
    }
}
#endif

/* dx/dy in -1..1, y down. Held as long as the direction is non-zero: the
 * stick keeps its direction even at full speed (the game reads a unit
 * vector, speed comes from joy_speed). */
static void pc_ctl_stick(float dx, float dy) {
    float jx = 0, jy = 0, jr = 70, len;
    if (keyboard_visible() || !game_pc_in_battle()) {
        dx = dy = 0;
    }
    len = dx * dx + dy * dy;
    if (len > 0.0001f) {
        game_pc_joy(&jx, &jy, &jr);
        if (!pc_ctl_stick_on) {
            pc_touch(jx, jy, 0, PC_TOUCH_STICK);
            pc_ctl_stick_on = 1;
        }
        {
            float l = (float)sqrt((double)len);
            pc_touch(jx + dx / l * jr, jy + dy / l * jr, 2, PC_TOUCH_STICK);
        }
    } else if (pc_ctl_stick_on) {
        game_pc_joy(&jx, &jy, NULL);
        pc_touch(jx, jy, 1, PC_TOUCH_STICK);
        pc_ctl_stick_on = 0;
    }
}

static void pc_ctl_pad_touch(int which, int down) {
    float x = 0, y = 0, r = 60;
    int *state = which == 0 ? &pc_ctl_atk_down : which == 1 ? &pc_ctl_blk_down : &pc_ctl_super_down;
    if (*state == down)
        return;
    if (keyboard_visible() || !game_pc_in_battle()) {
        /* A field is open or there is no fight: nothing to press, but a held
         * pad must still let go so it does not stick. */
        down = 0;
        if (*state == down)
            return;
    }
    if (which == 0)
        game_pc_pads(&x, &y, NULL, NULL, NULL, NULL, &r);
    else if (which == 1)
        game_pc_pads(NULL, NULL, NULL, NULL, &x, &y, &r);
    else
        game_pc_pads(NULL, NULL, &x, &y, NULL, NULL, &r);
    pc_touch(x, y, down ? 0 : 1, which == 0 ? PC_TOUCH_ATK : which == 1 ? PC_TOUCH_BLK : PC_TOUCH_SUPER);
    *state = down;
}

static void pc_ctl_attack(int down) { pc_ctl_pad_touch(0, down); }
static void pc_ctl_block(int down) { pc_ctl_pad_touch(1, down); }
static void pc_ctl_super(int down) { pc_ctl_pad_touch(2, down); }
