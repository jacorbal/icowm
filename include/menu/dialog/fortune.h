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
#include <defs/dialog.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/**
 * @brief Open a message dialog showing the output of the @c fortune
 *        command, or an invitation to install it if unavailable
 *
 * Runs @c fortune and shows its output through
 * @c menu_message_dialog_show, which already wraps arbitrarily long
 * or multi-line text across several lines, so a long fortune (or a
 * one-word-short one) is handled the same general way any other
 * message would be.  If @c fortune is not installed or exits without
 * producing output, shows a fixed, deliberately archaic message
 * inviting the person to install it instead of silently doing
 * nothing.
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the dialog
 * @param config     Active configuration (theme colors and font)
 *
 * @note Complexity: @e O(n), where @e n is the length of the
 *       @c fortune output actually read (bounded, see
 *       @c DIALOG_FORTUNE_MAX_LENGTH above)
 */
void dialog_fortune_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config);


#endif  /* ! MENU_FORTUNE_H */
