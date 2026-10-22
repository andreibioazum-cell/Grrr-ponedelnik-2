/* Black-and-white filter and screen shake helpers. The state lives in types.inc
 * so the color packer and the geometry layer can see it too. */

static void gray_texture_pixels(uint32_t *px, size_t n) {
    for (size_t i = 0; i < n; i++) {
        uint32_t c = px[i];
        uint32_t y = (299 * (c & 0xff) + 587 * ((c >> 8) & 0xff) + 114 * ((c >> 16) & 0xff)) / 1000;
        px[i] = (c & 0xff000000u) | (y << 16) | (y << 8) | y;
    }
}

/* Put the original pixels back, apply the filter if it is on, and upload the
 * texture to the GPU again. The original is hidden once and never overwritten. */
static void texture_refresh(Texture *t) {
    size_t n = (size_t)t->w * (size_t)t->h;
    if (t->gray_backup) {
        memcpy(t->pixels, t->gray_backup, n * 4);
    } else {
        t->gray_backup = (uint32_t *)malloc(n * 4);
        if (!t->gray_backup)
            return;
        memcpy(t->gray_backup, t->pixels, n * 4);
    }
    if (gfx_gray_on)
        gray_texture_pixels(t->pixels, n);
    if (!gfx_gray_on) {
        free(t->gray_backup);
        t->gray_backup = NULL;
    }
    if (t->gpu.uploaded) {
        ds_vk_texture_gpu_release(t);
        ds_vk_pending_texture(t);
    }
}

/* Desaturate a freshly decoded texture when a filter is already on, keeping
 * its original colors for the eventual restore. */
static void texture_gray_apply(Texture *t) {
    if (!t || !t->pixels)
        return;
    if (!gfx_gray_on)
        return;
    size_t n = (size_t)t->w * (size_t)t->h;
    t->gray_backup = (uint32_t *)malloc(n * 4);
    if (!t->gray_backup)
        return;
    memcpy(t->gray_backup, t->pixels, n * 4);
    gray_texture_pixels(t->pixels, n);
}

void ds_gfx_gray(double on) {
    int want = on != 0;
    if (want == gfx_gray_on)
        return;
    gfx_gray_on = want;
    /* Swap the CPU-side texture pixels and re-upload them: the grayscale
     * happens on the stored image data, not in the prebuilt shaders. */
    for (Texture *t = textures; t; t = t->next) {
        if (!t->pixels)
            continue;
        texture_refresh(t);
    }
    ds_log("gfx: gray filter %s", want ? "on" : "off");
}

void ds_gfx_shake(double amp, double seconds) {
    if (seconds <= 0 || amp <= 0) {
        gfx_shake_amp = 0;
        gfx_shake_left = 0;
        gfx_shake_dur = 0;
        gfx_shake_dx = 0;
        gfx_shake_dy = 0;
        return;
    }
    /* An ongoing stronger shake is not cut short by a weaker trigger. */
    if (gfx_shake_left > 0 && gfx_shake_amp >= (float)amp)
        return;
    gfx_shake_amp = (float)amp;
    gfx_shake_dur = (float)seconds;
    gfx_shake_left = (float)seconds;
}
