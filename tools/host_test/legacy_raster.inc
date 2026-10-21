#include "engine/runtime.h"
#include <math.h>
#include <string.h>
typedef struct Texture Texture;
struct Texture {
    int w, h, opaque;
    uint32_t *pixels;
};
static uint32_t pack_c(uint32_t c) {
    uint32_t a = (c >> 24) & 0xff, r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
    if (!a)
        a = 255;
    return r | (g << 8) | (b << 16) | (a << 24);
}
static uint32_t blend(uint32_t d, uint32_t s) {
    uint32_t a = (s >> 24) & 0xff;
    if (!a)
        return d;
    if (a == 255)
        return s;
    uint32_t inv = 255 - a;
    uint32_t r = (((s & 0xff) * a + (d & 0xff) * inv + 127) / 255);
    uint32_t g = ((((s >> 8) & 0xff) * a + ((d >> 8) & 0xff) * inv + 127) / 255);
    uint32_t b = ((((s >> 16) & 0xff) * a + ((d >> 16) & 0xff) * inv + 127) / 255);
    uint32_t oa = a + ((((d >> 24) & 0xff) * inv + 127) / 255);
    return r | (g << 8) | (b << 16) | (oa << 24);
}
static int cl_floor(float v, int lim) {
    if (v <= 0)
        return 0;
    if (v >= lim)
        return lim;
    int r = (int)floorf(v);
    return r < 0 ? 0 : r > lim ? lim : r;
}
static int cl_ceil(float v, int lim) {
    if (v <= 0)
        return 0;
    if (v >= lim)
        return lim;
    int r = (int)ceilf(v);
    return r < 0 ? 0 : r > lim ? lim : r;
}
static void fill_span(uint32_t *d, int n, uint32_t c) {
    while (n >= 8) {
        d[0] = c;
        d[1] = c;
        d[2] = c;
        d[3] = c;
        d[4] = c;
        d[5] = c;
        d[6] = c;
        d[7] = c;
        d += 8;
        n -= 8;
    }
    while (n-- > 0) *d++ = c;
}
static void paint_span(uint32_t *d, int n, uint32_t c) {
    if ((c >> 24) >= 255) {
        fill_span(d, n, c);
        return;
    }
    while (n-- > 0) {
        *d = blend(*d, c);
        d++;
    }
}
static void render_rect(Buffer *b, float x, float y, float w, float h, uint32_t c) {
    if (!b || !isfinite(x + y + w + h) || w <= 0 || h <= 0)
        return;
    if (x >= b->width || y >= b->height || x + w <= 0 || y + h <= 0)
        return;
    int l = cl_floor(floorf(x), b->width), t = cl_floor(floorf(y), b->height);
    int r = cl_ceil(ceilf(x + w), b->width), bo = cl_ceil(ceilf(y + h), b->height);
    if ((c >> 24) >= 255) {
        for (int row = t; row < bo; row++) fill_span(b->pixels + row * b->stride + l, r - l, c);
    } else {
        for (int row = t; row < bo; row++) {
            uint32_t *d = b->pixels + row * b->stride + l;
            for (int col = 0; col < r - l; col++) d[col] = blend(d[col], c);
        }
    }
}
static void render_roundrect(Buffer *b, float x, float y, float w, float h, float rad, uint32_t c) {
    if (!b || !isfinite(x + y + w + h + rad) || w <= 0 || h <= 0)
        return;
    if (x >= b->width || y >= b->height || x + w <= 0 || y + h <= 0)
        return;
    if (rad < 0)
        rad = 0;
    if (rad > w * 0.5f)
        rad = w * 0.5f;
    if (rad > h * 0.5f)
        rad = h * 0.5f;
    int t0 = cl_floor(floorf(y), b->height), t1 = cl_ceil(ceilf(y + h), b->height);
    for (int row = t0; row < t1; row++) {
        float yi = (float)row + 0.5f - y, ins = 0;
        if (rad > 0) {
            if (yi < rad) {
                float d = rad - yi;
                if (d > rad)
                    d = rad;
                ins = rad - sqrtf(rad * rad - d * d);
            } else if (yi > h - rad) {
                float d = yi - (h - rad);
                if (d > rad)
                    d = rad;
                ins = rad - sqrtf(rad * rad - d * d);
            }
        }
        int s = (int)ceilf(x + ins);
        if (s < 0)
            s = 0;
        int e = (int)ceilf(x + w - ins);
        if (e > b->width)
            e = b->width;
        if (e > s)
            fill_span(b->pixels + row * b->stride + s, e - s, c);
    }
}
static void render_circle(Buffer *b, float x, float y, float rad, uint32_t c) {
    if (!b || !isfinite(x + y + rad) || rad <= 0)
        return;
    int r = (int)ceilf(rad);
    if (r <= 0)
        return;
    int cx = (int)floorf(x + 0.5f), cy = (int)floorf(y + 0.5f);
    long long r2 = (long long)r * r;
    for (int dy = -r; dy <= r; dy++) {
        int sy = cy + dy;
        if (sy < 0 || sy >= b->height)
            continue;
        int hw = (int)sqrt((double)(r2 - (long long)dy * dy));
        int l = cx - hw, rr = cx + hw + 1;
        if (l < 0)
            l = 0;
        if (rr > b->width)
            rr = b->width;
        if (l < rr)
            paint_span(b->pixels + sy * b->stride + l, rr - l, c);
    }
}
static void render_ring(Buffer *b, float x, float y, float rad, float th, uint32_t c) {
    if (!b || !isfinite(x + y + rad + th) || rad <= 0 || th <= 0)
        return;
    int out = (int)ceilf(rad), in = (int)floorf(rad - th);
    if (in <= 0) {
        render_circle(b, x, y, rad, c);
        return;
    }
    int cx = (int)floorf(x + 0.5f), cy = (int)floorf(y + 0.5f);
    long long o2 = (long long)out * out, i2 = (long long)in * in;
    for (int dy = -out; dy <= out; dy++) {
        int sy = cy + dy;
        if (sy < 0 || sy >= b->height)
            continue;
        int oh = (int)sqrt((double)(o2 - (long long)dy * dy));
        int ih = -1;
        if (abs(dy) <= in)
            ih = (int)sqrt((double)(i2 - (long long)dy * dy));
        int l = cx - oh, rr = cx + oh + 1;
        if (l < 0)
            l = 0;
        if (rr > b->width)
            rr = b->width;
        if (ih < 0) {
            if (l < rr)
                paint_span(b->pixels + sy * b->stride + l, rr - l, c);
        } else {
            int il = cx - ih, ir = cx + ih + 1;
            int lr = il < rr ? il : rr, rl = ir > l ? ir : l;
            if (l < lr)
                paint_span(b->pixels + sy * b->stride + l, lr - l, c);
            if (rl < rr)
                paint_span(b->pixels + sy * b->stride + rl, rr - rl, c);
        }
    }
}
static void render_line(Buffer *b, float x1, float y1, float x2, float y2, float th, uint32_t c) {
    if (!b || !isfinite(x1 + y1 + x2 + y2 + th) || th <= 0)
        return;
    float dx = x2 - x1, dy = y2 - y1, len2 = dx * dx + dy * dy;
    float rad = th * 0.5f, rad2 = rad * rad;
    if (len2 <= 0.0001f) {
        render_circle(b, x1, y1, rad, c);
        return;
    }
    float minx = (x1 < x2 ? x1 : x2) - rad;
    float maxx = (x1 > x2 ? x1 : x2) + rad;
    float miny = (y1 < y2 ? y1 : y2) - rad;
    float maxy = (y1 > y2 ? y1 : y2) + rad;
    int left = cl_floor(floorf(minx), b->width);
    int right = cl_ceil(ceilf(maxx), b->width);
    int top = cl_floor(floorf(miny), b->height);
    int bottom = cl_ceil(ceilf(maxy), b->height);
    for (int py = top; py < bottom; py++) {
        float fy = (float)py + 0.5f;
        for (int px = left; px < right; px++) {
            float fx = (float)px + 0.5f;
            float u = ((fx - x1) * dx + (fy - y1) * dy) / len2;
            if (u < 0 || u > 1)
                continue;
            float ox = x1 + u * dx - fx;
            float oy = y1 + u * dy - fy;
            if (ox * ox + oy * oy <= rad2) {
                uint32_t *pixel = &b->pixels[py * b->stride + px];
                *pixel = blend(*pixel, c);
            }
        }
    }
}
static void draw_tx_unrot(Buffer *b, const Texture *t, float x, float y, float sc) {
    if (!b || !t || !t->pixels || !isfinite(x + y + sc) || sc <= 0)
        return;
    float w = t->w * sc, h = t->h * sc;
    if (x >= b->width || y >= b->height || x + w <= 0 || y + h <= 0)
        return;
    int l = cl_floor(floorf(x), b->width), t0 = cl_floor(floorf(y), b->height);
    int r = cl_ceil(ceilf(x + w), b->width), bo = cl_ceil(ceilf(y + h), b->height);
    if (sc == 1.0f && x == floorf(x) && y == floorf(y) && t->opaque && l == (int)x && t0 == (int)y) {
        int sl = l - (int)x, st = t0 - (int)y;
        for (int sy = t0; sy < bo; sy++) {
            int src_y = st + sy - t0;
            int cw = r - l;
            if (src_y < 0 || src_y >= t->h)
                continue;
            if (sl < 0) {
                cw += sl;
                sl = 0;
            }
            if (sl + cw > t->w)
                cw = t->w - sl;
            if (cw > 0)
                memcpy(b->pixels + sy * b->stride + l, t->pixels + src_y * t->w + sl, (size_t)cw * 4);
        }
        return;
    }
    for (int sy = t0; sy < bo; sy++) {
        int src_y = (int)(((float)sy + 0.5f - y) / sc);
        if (src_y < 0)
            src_y = 0;
        if (src_y >= t->h)
            src_y = t->h - 1;
        for (int sx = l; sx < r; sx++) {
            int src_x = (int)(((float)sx + 0.5f - x) / sc);
            if (src_x < 0)
                src_x = 0;
            if (src_x >= t->w)
                src_x = t->w - 1;
            b->pixels[sy * b->stride + sx] = blend(b->pixels[sy * b->stride + sx], t->pixels[src_y * t->w + src_x]);
        }
    }
}
static void draw_tx_rot(Buffer *b, const Texture *t, float x, float y, float ang, float sc) {
    float hw = t->w * 0.5f * sc, hh = t->h * 0.5f * sc;
    float cx = x + hw, cy = y + hh, ca = cosf(ang), sa = sinf(ang);
    float dx = fabsf(hw * ca) + fabsf(hh * sa), dy = fabsf(hw * sa) + fabsf(hh * ca);
    int l = cl_floor(floorf(cx - dx), b->width), t0 = cl_floor(floorf(cy - dy), b->height);
    int r = cl_ceil(ceilf(cx + dx), b->width), bo = cl_ceil(ceilf(cy + dy), b->height);
    for (int sy = t0; sy < bo; sy++) {
        float py = (float)sy + 0.5f - cy;
        for (int sx = l; sx < r; sx++) {
            float px = (float)sx + 0.5f - cx;
            int tx = (int)floorf((px * ca + py * sa) / sc + t->w * 0.5f);
            int ty = (int)floorf((-px * sa + py * ca) / sc + t->h * 0.5f);
            if (tx < 0 || tx >= t->w || ty < 0 || ty >= t->h)
                continue;
            b->pixels[sy * b->stride + sx] = blend(b->pixels[sy * b->stride + sx], t->pixels[ty * t->w + tx]);
        }
    }
}
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static void draw_tx(Buffer *b, const Texture *t, float x, float y, float a, float sc) {
    if (!b || !t || !t->pixels || !isfinite(x + y + a + sc) || sc <= 0)
        return;
    if (fabsf(a) < 0.0005f) {
        draw_tx_unrot(b, t, x, y, sc);
        return;
    }
    draw_tx_rot(b, t, x, y, a, sc);
}

static void draw_tx_unrot_tint(Buffer *b, const Texture *t, float x, float y, float sc, uint32_t tint) {
    if (!b || !t || !t->pixels || !isfinite(x + y + sc) || sc <= 0)
        return;
    float w = t->w * sc, h = t->h * sc;
    if (x >= b->width || y >= b->height || x + w <= 0 || y + h <= 0)
        return;
    int l = cl_floor(floorf(x), b->width), t0 = cl_floor(floorf(y), b->height);
    int r = cl_ceil(ceilf(x + w), b->width), bo = cl_ceil(ceilf(y + h), b->height);
    uint32_t tr = tint & 0xff, tg = (tint >> 8) & 0xff, tb = (tint >> 16) & 0xff, ta = (tint >> 24) & 0xff;
    for (int sy = t0; sy < bo; sy++) {
        int src_y = (int)(((float)sy + 0.5f - y) / sc);
        if (src_y < 0)
            src_y = 0;
        if (src_y >= t->h)
            src_y = t->h - 1;
        for (int sx = l; sx < r; sx++) {
            int src_x = (int)(((float)sx + 0.5f - x) / sc);
            if (src_x < 0)
                src_x = 0;
            if (src_x >= t->w)
                src_x = t->w - 1;
            uint32_t p = t->pixels[src_y * t->w + src_x];
            uint32_t sa = (p >> 24) & 0xff;
            if (!sa)
                continue;
            uint32_t a = (sa * ta) / 255;
            if (!a)
                continue;
            uint32_t s = tr | (tg << 8) | (tb << 16) | (a << 24);
            b->pixels[sy * b->stride + sx] = blend(b->pixels[sy * b->stride + sx], s);
        }
    }
}
static void draw_tx_rot_tint(Buffer *b, const Texture *t, float x, float y, float ang, float sc, uint32_t tint) {
    float hw = t->w * 0.5f * sc, hh = t->h * 0.5f * sc;
    float cx = x + hw, cy = y + hh, ca = cosf(ang), sa = sinf(ang);
    float dx = fabsf(hw * ca) + fabsf(hh * sa), dy = fabsf(hw * sa) + fabsf(hh * ca);
    int l = cl_floor(floorf(cx - dx), b->width), t0 = cl_floor(floorf(cy - dy), b->height);
    int r = cl_ceil(ceilf(cx + dx), b->width), bo = cl_ceil(ceilf(cy + dy), b->height);
    uint32_t tr = tint & 0xff, tg = (tint >> 8) & 0xff, tb = (tint >> 16) & 0xff, ta = (tint >> 24) & 0xff;
    for (int sy = t0; sy < bo; sy++) {
        float py = (float)sy + 0.5f - cy;
        for (int sx = l; sx < r; sx++) {
            float px = (float)sx + 0.5f - cx;
            int tx = (int)floorf((px * ca + py * sa) / sc + t->w * 0.5f);
            int ty = (int)floorf((-px * sa + py * ca) / sc + t->h * 0.5f);
            if (tx < 0 || tx >= t->w || ty < 0 || ty >= t->h)
                continue;
            uint32_t p = t->pixels[ty * t->w + tx];
            uint32_t sa = (p >> 24) & 0xff;
            if (!sa)
                continue;
            uint32_t a = (sa * ta) / 255;
            if (!a)
                continue;
            uint32_t s = tr | (tg << 8) | (tb << 16) | (a << 24);
            b->pixels[sy * b->stride + sx] = blend(b->pixels[sy * b->stride + sx], s);
        }
    }
}
static void draw_tx_tint(Buffer *b, const Texture *t, float x, float y, float a, float sc, uint32_t tint) {
    if (!b || !t || !t->pixels || !isfinite(x + y + a + sc) || sc <= 0)
        return;
    if (fabsf(a) < 0.0005f) {
        draw_tx_unrot_tint(b, t, x, y, sc, tint);
        return;
    }
    draw_tx_rot_tint(b, t, x, y, a, sc, tint);
}
