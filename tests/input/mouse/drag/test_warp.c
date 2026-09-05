/**
 * @file tests/input/mouse/drag/test_warp.c
 *
 * @brief Test battery for the screen-edge desktop warp during a drag
 *
 * warp.c reaches deep into the running window manager: the surface
 * and desktop it warps across (@a wm_get_surface_by_id,
 * @a wm_get_client_desktop, @a surface_desktop_get), the transient
 * family it drags along (@c cmds/client/transient.c), the actual X
 * requests that move the pointer and windows
 * (@c xcb_warp_pointer, @c xcb_configure_window, @a enact_client_move),
 * and the desktop-switch chrome (@a notify_desktop_show,
 * @a surface_clients_hide, @a surface_clients_show).  None of that is
 * needed to exercise this file's real target: the edge-detection and
 * countdown bookkeeping in @a drag_warp_edge_check and
 * @a drag_warp_ms_remaining, and the early-exit guards at the top of
 * @a drag_warp_tick, so every one of those heavier calls is a
 * link-only or recording stand-in below instead.  @a clock_gettime and
 * @a clock_ms_until/@a clock_add_ms (utils/time/clock.c) are linked
 * real, since the countdown math itself is exactly what is under
 * test.  Storage for @c s_drag lives in drag.c, which this file never
 * links, so it is defined here instead, the same way drag.c itself
 * would define it.
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

/* Project includes */
#include <client.h>
#include <cmds/client/transient.h>
#include <desktop.h>
#include <enact.h>
#include <input/mouse/drag/icon.h>
#include <input/mouse/drag/outline.h>
#include <input/mouse/drag/overlay.h>
#include <menu/notify/desktop.h>
#include <policy/focus.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/warp.h>
#include <utils/time/clock.h>


/** Singleton drag state; storage normally lives in drag.c, which this
 *  file never links, so it is defined here instead */
drag_state_td s_drag;

/** Surface @a wm_get_surface_by_id hands back, @c NULL to make the
 *  lookup itself fail */
static surface_td *s_stub_surface;

/** Count of every heavy call this file only records rather than acts
 *  on, reset by @a s_reset before each scenario */
static int s_call_enact_client_move;
static int s_call_surface_clients_hide;
static int s_call_surface_clients_show;
static int s_call_notify_desktop_show;
static int s_call_drag_outline_move;
static int s_call_drag_overlay_show;
static int s_call_desktop_action_client_add;
static int s_call_desktop_action_client_rem;

/** Value @a scmd_surface_viewport_pan_available hands back, reset to
 *  @c false (the sensible "no room to pan" default) by @a s_reset
 *  before each scenario */
static bool s_stub_pan_available;


/**
 * @brief Link-only stand-in for @a wm_get_surface_by_id
 *
 * @note Complexity: @e O(1)
 */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return s_stub_surface;
}


/**
 * @brief Link-only stand-in for @a wm_get_client_desktop, never
 *        exercised by any scenario below
 *
 * @note Complexity: @e O(1)
 */
desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a surface_desktop_get
 *
 * @note Complexity: @e O(1)
 */
desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a surface_desktop_north
 *
 * @note Complexity: @e O(1)
 */
desktop_td *surface_desktop_north(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    (void) surface;
    (void) desktop_id;
    (void) cycle;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a surface_desktop_south
 *
 * @note Complexity: @e O(1)
 */
desktop_td *surface_desktop_south(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    (void) surface;
    (void) desktop_id;
    (void) cycle;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a surface_desktop_east
 *
 * @note Complexity: @e O(1)
 */
desktop_td *surface_desktop_east(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    (void) surface;
    (void) desktop_id;
    (void) cycle;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a surface_desktop_west
 *
 * @note Complexity: @e O(1)
 */
desktop_td *surface_desktop_west(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    (void) surface;
    (void) desktop_id;
    (void) cycle;
    return NULL;
}


/**
 * @brief Recording stand-in for @a surface_clients_hide
 *
 * @note Complexity: @e O(1)
 */
void surface_clients_hide(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
    s_call_surface_clients_hide++;
}


/**
 * @brief Recording stand-in for @a surface_clients_show
 *
 * @note Complexity: @e O(1)
 */
void surface_clients_show(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
    s_call_surface_clients_show++;
}


/**
 * @brief Recording stand-in for @a desktop_action_client_add
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_add(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    s_call_desktop_action_client_add++;
    return 0;
}


/**
 * @brief Recording stand-in for @a desktop_action_client_rem
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_rem(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    s_call_desktop_action_client_rem++;
    return 0;
}


/**
 * @brief Link-only stand-in for @a desktop_action_client_move, never
 *        exercised by any scenario below
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_move(desktop_td *from, desktop_td *to,
        client_td *client)
{
    (void) from;
    (void) to;
    (void) client;
    return 0;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_transient_top_parent
 *
 * @note Complexity: @e O(1)
 */
client_td *ccmd_client_transient_top_parent(client_td *client)
{
    return client;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_transient_family_snapshot,
 *        never exercised by any scenario below
 *
 * @note Complexity: @e O(1)
 */
client_td **ccmd_client_transient_family_snapshot(const desktop_td *desktop,
        client_td *top, size_t *out_count)
{
    (void) desktop;
    (void) top;
    *out_count = 0;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a focus_order_to_top, never exercised
 *        by any scenario below
 *
 * @note Complexity: @e O(1)
 */
void focus_order_to_top(client_td *client)
{
    (void) client;
}


/**
 * @brief Recording stand-in for @a notify_desktop_show
 *
 * @note Complexity: @e O(1)
 */
void notify_desktop_show(xcb_connection_t *connection, surface_td *surface,
        uint32_t desktop_idx, const char *desktop_name,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) desktop_idx;
    (void) desktop_name;
    (void) config;
    s_call_notify_desktop_show++;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get, never exercised
 *        by any scenario below
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/**
 * @brief Recording stand-in for @a enact_client_move
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move(client_td *client, struct position_s pos)
{
    (void) client;
    (void) pos;
    s_call_enact_client_move++;
}


/**
 * @brief Recording stand-in for @a drag_outline_move
 *
 * @note Complexity: @e O(1)
 */
void drag_outline_move(xcb_connection_t *connection, struct geometry_s geom)
{
    (void) connection;
    (void) geom;
    s_call_drag_outline_move++;
}


/**
 * @brief Recording stand-in for @a drag_overlay_show
 *
 * @note Complexity: @e O(1)
 */
void drag_overlay_show(xcb_connection_t *connection, bool is_icon,
        struct geometry_s target, const char *text)
{
    (void) connection;
    (void) is_icon;
    (void) target;
    (void) text;
    s_call_drag_overlay_show++;
}


/**
 * @brief Link-only stand-in for @a drag_icon_height, never exercised
 *        by any scenario below
 *
 * @note Complexity: @e O(1)
 */
uint16_t drag_icon_height(const client_td *client)
{
    (void) client;
    return 0u;
}


/**
 * @brief Controllable stand-in for @a scmd_surface_viewport_pan_available
 *
 * @note Complexity: @e O(1)
 */
bool scmd_surface_viewport_pan_available(surface_td *surface,
        enum compass_direction_e direction)
{
    (void) surface;
    (void) direction;
    return s_stub_pan_available;
}


static void s_reset(void)
{
    static client_td dragged;
    static surface_td surface;
    static config_td config;

    memset(&s_drag, 0, sizeof(s_drag));
    memset(&dragged, 0, sizeof(dragged));
    memset(&surface, 0, sizeof(surface));
    memset(&config, 0, sizeof(config));

    dragged.id = 1u;
    dragged.screen_id = 0u;

    config.desktops.warp_on_edge_drag = true;

    surface.config = &config;
    surface.desktop_count = 4u;

    s_drag.client = &dragged;
    s_drag.screen_w = 1920u;
    s_drag.screen_h = 1080u;

    s_stub_surface = &surface;

    s_call_enact_client_move = 0;
    s_call_surface_clients_hide = 0;
    s_call_surface_clients_show = 0;
    s_call_notify_desktop_show = 0;
    s_call_drag_outline_move = 0;
    s_call_drag_overlay_show = 0;
    s_call_desktop_action_client_add = 0;
    s_call_desktop_action_client_rem = 0;
    s_stub_pan_available = false;
}


/* No client under drag at all: the edge check clears any pending warp
 * and does nothing else */
static void s_test_edge_check_no_client_clears_pending(void)
{
    s_reset();
    s_drag.client = NULL;
    s_drag.is_warp_pending = true;

    drag_warp_edge_check(0, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "no dragged client: pending warp is cleared");
}


/* wm_get_surface_by_id fails to resolve a surface: no-op */
static void s_test_edge_check_no_surface_is_noop(void)
{
    s_reset();
    s_stub_surface = NULL;

    drag_warp_edge_check(0, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "surface lookup failing clears any pending warp");
}


/* warp_on_edge_drag is off in configuration: no-op even at an edge */
static void s_test_edge_check_disabled_in_config_is_noop(void)
{
    s_reset();
    s_stub_surface->config->desktops.warp_on_edge_drag = false;

    drag_warp_edge_check(0, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "warp_on_edge_drag false: edge touch never arms a warp");
}


/* Only one desktop on the surface: no-op even at an edge, there being
 * nowhere else to warp to */
static void s_test_edge_check_single_desktop_is_noop(void)
{
    s_reset();
    s_stub_surface->desktop_count = 1u;

    drag_warp_edge_check(0, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "a single-desktop surface never arms a warp");
}


/* Pointer well inside the screen on both axes: no-op */
static void s_test_edge_check_middle_of_screen_is_noop(void)
{
    s_reset();

    drag_warp_edge_check(960, 540);

    TAP_OK(!s_drag.is_warp_pending,
            "pointer away from every edge never arms a warp");
}


/* Pointer at the exact left edge (x == 0) arms a west warp */
static void s_test_edge_check_left_edge_arms_west(void)
{
    s_reset();

    drag_warp_edge_check(0, 500);

    TAP_OK(s_drag.is_warp_pending, "x=0 arms a pending warp");
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_WEST,
            "direction is west");
}


/* Pointer at the left edge, but the viewport still has room to pan
 * that same edge with 'pan_on_edge_drag' enabled: the warp defers to
 * the pan instead of arming */
static void s_test_edge_check_pan_available_defers_to_pan(void)
{
    s_reset();
    s_stub_surface->config->desktops.pan_on_edge_drag = true;
    s_stub_pan_available = true;

    drag_warp_edge_check(0, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "viewport pan still available: no warp armed");
}


/* Pointer one pixel inside the left edge (x == 1) does not arm a
 * warp: the boundary is inclusive of 0, not 1 */
static void s_test_edge_check_one_past_left_edge_is_noop(void)
{
    s_reset();

    drag_warp_edge_check(1, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "x=1 is one pixel short of the left edge: no warp armed");
}


/* Pointer at the exact right edge (x == screen_w - 1) arms an east
 * warp */
static void s_test_edge_check_right_edge_arms_east(void)
{
    s_reset();

    drag_warp_edge_check(1919, 500);

    TAP_OK(s_drag.is_warp_pending, "x=screen_w-1 arms a pending warp");
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_EAST,
            "direction is east");
}


/* Pointer at the exact top edge (y == 0) arms a north warp */
static void s_test_edge_check_top_edge_arms_north(void)
{
    s_reset();

    drag_warp_edge_check(960, 0);

    TAP_OK(s_drag.is_warp_pending, "y=0 arms a pending warp");
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_NORTH,
            "direction is north");
}


/* Pointer at the exact bottom edge (y == screen_h - 1) arms a south
 * warp */
static void s_test_edge_check_bottom_edge_arms_south(void)
{
    s_reset();

    drag_warp_edge_check(960, 1079);

    TAP_OK(s_drag.is_warp_pending, "y=screen_h-1 arms a pending warp");
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_SOUTH,
            "direction is south");
}


/* A corner holds two edges at once: the horizontal direction always
 * wins over the vertical one */
static void s_test_edge_check_corner_prefers_horizontal(void)
{
    s_reset();

    drag_warp_edge_check(0, 0);

    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_WEST,
            "top-left corner: west (horizontal) wins over north");

    s_reset();

    drag_warp_edge_check(1919, 1079);

    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_EAST,
            "bottom-right corner: east (horizontal) wins over south");
}


/* Holding the same edge across two calls keeps the original countdown
 * running rather than restarting it */
static void s_test_edge_check_same_edge_keeps_countdown(void)
{
    struct timespec first_due;

    s_reset();

    drag_warp_edge_check(0, 500);
    first_due = s_drag.warp_due;

    drag_warp_edge_check(0, 501);

    TAP_OK(s_drag.warp_due.tv_sec == first_due.tv_sec &&
            s_drag.warp_due.tv_nsec == first_due.tv_nsec,
            "the same edge held again does not restart the countdown");
}


/* Switching to a different edge mid-hold re-arms a fresh countdown
 * for the new direction */
static void s_test_edge_check_different_edge_rearms(void)
{
    s_reset();

    drag_warp_edge_check(0, 500);
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_WEST,
            "first touch arms west");

    drag_warp_edge_check(960, 0);
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_NORTH,
            "moving to the top edge re-arms as north instead");
}


/* Leaving every edge after being armed clears the pending warp again */
static void s_test_edge_check_leaving_edge_clears_pending(void)
{
    s_reset();

    drag_warp_edge_check(0, 500);
    TAP_OK(s_drag.is_warp_pending, "armed at the left edge");

    drag_warp_edge_check(960, 540);
    TAP_OK(!s_drag.is_warp_pending,
            "moving back to the middle of the screen clears it");
}


/* drag_warp_ms_remaining returns -1 whenever nothing is pending */
static void s_test_ms_remaining_returns_negative_when_idle(void)
{
    s_reset();
    s_drag.is_warp_pending = false;

    TAP_EQ_INT(drag_warp_ms_remaining(), -1,
            "no pending warp: -1, never a stale countdown");
}


/* drag_warp_ms_remaining reflects a countdown armed by the edge check
 * itself: strictly positive and no larger than the configured delay */
static void s_test_ms_remaining_positive_right_after_arming(void)
{
    int remaining;

    s_reset();

    drag_warp_edge_check(0, 500);
    remaining = drag_warp_ms_remaining();

    TAP_OK(remaining > 0 && remaining <= WM_DESKTOP_WARP_DELAY_MS,
            "freshly armed countdown is within (0, configured delay]");
}


/* drag_warp_ms_remaining reads 0, never negative, once due has
 * already passed */
static void s_test_ms_remaining_zero_once_due_has_passed(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 10;

    TAP_EQ_INT(drag_warp_ms_remaining(), 0,
            "a due time already 10 seconds in the past reads back as"
            " exactly 0, never negative");
}


/* drag_warp_tick: a NULL connection is a no-op, touching nothing */
static void s_test_tick_null_connection_is_noop(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;

    drag_warp_tick(NULL);

    TAP_OK(s_drag.is_warp_pending,
            "a NULL connection leaves the pending flag untouched");
    TAP_EQ_INT(s_call_surface_clients_hide, 0,
            "and never reaches the actual warp machinery");
}


/* drag_warp_tick: nothing pending is a no-op */
static void s_test_tick_nothing_pending_is_noop(void)
{
    s_reset();
    s_drag.is_warp_pending = false;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_surface_clients_hide, 0,
            "nothing pending: the tick never touches the desktop"
            " switch machinery");
}


/* drag_warp_tick: countdown still running (ms_remaining > 0) is a
 * no-op, and leaves the pending flag set for the next tick */
static void s_test_tick_countdown_not_due_is_noop(void)
{
    s_reset();

    drag_warp_edge_check(0, 500);
    TAP_OK(s_drag.is_warp_pending, "armed by the edge check");

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_OK(s_drag.is_warp_pending,
            "countdown not yet due: still pending after the tick");
    TAP_EQ_INT(s_call_surface_clients_hide, 0,
            "and the desktop switch itself never ran");
}


/* drag_warp_tick: due, but no client under drag any more: clears
 * pending and stops there */
static void s_test_tick_due_no_client_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.client = NULL;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_OK(!s_drag.is_warp_pending,
            "due tick with no client: pending flag is cleared anyway");
    TAP_EQ_INT(s_call_surface_clients_hide, 0,
            "but the desktop switch itself never ran");
}


/* drag_warp_tick: due, but the operation is a resize, not a move:
 * stops before touching the desktop switch machinery */
static void s_test_tick_due_wrong_operation_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_RESIZING;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_surface_clients_hide, 0,
            "a resize in progress never triggers the desktop switch");
}


/* drag_warp_tick: due, moving, but drag_window names neither
 * XCB_WINDOW_NONE nor the dragged client's own icon window: stale
 * state, stops before the switch */
static void s_test_tick_due_stale_drag_window_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.client->icon_window = 42u;
    s_drag.drag_window = 99u;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_surface_clients_hide, 0,
            "drag_window matching neither XCB_WINDOW_NONE nor the"
            " client's own icon window never triggers the switch");
}


/* drag_warp_tick: due, moving, drag_window matches the client's own
 * icon window (an icon drag): allowed past that guard, only stopped
 * later by the surface having a single desktop */
static void s_test_tick_icon_drag_window_matches_is_allowed(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.client->icon_window = 42u;
    s_drag.drag_window = 42u;
    s_stub_surface->desktop_count = 1u;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_surface_clients_hide, 0,
            "a matching icon window clears the drag_window guard, but"
            " the single-desktop surface still stops it right after");
}


/* drag_warp_tick: due, moving, but the resolved surface has only one
 * desktop: stops before touching the desktop switch machinery */
static void s_test_tick_due_single_desktop_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_stub_surface->desktop_count = 1u;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_surface_clients_hide, 0,
            "a single-desktop surface stops the tick before the"
            " desktop switch itself");
}


/* drag_warp_tick: due, moving, but warp_on_edge_drag was disabled
 * after the countdown was already armed: stops before the switch */
static void s_test_tick_due_config_disabled_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_stub_surface->config->desktops.warp_on_edge_drag = false;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_surface_clients_hide, 0,
            "warp_on_edge_drag turned off since arming: the switch"
            " itself never runs");
}


/* drag_warp_tick: due, moving, surface itself no longer resolvable:
 * stops before the switch */
static void s_test_tick_due_no_surface_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_stub_surface = NULL;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_surface_clients_hide, 0,
            "the surface lookup failing stops the tick before the"
            " switch itself");
}


int main(void)
{
    TAP_PLAN(39);

    s_test_edge_check_no_client_clears_pending();
    s_test_edge_check_no_surface_is_noop();
    s_test_edge_check_disabled_in_config_is_noop();
    s_test_edge_check_single_desktop_is_noop();
    s_test_edge_check_middle_of_screen_is_noop();
    s_test_edge_check_left_edge_arms_west();
    s_test_edge_check_pan_available_defers_to_pan();
    s_test_edge_check_one_past_left_edge_is_noop();
    s_test_edge_check_right_edge_arms_east();
    s_test_edge_check_top_edge_arms_north();
    s_test_edge_check_bottom_edge_arms_south();
    s_test_edge_check_corner_prefers_horizontal();
    s_test_edge_check_same_edge_keeps_countdown();
    s_test_edge_check_different_edge_rearms();
    s_test_edge_check_leaving_edge_clears_pending();
    s_test_ms_remaining_returns_negative_when_idle();
    s_test_ms_remaining_positive_right_after_arming();
    s_test_ms_remaining_zero_once_due_has_passed();
    s_test_tick_null_connection_is_noop();
    s_test_tick_nothing_pending_is_noop();
    s_test_tick_countdown_not_due_is_noop();
    s_test_tick_due_no_client_stops_early();
    s_test_tick_due_wrong_operation_stops_early();
    s_test_tick_due_stale_drag_window_stops_early();
    s_test_tick_icon_drag_window_matches_is_allowed();
    s_test_tick_due_single_desktop_stops_early();
    s_test_tick_due_config_disabled_stops_early();
    s_test_tick_due_no_surface_stops_early();

    return TAP_DONE();
}
