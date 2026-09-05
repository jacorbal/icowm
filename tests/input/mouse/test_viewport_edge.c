/**
 * @file tests/input/mouse/test_viewport_edge.c
 *
 * @brief Test battery for the edge-triggered viewport pan while the
 *        pointer merely rests at a screen edge, no drag in progress
 *
 * 'lookup_surface_for_root' and the four
 * 'scmd_surface_viewport_pan_*' commands are heavy calls into the rest
 * of the running window manager that this file's real target, the
 * edge-detection and countdown bookkeeping in
 * 'mouse_viewport_edge_check' and 'mouse_viewport_edge_ms_remaining',
 * and the guards at the top of 'mouse_viewport_edge_tick', never needs
 * to actually exercise, so every one of them is a controlled or
 * recording stand-in below instead.  'xcb_query_pointer' and
 * 'xcb_query_pointer_reply' are real XCB protocol calls that need a
 * live X server to answer for real, so they too are replaced by
 * controlled stand-ins that hand back a fabricated reply built by each
 * scenario, the same pattern 'tests/input/mouse/test_hover.c' already
 * uses for the same pair of functions.  'clock_gettime',
 * 'clock_add_ms', and 'clock_ms_until' (utils/time/clock.c) are
 * linked real, since the countdown math itself is exactly what is
 * under test; reaching a due countdown from outside this file is done
 * with an actual 'nanosleep' rather than reaching into the module's
 * own state, since, unlike 'drag/warp.c', it keeps no header-exposed
 * struct a test could poke directly, the same choice
 * 'tests/input/mouse/test_hover.c' makes for its own analogous poll
 * countdown.
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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <defs/desktop.h>
#include <surface.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/viewport_edge.h>


/** Non-null opaque handle standing in for a real list_td of surfaces,
 *  which this file never actually builds, since the type is opaque
 *  outside its own translation unit; 'lookup_surface_for_root' below
 *  ignores it, only ever checking it is non-NULL */
static int s_fake_surfaces_storage;
static list_td *const s_fake_surfaces =
        (list_td *) &s_fake_surfaces_storage;

/** Surface @a lookup_surface_for_root hands back, @c NULL to make the
 *  lookup itself fail */
static surface_td *s_stub_surface;

/** Whether @a drag_is_active reports a drag in progress */
static bool s_drag_active;

/** Count of every pan command this file only records rather than
 *  acts on, reset by @a s_reset before each scenario */
static int s_call_pan_north;
static int s_call_pan_south;
static int s_call_pan_east;
static int s_call_pan_west;

/** Whether @a xcb_query_pointer_reply should hand back a reply at
 *  all, and, when it does, what @c same_screen/coordinates it
 *  carries */
static bool s_reply_present;
static uint8_t s_reply_same_screen;
static int16_t s_reply_root_x;
static int16_t s_reply_root_y;


/**
 * @brief Link-only stand-in for @a lookup_surface_for_root
 *
 * @note Complexity: @e O(1)
 */
surface_td *lookup_surface_for_root(list_td *surfaces, xcb_window_t root)
{
    (void) surfaces;
    (void) root;
    return s_stub_surface;
}


/**
 * @brief Link-only stand-in for @a drag_is_active
 *
 * @note Complexity: @e O(1)
 */
bool drag_is_active(void)
{
    return s_drag_active;
}


/**
 * @brief Recording stand-in for @a scmd_surface_viewport_pan_north
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_viewport_pan_north(surface_td *surface)
{
    (void) surface;
    s_call_pan_north++;
}


/**
 * @brief Recording stand-in for @a scmd_surface_viewport_pan_south
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_viewport_pan_south(surface_td *surface)
{
    (void) surface;
    s_call_pan_south++;
}


/**
 * @brief Recording stand-in for @a scmd_surface_viewport_pan_east
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_viewport_pan_east(surface_td *surface)
{
    (void) surface;
    s_call_pan_east++;
}


/**
 * @brief Recording stand-in for @a scmd_surface_viewport_pan_west
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_viewport_pan_west(surface_td *surface)
{
    (void) surface;
    s_call_pan_west++;
}


/**
 * @brief Link-only stand-in for @a xcb_query_pointer
 *
 * The returned cookie is never inspected by @c viewport_edge.c beyond
 * handing it straight to @a xcb_query_pointer_reply, so a zeroed one
 * is enough
 *
 * @note Complexity: @e O(1)
 */
xcb_query_pointer_cookie_t xcb_query_pointer(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_query_pointer_cookie_t cookie;

    (void) connection;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Controlled stand-in for @a xcb_query_pointer_reply
 *
 * Hands back a heap-allocated reply built from @c s_reply_* when
 * @c s_reply_present is set (@c viewport_edge.c frees it, so a static
 * or stack one would be an invalid free, the same reasoning
 * 'tests/input/mouse/test_hover.c' documents for its own identical
 * stand-in), or @c NULL otherwise
 *
 * @note Complexity: @e O(1)
 */
xcb_query_pointer_reply_t
    *xcb_query_pointer_reply(xcb_connection_t *connection,
            xcb_query_pointer_cookie_t cookie,
            xcb_generic_error_t **error)
{
    xcb_query_pointer_reply_t *reply;

    (void) connection;
    (void) cookie;
    if (error != NULL) {
        *error = NULL;
    }

    if (!s_reply_present) {
        return NULL;
    }

    reply = malloc(sizeof(*reply));
    memset(reply, 0, sizeof(*reply));
    reply->same_screen = s_reply_same_screen;
    reply->root_x = s_reply_root_x;
    reply->root_y = s_reply_root_y;
    return reply;
}


/**
 * @brief Reset every recording stand-in and rebuild a fresh, active,
 *        pannable surface before each scenario
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    static config_td config;
    static surface_td surface;
    static xcb_screen_t screen;

    memset(&config, 0, sizeof(config));
    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));

    config.desktops.pan_on_edge_hover = true;
    config.base.screens[0].viewport.columns = 3u;
    config.base.screens[0].viewport.rows = 1u;

    screen.root = 1u;

    surface.config = &config;
    surface.screen = &screen;
    surface.id = 0u;
    surface.properties.dim.w = 1920u;
    surface.properties.dim.h = 1080u;

    s_stub_surface = &surface;
    s_drag_active = false;

    s_call_pan_north = 0;
    s_call_pan_south = 0;
    s_call_pan_east = 0;
    s_call_pan_west = 0;

    s_reply_present = false;
    s_reply_same_screen = 0;
    s_reply_root_x = 0;
    s_reply_root_y = 0;
}


/* No surface resolves at all: clears any pending pan */
static void s_test_check_no_surface_is_noop(void)
{
    s_reset();
    s_stub_surface = NULL;

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);

    TAP_EQ_INT(mouse_viewport_edge_ms_remaining(), -1,
            "surface lookup failing clears any pending pan");
}


/* pan_on_edge_hover is off in configuration: no-op even at an edge */
static void s_test_check_disabled_in_config_is_noop(void)
{
    s_reset();
    s_stub_surface->config->desktops.pan_on_edge_hover = false;

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);

    TAP_EQ_INT(mouse_viewport_edge_ms_remaining(), -1,
            "pan_on_edge_hover false: edge touch never arms a pan");
}


/* Only one viewport column and row: no-op even at an edge, there
 * being nowhere else to pan to */
static void s_test_check_single_column_is_noop(void)
{
    s_reset();
    s_stub_surface->config->base.screens[0].viewport.columns = 1u;
    s_stub_surface->config->base.screens[0].viewport.rows = 1u;

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);

    TAP_EQ_INT(mouse_viewport_edge_ms_remaining(), -1,
            "a one-by-one viewport never arms a pan");
}


/* Pointer well inside the screen on both axes: no-op */
static void s_test_check_middle_of_screen_is_noop(void)
{
    s_reset();

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 960, 540);

    TAP_EQ_INT(mouse_viewport_edge_ms_remaining(), -1,
            "pointer away from every edge never arms a pan");
}


/* Pointer at the exact left edge (x == 0) arms a west pan */
static void s_test_check_left_edge_arms_west(void)
{
    s_reset();

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);

    TAP_OK(mouse_viewport_edge_ms_remaining() >= 0,
            "x=0 arms a pending pan");
}


/* Pointer one pixel inside the left edge (x == 1) does not arm a
 * pan: the boundary is inclusive of 0, not 1 */
static void s_test_check_one_past_left_edge_is_noop(void)
{
    s_reset();

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 1, 500);

    TAP_EQ_INT(mouse_viewport_edge_ms_remaining(), -1,
            "x=1 is one pixel short of the left edge: no pan armed");
}


/* Pointer at the exact right edge (x == screen_w - 1) arms an east
 * pan */
static void s_test_check_right_edge_arms_east(void)
{
    s_reset();

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 1919, 500);

    TAP_OK(mouse_viewport_edge_ms_remaining() >= 0,
            "x=screen_w-1 arms a pending pan");
}


/* Leaving every edge after being armed clears the pending pan again */
static void s_test_check_leaving_edge_clears_pending(void)
{
    s_reset();

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);
    TAP_OK(mouse_viewport_edge_ms_remaining() >= 0, "armed at the left edge");

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 960, 540);
    TAP_EQ_INT(mouse_viewport_edge_ms_remaining(), -1,
            "moving back to the middle of the screen clears it");
}


/* Holding the same edge across two calls keeps the original countdown
 * running rather than restarting it */
static void s_test_check_same_edge_keeps_countdown(void)
{
    int first_remaining;
    int second_remaining;

    s_reset();

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);
    first_remaining = mouse_viewport_edge_ms_remaining();

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 501);
    second_remaining = mouse_viewport_edge_ms_remaining();

    TAP_OK(second_remaining <= first_remaining,
            "the same edge held again does not restart the countdown");
}


/* mouse_viewport_edge_ms_remaining returns -1 whenever nothing is
 * pending */
static void s_test_ms_remaining_returns_negative_when_idle(void)
{
    s_reset();

    TAP_EQ_INT(mouse_viewport_edge_ms_remaining(), -1,
            "no pending pan: -1, never a stale countdown");
}


/* mouse_viewport_edge_ms_remaining reflects a countdown armed by the
 * edge check itself: strictly within (0, configured delay] */
static void s_test_ms_remaining_positive_right_after_arming(void)
{
    int remaining;

    s_reset();

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);
    remaining = mouse_viewport_edge_ms_remaining();

    TAP_OK(remaining >= 0 && remaining <= WM_VIEWPORT_PAN_DELAY_MS,
            "freshly armed countdown is within [0, configured delay]");
}


/* mouse_viewport_edge_tick: a NULL connection is a no-op, touching
 * nothing */
static void s_test_tick_null_connection_is_noop(void)
{
    s_reset();
    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);

    mouse_viewport_edge_tick(NULL);

    TAP_EQ_INT(s_call_pan_west, 0,
            "a NULL connection never reaches the pan machinery");
}


/* mouse_viewport_edge_tick: nothing pending is a no-op */
static void s_test_tick_nothing_pending_is_noop(void)
{
    s_reset();

    mouse_viewport_edge_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "nothing pending: the tick never touches the pan"
            " machinery");
}


/* mouse_viewport_edge_tick: countdown still running is a no-op, and
 * leaves the pending flag set for the next tick */
static void s_test_tick_countdown_not_due_is_noop(void)
{
    s_reset();

    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);

    mouse_viewport_edge_tick((xcb_connection_t *) (void *) 1);

    TAP_OK(mouse_viewport_edge_ms_remaining() >= 0,
            "countdown not yet due: still pending after the tick");
    TAP_EQ_INT(s_call_pan_west, 0,
            "and the pan itself never ran");
}


/**
 * @brief Sleep past the given delay, so the next tick sees a due
 *        countdown
 *
 * @param ms Milliseconds to sleep past
 *
 * @note Complexity: @e O(1)
 */
static void s_sleep_past(unsigned int ms)
{
    struct timespec until_due;

    until_due.tv_sec = (time_t) (ms / 1000u);
    until_due.tv_nsec = (long) (ms % 1000u) * 1000L * 1000L +
        5L * 1000L * 1000L;
    nanosleep(&until_due, NULL);
}


/* mouse_viewport_edge_tick: due, but a drag started in the meantime:
 * defers entirely, never panning nor re-arming */
static void s_test_tick_due_drag_active_stops_early(void)
{
    s_reset();
    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);
    s_sleep_past(WM_VIEWPORT_PAN_DELAY_MS);
    s_drag_active = true;

    mouse_viewport_edge_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "a drag already in progress: the pan never runs");
    TAP_EQ_INT(mouse_viewport_edge_ms_remaining(), -1,
            "and the pending pan is cleared, never left stale");
}


/* mouse_viewport_edge_tick: due, but the pointer already moved away
 * from the edge by the time of the live re-check: no pan */
static void s_test_tick_due_pointer_moved_away_stops_early(void)
{
    s_reset();
    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);
    s_sleep_past(WM_VIEWPORT_PAN_DELAY_MS);

    s_reply_present = true;
    s_reply_same_screen = 1u;
    s_reply_root_x = 960;
    s_reply_root_y = 540;

    mouse_viewport_edge_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "a live re-check that the pointer already left the edge:"
            " the pan never runs");
}


/* mouse_viewport_edge_tick: due, no reply at all (query failed): no
 * pan, same as the pointer having moved away */
static void s_test_tick_due_no_reply_stops_early(void)
{
    s_reset();
    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);
    s_sleep_past(WM_VIEWPORT_PAN_DELAY_MS);

    s_reply_present = false;

    mouse_viewport_edge_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 0,
            "a failed pointer query never runs the pan either");
}


/* mouse_viewport_edge_tick: due, and the live re-check confirms the
 * pointer is still at the same edge: pans once and re-arms */
static void s_test_tick_due_still_at_edge_pans_and_rearms(void)
{
    int remaining;

    s_reset();
    mouse_viewport_edge_check(s_fake_surfaces, 1u, 0, 500);
    s_sleep_past(WM_VIEWPORT_PAN_DELAY_MS);

    s_reply_present = true;
    s_reply_same_screen = 1u;
    s_reply_root_x = 0;
    s_reply_root_y = 500;

    mouse_viewport_edge_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_pan_west, 1,
            "still at the west edge: the pan runs exactly once");

    remaining = mouse_viewport_edge_ms_remaining();
    TAP_OK(remaining >= 0 && remaining <= WM_VIEWPORT_PAN_REPEAT_MS,
            "and re-arms at the shorter repeat interval, not the"
            " original delay");
}


int main(void)
{
    TAP_PLAN(22);

    /* Run first, before any other test arms a pending countdown in
     * the module's own static state, since this module exposes no
     * reset hook to clear it back to idle afterward (the same
     * ordering constraint 'tests/input/mouse/test_hover.c' applies
     * to its own "none" case) */
    s_test_ms_remaining_returns_negative_when_idle();

    s_test_check_no_surface_is_noop();
    s_test_check_disabled_in_config_is_noop();
    s_test_check_single_column_is_noop();
    s_test_check_middle_of_screen_is_noop();
    s_test_check_left_edge_arms_west();
    s_test_check_one_past_left_edge_is_noop();
    s_test_check_right_edge_arms_east();
    s_test_check_leaving_edge_clears_pending();
    s_test_check_same_edge_keeps_countdown();
    s_test_ms_remaining_positive_right_after_arming();
    s_test_tick_null_connection_is_noop();
    s_test_tick_nothing_pending_is_noop();
    s_test_tick_countdown_not_due_is_noop();
    s_test_tick_due_drag_active_stops_early();
    s_test_tick_due_pointer_moved_away_stops_early();
    s_test_tick_due_no_reply_stops_early();
    s_test_tick_due_still_at_edge_pans_and_rearms();

    return TAP_DONE();
}
