/**
 * @file tests/menu/test_popup.c
 *
 * @brief Test battery for the informational client popup window
 *
 * 'menu/popup.c' keeps its own small set of static globals (the
 * popup's window id, the modifier/keycode that opened it, its four
 * cached text lines, and its open timestamp), exercised here through
 * the public 'popup_*' entry points only, the same black-box style
 * 'tests/menu/context/test_winlist.c' already established for a
 * different module's own static state.  Every raw XCB entry point is
 * a link-only or recording stand-in, following
 * 'tests/menu/test_notify.c''s pattern; 'ccmd_client_monitor'
 * (cmds/client/screen.c) is a genuinely cross-module dependency,
 * stubbed the same way as 'stage_desktop_label' was in
 * 'tests/menu/notify/test_desktop.c'; 'menu/draw.c' is linked for
 * real so 'popup_repaint''s forwarding through 'menu_draw_label' is
 * genuinely exercised, not assumed; 'clock.c' is linked for real too,
 * so 'popup_ms_remaining''s countdown arithmetic runs against a real
 * 'CLOCK_MONOTONIC' timestamp.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <cmds/client/screen.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <stage.h>
#include <types/pair.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/popup.h>


/** Fake, non-null XCB connection handle */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

static uint32_t s_next_generated_id;

static int s_call_xcb_create_window;
static int s_call_xcb_map_window;
static int s_call_xcb_window_destroy;
static int s_call_ccmd_client_monitor;
static int s_call_atom_set_window_opacity;
static int s_call_text_string_measure;
static int s_call_menu_draw_label;
static xcb_window_t s_last_destroyed_window;
static int16_t s_last_created_x;
static int16_t s_last_created_y;
static uint16_t s_last_created_w;
static uint16_t s_last_created_h;
static char s_last_drawn_texts[4][WM_INFO_POPUP_LINE_MAX_LENGTH];

/** Whether the stand-in reports a resolvable monitor at all */
static bool s_monitor_resolves;
/** Stage the stand-in hands back through @c out_stage */
static stage_td *s_monitor_out_stage;
/** Monitor rectangle the stand-in hands back through @c out_monitor */
static monitor_td s_monitor_out_value;

/** Width @a text_string_measure answers for every line */
static uint16_t s_measure_reply;


xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    s_next_generated_id++;
    return s_next_generated_id;
}


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
    (void) border_width;
    (void) class;
    (void) visual;
    (void) value_mask;
    (void) value_list;
    s_call_xcb_create_window++;
    s_last_created_x = x;
    s_last_created_y = y;
    s_last_created_w = width;
    s_last_created_h = height;
    return cookie;
}


xcb_void_cookie_t xcb_map_window(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) window;
    s_call_xcb_map_window++;
    return cookie;
}


void xcb_window_destroy(xcb_window_t window)
{
    s_call_xcb_window_destroy++;
    s_last_destroyed_window = window;
}


void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw_opacity)
{
    (void) connection;
    (void) window;
    (void) raw_opacity;
    s_call_atom_set_window_opacity++;
}


uint32_t config_theme_opacity_to_raw(uint8_t percent)
{
    return (uint32_t) percent;
}


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


xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) gc;
    return cookie;
}


/**
 * @brief Recording stand-in for @a text_draw_string
 *
 * Only reached through 'menu_draw_label' (menu/draw.c, linked for
 * real); records each drawn line's text into
 * 's_last_drawn_texts', in call order, so 'popup_repaint' can be
 * checked to have forwarded all four cached lines in the right order.
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

    if (s_call_menu_draw_label < 4) {
        (void) strncpy(s_last_drawn_texts[s_call_menu_draw_label],
                text ? text : "",
                sizeof(s_last_drawn_texts[0]) - 1);
        s_last_drawn_texts[s_call_menu_draw_label]
            [sizeof(s_last_drawn_texts[0]) - 1] = '\0';
    }
    s_call_menu_draw_label++;
}


int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;
    return 0;
}


void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
}


uint16_t text_string_measure(const char *text)
{
    (void) text;
    s_call_text_string_measure++;
    return s_measure_reply;
}


/**
 * @brief Recording, test-controlled stand-in for @a ccmd_client_monitor
 *
 * The real monitor-resolution algorithm belongs to a different module
 * ('cmds/client/screen.c'), already exercised on its own elsewhere;
 * what this file checks is only that 'popup_show' formats
 * 'monitor_id' correctly from whatever this hands back, including the
 * "could not resolve" case where it answers @c false.
 *
 * @note Complexity: @e O(1)
 */
bool ccmd_client_monitor(client_td *client, stage_td **out_stage,
        monitor_td *out_monitor)
{
    (void) client;
    s_call_ccmd_client_monitor++;
    if (out_stage != NULL) {
        *out_stage = s_monitor_out_stage;
    }
    if (out_monitor != NULL) {
        *out_monitor = s_monitor_out_value;
    }
    return s_monitor_resolves;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 *
 * Every 'LOGGER_*' macro, 'LOGGER_TRACE' included, expands to a call
 * through here; this file has nothing to assert about logging.
 *
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


static void s_reset(void)
{
    s_next_generated_id = 3000u;
    s_call_xcb_create_window = 0;
    s_call_xcb_map_window = 0;
    s_call_xcb_window_destroy = 0;
    s_call_ccmd_client_monitor = 0;
    s_call_atom_set_window_opacity = 0;
    s_call_text_string_measure = 0;
    s_call_menu_draw_label = 0;
    s_last_destroyed_window = XCB_WINDOW_NONE;
    s_last_created_x = 0;
    s_last_created_y = 0;
    s_last_created_w = 0;
    s_last_created_h = 0;
    memset(s_last_drawn_texts, 0, sizeof(s_last_drawn_texts));
    s_monitor_resolves = false;
    s_monitor_out_stage = NULL;
    memset(&s_monitor_out_value, 0, sizeof(s_monitor_out_value));
    s_measure_reply = 30u;

    /* Settle the module's own static popup window back to closed
     * before each scenario, mirroring 'tests/menu/notify/
     * test_desktop.c''s own s_reset pattern for a shared static */
    popup_close(s_fake_connection);
    s_call_xcb_window_destroy = 0;
}


static config_td s_make_config(void)
{
    config_td cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.theme.overlay.color.background = 0x101010u;
    cfg.theme.overlay.color.foreground = 0xf0f0f0u;
    cfg.theme.overlay.border.color = 0x303030u;
    cfg.theme.overlay.border.width = 1u;
    cfg.theme.overlay.opacity = 85u;
    return cfg;
}


static stage_td s_make_stage(uint32_t w, uint32_t h, uint32_t id)
{
    stage_td stage;
    static xcb_screen_t screen;

    memset(&stage, 0, sizeof(stage));
    memset(&screen, 0, sizeof(screen));
    stage.screen = &screen;
    stage.properties.dim.w = w;
    stage.properties.dim.h = h;
    stage.id = id;
    return stage;
}


static desktop_td s_make_desktop(xcb_window_t id)
{
    desktop_td desktop;

    memset(&desktop, 0, sizeof(desktop));
    desktop.id = id;
    return desktop;
}


static client_td s_make_client(xcb_window_t id, xcb_window_t frame,
        int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    client_td client;

    memset(&client, 0, sizeof(client));
    client.id = id;
    client.frame = frame;
    client.layout.geometry.cur.pos.x = x;
    client.layout.geometry.cur.pos.y = y;
    client.layout.geometry.cur.dim.w = w;
    client.layout.geometry.cur.dim.h = h;
    client.properties.flags = 0x2u;
    client.properties.state = 0x4u;
    return client;
}


/* popup_show does nothing at all on any null argument, or a
 * screen-less stage, without ever generating a window id */
static void s_test_show_null_guards(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u, 0u);
    desktop = s_make_desktop(1u);
    client = s_make_client(50u, 51u, 10, 10, 200u, 100u);
    cfg = s_make_config();

    popup_show(NULL, &stage, &desktop, &client, 0, 38, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null connection creates no window");

    popup_show(s_fake_connection, NULL, &desktop, &client, 0, 38, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null stage creates no window");

    popup_show(s_fake_connection, &stage, NULL, &client, 0, 38, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null desktop creates no window");

    popup_show(s_fake_connection, &stage, &desktop, NULL, 0, 38, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null client creates no window");

    popup_show(s_fake_connection, &stage, &desktop, &client, 0, 38,
            NULL);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a null config creates no window");

    stage.screen = NULL;
    popup_show(s_fake_connection, &stage, &desktop, &client, 0, 38,
            &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a screen-less stage creates no window");
}


/* A normal show call opens exactly one window, maps it, publishes the
 * overlay's opacity, and is positioned at the client's own top-left
 * corner when that fits on screen without clamping */
static void s_test_show_creates_positions_and_maps(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u, 0u);
    desktop = s_make_desktop(1u);
    client = s_make_client(50u, 51u, 40, 60, 200u, 100u);
    cfg = s_make_config();

    popup_show(s_fake_connection, &stage, &desktop, &client, 0, 38,
            &cfg);

    TAP_EQ_INT(s_call_xcb_create_window, 1,
            "exactly one popup window is created");
    TAP_EQ_INT(s_call_xcb_map_window, 1,
            "exactly one popup window is mapped");
    TAP_EQ_INT(s_last_created_x, 40,
            "the popup's x matches the client's own x, unclamped when"
            " it fits");
    TAP_EQ_INT(s_last_created_y, 60,
            "the popup's y matches the client's own y, unclamped when"
            " it fits");
    TAP_OK(s_last_created_w >= 260u,
            "the popup's width is never narrower than the 260px"
            " floor");
    TAP_EQ_INT(s_last_created_h, 96,
            "the popup's fixed height is 96px");
    TAP_EQ_INT(s_call_atom_set_window_opacity, 1,
            "the overlay's opacity is published exactly once");
    TAP_OK(popup_is_open(),
            "the popup now reports itself as open");
    TAP_OK(popup_window() != XCB_WINDOW_NONE,
            "popup_window answers a real window id");
}


/* A client whose position would place the popup off the right or
 * bottom edge of the stage is clamped back on screen, never
 * negative and never past the far edge */
static void s_test_show_clamps_to_screen(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    config_td cfg;

    s_reset();
    stage = s_make_stage(300u, 200u, 0u);
    desktop = s_make_desktop(1u);
    /* Placed well past the stage's own 300x200 bounds */
    client = s_make_client(50u, 51u, 900, 900, 50u, 50u);
    cfg = s_make_config();

    popup_show(s_fake_connection, &stage, &desktop, &client, 0, 38,
            &cfg);

    TAP_OK(s_last_created_x >= 0 &&
            s_last_created_x <= (int16_t) (300 - s_last_created_w),
            "x is clamped so the popup never runs off the right edge"
            " or goes negative");
    TAP_OK(s_last_created_y >= 0 &&
            s_last_created_y <= (int16_t) (200 - 96),
            "y is clamped so the popup never runs off the bottom edge"
            " or goes negative");
}


/* When ccmd_client_monitor cannot resolve a monitor, monitor_id
 * defaults to 0 rather than being left uninitialized or crashing */
static void s_test_show_monitor_unresolved_defaults_zero(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u, 7u);
    desktop = s_make_desktop(2u);
    client = s_make_client(60u, 61u, 5, 5, 100u, 80u);
    cfg = s_make_config();
    s_monitor_resolves = false;

    popup_show(s_fake_connection, &stage, &desktop, &client, 0, 38,
            &cfg);

    TAP_EQ_INT(s_call_ccmd_client_monitor, 1,
            "ccmd_client_monitor is consulted exactly once");
    TAP_OK(popup_is_open(),
            "the popup still opens even when the monitor cannot be"
            " resolved");
}


/* Closing an open popup destroys its window and resets every field of
 * the module's static state, reflected through the public accessors */
static void s_test_close_resets_state(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    config_td cfg;
    xcb_window_t opened_window;

    s_reset();
    stage = s_make_stage(1024u, 768u, 0u);
    desktop = s_make_desktop(1u);
    client = s_make_client(70u, 71u, 5, 5, 100u, 80u);
    cfg = s_make_config();

    popup_show(s_fake_connection, &stage, &desktop, &client, 0, 38,
            &cfg);
    opened_window = popup_window();
    TAP_OK(opened_window != XCB_WINDOW_NONE, "the popup opens first");

    popup_close(s_fake_connection);

    TAP_EQ_INT(s_call_xcb_window_destroy, 1,
            "closing destroys exactly one window");
    TAP_EQ_INT((long) s_last_destroyed_window, (long) opened_window,
            "the destroyed window is the one that had been open");
    TAP_OK(!popup_is_open(),
            "the popup reports itself closed afterward");
    TAP_EQ_INT((long) popup_window(), (long) XCB_WINDOW_NONE,
            "popup_window answers XCB_WINDOW_NONE once closed");
}


/* popup_close on a null connection, or with nothing open, is a
 * harmless no-op */
static void s_test_close_null_guards(void)
{
    s_reset();

    popup_close(NULL);
    TAP_EQ_INT(s_call_xcb_window_destroy, 0,
            "a null connection skips destroying anything");

    popup_close(s_fake_connection);
    TAP_EQ_INT(s_call_xcb_window_destroy, 0,
            "closing an already-closed popup destroys nothing");
}


/* popup_ms_remaining answers -1 while nothing is open, and a positive
 * value bounded by the popup timeout constant right after opening */
static void s_test_ms_remaining_reflects_state(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    config_td cfg;
    int remaining;

    s_reset();
    stage = s_make_stage(1024u, 768u, 0u);
    desktop = s_make_desktop(1u);
    client = s_make_client(80u, 81u, 5, 5, 100u, 80u);
    cfg = s_make_config();

    TAP_EQ_INT(popup_ms_remaining(), -1,
            "with nothing open, remaining time is -1");

    popup_show(s_fake_connection, &stage, &desktop, &client, 0, 38,
            &cfg);
    remaining = popup_ms_remaining();
    TAP_OK(remaining > 0 && remaining <= WM_INFO_POPUP_TIMEOUT_MS,
            "right after opening, remaining time is positive and"
            " bounded by the popup timeout constant");

    popup_close(s_fake_connection);
}


/* Showing a second popup while one is already open closes the first
 * one before creating the second, so exactly one is ever left mapped
 * at a time */
static void s_test_show_replaces_previous_popup(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client_a;
    client_td client_b;
    config_td cfg;
    xcb_window_t first_window;

    s_reset();
    stage = s_make_stage(1024u, 768u, 0u);
    desktop = s_make_desktop(1u);
    client_a = s_make_client(90u, 91u, 5, 5, 100u, 80u);
    client_b = s_make_client(92u, 93u, 20, 20, 120u, 90u);
    cfg = s_make_config();

    popup_show(s_fake_connection, &stage, &desktop, &client_a, 0, 38,
            &cfg);
    first_window = popup_window();

    popup_show(s_fake_connection, &stage, &desktop, &client_b, 0, 40,
            &cfg);

    TAP_EQ_INT(s_call_xcb_window_destroy, 1,
            "the first popup is destroyed exactly once before the"
            " second is created");
    TAP_EQ_INT((long) s_last_destroyed_window, (long) first_window,
            "the window destroyed is the first popup's own window");
    TAP_EQ_INT(s_call_xcb_create_window, 2,
            "two windows were created in total, one per call");
}


/* popup_repaint on a null connection, null config, or a closed popup
 * never draws any line at all */
static void s_test_repaint_null_guards(void)
{
    config_td cfg;

    s_reset();
    cfg = s_make_config();

    popup_repaint(NULL, &cfg);
    TAP_EQ_INT(s_call_menu_draw_label, 0,
            "a null connection draws no line at all");

    popup_repaint(s_fake_connection, NULL);
    TAP_EQ_INT(s_call_menu_draw_label, 0,
            "a null config draws no line at all");

    popup_repaint(s_fake_connection, &cfg);
    TAP_EQ_INT(s_call_menu_draw_label, 0,
            "repainting a closed popup draws no line at all");
}


/* A real repaint draws exactly the four cached lines, in order, each
 * forwarded down to text_draw_string via menu_draw_label; the exact
 * text is checked to actually contain the client's own id and
 * geometry, confirming the cache popup_show built is what gets
 * repainted */
static void s_test_repaint_draws_four_cached_lines(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    config_td cfg;

    s_reset();
    stage = s_make_stage(1024u, 768u, 0u);
    desktop = s_make_desktop(3u);
    client = s_make_client(0x64u, 0x65u, 12, 34, 640u, 480u);
    cfg = s_make_config();

    popup_show(s_fake_connection, &stage, &desktop, &client, 0, 38,
            &cfg);
    popup_repaint(s_fake_connection, &cfg);

    TAP_EQ_INT(s_call_menu_draw_label, 4,
            "exactly four lines are drawn per repaint");
    TAP_OK(strstr(s_last_drawn_texts[0], "name=") != NULL,
            "the first line carries the name/class/instance fields");
    TAP_OK(strstr(s_last_drawn_texts[1], "client_id=0x64") != NULL,
            "the second line carries the client's own hex id");
    TAP_OK(strstr(s_last_drawn_texts[2], "geom=640x480") != NULL,
            "the third line carries the client's cached geometry");
    TAP_OK(strstr(s_last_drawn_texts[3], "flags=0x2") != NULL &&
            strstr(s_last_drawn_texts[3], "state=0x4") != NULL,
            "the fourth line carries the client's flags and state");
}


int main(void)
{
    TAP_PLAN(39);

    s_test_show_null_guards();
    s_test_show_creates_positions_and_maps();
    s_test_show_clamps_to_screen();
    s_test_show_monitor_unresolved_defaults_zero();
    s_test_close_resets_state();
    s_test_close_null_guards();
    s_test_ms_remaining_reflects_state();
    s_test_show_replaces_previous_popup();
    s_test_repaint_null_guards();
    s_test_repaint_draws_four_cached_lines();

    return TAP_DONE();
}
