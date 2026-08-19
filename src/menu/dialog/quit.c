/**
 * @file menu/dialog/quit.c
 *
 * @brief Quit-confirmation dialog thin wrapper implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdio.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/ewmh.h>

/* Project includes */
#include <config.h>
#include <surface.h>
#include <wm.h>

/* Defs includes */
#include <defs/uistr.h>
#include <i18n.h>

/* Local includes */
#include <menu/dialog/confirm.h>
#include <menu/dialog/quit.h>
#include <menu/draw.h>


/**
 * @brief Callback invoked by the generic confirm dialog when exit is
 *        confirmed
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
static void s_on_quit_confirm(xcb_connection_t *connection)
{
    (void) connection;
    wm_request_graceful_stop();
}


/* Open the quit-confirmation dialog */
void dialog_quit_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *cfg)
{
    char prompt[DIALOG_QUIT_PROMPT_MAX_LENGTH];

    (void) snprintf(prompt, sizeof(prompt),
            _(STR_DIALOG_QUIT_PROMPT_FMT), WM_EWMH_NAME);

    menu_confirm_dialog_show(connection, surface, cfg,
            prompt, _(STR_DIALOG_QUIT_CANCEL), _(STR_DIALOG_QUIT_EXIT),
            s_on_quit_confirm, NULL, 0u);

}
