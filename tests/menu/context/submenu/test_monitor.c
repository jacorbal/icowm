/**
 * @file tests/menu/context/submenu/test_monitor.c
 *
 * @brief Test battery for the shared "Send to monitor" context menu
 *        submenu (menu/context/submenu/monitor.c)
 *
 * 'stage_monitor_for_point' is test-controlled, answering whichever
 * monitor a scenario registers as "current" via 's_current_monitor',
 * instead of resolving one from a real monitor rectangle list.
 * 'enact_client_move_to_monitor' is a recording stand-in: a scenario
 * can activate a built entry and check which client and monitor index
 * it fired with.
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
#include <desktop.h>
#include <enact.h>
#include <harness/tap.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/submenu/monitor.h>
#include <stage.h>


/** Test-controlled stand-in for @a stage_monitor_for_point,
 *  answering whichever monitor this file last registered as
 *  "current" via @a s_current_monitor
 * @note Complexity: @e O(1) */
static monitor_td s_current_monitor;

monitor_td stage_monitor_for_point(const stage_td *stage,
        struct position_s pos)
{
    (void) stage;
    (void) pos;
    return s_current_monitor;
}


/** Recording stand-in for @a enact_client_move_to_monitor; records
 *  whether it fired and, if so, its own two arguments
 * @note Complexity: @e O(1) */
static unsigned int s_call_move_to_monitor;
static client_td *s_last_moved_client;
static uint32_t s_last_moved_index;

void enact_client_move_to_monitor(client_td *client,
        uint32_t monitor_index)
{
    s_call_move_to_monitor++;
    s_last_moved_client = client;
    s_last_moved_index = monitor_index;
}


static void s_reset(void)
{
    memset(&s_current_monitor, 0, sizeof(s_current_monitor));
    s_call_move_to_monitor = 0u;
    s_last_moved_client = NULL;
    s_last_moved_index = 0u;
}


/* A null argument, in any position, builds nothing */
static void s_test_null_guards(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    ctxmenu_entry_td *entries = (ctxmenu_entry_td *) 1;
    ctxmenu_state_td *state = (ctxmenu_state_td *) 1;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    stage.monitor_count = 2u;

    TAP_EQ_INT(ctxmenu_submenu_monitor_build(NULL, &desktop, &client,
                &entries, &state), 0, "a null stage builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_monitor_build(&stage, &desktop, NULL,
                &entries, &state), 0, "a null client builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_monitor_build(&stage, &desktop,
                &client, NULL, &state), 0,
            "a null out_entries builds nothing");
    TAP_EQ_INT(ctxmenu_submenu_monitor_build(&stage, &desktop,
                &client, &entries, NULL), 0,
            "a null out_state builds nothing");
}


/* A stage with only one monitor has nowhere to send a client, so
 * the whole submenu is omitted rather than built with a single,
 * always-refused row in it */
static void s_test_single_monitor_builds_nothing(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    ctxmenu_entry_td *entries = NULL;
    ctxmenu_state_td *state = NULL;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    stage.monitor_count = 1u;

    TAP_EQ_INT(ctxmenu_submenu_monitor_build(&stage, &desktop,
                &client, &entries, &state), 0,
            "a single-monitor stage builds nothing");
    TAP_NULL(entries, "out_entries is left untouched");
    TAP_NULL(state, "out_state is left untouched");
}


/* A client on a two-monitor stage gets one row per monitor, its own
 * current monitor refused, the other one selectable and, once
 * activated, sending the client to that monitor's index */
static void s_test_two_monitors_one_row_each(void)
{
    stage_td stage;
    desktop_td desktop;
    client_td client;
    ctxmenu_entry_td *e = NULL;
    ctxmenu_state_td *state = NULL;
    int n;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    stage.monitor_count = 2u;
    stage.monitors[0] = (monitor_td) { 0, 0, 1920u, 1080u };
    stage.monitors[1] = (monitor_td) { 1920, 0, 1920u, 1080u };
    stage.primary_monitor_index = 0u;
    s_current_monitor = stage.monitors[0];

    n = ctxmenu_submenu_monitor_build(&stage, &desktop, &client,
            &e, &state);

    TAP_EQ_INT(n, 2, "two monitors yield one row each");
    TAP_NOT_NULL(e, "out_entries points at real storage");
    TAP_OK(state != NULL && state->entries == e && state->entry_count
                == n,
            "out_state wraps the same entries and count");
    TAP_OK(e[0].is_disabled,
            "the client's own current monitor is refused");
    TAP_OK(!e[1].is_disabled, "the other monitor stays selectable");

    e[1].on_activate((xcb_connection_t *) 1, e[1].userdata);
    TAP_EQ_INT(s_call_move_to_monitor, 1,
            "activating a monitor row moves the client exactly once");
    TAP_OK(s_last_moved_client == &client && s_last_moved_index == 1u,
            "...to the monitor index that row names");
}


int main(void)
{
    TAP_PLAN(14);

    s_test_null_guards();
    s_test_single_monitor_builds_nothing();
    s_test_two_monitors_one_row_each();

    return TAP_DONE();
}
