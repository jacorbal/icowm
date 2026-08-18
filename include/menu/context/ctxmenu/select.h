/**
 * @file menu/context/ctxmenu/select.h
 *
 * @brief Selection and activation of a context menu's own entries
 *
 * @ingroup menu_context
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_CONTEXT_CTXMENU_SELECT_H
#define MENU_CONTEXT_CTXMENU_SELECT_H


/* System includes */
#include <stdbool.h>

/* Local includes */
#include <menu/context/ctxmenu.h>


/**
 * @brief Activate the entry at the given index in a context menu
 *
 * Closes the entire menu hierarchy first (releasing keyboard and pointer
 * grabs), then invokes the entry's @p on_activate callback or calls
 * @c lifecycle_launch_dispatch for command entries.  Closing before the
 * callback allows the callback to establish its own grabs (e.g., for
 * interactive keyboard move or resize) without conflicting with the
 * menu's active grab.  For separator, label, or disabled entries no
 * action is taken but @c true is returned to consume the event.
 *
 * Also records @p by_keyboard for @c ctxmenu_last_activation_was_
 * keyboard to report back to the entry's own @c on_activate callback.
 *
 * @param state       Menu state that contains the entry
 * @param idx         Zero-based index of the entry to activate
 * @param by_keyboard Whether this activation came from the keyboard
 *                    (@c Return / @c KP_Enter or a printable-character
 *                    shortcut) rather than a mouse click
 *
 * @return @c true if the event was consumed, @c false when @p state or
 *         @p idx is out of range
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
bool ctxmenu_entry_activate(ctxmenu_state_td *state, int idx,
        bool by_keyboard);

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
void ctxmenu_selection_move(ctxmenu_state_td *state, int step);

/**
 * @brief Query whether the last activated entry was triggered by the
 *        keyboard rather than a mouse click
 *
 * Set by @c ctxmenu_entry_activate right before an entry's
 * @p on_activate callback runs.  Callbacks that need to behave
 * differently for keyboard vs. mouse activation (e.g., window move or
 * resize, which use keyboard modal mode vs. a pointer drag) should
 * query this at the top of @p on_activate.
 *
 * @return @c true if the most recent activation was keyboard-driven
 *
 * @note Complexity: @e O(1)
 */
bool ctxmenu_last_activation_was_keyboard(void);


#endif  /* ! MENU_CONTEXT_CTXMENU_SELECT_H */
