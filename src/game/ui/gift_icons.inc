/* Sticker pictures ship as PNG sprites (assets/textures/gift_*.png, drawn by
 * tools/gen_gift_textures.py), so the shop shows real artwork instead of
 * shapes assembled from rects and circles. Every sprite is 192x192 with a
 * transparent background and is drawn at the size of the card slot.
 *
 * Rarity gives each sticker a colour for its lettering:
 *   0 common    white
 *   1 rare      light blue
 *   2 epic      violet
 *   3 exclusive rainbow (every glyph takes the next hue step) */

#define GIFT_TEX_PX 192.0

static const char *gift_tex(double id) {
    if (id == 1) return "gift_cake.png";
    if (id == 2) return "gift_bear.png";
    if (id == 3) return "gift_rocket.png";
    if (id >= 4) return "gift_emerald.png";
    return "gift_poop.png";
}

/* Emerald hue: 0 is the plain rainbow, 1..7 the one the admin picked. */
static double gift_emerald_base_hue(void) {
    double h = net_gift_hue();
    if (h < 0) h = 0;
    if (h > 7) h = 7;
    return h / 8.0;
}

/* The hue drifts slowly: the emerald shimmers. */
static double gift_emerald_phase(void) {
    return gift_emerald_base_hue() + ds_frame_seconds() * 0.08;
}

/* The hue colour comes from the same wheel as the rainbow letters. */
static uint32_t gift_hue_color(double hue) {
    double h = hue - floor(hue);
    double r = fabs(h * 6 - 3) - 1, g = 2 - fabs(h * 6 - 2), b = 2 - fabs(h * 6 - 4);
    if (r < 0) r = 0;
    if (r > 1) r = 1;
    if (g < 0) g = 0;
    if (g > 1) g = 1;
    if (b < 0) b = 0;
    if (b > 1) b = 1;
    return 0xFF000000u | ((uint32_t)(r * 255) << 16) | ((uint32_t)(g * 255) << 8) | (uint32_t)(b * 255);
}

/* The emerald: the white stone plus a shimmering layer of the hue, so the
 * colour flows over it and drifts with time. The tint reads only on a white
 * base - on a coloured sprite the hue would just turn to mud. */
static void gift_icon(double id, double x, double y, double s) {
    if (s <= 0) return;
    tex(x, y, gift_tex(id), 0, s / GIFT_TEX_PX);
    if (id >= 4) {
        double a = 110 + 45 * sin(ds_frame_seconds() * 2.4);
        if (a < 0) a = 0;
        if (a > 180) a = 180;
        uint32_t c = gift_hue_color(gift_emerald_phase());
        tex_tint(x, y, gift_tex(id), 0, s / GIFT_TEX_PX, (uint32_t)(((unsigned)(int)a) << 24) | (c & 0x00FFFFFFu));
    }
}

static double gift_rarity(double id) {
    if (id >= 4) return 3;
    if (id >= 2) return 2;
    if (id == 1) return 1;
    return 0;
}

static const char *gift_rarity_name(double id) {
    double r = gift_rarity(id);
    if (language == 1) {
        if (r == 1) return "РЕДКИЙ";
        if (r == 2) return "ЭПИЧЕСКИЙ";
        if (r == 3) return "ЭКСКЛЮЗИВНЫЙ";
        return "ОБЫЧНЫЙ";
    }
    if (r == 1) return "RARE";
    if (r == 2) return "EPIC";
    if (r == 3) return "EXCLUSIVE";
    return "COMMON";
}

/* White, light blue and violet; the exclusive emerald is signed white as
 * well - its colour lives on the stone itself, which drifts through the hue
 * wheel, not in the lettering. */
static uint32_t gift_rarity_color(double id) {
    double r = gift_rarity(id);
    if (r == 1) return 0xFF5CC8FF;
    if (r == 2) return 0xFFC08CFF;
    return 0xFFFFFFFF;
}

/* Centred rarity-coloured lettering inside a box, with the same baseline the
 * other card labels use (text_in_box). The exclusive emerald is signed in
 * plain white: the colour belongs to the stone itself, which drifts through
 * the hue wheel on its own. */
static void gift_text_rarity(const char *s, double x, double y, double w, double h, double id, double sc) {
    double tw = text_ink_width(s) * sc;
    double th = text_ink_height(s) * sc;
    double tt = text_ink_top(s) * sc;
    double tx = x + (w - tw) / 2, ty = y + (h - th) / 2 - tt + 1.5;
    text_scaled(s, tx, ty, gift_rarity_color(id), sc);
}

/* Left-aligned variant for the one-line cards. */
static void gift_text_rarity_left(const char *s, double x, double y, double h, double id, double sc) {
    double th = text_ink_height(s) * sc;
    double tt = text_ink_top(s) * sc;
    double ty = y + (h - th) / 2 - tt + 1.5;
    text_scaled(s, x, ty, gift_rarity_color(id), sc);
}
