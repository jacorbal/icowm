/**
 * @file menu/context/rootmenu.h
 *
 * @brief Root desktop menu (right-click on empty desktop)
 *
 * Displays a popup menu when the user right-clicks on the root window
 * (empty desktop).  The menu entries come from @c menu.json in the
 * configuration directory, followed by a fixed footer:
 *
 * @code
 * ----------------------
 * Reload configuration
 * Redraw all windows
 * ----------------------
 * Exit
 * @endcode
 *
 * "Reload configuration", "Redraw all windows", and "Exit" map directly
 * to the corresponding window manager keyboard actions.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_CONTEXT_ROOTMENU_H
#define MENU_CONTEXT_ROOTMENU_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/* Public interface */
/**
 * @brief Load and display the root desktop menu
 *
 * Parses @c menu.json (located in the active configuration directory),
 * appends the fixed footer entries, and shows the menu at (@p x, @p y).
 * Any previously open root menu is closed first.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to display the menu
 * @param x          Requested X origin (root coordinates)
 * @param y          Requested Y origin (root coordinates)
 * @param config     Active configuration
 * @param config_dir Path to the configuration directory (used to locate
 *                   @c menu.json)
 *
 * @note Complexity: @e O(n), where @e n is the total number of menu
 *       entries in @c menu.json
 */
void rootmenu_show(xcb_connection_t *connection,
        surface_td *surface, int16_t x, int16_t y,
        const config_td *config, const char *config_dir);

/**
 * @brief Close the root desktop menu
 *
 * Destroys the menu window and frees all resources allocated for the
 * JSON-loaded entries.
 *
 * @note Complexity: @e O(n)
 */
void rootmenu_close(void);

/**
 * @brief Repaint the root desktop menu
 *
 * @param win Window that received the expose event
 *
 * @note Complexity: @e O(n)
 */
void rootmenu_repaint(xcb_window_t win);

/**
 * @brief Handle a button-press event inside the root desktop menu
 *
 * @param connection XCB connection
 * @param surface    Surface associated with the event
 * @param win        Window that received the press
 * @param x          Pointer X relative to @p win
 * @param y          Pointer Y relative to @p win
 * @param config     Active configuration
 *
 * @return @c true if the event was consumed
 *
 * @note Complexity: @e O(1)
 */
bool rootmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int x, int y,
        const config_td *config);

/**
 * @brief Query whether the root desktop menu is currently open
 *
 * @return @c true when the menu is visible
 *
 * @note Complexity: @e O(1)
 */
bool rootmenu_is_open(void);

/**
 * @brief Return the root desktop menu XCB window
 *
 * @return The menu's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t rootmenu_window(void);

/**
 * @brief Check whether @p win belongs to the root menu hierarchy
 *
 * @param win XCB window to test
 *
 * @return @c true if @p win is part of the currently open menu
 *
 * @note Complexity: @e O(d)
 */
bool rootmenu_owns_window(xcb_window_t win);


#endif  /* ! MENU_CONTEXT_ROOTMENU_H */
