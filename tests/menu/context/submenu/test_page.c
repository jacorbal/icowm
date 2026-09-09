/**
 * @file tests/menu/context/submenu/test_page.c
 *
 * @brief Test battery for the shared "Send to page" context menu
 *        submenu (menu/context/submenu/page.c)
 *
 * 'surface_viewport_has_room', 'surface_viewport_dims', and
 * 'scmd_surface_viewport_client_page' are test-controlled, letting a
 * scenario hand 'ctxmenu_submenu_page_build' any viewport grid and any
 * "client's current page" answer it wants.  'enact_client_send_to_page'
 * and 'enact_client_toggle_stick' are recording stand-ins: a scenario
 * can activate a built entry and check which one fired, and with what.
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
#include <client/predicates.h>
#include <client/state.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <harness/tap.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/submenu/page.h>
#include <surface.h>


/** Link-only stand-in for @a enact_client_send_to_page; records
 *  whether it fired and, if so, its own arguments
 * @note Complexity: @e O(1) */
static unsigned int s_call_send_to_page;
static client_td *s_last_sent_client;
static uint32_t s_last_sent_col;
static uint32_t s_last_sent_row;

void enact_client_send_to_page(surface_td *surface, client_td *client,
        uint32_t col, uint32_t row)
{
    (void) surface;

    s_call_send_to_page++;
    s_last_sent_client = client;
    s_last_sent_col = col;
    s_last_sent_row = row;
}


/** Link-only stand-in for @a enact_client_toggle_stick; records
 *  whether it fired and, if so, which client it was asked to toggle
 * @note Complexity: @e O(1) */
static unsigned int s_call_toggle_stick;
static client_td *s_last_toggled_stick_client;

void enact_client_toggle_stick(client_td *client)
{
    s_call_toggle_stick++;
    s_last_toggled_stick_client = client;
}


/** Test-controlled stand-in for @a surface_viewport_has_room
 * @note Complexity: @e O(1) */
static bool s_viewport_has_room = true;

bool surface_viewport_has_room(const surface_td *surface)
{
    (void) surface;

    return s_viewport_has_room;
}


/** Test-controlled viewport grid @a surface_viewport_dims reports
 * @note Complexity: @e O(1) */
static uint32_t s_viewport_columns = 1u;
static uint32_t s_viewport_rows = 1u;

void surface_viewport_dims(const surface_td *surface,
        uint32_t *columns_out, uint32_t *rows_out)
{
    (void) surface;

    *columns_out = s_viewport_columns;
    *rows_out = s_viewport_rows;
}


/** Test-controlled stand-in for @a scmd_surface_viewport_client_page,
 *  reporting whichever page a scenario last registered as the target
 *  client's own, or none at all
 * @note Complexity: @e O(1) */
static bool s_client_page_known;
static uint32_t s_client_page_col;
static uint32_t s_client_page_row;

bool scmd_surface_viewport_client_page(const surface_td *surface,
        const desktop_td *desktop, const client_td *client,
        uint32_t *out_col, uint32_t *out_row)
{
    (void) surface;
    (void) desktop;
    (void) client;

    *out_col = s_client_page_col;
    *out_row = s_client_page_row;
    return s_client_page_known;
}


static void s_reset(void)
{
    s_call_send_to_page = 0u;
    s_last_sent_client = NULL;
    s_last_sent_col = 0u;
    s_last_sent_row = 0u;
    s_call_toggle_stick = 0u;
    s_last_toggled_stick_client = NULL;
    s_viewport_has_room = true;
    s_viewport_columns = 1u;
    s_viewport_rows = 1u;
    s_client_page_known = false;
    s_client_page_col = 0u;
    s_client_page_row = 0u;
}


/* A null argument, in any position, builds nothing */
static void s_test_null_guards(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td client;
    ctxmenu_entry_td *entries = (ctxmenu_entry_td *) 1;
    ctxmenu_state_td *state = (ctxmenu_state_td *) 1;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    s_viewport_columns = 2u;
    s_viewport_rows = 2u;

    TAP_EQ_INT(ctxmenu_submenu_page_build(NULL, &desktop, &client,
                &entries, &state), 0, "a null surface builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_page_build(&surface, NULL, &client,
                &entries, &state), 0, "a null desktop builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_page_build(&surface, &desktop, NULL,
                &entries, &state), 0, "a null client builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_page_build(&surface, &desktop, &client,
                NULL, &state), 0, "a null out_entries builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_page_build(&surface, &desktop, &client,
                &entries, NULL), 0, "a null out_state builds nothing");
}


/* A viewport that cannot pan at all, or a 1x1 grid, has nowhere to
 * send anything, so the whole submenu is omitted */
static void s_test_no_room_or_single_page_builds_nothing(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td client;
    ctxmenu_entry_td *entries = NULL;
    ctxmenu_state_td *state = NULL;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    s_viewport_has_room = false;

    TAP_EQ_INT(ctxmenu_submenu_page_build(&surface, &desktop, &client,
                &entries, &state), 0,
            "a viewport that cannot pan builds nothing");

    s_viewport_has_room = true;
    s_viewport_columns = 1u;
    s_viewport_rows = 1u;

    TAP_EQ_INT(ctxmenu_submenu_page_build(&surface, &desktop, &client,
                &entries, &state), 0,
            "a 1x1 grid builds nothing even if room is reported");
}


/* An unstuck client on a 2x2 grid gets one row per page, its own
 * current page refused, a separator, and a sticky toggle offering to
 * stick */
static void s_test_unstuck_client_2x2_grid(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td client;
    ctxmenu_entry_td *e = NULL;
    ctxmenu_state_td *state = NULL;
    int n;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    s_viewport_columns = 2u;
    s_viewport_rows = 2u;
    s_client_page_known = true;
    s_client_page_col = 1u;
    s_client_page_row = 0u;

    n = ctxmenu_submenu_page_build(&surface, &desktop, &client, &e,
            &state);

    TAP_EQ_INT(n, 6,
            "a 2x2 grid yields 4 page rows, a separator, and the"
            " sticky toggle");
    TAP_OK(state != NULL && state->entries == e && state->entry_count
                == n,
            "out_state wraps the same entries and count");
    TAP_OK(e[1].is_disabled,
            "the page the client already sits on is refused");
    TAP_OK(!e[0].is_disabled, "every other page stays selectable");
    TAP_EQ_INT((int) e[4].type, (int) CTXMENU_SEPARATOR,
            "a separator closes the page list");
    TAP_EQ_STR(e[5].label, "All pages (sticky)",
            "an unstuck client is offered to stick");
    TAP_OK(!e[5].is_disabled, "the sticky toggle stays live");

    e[3].on_activate((xcb_connection_t *) 1, e[3].userdata);
    TAP_EQ_INT(s_call_send_to_page, 1,
            "activating a page row sends the client exactly once");
    TAP_OK(s_last_sent_client == &client && s_last_sent_col == 1u &&
                s_last_sent_row == 1u,
            "...to the page that row names, in row-major order");

    e[5].on_activate((xcb_connection_t *) 1, e[5].userdata);
    TAP_EQ_INT(s_call_toggle_stick, 1,
            "activating the sticky toggle fires exactly once");
    TAP_OK(s_last_toggled_stick_client == &client,
            "...for the target client");
}


/* A sticky client has every page row refused, and the trailing toggle
 * relabeled as an active unsticky action instead */
static void s_test_sticky_client_relabels_toggle(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td client;
    ctxmenu_entry_td *e = NULL;
    ctxmenu_state_td *state = NULL;
    int n;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    s_viewport_columns = 2u;
    s_viewport_rows = 2u;
    client.properties.flags |= (uint16_t) CLIENT_FLAG_STICKY;

    n = ctxmenu_submenu_page_build(&surface, &desktop, &client, &e,
            &state);

    TAP_OK(e[0].is_disabled && e[1].is_disabled && e[2].is_disabled &&
                e[3].is_disabled,
            "every page is refused for a sticky client");
    TAP_EQ_STR(e[n - 1].label, "This page only (unsticky)",
            "the trailing toggle offers to unstick instead");
    TAP_OK(!e[n - 1].is_disabled, "leaving it the one live entry");
}


int main(void)
{
    TAP_PLAN(21);

    s_test_null_guards();
    s_test_no_room_or_single_page_builds_nothing();
    s_test_unstuck_client_2x2_grid();
    s_test_sticky_client_relabels_toggle();

    return TAP_DONE();
}
