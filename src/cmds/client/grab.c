/**
 * @file cmds/client/grab.c
 *
 * @brief Passive mouse-button grabs on undecorated clients
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* size_t */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>

/* Local includes */
#include <cmds/client/internal.h>


/* Passively grab mouse buttons (excluding scroll wheel) on an
 * undecorated client.  Buttons 4 and 5 (scroll wheel) are intentionally
 * not grabbed so that scroll events are delivered directly to the
 * application. */
void ccmd_client_grab_buttons(client_td *client)
{
    static const xcb_button_t s_grab_buttons[] = {
        XCB_BUTTON_INDEX_1,
        XCB_BUTTON_INDEX_2,
        XCB_BUTTON_INDEX_3,
        6,
        7
    };
    size_t nb = sizeof(s_grab_buttons) / sizeof(s_grab_buttons[0]);

    if (client == NULL || client->connection == NULL ||
            client->window == XCB_WINDOW_NONE) {
        return;
    }

    for (size_t bi = 0; bi < nb; ++bi) {
        xcb_grab_button(client->connection,
                0,
                client->window,
                XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                XCB_GRAB_MODE_SYNC,
                XCB_GRAB_MODE_ASYNC,
                XCB_NONE,
                XCB_NONE,
                s_grab_buttons[bi],
                XCB_MOD_MASK_ANY);
    }
    xcb_flush(client->connection);
}


/* Remove passive button grabs from an undecorated client */
void ccmd_client_ungrab_buttons(client_td *client)
{
    if (client == NULL || client->connection == NULL ||
            client->window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_ungrab_button(client->connection,
            (uint8_t) XCB_BUTTON_INDEX_ANY,
            client->window,
            (uint16_t) XCB_MOD_MASK_ANY);
    xcb_flush(client->connection);
}
