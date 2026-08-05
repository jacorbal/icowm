/**
 * @file menu/context/winlist.h
 *
 * @brief Window list menu (middle-click on empty desktop)
 *
 * Displays a popup menu listing open windows grouped by desktop when
 * the user middle-clicks on the root window (empty desktop).
 *
 * Each desktop group is introduced by a non-clickable label entry of
 * the form "[index] -- <desktop name>" (or "[index]" if the name is
 * empty).  Each client window inside that group is listed as
 * a clickable command entry that focuses and raises the window when
 * activated.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_CONTEXT_WINLIST_H
#define MENU_CONTEXT_WINLIST_H


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
 * @brief Open the window list menu
 *
 * Iterates over all desktops on @p surface, and for each desktop that
 * has at least one client, adds a label entry for the desktop and one
 * command entry per client.  Any previously open window list menu is
 * closed first.
 *
 * @param connection XCB connection
 * @param surface    Surface whose clients are listed
 * @param x          Requested X origin (root coordinates)
 * @param y          Requested Y origin (root coordinates)
 * @param config     Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the total number of clients
 *       across all desktops
 */
void winlist_show(xcb_connection_t *connection,
        surface_td *surface, int16_t x, int16_t y,
        const config_td *config);

/**
 * @brief Close the window list menu
 *
 * @note Complexity: @e O(1)
 */
void winlist_close(void);

/**
 * @brief Repaint the window list menu
 *
 * @param win Window that received the expose event
 *
 * @note Complexity: @e O(n)
 */
void winlist_repaint(xcb_window_t win);

/**
 * @brief Handle a button-press event inside the window list menu
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
bool winlist_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int x, int y,
        const config_td *config);

/**
 * @brief Query whether the window list menu is currently open
 *
 * @return @c true when the menu is visible
 *
 * @note Complexity: @e O(1)
 */
bool winlist_is_open(void);

/**
 * @brief Return the window list menu XCB window
 *
 * @return The menu's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t winlist_window(void);

/**
 * @brief Check whether @p win belongs to the window list menu
 *
 * @param win XCB window to test
 *
 * @return @c true if @p win is the window list menu window
 *
 * @note Complexity: @e O(1)
 */
bool winlist_owns_window(xcb_window_t win);


#endif  /* ! MENU_CONTEXT_WINLIST_H */
