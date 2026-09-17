/**
 * @file tests/menu/test_notify.c
 *
 * @brief Test battery for the generic notification popup helpers
 *
 * 'menu/notify.c' owns no static state of its own; every function
 * operates purely on the caller-supplied 'notify_popup_state_s',
 * which lets this file build several independent instances and check
 * each in isolation without any of the singleton bookkeeping
 * 'menu/notify/desktop.c' (covered separately in
 * 'tests/menu/notify/test_desktop.c') layers on top of it.  Every raw
 * XCB entry point this file's real dependency calls
 * ('xcb_generate_id', 'xcb_create_window', 'xcb_map_window') is a
 * link-only or recording stand-in, following the same pattern
 * 'tests/systray/test_layout.c' already established; 'xcb_window_
 * destroy' and 'atom_set_window_opacity' are this project's own thin
 * wrappers and are stubbed the same way.  'xcb_ewmh_connection_get'
 * answers NULL throughout, the same as the real one before EWMH setup
 * has run, so the '_NET_WM_WINDOW_TYPE' branch is a deliberately
 * skipped no-op here rather than requiring a real 'xcb_ewmh_
 * connection_t' to be built by hand.  'text_renderer_use_font',
 * 'text_renderer_set_color', and 'text_string_measure' are
 * controllable stand-ins standing in for a live font connection;
 * 'clock.c' is linked for real, so 'notify_popup_ms_remaining''s
 * actual countdown arithmetic against a real 'CLOCK_MONOTONIC'
 * timestamp is genuinely exercised, not just assumed.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdint.h>
#include <string.h>
#include <time.h>       /* clock_gettime, CLOCK_MONOTONIC */
#include <unistd.h>     /* usleep */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <stage.h>
#include <types/pair.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/notify.h>


/** Fake, non-null XCB connection handle */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

/** Next id 'xcb_generate_id' hands out; incremented every call so two
 *  popups created back to back get two distinct window ids, the same
 *  as the real server would */
static uint32_t s_next_generated_id;

static int s_call_xcb_create_window;
static int s_call_xcb_map_window;
static int s_call_xcb_window_destroy;
static int s_call_atom_set_window_opacity;
static xcb_window_t s_last_destroyed_window;
static uint32_t s_last_opacity_raw;

/** Width 'text_string_measure' answers; test-controlled */
static uint16_t s_measure_reply;


/**
 * @brief Link-only stand-in for @a xcb_ewmh_connection_get
 *
 * Answering NULL throughout means every '_NET_WM_WINDOW_TYPE' branch
 * in 'notify_popup_show_centered' is skipped, the same as before EWMH
 * setup has run in the real program; building a real
 * 'xcb_ewmh_connection_t' by hand would test this stand-in's own
 * bookkeeping, not 'menu/notify.c' itself.
 *
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


/**
 * @brief Recording stand-in for @a xcb_generate_id
 * @note Complexity: @e O(1)
 */
uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    s_next_generated_id++;
    return s_next_generated_id;
}


/**
 * @brief Recording stand-in for @a xcb_create_window
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_create_window(xcb_connection_t *connection,
        uint8_t depth, xcb_window_t wid, xcb_window_t parent,
        int16_t x, int16_t y, uint16_t width, uint16_t height,
        uint16_t border_width, uint16_t class,
        xcb_visualid_t visual, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) depth;
    (void) wid;
    (void) parent;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    (void) border_width;
    (void) class;
    (void) visual;
    (void) value_mask;
    (void) value_list;
    s_call_xcb_create_window++;
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_map_window
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_map_window(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) window;
    s_call_xcb_map_window++;
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_window_destroy
 * @note Complexity: @e O(1)
 */
void xcb_window_destroy(xcb_window_t window)
{
    s_call_xcb_window_destroy++;
    s_last_destroyed_window = window;
}


/**
 * @brief Recording stand-in for @a atom_set_window_opacity
 * @note Complexity: @e O(1)
 */
void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw_opacity)
{
    (void) connection;
    (void) window;
    s_call_atom_set_window_opacity++;
    s_last_opacity_raw = raw_opacity;
}


/**
 * @brief Link-only stand-in for @a config_theme_opacity_to_raw
 *
 * The real conversion arithmetic (percent to the 32-bit
 * '_NET_WM_WINDOW_OPACITY' range) belongs to 'config.c', already
 * covered on its own elsewhere; what this file checks is only that
 * 'notify_popup_show_centered' calls through to it exactly once per
 * popup, via 's_call_atom_set_window_opacity'.
 *
 * @note Complexity: @e O(1)
 */
uint32_t config_theme_opacity_to_raw(uint8_t percent)
{
    return (uint32_t) percent;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_set_wm_window_type
 *
 * Never actually reached: 'xcb_ewmh_connection_get' answers NULL
 * throughout this file, so 'notify_popup_show_centered''s
 * '_NET_WM_WINDOW_TYPE' branch is always skipped before this could be
 * called; only needed to satisfy the linker.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_wm_window_type(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t list_len, xcb_atom_t *list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) ewmh;
    (void) window;
    (void) list_len;
    (void) list;
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_create_gc
 *
 * Reached only through 'menu_draw_row_bg' (menu/draw.c, linked for
 * real here since 'notify_popup_repaint_centered' calls it), which
 * this file's own tests never exercise past its null guards.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_create_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc, xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) gc;
    (void) drawable;
    (void) value_mask;
    (void) value_list;
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_poly_fill_rectangle
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc, uint32_t rects_len,
        const xcb_rectangle_t *rects)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) drawable;
    (void) gc;
    (void) rects_len;
    (void) rects;
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_free_gc
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) gc;
    return cookie;
}


/**
 * @brief Link-only stand-in for @a text_draw_string
 *
 * Reached only through 'notify_popup_repaint_centered', which this
 * file's own tests never exercise past its null guards.
 *
 * @note Complexity: @e O(1)
 */
void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) drawable;
    (void) gc;
    (void) pos;
    (void) text;
}


/**
 * @brief Link-only stand-in for @a text_renderer_use_font
 * @note Complexity: @e O(1)
 */
int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;
    return 0;
}


/**
 * @brief Link-only stand-in for @a text_renderer_set_color
 * @note Complexity: @e O(1)
 */
void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
}


/**
 * @brief Test-controlled stand-in for @a text_string_measure
 * @note Complexity: @e O(1)
 */
uint16_t text_string_measure(const char *text)
{
    (void) text;
    return s_measure_reply;
}


static void s_reset(void)
{
    s_next_generated_id = 1000u;
    s_call_xcb_create_window = 0;
    s_call_xcb_map_window = 0;
    s_call_xcb_window_destroy = 0;
    s_call_atom_set_window_opacity = 0;
    s_last_destroyed_window = XCB_WINDOW_NONE;
    s_last_opacity_raw = 0u;
    s_measure_reply = 20u;
}


static config_td s_make_config(void)
{
    config_td cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.theme.overlay.color.background = 0x101010u;
    cfg.theme.overlay.color.foreground = 0xf0f0f0u;
    cfg.theme.overlay.border.color = 0x202020u;
    cfg.theme.overlay.border.width = 1u;
    cfg.theme.overlay.opacity = 90u;
    return cfg;
}


static stage_td s_make_stage(uint32_t w, uint32_t h)
{
    stage_td stage;
    static xcb_screen_t screen;

    memset(&stage, 0, sizeof(stage));
    memset(&screen, 0, sizeof(screen));
    stage.screen = &screen;
    stage.properties.dim.w = w;
    stage.properties.dim.h = h;
    return stage;
}


/* notify_popup_close is a no-op on a null connection, a null state, or
 * a state whose window is already XCB_WINDOW_NONE */
static void s_test_close_null_guards(void)
{
    struct notify_popup_state_s state;

    s_reset();
    memset(&state, 0, sizeof(state));
    state.window = 999u;

    notify_popup_close(NULL, &state);
    TAP_EQ_INT(s_call_xcb_window_destroy, 0,
            "a null connection skips destroying the window entirely");

    notify_popup_close(s_fake_connection, NULL);
    TAP_EQ_INT(s_call_xcb_window_destroy, 0,
            "a null state also skips destroying anything");

    state.window = XCB_WINDOW_NONE;
    notify_popup_close(s_fake_connection, &state);
    TAP_EQ_INT(s_call_xcb_window_destroy, 0,
            "a state with no window open is left alone");
}


/* A real close destroys the window and resets every field of state */
static void s_test_close_resets_state(void)
{
    struct notify_popup_state_s state;

    s_reset();
    memset(&state, 0, sizeof(state));
    state.window = 555u;
    state.open_time.tv_sec = 123;
    state.open_time.tv_nsec = 456;

    notify_popup_close(s_fake_connection, &state);

    TAP_EQ_INT(s_call_xcb_window_destroy, 1,
            "exactly one window is destroyed");
    TAP_EQ_INT((long) s_last_destroyed_window, (long) 555u,
            "the destroyed window is the one that was open");
    TAP_EQ_INT((long) state.window, (long) XCB_WINDOW_NONE,
            "state.window is reset to XCB_WINDOW_NONE");
    TAP_EQ_INT((long) state.open_time.tv_sec, 0,
            "state.open_time.tv_sec is reset to 0");
    TAP_EQ_INT((long) state.open_time.tv_nsec, 0,
            "state.open_time.tv_nsec is reset to 0");
}


/* notify_popup_is_open / notify_popup_window reflect a null state and
 * an open/closed one correctly */
static void s_test_is_open_and_window_accessors(void)
{
    struct notify_popup_state_s state;

    memset(&state, 0, sizeof(state));

    TAP_OK(!notify_popup_is_open(NULL),
            "a null state is never reported open");
    TAP_EQ_INT((long) notify_popup_window(NULL), (long) XCB_WINDOW_NONE,
            "a null state's window is XCB_WINDOW_NONE");

    state.window = XCB_WINDOW_NONE;
    TAP_OK(!notify_popup_is_open(&state),
            "a state with no window is not open");

    state.window = 42u;
    TAP_OK(notify_popup_is_open(&state),
            "a state with a real window is open");
    TAP_EQ_INT((long) notify_popup_window(&state), (long) 42u,
            "notify_popup_window returns the real window id");
}


/* notify_popup_ms_remaining answers -1 for every "nothing to count
 * down" case: null state, no window, or a never-recorded open time */
static void s_test_ms_remaining_minus_one_cases(void)
{
    struct notify_popup_state_s state;

    memset(&state, 0, sizeof(state));

    TAP_EQ_INT(notify_popup_ms_remaining(NULL, 1000), -1,
            "a null state answers -1");

    state.window = XCB_WINDOW_NONE;
    TAP_EQ_INT(notify_popup_ms_remaining(&state, 1000), -1,
            "no window open answers -1");

    state.window = 7u;
    state.open_time.tv_sec = 0;
    state.open_time.tv_nsec = 0;
    TAP_EQ_INT(notify_popup_ms_remaining(&state, 1000), -1,
            "an all-zero open_time (never recorded) answers -1");
}


/* notify_popup_ms_remaining counts down correctly against a real
 * CLOCK_MONOTONIC timestamp recorded a moment ago, and clamps to 0
 * once the timeout has passed */
static void s_test_ms_remaining_counts_down(void)
{
    struct notify_popup_state_s state;
    int remaining;

    memset(&state, 0, sizeof(state));
    state.window = 7u;
    (void) clock_gettime(CLOCK_MONOTONIC, &state.open_time);

    remaining = notify_popup_ms_remaining(&state, 1000);
    TAP_OK(remaining > 0 && remaining <= 1000,
            "just after opening, remaining time is positive and at"
            " most the full timeout");

    /* Force the recorded open time far enough into the past that
     * the timeout has certainly elapsed, rather than sleeping in a
     * test */
    state.open_time.tv_sec -= 5;
    remaining = notify_popup_ms_remaining(&state, 1000);
    TAP_EQ_INT(remaining, 0,
            "once elapsed time exceeds timeout_ms, remaining clamps"
            " to exactly 0, never negative");
}


/* notify_popup_show_centered is a no-op on any null argument, or when
 * stage->screen is null, and creates no window in that case */
static void s_test_show_null_guards(void)
{
    struct notify_popup_state_s state;
    stage_td stage;
    config_td cfg;

    s_reset();
    memset(&state, 0, sizeof(state));
    stage = s_make_stage(800u, 600u);
    cfg = s_make_config();

    notify_popup_show_centered(NULL, &stage, &state, "hi", &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null connection creates no window");

    notify_popup_show_centered(s_fake_connection, NULL, &state, "hi",
            &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null stage creates no window");

    notify_popup_show_centered(s_fake_connection, &stage, NULL, "hi",
            &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null state creates no window");

    notify_popup_show_centered(s_fake_connection, &stage, &state,
            NULL, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "null text creates no window");

    notify_popup_show_centered(s_fake_connection, &stage, &state,
            "hi", NULL);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null config creates no window");

    stage.screen = NULL;
    notify_popup_show_centered(s_fake_connection, &stage, &state,
            "hi", &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a stage with no screen creates no window");
}


/* A normal show call caches the text, creates and maps exactly one
 * window, applies the theme opacity, and records the open time */
static void s_test_show_creates_and_maps_window(void)
{
    struct notify_popup_state_s state;
    stage_td stage;
    config_td cfg;

    s_reset();
    memset(&state, 0, sizeof(state));
    stage = s_make_stage(800u, 600u);
    cfg = s_make_config();
    s_measure_reply = 50u;

    notify_popup_show_centered(s_fake_connection, &stage, &state,
            "Desktop 2", &cfg);

    TAP_EQ_STR(state.text, "Desktop 2",
            "the popup's text is cached in state for later repaint");
    TAP_EQ_INT(s_call_xcb_create_window, 1,
            "exactly one window is created");
    TAP_EQ_INT(s_call_xcb_map_window, 1,
            "exactly one window is mapped");
    TAP_OK(state.window != XCB_WINDOW_NONE,
            "state.window now holds a real window id");
    TAP_EQ_INT(s_call_atom_set_window_opacity, 1,
            "the theme's overlay opacity is published exactly once");
    TAP_OK(state.open_time.tv_sec != 0 || state.open_time.tv_nsec != 0,
            "the open time is recorded (nonzero CLOCK_MONOTONIC"
            " timestamp)");
}


/* Calling show again while a popup is already open closes the old one
 * first, so only one window is ever left mapped */
static void s_test_show_closes_previous_popup_first(void)
{
    struct notify_popup_state_s state;
    stage_td stage;
    config_td cfg;
    xcb_window_t first_window;

    s_reset();
    memset(&state, 0, sizeof(state));
    stage = s_make_stage(800u, 600u);
    cfg = s_make_config();

    notify_popup_show_centered(s_fake_connection, &stage, &state,
            "first", &cfg);
    first_window = state.window;

    notify_popup_show_centered(s_fake_connection, &stage, &state,
            "second", &cfg);

    TAP_EQ_INT(s_call_xcb_window_destroy, 1,
            "the first popup's window is destroyed exactly once"
            " before the second is created");
    TAP_EQ_INT((long) s_last_destroyed_window, (long) first_window,
            "the window destroyed is the first popup's own window,"
            " not the new one");
    TAP_EQ_STR(state.text, "second",
            "the cached text now reflects the second call");
    TAP_EQ_INT(s_call_xcb_create_window, 2,
            "two windows were created in total, one per call");
}


/* A very short text still yields at least the floor width (72px in
 * the real source), and a wide one is sized to fit it plus padding;
 * neither dimension is asserted directly here since it depends on
 * private layout constants, so this only checks the popup opens
 * successfully in both cases */
static void s_test_show_handles_short_and_long_text(void)
{
    struct notify_popup_state_s state;
    stage_td stage;
    config_td cfg;

    s_reset();
    memset(&state, 0, sizeof(state));
    stage = s_make_stage(800u, 600u);
    cfg = s_make_config();

    s_measure_reply = 5u;
    notify_popup_show_centered(s_fake_connection, &stage, &state,
            "a", &cfg);
    TAP_OK(state.window != XCB_WINDOW_NONE,
            "a very short label still produces an open popup");

    s_measure_reply = 500u;
    notify_popup_show_centered(s_fake_connection, &stage, &state,
            "a very long desktop name indeed", &cfg);
    TAP_OK(state.window != XCB_WINDOW_NONE,
            "a very wide label still produces an open popup");
}


/* notify_popup_repaint_centered is a no-op on any null argument or a
 * state with no window open, never touching the text renderer */
static void s_test_repaint_null_guards(void)
{
    struct notify_popup_state_s state;
    config_td cfg;

    s_reset();
    memset(&state, 0, sizeof(state));
    cfg = s_make_config();

    notify_popup_repaint_centered(NULL, &state, &cfg);
    notify_popup_repaint_centered(s_fake_connection, NULL, &cfg);
    notify_popup_repaint_centered(s_fake_connection, &state, NULL);

    state.window = XCB_WINDOW_NONE;
    notify_popup_repaint_centered(s_fake_connection, &state, &cfg);

    /* None of the calls above touch xcb_poly_fill_rectangle, but that
     * symbol is not linked into this file at all; the true guard this
     * checks for is that none of them crash under
     * AddressSanitizer/UndefinedBehaviorSanitizer, so a bare TAP_OK
     * confirms this point was reached */
    TAP_OK(1, "repainting with any null argument, or a closed popup,"
            " never crashes");
}


int main(void)
{
    TAP_PLAN(37);

    s_test_close_null_guards();
    s_test_close_resets_state();
    s_test_is_open_and_window_accessors();
    s_test_ms_remaining_minus_one_cases();
    s_test_ms_remaining_counts_down();
    s_test_show_null_guards();
    s_test_show_creates_and_maps_window();
    s_test_show_closes_previous_popup_first();
    s_test_show_handles_short_and_long_text();
    s_test_repaint_null_guards();

    return TAP_DONE();
}
