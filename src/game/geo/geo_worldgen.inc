/* Procedural 3D Geometrium world generation (96 x 48 x 96 voxels), dynamic point light map,
 * architectural spawn plaza, biomes, caves, lakes, cedar trees, and solo world persistence. */

static unsigned int geo_hash3(int x, int y, int z, unsigned int seed) {
    unsigned int h = seed ^ ((unsigned int)x * 374761393u) ^ ((unsigned int)y * 668265263u) ^
                     ((unsigned int)z * 2246822519u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float geo_vnoise2(float x, float z, unsigned int seed) {
    int ix = (int)floorf(x), iz = (int)floorf(z);
    float fx = x - (float)ix, fz = z - (float)iz;
    float ux = fx * fx * (3.0f - 2.0f * fx), uz = fz * fz * (3.0f - 2.0f * fz);
    float a = (float)(geo_hash3(ix, 0, iz, seed) & 65535u) / 65535.0f;
    float b = (float)(geo_hash3(ix + 1, 0, iz, seed) & 65535u) / 65535.0f;
    float c = (float)(geo_hash3(ix, 0, iz + 1, seed) & 65535u) / 65535.0f;
    float d = (float)(geo_hash3(ix + 1, 0, iz + 1, seed) & 65535u) / 65535.0f;
    return (a + (b - a) * ux) + ((c + (d - c) * ux) - (a + (b - a) * ux)) * uz;
}

static float geo_fbm2(float x, float z, unsigned int seed) {
    return geo_vnoise2(x, z, seed) * 0.56f + geo_vnoise2(x * 2.07f, z * 2.07f, seed ^ 0x55aa11u) * 0.29f +
           geo_vnoise2(x * 4.13f, z * 4.13f, seed ^ 0x993377u) * 0.15f;
}

static unsigned int geo_seed_for_room(const char *room) {
    unsigned int h = 2166136261u;
    size_t i;
    if (!room || !room[0])
        return 0x6E07EA1u;
    for (i = 0; room[i]; i++) {
        h ^= (unsigned char)tolower((unsigned char)room[i]);
        h *= 16777619u;
    }
    return h ? h : 0x6E07EA1u;
}

static void geo_raw_set(int x, int y, int z, unsigned char b) {
    if (geo_in_bounds(x, y, z))
        geo.vox[y][z][x] = b;
}

static void geo_fill_box(int x0, int y0, int z0, int x1, int y1, int z1, unsigned char b) {
    int x, y, z;
    for (y = y0; y <= y1; y++)
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++)
                geo_raw_set(x, y, z, b);
}

static void geo_rebuild_lights(void) {
    int x, y, z, dx, dy, dz;
    memset(geo.light_warm, 0, sizeof(geo.light_warm));
    for (y = 0; y < GEO_WY; y++) {
        for (z = 0; z < GEO_WZ; z++) {
            for (x = 0; x < GEO_WX; x++) {
                unsigned char b = geo.vox[y][z][x];
                if (b != GEO_LUMEN && b != GEO_CYAN_LAMP)
                    continue;
                for (dy = -5; dy <= 5; dy++) {
                    if (y + dy < 0 || y + dy >= GEO_WY)
                        continue;
                    for (dz = -5; dz <= 5; dz++) {
                        if (z + dz < 0 || z + dz >= GEO_WZ)
                            continue;
                        for (dx = -5; dx <= 5; dx++) {
                            int md = abs(dx) + abs(dy) + abs(dz);
                            int str;
                            if (x + dx < 0 || x + dx >= GEO_WX || md > 6)
                                continue;
                            str = (7 - md) * 34;
                            if (str > 255)
                                str = 255;
                            if (str > (int)geo.light_warm[y + dy][z + dz][x + dx])
                                geo.light_warm[y + dy][z + dz][x + dx] = (unsigned char)str;
                        }
                    }
                }
            }
        }
    }
}

static int geo_top_solid_y(int x, int z) {
    int y;
    if (x < 0 || x >= GEO_WX || z < 0 || z >= GEO_WZ)
        return GEO_SEA_Y;
    for (y = GEO_WY - 1; y >= 0; y--) {
        if (geo_is_solid_id(geo.vox[y][z][x]))
            return y;
    }
    return 0;
}

static void geo_plant_cedar(int tx, int ty, int tz, int tall) {
    int h = tall ? 6 : 5;
    int y, dx, dz;
    if (tx < 3 || tx >= GEO_WX - 3 || tz < 3 || tz >= GEO_WZ - 3 || ty + h + 2 >= GEO_WY)
        return;
    for (y = 0; y < h; y++)
        geo_raw_set(tx, ty + y, tz, GEO_CEDAR_LOG);
    for (y = ty + h - 3; y <= ty + h + 1; y++) {
        int r = (y >= ty + h) ? 1 : 2;
        for (dz = -r; dz <= r; dz++) {
            for (dx = -r; dx <= r; dx++) {
                if (abs(dx) == 2 && abs(dz) == 2 && y > ty + h - 2)
                    continue;
                if (geo_get(tx + dx, y, tz + dz) == GEO_AIR)
                    geo_raw_set(tx + dx, y, tz + dz, GEO_CANOPY);
            }
        }
    }
}

static void geo_build_spawn_courtyard(void) {
    int cx = GEO_WX / 2, cz = GEO_WZ / 2;
    int py = 15, x, z, k;
    int corners[4][2] = {{-5, -5}, {5, -5}, {-5, 5}, {5, 5}};

    /* Clear airy architectural volume above central meeting courtyard */
    geo_fill_box(cx - 7, py + 1, cz - 7, cx + 7, py + 12, cz + 7, GEO_AIR);
    geo_fill_box(cx - 6, 1, cz - 6, cx + 6, py - 1, cz + 6, GEO_SLATE);

    for (z = -6; z <= 6; z++) {
        for (x = -6; x <= 6; x++) {
            int edge = (abs(x) == 6 || abs(z) == 6);
            int ring_in = (abs(x) == 3 && abs(z) <= 3) || (abs(z) == 3 && abs(x) <= 3);
            geo_raw_set(cx + x, py, cz + z, edge ? GEO_STEEL : (ring_in ? GEO_MARBLE : GEO_BASALT));
        }
    }
    geo_raw_set(cx, py, cz, GEO_LUMEN);

    /* Four fluted marble corner columns with glass arches & glowing lumen caps */
    for (k = 0; k < 4; k++) {
        int px = cx + corners[k][0], pz = cz + corners[k][1];
        geo_fill_box(px, py + 1, pz, px, py + 4, pz, GEO_MARBLE);
        geo_raw_set(px, py + 5, pz, (k % 2 == 0) ? GEO_LUMEN : GEO_CYAN_LAMP);
    }
    /* Architectural glass pavilion wall along north side */
    geo_fill_box(cx - 4, py + 1, cz - 5, cx + 4, py + 3, cz - 5, GEO_GLASS);
    geo_fill_box(cx - 5, py + 4, cz - 5, cx + 5, py + 4, cz - 5, GEO_BASALT);
}

#define GEO_SOLO_MAGIC 0x47454F31u

static void geo_solo_save(void) {
    char path[384];
    FILE *f;
    unsigned int magic = GEO_SOLO_MAGIC;
    int dims[3] = {GEO_WX, GEO_WY, GEO_WZ};
    float pose[5] = {geo.px, geo.py, geo.pz, geo.yaw, geo.pitch};
    net_solo_world_path(path, sizeof(path));
    if (!path[0])
        return;
    f = fopen(path, "wb");
    if (!f)
        return;
    (void)fwrite(&magic, sizeof(magic), 1, f);
    (void)fwrite(dims, sizeof(dims), 1, f);
    (void)fwrite(pose, sizeof(pose), 1, f);
    (void)fwrite(geo.vox, 1, sizeof(geo.vox), f);
    fclose(f);
}

static int geo_solo_load(void) {
    char path[384];
    FILE *f;
    unsigned int magic = 0;
    int dims[3] = {0, 0, 0};
    float pose[5];
    net_solo_world_path(path, sizeof(path));
    if (!path[0])
        return 0;
    f = fopen(path, "rb");
    if (!f)
        return 0;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != GEO_SOLO_MAGIC ||
        fread(dims, sizeof(dims), 1, f) != 1 || dims[0] != GEO_WX || dims[1] != GEO_WY || dims[2] != GEO_WZ ||
        fread(pose, sizeof(pose), 1, f) != 1 || fread(geo.vox, 1, sizeof(geo.vox), f) != sizeof(geo.vox)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    geo.px = pose[0];
    geo.py = pose[1];
    geo.pz = pose[2];
    geo.yaw = pose[3];
    geo.pitch = pose[4];
    geo_rebuild_lights();
    return 1;
}

static void geo_init_world(int online_mode, const char *room_name, int force_fresh) {
    unsigned int seed = geo_seed_for_room(room_name);
    int x, y, z;

    memset(geo.vox, 0, sizeof(geo.vox));
    memset(geo.rem, 0, sizeof(geo.rem));
    memset(geo.charges, 0, sizeof(geo.charges));
    memset(geo.parts, 0, sizeof(geo.parts));
    geo.online = online_mode ? 1 : 0;
    snprintf(geo.room_name, sizeof(geo.room_name), "%s", (room_name && *room_name) ? room_name : GEO_DEFAULT_ROOM);
    geo.mine_on = 0;
    geo.mine_prog = 0.0f;
    geo.third_person = 1;      /* Roblox view: 3rd person by default */
    geo.cam_dist = 5.5f;
    geo.part_sel = -1;
    geo.spectate = 0;
    geo.build_mode = 0;
    geo.players_open = 0;
    geo.inv_open = 0;
    geo.chat_open = 0;
    geo.pause_open = 0;
    geo.flying = 0;
    geo.vx = geo.vy = geo.vz = 0.0f;

    if (!online_mode && !force_fresh && strcmp(geo.room_name, "solo") == 0 && geo_solo_load())
        return;

    for (z = 0; z < GEO_WZ; z++) {
        for (x = 0; x < GEO_WX; x++) {
            float elev = geo_fbm2((float)x * 0.042f, (float)z * 0.042f, seed);
            float ridge = geo_fbm2((float)x * 0.11f + 31.0f, (float)z * 0.11f - 17.0f, seed ^ 0x7711u);
            float temp = geo_fbm2((float)x * 0.026f + 90.0f, (float)z * 0.026f + 40.0f, seed ^ 0x2299u);
            int h = 8 + (int)(elev * 14.0f + ridge * 5.0f);
            if (h < 4)
                h = 4;
            if (h > 30)
                h = 30;

            geo.vox[0][z][x] = GEO_MONOLITH;
            for (y = 1; y <= h; y++) {
                unsigned char b = GEO_SLATE;
                if (y == h) {
                    if (h <= GEO_SEA_Y + 1)
                        b = GEO_DUNE;
                    else if (h >= 22 || temp < 0.28f)
                        b = GEO_FROST;
                    else if (temp > 0.73f)
                        b = GEO_DUNE;
                    else
                        b = GEO_TURF;
                } else if (y >= h - 3) {
                    b = (temp > 0.73f || h <= GEO_SEA_Y + 1) ? GEO_DUNE : GEO_LOAM;
                } else if ((geo_hash3(x, y, z, seed ^ 0xA1B2u) & 31u) == 0u) {
                    b = GEO_AMBER;
                }
                /* Natural underground caves */
                if (y >= 3 && y <= h - 3) {
                    float c1 = geo_vnoise2((float)x * 0.16f + (float)y * 0.21f, (float)z * 0.16f, seed ^ 0xCA7Eu);
                    if (c1 > 0.79f)
                        b = GEO_AIR;
                }
                geo.vox[y][z][x] = b;
            }
            for (y = h + 1; y <= GEO_SEA_Y; y++)
                geo.vox[y][z][x] = (temp < 0.25f && y == GEO_SEA_Y) ? GEO_ICE : GEO_WATER;
        }
    }

    /* Scatter cedar trees & geometric monolith shrines outside central courtyard */
    for (z = 5; z < GEO_WZ - 5; z += 4) {
        for (x = 5; x < GEO_WX - 5; x += 4) {
            unsigned int hh;
            int tx, tz, top;
            if (abs(x - GEO_WX / 2) <= 9 && abs(z - GEO_WZ / 2) <= 9)
                continue;
            hh = geo_hash3(x, 7, z, seed ^ 0x7CEDu);
            tx = x + (int)(hh & 1u);
            tz = z + (int)((hh >> 2) & 1u);
            top = geo_top_solid_y(tx, tz);
            if ((hh & 7u) <= 2u && geo_get(tx, top, tz) == GEO_TURF && top > GEO_SEA_Y)
                geo_plant_cedar(tx, top + 1, tz, (int)(hh & 1u));
            else if ((hh & 63u) == 7u && top > GEO_SEA_Y) {
                geo_fill_box(tx, top + 1, tz, tx, top + 3, tz, GEO_OBSIDIAN);
                geo_raw_set(tx, top + 4, tz, GEO_LUMEN);
            }
        }
    }

    geo_build_spawn_courtyard();
    geo_rebuild_lights();

    geo.px = (float)(GEO_WX / 2) + 0.5f;
    geo.pz = (float)(GEO_WZ / 2) + 4.5f;
    geo.py = (float)geo_top_solid_y((int)geo.px, (int)geo.pz) + 1.01f;
    geo.yaw = 3.14159265f;
    geo.face_yaw = geo.yaw;
    geo.pitch = -0.10f;
}
