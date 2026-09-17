/**
 * @file tests/menu/context/ctxmenu/test_select.c
 *
 * @brief Test battery for context menu selection and activation
 *        (menu/context/ctxmenu/select.c)
 *
 * Every public function here is pure dispatch logic over a caller
 * supplied 'ctxmenu_state_td' array, with no direct X11 drawing of
 * its own: 'ctxmenu_entry_activate' walks to the root of a menu
 * chain, closes it, and either calls the entry's 'on_activate'
 * callback or forwards to 'cctl_launch_dispatch'; 'ctxmenu_selection_
 * move' walks 'entries' skipping unselectable rows and repaints
 * through 'ctxmenu_redraw_entries'.  Both of those two collaborators,
 * plus 'cctl_launch_dispatch' itself, are recording stand-ins here,
 * so every test can check exactly what the real function decided to
 * do without any of them needing a live X connection.  The real
 * 'utils/xcb/connection.c' is linked instead of stood in, since
 * 'ctxmenu_entry_activate' only ever reads whatever
 * 'xcb_connection_get' answers and hands it along to the activated
 * callback, never dereferencing it itself.
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
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/redraw.h>
#include <menu/context/ctxmenu/select.h>
#include <stage.h>
#include <utils/xcb/connection.h>


/** Number of times 'ctxmenu_close' has been called, and the state it
 *  was last called with */
static int s_close_calls;
static ctxmenu_state_td *s_close_last_state;

/** Recording stand-in for @a ctxmenu_close
 * @note Complexity: @e O(1) */
void ctxmenu_close(ctxmenu_state_td *state)
{
    s_close_calls++;
    s_close_last_state = state;
}


/** Number of times 'ctxmenu_redraw_entries' has been called, and the
 *  arguments of the last call */
static int s_redraw_entries_calls;
static int s_redraw_entries_idx_a;
static int s_redraw_entries_idx_b;

/** Recording stand-in for @a ctxmenu_redraw_entries
 * @note Complexity: @e O(1) */
void ctxmenu_redraw_entries(const ctxmenu_state_td *state, int idx_a,
        int idx_b)
{
    (void) state;
    s_redraw_entries_calls++;
    s_redraw_entries_idx_a = idx_a;
    s_redraw_entries_idx_b = idx_b;
}


/** Recording stand-in for @a ctxmenu_redraw
 *
 * Not reached by anything in 'select.c' itself, but linked so that
 * 'redraw.h''s declarations stay fully resolved.
 * @note Complexity: @e O(1) */
void ctxmenu_redraw(ctxmenu_state_td *state)
{
    (void) state;
}


/** Number of times 'cctl_launch_dispatch' has been called, and the
 *  arguments of the last call */
static int s_dispatch_calls;
static stage_td *s_dispatch_stage;
static const char *s_dispatch_prog;
static const char *s_dispatch_class_name;

/** Recording stand-in for @a cctl_launch_dispatch
 * @note Complexity: @e O(1) */
void cctl_launch_dispatch(stage_td *stage, const char *restrict prog,
        const char *restrict class_name)
{
    s_dispatch_calls++;
    s_dispatch_stage = stage;
    s_dispatch_prog = prog;
    s_dispatch_class_name = class_name;
}


/** Number of times a test's own 'on_activate' callback has been
 *  called, and the arguments of the last call */
static int s_on_activate_calls;
static xcb_connection_t *s_on_activate_conn;
static void *s_on_activate_userdata;

static void s_on_activate(xcb_connection_t *connection, void *userdata)
{
    s_on_activate_calls++;
    s_on_activate_conn = connection;
    s_on_activate_userdata = userdata;
}


static void s_reset(void)
{
    s_close_calls = 0;
    s_close_last_state = NULL;
    s_redraw_entries_calls = 0;
    s_redraw_entries_idx_a = 0;
    s_redraw_entries_idx_b = 0;
    s_dispatch_calls = 0;
    s_dispatch_stage = NULL;
    s_dispatch_prog = NULL;
    s_dispatch_class_name = NULL;
    s_on_activate_calls = 0;
    s_on_activate_conn = NULL;
    s_on_activate_userdata = NULL;
}


/**
 * @brief Build a three-entry menu: a command, a separator, and a
 *        disabled command, for tests that need one row of each kind
 */
static void s_make_three_entries(ctxmenu_entry_td entries[3])
{
    memset(entries, 0, 3 * sizeof(*entries));
    entries[0].type = CTXMENU_COMMAND;
    entries[1].type = CTXMENU_SEPARATOR;
    entries[2].type = CTXMENU_COMMAND;
    entries[2].is_disabled = true;
}


/**
 * @brief Verify @a ctxmenu_entry_activate rejects a null state or an
 *        out-of-range index without touching close or dispatch
 */
static void s_test_activate_guards(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_state_td state;

    s_reset();
    s_make_three_entries(entries);
    memset(&state, 0, sizeof(state));
    state.entries = entries;
    state.entry_count = 3;

    TAP_OK(!ctxmenu_entry_activate(NULL, 0, false),
            "activating on a null state returns false");
    TAP_OK(!ctxmenu_entry_activate(&state, -1, false),
            "activating a negative index returns false");
    TAP_OK(!ctxmenu_entry_activate(&state, 3, false),
            "activating an index past entry_count returns false");
    TAP_EQ_INT(s_close_calls, 0,
            "none of the rejected calls closes the menu");
}


/**
 * @brief Verify a separator or a disabled entry consumes the event
 *        without closing the menu or dispatching anything
 */
static void s_test_activate_noop_entries(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_state_td state;

    s_reset();
    s_make_three_entries(entries);
    memset(&state, 0, sizeof(state));
    state.entries = entries;
    state.entry_count = 3;

    TAP_OK(ctxmenu_entry_activate(&state, 1, false),
            "activating a separator returns true");
    TAP_OK(ctxmenu_entry_activate(&state, 2, false),
            "activating a disabled entry returns true");
    TAP_EQ_INT(s_close_calls, 0,
            "neither no-op activation closes the menu");
    TAP_EQ_INT(s_dispatch_calls, 0,
            "neither no-op activation dispatches a command");
}


/**
 * @brief Verify a command entry with an @c on_activate callback
 *        closes the menu first and then invokes the callback, never
 *        'cctl_launch_dispatch'
 */
static void s_test_activate_callback(void)
{
    ctxmenu_entry_td entries[1];
    ctxmenu_state_td state;
    int userdata_marker = 7;

    s_reset();
    memset(entries, 0, sizeof(entries));
    entries[0].type = CTXMENU_COMMAND;
    entries[0].on_activate = s_on_activate;
    entries[0].userdata = &userdata_marker;

    memset(&state, 0, sizeof(state));
    state.entries = entries;
    state.entry_count = 1;

    TAP_OK(ctxmenu_entry_activate(&state, 0, true),
            "activating a callback entry returns true");
    TAP_EQ_INT(s_close_calls, 1, "activation closes the menu once");
    TAP_EQ_INT(s_on_activate_calls, 1,
            "activation invokes the on_activate callback once");
    TAP_OK(s_on_activate_userdata == &userdata_marker,
            "the callback receives the entry's userdata");
    TAP_EQ_INT(s_dispatch_calls, 0,
            "a callback entry never falls through to launch dispatch");
    TAP_OK(ctxmenu_last_activation_was_keyboard(),
            "keyboard activation is recorded as such");
}


/**
 * @brief Verify a command entry with no callback but a command string
 *        falls through to @a cctl_launch_dispatch with the entry's
 *        command, class name, and the root menu's stage
 */
static void s_test_activate_dispatch(void)
{
    ctxmenu_entry_td entries[1];
    ctxmenu_state_td child;
    ctxmenu_state_td parent;
    stage_td stage;

    s_reset();
    memset(entries, 0, sizeof(entries));
    entries[0].type = CTXMENU_COMMAND;
    entries[0].command = (char *) "xterm";
    entries[0].class_name = (char *) "XTerm";

    memset(&stage, 0, sizeof(stage));
    memset(&parent, 0, sizeof(parent));
    memset(&child, 0, sizeof(child));
    parent.stage = &stage;
    child.parent = &parent;
    child.entries = entries;
    child.entry_count = 1;

    TAP_OK(ctxmenu_entry_activate(&child, 0, false),
            "activating a command entry returns true");
    TAP_EQ_INT(s_close_calls, 1, "activation closes the menu once");
    TAP_OK(s_close_last_state == &parent,
            "activation closes the root of the chain, not the child");
    TAP_EQ_INT(s_dispatch_calls, 1,
            "activation dispatches the command exactly once");
    TAP_OK(s_dispatch_stage == &stage,
            "dispatch receives the root menu's stage");
    TAP_EQ_STR(s_dispatch_prog, "xterm",
            "dispatch receives the entry's command string");
    TAP_EQ_STR(s_dispatch_class_name, "XTerm",
            "dispatch receives the entry's class name override");
    TAP_OK(!ctxmenu_last_activation_was_keyboard(),
            "mouse activation is recorded as such");
}


/**
 * @brief Verify @a ctxmenu_selection_move starts at the first
 *        selectable row when nothing is selected yet and moving
 *        forward
 */
static void s_test_selection_move_from_unselected_forward(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_state_td state;

    s_reset();
    s_make_three_entries(entries);
    memset(&state, 0, sizeof(state));
    state.entries = entries;
    state.entry_count = 3;
    state.selected = -1;

    ctxmenu_selection_move(&state, 1);

    TAP_EQ_INT(state.selected, 0,
            "moving forward from unselected lands on the first row");
    TAP_EQ_INT(s_redraw_entries_calls, 1,
            "a successful move repaints exactly once");
}


/**
 * @brief Verify @a ctxmenu_selection_move skips a separator and a
 *        disabled entry while wrapping around the end of the list
 */
static void s_test_selection_move_skips_and_wraps(void)
{
    ctxmenu_entry_td entries[4];
    ctxmenu_state_td state;

    s_reset();
    memset(entries, 0, 4 * sizeof(*entries));
    entries[0].type = CTXMENU_COMMAND;
    entries[1].type = CTXMENU_SEPARATOR;
    entries[2].type = CTXMENU_COMMAND;
    entries[2].is_disabled = true;
    entries[3].type = CTXMENU_COMMAND;

    memset(&state, 0, sizeof(state));
    state.entries = entries;
    state.entry_count = 4;
    state.selected = 3;

    ctxmenu_selection_move(&state, 1);

    TAP_EQ_INT(state.selected, 0,
            "moving forward past the last row wraps to the first"
            " selectable one, skipping the separator and the"
            " disabled entry along the way");

    state.selected = 0;
    ctxmenu_selection_move(&state, -1);

    TAP_EQ_INT(state.selected, 3,
            "moving backward from the first row wraps to the last"
            " selectable one");
}


/**
 * @brief Verify @a ctxmenu_selection_move leaves @c selected
 *        untouched and never repaints when every entry is
 *        unselectable
 */
static void s_test_selection_move_none_selectable(void)
{
    ctxmenu_entry_td entries[2];
    ctxmenu_state_td state;

    s_reset();
    memset(entries, 0, 2 * sizeof(*entries));
    entries[0].type = CTXMENU_SEPARATOR;
    entries[1].type = CTXMENU_LABEL;

    memset(&state, 0, sizeof(state));
    state.entries = entries;
    state.entry_count = 2;
    state.selected = -1;

    ctxmenu_selection_move(&state, 1);

    TAP_EQ_INT(state.selected, -1,
            "selection stays unset when nothing is selectable");
    TAP_EQ_INT(s_redraw_entries_calls, 0,
            "no repaint happens when nothing is selectable");
}


int main(void)
{
    TAP_PLAN(28);

    s_test_activate_guards();
    s_test_activate_noop_entries();
    s_test_activate_callback();
    s_test_activate_dispatch();
    s_test_selection_move_from_unselected_forward();
    s_test_selection_move_skips_and_wraps();
    s_test_selection_move_none_selectable();

    return TAP_DONE();
}
