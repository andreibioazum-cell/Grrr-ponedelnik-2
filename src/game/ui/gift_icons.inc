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
#define GIFT_RARITY_N 4

static const char *gift_tex(double id) {
    if (id == 1) return "gift_cake.png";
    if (id == 2) return "gift_bear.png";
    if (id == 3) return "gift_rocket.png";
    if (id >= 4) return "gift_emerald.png";
    return "gift_poop.png";
}

static void gift_icon(double id, double x, double y, double s) {
    if (s <= 0) return;
    tex(x, y, gift_tex(id), 0, s / GIFT_TEX_PX);
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

/* White, light blue and violet; the exclusive one is drawn as a rainbow and
 * returns white only so callers always have a plain colour to fall back on. */
static uint32_t gift_rarity_color(double id) {
    double r = gift_rarity(id);
    if (r == 1) return 0xFF5CC8FF;
    if (r == 2) return 0xFFC08CFF;
    return 0xFFFFFFFF;
}

/* Centred rarity-coloured lettering inside a box, with the same baseline the
 * other card labels use (text_in_box). */
static void gift_text_rarity(const char *s, double x, double y, double w, double h, double id, double sc) {
    double tw = text_ink_width(s) * sc;
    double th = text_ink_height(s) * sc;
    double tt = text_ink_top(s) * sc;
    double tx = x + (w - tw) / 2, ty = y + (h - th) / 2 - tt + 1.5;
    if (gift_rarity(id) == 3) text_rainbow(s, tx, ty, sc, 0.0);
    else text_scaled(s, tx, ty, gift_rarity_color(id), sc);
}

/* Left-aligned variant for the one-line cards. */
static void gift_text_rarity_left(const char *s, double x, double y, double h, double id, double sc) {
    double th = text_ink_height(s) * sc;
    double tt = text_ink_top(s) * sc;
    double ty = y + (h - th) / 2 - tt + 1.5;
    if (gift_rarity(id) == 3) text_rainbow(s, x, ty, sc, 0.0);
    else text_scaled(s, x, ty, gift_rarity_color(id), sc);
}
