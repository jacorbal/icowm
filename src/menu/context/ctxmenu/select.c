/**
 * @file menu/context/ctxmenu/select.c
 *
 * @brief Selection and activation of a context menu's own entries
 *
 * Split out of what used to be a single, flat
 * @c menu/context/ctxmenu.c.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <lifecycle.h>
#include <surface.h>

/* Local includes */
#include <menu/context/ctxmenu/redraw.h>
#include <menu/context/ctxmenu/select.h>


/**
 * @brief Records whether the most recently activated entry was
 *        triggered by the keyboard (@c Return / @c KP_Enter or a
 *        printable-character shortcut) or by a mouse click
 *
 * Set by @c ctxmenu_entry_activate itself from its own @p by_keyboard
 * parameter, so that an entry's @c on_activate callback can query
 * @c ctxmenu_last_activation_was_keyboard to decide between a
 * keyboard-driven and a pointer-driven interaction (e.g., window move
 * or resize).
 */
static bool s_activated_by_keyboard = false;


/* Activate the entry at the given index in a context menu; see the
 * header's own doc comment for the full reasoning */
bool ctxmenu_entry_activate(ctxmenu_state_td *state, int idx,
        bool by_keyboard)
{
    ctxmenu_entry_td e;
    ctxmenu_state_td *root;
    xcb_connection_t *conn;
    surface_td *surf;

    if (state == NULL || idx < 0 || idx >= state->entry_count) {
        return false;
    }

    e = state->entries[idx];
    if (e.type == CTXMENU_SEPARATOR || e.type == CTXMENU_LABEL ||
            e.is_disabled) {
        return true;
    }

    s_activated_by_keyboard = by_keyboard;

    root = state;
    while (root->parent != NULL) {
        root = root->parent;
    }

    /* Save connection and surface before close clears them */
    conn = root->connection;
    surf = root->surface;

    /* Close first so keyboard and pointer grabs are released before the
     * callback runs; this lets callbacks establish their own grabs */
    ctxmenu_close(root);

    if (e.on_activate != NULL) {
        e.on_activate(conn, e.userdata);
    } else if (e.command != NULL) {
        lifecycle_launch_dispatch(surf, e.command, e.class_name);
    }

    return true;
}


/**
 * @brief Move a menu's selection one step in a direction, skipping
 *        separators, labels, and disabled entries
 *
 * Shared by the Up and Down arrow handling in @c ctxmenu_handle_
 * keypress, which only differ in @p step's sign and where an
 * initially-unselected state (@c selected @c < @c 0) starts scanning
 * from; everything else (wrapping around either end of the entry
 * list, skipping unselectable entries, repainting once a valid one is
 * found) is identical between the two.
 *
 * @param state Menu state whose selection to move
 * @param step  @c +1 to move down/forward, @c -1 to move up/backward
 *
 * @note Complexity: @e O(n), where @e n is @c state->entry_count
 */
void ctxmenu_selection_move(ctxmenu_state_td *state, int step)
{
    int prev_sel = state->selected;
    int next = (prev_sel < 0)
        ? ((step > 0) ? 0 : state->entry_count - 1)
        : prev_sel + step;
    int i;

    for (i = 0; i < state->entry_count; ++i) {
        if (next < 0) { next = state->entry_count - 1; }
        if (next >= state->entry_count) { next = 0; }
        if (state->entries[next].type != CTXMENU_SEPARATOR &&
                state->entries[next].type != CTXMENU_LABEL &&
                !state->entries[next].is_disabled) {
            break;
        }
        next += step;
    }
    if (i < state->entry_count) {
        state->selected = next;
        ctxmenu_redraw_entries(state, prev_sel, next);
    }
}


/* Query whether the last activated entry was triggered by the keyboard */
bool ctxmenu_last_activation_was_keyboard(void)
{
    return s_activated_by_keyboard;
}
