/* Real 3D Minecraft sky, sun & clouds, exact 8-octant back-to-front voxel
 * renderer with smooth AO, articulated 6-box 3D characters, cracks & hand. */

#define MC_RENDER_DIST 28

static int mc_ao_solid(int x, int y, int z) {
    unsigned char b = mc_get(x, y, z);
    return (mc_blocks[b].flags & (MC_F_OPAQUE | MC_F_CUTOUT)) != 0;
}

static float mc_vertex_ao(int s1, int s2, int corner) {
    int occ = (s1 && s2) ? 3 : (s1 + s2 + corner);
    static const float lut[4] = {1.00f, 0.82f, 0.66f, 0.48f};
    return lut[occ];
}

static float mc_point_glow(int x, int y, int z) {
    return mc_in_bounds(x, y, z) ? (float)mc.glow_lut[y][z][x] / 255.0f : 0.0f;
}

/* Emits one visible face of block (x, y, z).
 * face: 0=+Y top, 1=-Y bottom, 2=+Z, 3=-Z, 4=+X, 5=-X.
 * Vertex winding keeps UV(0,0)->UV(1,0) strictly along the TOP edge of side faces
 * so grass fringes, bricks, bookshelves, crafting tables & "TNT" text stand UPRIGHT! */
static void mc_draw_block_face(int x, int y, int z, unsigned char b, int face, float fog_k, int crack_stage) {
    float wx[4], wy[4], wz[4], vl[4];
    float fx = (float)x, fy = (float)y, fz = (float)z;
    float y_top = (b == MC_WATER) ? fy + 0.88f : fy + 1.0f;
    float base_sun = 1.0f;
    float glow = (mc_blocks[b].flags & MC_F_GLOW) ? 0.55f : 0.0f;
    float alpha = (b == MC_WATER) ? 190.0f : ((b == MC_ICE) ? 215.0f : 255.0f);
    int i, tile = mc_face_tile(b, face);

    if (face == 0) { /* +Y Top */
        base_sun = 1.00f;
        wx[0] = fx;       wy[0] = y_top; wz[0] = fz;
        wx[1] = fx + 1.f; wy[1] = y_top; wz[1] = fz;
        wx[2] = fx + 1.f; wy[2] = y_top; wz[2] = fz + 1.f;
        wx[3] = fx;       wy[3] = y_top; wz[3] = fz + 1.f;
        vl[0] = mc_vertex_ao(mc_ao_solid(x - 1, y + 1, z), mc_ao_solid(x, y + 1, z - 1), mc_ao_solid(x - 1, y + 1, z - 1));
        vl[1] = mc_vertex_ao(mc_ao_solid(x + 1, y + 1, z), mc_ao_solid(x, y + 1, z - 1), mc_ao_solid(x + 1, y + 1, z - 1));
        vl[2] = mc_vertex_ao(mc_ao_solid(x + 1, y + 1, z), mc_ao_solid(x, y + 1, z + 1), mc_ao_solid(x + 1, y + 1, z + 1));
        vl[3] = mc_vertex_ao(mc_ao_solid(x - 1, y + 1, z), mc_ao_solid(x, y + 1, z + 1), mc_ao_solid(x - 1, y + 1, z + 1));
    } else if (face == 1) { /* -Y Bottom */
        base_sun = 0.50f;
        wx[0] = fx;       wy[0] = fy; wz[0] = fz + 1.f;
        wx[1] = fx + 1.f; wy[1] = fy; wz[1] = fz + 1.f;
        wx[2] = fx + 1.f; wy[2] = fy; wz[2] = fz;
        wx[3] = fx;       wy[3] = fy; wz[3] = fz;
        vl[0] = vl[1] = vl[2] = vl[3] = 0.92f;
    } else if (face == 2) { /* +Z South */
        base_sun = 0.83f;
        wx[0] = fx;       wy[0] = y_top; wz[0] = fz + 1.f;
        wx[1] = fx + 1.f; wy[1] = y_top; wz[1] = fz + 1.f;
        wx[2] = fx + 1.f; wy[2] = fy;    wz[2] = fz + 1.f;
        wx[3] = fx;       wy[3] = fy;    wz[3] = fz + 1.f;
        vl[0] = mc_vertex_ao(mc_ao_solid(x - 1, y, z + 1), mc_ao_solid(x, y + 1, z + 1), mc_ao_solid(x - 1, y + 1, z + 1));
        vl[1] = mc_vertex_ao(mc_ao_solid(x + 1, y, z + 1), mc_ao_solid(x, y + 1, z + 1), mc_ao_solid(x + 1, y + 1, z + 1));
        vl[2] = mc_vertex_ao(mc_ao_solid(x + 1, y, z + 1), mc_ao_solid(x, y - 1, z + 1), mc_ao_solid(x + 1, y - 1, z + 1));
        vl[3] = mc_vertex_ao(mc_ao_solid(x - 1, y, z + 1), mc_ao_solid(x, y - 1, z + 1), mc_ao_solid(x - 1, y - 1, z + 1));
    } else if (face == 3) { /* -Z North */
        base_sun = 0.78f;
        wx[0] = fx + 1.f; wy[0] = y_top; wz[0] = fz;
        wx[1] = fx;       wy[1] = y_top; wz[1] = fz;
        wx[2] = fx;       wy[2] = fy;    wz[2] = fz;
        wx[3] = fx + 1.f; wy[3] = fy;    wz[3] = fz;
        vl[0] = mc_vertex_ao(mc_ao_solid(x + 1, y, z - 1), mc_ao_solid(x, y + 1, z - 1), mc_ao_solid(x + 1, y + 1, z - 1));
        vl[1] = mc_vertex_ao(mc_ao_solid(x - 1, y, z - 1), mc_ao_solid(x, y + 1, z - 1), mc_ao_solid(x - 1, y + 1, z - 1));
        vl[2] = mc_vertex_ao(mc_ao_solid(x - 1, y, z - 1), mc_ao_solid(x, y - 1, z - 1), mc_ao_solid(x - 1, y - 1, z - 1));
        vl[3] = mc_vertex_ao(mc_ao_solid(x + 1, y, z - 1), mc_ao_solid(x, y - 1, z - 1), mc_ao_solid(x + 1, y - 1, z - 1));
    } else if (face == 4) { /* +X East */
        base_sun = 0.68f;
        wx[0] = fx + 1.f; wy[0] = y_top; wz[0] = fz + 1.f;
        wx[1] = fx + 1.f; wy[1] = y_top; wz[1] = fz;
        wx[2] = fx + 1.f; wy[2] = fy;    wz[2] = fz;
        wx[3] = fx + 1.f; wy[3] = fy;    wz[3] = fz + 1.f;
        vl[0] = mc_vertex_ao(mc_ao_solid(x + 1, y, z + 1), mc_ao_solid(x + 1, y + 1, z), mc_ao_solid(x + 1, y + 1, z + 1));
        vl[1] = mc_vertex_ao(mc_ao_solid(x + 1, y, z - 1), mc_ao_solid(x + 1, y + 1, z), mc_ao_solid(x + 1, y + 1, z - 1));
        vl[2] = mc_vertex_ao(mc_ao_solid(x + 1, y, z - 1), mc_ao_solid(x + 1, y - 1, z), mc_ao_solid(x + 1, y - 1, z - 1));
        vl[3] = mc_vertex_ao(mc_ao_solid(x + 1, y, z + 1), mc_ao_solid(x + 1, y - 1, z), mc_ao_solid(x + 1, y - 1, z + 1));
    } else { /* -X West */
        base_sun = 0.65f;
        wx[0] = fx; wy[0] = y_top; wz[0] = fz;
        wx[1] = fx; wy[1] = y_top; wz[1] = fz + 1.f;
        wx[2] = fx; wy[2] = fy;    wz[2] = fz + 1.f;
        wx[3] = fx; wy[3] = fy;    wz[3] = fz;
        vl[0] = mc_vertex_ao(mc_ao_solid(x - 1, y, z - 1), mc_ao_solid(x - 1, y + 1, z), mc_ao_solid(x - 1, y + 1, z - 1));
        vl[1] = mc_vertex_ao(mc_ao_solid(x - 1, y, z + 1), mc_ao_solid(x - 1, y + 1, z), mc_ao_solid(x - 1, y + 1, z + 1));
        vl[2] = mc_vertex_ao(mc_ao_solid(x - 1, y, z + 1), mc_ao_solid(x - 1, y - 1, z), mc_ao_solid(x - 1, y - 1, z + 1));
        vl[3] = mc_vertex_ao(mc_ao_solid(x - 1, y, z - 1), mc_ao_solid(x - 1, y - 1, z), mc_ao_solid(x - 1, y - 1, z - 1));
    }

    if (glow == 0.0f && fog_k < 0.55f)
        glow = mc_point_glow(x, y, z);

    for (i = 0; i < 4; i++) {
        float lit = vl[i] * base_sun + glow;
        vl[i] = lit > 1.0f ? 1.0f : lit;
    }
    mc_draw_quad3d(wx, wy, wz, tile, vl, alpha, fog_k);

    if (crack_stage >= 0 && crack_stage <= 9) {
        float cl[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        mc_draw_quad3d(wx, wy, wz, 240 + crack_stage, cl, 230.0f, 0.0f);
    }
}

static int mc_face_visible(unsigned char self_b, int nx, int ny, int nz) {
    unsigned char nb = mc_get(nx, ny, nz);
    if (nb == MC_AIR)
        return 1;
    if (nb == self_b && (self_b == MC_WATER || self_b == MC_ICE || self_b == MC_GLASS))
        return 0;
    return !mc_is_opaque_id(nb);
}

static void mc_draw_line3d(float x0, float y0, float z0, float x1, float y1, float z1, float thick, uint32_t col) {
    float sx0, sy0, sx1, sy1, dx, dy, len;
    if (!mc_project_point(x0, y0, z0, &sx0, &sy0, NULL) || !mc_project_point(x1, y1, z1, &sx1, &sy1, NULL))
        return;
    dx = sx1 - sx0;
    dy = sy1 - sy0;
    len = sqrtf(dx * dx + dy * dy);
    if (len < 0.5f || len > 2400.0f)
        return;
    rect_rot(0.5f * (sx0 + sx1) - 0.5f * len, 0.5f * (sy0 + sy1) - 0.5f * thick, len, thick, atan2f(dy, dx), col);
}

static void mc_draw_wire_cube(int bx, int by, int bz, uint32_t col) {
    const float pad = 0.008f;
    float x0 = (float)bx - pad, y0 = (float)by - pad, z0 = (float)bz - pad;
    float x1 = (float)bx + 1.0f + pad, y1 = (float)by + 1.0f + pad, z1 = (float)bz + 1.0f + pad;
    mc_draw_line3d(x0, y0, z0, x1, y0, z0, 2.4f, col);
    mc_draw_line3d(x1, y0, z0, x1, y0, z1, 2.4f, col);
    mc_draw_line3d(x1, y0, z1, x0, y0, z1, 2.4f, col);
    mc_draw_line3d(x0, y0, z1, x0, y0, z0, 2.4f, col);
    mc_draw_line3d(x0, y1, z0, x1, y1, z0, 2.4f, col);
    mc_draw_line3d(x1, y1, z0, x1, y1, z1, 2.4f, col);
    mc_draw_line3d(x1, y1, z1, x0, y1, z1, 2.4f, col);
    mc_draw_line3d(x0, y1, z1, x0, y1, z0, 2.4f, col);
    mc_draw_line3d(x0, y0, z0, x0, y1, z0, 2.4f, col);
    mc_draw_line3d(x1, y0, z0, x1, y1, z0, 2.4f, col);
    mc_draw_line3d(x1, y0, z1, x1, y1, z1, 2.4f, col);
    mc_draw_line3d(x0, y0, z1, x0, y1, z1, 2.4f, col);
}

