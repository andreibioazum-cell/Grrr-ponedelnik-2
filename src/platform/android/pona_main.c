/* Android NativeActivity host for the Pona 2D runtime. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/keycodes.h>

#include "engine/runtime.h"
#include "engine2d/app.h"
#include "engine2d/input.h"

#include <time.h>

static int renderer_ready;
static int app_active;
static int app_started_once;
static int app_focused = 1;
static int screen_width_px;
static int screen_height_px;
static AAssetManager *app_assets;
static uint64_t previous_frame_ns;
static uint64_t previous_loop_ns;
static uint64_t renderer_retry_at_ns;
static uint64_t script_restart_at_ns;
static unsigned int script_restart_count;
static double measured_frame_seconds;

static uint64_t monotonic_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
}

static void apply_screen_size(void) {
    if (screen_width_px > 0 && screen_height_px > 0) {
        screen_w = screen_width_px;
        screen_h = screen_height_px;
    }
}

static void protected_script_init(void *userdata) {
    pond_app_init((AAssetManager *)userdata);
}

static void protected_script_stop(void *userdata) {
    (void)userdata;
    pond_app_stop();
}

static void protected_script_update(void *userdata) {
    (void)userdata;
    pond_app_update();
}

static void protected_script_draw(void *userdata) {
    pond_app_draw((Buffer *)userdata);
}

typedef struct {
    float x;
    float y;
    int down;
} TouchCall;

static void protected_script_touch(void *userdata) {
    const TouchCall *call = (const TouchCall *)userdata;
    pond_app_touch(call->x, call->y, call->down);
}

static void protected_script_key(void *userdata) {
    const int *key_and_down = (const int *)userdata;
    pond_app_key(key_and_down[0], key_and_down[1]);
}

static void mark_script_failed(const char *hook) {
    const char *message = ds_runtime_error_message();
    unsigned int shift = script_restart_count < 5u ? script_restart_count : 5u;
    uint64_t delay = 1000000000ull << shift;
    ds_log_err("Pona hook '%s' stopped: %s; restarting shortly", hook, message);
    ds_console_log(1, "Pona error in %s: %s", hook, message);
    app_active = 0;
    ds_request_app_restart();
    script_restart_at_ns = monotonic_ns() + delay;
    ++script_restart_count;
}

static int start_script(int reset_state) {
    ds_clear_runtime_error();
    ds_clear_app_restart();
    if (reset_state) {
        if (!ds_call_protected(protected_script_stop, NULL, "stop")) {
            ds_log_err("Pona stop hook failed: %s", ds_runtime_error_message());
            ds_clear_runtime_error();
        }
    }
    app_started_once = 1;
    if (!ds_call_protected(protected_script_init, app_assets, "init")) {
        mark_script_failed("init");
        return 0;
    }
    ds_clear_runtime_error();
    app_active = 1;
    app_started_once = 1;
    script_restart_count = 0;
    script_restart_at_ns = 0;
    previous_frame_ns = 0;
    return 1;
}

static void restart_script_if_due(void) {
    if (app_active || !ds_app_restart_requested() || !renderer_ready) return;
    if (monotonic_ns() < script_restart_at_ns) return;
    (void)start_script(app_started_once);
}

static void start_window(struct android_app *app) {
    if (!app || !app->window) {
        renderer_ready = 0;
        return;
    }
    screen_width_px = ANativeWindow_getWidth(app->window);
    screen_height_px = ANativeWindow_getHeight(app->window);
    if (screen_width_px <= 0 || screen_height_px <= 0) {
        ds_log_err("renderer start deferred: invalid window size %dx%d", screen_width_px, screen_height_px);
        renderer_retry_at_ns = monotonic_ns() + 500000000ull;
        return;
    }
    apply_screen_size();
    app_assets = app->activity ? app->activity->assetManager : NULL;
    if (!app_assets) {
        ds_log_err("renderer start deferred: Android asset manager unavailable");
        renderer_retry_at_ns = monotonic_ns() + 1000000000ull;
        return;
    }
    if (!ds_graphics_init(app_assets, app->window)) {
        renderer_ready = 0;
        ds_log_err("Vulkan renderer failed to start: %s", ds_graphics_failure());
        renderer_retry_at_ns = monotonic_ns() + 1000000000ull;
        return;
    }
    renderer_ready = 1;
    renderer_retry_at_ns = 0;
    previous_frame_ns = 0;
    previous_loop_ns = 0;
    measured_frame_seconds = 0.0;
    ds_log("Pona 2D engine ready (%dx%d)", screen_width_px, screen_height_px);
    if (!app_active) (void)start_script(app_started_once);
}

static void handle_command(struct android_app *app, int32_t command) {
    if (!app) return;
    switch (command) {
    case APP_CMD_INIT_WINDOW:
        renderer_retry_at_ns = 0;
        start_window(app);
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONTENT_RECT_CHANGED:
    case APP_CMD_CONFIG_CHANGED:
        if (app->window) {
            int width = ANativeWindow_getWidth(app->window);
            int height = ANativeWindow_getHeight(app->window);
            if (width > 0 && height > 0) {
                screen_width_px = width;
                screen_height_px = height;
                apply_screen_size();
            }
        }
        break;
    case APP_CMD_TERM_WINDOW:
        renderer_ready = 0;
        renderer_retry_at_ns = 0;
        p2d_input_reset();
        ds_graphics_window_lost();
        break;
    case APP_CMD_GAINED_FOCUS:
        app_focused = 1;
        previous_frame_ns = 0;
        break;
    case APP_CMD_LOST_FOCUS:
        app_focused = 0;
        previous_frame_ns = 0;
        p2d_input_reset();
        break;
    case APP_CMD_WINDOW_REDRAW_NEEDED:
    default:
        break;
    }
}

static int translate_key(int android_key) {
    if (android_key >= AKEYCODE_A && android_key <= AKEYCODE_Z)
        return P2D_KEY_A + android_key - AKEYCODE_A;
    if (android_key >= AKEYCODE_0 && android_key <= AKEYCODE_9)
        return P2D_KEY_0 + android_key - AKEYCODE_0;
    switch (android_key) {
    case AKEYCODE_DPAD_LEFT: return P2D_KEY_LEFT;
    case AKEYCODE_DPAD_RIGHT: return P2D_KEY_RIGHT;
    case AKEYCODE_DPAD_UP: return P2D_KEY_UP;
    case AKEYCODE_DPAD_DOWN: return P2D_KEY_DOWN;
    case AKEYCODE_SPACE: return P2D_KEY_SPACE;
    case AKEYCODE_ENTER: return P2D_KEY_ENTER;
    case AKEYCODE_BACK: return P2D_KEY_BACK;
    case AKEYCODE_ESCAPE: return P2D_KEY_ESCAPE;
    default: return 0;
    }
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    (void)app;
    if (!event || !app_active) return 0;
    int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        size_t count = AMotionEvent_getPointerCount(event);
        if (count == 0) return 0;
        int raw_action = AMotionEvent_getAction(event);
        int action = raw_action & AMOTION_EVENT_ACTION_MASK;
        size_t index = (size_t)((raw_action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
        if (action == AMOTION_EVENT_ACTION_POINTER_DOWN) action = AMOTION_EVENT_ACTION_DOWN;
        else if (action == AMOTION_EVENT_ACTION_POINTER_UP) action = AMOTION_EVENT_ACTION_UP;
        if (action == AMOTION_EVENT_ACTION_MOVE) index = 0;
        if (index >= count) index = 0;
        if (action != AMOTION_EVENT_ACTION_DOWN && action != AMOTION_EVENT_ACTION_UP
                && action != AMOTION_EVENT_ACTION_MOVE && action != AMOTION_EVENT_ACTION_CANCEL)
            return 0;
        TouchCall call;
        call.x = AMotionEvent_getX(event, index);
        call.y = AMotionEvent_getY(event, index);
        call.down = action != AMOTION_EVENT_ACTION_UP && action != AMOTION_EVENT_ACTION_CANCEL;
        if (screen_w > 0) {
            if (call.x < 0.0f) call.x = 0.0f;
            if (call.x > (float)(screen_w - 1)) call.x = (float)(screen_w - 1);
        }
        if (screen_h > 0) {
            if (call.y < 0.0f) call.y = 0.0f;
            if (call.y > (float)(screen_h - 1)) call.y = (float)(screen_h - 1);
        }
        if (!ds_call_protected(protected_script_touch, &call, "touch"))
            mark_script_failed("touch");
        return 1;
    }
    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t action = AKeyEvent_getAction(event);
        int android_key = AKeyEvent_getKeyCode(event);
        int translated = translate_key(android_key);
        if (!translated) return 0;
        if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP) return 1;
        int key_and_down[2] = {translated, action == AKEY_EVENT_ACTION_DOWN};
        if (!ds_call_protected(protected_script_key, key_and_down, "key"))
            mark_script_failed("key");
        return android_key == AKEYCODE_BACK ? 0 : 1;
    }
    return 0;
}

void android_main(struct android_app *app) {
    Buffer frame = {0};
    if (!app) return;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;
    ds_log("Pona native 2D engine (C + Vulkan)");

    for (;;) {
        struct android_poll_source *source = NULL;
        int ident;
        int timeout = renderer_ready && app_active && app_focused ? 0 : 100;
        while ((ident = ALooper_pollOnce(timeout, NULL, NULL, (void **)&source)) >= 0) {
            (void)ident;
            if (source && source->process) source->process(app, source);
            if (app->destroyRequested) goto shutdown;
            timeout = 0;
        }
        if (app->destroyRequested) break;
        if (app->window && !renderer_ready && renderer_retry_at_ns
                && monotonic_ns() >= renderer_retry_at_ns) {
            renderer_retry_at_ns = 0;
            start_window(app);
        }
        if (!renderer_ready || !app->window || !app_focused) continue;
        restart_script_if_due();

        uint64_t now = monotonic_ns();
        if (previous_loop_ns && now >= previous_loop_ns) {
            measured_frame_seconds = (double)(now - previous_loop_ns) / 1000000000.0;
            ds_graphics_report_frame_interval(measured_frame_seconds);
        }
        previous_loop_ns = now;
        apply_screen_size();

        if (app_active) {
            dt = previous_frame_ns && now >= previous_frame_ns
                ? (double)(now - previous_frame_ns) / 1000000000.0 : 0.0;
            if (dt < 0.0) dt = 0.0;
            if (dt > 0.1) dt = 0.1;
            previous_frame_ns = now;
            if (!ds_call_protected(protected_script_update, NULL, "update"))
                mark_script_failed("update");
        }

        frame.pixels = NULL;
        frame.width = screen_w;
        frame.height = screen_h;
        frame.stride = screen_w;
        int frame_open = frame.width > 0 && frame.height > 0 && ds_graphics_begin_frame(&frame);
        if (frame_open) {
            if (app_active) {
                if (!ds_call_protected(protected_script_draw, &frame, "draw")) {
                    mark_script_failed("draw");
                    ds_graphics_error_screen(ds_runtime_error_message());
                }
            } else if (ds_runtime_has_error()) {
                ds_graphics_error_screen(ds_runtime_error_message());
            }
            ds_graphics_end_frame();
        }
        if (app_active) pond_app_end_frame();
    }

shutdown:
    if (app_active) (void)ds_call_protected(protected_script_stop, NULL, "stop");
    app_active = 0;
    renderer_ready = 0;
    ds_graphics_shutdown();
}
