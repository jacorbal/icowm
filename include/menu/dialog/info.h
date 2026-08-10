/**
 * @file menu/dialog/info.h
 *
 * @brief Informational message dialog thin wrapper
 *
 * Provides a single-entry-point API for displaying a read-only
 * informational dialog with a given message and an alert level.  The
 * dialog has only one button ("OK") to dismiss it.
 *
 * The implementation is a thin wrapper around the generic
 * @c menu_message_dialog API declared in @c menu/dialog/message.h;
 * all layout and rendering logic lives there.
 *
 * Example usage:
 * @code
 * dialog_info_show(connection, surface, config,
 *         "Cannot launch 'xterm': command not found",
 *         MENU_MSG_LEVEL_WARNING);
 * @endcode
 *
 * @ingroup menu_dialog
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef MENU_DIALOG_INFO_H
#define MENU_DIALOG_INFO_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>

/* Menu includes */
#include <menu/dialog/message.h>    /* menu_msg_level_e */


/* Public interface */
/**
 * @brief Open the informational message dialog
 *
 * Displays a modal dialog showing @p message with a level prefix
 * determined by @p level.  A single "OK" button dismisses the dialog.
 * Any previously open message dialog is replaced.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the dialog
 * @param config     Active configuration (theme colors and font)
 * @param message    Null-terminated message text to display
 * @param level      Alert severity level (@c MENU_MSG_LEVEL_NONE,
 *                   @c MENU_MSG_LEVEL_INFO,
 *                   @c MENU_MSG_LEVEL_WARNING, or
 *                   @c MENU_MSG_LEVEL_ERROR)
 *
 * @note Complexity: @e O(1)
 */
void dialog_info_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *message, menu_msg_level_e level);

/**
 * @brief Destroy the currently visible informational dialog
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void dialog_info_close(xcb_connection_t *connection);

/**
 * @brief Repaint the informational dialog from current state
 *
 * @param connection XCB connection
 * @param config     Active configuration
 *
 * @note Complexity: @e O(1)
 */
void dialog_info_repaint(xcb_connection_t *connection,
        const config_td *config);

/**
 * @brief Handle a mouse click inside the informational dialog
 *
 * Closes the dialog when the pointer lands inside the "OK" button.
 *
 * @param connection XCB connection
 * @param x          Pointer X coordinate relative to the dialog
 * @param y          Pointer Y coordinate relative to the dialog
 *
 * @note Complexity: @e O(1)
 */
void dialog_info_handle_click(xcb_connection_t *connection,
        int x, int y);

/**
 * @brief Query whether the informational dialog is currently visible
 *
 * @return @c true when the dialog window exists
 *
 * @note Complexity: @e O(1)
 */
bool dialog_info_is_open(void);

/**
 * @brief Return the informational dialog window identifier
 *
 * @return The dialog's @c xcb_window_t, or @c XCB_WINDOW_NONE
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t dialog_info_window(void);


#endif  /* ! MENU_DIALOG_INFO_H */
