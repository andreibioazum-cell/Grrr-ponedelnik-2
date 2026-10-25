/* geo_user_scripts.inc - USER SCRIPTS IN C. This is where you script.
 *
 * A script is two C functions and one line in the registry below:
 *
 *   static void my_script_start(GeoScriptApi *api) { ... runs once ... }
 *   static void my_script_tick(GeoScriptApi *api, float dt) { ... 60 fps ... }
 *   ... { "my_script", my_script_start, my_script_tick } ...
 *
 * The api is your whole game API (see GeoScriptApi in geo_studio.inc):
 * get/set blocks, explode, spawn/move dynamic Parts, read the player,
 * bounce the player, print. Parts you spawn appear in the 3D world
 * immediately (Workspace-style). Full guide: docs/scripting.md.
 *
 * The three scripts below ship with the game as working examples.
 */

/* 1) Bounce pad: a glowing pad near spawn that throws the player up. */
static void bounce_pad_start(GeoScriptApi *api) {
    float x, y, z;
    api->player_pos(&x, &y, &z);
    api->spawn_part("BouncePad", x + 3.0f, y + 0.25f, z, 2.0f, 0.5f, 2.0f, 16 /* Lumen */);
    api->print("BouncePad ready");
}
static void bounce_pad_tick(GeoScriptApi *api, float dt) {
    (void)dt;
    if (api->player_on_part("BouncePad"))
        api->player_bounce(13.0f);
}

/* 2) Spinner: a platform that slowly rotates (scripts animate Parts). */
static void spinner_start(GeoScriptApi *api) {
    float x, y, z;
    api->player_pos(&x, &y, &z);
    api->spawn_part("Spinner", x - 4.0f, y + 0.3f, z, 3.4f, 0.4f, 3.4f, 20 /* Marble */);
}
static void spinner_tick(GeoScriptApi *api, float dt) {
    GeoPart *p = api->find_part("Spinner");
    if (p)
        p->yaw += dt * 0.7f;
}

/* 3) Builder: raises a small marble tower next to spawn on world start. */
static void builder_start(GeoScriptApi *api) {
    float x, y, z;
    int bx, by, bz, i;
    api->player_pos(&x, &y, &z);
    bx = (int)x + 6; by = (int)y; bz = (int)z;
    for (i = 0; i < 5; i++)
        api->set_block(bx, by + i, bz, 20 /* Marble */);
    api->set_block(bx, by + 5, bz, 16 /* Lumen lantern on top */);
    api->print("Builder raised a tower");
}

/* ---- script registry: add your line here ---- */
#define GEO_SCRIPT_COUNT 3
static const GeoScriptDef geo_scripts[GEO_SCRIPT_COUNT] = {
    {"bounce_pad", bounce_pad_start, bounce_pad_tick},
    {"spinner",    spinner_start,    spinner_tick},
    {"builder",    builder_start,    NULL},
};
