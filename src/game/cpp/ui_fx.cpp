// Vertical-gradient rounded buttons for Geometrium.
//
// This is the one spot where C++ earns its place: the gradient needs a little
// analytic geometry per scanline (the row inset of a rounded silhouette) plus
// per-channel ARGB blending, and inline helpers / constexpr keep that math
// readable instead of spilling scratch doubles into the C include chain.
//
// The exported entry point has C linkage and no C++ runtime requirements
// (no exceptions, RTTI, static locals or STL), so it links into the plain
// -std=c99 shared library unchanged.

#include <math.h>
#include <stdint.h>

extern "C" void rect(float x, float y, float w, float h, uint32_t color);

namespace {

// Classic glossy look: a lighter sheen at the top fading into a deeper base.
constexpr float kTopShade = 1.24f;
constexpr float kBottomShade = 0.72f;

constexpr uint32_t kAlphaMask = 0xFF000000u;

inline uint32_t channel_of(uint32_t color, int shift) {
    return (color >> shift) & 0xFFu;
}

inline uint32_t scaled_channel(uint32_t value, float k) {
    float v = static_cast<float>(value) * k;
    if (v > 255.0f) {
        v = 255.0f;
    }
    return static_cast<uint32_t>(v + 0.5f);
}

uint32_t shade_argb(uint32_t color, float k) {
    return (color & kAlphaMask) | (scaled_channel(channel_of(color, 16), k) << 16) |
           (scaled_channel(channel_of(color, 8), k) << 8) |
           scaled_channel(channel_of(color, 0), k);
}

inline uint32_t mix_channel(uint32_t top_c, uint32_t bottom_c, float t) {
    // Subtract in float: uint32_t would underflow when the bottom shade is darker.
    float v = static_cast<float>(top_c) +
              (static_cast<float>(bottom_c) - static_cast<float>(top_c)) * t;
    return static_cast<uint32_t>(v + 0.5f);
}

uint32_t lerp_argb(uint32_t top, uint32_t bottom, float t) {
    uint32_t r = mix_channel(channel_of(top, 16), channel_of(bottom, 16), t);
    uint32_t g = mix_channel(channel_of(top, 8), channel_of(bottom, 8), t);
    uint32_t b = mix_channel(channel_of(top, 0), channel_of(bottom, 0), t);
    return (top & kAlphaMask) | (r << 16) | (g << 8) | b;
}

// Horizontal inset of the rounded silhouette at |dy| from a corner-circle
// center: the exact half-chord of the corner arc, so every strip stays
// inside the same outline roundrect() would produce. Only rows inside the
// corner bands ask for it, so |dy| == rad is the flat top or bottom edge
// itself: the inset there is the whole radius, never zero (a zero inset
// would paint the edge row full width, past both corners).
inline float row_inset(float dy, float rad) {
    float d = fabsf(dy);
    if (d >= rad) {
        return rad;
    }
    return rad - sqrtf(rad * rad - d * d);
}

inline float row_inset_for(float row_center, float cy, float rad) {
    if (rad <= 0.0f) {
        return 0.0f;
    }
    return row_inset(row_center - cy, rad);
}

}  // namespace

extern "C" void cb_button_gradient(float x, float y, float w, float h, float rad, uint32_t color) {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    if (rad > h / 2.0f) {
        rad = h / 2.0f;
    }
    if (rad > w / 2.0f) {
        rad = w / 2.0f;
    }
    const uint32_t top = shade_argb(color, kTopShade);
    const uint32_t bottom = shade_argb(color, kBottomShade);
    const float cy_top = y + rad;
    const float cy_bottom = y + h - rad;
    const float y_end = y + h;
    // Strips follow the pixel grid and are clipped to [y, y + h]. A pressed
    // button is squeezed to a fractional height, and the old loop stepped
    // ceil(h) whole rows from y: the last row then hung below the button,
    // its centre fell past the corner arcs and it was drawn full width - two
    // thin stripes flashing out of the bottom corners on every press.
    const int first = static_cast<int>(floorf(y));
    const int last = static_cast<int>(ceilf(y_end));
    for (int py = first; py < last; ++py) {
        const float r0 = fmaxf(static_cast<float>(py), y);
        const float r1 = fminf(static_cast<float>(py) + 1.0f, y_end);
        if (r1 <= r0) {
            continue;
        }
        // The pixel centre decides what the rasterizer fills; clamped into
        // the clipped strip it never leaves the silhouette, so the inset is
        // always taken inside the corner arcs.
        const float rc = fminf(fmaxf(static_cast<float>(py) + 0.5f, r0), r1);
        const float t = (h > 1.0f) ? (rc - y) / h : 0.0f;
        const uint32_t c = lerp_argb(top, bottom, t);
        // Straight sides between the two corner-circle centers; the arcs
        // clip only the rows above/below them.
        float inset = 0.0f;
        if (rc < cy_top) {
            inset = row_inset_for(rc, cy_top, rad);
        } else if (rc > cy_bottom) {
            inset = row_inset_for(rc, cy_bottom, rad);
        }
        const float sw = w - 2.0f * inset;
        if (sw > 0.0f) {
            rect(x + inset, r0, sw, r1 - r0, c);
        }
    }
}
