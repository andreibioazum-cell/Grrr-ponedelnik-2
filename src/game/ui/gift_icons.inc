/* Sticker icons drawn from primitives (rect, roundrect, circle, ring, line,
 * rect_rot). The pixel font has no ✿/♥/★ glyphs, so the shop never asks it
 * for a symbol: every gift is a small hand-drawn picture instead.
 *
 * All helpers take the top-left corner and the size of a square box, so the
 * same drawing scales from a 22 px list row to a 54 px shop card. */

static void gift_icon_poop(double x, double y, double s) {
    uint32_t dark = 0xFF5A3618, mid = 0xFF7A4A22, lite = 0xFF9C642F;
    double cx = x + s * 0.5;
    double base = y + s * 0.86;
    /* Three dollops, widest at the bottom, with a little swirl on top. */
    roundrect(x + s * 0.08, base - s * 0.30, s * 0.84, s * 0.30, s * 0.15, dark);
    roundrect(x + s * 0.17, base - s * 0.51, s * 0.66, s * 0.27, s * 0.135, mid);
    roundrect(x + s * 0.27, base - s * 0.69, s * 0.46, s * 0.24, s * 0.12, lite);
    circle(x + s * 0.72, base - s * 0.72, s * 0.09, lite);
    /* A face makes it read as a sticker, not a smudge. */
    circle(cx - s * 0.10, base - s * 0.38, s * 0.058, 0xFFFFFFFF);
    circle(cx + s * 0.10, base - s * 0.38, s * 0.058, 0xFFFFFFFF);
    circle(cx - s * 0.10, base - s * 0.375, s * 0.026, 0xFF241206);
    circle(cx + s * 0.10, base - s * 0.375, s * 0.026, 0xFF241206);
    line(cx - s * 0.07, base - s * 0.25, cx + s * 0.07, base - s * 0.25, s * 0.03, 0xFFE8C9A8);
}

static void gift_icon_cake(double x, double y, double s) {
    uint32_t sponge = 0xFFE0A45C, icing = 0xFFF7E6F1, plate = 0xFFD3D3E2;
    double cx = x + s * 0.5;
    roundrect(x + s * 0.06, y + s * 0.82, s * 0.88, s * 0.10, s * 0.05, plate);
    roundrect(x + s * 0.16, y + s * 0.48, s * 0.68, s * 0.36, s * 0.06, sponge);
    roundrect(x + s * 0.14, y + s * 0.36, s * 0.72, s * 0.14, s * 0.07, icing);
    circle(x + s * 0.28, y + s * 0.49, s * 0.05, icing);
    circle(x + s * 0.50, y + s * 0.52, s * 0.045, icing);
    circle(x + s * 0.70, y + s * 0.49, s * 0.05, icing);
    rect(x + s * 0.47, y + s * 0.16, s * 0.06, s * 0.22, 0xFFE0526B);
    circle(cx, y + s * 0.12, s * 0.06, 0xFFFFC24D);
    circle(cx, y + s * 0.115, s * 0.028, 0xFFFFF3B0);
}

static void gift_icon_bear(double x, double y, double s) {
    uint32_t fur = 0xFF7B4B28, light = 0xFFC79A72, muzzle = 0xFFE9C9A6, ink = 0xFF241610;
    circle(x + s * 0.23, y + s * 0.26, s * 0.12, fur);
    circle(x + s * 0.77, y + s * 0.26, s * 0.12, fur);
    circle(x + s * 0.23, y + s * 0.26, s * 0.06, light);
    circle(x + s * 0.77, y + s * 0.26, s * 0.06, light);
    circle(x + s * 0.50, y + s * 0.56, s * 0.29, fur);
    roundrect(x + s * 0.33, y + s * 0.54, s * 0.34, s * 0.28, s * 0.14, muzzle);
    circle(x + s * 0.38, y + s * 0.46, s * 0.036, ink);
    circle(x + s * 0.62, y + s * 0.46, s * 0.036, ink);
    roundrect(x + s * 0.44, y + s * 0.60, s * 0.12, s * 0.09, s * 0.045, ink);
    line(x + s * 0.50, y + s * 0.69, x + s * 0.50, y + s * 0.74, s * 0.022, ink);
    line(x + s * 0.50, y + s * 0.74, x + s * 0.44, y + s * 0.77, s * 0.02, ink);
    line(x + s * 0.50, y + s * 0.74, x + s * 0.56, y + s * 0.77, s * 0.02, ink);
}

static void gift_icon_rocket(double x, double y, double s) {
    uint32_t body = 0xFFE6E6F0, red = 0xFFE0526B, glass = 0xFF7FE3FF, rim = 0xFF3A9FC2;
    double cx = x + s * 0.5;
    /* Exhaust first, so the body covers its top. */
    circle(cx, y + s * 0.87, s * 0.10, 0xFFFFB74D);
    circle(cx, y + s * 0.91, s * 0.055, 0xFFFFF176);
    rect_rot(x + s * 0.20, y + s * 0.52, s * 0.14, s * 0.24, 0.55, red);
    rect_rot(x + s * 0.66, y + s * 0.52, s * 0.14, s * 0.24, -0.55, red);
    roundrect(x + s * 0.37, y + s * 0.20, s * 0.26, s * 0.62, s * 0.13, body);
    circle(cx, y + s * 0.22, s * 0.12, red);
    ring(cx, y + s * 0.45, s * 0.075, s * 0.03, rim);
    circle(cx, y + s * 0.45, s * 0.055, glass);
}

static void gift_icon(double i, double x, double y, double s) {
    int k = (int)i;
    if (s <= 0) {
        return;
    }
    if (k == 0) {
        gift_icon_poop(x, y, s);
    } else if (k == 1) {
        gift_icon_cake(x, y, s);
    } else if (k == 2) {
        gift_icon_bear(x, y, s);
    } else {
        gift_icon_rocket(x, y, s);
    }
}

/* Width of the little heap: one icon per owned sticker, up to three, laid
 * out with a slight overlap so any amount reads as "a pile", never a number. */
static double gift_pile_w(double s, double count) {
    int n = count >= 3 ? 3 : (count >= 1 ? (int)count : 0);
    return n > 0 ? s + (n - 1) * s * 0.34 : 0;
}

/* Draws the heap with its right edge at `right`. The back icons go first so
 * the front one is fully visible. */
static void gift_pile(double i, double right, double y, double s, double count) {
    int n = count >= 3 ? 3 : (count >= 1 ? (int)count : 0);
    int k;
    /* Back to front, each next sticker half a step lower and further right, so
     * the heap grows downwards like a real stack instead of a row of stamps. */
    for (k = n - 1; k >= 0; k--) {
        gift_icon(i, right - s - k * s * 0.34, y + k * s * 0.17, s);
    }
}
