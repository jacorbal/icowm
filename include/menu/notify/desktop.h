/**
 * @file menu/notify/desktop.h
 *
 * @brief Desktop-switch notification popup interface
 *
 * @ingroup menu_notify
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
#include <types/handles.h>


/**
 * @brief What moved the view, and so which @c overlay setting decides
 *        whether it is announced
 */
enum notify_desktop_cause_e {
    NOTIFY_DESKTOP_CAUSE_SWITCH = 0, /**< A different desktop */
    NOTIFY_DESKTOP_CAUSE_VIEWPORT    /**< A whole-page viewport move */
};

/**
 * @brief Show the notification popup naming where the view has moved
 *        to, centered on screen
 *
 * Names only what there is to name: the desktop when the surface has
 * more than one, the viewport page when the grid holds more than one,
 * both when both, and nothing at all when neither, in which case no
 * popup is shown.  A desktop that carries a name is announced by it,
 * ahead of the coordinates.
 *
 * @param connection   XCB connection
 * @param surface      Surface on which to center the popup
 * @param desktop_idx  Zero-based index of the active desktop
 * @param desktop_name Name of the active desktop, or @c NULL
 * @param cause        What moved the view, deciding which of the two
 *                     @c overlay settings gates this call
 * @param config       Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void notify_desktop_show(xcb_connection_t *connection,
        surface_td *surface, uint32_t desktop_idx,
        const char *desktop_name,
        enum notify_desktop_cause_e cause, const config_td *config);

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
 * @return Remaining milliseconds, @c 0 if expired, or @c -1 on error
 *
 * @note Complexity: @e O(1)
 */
int notify_desktop_ms_remaining(void);


#endif  /* ! MENU_NOTIFY_DESKTOP_H */
