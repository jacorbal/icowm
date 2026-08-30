/**
 * @file menu/popup.h
 *
 * @brief Informational client popup window interface
 *
 * Declares the functions for showing, closing, and repainting the small
 * popup that displays basic information about the currently focused
 * client.  All popup state is private to the implementation.
 *
 * @defgroup menu Popup menu system
 * @ingroup wm
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
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/popup.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Show a popup near the client window with focused-client
 *        information
 *
 * Creates a popup window at the client's screen coordinates and
 * displays name, class, instance, window identifiers, geometry, and
 * state flags for @p client.  Any previously visible popup is closed
 * first.  The @p modifier is the modifier mask of the key binding that
 * triggered the popup; releasing it will auto-close the popup.
 *
 * @param connection XCB connection
 * @param surface    Surface where the popup should appear
 * @param desktop    Desktop associated with the client
 * @param client     Client to describe
 * @param modifier   Modifier mask of the opening key binding, or
 *                   @c 0 when it has none
 * @param keycode    Keycode of the opening key binding
 * @param cfg        Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void popup_show(xcb_connection_t *connection,
        surface_td *surface, const desktop_td *desktop,
        client_td *client,
        uint16_t modifier, xcb_keycode_t keycode, const config_td *cfg);

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

/**
 * @brief Return the milliseconds remaining before the popup auto-closes
 *
 * Computes the remaining time until @c WM_INFO_POPUP_TIMEOUT_MS has
 * elapsed since the popup was shown.  Returns 0 when the popup has
 * already expired, and -1 when no popup is currently open or the open
 * time was not recorded.
 *
 * @return Milliseconds until auto-close, @c 0 if expired, @c -1 if
 *         no popup
 *
 * @note Complexity: @e O(1)
 */
int popup_ms_remaining(void);


#endif  /* ! MENU_POPUP_H */
