/* Procedural 3D Minecraft terrain, ores, cabin & pier, and Solo world save. */

static unsigned int mc_hash3(int x, int y, int z, unsigned int seed) {
    unsigned int h = seed ^ ((unsigned int)x * 374761393u) ^ ((unsigned int)y * 668265263u) ^
                     ((unsigned int)z * 2246822519u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float mc_vnoise2(float x, float z, unsigned int seed) {
    int xi = (int)floorf(x), zi = (int)floorf(z);
    float fx = x - (float)xi, fz = z - (float)zi;
    float ux = fx * fx * (3.0f - 2.0f * fx), uz = fz * fz * (3.0f - 2.0f * fz);
    float a = (float)(mc_hash3(xi, 0, zi, seed) & 0xFFFFu) / 65535.0f;
    float b = (float)(mc_hash3(xi + 1, 0, zi, seed) & 0xFFFFu) / 65535.0f;
    float c = (float)(mc_hash3(xi, 0, zi + 1, seed) & 0xFFFFu) / 65535.0f;
    float d = (float)(mc_hash3(xi + 1, 0, zi + 1, seed) & 0xFFFFu) / 65535.0f;
    return (a + (b - a) * ux) + ((c + (d - c) * ux) - (a + (b - a) * ux)) * uz;
}

static float mc_fbm2(float x, float z, unsigned int seed) {
    return mc_vnoise2(x * 0.055f, z * 0.055f, seed) * 0.62f +
           mc_vnoise2(x * 0.115f, z * 0.115f, seed + 97u) * 0.26f +
           mc_vnoise2(x * 0.24f, z * 0.24f, seed + 251u) * 0.12f;
}

static unsigned int mc_seed_for_room(const char *room) {
    unsigned int h = 20261023u;
    if (!room || !room[0] || strcmp(room, "main") == 0)
        return 777420u;
    while (*room) {
        h = h * 131u + (unsigned char)(*room++);
    }
    return h ? h : 777420u;
}

static void mc_raw_set(int x, int y, int z, unsigned char b) {
    if (mc_in_bounds(x, y, z))
        mc.vox[y][z][x] = b;
}

static void mc_fill_box(int x0, int y0, int z0, int x1, int y1, int z1, unsigned char b) {
    int x, y, z;
    for (y = y0; y <= y1; y++)
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++)
                mc_raw_set(x, y, z, b);
}

static void mc_rebuild_glow_lut(void) {
    int x, y, z, dx, dy, dz;
    memset(mc.glow_lut, 0, sizeof(mc.glow_lut));
    for (y = 0; y < MC_WY; y++)
        for (z = 0; z < MC_WZ; z++)
            for (x = 0; x < MC_WX; x++)
                if (mc.vox[y][z][x] == MC_GLOWSTONE) {
                    for (dy = -3; dy <= 3; dy++)
                        for (dz = -3; dz <= 3; dz++)
                            for (dx = -3; dx <= 3; dx++) {
                                int xx = x + dx, yy = y + dy, zz = z + dz;
                                int g = 115 - (dx * dx + dy * dy + dz * dz) * 11;
                                if (g > 0 && mc_in_bounds(xx, yy, zz) && g > (int)mc.glow_lut[yy][zz][xx])
                                    mc.glow_lut[yy][zz][xx] = (unsigned char)g;
                            }
                }
}

static int mc_top_solid_y(int x, int z) {
    int y;
    for (y = MC_WY - 1; y >= 0; y--) {
        unsigned char b = mc_get(x, y, z);
        if (mc_is_solid_id(b) && b != MC_LEAVES)
            return y;
    }
    return MC_SEA_Y;
}

static void mc_plant_tree(int tx, int ty, int tz, int pine) {
    int h = pine ? 6 : 5;
    int y, dx, dz;
    for (y = 0; y < h; y++)
        mc_raw_set(tx, ty + y, tz, MC_LOG);
    if (pine) {
        for (y = 2; y <= h + 1; y++) {
            int rad = (h + 2 - y) / 2 + ((y < h) ? 1 : 0);
            if (rad > 2)
                rad = 2;
            for (dz = -rad; dz <= rad; dz++)
                for (dx = -rad; dx <= rad; dx++) {
                    if (abs(dx) == rad && abs(dz) == rad && rad > 1)
                        continue;
                    if (mc_get(tx + dx, ty + y, tz + dz) == MC_AIR)
                        mc_raw_set(tx + dx, ty + y, tz + dz, (ty > 19 && y >= h) ? MC_SNOW : MC_LEAVES);
                }
        }
        return;
    }
    for (y = h - 2; y <= h + 1; y++) {
        int rad = (y >= h) ? 1 : 2;
        for (dz = -rad; dz <= rad; dz++)
            for (dx = -rad; dx <= rad; dx++) {
                if (abs(dx) == 2 && abs(dz) == 2 && y == h - 1)
                    continue;
                if (mc_get(tx + dx, ty + y, tz + dz) == MC_AIR)
                    mc_raw_set(tx + dx, ty + y, tz + dz, MC_LEAVES);
            }
    }
}

static void mc_build_spawn_village(void) {
    /* Flatten cozy meadow terrace around (26..38, z=20..32) + cabin at (30..36, z=18..24) */
    int x, z, gy = 11;
    for (z = 16; z <= 33; z++) {
        for (x = 24; x <= 40; x++) {
            int cur = mc_top_solid_y(x, z);
            if (z >= 17 && z <= 31 && x >= 26 && x <= 38) {
                int y;
                for (y = 1; y <= gy - 1; y++)
                    mc_raw_set(x, y, z, y < gy - 3 ? MC_STONE : MC_DIRT);
                mc_raw_set(x, gy, z, MC_GRASS);
                for (y = gy + 1; y < MC_WY; y++)
                    mc_raw_set(x, y, z, MC_AIR);
            } else if (cur > gy + 2) {
                mc_raw_set(x, cur, z, MC_GRASS);
            }
        }
    }
    /* Cobblestone foundation + Oak Planks floor */
    mc_fill_box(29, gy, 18, 36, gy, 24, MC_COBBLE);
    mc_fill_box(30, gy, 19, 35, gy, 23, MC_PLANKS);
    /* Oak walls + Log corner pillars */
    mc_fill_box(29, gy + 1, 18, 36, gy + 4, 24, MC_PLANKS);
    mc_fill_box(30, gy + 1, 19, 35, gy + 4, 23, MC_AIR);
    for (z = 18; z <= 24; z += 6)
        for (x = 29; x <= 36; x += 7)
            mc_fill_box(x, gy + 1, z, x, gy + 4, z, MC_LOG);
    /* Front doorway facing spawn (z = 24, x = 32..33) + Glass windows */
    mc_fill_box(32, gy + 1, 24, 33, gy + 2, 24, MC_AIR);
    mc_raw_set(30, gy + 2, 24, MC_GLASS);
    mc_raw_set(35, gy + 2, 24, MC_GLASS);
    mc_fill_box(29, gy + 2, 20, 29, gy + 2, 22, MC_GLASS);
    mc_fill_box(36, gy + 2, 20, 36, gy + 2, 22, MC_GLASS);
    mc_fill_box(31, gy + 2, 18, 34, gy + 2, 18, MC_GLASS);
    /* Pitched stair-stepped roof + Brick chimney */
    mc_fill_box(28, gy + 5, 17, 37, gy + 5, 25, MC_COBBLE);
    mc_fill_box(29, gy + 6, 18, 36, gy + 6, 24, MC_BRICK);
    mc_fill_box(30, gy + 7, 19, 35, gy + 7, 23, MC_BRICK);
    mc_fill_box(35, gy + 5, 19, 35, gy + 9, 19, MC_BRICK);
    /* Interior furnishings: Glowstone chandelier, Bookshelves, Crafting Table, Treasure */
    mc_raw_set(32, gy + 4, 21, MC_GLOWSTONE);
    mc_raw_set(33, gy + 4, 21, MC_GLOWSTONE);
    mc_fill_box(30, gy + 1, 19, 31, gy + 2, 19, MC_BOOKSHELF);
    mc_raw_set(34, gy + 1, 19, MC_CRAFTING);
    mc_raw_set(35, gy + 1, 19, MC_GOLD_BLOCK);
    mc_raw_set(35, gy + 1, 20, MC_DIAMOND_BLOCK);
    mc_raw_set(35, gy + 2, 19, MC_EMERALD_BLOCK);
    /* Cobblestone garden path + Glowstone lamp post + Wooden Pier over lake + TNT demo crate */
    mc_fill_box(32, gy, 25, 33, gy, 30, MC_COBBLE);
    mc_fill_box(28, gy + 1, 26, 28, gy + 3, 26, MC_LOG);
    mc_raw_set(28, gy + 4, 26, MC_GLOWSTONE);
    mc_raw_set(37, gy + 1, 27, MC_TNT);
    /* Waterfront pier heading toward the river/lake */
    for (x = 22; x <= 27; x++) {
        mc_fill_box(x, MC_SEA_Y + 1, 29, x, MC_SEA_Y + 1, 30, MC_PLANKS);
        if (x == 22 || x == 25)
            mc_fill_box(x, MC_SEA_Y - 2, 29, x, MC_SEA_Y, 29, MC_LOG);
    }
}

static void mc_solo_save(void) {
    char path[512];
    FILE *f;
    int x, y, z;
    unsigned short count = 0;
    net_mc_solo_path(path, sizeof(path));
    f = fopen(path, "wb");
    if (!f)
        return;
    for (y = 0; y < MC_WY; y++)
        for (z = 0; z < MC_WZ; z++)
            for (x = 0; x < MC_WX; x++)
                if (mc.vox[y][z][x] != mc.base_vox[y][z][x] && count < 65000u)
                    count++;
    fwrite("MCB4", 1, 4, f);
    fwrite(&count, sizeof(count), 1, f);
    for (y = 0; y < MC_WY && count > 0; y++)
        for (z = 0; z < MC_WZ && count > 0; z++)
            for (x = 0; x < MC_WX && count > 0; x++)
                if (mc.vox[y][z][x] != mc.base_vox[y][z][x]) {
                    unsigned char rec[4];
                    rec[0] = (unsigned char)x;
                    rec[1] = (unsigned char)y;
                    rec[2] = (unsigned char)z;
                    rec[3] = mc.vox[y][z][x];
                    fwrite(rec, 1, 4, f);
                    count--;
                }
    fclose(f);
}

static void mc_solo_load(void) {
    char path[512], magic[4];
    unsigned short count = 0, i;
    FILE *f;
    net_mc_solo_path(path, sizeof(path));
    f = fopen(path, "rb");
    if (!f)
        return;
    if (fread(magic, 1, 4, f) == 4 && memcmp(magic, "MCB4", 4) == 0 && fread(&count, sizeof(count), 1, f) == 1) {
        for (i = 0; i < count; i++) {
            unsigned char rec[4];
            if (fread(rec, 1, 4, f) != 4)
                break;
            if (mc_in_bounds(rec[0], rec[1], rec[2]))
                mc.vox[rec[1]][rec[2]][rec[0]] = rec[3];
        }
    }
    fclose(f);
}

static void mc_spawn_solo_npcs(void) {
    static const struct {
        float x, z;
        int cls, skin, held;
        const char *name;
    } defs[4] = {
        {29.5f, 27.5f, 1, 1, MC_DIAMOND_BLOCK, "Zombie Azum"},
        {35.5f, 27.5f, 4, 0, MC_TNT, "Builder buK"},
        {27.5f, 25.5f, 3, 0, MC_GOLD_BLOCK, "Astra"},
        {36.5f, 30.5f, 5, 0, MC_EMERALD_BLOCK, "CodeCracker"},
    };
    int i;
    memset(mc.npcs, 0, sizeof(mc.npcs));
    for (i = 0; i < 4; i++) {
        int ix = (int)defs[i].x, iz = (int)defs[i].z;
        mc.npcs[i].active = 1;
        mc.npcs[i].x = defs[i].x;
        mc.npcs[i].z = defs[i].z;
        mc.npcs[i].y = (float)(mc_top_solid_y(ix, iz) + 1);
        mc.npcs[i].max_hp = 10.0f;
        mc.npcs[i].hp = 10.0f;
        mc.npcs[i].skin_idx = defs[i].cls * 10 + defs[i].skin;
        mc.npcs[i].held_block = defs[i].held;
        mc.npcs[i].name = defs[i].name;
    }
}

static void mc_generate_world(const char *room_name, int online_mode) {
    unsigned int seed = mc_seed_for_room(room_name);
    int x, y, z;
    memset(mc.vox, 0, sizeof(mc.vox));
    mc.world_seed = seed;
    mc.is_online_world = online_mode;
    snprintf(mc.world_room, sizeof(mc.world_room), "%s", room_name ? room_name : "");

    for (z = 0; z < MC_WZ; z++) {
        for (x = 0; x < MC_WX; x++) {
            float n = mc_fbm2((float)x, (float)z, seed);
            /* Alpine snow mountain in NE corner (x > 38, z < 26) + lake/river in SW (x < 22) */
            float mdx = ((float)x - 50.0f) / 14.0f, mdz = ((float)z - 13.0f) / 13.0f;
            float mtn = 1.0f - (mdx * mdx + mdz * mdz);
            float rdx = ((float)x - 15.0f + sinf((float)z * 0.11f) * 4.5f) / 7.5f;
            float lake = expf(-rdx * rdx);
            float hf = 9.5f + n * 7.5f + (mtn > 0.0f ? mtn * mtn * 14.5f : 0.0f) - lake * 5.2f;
            int h = (int)floorf(hf);
            if (h < 3)
                h = 3;
            if (h > MC_WY - 6)
                h = MC_WY - 6;

            mc.vox[0][z][x] = MC_BEDROCK;
            for (y = 1; y <= h; y++) {
                unsigned char b = MC_STONE;
                if (y == h) {
                    if (h <= MC_SEA_Y + 1)
                        b = MC_SAND;
                    else if (h >= 21)
                        b = MC_SNOW;
                    else if (h >= 18)
                        b = MC_SNOW_GRASS;
                    else
                        b = MC_GRASS;
                } else if (y >= h - 3) {
                    b = (h <= MC_SEA_Y + 1) ? MC_SANDSTONE : MC_DIRT;
                } else {
                    unsigned int oh = mc_hash3(x / 2, y / 2, z / 2, seed + 991u) % 100u;
                    if (oh < 3 && y < 12)
                        b = MC_DIAMOND_ORE;
                    else if (oh < 8 && y < 18)
                        b = MC_IRON_ORE;
                    else if (oh < 15)
                        b = MC_COAL_ORE;
                }
                mc.vox[y][z][x] = b;
            }
            for (y = h + 1; y <= MC_SEA_Y; y++)
                mc.vox[y][z][x] = (x > 42 && z < 20 && y == MC_SEA_Y) ? MC_ICE : MC_WATER;
        }
    }

    /* Carve natural cave tunnel into mountain base with visible ores & glowstone */
    for (z = 11; z <= 21; z++) {
        int cx = 44 + (int)(sinf((float)z * 0.4f) * 2.0f);
        mc_fill_box(cx - 1, 11, z, cx + 1, 13, z, MC_AIR);
        if (z % 4 == 0)
            mc_raw_set(cx + 1, 13, z, MC_GLOWSTONE);
        mc_raw_set(cx - 2, 11, z, (z % 3 == 0) ? MC_DIAMOND_ORE : MC_IRON_ORE);
    }

    mc_build_spawn_village();

    /* Scatter oak & alpine pine trees outside cabin & water */
    for (z = 4; z < MC_WZ - 4; z += 5) {
        for (x = 4; x < MC_WX - 4; x += 5) {
            int tx = x + (int)(mc_hash3(x, 1, z, seed) % 3u) - 1;
            int tz = z + (int)(mc_hash3(x, 2, z, seed) % 3u) - 1;
            int ty;
            unsigned char ground;
            if (tx >= 23 && tx <= 40 && tz >= 16 && tz <= 34)
                continue;
            if ((mc_hash3(tx, 7, tz, seed) % 100u) > 56u)
                continue;
            ty = mc_top_solid_y(tx, tz);
            ground = mc_get(tx, ty, tz);
            if ((ground == MC_GRASS || ground == MC_SNOW_GRASS) && ty > MC_SEA_Y + 1 && ty < MC_WY - 9)
                mc_plant_tree(tx, ty + 1, tz, ground == MC_SNOW_GRASS);
        }
    }

    memcpy(mc.base_vox, mc.vox, sizeof(mc.vox));
    if (!online_mode)
        mc_solo_load();
    mc_rebuild_glow_lut();
    mc.world_ready = 1;
}

static void mc_init_session(int online_mode, const char *room_name) {
    int slot_off = online_mode && net_slot() >= 0 ? (int)net_slot() : 0;
    mc_generate_world(room_name, online_mode);
    memset(mc.parts, 0, sizeof(mc.parts));
    memset(mc.tnts, 0, sizeof(mc.tnts));
    memset(mc.rem, 0, sizeof(mc.rem));
    mc.px = 32.5f + (float)((slot_off % 2) * 1.6f - 0.8f);
    mc.pz = 29.5f + (float)((slot_off / 2) * 1.4f);
    mc.py = (float)(mc_top_solid_y((int)mc.px, (int)mc.pz) + 1) + 0.02f;
    mc.vx = mc.vy = mc.vz = 0.0f;
    /* Face straight toward the starter cabin, trees, pier & snow mountain! */
    mc.yaw = 3.14159265f;
    mc.pitch = -0.10f;
    mc.on_ground = 1;
    mc.flying = 0;
    mc.sneaking = 0;
    mc.inv_open = 0;
    mc.mine_on = 0;
    mc.mine_prog = 0.0f;
    mc.joy_tid = -1;
    mc.look_tid = -999;
    mc.kills_this_run = 0;
    mc.banner_t = 3.0f;
    if (mc.hotbar[0] == 0) {
        mc.hotbar[0] = MC_GRASS;
        mc.hotbar[1] = MC_PLANKS;
        mc.hotbar[2] = MC_COBBLE;
        mc.hotbar[3] = MC_BRICK;
        mc.hotbar[4] = MC_GLASS;
        mc.hotbar[5] = MC_LOG;
        mc.hotbar[6] = MC_GLOWSTONE;
        mc.hotbar[7] = MC_DIAMOND_BLOCK;
        mc.hotbar[8] = MC_TNT;
    }
    if (!online_mode)
        mc_spawn_solo_npcs();
    else
        memset(mc.npcs, 0, sizeof(mc.npcs));
}
