/**
 * @file menu/context/ctxmenu/handle.h
 *
 * @brief Raw keyboard, click, and motion event handling for a single
 *        context menu window
 *
 * @note This header is private to @c menu/context/ctxmenu/ and must
 *       not be included outside of it; every concrete menu (root
 *       menu, window menu, window list) reaches these only through
 *       @c menu/context/ctxmenu/tree.h's tree-wide dispatch
 *       instead
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

#ifndef MENU_CONTEXT_CTXMENU_HANDLE_H
#define MENU_CONTEXT_CTXMENU_HANDLE_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Local includes */
#include <menu/context/ctxmenu.h>


/**
 * @brief Handle a key-press event while a context menu is open
 *
 * Dispatches navigation and activation keys:
 *
 * - @c Up / @c Down arrows: move the selection highlight to the
 *   previous or next selectable entry (skipping separators and labels),
 *   wrapping around at the ends.
 * - @c Right arrow: if the currently selected entry is a submenu, open
 *   it; otherwise no action.
 * - @c Left arrow: if @p state has a parent (i.e., it is a submenu),
 *   close this submenu and return focus to the parent.
 * - @c Return / @c KP_Enter: activate the currently selected entry.
 * - @c Escape: close the entire menu hierarchy from the root.
 * - Any printable character: scan entries whose label begins with that
 *   character (case-insensitive); if exactly one match is found the
 *   entry is activated immediately; if more than one match is found the
 *   first match is highlighted without activating.
 *
 * The @p state parameter should be the deepest currently open level
 * (i.e., the visible submenu, or the root if no submenu is open).
 *
 * @param connection XCB connection (used to open submenus)
 * @param stage      Stage on which the menu is displayed
 * @param state      Deepest open menu state
 * @param keysym     X keysym of the pressed key
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed
 *
 * @note Complexity: @e O(n), where @e n is @p state->entry_count
 */
bool ctxmenu_handle_keypress(xcb_connection_t *connection,
        stage_td *stage, ctxmenu_state_td *state,
        xcb_keysym_t keysym, const config_td *config);

/**
 * @brief Handle a button-press event inside a context menu window
 *
 * Activates the entry at the pointer coordinates.  For
 * @c CTXMENU_COMMAND entries, invokes @p on_activate if set and then
 * closes the whole menu hierarchy.  For @c CTXMENU_SUBMENU entries,
 * opens the child menu.  Disabled entries are ignored.
 *
 * @param connection XCB connection
 * @param stage      Stage on which the menu is displayed
 * @param state      Menu state that owns the window receiving the event
 * @param y          Pointer Y in root (screen) coordinates
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed
 *
 * @note Complexity: @e O(1)
 */
bool ctxmenu_handle_click(xcb_connection_t *connection,
        stage_td *stage, ctxmenu_state_td *state,
        int y, const config_td *config);

/**
 * @brief Handle a pointer-motion event inside a context menu window
 *
 * Updates the hover highlight to the entry under the pointer position
 * @p y (relative to the menu window top edge).  Non-selectable entries
 * (separators, labels, disabled items) clear the selection instead of
 * highlighting.  Repaints the menu only when the selection changes.
 *
 * @param state Menu state that owns the window the pointer is over
 * @param x     Pointer X relative to the menu window (unused; kept for
 *              future use)
 * @param y Pointer Y relative to the menu window top edge
 *
 * @note Complexity: @e O(n), where @e n is @p state->entry_count
 */
void ctxmenu_handle_motion(ctxmenu_state_td *state, int x, int y);


#endif  /* ! MENU_CONTEXT_CTXMENU_HANDLE_H */
