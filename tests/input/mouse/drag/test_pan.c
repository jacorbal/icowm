/**
 * @file tests/input/mouse/drag/test_pan.c
 *
 * @brief Test battery for the screen-edge viewport pan during a drag
 *
 * pan.c reaches deep into the running window manager: the stage and
 * desktop it pans across (@a wm_get_stage_by_id,
 * @a lookup_current_desktop), the actual pan commands
 * (@c cmds/stage.c's @a scmd_stage_viewport_pan_north and its
 * three siblings, plus @a scmd_stage_viewport_pan_available), and
 * the raw X requests moving an icon window or outline
 * (@c xcb_configure_window, @a drag_outline_move, @a drag_overlay_show).
 * None of that is needed to exercise this file's real target: the
 * edge-detection and countdown bookkeeping in @a drag_pan_edge_check
 * and @a drag_pan_ms_remaining, the guards at the top of
 * @a drag_pan_tick, and its own delta/drag-state fixup once a pan is
 * actually due, so every one of those heavier calls is a link-only or
 * recording (or, where the resulting delta needs checking, a
 * controlled) stand-in below instead.  @a clock_gettime and
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
#include <client/predicates.h>
#include <desktop.h>
#include <input/mouse/drag/icon.h>
#include <input/mouse/drag/outline.h>
#include <input/mouse/drag/overlay.h>
#include <stage.h>
#include <wm.h>

/* Command includes */
#include <cmds/stage.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/pan.h>
#include <utils/time/clock.h>


/** Singleton drag state; storage normally lives in drag.c, which this
 *  file never links, so it is defined here instead */
drag_state_td s_drag;

/** Stage @a wm_get_stage_by_id hands back, @c NULL to make the
 *  lookup itself fail */
static stage_td *s_stub_stage;

/** Desktop @a lookup_current_desktop hands back, @c NULL to make the
 *  lookup itself fail */
static desktop_td *s_stub_desktop;

/** Value @a scmd_stage_viewport_pan_available hands back, reset to
 *  @c false (the sensible "no room to pan" default) by @a s_reset
 *  before each scenario */
static bool s_stub_pan_available;

/** Count of every heavy pan command this file only records rather
 *  than acts on, reset by @a s_reset before each scenario */
static int s_call_pan_north;
static int s_call_pan_south;
static int s_call_pan_east;
static int s_call_pan_west;

/** When true, every direction's recording stand-in sets the stub
 *  desktop's @c viewport_origin straight to @a s_stub_origin_override
 *  instead of doing its own unconditional whole-screen shift,
 *  standing in for whatever a real, possibly boundary-clamped
 *  'scmd_stage_viewport_pan_*' call (cmds/stage.c) would have
 *  actually left it at; reset to @c false by @a s_reset before each
 *  scenario */
static bool s_stub_origin_override_active;

/** Origin @a s_stub_origin_override_active switches every direction's
 *  stand-in over to, reset to @c { 0, 0 } by @a s_reset before each
 *  scenario */
static struct position_s s_stub_origin_override;

/** Count of every other heavy call this file only records rather than
 *  acts on, reset by @a s_reset before each scenario */
static int s_call_configure_window;
static int s_call_drag_outline_move;
static int s_call_drag_overlay_show;

/** Last window, mask, and X/Y values handed to
 *  @a xcb_configure_window, for scenarios that check the icon really
 *  gets repositioned to the expected spot */
static xcb_window_t s_configure_window_last_window;
static uint16_t s_configure_window_last_mask;
static int32_t s_configure_window_last_x;
static int32_t s_configure_window_last_y;

/** Count of calls to the @a xcb_warp_pointer stand-in below, and the
 *  last X/Y it was asked to warp the pointer to, reset by @a s_reset
 *  before each scenario */
static int s_call_xcb_warp_pointer;
static int16_t s_warp_pointer_last_x;
static int16_t s_warp_pointer_last_y;


/**
 * @brief Link-only stand-in for @a wm_get_stage_by_id
 *
 * @note Complexity: @e O(1)
 */
stage_td *wm_get_stage_by_id(uint32_t stage_id)
{
    (void) stage_id;
    return s_stub_stage;
}


/**
 * @brief Link-only stand-in for @a lookup_current_desktop
 *
 * @note Complexity: @e O(1)
 */
desktop_td *lookup_current_desktop(stage_td *stage)
{
    (void) stage;
    return s_stub_desktop;
}


/**
 * @brief Controllable stand-in for @a scmd_stage_viewport_pan_available
 *
 * @note Complexity: @e O(1)
 */
bool scmd_stage_viewport_pan_available(stage_td *stage,
        enum compass_direction_e direction)
{
    (void) stage;
    (void) direction;
    return s_stub_pan_available;
}


/**
 * @brief Recording stand-in for @a scmd_stage_viewport_pan_north
 *
 * Also shifts the stub desktop's own @c viewport_origin exactly the
 * way the real function would, since @a drag_pan_tick now reads that
 * field back itself (before and after this call) to work out the
 * delta it hands the dragged client, rather than assuming a fixed
 * whole-screen step.
 *
 * @note Complexity: @e O(1)
 */
void scmd_stage_viewport_pan_north(stage_td *stage)
{
    (void) stage;
    s_call_pan_north++;
    if (s_stub_origin_override_active) {
        s_stub_desktop->viewport_origin = s_stub_origin_override;
    } else {
        s_stub_desktop->viewport_origin.y -=
            (int32_t) s_stub_desktop->geometry.dim.h;
    }
}


/**
 * @brief Recording stand-in for @a scmd_stage_viewport_pan_south
 *
 * @note Complexity: @e O(1)
 */
void scmd_stage_viewport_pan_south(stage_td *stage)
{
    (void) stage;
    s_call_pan_south++;
    if (s_stub_origin_override_active) {
        s_stub_desktop->viewport_origin = s_stub_origin_override;
    } else {
        s_stub_desktop->viewport_origin.y +=
            (int32_t) s_stub_desktop->geometry.dim.h;
    }
}


/**
 * @brief Recording stand-in for @a scmd_stage_viewport_pan_east
 *
 * @note Complexity: @e O(1)
 */
void scmd_stage_viewport_pan_east(stage_td *stage)
{
    (void) stage;
    s_call_pan_east++;
    if (s_stub_origin_override_active) {
        s_stub_desktop->viewport_origin = s_stub_origin_override;
    } else {
        s_stub_desktop->viewport_origin.x +=
            (int32_t) s_stub_desktop->geometry.dim.w;
    }
}


/**
 * @brief Recording stand-in for @a scmd_stage_viewport_pan_west
 *
 * @note Complexity: @e O(1)
 */
void scmd_stage_viewport_pan_west(stage_td *stage)
{
    (void) stage;
    s_call_pan_west++;
    if (s_stub_origin_override_active) {
        s_stub_desktop->viewport_origin = s_stub_origin_override;
    } else {
        s_stub_desktop->viewport_origin.x -=
            (int32_t) s_stub_desktop->geometry.dim.w;
    }
}


/**
 * @brief Recording stand-in for the raw @a xcb_configure_window
 *        request
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_configure_window(xcb_connection_t *c,
        xcb_window_t window, uint16_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie;
    const uint32_t *vals;

    (void) c;

    memset(&cookie, 0, sizeof(cookie));
    s_call_configure_window++;
    s_configure_window_last_window = window;
    s_configure_window_last_mask = value_mask;

    vals = value_list;
    s_configure_window_last_x = (int32_t) vals[0];
    s_configure_window_last_y = (int32_t) vals[1];

    return cookie;
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
 * @brief Recording stand-in for the raw @a xcb_warp_pointer request
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_warp_pointer(xcb_connection_t *c,
        xcb_window_t src_window, xcb_window_t dst_window,
        int16_t src_x, int16_t src_y, uint16_t src_width,
        uint16_t src_height, int16_t dst_x, int16_t dst_y)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) src_window;
    (void) dst_window;
    (void) src_x;
    (void) src_y;
    (void) src_width;
    (void) src_height;

    memset(&cookie, 0, sizeof(cookie));
    s_call_xcb_warp_pointer++;
    s_warp_pointer_last_x = dst_x;
    s_warp_pointer_last_y = dst_y;

    return cookie;
}


static void s_reset(void)
{
    static client_td dragged;
    static stage_td stage;
    static config_td config;
    static desktop_td desktop;
    static xcb_screen_t screen;

    memset(&s_drag, 0, sizeof(s_drag));
    memset(&dragged, 0, sizeof(dragged));
    memset(&stage, 0, sizeof(stage));
    memset(&config, 0, sizeof(config));
    memset(&desktop, 0, sizeof(desktop));
    memset(&screen, 0, sizeof(screen));

    s_stub_origin_override_active = false;
    s_stub_origin_override = (struct position_s) { 0, 0 };

    dragged.id = 1u;
    dragged.screen_id = 0u;
    dragged.icon_window = 42u;

    config.base.viewport.pan_on_edge_drag = true;

    stage.config = &config;
    stage.screen = &screen;

    desktop.geometry.dim.w = 1920u;
    desktop.geometry.dim.h = 1080u;

    s_drag.client = &dragged;
    s_drag.screen_w = 1920u;
    s_drag.screen_h = 1080u;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.is_solid_drag = true;

    s_stub_stage = &stage;
    s_stub_desktop = &desktop;
    s_stub_pan_available = true;

    s_call_pan_north = 0;
    s_call_pan_south = 0;
    s_call_pan_east = 0;
    s_call_pan_west = 0;
    s_call_configure_window = 0;
    s_call_drag_outline_move = 0;
    s_call_drag_overlay_show = 0;
    s_call_xcb_warp_pointer = 0;
    s_warp_pointer_last_x = 0;
    s_warp_pointer_last_y = 0;
    s_configure_window_last_window = XCB_WINDOW_NONE;
    s_configure_window_last_mask = 0u;
    s_configure_window_last_x = 0;
    s_configure_window_last_y = 0;
}


/* No client under drag at all: the edge check clears any pending pan
 * and does nothing else */
static void s_test_edge_check_no_client_clears_pending(void)
{
    s_reset();
    s_drag.client = NULL;
    s_drag.is_pan_pending = true;

    drag_pan_edge_check(0, 500);

    TAP_OK(!s_drag.is_pan_pending,
            "no dragged client: pending pan is cleared");
}


/* wm_get_stage_by_id fails to resolve a stage: no-op */
static void s_test_edge_check_no_stage_is_noop(void)
{
    s_reset();
    s_stub_stage = NULL;

    drag_pan_edge_check(0, 500);

    TAP_OK(!s_drag.is_pan_pending,
            "stage lookup failing clears any pending pan");
}


/* pan_on_edge_drag is off in configuration: no-op even at an edge */
static void s_test_edge_check_disabled_in_config_is_noop(void)
{
    s_reset();
    s_stub_stage->config->base.viewport.pan_on_edge_drag = false;

    drag_pan_edge_check(0, 500);

    TAP_OK(!s_drag.is_pan_pending,
            "pan_on_edge_drag false: edge touch never arms a pan");
}


/* Pointer well inside the screen on both axes: no-op */
static void s_test_edge_check_middle_of_screen_is_noop(void)
{
    s_reset();

    drag_pan_edge_check(960, 540);

    TAP_OK(!s_drag.is_pan_pending,
            "pointer away from every edge never arms a pan");
}


/* At the left edge, but the viewport has no room left to pan that
 * direction: no-op, leaving the warp free to take over instead */
static void s_test_edge_check_pan_unavailable_is_noop(void)
{
    s_reset();
    s_stub_pan_available = false;

    drag_pan_edge_check(0, 500);

    TAP_OK(!s_drag.is_pan_pending,
            "no room to pan that direction: nothing armed");
}


/* Pointer at the exact left edge (x == 0) arms a west pan */
static void s_test_edge_check_left_edge_arms_west(void)
{
    s_reset();

    drag_pan_edge_check(0, 500);

    TAP_OK(s_drag.is_pan_pending, "x=0 arms a pending pan");
    TAP_EQ_INT((long) s_drag.pan_direction, (long) COMPASS_WEST,
            "direction is west");
}


/* Pointer one pixel inside the left edge (x == 1) does not arm a
 * pan: the boundary is inclusive of 0, not 1 */
static void s_test_edge_check_one_past_left_edge_is_noop(void)
{
    s_reset();

    drag_pan_edge_check(1, 500);

    TAP_OK(!s_drag.is_pan_pending,
            "x=1 is one pixel short of the left edge: no pan armed");
}


/* Pointer at the exact right edge (x == screen_w - 1) arms an east
 * pan */
static void s_test_edge_check_right_edge_arms_east(void)
{
    s_reset();

    drag_pan_edge_check(1919, 500);

    TAP_OK(s_drag.is_pan_pending, "x=screen_w-1 arms a pending pan");
    TAP_EQ_INT((long) s_drag.pan_direction, (long) COMPASS_EAST,
            "direction is east");
}


/* Pointer at the exact top edge (y == 0) arms a north pan */
static void s_test_edge_check_top_edge_arms_north(void)
{
    s_reset();

    drag_pan_edge_check(960, 0);

    TAP_OK(s_drag.is_pan_pending, "y=0 arms a pending pan");
    TAP_EQ_INT((long) s_drag.pan_direction, (long) COMPASS_NORTH,
            "direction is north");
}


/* Pointer at the exact bottom edge (y == screen_h - 1) arms a south
 * pan */
static void s_test_edge_check_bottom_edge_arms_south(void)
{
    s_reset();

    drag_pan_edge_check(960, 1079);

    TAP_OK(s_drag.is_pan_pending, "y=screen_h-1 arms a pending pan");
    TAP_EQ_INT((long) s_drag.pan_direction, (long) COMPASS_SOUTH,
            "direction is south");
}


/* A corner holds two edges at once: the horizontal direction always
 * wins over the vertical one */
static void s_test_edge_check_corner_prefers_horizontal(void)
{
    s_reset();

    drag_pan_edge_check(0, 0);

    TAP_EQ_INT((long) s_drag.pan_direction, (long) COMPASS_WEST,
            "top-left corner: west (horizontal) wins over north");

    s_reset();

    drag_pan_edge_check(1919, 1079);

    TAP_EQ_INT((long) s_drag.pan_direction, (long) COMPASS_EAST,
            "bottom-right corner: east (horizontal) wins over south");
}


/* Holding the same edge across two calls keeps the original countdown
 * running rather than restarting it */
static void s_test_edge_check_same_edge_keeps_countdown(void)
{
    struct timespec first_due;

    s_reset();

    drag_pan_edge_check(0, 500);
    first_due = s_drag.pan_due;

    drag_pan_edge_check(0, 501);

    TAP_OK(s_drag.pan_due.tv_sec == first_due.tv_sec &&
            s_drag.pan_due.tv_nsec == first_due.tv_nsec,
            "the same edge held again does not restart the countdown");
}


/* Switching to a different edge mid-hold re-arms a fresh countdown
 * for the new direction */
static void s_test_edge_check_different_edge_rearms(void)
{
    s_reset();

    drag_pan_edge_check(0, 500);
    TAP_EQ_INT((long) s_drag.pan_direction, (long) COMPASS_WEST,
            "first touch arms west");

    drag_pan_edge_check(960, 0);
    TAP_EQ_INT((long) s_drag.pan_direction, (long) COMPASS_NORTH,
            "moving to the top edge re-arms as north instead");
}


/* Leaving every edge after being armed clears the pending pan again */
static void s_test_edge_check_leaving_edge_clears_pending(void)
{
    s_reset();

    drag_pan_edge_check(0, 500);
    TAP_OK(s_drag.is_pan_pending, "armed at the left edge");

    drag_pan_edge_check(960, 540);
    TAP_OK(!s_drag.is_pan_pending,
            "moving back to the middle of the screen clears it");
}


/* drag_pan_ms_remaining returns -1 whenever nothing is pending */
static void s_test_ms_remaining_returns_negative_when_idle(void)
{
    s_reset();
    s_drag.is_pan_pending = false;

    TAP_EQ_INT(drag_pan_ms_remaining(), -1,
            "no pending pan: -1, never a stale countdown");
}


/* drag_pan_ms_remaining reflects a countdown armed by the edge check
 * itself: strictly positive and no larger than the configured delay */
static void s_test_ms_remaining_positive_right_after_arming(void)
{
    int remaining;

    s_reset();

    drag_pan_edge_check(0, 500);
    remaining = drag_pan_ms_remaining();

    TAP_OK(remaining > 0 && remaining <= WM_VIEWPORT_PAN_DELAY_MS,
            "freshly armed countdown is within (0, configured delay]");
}


/* drag_pan_ms_remaining reads 0, never negative, once due has already
 * passed */
static void s_test_ms_remaining_zero_once_due_has_passed(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 10;

    TAP_EQ_INT(drag_pan_ms_remaining(), 0,
            "a due time already 10 seconds in the past reads back as"
            " exactly 0, never negative");
}


/* drag_pan_tick: a NULL connection is a no-op, touching nothing */
static void s_test_tick_null_connection_is_noop(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;

    drag_pan_tick(NULL);

    TAP_OK(s_drag.is_pan_pending,
            "a NULL connection leaves the pending flag untouched");
    TAP_EQ_INT(s_call_pan_west, 0,
            "and never reaches the actual pan machinery");
}


/* drag_pan_tick: nothing pending is a no-op */
static void s_test_tick_nothing_pending_is_noop(void)
{
    s_reset();
    s_drag.is_pan_pending = false;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "nothing pending: the tick never touches the pan"
            " machinery");
}


/* drag_pan_tick: countdown still running (ms_remaining > 0) is a
 * no-op, and leaves the pending flag set for the next tick */
static void s_test_tick_countdown_not_due_is_noop(void)
{
    s_reset();

    drag_pan_edge_check(0, 500);
    TAP_OK(s_drag.is_pan_pending, "armed by the edge check");

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_OK(s_drag.is_pan_pending,
            "countdown not yet due: still pending after the tick");
    TAP_EQ_INT(s_call_pan_west, 0,
            "and the pan itself never ran");
}


/* drag_pan_tick: due, but no client under drag any more: clears
 * pending and stops there */
static void s_test_tick_due_no_client_stops_early(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_drag.client = NULL;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_OK(!s_drag.is_pan_pending,
            "due tick with no client: pending flag is cleared anyway");
    TAP_EQ_INT(s_call_pan_west, 0,
            "but the pan itself never ran");
}


/* drag_pan_tick: due, but the operation is a resize, not a move:
 * stops before touching the pan machinery */
static void s_test_tick_due_wrong_operation_stops_early(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_RESIZING;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "a resize in progress never triggers the pan");
}


/* drag_pan_tick: due, moving, but drag_window names neither
 * XCB_WINDOW_NONE nor the dragged client's own icon window: stale
 * state, stops before the pan */
static void s_test_tick_due_stale_drag_window_stops_early(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_drag.drag_window = 99u;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "drag_window matching neither XCB_WINDOW_NONE nor the"
            " client's own icon window never triggers the pan");
}


/* drag_pan_tick: due, moving, but pan_on_edge_drag was disabled after
 * the countdown was already armed: stops before the pan */
static void s_test_tick_due_config_disabled_stops_early(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_stub_stage->config->base.viewport.pan_on_edge_drag = false;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "pan_on_edge_drag turned off since arming: the pan itself"
            " never runs");
}


/* drag_pan_tick: due, moving, but the viewport no longer has room to
 * pan that direction since arming: stops before the pan */
static void s_test_tick_due_pan_unavailable_stops_early(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_stub_pan_available = false;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "no more room to pan since arming: the pan itself never"
            " runs");
}


/* drag_pan_tick: due, moving, stage itself no longer resolvable:
 * stops before the pan */
static void s_test_tick_due_no_stage_stops_early(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_stub_stage = NULL;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "the stage lookup failing stops the tick before the"
            " pan itself");
}


/* drag_pan_tick: due, moving, stage resolves but has no attached
 * XCB screen: stops before the pan, since the pointer warp below now
 * needs 'stage->screen->root' */
static void s_test_tick_due_no_screen_stops_early(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_stub_stage->screen = NULL;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "a stage with no attached XCB screen stops the tick"
            " before the pan itself");
}


/* drag_pan_tick: due, moving, stage resolves but the current
 * desktop does not: stops before the pan */
static void s_test_tick_due_no_desktop_stops_early(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_stub_desktop = NULL;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "no resolvable current desktop: the pan itself never"
            " runs");
}


/* drag_pan_tick: a due, solid, non-icon, non-sticky west pan calls
 * the matching command exactly once, leaves client_start untouched,
 * shifts client_cur by the desktop's own width, warps the real
 * pointer by that same delta (clamped to stay on the physical
 * screen) and keeps 'last_root_x'/'last_root_y' in step with it,
 * never touches the icon window or outline, and re-arms a fresh,
 * shorter-interval countdown rather than clearing pending entirely */
static void s_test_tick_due_solid_west_pan_shifts_state(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_drag.client_start.pos.x = 100;
    s_drag.client_start.pos.y = 200;
    s_drag.client_cur.pos.x = 110;
    s_drag.client_cur.pos.y = 210;
    s_drag.last_root_x = 0;
    s_drag.last_root_y = 500;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 1,
            "west direction dispatches to the matching pan command"
            " exactly once");
    TAP_EQ_INT(s_call_pan_north, 0, "and none of its siblings");
    TAP_EQ_INT(s_call_pan_south, 0, "none of its siblings");
    TAP_EQ_INT(s_call_pan_east, 0, "none of its siblings");
    TAP_EQ_INT(s_drag.client_start.pos.x, 100,
            "client_start.pos.x is left untouched by a west pan, since"
            " the pointer warp below is what now keeps the drag"
            " invariant correct, not a shift of the start position"
            " itself");
    TAP_EQ_INT(s_drag.client_start.pos.y, 200,
            "client_start.pos.y is likewise untouched");
    TAP_EQ_INT(s_drag.client_cur.pos.x, 2029,
            "client_cur.pos.x shifts by the pointer's own effective"
            " delta (1919), clamped to the physical screen just like"
            " the pointer warp below, not the raw, unclamped +1920"
            " viewport step");
    TAP_EQ_INT(s_drag.client_cur.pos.y, 210,
            "client_cur.pos.y is likewise untouched");
    TAP_EQ_INT(s_call_configure_window, 1,
            "a solid drag repositions the real window exactly once"
            " via a raw configure, bypassing the generic per-client"
            " viewport walk, which now excludes the dragged client"
            " outright");
    TAP_OK(s_configure_window_last_window != s_drag.client->icon_window,
            "specifically the client's own real window, never its icon"
            " window");
    TAP_EQ_INT((int) s_configure_window_last_mask,
            (int) (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y),
            "only X and Y are configured, no size change");
    TAP_EQ_INT(s_configure_window_last_x, 2029,
            "moved to the same shifted X the state tracking now"
            " reflects");
    TAP_EQ_INT(s_configure_window_last_y, 210,
            "moved to the same untouched Y the state tracking now"
            " reflects");
    TAP_EQ_INT(s_drag.client->layout.geometry.cur.pos.x, 2029,
            "the client's own 'cur.pos.x' is kept in sync with the"
            " raw configure, matching what every other part of the"
            " window manager still relies on");
    TAP_EQ_INT(s_drag.client->layout.geometry.cur.pos.y, 210,
            "likewise for 'cur.pos.y'");
    TAP_EQ_INT(s_call_drag_outline_move, 0,
            "a solid drag never goes through the outline path"
            " either");
    TAP_EQ_INT(s_call_xcb_warp_pointer, 1,
            "a non-sticky dragged client also warps the real pointer"
            " exactly once, keeping it glued to the window");
    TAP_EQ_INT((int) s_warp_pointer_last_x, 1919,
            "the pointer's own +1920 target clamps to the last valid"
            " screen column (1919), the same as the dragged window"
            " would if it, too, could not move past the physical"
            " screen");
    TAP_EQ_INT((int) s_warp_pointer_last_y, 500,
            "a purely horizontal pan leaves the pointer's Y untouched");
    TAP_EQ_INT((int) s_drag.last_root_x, 1919,
            "'last_root_x' is kept in step with the warp just issued");
    TAP_EQ_INT((int) s_drag.last_root_y, 500,
            "'last_root_y' likewise");
    TAP_OK(s_drag.is_pan_pending,
            "a successful pan re-arms rather than clearing pending"
            " entirely");
    TAP_OK(drag_pan_ms_remaining() > 0 &&
            drag_pan_ms_remaining() <= WM_VIEWPORT_PAN_REPEAT_MS,
            "the re-armed countdown uses the shorter repeat interval,"
            " not the initial delay");
}


/* An east pan shifts client_cur.pos by the negative of the desktop's
 * width, a north pan by the positive height, and a south pan by the
 * negative height: only the axis matching the direction's own
 * dimension moves, client_start.pos stays untouched throughout (the
 * pointer warp is what keeps the drag invariant now, not a shift of
 * the start position), and the pointer itself warps by that same
 * per-axis delta, clamped to the physical screen */
static void s_test_tick_due_other_directions_shift_correctly(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_EAST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_drag.client_start.pos.x = 100;
    s_drag.client_cur.pos.x = 110;
    s_drag.last_root_x = 1919;
    s_drag.last_root_y = 500;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_east, 1, "east dispatches to its own command");
    TAP_EQ_INT(s_drag.client_start.pos.x, 100,
            "an east pan leaves client_start.pos.x untouched");
    TAP_EQ_INT(s_drag.client_cur.pos.x, -1809,
            "client_cur.pos.x shifts by the pointer's own clamped"
            " effective delta (-1919), not the raw -1920 viewport"
            " step, since the pointer itself was already one pixel"
            " short of a full screen width away from its own clamp"
            " boundary");
    TAP_EQ_INT((int) s_warp_pointer_last_x, 0,
            "the pointer's own -1920 target clamps to column 0, the"
            " first valid one");

    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_NORTH;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_drag.client_start.pos.y = 100;
    s_drag.client_cur.pos.y = 110;
    s_drag.last_root_x = 500;
    s_drag.last_root_y = 0;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_north, 1, "north dispatches to its own command");
    TAP_EQ_INT(s_drag.client_start.pos.y, 100,
            "a north pan leaves client_start.pos.y untouched");
    TAP_EQ_INT(s_drag.client_cur.pos.y, 1189,
            "client_cur.pos.y shifts by the pointer's own clamped"
            " effective delta (1079), not the raw +1080 viewport"
            " step");
    TAP_EQ_INT((int) s_warp_pointer_last_y, 1079,
            "the pointer's own +1080 target clamps to row 1079, the"
            " last valid one");

    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_SOUTH;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_drag.client_start.pos.y = 100;
    s_drag.client_cur.pos.y = 110;
    s_drag.last_root_x = 500;
    s_drag.last_root_y = 1079;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_south, 1, "south dispatches to its own command");
    TAP_EQ_INT(s_drag.client_start.pos.y, 100,
            "a south pan leaves client_start.pos.y untouched");
    TAP_EQ_INT(s_drag.client_cur.pos.y, -969,
            "client_cur.pos.y shifts by the pointer's own clamped"
            " effective delta (-1079), not the raw -1080 viewport"
            " step");
    TAP_EQ_INT((int) s_warp_pointer_last_y, 0,
            "the pointer's own -1080 target clamps to row 0, the"
            " first valid one");
}


/* When the viewport's origin is not aligned to a whole screen step,
 * e.g. left over from a background-drag pan
 * ('input/mouse/drag/background.c') or an EWMH
 * '_NET_DESKTOP_VIEWPORT' request, a west pan can be clamped by
 * 's_viewport_apply_origin' (cmds/stage.c) to less than a full
 * screen width. The dragged client must follow that same, real,
 * possibly-partial delta rather than a blindly assumed full step,
 * otherwise it drifts away from the pointer and every other window
 * on the desktop */
static void s_test_tick_due_clamped_pan_uses_real_delta(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_drag.client_start.pos.x = 100;
    s_drag.client_cur.pos.x = 110;
    s_drag.last_root_x = 200;
    s_drag.last_root_y = 500;

    s_stub_desktop->viewport_origin.x = 500;
    s_stub_origin_override_active = true;
    s_stub_origin_override.x = 0;
    s_stub_origin_override.y = 0;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 1, "west still dispatches exactly once");
    TAP_EQ_INT(s_drag.client_start.pos.x, 100,
            "client_start.pos.x stays untouched regardless of the"
            " real delta the pan turned out to use");
    TAP_EQ_INT(s_drag.client_cur.pos.x, 610,
            "the client's current position tracks the real 500-pixel"
            " delta the viewport actually moved, not a blindly"
            " assumed full screen width");
    TAP_EQ_INT((int) s_warp_pointer_last_x, 700,
            "the pointer follows that same real, possibly-partial"
            " delta (200 + 500), not a blindly assumed full screen"
            " width either");
}


/* A sticky dragged client is left entirely untouched by the
 * drag-state fixup, since it never visually moved on screen in the
 * first place, so the pointer must not be warped either: it never
 * moved out from under the client to begin with */
static void s_test_tick_due_sticky_client_skips_fixup(void)
{
    s_reset();
    s_drag.client->properties.flags |= CLIENT_FLAG_STICKY;
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_drag.client_start.pos.x = 100;
    s_drag.client_cur.pos.x = 110;
    s_drag.last_root_x = 300;
    s_drag.last_root_y = 400;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 1,
            "the viewport pan command itself still runs for a sticky"
            " client's own desktop");
    TAP_EQ_INT(s_drag.client_start.pos.x, 100,
            "but the sticky dragged client's own cached start position"
            " is left untouched");
    TAP_EQ_INT(s_drag.client_cur.pos.x, 110,
            "and likewise its current position");
    TAP_EQ_INT(s_call_xcb_warp_pointer, 0,
            "and the pointer itself is never warped for a sticky"
            " client, or it would visually disconnect from a window"
            " that correctly stayed put");
    TAP_EQ_INT((int) s_drag.last_root_x, 300,
            "'last_root_x' is likewise left untouched");
    TAP_EQ_INT((int) s_drag.last_root_y, 400,
            "'last_root_y' likewise");
}


/* An icon drag (drag_window matches the client's own icon window)
 * repositions the icon window directly via xcb_configure_window,
 * rather than relying on the generic per-client viewport walk that
 * only ever touches a client's real window */
static void s_test_tick_due_icon_drag_configures_icon_window(void)
{
    s_reset();
    s_drag.drag_window = s_drag.client->icon_window;
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;
    s_drag.client_cur.pos.x = 20;
    s_drag.client_cur.pos.y = 30;
    s_drag.last_root_x = 0;
    s_drag.last_root_y = 30;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_configure_window, 1,
            "an icon drag repositions the icon window exactly once");
    TAP_OK(s_configure_window_last_window == s_drag.client->icon_window,
            "specifically the client's own icon window");
    TAP_EQ_INT((int) s_configure_window_last_mask,
            (int) (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y),
            "only X and Y are configured, no size change");
    TAP_EQ_INT(s_configure_window_last_x, 1939,
            "moved to the icon's current position plus the pointer's"
            " own clamped effective delta on X (20 + 1919), not the"
            " raw, unclamped +1920 viewport step");
    TAP_EQ_INT(s_call_drag_outline_move, 0,
            "an icon drag never goes through the outline path");
    TAP_EQ_INT(s_call_xcb_warp_pointer, 1,
            "an icon drag also warps the pointer, exactly like a"
            " plain window drag does");
    TAP_EQ_INT((int) s_warp_pointer_last_x, 1919,
            "clamped the same way a plain window drag's own pointer"
            " warp is");
}


/* A non-solid (outline) drag calls drag_outline_move directly instead
 * of relying on the generic per-client viewport walk, which only ever
 * touches a client's own real window, never the separate outline
 * windows */
static void s_test_tick_due_outline_drag_moves_outline(void)
{
    s_reset();
    s_drag.is_solid_drag = false;
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_drag_outline_move, 1,
            "a non-solid drag moves its outline exactly once");
    TAP_EQ_INT(s_call_configure_window, 0,
            "and never touches the icon window, this being a plain"
            " window drag, not an icon drag");
}


/* A solid, non-icon window drag never shows a geometry overlay
 * regardless of config, matching the same guard on the icon/solid
 * paths: 'show_geom' is only ever read from the client's own config,
 * which starts NULL here */
static void s_test_tick_due_no_config_never_shows_overlay(void)
{
    s_reset();
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_drag_overlay_show, 0,
            "no client config attached at all: the geometry overlay"
            " never shows, regardless of any window/icon show_geom"
            " setting");
}


/* When the dragged client's own config does ask for the geometry
 * overlay, a due pan keeps it shown at the freshly panned position */
static void s_test_tick_due_show_geom_config_shows_overlay(void)
{
    static config_td client_config;

    memset(&client_config, 0, sizeof(client_config));
    client_config.base.windows.show_geom = true;

    s_reset();
    s_drag.client->config = &client_config;
    s_drag.is_pan_pending = true;
    s_drag.pan_direction = COMPASS_WEST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.pan_due);
    s_drag.pan_due.tv_sec -= 1;

    drag_pan_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_drag_overlay_show, 1,
            "windows.show_geom true on the client's own config: the"
            " overlay is kept current across the pan");
}


int main(void)
{
    TAP_PLAN(95);

    s_test_edge_check_no_client_clears_pending();
    s_test_edge_check_no_stage_is_noop();
    s_test_edge_check_disabled_in_config_is_noop();
    s_test_edge_check_middle_of_screen_is_noop();
    s_test_edge_check_pan_unavailable_is_noop();
    s_test_edge_check_left_edge_arms_west();
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
    s_test_tick_due_config_disabled_stops_early();
    s_test_tick_due_pan_unavailable_stops_early();
    s_test_tick_due_no_stage_stops_early();
    s_test_tick_due_no_screen_stops_early();
    s_test_tick_due_no_desktop_stops_early();
    s_test_tick_due_solid_west_pan_shifts_state();
    s_test_tick_due_other_directions_shift_correctly();
    s_test_tick_due_clamped_pan_uses_real_delta();
    s_test_tick_due_sticky_client_skips_fixup();
    s_test_tick_due_icon_drag_configures_icon_window();
    s_test_tick_due_outline_drag_moves_outline();
    s_test_tick_due_no_config_never_shows_overlay();
    s_test_tick_due_show_geom_config_shows_overlay();

    return TAP_DONE();
}
