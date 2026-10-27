/* Internal resolution is fixed at 1/1: the app is drawn at the panel's own
 * size, always.
 *
 * There used to be a dynamic scaler here that dropped the render target to as
 * little as 1/6 of the screen whenever frames ran long. It kept the frame
 * time up, but the picture broke into fat blocks - the app then looked
 * broken rather than fast, and once it had coarsened it often stayed coarse
 * for the rest of the session. Budgeting belongs in the draw calls, not in
 * the resolution, so the scaler is gone and only the frame-time report stays:
 * a slow device is still visible in the log, it just no longer costs pixels. */
static int gfx_auto_scale = 1;

int ds_graphics_pixel_scale(void) { return gfx_auto_scale; }

#define GFX_REPORT_SEC 5.0

static double gfx_report_acc;
static int gfx_report_frames;

static void gfx_autoscale_new_window(void) {
    gfx_auto_scale = 1;
    gfx_report_acc = 0;
    gfx_report_frames = 0;
}

static void gfx_autoscale_reset(void) { gfx_autoscale_new_window(); }

void ds_graphics_report_frame_interval(double sec) {
    /* Only sane intervals: a pause or a resume reports whole seconds. */
    if (!(sec > 0.0) || sec > 0.25)
        return;
    gfx_report_acc += sec;
    gfx_report_frames++;
    if (gfx_report_acc < GFX_REPORT_SEC)
        return;
    ds_log("graphics: %.1f fps, frame %.1f ms, internal 1/%d, present %s, swapchain %ux%u, recreates %u",
           (double)gfx_report_frames / gfx_report_acc, 1000.0 * gfx_report_acc / (double)gfx_report_frames,
           gfx_auto_scale, ds_vk_present_mode_name(vk_sw_present_mode), (unsigned)vk_sw_extent.width,
           (unsigned)vk_sw_extent.height, vk_sw_recreates);
    gfx_report_acc = 0;
    gfx_report_frames = 0;
}
