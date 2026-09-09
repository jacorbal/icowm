/**
 * @file menu/context/iconmenu.h
 *
 * @brief Icon context menu (right-click on an iconified client's icon)
 *
 * Provides a context menu that appears when the user right-clicks on
 * an iconified client's icon window.  The menu contains operations
 * applicable to an iconified client: send-to-desktop, send-to-page,
 * restore, hide, inspect, and close, the subset of the window context
 * menu's own entries that still mean something with no visible frame
 * or titlebar to move, resize, maximize, shade, or decorate.
 *
 * The menu is a singleton: at most one instance is open at a time.
 * Opening a new one closes the previous one automatically.
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

#ifndef MENU_CONTEXT_ICONMENU_H
#define MENU_CONTEXT_ICONMENU_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <types/handles.h>
#include <config.h>


/* Public interface */
/**
 * @brief Open the icon context menu for a client
 *
 * Builds and displays a context menu for @p client's icon at @p pos
 * (root coordinates).  Any previously open icon context menu is
 * closed first.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to display the menu
 * @param desktop    Desktop that currently contains @p client
 * @param client     Target client
 * @param pos        Requested origin (root coordinates)
 * @param config     Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       the surface
 */
void iconmenu_show(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, client_td *client,
        struct position_s pos, const config_td *config);

/**
 * @brief Close the icon context menu
 *
 * Destroys the menu window (and any open child menu) and resets all
 * internal state.
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
void iconmenu_close(void);

/**
 * @brief Repaint the icon context menu
 *
 * @param win Window that received the expose event
 *
 * @note Complexity: @e O(n)
 */
void iconmenu_repaint(xcb_window_t win);

/**
 * @brief Handle a button-press event inside the icon context menu
 *
 * @param connection XCB connection
 * @param surface    Surface associated with the event
 * @param win        Window that received the press
 * @param root_y     Pointer Y in root (screen) coordinates
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed
 *
 * @note Complexity: @e O(1)
 */
bool iconmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int root_y,
        const config_td *config);

/**
 * @brief Query whether the icon context menu is currently open
 *
 * @return @c true when the menu is visible
 *
 * @note Complexity: @e O(1)
 */
bool iconmenu_is_open(void);

/**
 * @brief Check whether @p win belongs to the icon context menu
 *        hierarchy
 *
 * @param win XCB window to test
 *
 * @return @c true if @p win is part of the currently open menu
 *
 * @note Complexity: @e O(d)
 */
bool iconmenu_owns_window(xcb_window_t win);

/**
 * @brief Handle a key-press event while the icon context menu is open
 *
 * Forwards the key event to the deepest open menu level in the icon
 * context menu hierarchy.
 *
 * @param connection XCB connection
 * @param surface    Surface on which the menu is displayed
 * @param keysym     X keysym of the pressed key
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed
 *
 * @note Complexity: @e O(n), where @e n is the number of menu entries
 */
bool iconmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config);

/**
 * @brief Handle a pointer-motion event over an icon context menu
 *        window
 *
 * Finds the menu state that owns @p win and updates its hover highlight
 * based on the pointer position (@p x, @p y) relative to that window.
 *
 * @param win Window that received the motion event
 * @param x   Pointer X relative to @p win
 * @param y   Pointer Y relative to @p win
 */
void iconmenu_handle_motion(xcb_window_t win, int x, int y);


#endif  /* ! MENU_CONTEXT_ICONMENU_H */
