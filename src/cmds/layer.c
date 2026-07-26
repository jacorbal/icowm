/**
 * @file cmds/layer.c
 *
 * @brief Client stacking-order command implementation
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

/* Project includes */
#include <client.h>

/* Local includes */
#include <cmds/layer.h>
#include <cmds/util.h>


/* Raise the client to the top of the stacking order */
void wcmd_client_raise(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_ABOVE };
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_flush(client->connection);
}


/* Lower the client to the bottom of the stacking order */
void wcmd_client_lower(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_BELOW };
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_flush(client->connection);
}


/* Place the client in the above layer */
void wcmd_client_layer_above(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_ABOVE };

    if (client == NULL) {
        return;
    }

    client->properties.layer = CLIENT_LAYER_ABOVE;
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_STACK_MODE, values);

    wcmd_rem_states(client, 1, "_NET_WM_STATE_BELOW");
    wcmd_add_states(client, 1, "_NET_WM_STATE_ABOVE");

    xcb_flush(client->connection);
}


/* Place the client in the normal (default) layer */
void wcmd_client_layer_normal(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_OPPOSITE };

    if (client == NULL) {
        return;
    }

    client->properties.layer = CLIENT_LAYER_NORMAL;
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_STACK_MODE, values);

    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_ABOVE",
            "_NET_WM_STATE_BELOW");

    xcb_flush(client->connection);
}


/* Place the client in the below layer */
void wcmd_client_layer_below(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_BELOW };

    if (client == NULL) {
        return;
    }

    client->properties.layer = CLIENT_LAYER_BELOW;
    xcb_configure_window(client->connection, client->window,
            XCB_CONFIG_WINDOW_STACK_MODE, values);

    wcmd_rem_states(client, 1, "_NET_WM_STATE_ABOVE");
    wcmd_add_states(client, 1, "_NET_WM_STATE_BELOW");

    xcb_flush(client->connection);
}
