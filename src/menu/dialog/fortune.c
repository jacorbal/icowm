/**
 * @file menu/dialog/fortune.c
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

#define _POSIX_C_SOURCE 200112L /* popen, pclose */


/* System includes */
#include <stddef.h>     /* NULL, size_t */
#include <stdio.h>      /* popen, pclose, FILE, fread */

/* XCB includes */
#include <xcb/xcb.h>

/* Util includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <config.h>
#include <surface.h>

/* Local includes */
#include <menu/dialog/fortune.h>
#include <menu/dialog/message.h>


/* Show the output of 'fortune', or an invitation to install it */
void dialog_fortune_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config)
{
    char buffer[DIALOG_FORTUNE_MAX_LEN];
    FILE *pipe;
    size_t len;
    const char *text;

    if (connection == NULL || surface == NULL || config == NULL) {
        return;
    }

    buffer[0] = '\0';

    /* Redirect stderr to /dev/null so a missing binary's shell
     * "command not found" complaint never ends up as this dialog's
     * text; an empty read is exactly what should fall through to the
     * fallback message below regardless of why it came up empty. */
    pipe = popen("fortune 2>/dev/null", "r");
    if (pipe != NULL) {
        size_t n = fread(buffer, 1u, sizeof(buffer) - 1u, pipe);

        buffer[n] = '\0';
        (void) pclose(pipe);
    }

    /* Trim the trailing newline(s) 'fortune' output typically ends
     * with, so wrapping does not leave a visibly blank final line at
     * the bottom of the dialog. */
    len = safe_strlen(buffer);
    while (len > 0u &&
            (buffer[len - 1u] == '\n' || buffer[len - 1u] == '\r' ||
             buffer[len - 1u] == ' ' || buffer[len - 1u] == '\t')) {
        buffer[--len] = '\0';
    }

    text = (len > 0u) ? buffer : DIALOG_FORTUNE_FALLBACK_MSG;

    menu_message_dialog_show(connection, surface, config,
            text, MENU_MSG_LEVEL_NONE);
}
