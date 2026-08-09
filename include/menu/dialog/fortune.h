/**
 * @file menu/dialog/fortune.h
 *
 * @brief The @c fortune easter egg, shown through the message dialog
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

/* Project includes */
#include <config.h>
#include <surface.h>


/** Maximum bytes read from the @c fortune command's output */
#define DIALOG_FORTUNE_MAX_LEN (1024u)

/** Shown instead when @c fortune is missing or produces no output;
 *  deliberately overwrought and archaic, per its whole point being a
 *  small joke rather than a plain error message */
#define DIALOG_FORTUNE_FALLBACK_MSG \
    "Alack!  The oracle 'fortune' abideth not upon this machine, " \
    "wherefore no wisdom of the ancients may this day be divined.  " \
    "Prithee, entreat thy package steward with an incantation " \
    "such as 'sudo apt install fortune-mod' (or whate'er charm " \
    "thy distribution demandeth), that the sages of yore might " \
    "once more speak through this humble dialog."


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
 *       @c DIALOG_FORTUNE_MAX_LEN above)
 */
void dialog_fortune_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config);


#endif  /* ! MENU_FORTUNE_H */
