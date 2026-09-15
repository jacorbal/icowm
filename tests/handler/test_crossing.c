/**
 * @file tests/handler/test_crossing.c
 *
 * @brief Test battery for handler/crossing.c: the @c LEAVE_NOTIFY
 *        event handler
 *
 * handler_leave_notify itself takes a real, stack-built 'struct wm_s'
 * (see wm/internal.h, safe to include from a test file since only
 * creating/editing/touching include/ is forbidden, not reading it),
 * with src/wm/instance.c linked for real: it is nothing but narrow,
 * side-effect-free field accessors over that struct, the same leaf
 * rationale already used for adt/cdlist.c in
 * tests/input/mouse/event/test_press.c.
 *
 * Every other collaborator (mouse_hover_poll_clear,
 * mouse_enter_focus_cancel, cycle_is_open, focus_is_sloppy,
 * lookup_find_client, enact_client_unfocus, and the raw
 * xcb_query_pointer/xcb_query_pointer_reply pair) is cross-module or
 * X-server-bound and is a link-only or controlled stand-in below,
 * following the exact xcb_query_pointer/_reply convention already
 * established in tests/input/mouse/test_hover.c.
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <stdlib.h>     /* malloc, free */
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <client/state.h>
#include <config.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <handler/leave.h>
#include <harness/tap.h>


/* Recording state for every link-only stand-in below, reset by
 * s_reset before each scenario */
static int s_call_hover_poll_clear;
static xcb_window_t s_hover_poll_clear_window;
static int s_call_enter_focus_cancel;
static xcb_window_t s_enter_focus_cancel_window;
static bool s_cycle_is_open_value;
static bool s_focus_is_sloppy_value;
static int s_call_unfocus;
static client_td *s_unfocus_last_client;

/* What lookup_find_client should hand back, and to which of its two
 * call sites (the event window, or the active-client id lookup) */
static client_td *s_lookup_event_client;
static surface_td *s_lookup_event_surface;
static desktop_td *s_lookup_event_desktop;
static client_td *s_lookup_active_client;

/* What xcb_query_pointer_reply should hand back */
static bool s_reply_present;
static xcb_window_t s_reply_child;


static void s_reset(void)
{
    s_call_hover_poll_clear = 0;
    s_hover_poll_clear_window = XCB_WINDOW_NONE;
    s_call_enter_focus_cancel = 0;
    s_enter_focus_cancel_window = XCB_WINDOW_NONE;
    s_cycle_is_open_value = false;
    s_focus_is_sloppy_value = true;
    s_call_unfocus = 0;
    s_unfocus_last_client = NULL;
    s_lookup_event_client = NULL;
    s_lookup_event_surface = NULL;
    s_lookup_event_desktop = NULL;
    s_lookup_active_client = NULL;
    s_reply_present = false;
    s_reply_child = XCB_WINDOW_NONE;
}


/** Link-only stand-in for mouse_hover_poll_clear */
void mouse_hover_poll_clear(xcb_window_t window)
{
    s_call_hover_poll_clear++;
    s_hover_poll_clear_window = window;
}


/** Link-only stand-in for mouse_enter_focus_cancel */
void mouse_enter_focus_cancel(xcb_window_t window)
{
    s_call_enter_focus_cancel++;
    s_enter_focus_cancel_window = window;
}


/** Link-only stand-in for cycle_is_open */
bool cycle_is_open(void)
{
    return s_cycle_is_open_value;
}


/** Link-only stand-in for focus_is_sloppy */
bool focus_is_sloppy(const config_td *cfg)
{
    (void) cfg;

    return s_focus_is_sloppy_value;
}


/** Link-only stand-in for enact_client_unfocus */
void enact_client_unfocus(client_td *client)
{
    s_call_unfocus++;
    s_unfocus_last_client = client;
}


/**
 * @brief Controlled stand-in for lookup_find_client
 *
 * handler_leave_notify calls this exactly twice on its sloppy-focus
 * path: once with the event window (to resolve surface/desktop), and
 * once with a desktop's client_active_id (to resolve the active
 * client, passing NULL for the out parameters).  Distinguished here
 * by whether @p surface/@p desktop are NULL, the same way the two
 * call sites in the real source differ.
 */
client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **surface, desktop_td **desktop)
{
    (void) surfaces;
    (void) window;

    if (surface == NULL && desktop == NULL) {
        return s_lookup_active_client;
    }

    if (surface != NULL) {
        *surface = s_lookup_event_surface;
    }
    if (desktop != NULL) {
        *desktop = s_lookup_event_desktop;
    }

    return s_lookup_event_client;
}


/** Link-only stand-in for xcb_query_pointer: the returned cookie is
 *  only ever handed straight to xcb_query_pointer_reply */
xcb_query_pointer_cookie_t xcb_query_pointer(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_query_pointer_cookie_t cookie;

    (void) connection;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/** Controlled stand-in for xcb_query_pointer_reply: hands back a
 *  heap-allocated reply naming s_reply_child when s_reply_present is
 *  set, so the real free() in crossing.c has a genuine allocation to
 *  release, or NULL otherwise */
xcb_query_pointer_reply_t *xcb_query_pointer_reply(
        xcb_connection_t *connection, xcb_query_pointer_cookie_t cookie,
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
    reply->child = s_reply_child;

    return reply;
}


/** Build a minimal, real 'struct wm_s' on the stack */
static wm_td s_make_wm(config_td *config)
{
    wm_td local_wm;

    memset(&local_wm, 0, sizeof(local_wm));
    local_wm.config = config;

    return local_wm;
}


int main(void)
{
    wm_td wm;
    config_td config;
    xcb_enter_notify_event_t event;
    client_td active;
    desktop_td desktop;
    surface_td surface;
    xcb_screen_t screen;

    TAP_PLAN(20);

    memset(&config, 0, sizeof(config));
    wm = s_make_wm(&config);

    /* Guard clauses: neither a null wm nor a null event ever reaches
     * any collaborator */
    s_reset();
    handler_leave_notify(NULL, NULL);
    TAP_EQ_INT(s_call_hover_poll_clear, 0,
            "a null wm never calls mouse_hover_poll_clear");

    memset(&event, 0, sizeof(event));
    event.event = 0x100u;
    s_reset();
    handler_leave_notify(NULL, &event);
    TAP_EQ_INT(s_call_hover_poll_clear, 0,
            "a null wm with a real event still never calls" \
            " mouse_hover_poll_clear");

    s_reset();
    handler_leave_notify(&wm, NULL);
    TAP_EQ_INT(s_call_hover_poll_clear, 0,
            "a real wm with a null event never calls" \
            " mouse_hover_poll_clear");

    /* mouse_hover_poll_clear and mouse_enter_focus_cancel are always
     * called first, with the raw event->event window, regardless of
     * every other branch below; proven once here with cycle open (the
     * very next guard) so nothing past them runs */
    s_reset();
    s_cycle_is_open_value = true;
    handler_leave_notify(&wm, &event);
    TAP_EQ_INT(s_call_hover_poll_clear, 1,
            "hover poll is cleared exactly once, unconditionally");
    TAP_EQ_INT((int) s_hover_poll_clear_window, (int) 0x100,
            "hover poll is cleared for the event's own window");
    TAP_EQ_INT(s_call_enter_focus_cancel, 1,
            "delayed sloppy focus is canceled exactly once," \
            " unconditionally");
    TAP_EQ_INT((int) s_enter_focus_cancel_window, (int) 0x100,
            "delayed sloppy focus is canceled for the event's own" \
            " window");

    /* An open cycle menu stops everything past the two cancellations
     * above: no lookup, no unfocus */
    TAP_EQ_INT(s_call_unfocus, 0,
            "an open cycle menu never unfocuses anything");

    /* Focus-follows-mouse disabled: the whole sloppy-focus block is
     * skipped even with the cycle menu closed */
    s_reset();
    s_focus_is_sloppy_value = false;
    handler_leave_notify(&wm, &event);
    TAP_EQ_INT(s_call_unfocus, 0,
            "sloppy focus disabled means no unfocus happens either");

    /* Sloppy focus on, cycle closed, but the crossing mode/detail
     * disqualify it (an INFERIOR crossing, i.e., between a window and
     * its own sub-window, never triggers unfocus) */
    s_reset();
    event.mode = XCB_NOTIFY_MODE_NORMAL;
    event.detail = XCB_NOTIFY_DETAIL_INFERIOR;
    handler_leave_notify(&wm, &event);
    TAP_EQ_INT(s_call_unfocus, 0,
            "an INFERIOR crossing never triggers unfocus");

    /* Sloppy focus on, a real NORMAL/non-INFERIOR crossing, but
     * lookup_find_client finds no managed client owning this window:
     * still no unfocus */
    s_reset();
    event.mode = XCB_NOTIFY_MODE_NORMAL;
    event.detail = XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL;
    s_lookup_event_client = NULL;
    handler_leave_notify(&wm, &event);
    TAP_EQ_INT(s_call_unfocus, 0,
            "no managed client owning the window means no unfocus");

    /* A managed client is found, but its desktop has nobody active
     * (client_active_id == 0): nothing to unfocus */
    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    desktop.client_active_id = 0u;
    s_lookup_event_client = (client_td *) 0x1;   /* any non-null value */
    s_lookup_event_desktop = &desktop;
    handler_leave_notify(&wm, &event);
    TAP_EQ_INT(s_call_unfocus, 0,
            "a desktop with nobody active never calls unfocus");

    /* Everything lines up (sloppy focus, non-INFERIOR crossing, a
     * managed client, an active client on that desktop) and the
     * lookup for the active client itself finds someone real: unfocus
     * fires and the desktop/surface are marked dirty and outdated */
    s_reset();
    memset(&active, 0, sizeof(active));
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));
    desktop.client_active_id = 0x42u;
    desktop.is_focus_dirty = false;
    desktop.is_outdated = false;
    surface.is_outdated = false;
    surface.screen = NULL;      /* no screen: the NONLINEAR re-check
                                    below is skipped for this case */
    s_lookup_event_client = (client_td *) 0x1;
    s_lookup_event_surface = &surface;
    s_lookup_event_desktop = &desktop;
    s_lookup_active_client = &active;
    handler_leave_notify(&wm, &event);
    TAP_EQ_INT(s_call_unfocus, 1,
            "a real active client on the desktop is unfocused" \
            " exactly once");
    TAP_OK(s_unfocus_last_client == &active,
            "the exact active client instance is the one unfocused");
    TAP_EQ_INT((int) desktop.client_active_id, 0,
            "the desktop's active client id is cleared");
    TAP_OK(desktop.is_focus_dirty,
            "the desktop is marked focus-dirty");
    TAP_OK(desktop.is_outdated,
            "the desktop is marked outdated");
    TAP_OK(surface.is_outdated,
            "the surface is marked outdated");

    /* A NONLINEAR crossing with a real screen: a fresh QueryPointer is
     * consulted, and finding the same active client still on the
     * other side (a sibling crossing through the shared frame, not a
     * genuine departure) suppresses the unfocus entirely */
    s_reset();
    memset(&active, 0, sizeof(active));
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));
    screen.root = 0x900u;
    desktop.client_active_id = 0x42u;
    surface.screen = &screen;
    event.detail = XCB_NOTIFY_DETAIL_NONLINEAR;
    s_lookup_event_client = (client_td *) 0x1;
    s_lookup_event_surface = &surface;
    s_lookup_event_desktop = &desktop;
    s_lookup_active_client = &active;
    s_reply_present = true;
    s_reply_child = 0x42u;      /* the stand-in itself is treated as
                                    a client, matched below */
    handler_leave_notify(&wm, &event);
    TAP_EQ_INT(s_call_unfocus, 0,
            "a sibling crossing found via a fresh QueryPointer" \
            " suppresses unfocus");

    /* Same NONLINEAR setup, but the pointer reply names some other
     * window entirely (not the active client): a genuine departure,
     * so unfocus still fires */
    s_reset();
    memset(&active, 0, sizeof(active));
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));
    screen.root = 0x900u;
    desktop.client_active_id = 0x42u;
    surface.screen = &screen;
    event.detail = XCB_NOTIFY_DETAIL_NONLINEAR;
    s_lookup_event_client = (client_td *) 0x1;
    s_lookup_event_surface = &surface;
    s_lookup_event_desktop = &desktop;
    s_lookup_active_client = &active;
    s_reply_present = true;
    s_reply_child = XCB_WINDOW_NONE;    /* nobody managed owns this */
    handler_leave_notify(&wm, &event);
    TAP_EQ_INT(s_call_unfocus, 1,
            "a genuine departure (no sibling match) still unfocuses");

    return TAP_DONE();
}
