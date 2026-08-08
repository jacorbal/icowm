/**
 * @file menu/context/wincmenu.h
 *
 * @brief Window context menu (right-click on window frame/titlebar)
 *
 * Provides a context menu that appears when the user right-clicks on
 * a decorated window's frame or titlebar.  The menu contains operations
 * applicable to the target client: layer changes, send-to-desktop,
 * move, resize, iconify, hide, maximize, shade, decoration toggle, and
 * close.
 *
 * The menu is a singleton: at most one instance is open at a time.
 * Opening a new one closes the previous one automatically.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_CONTEXT_WINCMENU_H
#define MENU_CONTEXT_WINCMENU_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <surface.h>


/**
 * @brief Maximum number of desktops shown in the "Send to desktop"
 *        submenu (capped to avoid oversized menus)
 */
#define WINCMENU_MAX_DESKTOPS (32)

/**
 * @brief Number of fixed entries in the "Layer" submenu
 */
#define WINCMENU_LAYER_COUNT (3)

/**
 * @brief Number of fixed top-level entries in the window context menu:
 *        TWO submenus (Send to desktop, Layer) + ONE separator + NINE
 *        commands (Restore, Move, Resize, Iconify, Hide, Maximize,
 *        Un/fullscreen, Un/shade, Un/decorate) + ONE separator +
 *        ONE command (Close) = FOURTEEN total
 */
#define WINCMENU_FIXED_ENTRIES (14)

/**
 * @brief Total top-level entry slots:
 *        @c (WINCMENU_FIXED_ENTRIES + 2) extra slots reserved for
 *        future or dynamic entries
 */
#define WINCMENU_TOTAL_ENTRIES (WINCMENU_FIXED_ENTRIES + 2)


/* Public interface */
/**
 * @brief Open the window context menu for a client
 *
 * Builds and displays a context menu for @p client at (@p x, @p y)
 * (root coordinates).  Any previously open window context menu is
 * closed first.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to display the menu
 * @param desktop    Desktop that currently contains @p client
 * @param client     Target client
 * @param x          Requested X origin (root coordinates)
 * @param y          Requested Y origin (root coordinates)
 * @param config     Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       the surface
 */
void wincmenu_show(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, client_td *client,
        int16_t x, int16_t y, const config_td *config);

/**
 * @brief Close the window context menu
 *
 * Destroys the menu window (and any open child menu) and resets all
 * internal state.
 *
 * @note Complexity: @e O(d), where @e d is the submenu nesting depth
 */
void wincmenu_close(void);

/**
 * @brief Repaint the window context menu
 *
 * Called from the expose handler.  Redraws the currently open menu
 * level.
 *
 * @param win Window that received the expose event
 *
 * @note Complexity: @e O(n)
 */
void wincmenu_repaint(xcb_window_t win);

/**
 * @brief Handle a button-press event inside the window context menu
 *
 * @param connection XCB connection
 * @param surface    Surface associated with the event
 * @param win        Window that received the press
 * @param root_x     Pointer X in root (screen) coordinates
 * @param root_y     Pointer Y in root (screen) coordinates
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed
 *
 * @note Complexity: @e O(1)
 */
bool wincmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int root_x, int root_y,
        const config_td *config);

/**
 * @brief Query whether the window context menu is currently open
 *
 * @return @c true when the menu is visible
 *
 * @note Complexity: @e O(1)
 */
bool wincmenu_is_open(void);

/**
 * @brief Return the root window context menu XCB window
 *
 * @return The menu's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t wincmenu_window(void);

/**
 * @brief Check whether @p win belongs to the window context menu
 *        hierarchy
 *
 * @param win XCB window to test
 *
 * @return @c true if @p win is part of the currently open menu
 *
 * @note Complexity: @e O(d)
 */
bool wincmenu_owns_window(xcb_window_t win);

/**
 * @brief Handle a key-press event while the window context menu is open
 *
 * Forwards the key event to the deepest open menu level in the window
 * context menu hierarchy.
 *
 * @param connection XCB connection
 * @param surface    Surface on which the menu is displayed
 * @param keysym     X keysym of the pressed key
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed, @c false otherwise
 *
 * @note Complexity: @e O(n), where @e n is the number of menu entries
 */
bool wincmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config);

/**
 * @brief Handle a pointer-motion event over a window context menu window
 *
 * Finds the menu state that owns @p win and updates its hover highlight
 * based on the pointer position (@p x, @p y) relative to that window.
 *
 * @param win Window that received the motion event
 * @param x   Pointer X relative to @p win
 * @param y   Pointer Y relative to @p win
 */
void wincmenu_handle_motion(xcb_window_t win, int x, int y);


#endif  /* ! MENU_CONTEXT_WINCMENU_H */
