/**
 * @file menu/dialog/info.c
 *
 * @brief Informational message dialog thin wrapper implementation
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

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>

/* Local includes */
#include <menu/dialog.h>
#include <menu/dialog/info.h>


/* Open the informational message dialog */
void dialog_info_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *message, menu_msg_level_e level)
{
    menu_message_dialog_show(connection, surface, config,
            message, level);
}


/* Destroy the currently visible informational dialog */
void dialog_info_close(xcb_connection_t *connection)
{
    menu_message_dialog_close(connection);
}


/* Repaint the informational dialog from current state */
void dialog_info_repaint(xcb_connection_t *connection,
        const config_td *config)
{
    menu_message_dialog_repaint(connection, config);
}


/* Handle a mouse click inside the informational dialog */
void dialog_info_handle_click(xcb_connection_t *connection,
        int x, int y)
{
    menu_message_dialog_handle_click(connection, x, y);
}


/* Query whether the informational dialog is currently visible */
bool dialog_info_is_open(void)
{
    return menu_message_dialog_is_open();
}


/* Return the informational dialog window identifier */
xcb_window_t dialog_info_window(void)
{
    return menu_message_dialog_window();
}
