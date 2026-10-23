/* Real 3D Minecraft voxel world definitions, 24 block types, and state. */

#define MC_WX 64
#define MC_WY 36
#define MC_WZ 64
#define MC_SEA_Y 9
#define MC_REACH 6.2f
#define MC_GRAVITY 26.0f
#define MC_JUMP_VEL 8.7f
#define MC_PLAYER_W 0.60f
#define MC_PLAYER_H 1.80f
#define MC_EYE_H 1.62f
#define MC_SNEAK_EYE_H 1.40f
#define MC_MAX_PARTICLES 96
#define MC_MAX_TNT 16
#define MC_MAX_NPCS 6
#define MC_HOTBAR_SLOTS 9
#define MC_BLOCK_COUNT 25

#define MC_F_SOLID 1
#define MC_F_OPAQUE 2
#define MC_F_LIQUID 4
#define MC_F_CUTOUT 8
#define MC_F_GLOW 16
#define MC_F_UNBREAK 32

enum {
    MC_AIR = 0,
    MC_GRASS = 1,
    MC_DIRT = 2,
    MC_STONE = 3,
    MC_COBBLE = 4,
    MC_PLANKS = 5,
    MC_LOG = 6,
    MC_LEAVES = 7,
    MC_SAND = 8,
    MC_SANDSTONE = 9,
    MC_WATER = 10,
    MC_BRICK = 11,
    MC_GLASS = 12,
    MC_SNOW = 13,
    MC_SNOW_GRASS = 14,
    MC_ICE = 15,
    MC_COAL_ORE = 16,
    MC_IRON_ORE = 17,
    MC_GOLD_BLOCK = 18,
    MC_DIAMOND_ORE = 19,
    MC_DIAMOND_BLOCK = 20,
    MC_EMERALD_BLOCK = 21,
    MC_OBSIDIAN = 22,
    MC_GLOWSTONE = 23,
    MC_TNT = 24
};

/* Extra decorative blocks generated in starter cabin & creative inventory. */
enum {
    MC_BOOKSHELF = 25,
    MC_CRAFTING = 26,
    MC_WOOL_RED = 27,
    MC_WOOL_BLUE = 28,
    MC_BEDROCK = 29,
    MC_TOTAL_TYPES = 30
};

typedef struct {
    unsigned char top_tile;
    unsigned char side_tile;
    unsigned char bot_tile;
    unsigned char flags;
    float hardness;
    uint32_t particle_col;
    const char *ru;
    const char *en;
} McBlockDef;

static const McBlockDef mc_blocks[MC_TOTAL_TYPES] = {
    {0, 0, 0, 0, 0.0f, 0x00000000u, "", ""},
    {0, 1, 2, MC_F_SOLID | MC_F_OPAQUE, 0.28f, 0xFF59C135u, "Блок травы", "Grass Block"},
    {2, 2, 2, MC_F_SOLID | MC_F_OPAQUE, 0.25f, 0xFF866043u, "Земля", "Dirt"},
    {3, 3, 3, MC_F_SOLID | MC_F_OPAQUE, 0.55f, 0xFF7D7D7Du, "Камень", "Stone"},
    {4, 4, 4, MC_F_SOLID | MC_F_OPAQUE, 0.58f, 0xFF848484u, "Булыжник", "Cobblestone"},
    {5, 5, 5, MC_F_SOLID | MC_F_OPAQUE, 0.35f, 0xFFB28952u, "Дубовые доски", "Oak Planks"},
    {7, 6, 7, MC_F_SOLID | MC_F_OPAQUE, 0.42f, 0xFF664C2Cu, "Дубовое бревно", "Oak Log"},
    {8, 8, 8, MC_F_SOLID | MC_F_CUTOUT, 0.14f, 0xFF38982Au, "Листва", "Oak Leaves"},
    {9, 9, 9, MC_F_SOLID | MC_F_OPAQUE, 0.24f, 0xFFDED19Eu, "Песок", "Sand"},
    {11, 10, 11, MC_F_SOLID | MC_F_OPAQUE, 0.48f, 0xFFDACC98u, "Песчаник", "Sandstone"},
    {12, 12, 12, MC_F_LIQUID, 0.0f, 0xFF3E84E4u, "Вода", "Water"},
    {13, 13, 13, MC_F_SOLID | MC_F_OPAQUE, 0.55f, 0xFFA84A3Au, "Кирпичи", "Bricks"},
    {14, 14, 14, MC_F_SOLID | MC_F_CUTOUT, 0.18f, 0xFFD6F4FFu, "Стекло", "Glass"},
    {15, 15, 15, MC_F_SOLID | MC_F_OPAQUE, 0.20f, 0xFFF2F8FFu, "Снег", "Snow"},
    {15, 16, 2, MC_F_SOLID | MC_F_OPAQUE, 0.28f, 0xFFF2F8FFu, "Снежный дёрн", "Snowy Grass"},
    {17, 17, 17, MC_F_SOLID, 0.26f, 0xFF8EC0FCu, "Лёд", "Ice"},
    {18, 18, 18, MC_F_SOLID | MC_F_OPAQUE, 0.62f, 0xFF44444Au, "Угольная руда", "Coal Ore"},
    {19, 19, 19, MC_F_SOLID | MC_F_OPAQUE, 0.68f, 0xFFD6AE8Eu, "Железная руда", "Iron Ore"},
    {20, 20, 20, MC_F_SOLID | MC_F_OPAQUE, 0.65f, 0xFFF4CA34u, "Золотой блок", "Gold Block"},
    {21, 21, 21, MC_F_SOLID | MC_F_OPAQUE, 0.78f, 0xFF38E2E8u, "Алмазная руда", "Diamond Ore"},
    {22, 22, 22, MC_F_SOLID | MC_F_OPAQUE, 0.72f, 0xFF4AE8E2u, "Алмазный блок", "Diamond Block"},
    {23, 23, 23, MC_F_SOLID | MC_F_OPAQUE, 0.72f, 0xFF2ED660u, "Изумрудный блок", "Emerald Block"},
    {24, 24, 24, MC_F_SOLID | MC_F_OPAQUE, 1.15f, 0xFF3C2858u, "Обсидиан", "Obsidian"},
    {25, 25, 25, MC_F_SOLID | MC_F_OPAQUE | MC_F_GLOW, 0.22f, 0xFFFFE884u, "Светокамень", "Glowstone"},
    {29, 30, 31, MC_F_SOLID | MC_F_OPAQUE, 0.08f, 0xFFD23026u, "ТНТ (Динамит)", "TNT"},
    {5, 26, 5, MC_F_SOLID | MC_F_OPAQUE, 0.36f, 0xFFB28952u, "Книжная полка", "Bookshelf"},
    {27, 28, 5, MC_F_SOLID | MC_F_OPAQUE, 0.36f, 0xFFB28952u, "Верстак", "Crafting Table"},
    {32, 32, 32, MC_F_SOLID | MC_F_OPAQUE, 0.24f, 0xFFC43030u, "Красная шерсть", "Red Wool"},
    {33, 33, 33, MC_F_SOLID | MC_F_OPAQUE, 0.24f, 0xFF3060CCu, "Синяя шерсть", "Blue Wool"},
    {34, 34, 34, MC_F_SOLID | MC_F_OPAQUE | MC_F_UNBREAK, 99.0f, 0xFF303034u, "Бедрок", "Bedrock"}
};

static const unsigned char mc_creative_list[24] = {
    MC_GRASS,        MC_DIRT,         MC_COBBLE,        MC_STONE,       MC_PLANKS,     MC_LOG,
    MC_LEAVES,       MC_BRICK,        MC_GLASS,         MC_CRAFTING,    MC_BOOKSHELF,  MC_GLOWSTONE,
    MC_SAND,         MC_SANDSTONE,    MC_SNOW,          MC_ICE,         MC_WATER,      MC_GOLD_BLOCK,
    MC_DIAMOND_BLOCK, MC_EMERALD_BLOCK, MC_OBSIDIAN,    MC_WOOL_RED,    MC_WOOL_BLUE,  MC_TNT
};

typedef struct {
    int active;
    float x, y, z, vx, vy, vz, life, max_life, size;
    unsigned char tile;
    float u0, v0;
} McParticle;

typedef struct {
    int active;
    float x, y, z, vx, vy, vz, fuse;
} McTnt;

typedef struct {
    int active;
    float x, y, z, vx, vy, vz, yaw, hp, max_hp, walk_t, hurt_t, atk_cd, swing_t, respawn_t;
    int skin_idx, held_block, on_ground;
    const char *name;
} McNpc;

typedef struct {
    int active;
    float x, y, z, yaw, pitch, walk_t, swing_t, hurt_t;
    int sneaking, held_block, last_punch, last_edit_seq;
} McRemote3D;

typedef struct {
    unsigned char vox[MC_WY][MC_WZ][MC_WX];
    unsigned char base_vox[MC_WY][MC_WZ][MC_WX];
    unsigned char glow_lut[MC_WY][MC_WZ][MC_WX];
    int world_ready;
    unsigned int world_seed;
    int is_online_world;
    char world_room[32];

    float px, py, pz;
    float vx, vy, vz;
    float yaw, pitch;
    int on_ground, in_water, flying, sneaking, third_person;
    float walk_dist, bob_amp, swing_t, place_cd, jump_cd, fly_tap_t, hurt_t, banner_t;

    int hotbar[MC_HOTBAR_SLOTS];
    int slot;
    int inv_open;

    int hit, bx, by, bz, nx, ny, nz, hit_npc, hit_remote;
    float hit_dist;
    int mine_on, mx, my, mz;
    float mine_prog;

    int local_punch_seq;
    int local_edit_seq;
    int last_edit_pack;
    int last_edit_block;

    float cam_x, cam_y, cam_z;
    float fx, fy, fz, rx, ry, rz, ux, uy, uz;
    float tan_half_vfov, aspect;
    float time_sec;

    McParticle parts[MC_MAX_PARTICLES];
    McTnt tnts[MC_MAX_TNT];
    McNpc npcs[MC_MAX_NPCS];
    McRemote3D rem[4];

    /* Desktop / browser / multi-touch controls state */
    unsigned char keys[256];
    int mouse_mine, mouse_place, mouse_place_click, mouse_mine_click, cursor_locked;
    int look_tid, joy_tid;
    float look_lx, look_ly, joy_cx, joy_cy, joy_x, joy_y;
    int pad_mine, pad_place_tap, pad_jump;
    int kills_this_run, classic_sim, quad_count;
} McState;

static McState mc;

static int mc_in_bounds(int x, int y, int z) {
    return x >= 0 && x < MC_WX && y >= 0 && y < MC_WY && z >= 0 && z < MC_WZ;
}

static unsigned char mc_get(int x, int y, int z) {
    if (y < 0)
        return MC_BEDROCK;
    if (x < 0 || x >= MC_WX || z < 0 || z >= MC_WZ)
        return y <= MC_SEA_Y ? MC_WATER : MC_AIR;
    if (y >= MC_WY)
        return MC_AIR;
    return mc.vox[y][z][x];
}

static int mc_is_solid_id(unsigned char b) { return b < MC_TOTAL_TYPES && (mc_blocks[b].flags & MC_F_SOLID) != 0; }
static int mc_is_opaque_id(unsigned char b) { return b < MC_TOTAL_TYPES && (mc_blocks[b].flags & MC_F_OPAQUE) != 0; }
static int mc_is_liquid_id(unsigned char b) { return b < MC_TOTAL_TYPES && (mc_blocks[b].flags & MC_F_LIQUID) != 0; }
static int mc_is_solid(int x, int y, int z) { return mc_is_solid_id(mc_get(x, y, z)); }

static int mc_face_tile(unsigned char b, int face) {
    if (b >= MC_TOTAL_TYPES)
        return 0;
    if (face == 0)
        return mc_blocks[b].top_tile;
    if (face == 1)
        return mc_blocks[b].bot_tile;
    return mc_blocks[b].side_tile;
}

static const char *mc_block_label(int b) {
    if (b <= 0 || b >= MC_TOTAL_TYPES)
        return "";
    return language == 1 ? mc_blocks[b].ru : mc_blocks[b].en;
}

static void mc_skin_tiles_for(int cls, int skin, int *face_tile, int *body_tile) {
    int idx = 0;
    if (cls == 1)
        idx = skin == 1 ? 2 : 1; /* Azum / Zombie Azum */
    else if (cls == 2)
        idx = 3; /* Santa */
    else if (cls == 4)
        idx = skin == 1 ? 5 : 4; /* buK / Elf buK */
    else if (cls == 3)
        idx = 6; /* Astra */
    else if (cls == 5)
        idx = 7; /* CodeCracker */
    *face_tile = 48 + idx * 2;
    *body_tile = 49 + idx * 2;
}
