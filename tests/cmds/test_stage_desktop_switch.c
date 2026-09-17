/**
 * @file tests/cmds/test_stage_desktop_switch.c
 *
 * @brief Test battery for stage desktop-switch commands
 *        (cmds/stage.c)
 *
 * Exercises 'scmd_stage_desktop_switch' directly, and its four
 * cyclic siblings ('_north', '_south', '_east', '_west') through
 * their shared static helper.  'stage_desktop_select' and the four
 * 'stage_desktop_select_north/south/east/west' functions
 * (stage.c) are test-controlled stand-ins, each answering whatever
 * outcome and 'desktop_cur' value the currently running scenario
 * registered beforehand, so every branch of the switch logic itself
 * (already switched away, selection failure, selection success) runs
 * without a real desktop list ever needing to exist.
 * 'stage_client_hide_all', 'stage_client_show_all', and
 * 'stage_client_pinned_transfer_all' are call-counting stand-ins,
 * letting each scenario assert on the exact hide/show/transfer
 * sequence the function under test is documented to follow.
 * 'xcb_connection_get' is a test-controlled stand-in too, since
 * 's_show_desktop_overlay' (a static helper, unreachable directly)
 * skips its own notification work entirely whenever it reports
 * 'NULL', which every scenario here relies on to avoid needing a real
 * notification popup or a live X connection.  'lookup_current_desktop'
 * is a link-only stand-in always reporting 'NULL', reached only on
 * the path where a real connection exists, deliberately never taken
 * by any scenario below.  'notify_desktop_show' is a link-only
 * stand-in for the same reason: unreachable once 'xcb_connection_get'
 * reports 'NULL'.  'ccmd_target_win', 'ccmd_client_apply_geometry',
 * 'stacking_walk', and 'scratchpad_notice_viewport_panned' are
 * link-only stand-ins too, needed only because 'cmds/stage.c' now
 * also contains viewport-panning code that references them; no
 * scenario here exercises panning, so none of the four is ever
 * actually reached
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
#include <types/direction.h>

/* Project includes */
#include <client.h>
#include <cmds/stage.h>
#include <desktop.h>
#include <harness/tap.h>
#include <logger.h>
#include <policy/stacking.h>
#include <scratchpad.h>
#include <stage.h>


/** Link-only stand-in for @a logger_msg (logger.c): every LOGGER_DEBUG
 *  call in cmds/stage.c reaches this, and this file asserts on
 *  nothing it would print
 *  @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;

    return 0;
}


/** Whichever connection 'xcb_connection_get' should currently report,
 *  set by 's_reset' before every scenario */
static xcb_connection_t *s_stub_connection;

/** Link-only stand-in for @a xcb_connection_get (utils/xcb/
 *  connection.c): every scenario here keeps this 'NULL', so
 *  's_show_desktop_overlay' always takes its own early-return branch
 *  @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return s_stub_connection;
}


/** Link-only stand-in for @a lookup_current_desktop (lookup.c):
 *  unreachable while 'xcb_connection_get' reports 'NULL', which every
 *  scenario here relies on
 *  @note Complexity: @e O(1)
 */
desktop_td *lookup_current_desktop(stage_td *stage)
{
    (void) stage;
    return NULL;
}


/** Link-only stand-in for @a wm_get_client_desktop: reached only from
 *  the centring this file never exercises, whose own current-desktop
 *  lookup above already answers @c NULL
 *  @note Complexity: @e O(1) */
desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;

    return NULL;
}


/** Link-only stand-in for @a notify_desktop_show (menu/notify/
 *  desktop.c): unreachable for the same reason as
 *  'lookup_current_desktop' above
 *  @note Complexity: @e O(1)
 */
void notify_desktop_show(xcb_connection_t *connection,
        stage_td *stage, uint32_t desktop_idx,
        const char *desktop_name, const config_td *config)
{
    (void) connection;
    (void) stage;
    (void) desktop_idx;
    (void) desktop_name;
    (void) config;
}


/** Link-only stand-in for @a ccmd_target_win (cmds/client/screen.c):
 *  unreachable here, since no scenario below ever exercises viewport
 *  panning
 *  @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client)
{
    (void) client;
    return (xcb_window_t) 1;
}


/** Link-only stand-in for @a ccmd_client_apply_geometry (cmds/
 *  client/move.c): unreachable for the same reason as
 *  'ccmd_target_win' above
 *  @note Complexity: @e O(1)
 */
void ccmd_client_apply_geometry(client_td *client,
        xcb_window_t target, uint16_t mask,
        int32_t x, int32_t y, uint32_t w, uint32_t h,
        uint32_t border_width)
{
    (void) client;
    (void) target;
    (void) mask;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    (void) border_width;
}


/** Link-only stand-in for @a stacking_walk (policy/stacking.c):
 *  unreachable for the same reason as 'ccmd_target_win' above
 *  @note Complexity: @e O(1)
 */
void stacking_walk(const desktop_td *desktop, stacking_visitor_fn visit,
        void *data)
{
    (void) desktop;
    (void) visit;
    (void) data;
}


/** Link-only stand-in for @a scratchpad_notice_viewport_panned
 *  (scratchpad.c): unreachable for the same reason as
 *  'ccmd_target_win' above
 *  @note Complexity: @e O(1)
 */
void scratchpad_notice_viewport_panned(const desktop_td *desktop)
{
    (void) desktop;
}


/** Outcome 'stage_desktop_select' should report next, and the
 *  'desktop_cur' it should leave behind on success, both set by each
 *  scenario before calling the function under test */
static int s_select_result;
static uint32_t s_select_new_id;
static int s_call_select;

/** Test-controlled stand-in for @a stage_desktop_select
 *  (stage.c)
 *  @note Complexity: @e O(1)
 */
int stage_desktop_select(stage_td *stage, uint32_t desktop_id)
{
    (void) desktop_id;

    s_call_select++;
    if (s_select_result == 0) {
        stage->desktop_cur = s_select_new_id;
    }
    return s_select_result;
}


/** Outcome and destination each of the four directional stand-ins
 *  below should report, one pair per direction, all set by each
 *  scenario before calling the function under test */
static int s_select_north_result;
static uint32_t s_select_north_new_id;
static int s_call_select_north;
static int s_select_south_result;
static uint32_t s_select_south_new_id;
static int s_call_select_south;
static int s_select_east_result;
static uint32_t s_select_east_new_id;
static int s_call_select_east;
static int s_select_west_result;
static uint32_t s_select_west_new_id;
static int s_call_select_west;

/** Test-controlled stand-in for @a stage_desktop_select_north
 *  (stage.c)
 *  @note Complexity: @e O(1)
 */
int stage_desktop_select_north(stage_td *stage, bool cycle)
{
    (void) cycle;

    s_call_select_north++;
    if (s_select_north_result == 0) {
        stage->desktop_cur = s_select_north_new_id;
    }
    return s_select_north_result;
}


/** Test-controlled stand-in for @a stage_desktop_select_south
 *  (stage.c)
 *  @note Complexity: @e O(1)
 */
int stage_desktop_select_south(stage_td *stage, bool cycle)
{
    (void) cycle;

    s_call_select_south++;
    if (s_select_south_result == 0) {
        stage->desktop_cur = s_select_south_new_id;
    }
    return s_select_south_result;
}


/** Test-controlled stand-in for @a stage_desktop_select_east
 *  (stage.c)
 *  @note Complexity: @e O(1)
 */
int stage_desktop_select_east(stage_td *stage, bool cycle)
{
    (void) cycle;

    s_call_select_east++;
    if (s_select_east_result == 0) {
        stage->desktop_cur = s_select_east_new_id;
    }
    return s_select_east_result;
}


/** Test-controlled stand-in for @a stage_desktop_select_west
 *  (stage.c)
 *  @note Complexity: @e O(1)
 */
int stage_desktop_select_west(stage_td *stage, bool cycle)
{
    (void) cycle;

    s_call_select_west++;
    if (s_select_west_result == 0) {
        stage->desktop_cur = s_select_west_new_id;
    }
    return s_select_west_result;
}


/** Call counters for the three client-visibility primitives, and the
 *  desktop id each was last called with, all reset by 's_reset' */
static int s_call_hide;
static uint32_t s_last_hide_id;
static int s_call_show;
static uint32_t s_last_show_id;
static int s_call_pinned_transfer;
static uint32_t s_last_pinned_transfer_id;

/** Call-counting stand-in for @a stage_client_hide_all (stage/
 *  actions/clients.c)
 *  @note Complexity: @e O(1)
 */
void stage_client_hide_all(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;

    s_call_hide++;
    s_last_hide_id = desktop_id;
}


/** Call-counting stand-in for @a stage_client_show_all (stage/
 *  actions/clients.c)
 *  @note Complexity: @e O(1)
 */
void stage_client_show_all(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;

    s_call_show++;
    s_last_show_id = desktop_id;
}


/** Call-counting stand-in for @a stage_client_pinned_transfer_all
 *  (stage/actions/client.c)
 *  @note Complexity: @e O(1)
 */
void stage_client_pinned_transfer_all(stage_td *stage,
        uint32_t to_id)
{
    (void) stage;

    s_call_pinned_transfer++;
    s_last_pinned_transfer_id = to_id;
}


static void s_reset(void)
{
    s_stub_connection = NULL;
    s_select_result = 0;
    s_select_new_id = 0u;
    s_call_select = 0;
    s_select_north_result = 0;
    s_select_north_new_id = 0u;
    s_call_select_north = 0;
    s_select_south_result = 0;
    s_select_south_new_id = 0u;
    s_call_select_south = 0;
    s_select_east_result = 0;
    s_select_east_new_id = 0u;
    s_call_select_east = 0;
    s_select_west_result = 0;
    s_select_west_new_id = 0u;
    s_call_select_west = 0;
    s_call_hide = 0;
    s_last_hide_id = 0u;
    s_call_show = 0;
    s_last_show_id = 0u;
    s_call_pinned_transfer = 0;
    s_last_pinned_transfer_id = 0u;
}


static stage_td *s_make_stage(uint32_t desktop_cur)
{
    stage_td *stage = calloc(1, sizeof(*stage));

    stage->id = 0u;
    stage->desktop_cur = desktop_cur;
    stage->config = NULL;
    stage->is_outdated = false;
    return stage;
}


/* A null stage is refused outright by 'scmd_stage_desktop_switch' */
static void s_test_switch_null_stage(void)
{
    s_reset();

    scmd_stage_desktop_switch(NULL, 3u);
    TAP_OK(s_call_select == 0,
            "a null stage never reaches stage_desktop_select");
}


/* Requesting the desktop already current is a silent no-op: neither
 * hide, select, nor show ever runs */
static void s_test_switch_already_current_is_noop(void)
{
    stage_td *stage = s_make_stage(2u);

    s_reset();

    scmd_stage_desktop_switch(stage, 2u);
    TAP_OK(s_call_hide == 0,
            "switching to the already-current desktop never hides"
            " anything");
    TAP_OK(s_call_select == 0,
            "switching to the already-current desktop never calls"
            " stage_desktop_select");
    TAP_EQ_INT((int) stage->desktop_cur, 2,
            "desktop_cur is left untouched");

    free(stage);
}


/* A failed selection restores visibility on the old desktop and
 * leaves desktop_cur alone */
static void s_test_switch_selection_fails_restores_old(void)
{
    stage_td *stage = s_make_stage(1u);

    s_reset();
    s_select_result = 1;   /* No such desktop found */

    scmd_stage_desktop_switch(stage, 9u);
    TAP_EQ_INT(s_call_hide, 1,
            "the old desktop is hidden once before attempting the"
            " switch");
    TAP_EQ_INT((int) s_last_hide_id, 1,
            "the desktop hidden is the old one");
    TAP_EQ_INT(s_call_select, 1,
            "stage_desktop_select is attempted exactly once");
    TAP_EQ_INT(s_call_show, 1,
            "a failed selection restores visibility exactly once");
    TAP_EQ_INT((int) s_last_show_id, 1,
            "the desktop restored to view is the old one, not the"
            " requested target");
    TAP_EQ_INT(s_call_pinned_transfer, 0,
            "a failed selection never transfers pinned clients");
    TAP_EQ_INT((int) stage->desktop_cur, 1,
            "desktop_cur is left at the old value after a failed"
            " switch");
    TAP_OK(!stage->is_outdated,
            "a failed switch never marks the stage outdated");

    free(stage);
}


/* A successful selection hides the old desktop, transfers pinned
 * clients, shows the new one, and marks the stage outdated */
static void s_test_switch_success_full_sequence(void)
{
    stage_td *stage = s_make_stage(0u);

    s_reset();
    s_select_result = 0;
    s_select_new_id = 5u;

    scmd_stage_desktop_switch(stage, 5u);
    TAP_EQ_INT(s_call_hide, 1, "the old desktop is hidden once");
    TAP_EQ_INT((int) s_last_hide_id, 0, "hidden desktop is the old one");
    TAP_EQ_INT(s_call_select, 1,
            "stage_desktop_select is called exactly once");
    TAP_EQ_INT(s_call_pinned_transfer, 1,
            "pinned clients are transferred exactly once on success");
    TAP_EQ_INT((int) s_last_pinned_transfer_id, 5,
            "pinned clients are transferred to the new desktop");
    TAP_EQ_INT(s_call_show, 1, "the new desktop is shown exactly once");
    TAP_EQ_INT((int) s_last_show_id, 5, "the desktop shown is the new one");
    TAP_EQ_INT((int) stage->desktop_cur, 5,
            "desktop_cur reflects the new desktop");
    TAP_OK(stage->is_outdated,
            "a successful switch marks the stage outdated");

    free(stage);
}


/* A null stage is refused outright by every cyclic direction too */
static void s_test_cyclic_null_stage(void)
{
    s_reset();

    scmd_stage_desktop_switch_north(NULL);
    scmd_stage_desktop_switch_south(NULL);
    scmd_stage_desktop_switch_east(NULL);
    scmd_stage_desktop_switch_west(NULL);
    TAP_OK(s_call_select_north == 0 && s_call_select_south == 0 &&
            s_call_select_east == 0 && s_call_select_west == 0,
            "a null stage never reaches any of the four directional"
            " selectors");
}


/* North: a successful cyclic switch hides, transfers, shows, and
 * marks the stage outdated */
static void s_test_cyclic_north_success(void)
{
    stage_td *stage = s_make_stage(0u);

    s_reset();
    s_select_north_result = 0;
    s_select_north_new_id = 3u;

    scmd_stage_desktop_switch_north(stage);
    TAP_EQ_INT(s_call_select_north, 1,
            "north dispatches to stage_desktop_select_north exactly"
            " once");
    TAP_EQ_INT(s_call_select_south + s_call_select_east +
            s_call_select_west, 0,
            "no other direction's selector is ever called");
    TAP_EQ_INT((int) stage->desktop_cur, 3,
            "desktop_cur reflects the new desktop after switching north");
    TAP_EQ_INT(s_call_pinned_transfer, 1,
            "a successful cyclic switch transfers pinned clients");
    TAP_OK(stage->is_outdated,
            "a successful cyclic switch marks the stage outdated");

    free(stage);
}


/* North: a failed cyclic switch (no desktop found that way) restores
 * visibility on the old desktop and leaves it unmarked */
static void s_test_cyclic_north_failure_restores(void)
{
    stage_td *stage = s_make_stage(4u);

    s_reset();
    s_select_north_result = 1;   /* No desktop to the north */

    scmd_stage_desktop_switch_north(stage);
    TAP_EQ_INT((int) stage->desktop_cur, 4,
            "desktop_cur is unchanged when there is no desktop to the"
            " north");
    TAP_EQ_INT(s_call_show, 1,
            "visibility is restored on the old desktop exactly once");
    TAP_EQ_INT((int) s_last_show_id, 4,
            "the desktop restored to view is the current one");
    TAP_EQ_INT(s_call_pinned_transfer, 0,
            "a failed cyclic switch never transfers pinned clients");
    TAP_OK(!stage->is_outdated,
            "a failed cyclic switch never marks the stage outdated");

    free(stage);
}


/* South, east, and west each dispatch to their own selector alone */
static void s_test_cyclic_south_dispatches_correctly(void)
{
    stage_td *stage = s_make_stage(0u);

    s_reset();
    s_select_south_result = 0;
    s_select_south_new_id = 7u;

    scmd_stage_desktop_switch_south(stage);
    TAP_EQ_INT(s_call_select_south, 1,
            "south dispatches to stage_desktop_select_south exactly"
            " once");
    TAP_EQ_INT(s_call_select_north + s_call_select_east +
            s_call_select_west, 0,
            "no other direction's selector runs for a south switch");
    TAP_EQ_INT((int) stage->desktop_cur, 7,
            "desktop_cur reflects the south destination");

    free(stage);
}


static void s_test_cyclic_east_dispatches_correctly(void)
{
    stage_td *stage = s_make_stage(0u);

    s_reset();
    s_select_east_result = 0;
    s_select_east_new_id = 8u;

    scmd_stage_desktop_switch_east(stage);
    TAP_EQ_INT(s_call_select_east, 1,
            "east dispatches to stage_desktop_select_east exactly"
            " once");
    TAP_EQ_INT(s_call_select_north + s_call_select_south +
            s_call_select_west, 0,
            "no other direction's selector runs for an east switch");
    TAP_EQ_INT((int) stage->desktop_cur, 8,
            "desktop_cur reflects the east destination");

    free(stage);
}


static void s_test_cyclic_west_dispatches_correctly(void)
{
    stage_td *stage = s_make_stage(0u);

    s_reset();
    s_select_west_result = 0;
    s_select_west_new_id = 9u;

    scmd_stage_desktop_switch_west(stage);
    TAP_EQ_INT(s_call_select_west, 1,
            "west dispatches to stage_desktop_select_west exactly"
            " once");
    TAP_EQ_INT(s_call_select_north + s_call_select_south +
            s_call_select_east, 0,
            "no other direction's selector runs for a west switch");
    TAP_EQ_INT((int) stage->desktop_cur, 9,
            "desktop_cur reflects the west destination");

    free(stage);
}


/* A live connection but no config on the stage still takes the
 * overlay's own early-return branch, rather than crashing on a null
 * config dereference; exercises 's_show_desktop_overlay' with
 * a non-null connection while keeping 'stage->config' itself null */
static void s_test_switch_success_with_connection_no_config(void)
{
    stage_td *stage = s_make_stage(0u);
    int fake_connection_storage = 0;

    s_reset();
    s_stub_connection = (xcb_connection_t *) &fake_connection_storage;
    s_select_result = 0;
    s_select_new_id = 2u;

    scmd_stage_desktop_switch(stage, 2u);
    TAP_OK(stage->is_outdated,
            "a switch still succeeds when a connection exists but"
            " the stage carries no config at all");

    free(stage);
}


int main(void)
{
    TAP_PLAN(42);

    s_test_switch_null_stage();
    s_test_switch_already_current_is_noop();
    s_test_switch_selection_fails_restores_old();
    s_test_switch_success_full_sequence();
    s_test_cyclic_null_stage();
    s_test_cyclic_north_success();
    s_test_cyclic_north_failure_restores();
    s_test_cyclic_south_dispatches_correctly();
    s_test_cyclic_east_dispatches_correctly();
    s_test_cyclic_west_dispatches_correctly();
    s_test_switch_success_with_connection_no_config();

    return TAP_DONE();
}
