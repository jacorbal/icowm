/**
 * @file menu/context/ctxmenu/tree.h
 *
 * @brief Dispatch across a context menu's own submenu window tree
 *
 * Shared by every concrete menu (root menu, window menu, window
 * list): each one only differs in which root state it passes, so
 * this replaces an identical lookup-then-forward sequence that used
 * to be copied into each of them.
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

#ifndef MENU_CONTEXT_CTXMENU_TREE_H
#define MENU_CONTEXT_CTXMENU_TREE_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Local includes */
#include <menu/context/ctxmenu.h>


/**
 * @brief Find the state owning the given XCB window
 *
 * Searches @p state and all its open descendants for the one whose
 * @p window matches @p win.
 *
 * @param state Root menu state to search from
 * @param win   XCB window to find
 *
 * @return Pointer to the matching state, or @c NULL if not found
 *
 * @note Complexity: @e O(d), where @e d is the nesting depth
 */
ctxmenu_state_td *ctxmenu_tree_state_find_for_window(
        ctxmenu_state_td *state, xcb_window_t win);

/**
 * @brief Repaint whichever submenu under @p root currently owns @p win
 *
 * @param root Top-level state of the concrete menu's own submenu tree
 * @param win  Window the repaint request arrived for
 *
 * @note No-op if @p win does not belong to any submenu under @p root
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
void ctxmenu_tree_redraw_window(ctxmenu_state_td *root, xcb_window_t win);

/**
 * @brief Forward a pointer-motion event to whichever submenu under
 *        @p root currently owns @p win
 *
 * @param root Top-level state of the concrete menu's own submenu tree
 * @param win  Window the motion event arrived for
 * @param x    Pointer X position, in @p win's own coordinates
 * @param y    Pointer Y position, in @p win's own coordinates
 *
 * @note No-op if @p win does not belong to any submenu under @p root
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
void ctxmenu_tree_handle_motion_window(ctxmenu_state_td *root,
        xcb_window_t win, int x, int y);

/**
 * @brief Forward a click, translated to menu-local coordinates, to
 *        whichever submenu under @p root currently owns @p win
 *
 * @param connection XCB connection
 * @param surface    Surface the click occurred on
 * @param root       Top-level state of the concrete menu's own
 *                   submenu tree
 * @param win        Window the click event arrived for
 * @param y          Pointer Y position, in @p win's own coordinates
 * @param config     Active configuration
 *
 * @return @c true if @p win belonged to a submenu under @p root and
 *         the click was forwarded
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 *
 * @see @a ctxmenu_tree_redraw_window
 */
bool ctxmenu_tree_handle_click_window(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root, xcb_window_t win,
        int y, const config_td *config);

/**
 * @brief Forward a keypress to the deepest currently open submenu
 *        under @p root
 *
 * Applies the keypress to the deepest open submenu, not always @p root
 * itself.  Without this, arrow keys would keep moving the selection in
 * a top-level list even while a nested submenu was open in front of it,
 * making that submenu look unresponsive to the keyboard.
 *
 * @param connection XCB connection
 * @param surface    Surface the key press occurred on
 * @param root       Top-level state of the concrete menu's own
 *                   submenu tree
 * @param keysym     Keysym of the pressed key
 * @param config     Active configuration
 *
 * @return @c true if the key was consumed
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
bool ctxmenu_tree_handle_keypress_deepest(xcb_connection_t *connection,
        surface_td *surface, ctxmenu_state_td *root,
        xcb_keysym_t keysym, const config_td *config);


#endif  /* ! MENU_CONTEXT_CTXMENU_TREE_H */
