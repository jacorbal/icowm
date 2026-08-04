/**
 * @file menu/notify.h
 *
 * @brief Desktop-switch notification popup interface
 *
 * Declares the functions for showing and closing the transient centered
 * notification that appears when the active desktop changes.  The
 * notification displays the desktop index and name for a configurable
 * duration (@c WM_DESKTOP_NOTIFY_TIMEOUT_MS) and then closes
 * automatically.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_NOTIFY_H
#define MENU_NOTIFY_H


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
 * @brief Show the desktop-switch notification popup
 *
 * Creates a small centered window on @p surface that displays the
 * desktop index and name.  Any previously visible notification is
 * closed first.  The popup closes automatically after
 * @c WM_DESKTOP_NOTIFY_TIMEOUT_MS milliseconds.
 *
 * Format: "[index] -- Name" when the name is non-empty, or "[index]"
 * otherwise.
 *
 * @param connection   XCB connection
 * @param surface      Surface on which to display the notification
 * @param desktop_idx  Zero-based desktop index to display
 * @param desktop_name Desktop name string (may be empty or null)
 * @param cfg          Active configuration (theme colors and font)
 *
 * @note No-op when @c cfg->base.show_desktop_notify is @c false
 * @note Complexity: @e O(1)
 */
void notify_desktop_show(xcb_connection_t *connection,
        surface_td *surface, uint32_t desktop_idx,
        const char *desktop_name, const config_td *cfg);

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
 * Called from the expose handler when the notification window receives
 * an expose event.
 *
 * @param connection XCB connection
 * @param cfg        Active configuration (for font)
 *
 * @note Complexity: @e O(1)
 */
void notify_desktop_repaint(xcb_connection_t *connection,
        const config_td *cfg);

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
 * @return The window's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t notify_desktop_window(void);

/**
 * @brief Return the milliseconds remaining before auto-close
 *
 * @return Milliseconds until auto-close, 0 if expired, -1 if no
 *         notification is open or the open time was not recorded
 *
 * @note Complexity: @e O(1)
 */
int notify_desktop_ms_remaining(void);


#endif  /* ! MENU_NOTIFY_H */
