/**
 * @file menu/popup.h
 *
 * @brief Informational client popup window interface
 *
 * Declares the functions for showing, closing, and repainting the
 * small popup that displays basic information about the currently
 * focused client.  All popup state is private to the implementation.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_POPUP_H
#define MENU_POPUP_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <surface.h>


/* Public interface */
/**
 * @brief Show a small centered popup with focused-client information
 *
 * Creates a popup window centered on @p surface and displays name,
 * class, instance, window identifiers, geometry, and state flags for
 * @p client.  Any previously visible popup is closed first.
 *
 * @param connection XCB connection
 * @param surface    Surface where the popup should appear
 * @param desktop    Desktop associated with the client
 * @param client     Client to describe
 * @param cfg        Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void popup_show(xcb_connection_t *connection,
        surface_td *surface,
        desktop_td *desktop,
        client_td *client,
        const config_td *cfg);

/**
 * @brief Destroy the currently visible info popup
 *
 * Closes the info popup window if it is open and resets the cached
 * window identifier.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void popup_close(xcb_connection_t *connection);

/**
 * @brief Repaint the info popup from its cached text lines
 *
 * Called from the expose handler when the popup window receives an
 * expose event.  Redraws all four cached text lines.
 *
 * @param connection XCB connection
 * @param cfg        Active configuration (for font)
 *
 * @note Complexity: @e O(1)
 */
void popup_repaint(xcb_connection_t *connection,
        const config_td *cfg);

/**
 * @brief Query whether the info popup is currently visible
 *
 * @return @c true when the popup window exists
 *
 * @note Complexity: @e O(1)
 */
bool popup_is_open(void);

/**
 * @brief Return the info popup window identifier
 *
 * @return The popup's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t popup_window(void);


#endif  /* ! MENU_POPUP_H */
