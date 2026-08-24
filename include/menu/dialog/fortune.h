/**
 * @file menu/dialog/fortune.h
 *
 * @brief The @c fortune easter egg, shown through the message dialog
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

#ifndef MENU_FORTUNE_H
#define MENU_FORTUNE_H


/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <types/handles.h>
#include <defs/dialog.h>

/* Project includes */


/**
 * @brief Open a message dialog showing the output of the @c fortune
 *        command, or an invitation to install it if unavailable
 *
 * Runs @c fortune and shows its output through
 * @a menu_message_dialog_show, which already wraps arbitrarily long or
 * multi-line text across several lines, so a long fortune (or
 * a one-word-short one) is handled the same general way any other
 * message would be.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the dialog
 * @param config     Active configuration (theme colors and font)
 *
 * @note If @c fortune is not installed or exits without producing
 *       output, shows a fixed, deliberately archaic yet charming
 *       message inviting the user to install it rather of silently
 *       doing nothing
 * @note Complexity: @e O(n), where @e n is the length of the @c fortune
 *       output actually read (bounded by @c DIALOG_FORTUNE_MAX_LENGTH)
 */
void dialog_fortune_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config);


#endif  /* ! MENU_FORTUNE_H */
