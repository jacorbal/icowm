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
 */
static void s_on_quit_confirm(xcb_connection_t *connection)
{
    (void) connection;
    (void) wm_request_stop();
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


/* Destroy the quit-confirmation dialog */
void dialog_quit_close(xcb_connection_t *connection)
{
    menu_confirm_dialog_close(connection);
}


/* Repaint the quit-confirmation dialog */
void dialog_quit_repaint(xcb_connection_t *connection,
        const config_td *config)
{
    menu_confirm_dialog_repaint(connection, config);
}


/* Handle a mouse click in the quit-confirmation dialog */
bool dialog_quit_handle_click(xcb_connection_t *connection,
        const config_td *config, int x, int y)
{
    return menu_confirm_dialog_handle_click(connection, config, x, y);
}


/* Cycle to the next button */
void dialog_quit_toggle_selection(void)
{
    menu_confirm_dialog_toggle_selection();
}


/* Activate the currently selected button */
void dialog_quit_accept(xcb_connection_t *connection)
{
    menu_confirm_dialog_accept(connection);
}


/* Query whether the confirmation dialog is currently visible */
bool dialog_quit_is_open(void)
{
    return menu_confirm_dialog_is_open();
}


/* Return the confirmation dialog window identifier */
xcb_window_t dialog_quit_window(void)
{
    return menu_confirm_dialog_window();
}
