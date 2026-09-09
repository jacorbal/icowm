/**
 * @file tests/menu/context/submenu/test_desktop.c
 *
 * @brief Test battery for the shared "Send to desktop" context menu
 *        submenu (menu/context/submenu/desktop.c)
 *
 * 'surface_desktops_walk' is test-controlled, walking a small fixture
 * array instead of a real circular desktop list, so a scenario can
 * hand 'ctxmenu_submenu_desktop_build' exactly the desktops it wants
 * to see enumerated.  'surface_desktop_label' is also test-controlled,
 * producing a simple, predictable label instead of the real localized
 * one.  'enact_desktop_client_send' and 'enact_client_toggle_pin' are
 * recording stand-ins: a scenario can activate a built entry and
 * check which one fired, and with what.
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
#include <client/state.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <harness/tap.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/submenu/desktop.h>
#include <surface.h>


/** Link-only stand-in for @a enact_desktop_client_send; records
 *  whether it fired and, if so, its own three arguments
 * @note Complexity: @e O(1) */
static unsigned int s_call_send_to_desktop;
static const desktop_td *s_last_sent_src;
static client_td *s_last_sent_client;
static desktop_td *s_last_sent_dst;

void enact_desktop_client_send(const desktop_td *desktop,
        client_td *client, desktop_td *target)
{
    s_call_send_to_desktop++;
    s_last_sent_src = desktop;
    s_last_sent_client = client;
    s_last_sent_dst = target;
}


/** Link-only stand-in for @a enact_client_toggle_pin; records whether
 *  it fired and, if so, which client it was asked to toggle
 * @note Complexity: @e O(1) */
static unsigned int s_call_toggle_pin;
static client_td *s_last_toggled_pin_client;

void enact_client_toggle_pin(client_td *client)
{
    s_call_toggle_pin++;
    s_last_toggled_pin_client = client;
}


/** Test-controlled desktop fixture list 'surface_desktops_walk' below
 *  walks, in order, instead of a real circular desktop list
 * @note Complexity: @e O(1) */
#define MAX_TEST_DESKTOPS (4)
static desktop_td *s_desktops[MAX_TEST_DESKTOPS];
static int s_desktop_count;

/** Test-controlled stand-in for @a surface_desktops_walk, walking the
 *  fixture list above instead of a real surface's own desktops
 * @note Complexity: @e O(n), where @e n is @c s_desktop_count */
void surface_desktops_walk(const surface_td *surface,
        void (*visit)(desktop_td *desktop, void *data), void *data)
{
    (void) surface;

    for (int i = 0; i < s_desktop_count; ++i) {
        visit(s_desktops[i], data);
    }
}


/** Test-controlled stand-in for @a surface_desktop_label, producing a
 *  simple, predictable label instead of the real localized one
 * @note Complexity: @e O(1) */
void surface_desktop_label(const surface_td *surface,
        uint32_t desktop_id, const char *desktop_name, bool is_pinned,
        bool shows_name, char *out_label, size_t length)
{
    (void) surface;
    (void) is_pinned;
    (void) shows_name;

    (void) snprintf(out_label, length, "%u:%s", desktop_id,
            (desktop_name != NULL) ? desktop_name : "");
}


static void s_reset(void)
{
    s_desktop_count = 0;
    s_call_send_to_desktop = 0u;
    s_last_sent_src = NULL;
    s_last_sent_client = NULL;
    s_last_sent_dst = NULL;
    s_call_toggle_pin = 0u;
    s_last_toggled_pin_client = NULL;
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
    surface.desktop_count = 2u;

    TAP_EQ_INT(ctxmenu_submenu_desktop_build(NULL, &desktop, &client,
                &entries, &state), 0, "a null surface builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_desktop_build(&surface, NULL, &client,
                &entries, &state), 0, "a null desktop builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_desktop_build(&surface, &desktop, NULL,
                &entries, &state), 0, "a null client builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_desktop_build(&surface, &desktop,
                &client, NULL, &state), 0,
            "a null out_entries builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_desktop_build(&surface, &desktop,
                &client, &entries, NULL), 0,
            "a null out_state builds nothing");
}


/* A surface with only one desktop has nowhere to send a client, so
 * the whole submenu is omitted rather than built with nothing but
 * the pin toggle in it */
static void s_test_single_desktop_builds_nothing(void)
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
    surface.desktop_count = 1u;

    TAP_EQ_INT(ctxmenu_submenu_desktop_build(&surface, &desktop,
                &client, &entries, &state), 0,
            "a single-desktop surface builds nothing");
    TAP_NULL(entries, "out_entries is left untouched");
    TAP_NULL(state, "out_state is left untouched");
}


/* An unpinned client on a two-desktop surface gets one row per
 * desktop, its own current desktop refused, a separator, and a pin
 * toggle offering to pin */
static void s_test_unpinned_client_two_desktops(void)
{
    surface_td surface;
    desktop_td desktop_a;
    desktop_td desktop_b;
    client_td client;
    ctxmenu_entry_td *e = NULL;
    ctxmenu_state_td *state = NULL;
    int n;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop_a, 0, sizeof(desktop_a));
    memset(&desktop_b, 0, sizeof(desktop_b));
    memset(&client, 0, sizeof(client));
    desktop_a.id = 0u;
    desktop_b.id = 1u;
    s_desktops[0] = &desktop_a;
    s_desktops[1] = &desktop_b;
    s_desktop_count = 2;
    surface.desktop_count = 2u;

    n = ctxmenu_submenu_desktop_build(&surface, &desktop_a, &client,
            &e, &state);

    TAP_EQ_INT(n, 4,
            "two desktops yield 2 rows, a separator, and the pin"
            " toggle");
    TAP_NOT_NULL(e, "out_entries points at real storage");
    TAP_OK(state != NULL && state->entries == e && state->entry_count
                == n,
            "out_state wraps the same entries and count");
    TAP_OK(e[0].is_disabled,
            "the client's own current desktop is refused");
    TAP_OK(!e[1].is_disabled, "the other desktop stays selectable");
    TAP_EQ_INT((int) e[2].type, (int) CTXMENU_SEPARATOR,
            "a separator follows the desktop rows");
    TAP_EQ_STR(e[3].label, "All desktops (pin)",
            "an unpinned client is offered to pin");
    TAP_OK(!e[3].is_disabled, "the pin toggle stays live");

    e[1].on_activate((xcb_connection_t *) 1, e[1].userdata);
    TAP_EQ_INT(s_call_send_to_desktop, 1,
            "activating a desktop row sends the client exactly once");
    TAP_OK(s_last_sent_src == &desktop_a && s_last_sent_dst ==
                &desktop_b && s_last_sent_client == &client,
            "...to the desktop that row names, from the current one");

    e[3].on_activate((xcb_connection_t *) 1, e[3].userdata);
    TAP_EQ_INT(s_call_toggle_pin, 1,
            "activating the pin toggle fires exactly once");
    TAP_OK(s_last_toggled_pin_client == &client,
            "...for the target client");
}


/* A pinned client has every desktop row refused, and the trailing
 * toggle relabeled as an active unpin action instead */
static void s_test_pinned_client_relabels_toggle(void)
{
    surface_td surface;
    desktop_td desktop_a;
    desktop_td desktop_b;
    client_td client;
    ctxmenu_entry_td *e = NULL;
    ctxmenu_state_td *state = NULL;
    int n;

    s_reset();
    memset(&surface, 0, sizeof(surface));
    memset(&desktop_a, 0, sizeof(desktop_a));
    memset(&desktop_b, 0, sizeof(desktop_b));
    memset(&client, 0, sizeof(client));
    desktop_a.id = 0u;
    desktop_b.id = 1u;
    s_desktops[0] = &desktop_a;
    s_desktops[1] = &desktop_b;
    s_desktop_count = 2;
    surface.desktop_count = 2u;
    client.properties.flags |= (uint16_t) CLIENT_FLAG_PIN;

    n = ctxmenu_submenu_desktop_build(&surface, &desktop_a, &client,
            &e, &state);

    TAP_OK(e[0].is_disabled && e[1].is_disabled,
            "every desktop is refused for a pinned client");
    TAP_EQ_STR(e[n - 1].label, "This desktop only (unpin)",
            "the trailing toggle offers to unpin instead");
    TAP_OK(!e[n - 1].is_disabled, "leaving it the one live entry");
}


int main(void)
{
    TAP_PLAN(23);

    s_test_null_guards();
    s_test_single_desktop_builds_nothing();
    s_test_unpinned_client_two_desktops();
    s_test_pinned_client_relabels_toggle();

    return TAP_DONE();
}
