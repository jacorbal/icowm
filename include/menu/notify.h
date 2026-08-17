/**
 * @file menu/notify.h
 *
 * @brief Generic notification interfaces
 *
 * @defgroup menu_notify Desktop notifications
 * @ingroup menu
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
#include <time.h>       /* timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>

/* Default initial values */
#include <defs/desktop.h>


/**
 * @brief State shared by all notification popup instances
 *
 * Tracks the XCB window handle, the monotonic timestamp at which the
 * popup was last shown, and the text currently displayed inside it.
 */
struct notify_popup_state_s {
    xcb_window_t window;                        /**< XCB window
                                                     identifier, or
                                                     @c XCB_WINDOW_NONE */
    struct timespec open_time;                  /**< Monotonic time when
                                                     the popup was opened */
    char text[WM_DESKTOP_MAX_LENGTH_NAME + 16]; /**< Cached display text */

};


/* Public interface */
/**
 * @brief Destroy the notification popup window and reset its state
 *
 * @param connection XCB connection
 * @param state      Popup state to reset
 *
 * @note Complexity: @e O(1)
 */
void notify_popup_close(xcb_connection_t *connection,
        struct notify_popup_state_s *state);

/**
 * @brief Query whether the notification popup is currently visible
 *
 * @param state Popup state to inspect
 *
 * @return @c true when the popup window exists
 *
 * @note Complexity: @e O(1)
 */
bool notify_popup_is_open(const struct notify_popup_state_s *state);

/**
 * @brief Return the popup window identifier
 *
 * @param state Popup state to inspect
 *
 * @return The popup's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t
    notify_popup_window(const struct notify_popup_state_s *state);

/**
 * @brief Return milliseconds remaining before the popup should close
 *
 * @param state      Popup state to inspect
 * @param timeout_ms Total display duration in milliseconds
 *
 * @return Remaining milliseconds, 0 if expired, or -1 on error
 *
 * @note Complexity: @e O(1)
 */
int notify_popup_ms_remaining(const struct notify_popup_state_s *state,
        int timeout_ms);

/**
 * @brief Create and display a notification popup centered on the screen
 *
 * If a popup is already open it is destroyed before the new one is
 * created.  The text is cached in @p state for later repaints.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the popup
 * @param state      Popup state to initialize
 * @param text       Text to display inside the popup
 * @param config     Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void notify_popup_show_centered(xcb_connection_t *connection,
        surface_td *surface, struct notify_popup_state_s *state,
        const char *text, const config_td *config);

/**
 * @brief Repaint a centered notification popup from its cached text
 *
 * @param connection XCB connection
 * @param state      Popup state containing the cached text
 * @param config     Active configuration (for theme colors and font)
 *
 * @note Complexity: @e O(1)
 */
void notify_popup_repaint_centered(xcb_connection_t *connection,
        const struct notify_popup_state_s *state,
        const config_td *config);


#endif  /* ! MENU_NOTIFY_H */
