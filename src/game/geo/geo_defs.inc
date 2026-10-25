/* Geometrium core block definitions, monochrome palette helpers, and world state.
 * Every single block texture in assets/textures/ is a brand-new 32x32 RGBA PNG. */

#define GEO_WX 96
#define GEO_WY 48
#define GEO_WZ 96
#define GEO_SEA_Y 12
#define GEO_RENDER_DIST 24
#define GEO_HOTBAR_SLOTS 9
#define GEO_PALETTE_COUNT 23
#define GEO_MAX_PARTICLES 96
#define GEO_MAX_CHARGES 16

#define GEO_F_SOLID 1
#define GEO_F_OPAQUE 2
#define GEO_F_LIQUID 4
#define GEO_F_TRANSLUCENT 8
#define GEO_F_WARM_LIGHT 16
#define GEO_F_COOL_LIGHT 32

enum {
    GEO_AIR = 0,
    GEO_TURF = 1,
    GEO_LOAM = 2,
    GEO_SLATE = 3,
    GEO_BASALT = 4,
    GEO_LIMESTONE = 5,
    GEO_BRICK = 6,
    GEO_DUNE = 7,
    GEO_FROST = 8,
    GEO_CEDAR_LOG = 9,
    GEO_CEDAR_PLANKS = 10,
    GEO_ASH_PANEL = 11,
    GEO_CANOPY = 12,
    GEO_GLASS = 13,
    GEO_WATER = 14,
    GEO_ICE = 15,
    GEO_LUMEN = 16,
    GEO_CYAN_LAMP = 17,
    GEO_STEEL = 18,
    GEO_MONOLITH = 19,
    GEO_MARBLE = 20,
    GEO_OBSIDIAN = 21,
    GEO_AMBER = 22,
    GEO_COBALT = 23,
    GEO_CHARGE = 24,
    GEO_BLOCK_COUNT = 25
};

enum {
    SCR_WARNING = 0,
    SCR_MENU = 1,
    SCR_ROOMS = 2,
    SCR_PROFILE = 3,
    SCR_INFO = 4,
    SCR_PLAY = 5,
    SCR_STUDIO = 6,  /* Roblox-style avatar/model editor */
    SCR_ANIM = 7     /* custom animation editor */
};

enum {
    KB_NONE = 0,
    KB_NICK = 1,
    KB_PASS = 2,
    KB_ROOM = 3,
    KB_CHAT = 4
};

typedef struct {
    const char *ru;
    const char *en;
    const char *top_tex;
    const char *side_tex;
    const char *bot_tex;
    float hardness;
    unsigned int flags;
} GeoBlockDef;

static const GeoBlockDef geo_blocks[GEO_BLOCK_COUNT] = {
    {"Воздух", "Air", "", "", "", 0.0f, 0},
    {"Дёрн", "Turf", "turf_top.png", "turf_side.png", "loam.png", 0.42f, GEO_F_SOLID | GEO_F_OPAQUE},
    {"Грунт", "Loam Soil", "loam.png", "loam.png", "loam.png", 0.38f, GEO_F_SOLID | GEO_F_OPAQUE},
    {"Сланец", "Slate Stone", "slate.png", "slate.png", "slate.png", 0.85f, GEO_F_SOLID | GEO_F_OPAQUE},
    {"Базальтовая плита", "Basalt Tiles", "basalt_tiles.png", "basalt_tiles.png", "basalt_tiles.png", 0.90f,
     GEO_F_SOLID | GEO_F_OPAQUE},
    {"Известняк", "Limestone", "limestone.png", "limestone.png", "limestone.png", 0.75f, GEO_F_SOLID | GEO_F_OPAQUE},
    {"Клинкерный кирпич", "Masonry Brick", "terracotta_brick.png", "terracotta_brick.png", "terracotta_brick.png",
     0.85f, GEO_F_SOLID | GEO_F_OPAQUE},
    {"Дюнный песок", "Dune Sand", "dune.png", "dune.png", "dune.png", 0.35f, GEO_F_SOLID | GEO_F_OPAQUE},
    {"Снежный наст", "Frost Layer", "frost_top.png", "frost_side.png", "loam.png", 0.40f, GEO_F_SOLID | GEO_F_OPAQUE},
    {"Кедровый ствол", "Cedar Trunk", "cedar_top.png", "cedar_bark.png", "cedar_top.png", 0.65f,
     GEO_F_SOLID | GEO_F_OPAQUE},
    {"Кедровая доска", "Cedar Planks", "cedar_planks.png", "cedar_planks.png", "cedar_planks.png", 0.55f,
     GEO_F_SOLID | GEO_F_OPAQUE},
    {"Ясеневая панель", "Ash Wood Panel", "ash_panel.png", "ash_panel.png", "ash_panel.png", 0.55f,
     GEO_F_SOLID | GEO_F_OPAQUE},
    {"Листва кроны", "Canopy Foliage", "canopy.png", "canopy.png", "canopy.png", 0.22f, GEO_F_SOLID | GEO_F_OPAQUE},
    {"Витражное стекло", "Clear Glass", "clear_glass.png", "clear_glass.png", "clear_glass.png", 0.28f,
     GEO_F_SOLID | GEO_F_TRANSLUCENT},
    {"Кристальная вода", "Crystal Water", "azure_water.png", "azure_water.png", "azure_water.png", 0.0f,
     GEO_F_LIQUID | GEO_F_TRANSLUCENT},
    {"Призменный лёд", "Prism Ice", "prism_ice.png", "prism_ice.png", "prism_ice.png", 0.45f,
     GEO_F_SOLID | GEO_F_TRANSLUCENT},
    {"Люмен-фонарь", "Lumen Lantern", "lumen_core.png", "lumen_core.png", "lumen_core.png", 0.40f,
     GEO_F_SOLID | GEO_F_OPAQUE | GEO_F_WARM_LIGHT},
    {"Холодный неон", "Cold Neon", "cyan_lamp.png", "cyan_lamp.png", "cyan_lamp.png", 0.40f,
     GEO_F_SOLID | GEO_F_OPAQUE | GEO_F_COOL_LIGHT},
    {"Шлифованная сталь", "Brushed Steel", "steel_plate.png", "steel_plate.png", "steel_plate.png", 1.15f,
     GEO_F_SOLID | GEO_F_OPAQUE},
    {"Монолит основания", "Core Monolith", "monolith.png", "monolith.png", "monolith.png", 999.0f,
     GEO_F_SOLID | GEO_F_OPAQUE},
    {"Каннелюрный мрамор", "Fluted Marble", "marble_top.png", "marble_fluted.png", "marble_top.png", 0.85f,
     GEO_F_SOLID | GEO_F_OPAQUE},
    {"Обсидиановая сетка", "Obsidian Grid", "obsidian_grid.png", "obsidian_grid.png", "obsidian_grid.png", 1.35f,
     GEO_F_SOLID | GEO_F_OPAQUE},
    {"Янтарная жила", "Amber Vein", "amber_vein.png", "amber_vein.png", "amber_vein.png", 0.95f,
     GEO_F_SOLID | GEO_F_OPAQUE},
    {"Кобальтовая мозаика", "Cobalt Mosaic", "cobalt_mosaic.png", "cobalt_mosaic.png", "cobalt_mosaic.png", 0.90f,
     GEO_F_SOLID | GEO_F_OPAQUE},
    {"Импульсный заряд", "Demolition Charge", "charge_top.png", "charge_side.png", "charge_bot.png", 0.25f,
     GEO_F_SOLID | GEO_F_OPAQUE},
};

static const int geo_palette_blocks[GEO_PALETTE_COUNT] = {
    GEO_BASALT,   GEO_MARBLE,   GEO_CEDAR_PLANKS, GEO_ASH_PANEL, GEO_BRICK,  GEO_LIMESTONE, GEO_GLASS,    GEO_LUMEN,
    GEO_CYAN_LAMP, GEO_STEEL,   GEO_OBSIDIAN,     GEO_COBALT,    GEO_TURF,   GEO_LOAM,      GEO_SLATE,    GEO_CEDAR_LOG,
    GEO_CANOPY,   GEO_DUNE,     GEO_FROST,        GEO_ICE,       GEO_AMBER,  GEO_WATER,     GEO_CHARGE,
};

static const int geo_default_hotbar[GEO_HOTBAR_SLOTS] = {
    GEO_BASALT, GEO_MARBLE, GEO_CEDAR_PLANKS, GEO_BRICK, GEO_GLASS, GEO_LUMEN, GEO_TURF, GEO_SLATE, GEO_CHARGE,
};

typedef struct {
    int active;
    char uid[24];
    char name[24];
    float x, y, z, vx;
    float tx, ty, tz;
    float yaw, pitch, tyaw, tpitch;
    float walk_t, swing_t, last_swing;
    int held;
    int next_ent;
    /* Roblox-style synced appearance: model, 4 material slots, animation */
    int model;
    int skin[4];
    int anim;
    float anim_t;
} GeoRemoteAvatar;

typedef struct {
    int active;
    float x, y, z, vy, fuse;
    int next_ent;
} GeoChargeEnt;

typedef struct {
    int active;
    float x, y, z, vx, vy, vz, life;
    unsigned char block_id;
    int next_ent;
} GeoParticle;

typedef struct {
    float x, y, w, h;
} GeoRect;

typedef struct {
    int screen;
    int lang;
    int online;
    char room_name[24];
    float time_sec;

    unsigned char vox[GEO_WY][GEO_WZ][GEO_WX];
    unsigned char light_warm[GEO_WY][GEO_WZ][GEO_WX];

    float px, py, pz;
    float vx, vy, vz;
    float yaw, pitch;
    int on_ground, in_water, flying, third_person;
    /* Roblox orbit camera: yaw/pitch are the camera, face_yaw is the body.
     * cam_dist zooms the 3rd-person orbit; spectate = free-fly observer. */
    float cam_dist, face_yaw;
    int spectate;
    int spectate_sel;  /* rooms screen: next join is watch-only */
    int build_mode;    /* in-game Part builder (Roblox Studio style) */
    int part_sel;      /* selected GeoPart index, -1 = none */
    int players_open;  /* overlay listing other accounts in the world */
    float walk_phase, swing_anim, step_cd, place_cd, sync_cd;
    int swing_seq;

    /* Roblox-style avatar: model id, material per slot, animation state */
    int av_model;
    int av_skin[4];
    int anim_id;
    float anim_t;
    int emote_id;      /* ANIM_WAVE / ANIM_DANCE while emote_t > 0 */
    float emote_t;

    int hotbar[GEO_HOTBAR_SLOTS];
    int slot;
    int inv_open, chat_open, pause_open;

    int hit, bx, by, bz, nx, ny, nz;
    float hit_dist;
    int mine_on, mx, my, mz;
    float mine_prog;

    float cam_x, cam_y, cam_z;
    float fx, fy, fz, rx, ry, rz, ux, uy, uz;
    float tan_half_vfov, aspect;
    int quad_count;

    /* Record hook for automated 3D water occlusion verification in host tests */
    int record_quads;
    int rec_n;
    unsigned char rec_is_water[16384];
    float rec_min_x[16384], rec_max_x[16384], rec_min_y[16384], rec_max_y[16384];

    GeoRemoteAvatar rem[GEO_MAX_REMOTES];
    GeoChargeEnt charges[GEO_MAX_CHARGES];
    GeoParticle parts[GEO_MAX_PARTICLES];

    /* Touch & UI state */
    int look_tid, joy_tid, mine_tid, place_tid, jump_tid, up_tid, down_tid;
    float look_lx, look_ly, joy_cx, joy_cy, joy_x, joy_y;
    int place_tap_queued;

    int kb_mode;
    char edit_nick[24];
    char edit_pass[48];
    char edit_room[24];
    char auth_note[96];
} GeoState;

static GeoState geo;

/* Defined in geo_script_glue.inc (included later in the TU). */
static void geo_apply_remote_parts(void);
static void geo_sync_remote_players(float fdt);
static void geo_draw_anim(float cx, float cy);
static void geo_touch_anim(float x, float y, float cx, float cy);
static int geo_skin_pack(void);
static void geo_skin_unpack(int packed, int skin[4]);

/* Strict 100% monochrome dark-gray color packer: R == G == B guaranteed by type signature! */
static uint32_t geo_gray(int lum, int alpha) {
    unsigned int l = (unsigned int)(lum < 0 ? 0 : (lum > 255 ? 255 : lum));
    unsigned int a = (unsigned int)(alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha));
    return (a << 24) | (l << 16) | (l << 8) | l;
}

static int geo_in_bounds(int x, int y, int z) {
    return x >= 0 && x < GEO_WX && y >= 0 && y < GEO_WY && z >= 0 && z < GEO_WZ;
}

static unsigned char geo_get(int x, int y, int z) {
    if (y < 0)
        return GEO_MONOLITH;
    if (x < 0 || x >= GEO_WX || z < 0 || z >= GEO_WZ)
        return y <= GEO_SEA_Y ? GEO_WATER : GEO_AIR;
    if (y >= GEO_WY)
        return GEO_AIR;
    return geo.vox[y][z][x];
}

static int geo_is_solid_id(unsigned char b) { return b < GEO_BLOCK_COUNT && (geo_blocks[b].flags & GEO_F_SOLID) != 0; }
static int geo_is_opaque_id(unsigned char b) {
    return b < GEO_BLOCK_COUNT && (geo_blocks[b].flags & GEO_F_OPAQUE) != 0;
}
static int geo_is_liquid_id(unsigned char b) {
    return b < GEO_BLOCK_COUNT && (geo_blocks[b].flags & GEO_F_LIQUID) != 0;
}
static int geo_is_solid(int x, int y, int z) { return geo_is_solid_id(geo_get(x, y, z)); }

static const char *geo_face_tex(unsigned char b, int face) {
    if (b >= GEO_BLOCK_COUNT)
        return "";
    if (face == 0)
        return geo_blocks[b].top_tex;
    if (face == 1)
        return geo_blocks[b].bot_tex;
    return geo_blocks[b].side_tex;
}

static const char *geo_block_name(int b) {
    if (b <= 0 || b >= GEO_BLOCK_COUNT)
        return "";
    return geo.lang ? geo_blocks[b].ru : geo_blocks[b].en;
}
