/**
 * @file tests/cmds/client/test_move.c
 *
 * @brief Test battery for client positioning commands
 *        (cmds/client/move.c)
 *
 * 'ccmd_client_apply_geometry', 'ccmd_client_move', 'ccmd_client_
 * center', 'ccmd_client_move_to_monitor', and the four direction-
 * specific 'ccmd_client_move_to_monitor_north'/'_south'/'_east'/
 * '_west' entry points are exercised linked against the real
 * source file, since every one of them lives there; the four
 * directional entry points are only reachable through the file-
 * static 's_move_to_monitor_toward', which is the only way to reach
 * it from outside this file.  'ccmd_client_resolve_workarea' is a
 * test-controlled stand-in answering a fixed rectangle a test
 * registers first (the same pattern 'test_maximize.c' already uses),
 * so 'ccmd_client_center' can be exercised without any real desktop/
 * monitor lookup running underneath it; 'ccmd_screen_dim' is a link-
 * only stand-in refusing outright, reached only on the fallback path
 * once 'ccmd_client_resolve_workarea' itself refuses.  'ccmd_client_
 * monitor' is a test-controlled stand-in answering a fixed surface
 * and monitor a test registers first, exercising the monitor-search
 * logic in 'ccmd_client_move_to_monitor' and 's_move_to_monitor_
 * toward' without a real surface list.  'surface_monitor_direction'
 * is a test-controlled stand-in too, answering whichever single
 * monitor a test registers as the neighbor in that direction.
 * 'ccmd_client_refill_maximized_geometry' is a test-controlled
 * stand-in as well (the real one lives in cmds/client/maximize.c,
 * not this file), false and untouched by default, so a test
 * exercising a maximized client's move-to-monitor refold can make
 * it write fixed sentinel values instead.
 * 'ccmd_target_win', 'xcb_connection_get', and 'wm_request_client_
 * redraw' are link-only stand-ins: side effects this file's
 * assertions do not need to observe directly, the geometry changes
 * already being visible on 'client_td' itself.  'xcb_connection_get'
 * answers a non-null, otherwise-unused pointer so 'ccmd_client_
 * apply_geometry' clears its own null-connection refusal check, and
 * 'xcb_configure_window' itself is a recording stand-in rather than
 * the real libxcb call, since no test here ever opens a live X
 * connection: the real function would need one to write its request
 * to.  'logger_msg' is a link-only stand-in too, swallowing the
 * warning an out-of-range monitor index logs.
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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <client.h>
#include <cmds/client/move.h>
#include <desktop.h>
#include <harness/tap.h>
#include <logger.h>
#include <surface.h>
#include <types/pair.h>


/** Fixed rectangle @a ccmd_client_resolve_workarea answers with,
 *  registered by @a s_set_workarea; 'is_ok' controls whether it
 *  succeeds or refuses outright */
static int32_t s_wa_x;
static int32_t s_wa_y;
static uint16_t s_wa_w;
static uint16_t s_wa_h;
static bool s_wa_ok;


/**
 * @brief Test-controlled stand-in for @a ccmd_client_resolve_workarea
 * @note Complexity: @e O(1)
 */
bool ccmd_client_resolve_workarea(client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y,
        uint16_t *restrict out_w, uint16_t *restrict out_h)
{
    (void) client;

    if (!s_wa_ok) {
        return false;
    }

    if (out_x != NULL) {
        *out_x = s_wa_x;
    }
    if (out_y != NULL) {
        *out_y = s_wa_y;
    }
    *out_w = s_wa_w;
    *out_h = s_wa_h;

    return true;
}


/**
 * @brief Configurable stand-in for
 *        @a ccmd_client_refill_maximized_geometry
 *
 * Defaults to @c false, untouched, matching every test here except
 * the one exercising a maximized client's move-to-monitor; that one
 * sets @c s_refill_should_touch and the four output values first.
 *
 * @note Complexity: @e O(1)
 */
static bool s_refill_should_touch;
static int32_t s_refill_x;
static int32_t s_refill_y;
static uint32_t s_refill_w;
static uint32_t s_refill_h;

bool ccmd_client_refill_maximized_geometry(client_td *client)
{
    if (!s_refill_should_touch) {
        return false;
    }
    client->layout.geometry.cur.pos.x = s_refill_x;
    client->layout.geometry.cur.pos.y = s_refill_y;
    client->layout.geometry.cur.dim.w = s_refill_w;
    client->layout.geometry.cur.dim.h = s_refill_h;
    return true;
}


static void s_set_workarea(int32_t x, int32_t y, uint16_t w, uint16_t h)
{
    s_wa_x = x;
    s_wa_y = y;
    s_wa_w = w;
    s_wa_h = h;
    s_wa_ok = true;
}


/**
 * @brief Link-only stand-in for @a ccmd_screen_dim
 *
 * Reached only on the fallback path once @a ccmd_client_resolve_
 * workarea itself refuses, which every 'ccmd_client_center' test
 * here avoids needing by registering a fixed workarea through
 * @a s_set_workarea first, except the one test that deliberately
 * checks the no-workarea-at-all case
 *
 * @note Complexity: @e O(1)
 */
bool ccmd_screen_dim(const client_td *client, uint16_t *restrict out_w,
        uint16_t *restrict out_h)
{
    (void) client;
    (void) out_w;
    (void) out_h;

    return false;
}


/**
 * @brief Link-only stand-in for @a ccmd_target_win
 * @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client)
{
    (void) client;
    return (xcb_window_t) 1;
}


/** Fixed surface/monitor pair @a ccmd_client_monitor answers with,
 *  registered by @a s_set_client_monitor; 'is_ok' controls whether
 *  the lookup succeeds at all */
static surface_td *s_monitor_surface;
static monitor_td s_monitor_current;
static bool s_monitor_ok;


/**
 * @brief Test-controlled stand-in for @a ccmd_client_monitor
 * @note Complexity: @e O(1)
 */
bool ccmd_client_monitor(client_td *client, surface_td **out_surface,
        monitor_td *out_monitor)
{
    (void) client;

    if (!s_monitor_ok) {
        return false;
    }

    if (out_surface != NULL) {
        *out_surface = s_monitor_surface;
    }
    if (out_monitor != NULL) {
        *out_monitor = s_monitor_current;
    }

    return true;
}


static void s_set_client_monitor(surface_td *surface, monitor_td current)
{
    s_monitor_surface = surface;
    s_monitor_current = current;
    s_monitor_ok = true;
}


/** The single neighboring monitor @a surface_monitor_direction
 *  answers with, registered by @a s_set_direction_neighbor; when
 *  'is_set' is false it answers back with 'current' unchanged,
 *  meaning "no neighbor that way" per the real function's own
 *  contract */
static monitor_td s_direction_neighbor;
static bool s_direction_neighbor_is_set;


/**
 * @brief Test-controlled stand-in for @a surface_monitor_direction
 * @note Complexity: @e O(1)
 */
monitor_td surface_monitor_direction(const surface_td *surface,
        monitor_td current, enum compass_direction_e direction)
{
    (void) surface;
    (void) direction;

    if (!s_direction_neighbor_is_set) {
        return current;
    }

    return s_direction_neighbor;
}


static void s_set_direction_neighbor(int32_t x, int32_t y,
        uint32_t w, uint32_t h)
{
    s_direction_neighbor.x = x;
    s_direction_neighbor.y = y;
    s_direction_neighbor.w = w;
    s_direction_neighbor.h = h;
    s_direction_neighbor_is_set = true;
}


/** Whether @a wm_request_client_redraw was called at all, checked by
 *  tests reaching 'ccmd_client_move_to_monitor', the only entry
 *  point in this file that calls it */
static int s_redraw_count;


/**
 * @brief Link-only stand-in for @a wm_request_client_redraw,
 *        recording only whether it fired
 * @note Complexity: @e O(1)
 */
void wm_request_client_redraw(client_td *client)
{
    (void) client;
    s_redraw_count++;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * Answers a fixed non-null, never-dereferenced value so
 * @a ccmd_client_apply_geometry's own null-connection refusal
 * branch is never hit by accident for a non-null 'client'; nothing
 * in this file ever reads through the pointer itself, since
 * @a xcb_configure_window below is also a stand-in rather than the
 * real libxcb call
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return (xcb_connection_t *) (uintptr_t) 1;
}


/** Recorded arguments from the last @a xcb_configure_window call */
static xcb_window_t s_cw_window;
static uint16_t s_cw_mask;
static int s_cw_count;


/**
 * @brief Recording stand-in for @a xcb_configure_window
 *
 * Stands in for the real libxcb call, which needs a genuine
 * connection to write its request to; records only what
 * @a ccmd_client_apply_geometry itself is responsible for (the
 * target window and the mask), since the values array's own
 * per-field contents are already independently visible through
 * 'client_td' itself once the caller updates it
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_configure_window(xcb_connection_t *connection,
        xcb_window_t window, uint16_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) connection;
    (void) value_list;

    s_cw_window = window;
    s_cw_mask = value_mask;
    s_cw_count++;

    return cookie;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 *
 * Swallows the warning @a ccmd_client_move_to_monitor emits on an
 * out-of-range monitor index: nothing under test here checks log
 * output.
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
    s_wa_ok = false;
    s_wa_x = 0;
    s_wa_y = 0;
    s_wa_w = 0;
    s_wa_h = 0;
    s_monitor_ok = false;
    s_monitor_surface = NULL;
    memset(&s_monitor_current, 0, sizeof(s_monitor_current));
    s_direction_neighbor_is_set = false;
    memset(&s_direction_neighbor, 0, sizeof(s_direction_neighbor));
    s_redraw_count = 0;
    s_cw_window = XCB_WINDOW_NONE;
    s_cw_mask = 0u;
    s_cw_count = 0;
    s_refill_should_touch = false;
    s_refill_x = 0;
    s_refill_y = 0;
    s_refill_w = 0u;
    s_refill_h = 0u;
}


/* A null client is a silent no-op for every entry point in this
 * file, never a crash */
static void s_test_null_client_is_noop(void)
{
    struct position_s pos = {.x = 5, .y = 5};

    s_reset();

    ccmd_client_move(NULL, pos);
    ccmd_client_center(NULL);
    ccmd_client_move_to_monitor(NULL, 0u);
    ccmd_client_move_to_monitor_north(NULL);
    ccmd_client_move_to_monitor_south(NULL);
    ccmd_client_move_to_monitor_east(NULL);
    ccmd_client_move_to_monitor_west(NULL);

    TAP_OK(true, "every entry point tolerates a null client without"
            " crashing");
}


/* A plain, unmaximized, un-fullscreen client moves to the exact
 * position asked for, and its 'has_rule_position_locked' flag is
 * cleared */
static void s_test_move_plain_client(void)
{
    client_td client;
    struct position_s pos = {.x = 123, .y = 456};

    s_reset();
    memset(&client, 0, sizeof(client));
    client.has_rule_position_locked = true;

    ccmd_client_move(&client, pos);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 123,
            "x is updated to the requested position");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 456,
            "y is updated to the requested position");
    TAP_OK(!client.has_rule_position_locked,
            "the position-locked rule flag is cleared by a move");
}


/* A maximized client refuses the move outright, leaving its geometry
 * untouched */
static void s_test_move_maximized_refused(void)
{
    client_td client;
    struct position_s pos = {.x = 999, .y = 999};

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;
    client.layout.geometry.cur.pos.x = 10;
    client.layout.geometry.cur.pos.y = 20;

    ccmd_client_move(&client, pos);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 10,
            "a maximized client's x is left untouched");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 20,
            "a maximized client's y is left untouched");
}


/* A fullscreen client refuses the move outright too */
static void s_test_move_fullscreen_refused(void)
{
    client_td client;
    struct position_s pos = {.x = 999, .y = 999};

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    client.layout.geometry.cur.pos.x = 30;
    client.layout.geometry.cur.pos.y = 40;

    ccmd_client_move(&client, pos);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 30,
            "a fullscreen client's x is left untouched");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 40,
            "a fullscreen client's y is left untouched");
}


/* A single-axis maximized client (horizontal only) is deliberately
 * let through: 'client_is_maximized' only tests true once both
 * axis bits are set together, so a client maximized on one axis
 * alone is still movable, unlike a fully maximized or fullscreen
 * one */
/* A client maximized horizontally only keeps that axis pinned to
 * where it already is; the still-free vertical axis moves normally */
static void s_test_move_maximized_horz_only_locks_x(void)
{
    client_td client;
    struct position_s pos = {.x = 1, .y = 1};

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    client.layout.geometry.cur.pos.x = 7;
    client.layout.geometry.cur.pos.y = 8;

    ccmd_client_move(&client, pos);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 7,
            "the horizontally maximized axis stays pinned to its"
            " own workarea edge, not wherever the request asked"
            " for");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 1,
            "the still-free vertical axis moves to the requested"
            " position");
}


/* A client maximized vertically only keeps that axis pinned; the
 * still-free horizontal axis moves normally */
static void s_test_move_maximized_vert_only_locks_y(void)
{
    client_td client;
    struct position_s pos = {.x = 1, .y = 1};

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED_VERT;
    client.layout.geometry.cur.pos.x = 7;
    client.layout.geometry.cur.pos.y = 8;

    ccmd_client_move(&client, pos);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 1,
            "the still-free horizontal axis moves to the requested"
            " position");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 8,
            "the vertically maximized axis stays pinned to its own"
            " workarea edge, not wherever the request asked for");
}


/* Centering a client fits it within the resolved workarea, with the
 * remainder split evenly on both sides */
static void s_test_center_plain(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    s_set_workarea(0, 0, 800, 600);

    ccmd_client_center(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 300,
            "centered x leaves an equal margin on both sides");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 250,
            "centered y leaves an equal margin above and below");
    TAP_OK(!client.has_rule_position_locked,
            "centering clears the position-locked rule flag too");
}


/* Centering offsets by the workarea's own top-left corner, not just
 * its width and height */
static void s_test_center_offset_workarea(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 100u;
    client.layout.geometry.cur.dim.h = 100u;
    s_set_workarea(50, 60, 400, 300);

    ccmd_client_center(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 200,
            "centered x is offset by the workarea's own left edge");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 160,
            "centered y is offset by the workarea's own top edge");
}


/* A client larger than the workarea in either dimension is clamped
 * so the negative margin never pushes it off past the workarea's
 * own top-left corner */
static void s_test_center_oversized_client_clamped(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 1000u;
    client.layout.geometry.cur.dim.h = 900u;
    s_set_workarea(10, 20, 800, 600);

    ccmd_client_center(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 10,
            "an oversized client's x is clamped to the workarea's"
            " own left edge, never negative past it");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 20,
            "an oversized client's y is clamped to the workarea's"
            " own top edge, never negative past it");
}


/* Centering falls back to the raw screen dimensions when no
 * workarea resolves at all and the screen fallback also refuses:
 * the client is left untouched */
static void s_test_center_no_workarea_no_fallback_is_noop(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 77;
    client.layout.geometry.cur.pos.y = 88;
    client.layout.geometry.cur.dim.w = 100u;
    client.layout.geometry.cur.dim.h = 100u;
    s_wa_ok = false;

    ccmd_client_center(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 77,
            "with no resolvable workarea and no screen fallback,"
            " x is left untouched");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 88,
            "and y is left untouched too");
}


/* A maximized client refuses centering outright, the same
 * precondition 'ccmd_client_move' shares */
static void s_test_center_maximized_refused(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;
    client.layout.geometry.cur.pos.x = 5;
    client.layout.geometry.cur.pos.y = 5;
    s_set_workarea(0, 0, 800, 600);

    ccmd_client_center(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 5,
            "a maximized client is never centered");
}


/* Centering a client maximized horizontally only keeps that axis
 * pinned to its own workarea edge; the still-free vertical axis
 * centers normally */
static void s_test_center_maximized_horz_only_locks_x(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    client.layout.geometry.cur.pos.x = 7;
    client.layout.geometry.cur.pos.y = 8;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    s_set_workarea(0, 0, 800, 600);

    ccmd_client_center(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 7,
            "the horizontally maximized axis stays pinned to its"
            " own workarea edge, not centered");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 250,
            "the still-free vertical axis centers normally");
}


/* Moving to a monitor with the same coordinates as the client's
 * current one is a silent no-op, since there is nowhere to move to */
static void s_test_move_to_monitor_same_is_noop(void)
{
    surface_td surface;
    client_td client;
    monitor_td current = {.x = 0, .y = 0, .w = 800u, .h = 600u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 1u;
    surface.monitors[0] = current;
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 100;
    client.layout.geometry.cur.pos.y = 100;
    s_set_client_monitor(&surface, current);

    ccmd_client_move_to_monitor(&client, 0u);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 100,
            "moving to the same monitor the client is already on"
            " is a no-op");
    TAP_EQ_INT(s_redraw_count, 0,
            "and no redraw is requested for a no-op move");
}


/* Moving to a genuinely different monitor translates the client's
 * offset from its old monitor's corner onto the new one's corner */
static void s_test_move_to_monitor_translates_offset(void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 0, .y = 0, .w = 800u, .h = 600u};
    monitor_td target_mon = {.x = 800, .y = 0, .w = 800u, .h = 600u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 2u;
    surface.monitors[0] = cur_mon;
    surface.monitors[1] = target_mon;
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 50;
    client.layout.geometry.cur.pos.y = 60;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    s_set_client_monitor(&surface, cur_mon);

    ccmd_client_move_to_monitor(&client, 1u);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 850,
            "x offset (50 from monitor 0's left edge) is preserved"
            " onto monitor 1's own left edge (800 + 50)");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 60,
            "y offset is preserved the same way on the untouched"
            " axis");
    TAP_EQ_INT(s_redraw_count, 1,
            "a genuine cross-monitor move requests exactly one"
            " redraw");
    TAP_OK(!client.has_rule_position_locked,
            "moving to another monitor clears the position-locked"
            " rule flag");
}


/* A fullscreen client is refused outright, the same as
 * ccmd_client_move already refuses one; there is no free axis to
 * translate at all */
static void s_test_move_to_monitor_fullscreen_refused(void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 0, .y = 0, .w = 800u, .h = 600u};
    monitor_td target_mon = {.x = 800, .y = 0, .w = 800u, .h = 600u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 2u;
    surface.monitors[0] = cur_mon;
    surface.monitors[1] = target_mon;
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    client.layout.geometry.cur.pos.x = 50;
    client.layout.geometry.cur.pos.y = 60;
    s_set_client_monitor(&surface, cur_mon);

    ccmd_client_move_to_monitor(&client, 1u);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 50,
            "a fullscreen client's x is left untouched");
    TAP_EQ_INT(s_redraw_count, 0,
            "and no redraw is requested for a refused move");
}


/* A maximized client's axis is refolded fresh against the target
 * monitor's own workarea instead of translated, and the final apply
 * carries its new width/height too, not just its position */
static void s_test_move_to_monitor_maximized_refolds_against_target(
        void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 0, .y = 0, .w = 800u, .h = 600u};
    monitor_td target_mon = {.x = 800, .y = 0, .w = 1024u, .h = 768u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 2u;
    surface.monitors[0] = cur_mon;
    surface.monitors[1] = target_mon;
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;
    client.layout.geometry.cur.pos.x = 50;
    client.layout.geometry.cur.pos.y = 60;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    s_set_client_monitor(&surface, cur_mon);
    s_refill_should_touch = true;
    s_refill_x = 800;
    s_refill_y = 0;
    s_refill_w = 1024u;
    s_refill_h = 768u;

    ccmd_client_move_to_monitor(&client, 1u);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 800,
            "the maximized client's x is refolded against the"
            " target monitor, not just translated");
    TAP_EQ_INT((long) client.layout.geometry.cur.dim.w, 1024,
            "and its width fills the target monitor exactly");
    TAP_EQ_INT((long) client.layout.geometry.cur.dim.h, 768,
            "same for height");
    TAP_EQ_INT(s_cw_mask,
            (long) ((uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y |
                (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                (uint16_t) XCB_CONFIG_WINDOW_HEIGHT),
            "the one real apply carries width/height too, not just"
            " position");
}


/* An out-of-range monitor index falls back to monitor 0 rather than
 * reading past the end of the monitors array */
static void s_test_move_to_monitor_out_of_range_falls_back(void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 500, .y = 0, .w = 400u, .h = 300u};
    monitor_td mon0 = {.x = 0, .y = 0, .w = 400u, .h = 300u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 1u;
    surface.monitors[0] = mon0;
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 550;
    client.layout.geometry.cur.pos.y = 20;
    client.layout.geometry.cur.dim.w = 50u;
    client.layout.geometry.cur.dim.h = 50u;
    s_set_client_monitor(&surface, cur_mon);

    ccmd_client_move_to_monitor(&client, 42u);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 50,
            "an out-of-range monitor index falls back to monitor 0"
            " instead of reading out of bounds");
}


/* A monitor with zero entries refuses the move outright, since there
 * is no valid index 0 to fall back to */
static void s_test_move_to_monitor_zero_monitors_refused(void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 0, .y = 0, .w = 100u, .h = 100u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 0u;
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 15;

    s_set_client_monitor(&surface, cur_mon);

    ccmd_client_move_to_monitor(&client, 0u);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 15,
            "a surface reporting zero monitors leaves the client"
            " untouched");
}


/* When the client's surface cannot be resolved at all, the move is
 * a silent no-op */
static void s_test_move_to_monitor_unresolved_surface_is_noop(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 25;
    s_monitor_ok = false;

    ccmd_client_move_to_monitor(&client, 0u);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 25,
            "an unresolved surface leaves the client untouched");
}


/* Moving to a target monitor smaller than the client, on the side
 * where the offset would otherwise spill past its far edge, clamps
 * the position back so the window stays fully on the target
 * monitor */
static void s_test_move_to_monitor_clamps_oversized_client(void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 0, .y = 0, .w = 1000u, .h = 1000u};
    monitor_td target_mon = {.x = 2000, .y = 0, .w = 300u, .h = 1000u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 2u;
    surface.monitors[0] = cur_mon;
    surface.monitors[1] = target_mon;
    memset(&client, 0, sizeof(client));
    /* Deep into monitor 0, far right; translated as-is this would
     * land at 2000 + 900 = 2900, well past target_mon's right edge
     * at 2000 + 300 = 2300 */
    client.layout.geometry.cur.pos.x = 900;
    client.layout.geometry.cur.pos.y = 0;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 200u;
    s_set_client_monitor(&surface, cur_mon);

    ccmd_client_move_to_monitor(&client, 1u);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 2100,
            "x is clamped so the client's right edge lands exactly"
            " on the smaller target monitor's own right edge"
            " (2000 + (300 - 200))");
}


/* Moving to a target monitor smaller than the client on an axis
 * where the client itself is even bigger than the whole monitor
 * clamps flush to that monitor's own top-left corner instead */
static void s_test_move_to_monitor_clamps_when_client_bigger_than_target(
        void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 0, .y = 0, .w = 1000u, .h = 1000u};
    monitor_td target_mon = {.x = 2000, .y = 0, .w = 100u, .h = 100u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 2u;
    surface.monitors[0] = cur_mon;
    surface.monitors[1] = target_mon;
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 900;
    client.layout.geometry.cur.pos.y = 0;
    client.layout.geometry.cur.dim.w = 500u;
    client.layout.geometry.cur.dim.h = 500u;
    s_set_client_monitor(&surface, cur_mon);

    ccmd_client_move_to_monitor(&client, 1u);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 2000,
            "a client wider than the whole target monitor is"
            " clamped flush to its left edge instead of a negative"
            " overhang");
}


/* A client sitting left of the target monitor's own left edge after
 * translation is clamped forward to that left edge, never left
 * hanging off the near side either */
static void s_test_move_to_monitor_clamps_near_edge(void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 500, .y = 500, .w = 1000u, .h = 1000u};
    monitor_td target_mon = {.x = 0, .y = 0, .w = 800u, .h = 800u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 2u;
    surface.monitors[0] = cur_mon;
    surface.monitors[1] = target_mon;
    memset(&client, 0, sizeof(client));
    /* At the current monitor's own left edge; translating verbatim
     * would place it at 0 - 500 + 500 = 0, already fine, so force a
     * value that would go negative instead */
    client.layout.geometry.cur.pos.x = 400;
    client.layout.geometry.cur.pos.y = 400;
    client.layout.geometry.cur.dim.w = 50u;
    client.layout.geometry.cur.dim.h = 50u;
    s_set_client_monitor(&surface, cur_mon);

    ccmd_client_move_to_monitor(&client, 1u);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 0,
            "translating past the near edge clamps forward to the"
            " target monitor's own left edge");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 0,
            "the same clamp applies to the vertical axis");
}


/* A single-monitor surface has nowhere to move toward in any
 * direction: every directional entry point is a no-op */
static void s_test_direction_single_monitor_is_noop(void)
{
    surface_td surface;
    client_td client;
    monitor_td only_mon = {.x = 0, .y = 0, .w = 800u, .h = 600u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 1u;
    surface.monitors[0] = only_mon;
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 33;
    s_set_client_monitor(&surface, only_mon);
    s_set_direction_neighbor(0, 0, 800u, 600u);

    ccmd_client_move_to_monitor_north(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 33,
            "a single-monitor surface has no north neighbor to move"
            " toward");
}


/* A zero-monitor surface (the failure branch inside
 * 's_move_to_monitor_toward' itself, not just the case above) is a
 * no-op too */
static void s_test_direction_zero_monitors_is_noop(void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 0, .y = 0, .w = 100u, .h = 100u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 0u;
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 71;
    s_set_client_monitor(&surface, cur_mon);

    ccmd_client_move_to_monitor_south(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 71,
            "a zero-monitor surface is a no-op for a directional"
            " move too");
}


/* When 'ccmd_client_monitor' itself cannot resolve a surface, every
 * directional entry point is a no-op */
static void s_test_direction_unresolved_surface_is_noop(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 88;
    s_monitor_ok = false;

    ccmd_client_move_to_monitor_east(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 88,
            "an unresolved surface is a no-op for a directional"
            " move too");
}


/* A real neighbor to the west is found by matching coordinates in
 * the monitors array, and the move lands exactly on it */
static void s_test_direction_west_finds_real_neighbor(void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 800, .y = 0, .w = 800u, .h = 600u};
    monitor_td west_mon = {.x = 0, .y = 0, .w = 800u, .h = 600u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 2u;
    surface.monitors[0] = cur_mon;
    surface.monitors[1] = west_mon;
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 850;
    client.layout.geometry.cur.pos.y = 10;
    client.layout.geometry.cur.dim.w = 50u;
    client.layout.geometry.cur.dim.h = 50u;
    s_set_client_monitor(&surface, cur_mon);
    s_set_direction_neighbor(0, 0, 800u, 600u);

    ccmd_client_move_to_monitor_west(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 50,
            "the client lands translated onto the real western"
            " neighbor found in the monitors array");
    TAP_EQ_INT(s_redraw_count, 1,
            "a genuine directional move requests exactly one"
            " redraw");
}


/* If the neighbor the direction search reports back does not
 * actually match any entry in the monitors array (a contradiction
 * that should never happen in practice, but the loop still simply
 * falls through), nothing is moved */
static void s_test_direction_neighbor_not_in_array_is_noop(void)
{
    surface_td surface;
    client_td client;
    monitor_td cur_mon = {.x = 0, .y = 0, .w = 800u, .h = 600u};

    s_reset();
    memset(&surface, 0, sizeof(surface));
    surface.monitor_count = 1u;
    surface.monitors[0] = cur_mon;
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 5;
    s_set_client_monitor(&surface, cur_mon);
    /* Reports a neighbor whose coordinates match nothing in
     * 'surface.monitors' at all */
    s_set_direction_neighbor(9000, 9000, 1u, 1u);

    ccmd_client_move_to_monitor_east(&client);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 5,
            "a reported neighbor absent from the monitors array"
            " leaves the client untouched");
}


/* 'ccmd_client_apply_geometry' itself: a null client is a silent
 * no-op */
static void s_test_apply_geometry_null_client_is_noop(void)
{
    s_reset();

    ccmd_client_apply_geometry(NULL, (xcb_window_t) 1,
            (uint16_t) XCB_CONFIG_WINDOW_X, 1, 2, 3u, 4u, 5u);

    TAP_OK(true, "a null client is a silent no-op for"
            " ccmd_client_apply_geometry");
}


/* A target of 'XCB_WINDOW_NONE' is a silent no-op too */
static void s_test_apply_geometry_none_target_is_noop(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));

    ccmd_client_apply_geometry(&client, XCB_WINDOW_NONE,
            (uint16_t) XCB_CONFIG_WINDOW_X, 1, 2, 3u, 4u, 5u);

    TAP_OK(true, "an XCB_WINDOW_NONE target is a silent no-op for"
            " ccmd_client_apply_geometry");
}


/* A zero mask still runs harmlessly through the function, issuing
 * an XCB configure request with no values selected at all */
static void s_test_apply_geometry_zero_mask_is_safe(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));

    ccmd_client_apply_geometry(&client, (xcb_window_t) 1,
            0u, 1, 2, 3u, 4u, 5u);

    TAP_OK(true, "a zero mask runs through ccmd_client_apply_geometry"
            " without touching any of the optional fields");
}


/* Every bit set at once is also safe, exercising the full values
 * array build in one call */
static void s_test_apply_geometry_full_mask_is_safe(void)
{
    client_td client;
    uint16_t mask = (uint16_t) XCB_CONFIG_WINDOW_X |
        (uint16_t) XCB_CONFIG_WINDOW_Y |
        (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
        (uint16_t) XCB_CONFIG_WINDOW_HEIGHT |
        (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH;

    s_reset();
    memset(&client, 0, sizeof(client));

    ccmd_client_apply_geometry(&client, (xcb_window_t) 1,
            mask, -5, -6, 100u, 200u, 2u);

    TAP_OK(true, "every XCB_CONFIG_WINDOW_* bit set at once runs"
            " through ccmd_client_apply_geometry safely");
}


int main(void)
{
    TAP_PLAN(53);

    s_test_null_client_is_noop();
    s_test_move_plain_client();
    s_test_move_maximized_refused();
    s_test_move_fullscreen_refused();
    s_test_move_maximized_horz_only_locks_x();
    s_test_move_maximized_vert_only_locks_y();
    s_test_center_plain();
    s_test_center_offset_workarea();
    s_test_center_oversized_client_clamped();
    s_test_center_no_workarea_no_fallback_is_noop();
    s_test_center_maximized_refused();
    s_test_center_maximized_horz_only_locks_x();
    s_test_move_to_monitor_same_is_noop();
    s_test_move_to_monitor_translates_offset();
    s_test_move_to_monitor_fullscreen_refused();
    s_test_move_to_monitor_maximized_refolds_against_target();
    s_test_move_to_monitor_out_of_range_falls_back();
    s_test_move_to_monitor_zero_monitors_refused();
    s_test_move_to_monitor_unresolved_surface_is_noop();
    s_test_move_to_monitor_clamps_oversized_client();
    s_test_move_to_monitor_clamps_when_client_bigger_than_target();
    s_test_move_to_monitor_clamps_near_edge();
    s_test_direction_single_monitor_is_noop();
    s_test_direction_zero_monitors_is_noop();
    s_test_direction_unresolved_surface_is_noop();
    s_test_direction_west_finds_real_neighbor();
    s_test_direction_neighbor_not_in_array_is_noop();
    s_test_apply_geometry_null_client_is_noop();
    s_test_apply_geometry_none_target_is_noop();
    s_test_apply_geometry_zero_mask_is_safe();
    s_test_apply_geometry_full_mask_is_safe();

    return TAP_DONE();
}
