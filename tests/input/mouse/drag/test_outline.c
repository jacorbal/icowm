/**
 * @file tests/input/mouse/drag/test_outline.c
 *
 * @brief Test battery for the outline-drag adapter functions
 *
 * drag_outline_start, drag_outline_move and drag_outline_end
 * (input/mouse/drag/outline.c) are thin adapters over render/
 * outline.c's shared strip-window mechanism: each one just forwards
 * s_drag-derived parameters (root window, active border color, the
 * outline_windows array) to render_outline_show/move/hide.  Storage
 * for s_drag lives in drag.c, which this file never links, so it is
 * defined once here instead, the same way drag.c itself would define
 * it.  render_outline_show/move/hide are recording stand-ins, since
 * what is under test here is which arguments outline.c passes
 * through, not render/outline.c's own X11 strip-window drawing.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <config.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/outline.h>


/** Singleton drag state; storage normally lives in drag.c, which this
 *  file never links, so it is defined here instead */
drag_state_td s_drag;

/** Recorded arguments from the last render_outline_show call */
static xcb_connection_t *s_show_connection;
static xcb_window_t s_show_root;
static struct geometry_s s_show_geom;
static uint32_t s_show_border_width;
static uint32_t s_show_color;
static xcb_window_t s_show_stack_below;
static xcb_window_t *s_show_windows;
static int s_show_calls;

/** Recorded arguments from the last render_outline_move call */
static xcb_connection_t *s_move_connection;
static struct geometry_s s_move_geom;
static uint32_t s_move_border_width;
static xcb_window_t s_move_stack_below;
static xcb_window_t *s_move_windows;
static int s_move_calls;

/** Recorded arguments from the last render_outline_hide call */
static xcb_connection_t *s_hide_connection;
static xcb_window_t *s_hide_windows;
static int s_hide_calls;


/**
 * @brief Recording stand-in for @a render_outline_show
 * @note Complexity: @e O(1)
 */
void render_outline_show(xcb_connection_t *connection, xcb_window_t root,
        struct geometry_s geom, uint32_t border_width, uint32_t color,
        xcb_window_t stack_below, xcb_window_t windows[4])
{
    s_show_connection = connection;
    s_show_root = root;
    s_show_geom = geom;
    s_show_border_width = border_width;
    s_show_color = color;
    s_show_stack_below = stack_below;
    s_show_windows = windows;
    s_show_calls++;
}


/**
 * @brief Recording stand-in for @a render_outline_move
 * @note Complexity: @e O(1)
 */
void render_outline_move(xcb_connection_t *connection,
        struct geometry_s geom, uint32_t border_width,
        xcb_window_t stack_below, xcb_window_t windows[4])
{
    s_move_connection = connection;
    s_move_geom = geom;
    s_move_border_width = border_width;
    s_move_stack_below = stack_below;
    s_move_windows = windows;
    s_move_calls++;
}


/**
 * @brief Recording stand-in for @a render_outline_hide
 * @note Complexity: @e O(1)
 */
void render_outline_hide(xcb_connection_t *connection,
        xcb_window_t windows[4])
{
    s_hide_connection = connection;
    s_hide_windows = windows;
    s_hide_calls++;
}


static void s_reset(void)
{
    static client_td dummy_client;
    static config_td dummy_config;

    memset(&s_drag, 0, sizeof(s_drag));
    memset(&dummy_client, 0, sizeof(dummy_client));
    memset(&dummy_config, 0, sizeof(dummy_config));
    s_drag.client = &dummy_client;
    s_drag.client->config = &dummy_config;
    s_drag.root = 42u;

    s_show_connection = NULL;
    s_show_calls = 0;
    s_move_connection = NULL;
    s_move_calls = 0;
    s_hide_connection = NULL;
    s_hide_calls = 0;
}


/* drag_outline_start forwards s_drag.root, the geometry given, the
 * fixed border width constant and s_drag.outline_windows through to
 * render_outline_show, using XCB_WINDOW_NONE as the stack_below */
static void s_test_start_forwards_root_and_geometry(void)
{
    xcb_connection_t *const fake_connection = (xcb_connection_t *) 1;
    struct geometry_s geom = { { 10, 20 }, { 300, 200 } };

    s_reset();

    drag_outline_start(fake_connection, geom);

    TAP_EQ_INT(s_show_calls, 1, "render_outline_show called once");
    TAP_OK(s_show_connection == fake_connection,
            "connection passed through unchanged");
    TAP_EQ_INT((int) s_show_root, (int) s_drag.root,
            "root window taken from s_drag.root");
    TAP_EQ_INT(s_show_geom.pos.x, 10, "geometry x passed through");
    TAP_EQ_INT(s_show_geom.pos.y, 20, "geometry y passed through");
    TAP_EQ_INT((int) s_show_geom.dim.w, 300, "geometry width passed"
            " through");
    TAP_EQ_INT((int) s_show_geom.dim.h, 200, "geometry height passed"
            " through");
    TAP_EQ_INT((int) s_show_stack_below, (int) XCB_WINDOW_NONE,
            "stack_below is XCB_WINDOW_NONE");
    TAP_OK(s_show_windows == s_drag.outline_windows,
            "outline_windows array passed by reference");
}


/* drag_outline_start reads the client's active border color from its
 * config's theme when both the client and its config are non-null */
static void s_test_start_uses_client_theme_color_when_available(void)
{
    struct geometry_s geom = { { 0, 0 }, { 1, 1 } };
    config_td local_config;

    s_reset();
    memset(&local_config, 0, sizeof(local_config));
    local_config.theme.window.active.border.color = 0xabcdefu;
    s_drag.client->config = &local_config;

    drag_outline_start((xcb_connection_t *) 1, geom);

    TAP_EQ_INT((int) s_show_color, (int) 0xabcdefu,
            "color taken from client->config->theme.window.active."
            "border.color");
}


/* drag_outline_start defaults the color to 0 when s_drag.client is
 * null */
static void s_test_start_defaults_color_when_client_null(void)
{
    struct geometry_s geom = { { 0, 0 }, { 1, 1 } };

    s_reset();
    s_drag.client = NULL;

    drag_outline_start((xcb_connection_t *) 1, geom);

    TAP_EQ_INT((int) s_show_color, 0,
            "color defaults to 0 when s_drag.client is null");
}


/* drag_outline_start defaults the color to 0 when the client's config
 * is null, even though the client itself is not */
static void s_test_start_defaults_color_when_config_null(void)
{
    struct geometry_s geom = { { 0, 0 }, { 1, 1 } };

    s_reset();
    s_drag.client->config = NULL;

    drag_outline_start((xcb_connection_t *) 1, geom);

    TAP_EQ_INT((int) s_show_color, 0,
            "color defaults to 0 when client->config is null");
}


/* drag_outline_move forwards the new geometry, the same fixed border
 * width, XCB_WINDOW_NONE and s_drag.outline_windows through to
 * render_outline_move */
static void s_test_move_forwards_geometry_and_windows(void)
{
    struct geometry_s geom = { { 5, 6 }, { 70, 80 } };

    s_reset();
    s_drag.outline_windows[0] = 111u;

    drag_outline_move((xcb_connection_t *) 2, geom);

    TAP_EQ_INT(s_move_calls, 1, "render_outline_move called once");
    TAP_OK(s_move_connection == (xcb_connection_t *) 2,
            "connection passed through unchanged");
    TAP_EQ_INT(s_move_geom.pos.x, 5, "geometry x passed through");
    TAP_EQ_INT(s_move_geom.pos.y, 6, "geometry y passed through");
    TAP_EQ_INT((int) s_move_geom.dim.w, 70, "geometry width passed"
            " through");
    TAP_EQ_INT((int) s_move_geom.dim.h, 80, "geometry height passed"
            " through");
    TAP_EQ_INT((int) s_move_stack_below, (int) XCB_WINDOW_NONE,
            "stack_below is XCB_WINDOW_NONE");
    TAP_OK(s_move_windows == s_drag.outline_windows,
            "outline_windows array passed by reference");
}


/* drag_outline_end forwards the connection and s_drag.outline_windows
 * through to render_outline_hide */
static void s_test_end_forwards_connection_and_windows(void)
{
    s_reset();

    drag_outline_end((xcb_connection_t *) 3);

    TAP_EQ_INT(s_hide_calls, 1, "render_outline_hide called once");
    TAP_OK(s_hide_connection == (xcb_connection_t *) 3,
            "connection passed through unchanged");
    TAP_OK(s_hide_windows == s_drag.outline_windows,
            "outline_windows array passed by reference");
}


int main(void)
{
    TAP_PLAN(23);

    s_test_start_forwards_root_and_geometry();
    s_test_start_uses_client_theme_color_when_available();
    s_test_start_defaults_color_when_client_null();
    s_test_start_defaults_color_when_config_null();
    s_test_move_forwards_geometry_and_windows();
    s_test_end_forwards_connection_and_windows();

    return TAP_DONE();
}
