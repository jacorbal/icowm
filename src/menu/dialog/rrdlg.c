/**
 * @file menu/dialog/rrdlg.c
 *
 * @brief RandR output-profile confirm dialog (thin wrapper)
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/dialog.h>
#include <defs/uistr.h>

/* Project includes */
#include <config.h>
#include <surface.h>

/* Local includes */
#include <menu/dialog/confirm.h>
#include <menu/dialog/rrdlg.h>


/**
 * @brief Callback invoked by the generic confirm dialog when the
 *        just-applied RandR profile is confirmed (kept)
 *
 * Nothing to do: the profile was already applied to the X server
 * before this dialog was even shown (see @c wm_action_config_reload);
 * confirming just means not reverting it.
 *
 * @param connection XCB connection
 */
static void s_on_rrdlg_confirm(xcb_connection_t *connection)
{
    (void) connection;
}


/**
 * @brief Callback invoked by the generic confirm dialog when the
 *        just-applied RandR profile is cancelled -- by a person, by
 *        Escape, or by the countdown elapsing
 *
 * @param connection XCB connection
 */
static void s_on_rrdlg_cancel(xcb_connection_t *connection)
{
    (void) connection;
    surface_action_revert_randr_profiles();
}


/* Open the RandR output-profile confirm dialog */
void dialog_rrdlg_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config)
{
    menu_confirm_dialog_show(connection, surface, config,
            STR_DIALOG_RANDR_CONFIRM_PROMPT,
            STR_DIALOG_RANDR_CONFIRM_CANCEL,
            STR_DIALOG_RANDR_CONFIRM_OK,
            s_on_rrdlg_confirm, s_on_rrdlg_cancel,
            DIALOG_RANDR_CONFIRM_TIMEOUT_SECONDS);
}
