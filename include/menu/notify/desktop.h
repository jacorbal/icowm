/**
 * @file menu/notify/desktop.h
 *
 * @brief Desktop-switch notification popup interface
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_NOTIFY_DESKTOP_H
#define MENU_NOTIFY_DESKTOP_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/**
 * @brief Show the desktop-switch notification popup centered on screen
 *
 * Formats a label from @p desktop_idx and @p desktop_name, then
 * delegates to the generic @c notify_popup_show_centered helper.
 *
 * @param connection   XCB connection
 * @param surface      Surface on which to center the popup
 * @param desktop_idx  Zero-based index of the newly active desktop
 * @param desktop_name Name of the newly active desktop, or @c NULL
 * @param config       Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void notify_desktop_show(xcb_connection_t *connection,
        surface_td *surface, uint32_t desktop_idx,
        const char *desktop_name, const config_td *config);

/**
 * @brief Destroy the currently visible desktop-switch notification
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void notify_desktop_close(xcb_connection_t *connection);

/**
 * @brief Repaint the desktop-switch notification from its cached text
 *
 * @param connection XCB connection
 * @param config     Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void notify_desktop_repaint(xcb_connection_t *connection,
        const config_td *config);

/**
 * @brief Query whether the desktop notification is currently visible
 *
 * @return @c true when the notification window exists
 *
 * @note Complexity: @e O(1)
 */
bool notify_desktop_is_open(void);

/**
 * @brief Return the desktop notification window identifier
 *
 * @return The notification's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t notify_desktop_window(void);

/**
 * @brief Return milliseconds remaining before the notification closes
 *
 * @return Remaining milliseconds, 0 if expired, or -1 on error
 *
 * @note Complexity: @e O(1)
 */
int notify_desktop_ms_remaining(void);


#endif  /* ! MENU_NOTIFY_DESKTOP_H */
