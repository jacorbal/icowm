/**
 * @file tests/menu/context/test_ctxmenu.c
 *
 * @brief Test battery for the context menu window lifecycle
 *        (menu/context/ctxmenu.c)
 *
 * 'ctxmenu_show' only ever reaches an X server past its own guard
 * clause: every argument it accepts before that point (a null
 * 'connection', 'stage', 'state', or 'config', a null
 * 'state->entries', or a non-positive 'state->entry_count') is
 * checked here, since none of those five paths ever calls
 * 'xcb_generate_id' or touches the server at all.  Once past that
 * guard the real body allocates a window, computes layout with real
 * font metrics, and grabs the keyboard and pointer, none of which
 * this file can exercise without a live display.
 *
 * 'ctxmenu_close' is reached in full here instead: its own
 * 'xcb_window_destroy' and grab-release calls are both gated behind
 * 'xcb_connection_get() != NULL', and this file links the real
 * 'utils/xcb/connection.c' without ever calling 'xcb_connection_set',
 * so that getter answers null throughout, exactly as it does before
 * 'wm_start' opens a connection.  Under that condition 'ctxmenu_close'
 * takes its guard clause, its child-chain recursion, and its full set
 * of field resets, none of them stubbed away, with only the two
 * X-bound branches genuinely skipped.
 *
 * 'ctxmenu_is_open' has no X dependency at all and is exercised in
 * full.
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
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <config.h>
#include <harness/tap.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/layout.h>
#include <stage.h>
#include <utils/xcb/atom.h>


/**
 * @brief Link-only stand-in for @a xcb_window_destroy
 *
 * Reached only when 'xcb_connection_get' answers non-null, which
 * never happens in this file, since 'xcb_connection_set' is never
 * called.
 *
 * @note Complexity: @e O(1)
 */
void xcb_window_destroy(xcb_window_t window)
{
    (void) window;
}


/**
 * @brief Link-only stand-in for @a ctxmenu_width_compute
 *
 * Reached only past 'ctxmenu_show''s guard clause, on a live
 * connection this file never has.
 *
 * @note Complexity: @e O(1)
 */
uint16_t ctxmenu_width_compute(xcb_connection_t *connection,
        const ctxmenu_entry_td *entries, int entry_count,
        const config_td *config)
{
    (void) connection;
    (void) entries;
    (void) entry_count;
    (void) config;
    return 0;
}


/**
 * @brief Link-only stand-in for @a ctxmenu_layout_build
 *
 * Reached only past 'ctxmenu_show''s guard clause, on a live
 * connection this file never has.
 *
 * @note Complexity: @e O(1)
 */
uint16_t ctxmenu_layout_build(ctxmenu_state_td *state)
{
    (void) state;
    return 0;
}


/**
 * @brief Link-only stand-in for @a stage_desktop_get
 *
 * Reached only past 'ctxmenu_show''s guard clause, on a live
 * connection this file never has.
 *
 * @note Complexity: @e O(1)
 */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a config_theme_opacity_to_raw
 *
 * Reached only past 'ctxmenu_show''s guard clause, on a live
 * connection this file never has.
 *
 * @note Complexity: @e O(1)
 */
uint32_t config_theme_opacity_to_raw(uint8_t percent)
{
    (void) percent;
    return 0;
}


/**
 * @brief Link-only stand-in for @a atom_set_window_opacity
 *
 * Reached only past 'ctxmenu_show''s guard clause, on a live
 * connection this file never has.
 *
 * @note Complexity: @e O(1)
 */
void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw)
{
    (void) connection;
    (void) window;
    (void) raw;
}


/**
 * @brief Verify every guard clause that keeps @a ctxmenu_show from
 *        touching the server on bad input
 */
static void s_test_show_guards(void)
{
    ctxmenu_entry_td entries[1];
    ctxmenu_state_td state;
    stage_td stage;
    config_td config;
    struct position_s pos = { 0, 0 };

    memset(&entries, 0, sizeof(entries));
    memset(&state, 0, sizeof(state));
    memset(&stage, 0, sizeof(stage));
    memset(&config, 0, sizeof(config));
    state.entries = entries;
    state.entry_count = 1;
    state.window = XCB_WINDOW_NONE;

    ctxmenu_show(NULL, &stage, &state, pos, &config);
    TAP_EQ_INT((int) state.window, (int) XCB_WINDOW_NONE,
            "null connection leaves the window unset");

    ctxmenu_show((xcb_connection_t *) 1, NULL, &state, pos, &config);
    TAP_EQ_INT((int) state.window, (int) XCB_WINDOW_NONE,
            "null stage leaves the window unset");

    ctxmenu_show((xcb_connection_t *) 1, &stage, NULL, pos, &config);
    TAP_OK(true, "null state does not crash");

    ctxmenu_show((xcb_connection_t *) 1, &stage, &state, pos, NULL);
    TAP_EQ_INT((int) state.window, (int) XCB_WINDOW_NONE,
            "null config leaves the window unset");

    state.entries = NULL;
    ctxmenu_show((xcb_connection_t *) 1, &stage, &state, pos, &config);
    TAP_EQ_INT((int) state.window, (int) XCB_WINDOW_NONE,
            "null entries leaves the window unset");

    state.entries = entries;
    state.entry_count = 0;
    ctxmenu_show((xcb_connection_t *) 1, &stage, &state, pos, &config);
    TAP_EQ_INT((int) state.window, (int) XCB_WINDOW_NONE,
            "zero entry_count leaves the window unset");

    state.entry_count = -1;
    ctxmenu_show((xcb_connection_t *) 1, &stage, &state, pos, &config);
    TAP_EQ_INT((int) state.window, (int) XCB_WINDOW_NONE,
            "negative entry_count leaves the window unset");
}


/**
 * @brief Verify that @a ctxmenu_close on a null state is a no-op
 */
static void s_test_close_null(void)
{
    ctxmenu_close(NULL);
    TAP_OK(true, "closing a null state does not crash");
}


/**
 * @brief Verify that @a ctxmenu_close resets every field it owns,
 *        without a live X connection to reach
 */
static void s_test_close_resets_fields(void)
{
    ctxmenu_state_td state;
    int32_t *top_y = (int32_t *) calloc(3, sizeof(int32_t));

    memset(&state, 0, sizeof(state));
    state.window = (xcb_window_t) 0x1234;
    state.selected = 2;
    state.width = 100;
    state.height = 200;
    state.stage = (stage_td *) 1;
    state.config = (config_td *) 1;
    state.entry_top_y = top_y;
    state.parent = NULL;
    state.child = NULL;

    ctxmenu_close(&state);

    TAP_EQ_INT((int) state.window, (int) XCB_WINDOW_NONE,
            "close resets window to none");
    TAP_EQ_INT(state.selected, -1, "close resets selected to -1");
    TAP_EQ_INT((int) state.width, 0, "close resets width to zero");
    TAP_EQ_INT((int) state.height, 0, "close resets height to zero");
    TAP_NULL(state.stage, "close clears the stage pointer");
    TAP_NULL(state.config, "close clears the config pointer");
    TAP_NULL(state.entry_top_y, "close frees and nulls entry_top_y");
}


/**
 * @brief Verify that @a ctxmenu_close recurses into a child chain and
 *        clears the parent's @c child pointer
 */
static void s_test_close_recurses_into_child(void)
{
    ctxmenu_state_td parent;
    ctxmenu_state_td child;
    ctxmenu_state_td grandchild;

    memset(&parent, 0, sizeof(parent));
    memset(&child, 0, sizeof(child));
    memset(&grandchild, 0, sizeof(grandchild));

    parent.window = (xcb_window_t) 1;
    parent.selected = -1;
    child.window = (xcb_window_t) 2;
    child.selected = -1;
    child.parent = &parent;
    child.child = &grandchild;
    grandchild.window = (xcb_window_t) 3;
    grandchild.selected = -1;
    grandchild.parent = &child;
    parent.child = &child;

    ctxmenu_close(&parent);

    TAP_EQ_INT((int) parent.window, (int) XCB_WINDOW_NONE,
            "closing the parent resets its own window");
    TAP_NULL(parent.child, "closing the parent clears its child link");
    TAP_EQ_INT((int) child.window, (int) XCB_WINDOW_NONE,
            "closing the parent recurses into the child");
    TAP_EQ_INT((int) grandchild.window, (int) XCB_WINDOW_NONE,
            "closing the parent recurses into the grandchild");
}


/**
 * @brief Verify @a ctxmenu_is_open answers correctly for every
 *        combination of a null state and an unset or set window
 */
static void s_test_is_open(void)
{
    ctxmenu_state_td state;

    memset(&state, 0, sizeof(state));
    state.window = XCB_WINDOW_NONE;

    TAP_OK(!ctxmenu_is_open(NULL), "a null state is never open");
    TAP_OK(!ctxmenu_is_open(&state),
            "a state with no window is not open");

    state.window = (xcb_window_t) 42;
    TAP_OK(ctxmenu_is_open(&state),
            "a state with a real window is open");
}


int main(void)
{
    TAP_PLAN(22);

    s_test_show_guards();
    s_test_close_null();
    s_test_close_resets_fields();
    s_test_close_recurses_into_child();
    s_test_is_open();

    return TAP_DONE();
}
