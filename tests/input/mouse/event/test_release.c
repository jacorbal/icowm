/**
 * @file tests/input/mouse/event/test_release.c
 *
 * @brief Test battery for mouse_handle_release
 *
 * mouse_handle_release (input/mouse/event/release.c) is a thin
 * dispatcher: bail out early when no drag is active, otherwise resolve
 * the dragged client's owning surface/desktop (when both a client and
 * a surface list are available) and hand everything to drag_end.
 * drag_is_active, drag_client, drag_end (input/mouse/drag.h) and
 * lookup_find_client (lookup.h) are all genuinely external,
 * cross-module entry points this file has no interest in re-testing
 * (drag.c's own state machine is covered by
 * tests/input/mouse/drag/test_drag.c, and lookup_find_client's search
 * logic is a lookup.c concern), so all four are recording/return-value
 * stand-ins defined locally.
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

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag.h>
#include <input/mouse/event.h>


/** Stand-in return value for the next drag_is_active call */
static bool s_stub_is_active;

/** Stand-in return value for the next drag_client call */
static client_td *s_stub_client;

/** Recorded arguments from the last lookup_find_client call */
static int s_lookup_calls;
static list_td *s_lookup_surfaces_arg;
static xcb_window_t s_lookup_window_arg;
static surface_td *s_stub_lookup_surface;
static desktop_td *s_stub_lookup_desktop;

/** Recorded arguments from the last drag_end call */
static int s_drag_end_calls;
static xcb_connection_t *s_drag_end_connection;
static surface_td *s_drag_end_surface;
static desktop_td *s_drag_end_desktop;
static struct position_s s_drag_end_root_pos;


/**
 * @brief Stand-in for @a drag_is_active, returning a test-controlled
 *        value
 * @note Complexity: @e O(1)
 */
bool drag_is_active(void)
{
    return s_stub_is_active;
}


/**
 * @brief Stand-in for @a drag_client, returning a test-controlled
 *        value
 * @note Complexity: @e O(1)
 */
client_td *drag_client(void)
{
    return s_stub_client;
}


/**
 * @brief Recording stand-in for @a lookup_find_client
 * @note Complexity: @e O(1)
 */
client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **out_surface, desktop_td **out_desktop)
{
    s_lookup_calls++;
    s_lookup_surfaces_arg = surfaces;
    s_lookup_window_arg = window;

    if (out_surface != NULL) {
        *out_surface = s_stub_lookup_surface;
    }
    if (out_desktop != NULL) {
        *out_desktop = s_stub_lookup_desktop;
    }

    return s_stub_client;
}


/**
 * @brief Recording stand-in for @a drag_end
 * @note Complexity: @e O(1)
 */
void drag_end(xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, struct position_s root_pos)
{
    s_drag_end_calls++;
    s_drag_end_connection = connection;
    s_drag_end_surface = surface;
    s_drag_end_desktop = desktop;
    s_drag_end_root_pos = root_pos;
}


static void s_reset(void)
{
    s_stub_is_active = false;
    s_stub_client = NULL;
    s_lookup_calls = 0;
    s_lookup_surfaces_arg = NULL;
    s_lookup_window_arg = 0;
    s_stub_lookup_surface = NULL;
    s_stub_lookup_desktop = NULL;
    s_drag_end_calls = 0;
    s_drag_end_connection = NULL;
    s_drag_end_surface = NULL;
    s_drag_end_desktop = NULL;
    memset(&s_drag_end_root_pos, 0, sizeof(s_drag_end_root_pos));
}


/* No drag active: the function returns immediately, calling neither
 * lookup_find_client nor drag_end */
static void s_test_no_active_drag_is_a_no_op(void)
{
    xcb_button_release_event_t event;

    s_reset();
    memset(&event, 0, sizeof(event));
    s_stub_is_active = false;

    mouse_handle_release((xcb_connection_t *) 1, NULL, &event, NULL);

    TAP_EQ_INT(s_lookup_calls, 0,
            "no active drag: lookup_find_client is never called");
    TAP_EQ_INT(s_drag_end_calls, 0,
            "no active drag: drag_end is never called");
}


/* Active drag, event non-null: root_x/root_y are copied into the
 * position passed to drag_end */
static void s_test_active_drag_copies_root_position(void)
{
    xcb_button_release_event_t event;

    s_reset();
    memset(&event, 0, sizeof(event));
    event.root_x = 123;
    event.root_y = 456;
    s_stub_is_active = true;
    s_stub_client = NULL;

    mouse_handle_release((xcb_connection_t *) 1, NULL, &event, NULL);

    TAP_EQ_INT(s_drag_end_calls, 1, "drag_end is called once");
    TAP_EQ_INT(s_drag_end_root_pos.x, 123,
            "root_pos.x taken from event->root_x");
    TAP_EQ_INT(s_drag_end_root_pos.y, 456,
            "root_pos.y taken from event->root_y");
}


/* Active drag, event null: root_pos defaults to (0, 0) rather than
 * dereferencing a null event */
static void s_test_active_drag_null_event_defaults_position(void)
{
    s_reset();
    s_stub_is_active = true;
    s_stub_client = NULL;

    mouse_handle_release((xcb_connection_t *) 1, NULL, NULL, NULL);

    TAP_EQ_INT(s_drag_end_calls, 1,
            "drag_end is still called with a null event");
    TAP_EQ_INT(s_drag_end_root_pos.x, 0,
            "root_pos.x defaults to 0 with a null event");
    TAP_EQ_INT(s_drag_end_root_pos.y, 0,
            "root_pos.y defaults to 0 with a null event");
}


/* Active drag, drag_client returns null: lookup_find_client is never
 * called (nothing to look up), and drag_end still runs with null
 * surface/desktop */
static void s_test_null_drag_client_skips_lookup(void)
{
    xcb_button_release_event_t event;
    list_td *const fake_surfaces = (list_td *) 1;

    s_reset();
    memset(&event, 0, sizeof(event));
    s_stub_is_active = true;
    s_stub_client = NULL;

    mouse_handle_release((xcb_connection_t *) 1, fake_surfaces, &event,
            NULL);

    TAP_EQ_INT(s_lookup_calls, 0,
            "drag_client returns null: lookup_find_client is skipped");
    TAP_OK(s_drag_end_surface == NULL,
            "drag_end still called, with a null surface");
    TAP_OK(s_drag_end_desktop == NULL,
            "and a null desktop");
}


/* Active drag, drag_client returns non-null but surfaces is null:
 * lookup_find_client is skipped (nothing to search) */
static void s_test_null_surfaces_skips_lookup(void)
{
    client_td dummy_client;
    xcb_button_release_event_t event;

    s_reset();
    memset(&dummy_client, 0, sizeof(dummy_client));
    memset(&event, 0, sizeof(event));
    s_stub_is_active = true;
    s_stub_client = &dummy_client;

    mouse_handle_release((xcb_connection_t *) 1, NULL, &event, NULL);

    TAP_EQ_INT(s_lookup_calls, 0,
            "null surfaces list: lookup_find_client is skipped");
}


/* Active drag, both drag_client and surfaces non-null:
 * lookup_find_client is called with the dragged client's window ID,
 * and its resolved surface/desktop flow through to drag_end */
static void s_test_valid_client_and_surfaces_looks_up_and_forwards(void)
{
    client_td dummy_client;
    surface_td dummy_surface;
    desktop_td dummy_desktop;
    xcb_button_release_event_t event;
    list_td *const fake_surfaces = (list_td *) 7;

    s_reset();
    memset(&dummy_client, 0, sizeof(dummy_client));
    memset(&dummy_surface, 0, sizeof(dummy_surface));
    memset(&dummy_desktop, 0, sizeof(dummy_desktop));
    memset(&event, 0, sizeof(event));
    dummy_client.id = 999u;
    s_stub_is_active = true;
    s_stub_client = &dummy_client;
    s_stub_lookup_surface = &dummy_surface;
    s_stub_lookup_desktop = &dummy_desktop;

    mouse_handle_release((xcb_connection_t *) 1, fake_surfaces, &event,
            NULL);

    TAP_EQ_INT(s_lookup_calls, 1,
            "lookup_find_client called once for a valid client");
    TAP_OK(s_lookup_surfaces_arg == fake_surfaces,
            "lookup_find_client receives the same surfaces list");
    TAP_EQ_INT((int) s_lookup_window_arg, (int) dummy_client.id,
            "lookup_find_client is searched by the dragged client's"
            " window id");
    TAP_OK(s_drag_end_surface == &dummy_surface,
            "the resolved surface reaches drag_end");
    TAP_OK(s_drag_end_desktop == &dummy_desktop,
            "the resolved desktop reaches drag_end");
    TAP_OK(s_drag_end_connection == (xcb_connection_t *) 1,
            "the connection reaches drag_end unchanged");
}


int main(void)
{
    TAP_PLAN(18);

    s_test_no_active_drag_is_a_no_op();
    s_test_active_drag_copies_root_position();
    s_test_active_drag_null_event_defaults_position();
    s_test_null_drag_client_skips_lookup();
    s_test_null_surfaces_skips_lookup();
    s_test_valid_client_and_surfaces_looks_up_and_forwards();

    return TAP_DONE();
}
