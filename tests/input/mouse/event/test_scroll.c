/**
 * @file tests/input/mouse/event/test_scroll.c
 *
 * @brief Test battery for scroll-wheel bindings (input/mouse/event/
 *        scroll.c)
 *
 * im_press_scroll_binding dispatches to one of several titlebar
 * actions (maximize/restore/shade/unshade) when the scroll landed on
 * a client's titlebar, replays the event to the client when it landed
 * on its content, or switches desktops directly otherwise.  Every
 * genuinely external collaborator (the ccmd_client_* command layer,
 * desktop_action_client_send_back, focus_order_to_bottom,
 * client_focus_fallback, focus_apply, the four
 * enact_surface_desktop_switch_* entry points, and
 * lookup_surface_for_root) is a recording stand-in; client_is_maximized
 * and client_is_shaded are real macros over client_td.properties, left
 * untouched since they read only the fixture's own state.
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

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Policy includes */
#include <cmds/client/focus.h>
#include <policy/focus.h>

/* Command includes */
#include <cmds/client/maximize.h>
#include <cmds/client/state.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <render/outdate.h>
#include <surface.h>
#include <wm.h>

/* Input includes */
#include <input/mouse/bind.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/event.h>
#include <input/mouse/internal.h>


/** Recorded calls to ccmd_client_maximize */
static int s_maximize_calls;

/** Recorded calls to ccmd_client_shade / ccmd_client_unshade */
static int s_shade_calls;
static int s_unshade_calls;

/** Recorded calls to desktop_action_client_send_back */
static int s_send_back_calls;

/** Recorded calls to focus_order_to_bottom */
static int s_order_to_bottom_calls;

/** Recorded calls to client_focus_fallback */
static int s_focus_fallback_calls;

/** Recorded calls to focus_apply */
static int s_focus_apply_calls;

/** Recorded calls to each enact_surface_desktop_switch_* direction */
static int s_switch_north_calls;
static int s_switch_south_calls;
static int s_switch_east_calls;
static int s_switch_west_calls;

/** Stand-in return value for the next lookup_surface_for_root call */
static surface_td *s_stub_lookup_surface;

/** Recorded calls to im_sync_sticky_active and im_allow_and_flush */
static int s_sync_sticky_calls;
static int s_allow_and_flush_calls;


/**
 * @brief Recording stand-in for @a ccmd_client_maximize
 * @note Complexity: @e O(1)
 */
void ccmd_client_maximize(client_td *client)
{
    (void) client;

    s_maximize_calls++;
}


/**
 * @brief Stand-in for @a ccmd_client_maximize_horz, never reached by
 *        scroll.c
 * @note Complexity: @e O(1)
 */
void ccmd_client_maximize_horz(client_td *client)
{
    (void) client;
}


/**
 * @brief Stand-in for @a ccmd_client_maximize_vert, never reached by
 *        scroll.c
 * @note Complexity: @e O(1)
 */
void ccmd_client_maximize_vert(client_td *client)
{
    (void) client;
}


/**
 * @brief Recording stand-in for @a ccmd_client_shade
 * @note Complexity: @e O(1)
 */
void ccmd_client_shade(client_td *client)
{
    (void) client;

    s_shade_calls++;
}


/**
 * @brief Recording stand-in for @a ccmd_client_unshade
 * @note Complexity: @e O(1)
 */
void ccmd_client_unshade(client_td *client)
{
    (void) client;

    s_unshade_calls++;
}


/**
 * @brief Recording stand-in for @a desktop_action_client_send_back
 * @note Complexity: @e O(1)
 */
int desktop_action_client_send_back(desktop_td *desktop,
        client_td *client)
{
    (void) desktop;
    (void) client;

    s_send_back_calls++;

    return 0;
}


/**
 * @brief Recording stand-in for @a focus_order_to_bottom
 * @note Complexity: @e O(1)
 */
void focus_order_to_bottom(const desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;

    s_order_to_bottom_calls++;
}


/**
 * @brief Recording stand-in for @a client_focus_fallback
 * @note Complexity: @e O(1)
 */
void client_focus_fallback(desktop_td *desktop, surface_td *surface,
        client_td *client)
{
    (void) desktop;
    (void) surface;
    (void) client;

    s_focus_fallback_calls++;
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
    (void) surface;
    (void) desktop;
    (void) client;
    (void) raise;
    (void) cfg;

    s_focus_apply_calls++;
}


/**
 * @brief Recording stand-in for @a enact_surface_desktop_switch_north
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_switch_north(surface_td *surface)
{
    (void) surface;

    s_switch_north_calls++;
}


/**
 * @brief Recording stand-in for @a enact_surface_desktop_switch_south
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_switch_south(surface_td *surface)
{
    (void) surface;

    s_switch_south_calls++;
}


/**
 * @brief Recording stand-in for @a enact_surface_desktop_switch_east
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_switch_east(surface_td *surface)
{
    (void) surface;

    s_switch_east_calls++;
}


/**
 * @brief Recording stand-in for @a enact_surface_desktop_switch_west
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_switch_west(surface_td *surface)
{
    (void) surface;

    s_switch_west_calls++;
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
 * @brief Recording stand-in for @a im_sync_sticky_active
 * @note Complexity: @e O(1)
 */
void im_sync_sticky_active(surface_td *surface, const desktop_td *desktop,
        const client_td *client)
{
    (void) surface;
    (void) desktop;
    (void) client;

    s_sync_sticky_calls++;
}


/**
 * @brief Recording stand-in for @a im_allow_and_flush
 * @note Complexity: @e O(1)
 */
void im_allow_and_flush(xcb_connection_t *connection, uint8_t mode,
        xcb_timestamp_t time)
{
    (void) connection;
    (void) mode;
    (void) time;

    s_allow_and_flush_calls++;
}


static void s_reset(void)
{
    s_maximize_calls = 0;
    s_shade_calls = 0;
    s_unshade_calls = 0;
    s_send_back_calls = 0;
    s_order_to_bottom_calls = 0;
    s_focus_fallback_calls = 0;
    s_focus_apply_calls = 0;
    s_switch_north_calls = 0;
    s_switch_south_calls = 0;
    s_switch_east_calls = 0;
    s_switch_west_calls = 0;
    s_stub_lookup_surface = NULL;
    s_sync_sticky_calls = 0;
    s_allow_and_flush_calls = 0;
}


/* Titlebar client with the exact height/left extents used so
 * s_scroll_on_titlebar's Y-range test matches, defaulting event->child
 * to the titlebar window so both of its two ways of matching agree */
static void s_make_titlebar_client(client_td *client, xcb_window_t titlebar)
{
    memset(client, 0, sizeof(*client));
    client->titlebar = titlebar;
    client->layout.frame_extents.left = 2;
    client->layout.frame_extents.top = 24;
}


static xcb_button_press_event_t s_make_event(xcb_window_t root,
        xcb_window_t child, uint8_t detail, int16_t root_y)
{
    xcb_button_press_event_t event;

    memset(&event, 0, sizeof(event));
    event.root = root;
    event.child = child;
    event.detail = detail;
    event.root_y = root_y;

    return event;
}


/* Scroll north on an unmaximized client's titlebar: maximizes it */
static void s_test_north_on_titlebar_maximizes_unmaximized(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 55, 4, 10);

    s_reset();
    s_make_titlebar_client(&client, 55);

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            &client, NULL, MOUSEBIND_DESKTOP_NORTH, NULL);

    TAP_EQ_INT(s_maximize_calls, 1,
            "scroll north on titlebar, not maximized: maximize called");
}


/* Scroll north on an already-maximized client's titlebar: no-op */
static void s_test_north_on_titlebar_skips_already_maximized(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 55, 4, 10);

    s_reset();
    s_make_titlebar_client(&client, 55);
    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED;

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            &client, NULL, MOUSEBIND_DESKTOP_NORTH, NULL);

    TAP_EQ_INT(s_maximize_calls, 0,
            "scroll north on titlebar, already maximized: no-op");
}


/* Scroll south on a maximized client's titlebar: restores it (through
 * the same maximize toggle) */
static void s_test_south_on_titlebar_restores_maximized(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 55, 5, 10);

    s_reset();
    s_make_titlebar_client(&client, 55);
    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED;

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            &client, NULL, MOUSEBIND_DESKTOP_SOUTH, NULL);

    TAP_EQ_INT(s_maximize_calls, 1,
            "scroll south on titlebar, maximized: restore (maximize"
            " toggle) called");
}


/* Scroll south on a non-maximized client's titlebar: no-op */
static void s_test_south_on_titlebar_skips_not_maximized(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 55, 5, 10);

    s_reset();
    s_make_titlebar_client(&client, 55);

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            &client, NULL, MOUSEBIND_DESKTOP_SOUTH, NULL);

    TAP_EQ_INT(s_maximize_calls, 0,
            "scroll south on titlebar, not maximized: no-op");
}


/* Scroll west on an unshaded client's titlebar: shades it, sends it to
 * the back of both orders, and falls back focus when it was active */
static void s_test_west_on_titlebar_shades_and_sends_back(void)
{
    client_td client;
    desktop_td desktop;
    surface_td surface;
    xcb_button_press_event_t event = s_make_event(1, 55, 4, 10);

    s_reset();
    s_make_titlebar_client(&client, 55);
    client.id = 7u;
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));
    desktop.client_active_id = 7u;
    s_stub_lookup_surface = &surface;

    /* type is MOUSEBIND_DESKTOP_WEST, but scroll direction itself
     * (button 4 vs 5) does not gate the titlebar shade/unshade
     * dispatch, only 'type' does, so 'detail' here only needs to
     * satisfy s_scroll_on_titlebar's own Y-range test, already
     * satisfied by s_make_event's root_y = 10 */
    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            &client, &desktop, MOUSEBIND_DESKTOP_WEST, NULL);

    TAP_EQ_INT(s_shade_calls, 1,
            "scroll west on titlebar, unshaded: shade called");
    TAP_EQ_INT(s_send_back_calls, 1,
            "client sent to the back of the stacking order");
    TAP_EQ_INT(s_order_to_bottom_calls, 1,
            "and to the back of the focus order");
    TAP_EQ_INT(s_focus_fallback_calls, 1,
            "was the active client: focus falls back to another");
}


/* Scroll west on an already-shaded client's titlebar: no-op */
static void s_test_west_on_titlebar_skips_already_shaded(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 55, 4, 10);

    s_reset();
    s_make_titlebar_client(&client, 55);
    client.properties.flags |= CLIENT_FLAG_SHADED;

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            &client, NULL, MOUSEBIND_DESKTOP_WEST, NULL);

    TAP_EQ_INT(s_shade_calls, 0,
            "scroll west on titlebar, already shaded: no-op");
}


/* Scroll east on a shaded client's titlebar: unshades it, and reapplies
 * focus (with a sticky sync) since it was the active client */
static void s_test_east_on_titlebar_unshades_and_refocuses(void)
{
    client_td client;
    desktop_td desktop;
    surface_td surface;
    xcb_button_press_event_t event = s_make_event(1, 55, 5, 10);

    s_reset();
    s_make_titlebar_client(&client, 55);
    client.id = 3u;
    client.properties.flags |= CLIENT_FLAG_SHADED;
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));
    desktop.client_active_id = 3u;
    s_stub_lookup_surface = &surface;

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            &client, &desktop, MOUSEBIND_DESKTOP_EAST, NULL);

    TAP_EQ_INT(s_unshade_calls, 1,
            "scroll east on titlebar, shaded: unshade called");
    TAP_EQ_INT(s_focus_apply_calls, 1,
            "was the active client: focus_apply reapplies focus");
}


/* Scroll east on an already-unshaded client's titlebar: no-op */
static void s_test_east_on_titlebar_skips_already_unshaded(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 55, 5, 10);

    s_reset();
    s_make_titlebar_client(&client, 55);

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            &client, NULL, MOUSEBIND_DESKTOP_EAST, NULL);

    TAP_EQ_INT(s_unshade_calls, 0,
            "scroll east on titlebar, already unshaded: no-op");
}


/* Scroll over a client's content area (not the titlebar): no titlebar
 * action runs at all, since the client here has titlebar == 0 */
static void s_test_scroll_over_content_runs_no_titlebar_action(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 999, 4, 10);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.titlebar = 0;

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            &client, NULL, MOUSEBIND_DESKTOP_NORTH, NULL);

    TAP_EQ_INT(s_maximize_calls, 0,
            "titlebar == 0 (content click): no titlebar action runs");
}


/* No client under the pointer: a desktop switch happens directly, one
 * call per direction, only when a surface resolves for the root */
static void s_test_no_client_switches_desktop_directly(void)
{
    surface_td surface;
    xcb_button_press_event_t event = s_make_event(1, 0, 4, 10);

    s_reset();
    memset(&surface, 0, sizeof(surface));
    s_stub_lookup_surface = &surface;

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            NULL, NULL, MOUSEBIND_DESKTOP_NORTH, NULL);
    TAP_EQ_INT(s_switch_north_calls, 1,
            "no client, north binding: switch north called once");

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            NULL, NULL, MOUSEBIND_DESKTOP_SOUTH, NULL);
    TAP_EQ_INT(s_switch_south_calls, 1,
            "no client, south binding: switch south called once");

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            NULL, NULL, MOUSEBIND_DESKTOP_EAST, NULL);
    TAP_EQ_INT(s_switch_east_calls, 1,
            "no client, east binding: switch east called once");

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            NULL, NULL, MOUSEBIND_DESKTOP_WEST, NULL);
    TAP_EQ_INT(s_switch_west_calls, 1,
            "no client, west binding: switch west called once");
}


/* No client under the pointer, and no surface resolves for the root:
 * no desktop switch call happens at all */
static void s_test_no_client_no_surface_switches_nothing(void)
{
    xcb_button_press_event_t event = s_make_event(1, 0, 4, 10);

    s_reset();
    s_stub_lookup_surface = NULL;

    im_press_scroll_binding((xcb_connection_t *) 1, NULL, &event,
            NULL, NULL, MOUSEBIND_DESKTOP_NORTH, NULL);

    TAP_EQ_INT(s_switch_north_calls, 0,
            "no surface resolved for the root: no switch call happens");
}


int main(void)
{
    TAP_PLAN(18);

    s_test_north_on_titlebar_maximizes_unmaximized();
    s_test_north_on_titlebar_skips_already_maximized();
    s_test_south_on_titlebar_restores_maximized();
    s_test_south_on_titlebar_skips_not_maximized();
    s_test_west_on_titlebar_shades_and_sends_back();
    s_test_west_on_titlebar_skips_already_shaded();
    s_test_east_on_titlebar_unshades_and_refocuses();
    s_test_east_on_titlebar_skips_already_unshaded();
    s_test_scroll_over_content_runs_no_titlebar_action();
    s_test_no_client_switches_desktop_directly();
    s_test_no_client_no_surface_switches_nothing();

    return TAP_DONE();
}
