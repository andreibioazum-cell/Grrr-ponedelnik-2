/* Draws an (unrotated) texture clipped to a rounded-rectangle silhouette, so
 * square card art can get soft corners without having to bake them into the
 * PNG itself. radius is in the same on-screen pixel units as x/y/scale. */
void tex_round(float x, float y, const char *name, float s, float r) {
    if (!frame_open)
        return;
    Texture *t = load_png(name);
    if (!t)
        return;
    DSCmd *p = push(DS_CMD_TEX_ROUND);
    if (!p)
        return;
    p->v.txr.x = x;
    p->v.txr.y = y;
    p->v.txr.sc = s;
    p->v.txr.r = r;
    p->v.txr.tx = t;
}

/* Texture drawn with a plain transparency: the picture keeps its own colours
 * (the vertex colour is white), only the alpha is scaled. Used where something
 * has to appear gradually instead of growing out of nothing. */
void tex_alpha(float x, float y, const char *name, float a, float s, double alpha) {
    if (!frame_open)
        return;
    Texture *t = load_png(name);
    if (!t)
        return;
    double k = alpha;
    if (k < 0) {
        k = 0;
    }
    if (k > 1) {
        k = 1;
    }
    unsigned int byte = (unsigned int)floor(k * 254) + 1;
    if (byte > 255) {
        byte = 255;
    }
    DSCmd *p = push(DS_CMD_TEX_FADE);
    if (!p)
        return;
    p->v.tx2.x = x;
    p->v.tx2.y = y;
    p->v.tx2.a = a;
    p->v.tx2.sc = s;
    p->v.tx2.tx = t;
    p->v.tx2.c = pack_c((byte << 24) | 0x00FFFFFF);
}

void ds_gfx_quad(const char *tex_name, float x0, float y0, float u0, float v0, uint32_t c0, float x1, float y1,
                 float u1, float v1, uint32_t c1, float x2, float y2, float u2, float v2, uint32_t c2, float x3,
                 float y3, float u3, float v3, uint32_t c3) {
    if (!frame_open)
        return;
    Texture *t = tex_name ? load_png(tex_name) : NULL;
    if (tex_name && !t)
        return;
    DSCmd *p = push(DS_CMD_QUAD3D);
    if (!p)
        return;
    p->v.q3.x[0] = x0;
    p->v.q3.y[0] = y0;
    p->v.q3.u[0] = u0;
    p->v.q3.v[0] = v0;
    p->v.q3.c[0] = pack_c(c0);
    p->v.q3.x[1] = x1;
    p->v.q3.y[1] = y1;
    p->v.q3.u[1] = u1;
    p->v.q3.v[1] = v1;
    p->v.q3.c[1] = pack_c(c1);
    p->v.q3.x[2] = x2;
    p->v.q3.y[2] = y2;
    p->v.q3.u[2] = u2;
    p->v.q3.v[2] = v2;
    p->v.q3.c[2] = pack_c(c2);
    p->v.q3.x[3] = x3;
    p->v.q3.y[3] = y3;
    p->v.q3.u[3] = u3;
    p->v.q3.v[3] = v3;
    p->v.q3.c[3] = pack_c(c3);
    p->v.q3.tx = t;
}
