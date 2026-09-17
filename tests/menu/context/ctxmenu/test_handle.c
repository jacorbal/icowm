/**
 * @file tests/menu/context/ctxmenu/test_handle.c
 *
 * @brief Test battery for a single context menu window's raw event
 *        handling (menu/context/ctxmenu/handle.c)
 *
 * All three entry points here are pure dispatch logic over a caller
 * supplied 'ctxmenu_state_td': deciding which entry an event lands
 * on, whether it is selectable, and which one of a handful of
 * collaborators to call, none of which involves painting or a live
 * X connection by itself.  Every collaborator ('ctxmenu_close',
 * 'ctxmenu_show', 'ctxmenu_entry_activate', 'ctxmenu_selection_move',
 * 'ctxmenu_redraw_entries', 'ctxmenu_entry_at_y',
 * 'ctxmenu_entry_top_y') is a recording or test-controlled stand-in
 * here, so each test can check exactly what 'handle.c' decided to do
 * without any of its real, X-bound siblings ('redraw.c', the
 * layout half of 'layout.c') needing a live display.
 *
 * 'ctxmenu_entry_at_y' is test-controlled rather than a fixed
 * link-only no-op, since every click and motion test needs to steer
 * which row a given @c y lands on without depending on real pixel
 * geometry.
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
#include <xcb/xcb_keysyms.h>

/* Default initial values */
#include <defs/kbd.h>

/* Local includes */
#include <config.h>
#include <harness/tap.h>
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/handle.h>
#include <menu/context/ctxmenu/layout.h>
#include <menu/context/ctxmenu/redraw.h>
#include <menu/context/ctxmenu/select.h>
#include <stage.h>


/** Row a given 'y' maps to, and how many times the lookup ran */
static int s_entry_at_y_result;
static int s_entry_at_y_calls;

/** Test-controlled stand-in for @a ctxmenu_entry_at_y
 * @note Complexity: @e O(1) */
int ctxmenu_entry_at_y(const ctxmenu_state_td *state, int y)
{
    (void) state;
    (void) y;
    s_entry_at_y_calls++;
    return s_entry_at_y_result;
}


/** Link-only stand-in for @a ctxmenu_entry_top_y
 * @note Complexity: @e O(1) */
int ctxmenu_entry_top_y(const ctxmenu_state_td *state, int idx)
{
    (void) state;
    (void) idx;
    return 0;
}


static int s_close_calls;
static ctxmenu_state_td *s_close_last_state;

/** Recording stand-in for @a ctxmenu_close
 * @note Complexity: @e O(1) */
void ctxmenu_close(ctxmenu_state_td *state)
{
    s_close_calls++;
    s_close_last_state = state;
}


static int s_show_calls;
static ctxmenu_state_td *s_show_last_state;

/** Recording stand-in for @a ctxmenu_show
 * @note Complexity: @e O(1) */
void ctxmenu_show(xcb_connection_t *connection, stage_td *stage,
        ctxmenu_state_td *state, struct position_s pos,
        const config_td *config)
{
    (void) connection;
    (void) stage;
    (void) pos;
    (void) config;
    s_show_calls++;
    s_show_last_state = state;
}


static int s_activate_calls;
static int s_activate_last_idx;
static bool s_activate_last_by_keyboard;
static bool s_activate_return;

/** Recording, test-controlled stand-in for @a ctxmenu_entry_activate
 * @note Complexity: @e O(1) */
bool ctxmenu_entry_activate(ctxmenu_state_td *state, int idx,
        bool by_keyboard)
{
    (void) state;
    s_activate_calls++;
    s_activate_last_idx = idx;
    s_activate_last_by_keyboard = by_keyboard;
    return s_activate_return;
}


static int s_selection_move_calls;
static int s_selection_move_last_step;

/** Recording stand-in for @a ctxmenu_selection_move
 * @note Complexity: @e O(1) */
void ctxmenu_selection_move(ctxmenu_state_td *state, int step)
{
    (void) state;
    s_selection_move_calls++;
    s_selection_move_last_step = step;
}


static int s_redraw_entries_calls;

/** Recording stand-in for @a ctxmenu_redraw_entries
 * @note Complexity: @e O(1) */
void ctxmenu_redraw_entries(const ctxmenu_state_td *state, int idx_a,
        int idx_b)
{
    (void) state;
    (void) idx_a;
    (void) idx_b;
    s_redraw_entries_calls++;
}


static void s_reset(void)
{
    s_entry_at_y_result = -1;
    s_entry_at_y_calls = 0;
    s_close_calls = 0;
    s_close_last_state = NULL;
    s_show_calls = 0;
    s_show_last_state = NULL;
    s_activate_calls = 0;
    s_activate_last_idx = -1;
    s_activate_last_by_keyboard = false;
    s_activate_return = true;
    s_selection_move_calls = 0;
    s_selection_move_last_step = 0;
    s_redraw_entries_calls = 0;
}


/**
 * @brief Build a menu with a command, a submenu (with one real child
 *        entry and a heap-free child state), and a disabled command
 */
static void s_make_menu(ctxmenu_entry_td entries[3],
        ctxmenu_entry_td sub_items[1], ctxmenu_state_td *child_state,
        ctxmenu_state_td *state)
{
    memset(entries, 0, 3 * sizeof(*entries));
    memset(sub_items, 0, sizeof(*sub_items));
    memset(child_state, 0, sizeof(*child_state));
    memset(state, 0, sizeof(*state));

    sub_items[0].type = CTXMENU_COMMAND;
    strcpy(sub_items[0].label, "Child");

    entries[0].type = CTXMENU_COMMAND;
    strcpy(entries[0].label, "Alpha");

    entries[1].type = CTXMENU_SUBMENU;
    strcpy(entries[1].label, "Beta");
    entries[1].items = sub_items;
    entries[1].item_count = 1;
    entries[1].userdata = child_state;

    entries[2].type = CTXMENU_COMMAND;
    entries[2].is_disabled = true;
    strcpy(entries[2].label, "Gamma");

    state->entries = entries;
    state->entry_count = 3;
    state->window = (xcb_window_t) 99;
    state->selected = -1;
    state->last_motion_y = -1;
}


/**
 * @brief Verify all three handlers reject a null or closed state
 */
static void s_test_guards(void)
{
    ctxmenu_state_td state;

    s_reset();
    memset(&state, 0, sizeof(state));
    state.window = XCB_WINDOW_NONE;

    TAP_OK(!ctxmenu_handle_keypress(NULL, NULL, NULL, KS_UP, NULL),
            "keypress on a null state returns false");
    TAP_OK(!ctxmenu_handle_keypress(NULL, NULL, &state, KS_UP, NULL),
            "keypress on a windowless state returns false");
    TAP_OK(!ctxmenu_handle_click(NULL, NULL, NULL, 0, NULL),
            "click on a null state returns false");
    TAP_OK(!ctxmenu_handle_click(NULL, NULL, &state, 0, NULL),
            "click on a windowless state returns false");

    ctxmenu_handle_motion(NULL, 0, 0);
    TAP_OK(true, "motion on a null state does not crash");
    ctxmenu_handle_motion(&state, 0, 0);
    TAP_OK(true, "motion on a windowless state does not crash");
}


/**
 * @brief Verify @c KS_UP and @c KS_DOWN forward to
 *        @a ctxmenu_selection_move with the correct step
 */
static void s_test_keypress_up_down(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);

    TAP_OK(ctxmenu_handle_keypress(NULL, NULL, &state, KS_UP, NULL),
            "up arrow is consumed");
    TAP_EQ_INT(s_selection_move_calls, 1,
            "up arrow calls selection_move once");
    TAP_EQ_INT(s_selection_move_last_step, -1,
            "up arrow moves with step -1");

    TAP_OK(ctxmenu_handle_keypress(NULL, NULL, &state, KS_DOWN, NULL),
            "down arrow is consumed");
    TAP_EQ_INT(s_selection_move_calls, 2,
            "down arrow calls selection_move again");
    TAP_EQ_INT(s_selection_move_last_step, 1,
            "down arrow moves with step +1");
}


/**
 * @brief Verify @c KS_RIGHT opens the submenu of a selected,
 *        non-disabled submenu entry
 */
static void s_test_keypress_right_opens_submenu(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;
    config_td config;
    stage_td stage;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);
    state.selected = 1;
    memset(&config, 0, sizeof(config));
    memset(&stage, 0, sizeof(stage));

    TAP_OK(ctxmenu_handle_keypress((xcb_connection_t *) 1, &stage,
                &state, KS_RIGHT, &config),
            "right arrow on a submenu entry is consumed");
    TAP_EQ_INT(s_show_calls, 1, "right arrow shows the child menu");
    TAP_OK(s_show_last_state == &child_state,
            "right arrow shows the entry's own child state");
    TAP_OK(state.child == &child_state,
            "right arrow links the child into the parent state");
    TAP_OK(child_state.entries == sub_items,
            "the child state's entries point at the submenu's items");
    TAP_EQ_INT(child_state.entry_count, 1,
            "the child state's entry_count matches the submenu");
    TAP_OK(child_state.parent == &state,
            "the child state's parent points back at the opener");
}


/**
 * @brief Verify @c KS_RIGHT on a command (non-submenu) entry is
 *        consumed without opening anything
 */
static void s_test_keypress_right_non_submenu(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);
    state.selected = 0;

    TAP_OK(ctxmenu_handle_keypress(NULL, NULL, &state, KS_RIGHT, NULL),
            "right arrow on a command entry is still consumed");
    TAP_EQ_INT(s_show_calls, 0,
            "right arrow on a command entry shows nothing");
}


/**
 * @brief Verify @c KS_LEFT on a submenu closes it and clears the
 *        parent's child pointer, and is a no-op on a root menu
 */
static void s_test_keypress_left(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;
    ctxmenu_state_td parent;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);
    memset(&parent, 0, sizeof(parent));
    state.parent = &parent;
    parent.child = &state;

    TAP_OK(ctxmenu_handle_keypress(NULL, NULL, &state, KS_LEFT, NULL),
            "left arrow on a submenu is consumed");
    TAP_EQ_INT(s_close_calls, 1, "left arrow closes the submenu once");
    TAP_OK(s_close_last_state == &state,
            "left arrow closes this state, not the parent");
    TAP_NULL(parent.child,
            "left arrow clears the parent's child pointer");

    s_reset();
    state.parent = NULL;
    TAP_OK(ctxmenu_handle_keypress(NULL, NULL, &state, KS_LEFT, NULL),
            "left arrow on a root menu is still consumed");
    TAP_EQ_INT(s_close_calls, 0,
            "left arrow on a root menu closes nothing");
}


/**
 * @brief Verify @c KS_RETURN activates a command entry directly, and
 *        recurses as @c KS_RIGHT for a selected submenu entry
 */
static void s_test_keypress_return(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);
    state.selected = 0;

    TAP_OK(ctxmenu_handle_keypress(NULL, NULL, &state, KS_RETURN, NULL),
            "return on a command entry is consumed");
    TAP_EQ_INT(s_activate_calls, 1,
            "return activates the selected entry once");
    TAP_EQ_INT(s_activate_last_idx, 0,
            "return activates the currently selected index");
    TAP_OK(s_activate_last_by_keyboard,
            "return activation is recorded as keyboard-driven");

    s_reset();
    state.selected = 1;
    TAP_OK(ctxmenu_handle_keypress(NULL, NULL, &state, KS_KP_ENTER,
                NULL),
            "kp_enter on a submenu entry is consumed");
    TAP_EQ_INT(s_show_calls, 1,
            "kp_enter on a submenu entry opens it, same as right"
            " arrow");
    TAP_EQ_INT(s_activate_calls, 0,
            "kp_enter on a submenu entry never calls entry_activate"
            " directly");

    s_reset();
    state.selected = -1;
    TAP_OK(ctxmenu_handle_keypress(NULL, NULL, &state, KS_RETURN, NULL),
            "return with no selection is still consumed");
    TAP_EQ_INT(s_activate_calls, 0,
            "return with no selection activates nothing");
}


/**
 * @brief Verify @c KS_ESCAPE closes the whole menu hierarchy starting
 *        from the root, not the deepest open submenu
 */
static void s_test_keypress_escape(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;
    ctxmenu_state_td root;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);
    memset(&root, 0, sizeof(root));
    state.parent = &root;

    TAP_OK(ctxmenu_handle_keypress(NULL, NULL, &state, KS_ESCAPE, NULL),
            "escape is consumed");
    TAP_EQ_INT(s_close_calls, 1, "escape closes exactly once");
    TAP_OK(s_close_last_state == &root,
            "escape closes the root of the chain, not the submenu");
}


/**
 * @brief Verify a printable character with a single matching label
 *        selects and activates that entry
 */
static void s_test_keypress_typed_unique_match(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);

    TAP_OK(ctxmenu_handle_keypress(NULL, NULL, &state, 'a', NULL),
            "typing the unique letter 'a' is consumed");
    TAP_EQ_INT(state.selected, 0,
            "typing 'a' selects the one entry starting with it"
            " ('Alpha')");
    TAP_EQ_INT(s_redraw_entries_calls, 1,
            "typing a match repaints the old and new selection");
    TAP_EQ_INT(s_activate_calls, 1,
            "a unique match activates the entry immediately");
    TAP_EQ_INT(s_activate_last_idx, 0,
            "the activated index is the unique match");
}


/**
 * @brief Verify a printable character matching a disabled entry's
 *        label is ignored, since disabled entries are skipped by the
 *        scan
 */
static void s_test_keypress_typed_disabled_is_skipped(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);

    TAP_OK(!ctxmenu_handle_keypress(NULL, NULL, &state, 'g', NULL),
            "typing the disabled entry's letter is not consumed");
    TAP_EQ_INT(s_activate_calls, 0,
            "a disabled-only match activates nothing");
}


/**
 * @brief Verify a non-printable or out-of-range keysym is rejected
 */
static void s_test_keypress_unhandled(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);

    TAP_OK(!ctxmenu_handle_keypress(NULL, NULL, &state, 0xff80u, NULL),
            "an out-of-range keysym is not consumed");
}


/**
 * @brief Verify @a ctxmenu_handle_click ignores a click landing
 *        outside any entry, and consumes but no-ops on a separator
 *        or disabled row
 */
static void s_test_click_out_of_range_and_noop(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);

    s_entry_at_y_result = -1;
    TAP_OK(!ctxmenu_handle_click(NULL, NULL, &state, 5, NULL),
            "a click that lands on no row is not consumed");

    s_reset();
    s_entry_at_y_result = 2;
    TAP_OK(ctxmenu_handle_click(NULL, NULL, &state, 5, NULL),
            "a click on a disabled entry is consumed");
    TAP_EQ_INT(s_activate_calls, 0,
            "a click on a disabled entry activates nothing");
}


/**
 * @brief Verify a click on a submenu entry opens the child menu the
 *        same way the keyboard path does
 */
static void s_test_click_opens_submenu(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);
    s_entry_at_y_result = 1;

    TAP_OK(ctxmenu_handle_click((xcb_connection_t *) 1, NULL, &state,
                10, NULL),
            "a click on a submenu entry is consumed");
    TAP_EQ_INT(s_show_calls, 1, "a click on a submenu entry shows it");
    TAP_EQ_INT(state.selected, 1,
            "a click on a submenu entry selects that row");
    TAP_OK(state.child == &child_state,
            "a click on a submenu entry links the child state");
}


/**
 * @brief Verify a click on an ordinary command entry falls through to
 *        @a ctxmenu_entry_activate with @c by_keyboard false
 */
static void s_test_click_activates_command(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);
    s_entry_at_y_result = 0;

    TAP_OK(ctxmenu_handle_click(NULL, NULL, &state, 5, NULL),
            "a click on a command entry is consumed");
    TAP_EQ_INT(s_activate_calls, 1,
            "a click on a command entry activates it once");
    TAP_EQ_INT(s_activate_last_idx, 0,
            "the activated index matches the clicked row");
    TAP_OK(!s_activate_last_by_keyboard,
            "a mouse click is recorded as not keyboard-driven");
}


/**
 * @brief Verify @a ctxmenu_handle_motion deduplicates a repeated
 *        @c y, updates the selection on a genuine move, and clears it
 *        when the pointer leaves every entry
 */
static void s_test_motion(void)
{
    ctxmenu_entry_td entries[3];
    ctxmenu_entry_td sub_items[1];
    ctxmenu_state_td child_state;
    ctxmenu_state_td state;

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);
    state.last_motion_y = 5;

    ctxmenu_handle_motion(&state, 0, 5);
    TAP_EQ_INT(s_entry_at_y_calls, 0,
            "a motion at the same y as last time is ignored entirely");

    s_reset();
    s_make_menu(entries, sub_items, &child_state, &state);
    s_entry_at_y_result = 0;
    ctxmenu_handle_motion(&state, 0, 7);
    TAP_EQ_INT(state.selected, 0,
            "motion over a selectable entry selects it");
    TAP_EQ_INT(s_redraw_entries_calls, 1,
            "a genuine selection change repaints once");
    TAP_EQ_INT(state.last_motion_y, 7,
            "motion records the y it just processed");

    s_reset();
    state.selected = 0;
    s_entry_at_y_result = -1;
    ctxmenu_handle_motion(&state, 0, 40);
    TAP_EQ_INT(state.selected, -1,
            "motion past every entry clears the selection");
    TAP_EQ_INT(s_redraw_entries_calls, 1,
            "clearing the selection repaints once");

    s_reset();
    state.selected = 0;
    s_entry_at_y_result = 0;
    ctxmenu_handle_motion(&state, 0, 41);
    TAP_EQ_INT(s_redraw_entries_calls, 0,
            "motion that lands back on the already-selected row"
            " repaints nothing");
}


int main(void)
{
    TAP_PLAN(65);

    s_test_guards();
    s_test_keypress_up_down();
    s_test_keypress_right_opens_submenu();
    s_test_keypress_right_non_submenu();
    s_test_keypress_left();
    s_test_keypress_return();
    s_test_keypress_escape();
    s_test_keypress_typed_unique_match();
    s_test_keypress_typed_disabled_is_skipped();
    s_test_keypress_unhandled();
    s_test_click_out_of_range_and_noop();
    s_test_click_opens_submenu();
    s_test_click_activates_command();
    s_test_motion();

    return TAP_DONE();
}
