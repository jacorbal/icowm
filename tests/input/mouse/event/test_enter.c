/**
 * @file tests/input/mouse/event/test_enter.c
 *
 * @brief Test battery for enter-notify handling and delayed sloppy
 *        focus (input/mouse/event/enter.c)
 *
 * mouse_handle_enter orchestrates several genuinely external
 * collaborators (resize-cursor re-evaluation, hover tracking, the
 * cycle-menu-open guard, the focus policy check, client/surface
 * lookup, and focus_apply itself), each of which is a full subsystem
 * covered by its own test elsewhere; only this file's own dispatch and
 * delayed-focus bookkeeping (s_enter_focus_active, s_pending_window,
 * s_pending_due) are under test here, so every one of those
 * collaborators is a stand-in.  clock_gettime, clock_add_ms and
 * clock_ms_until (utils/time/clock.c) are real, side-effect-free
 * CLOCK_MONOTONIC arithmetic with no X, disk, or process dependency of
 * their own, so the real clock.c is linked instead of stood in, the
 * same way test_apply.c links match.c for a genuinely leaf
 * collaborator.
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
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Menu includes */
#include <menu/cycle.h>

/* Policy includes */
#include <policy/focus.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/cursor.h>
#include <input/mouse/event.h>
#include <input/mouse/hover.h>


/**
 * @brief Link-only stand-in for @a logger_msg
 *
 * mouse_handle_enter's LOGGER_TRACE call is a diagnostic side effect
 * with no bearing on the behavior under test here, so this discards
 * everything and returns 0, the same as a message that logged nothing.
 *
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;

    return 0;
}


/** Stand-in return value for the next mouse_resize_cursor_update call */
static client_td *s_stub_resize_cursor_client;

/** Recorded argument from the last mouse_hover_track call */
static int s_hover_track_calls;
static xcb_window_t s_hover_track_window;

/** Stand-in return value for the next cycle_is_open call */
static bool s_stub_cycle_open;

/** Stand-in return value for the next focus_is_sloppy call */
static bool s_stub_focus_sloppy;

/** Stand-in return value for the next lookup_find_client call */
static client_td *s_stub_lookup_client;
static desktop_td *s_stub_lookup_desktop;

/** Stand-in return value for the next lookup_surface_for_root call */
static surface_td *s_stub_lookup_surface;

/** Recorded arguments from the last focus_apply call */
static int s_focus_apply_calls;
static surface_td *s_focus_apply_surface;
static desktop_td *s_focus_apply_desktop;
static client_td *s_focus_apply_client;
static bool s_focus_apply_raise;


/**
 * @brief Stand-in for @a mouse_resize_cursor_update
 * @note Complexity: @e O(1)
 */
client_td *mouse_resize_cursor_update(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t window,
        struct position_s root_pos)
{
    (void) connection;
    (void) surfaces;
    (void) window;
    (void) root_pos;

    return s_stub_resize_cursor_client;
}


/**
 * @brief Recording stand-in for @a mouse_hover_track
 * @note Complexity: @e O(1)
 */
void mouse_hover_track(xcb_window_t window)
{
    s_hover_track_calls++;
    s_hover_track_window = window;
}


/**
 * @brief Stand-in for @a cycle_is_open
 * @note Complexity: @e O(1)
 */
bool cycle_is_open(void)
{
    return s_stub_cycle_open;
}


/**
 * @brief Stand-in for @a focus_is_sloppy
 * @note Complexity: @e O(1)
 */
bool focus_is_sloppy(const config_td *cfg)
{
    (void) cfg;

    return s_stub_focus_sloppy;
}


/**
 * @brief Stand-in for @a lookup_find_client
 * @note Complexity: @e O(1)
 */
client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **out_surface, desktop_td **out_desktop)
{
    (void) surfaces;
    (void) window;

    if (out_surface != NULL) {
        *out_surface = NULL;
    }
    if (out_desktop != NULL) {
        *out_desktop = s_stub_lookup_desktop;
    }

    return s_stub_lookup_client;
}


/**
 * @brief Stand-in for @a lookup_surface_for_root
 * @note Complexity: @e O(1)
 */
surface_td *lookup_surface_for_root(list_td *surfaces, xcb_window_t root)
{
    (void) surfaces;
    (void) root;

    return s_stub_lookup_surface;
}


/**
 * @brief Recording stand-in for @a focus_apply
 * @note Complexity: @e O(1)
 */
void focus_apply(list_td *surfaces, surface_td *surface,
        desktop_td *desktop, client_td *client, bool raise,
        const config_td *cfg)
{
    (void) surfaces;
    (void) cfg;

    s_focus_apply_calls++;
    s_focus_apply_surface = surface;
    s_focus_apply_desktop = desktop;
    s_focus_apply_client = client;
    s_focus_apply_raise = raise;
}


static void s_reset(void)
{
    s_stub_resize_cursor_client = NULL;
    s_hover_track_calls = 0;
    s_hover_track_window = 0;
    s_stub_cycle_open = false;
    s_stub_focus_sloppy = false;
    s_stub_lookup_client = NULL;
    s_stub_lookup_desktop = NULL;
    s_stub_lookup_surface = NULL;
    s_focus_apply_calls = 0;
    s_focus_apply_surface = NULL;
    s_focus_apply_desktop = NULL;
    s_focus_apply_client = NULL;
    s_focus_apply_raise = false;
    mouse_enter_focus_clear();
}


static xcb_enter_notify_event_t s_make_event(xcb_window_t event_win,
        xcb_window_t child_win, xcb_window_t root_win,
        uint8_t mode, uint8_t detail, int16_t root_x, int16_t root_y)
{
    xcb_enter_notify_event_t event;

    memset(&event, 0, sizeof(event));
    event.event = event_win;
    event.child = child_win;
    event.root = root_win;
    event.mode = mode;
    event.detail = detail;
    event.root_x = root_x;
    event.root_y = root_y;

    return event;
}


/* Null connection, event or config: no-op, no crash */
static void s_test_null_arguments_are_a_no_op(void)
{
    xcb_enter_notify_event_t event = s_make_event(1, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&config, 0, sizeof(config));

    mouse_handle_enter(NULL, NULL, &event, &config);
    mouse_handle_enter((xcb_connection_t *) 1, NULL, NULL, &config);
    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, NULL);

    TAP_EQ_INT(s_hover_track_calls, 0,
            "any null required argument: bails out before hover"
            " tracking runs");
}


/* A non-XCB_NOTIFY_MODE_NORMAL event (grab/ungrab pseudo-motion) is
 * ignored entirely, before even the resize-cursor re-evaluation */
static void s_test_non_normal_mode_is_ignored(void)
{
    xcb_enter_notify_event_t event = s_make_event(1, 0, 1,
            XCB_NOTIFY_MODE_GRAB, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&config, 0, sizeof(config));

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT(s_hover_track_calls, 0,
            "XCB_NOTIFY_MODE_GRAB event: ignored before hover tracking");
}


/* mouse_resize_cursor_update always runs for a normal-mode event,
 * independent of focus-follows-mouse */
static void s_test_resize_cursor_always_reevaluated(void)
{
    client_td decorated_client;
    xcb_enter_notify_event_t event = s_make_event(5, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&decorated_client, 0, sizeof(decorated_client));
    decorated_client.frame = 42u;
    memset(&config, 0, sizeof(config));
    s_stub_resize_cursor_client = &decorated_client;
    s_stub_focus_sloppy = false;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT(s_hover_track_calls, 1,
            "resize-cursor path always runs, feeding hover tracking");
}


/* An entered client with a real frame (decorated) tracks
 * XCB_WINDOW_NONE for hover polling, since EnterNotify already covers
 * its border/content transitions */
static void s_test_decorated_client_tracks_none(void)
{
    client_td decorated_client;
    xcb_enter_notify_event_t event = s_make_event(5, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&decorated_client, 0, sizeof(decorated_client));
    decorated_client.frame = 42u;
    memset(&config, 0, sizeof(config));
    s_stub_resize_cursor_client = &decorated_client;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT((int) s_hover_track_window, (int) XCB_WINDOW_NONE,
            "decorated client (frame != 0): hover tracks NONE");
}


/* An entered client with no frame (undecorated) tracks the event
 * window itself, since it has no separate frame to catch that
 * transition */
static void s_test_undecorated_client_tracks_event_window(void)
{
    client_td undecorated_client;
    xcb_enter_notify_event_t event = s_make_event(7, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&undecorated_client, 0, sizeof(undecorated_client));
    undecorated_client.frame = 0;
    memset(&config, 0, sizeof(config));
    s_stub_resize_cursor_client = &undecorated_client;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT((int) s_hover_track_window, 7,
            "undecorated client (frame == 0): hover tracks the event"
            " window");
}


/* No client resolved by the resize-cursor pass: hover tracks NONE */
static void s_test_no_entered_client_tracks_none(void)
{
    xcb_enter_notify_event_t event = s_make_event(9, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&config, 0, sizeof(config));
    s_stub_resize_cursor_client = NULL;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT((int) s_hover_track_window, (int) XCB_WINDOW_NONE,
            "no resolved client: hover tracks NONE");
}


/* Inferior-detail transitions (entering a client's own content area
 * from its frame) skip all sloppy-focus logic, since the client
 * already had focus to get there */
static void s_test_inferior_detail_skips_sloppy_focus(void)
{
    xcb_enter_notify_event_t event = s_make_event(1, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_INFERIOR, 0, 0);
    config_td config;

    s_reset();
    memset(&config, 0, sizeof(config));
    s_stub_focus_sloppy = true;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT(s_focus_apply_calls, 0,
            "XCB_NOTIFY_DETAIL_INFERIOR: sloppy focus never evaluated");
}


/* Focus policy is not sloppy (click-to-focus): no focus transfer or
 * pending delay is armed */
static void s_test_non_sloppy_policy_does_nothing_further(void)
{
    xcb_enter_notify_event_t event = s_make_event(1, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&config, 0, sizeof(config));
    s_stub_focus_sloppy = false;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT(s_focus_apply_calls, 0,
            "click-to-focus policy: focus_apply never called");
    TAP_EQ_INT(mouse_enter_focus_ms_remaining(), -1,
            "and no delayed focus is armed either");
}


/* Sloppy focus, but the cycle menu is currently open: skipped
 * entirely, so navigating the cycle menu cannot steal its own focus */
static void s_test_cycle_open_blocks_sloppy_focus(void)
{
    xcb_enter_notify_event_t event = s_make_event(1, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&config, 0, sizeof(config));
    s_stub_focus_sloppy = true;
    s_stub_cycle_open = true;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT(s_focus_apply_calls, 0,
            "cycle menu open: sloppy focus is not applied");
}


/* Sloppy focus, no client resolved for the entered window: no
 * transfer happens */
static void s_test_sloppy_focus_no_client_does_nothing(void)
{
    xcb_enter_notify_event_t event = s_make_event(1, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&config, 0, sizeof(config));
    s_stub_focus_sloppy = true;
    s_stub_lookup_client = NULL;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT(s_focus_apply_calls, 0,
            "lookup_find_client returns null: no focus transfer");
}


/* Sloppy focus with delay_ms == 0: focus_apply runs immediately, and
 * the hover-transfer flag is set */
static void s_test_zero_delay_applies_focus_immediately(void)
{
    client_td dummy_client;
    surface_td dummy_surface;
    desktop_td dummy_desktop;
    xcb_enter_notify_event_t event = s_make_event(1, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&dummy_client, 0, sizeof(dummy_client));
    memset(&dummy_surface, 0, sizeof(dummy_surface));
    memset(&dummy_desktop, 0, sizeof(dummy_desktop));
    memset(&config, 0, sizeof(config));
    config.base.windows.focus.delay_ms = 0u;
    s_stub_focus_sloppy = true;
    s_stub_lookup_client = &dummy_client;
    s_stub_lookup_desktop = &dummy_desktop;
    s_stub_lookup_surface = &dummy_surface;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT(s_focus_apply_calls, 1,
            "delay_ms 0: focus_apply runs immediately");
    TAP_OK(s_focus_apply_client == &dummy_client,
            "focus_apply receives the resolved client");
    TAP_OK(!s_focus_apply_raise,
            "focus_apply is called with raise = false");
    TAP_OK(mouse_enter_focus_is_active(),
            "the hover-transfer flag is set");
    TAP_EQ_INT(mouse_enter_focus_ms_remaining(), -1,
            "and no delayed focus is left pending");
}


/* Sloppy focus with delay_ms > 0: focus_apply is deferred, no pending
 * window armed yet becomes armed with a positive remaining time */
static void s_test_positive_delay_arms_pending_focus(void)
{
    client_td dummy_client;
    surface_td dummy_surface;
    desktop_td dummy_desktop;
    xcb_enter_notify_event_t event = s_make_event(11, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&dummy_client, 0, sizeof(dummy_client));
    memset(&dummy_surface, 0, sizeof(dummy_surface));
    memset(&dummy_desktop, 0, sizeof(dummy_desktop));
    memset(&config, 0, sizeof(config));
    config.base.windows.focus.delay_ms = 250u;
    s_stub_focus_sloppy = true;
    s_stub_lookup_client = &dummy_client;
    s_stub_lookup_desktop = &dummy_desktop;
    s_stub_lookup_surface = &dummy_surface;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);

    TAP_EQ_INT(s_focus_apply_calls, 0,
            "delay_ms > 0: focus_apply is deferred, not called yet");
    TAP_OK(mouse_enter_focus_ms_remaining() > 0,
            "a delayed focus is now pending with positive remaining"
            " time");

    /* Ticking before the delay elapses does nothing yet */
    mouse_enter_focus_tick(NULL, &config);
    TAP_EQ_INT(s_focus_apply_calls, 0,
            "ticking before the deadline: still not applied");

    /* Cancel the pending window explicitly (mirrors a LeaveNotify) */
    mouse_enter_focus_cancel(11u);
    TAP_EQ_INT(mouse_enter_focus_ms_remaining(), -1,
            "canceling the pending window clears it");

    mouse_enter_focus_tick(NULL, &config);
    TAP_EQ_INT(s_focus_apply_calls, 0,
            "ticking after cancellation: never applies");
}


/* mouse_enter_focus_cancel is a no-op for a window that does not match
 * the currently pending one */
static void s_test_cancel_ignores_non_matching_window(void)
{
    client_td dummy_client;
    surface_td dummy_surface;
    desktop_td dummy_desktop;
    xcb_enter_notify_event_t event = s_make_event(21, 0, 1,
            XCB_NOTIFY_MODE_NORMAL, XCB_NOTIFY_DETAIL_NONLINEAR, 0, 0);
    config_td config;

    s_reset();
    memset(&dummy_client, 0, sizeof(dummy_client));
    memset(&dummy_surface, 0, sizeof(dummy_surface));
    memset(&dummy_desktop, 0, sizeof(dummy_desktop));
    memset(&config, 0, sizeof(config));
    config.base.windows.focus.delay_ms = 250u;
    s_stub_focus_sloppy = true;
    s_stub_lookup_client = &dummy_client;
    s_stub_lookup_desktop = &dummy_desktop;
    s_stub_lookup_surface = &dummy_surface;

    mouse_handle_enter((xcb_connection_t *) 1, NULL, &event, &config);
    mouse_enter_focus_cancel(999u);

    TAP_OK(mouse_enter_focus_ms_remaining() > 0,
            "canceling an unrelated window id leaves the pending one"
            " untouched");

    mouse_enter_focus_cancel(21u);
}


int main(void)
{
    TAP_PLAN(22);

    s_test_null_arguments_are_a_no_op();
    s_test_non_normal_mode_is_ignored();
    s_test_resize_cursor_always_reevaluated();
    s_test_decorated_client_tracks_none();
    s_test_undecorated_client_tracks_event_window();
    s_test_no_entered_client_tracks_none();
    s_test_inferior_detail_skips_sloppy_focus();
    s_test_non_sloppy_policy_does_nothing_further();
    s_test_cycle_open_blocks_sloppy_focus();
    s_test_sloppy_focus_no_client_does_nothing();
    s_test_zero_delay_applies_focus_immediately();
    s_test_positive_delay_arms_pending_focus();
    s_test_cancel_ignores_non_matching_window();

    return TAP_DONE();
}
