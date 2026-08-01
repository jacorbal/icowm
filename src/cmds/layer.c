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

/* ADT includes */
#include <adt/cdlist.h>

/* Project includes */
#include <client.h>
#include <desktop.h>

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
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    client->properties.layer = CLIENT_LAYER_ABOVE;
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_STACK_MODE, values);

    wcmd_rem_states(client, 1, "_NET_WM_STATE_BELOW");
    wcmd_add_states(client, 1, "_NET_WM_STATE_ABOVE");

    xcb_flush(client->connection);
}


/* Place the client in the normal (default) layer */
void wcmd_client_layer_normal(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client->properties.layer = CLIENT_LAYER_NORMAL;

    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_ABOVE",
            "_NET_WM_STATE_BELOW");

    xcb_flush(client->connection);
}


/* Place the client in the below layer */
void wcmd_client_layer_below(client_td *client)
{
    uint32_t values[] = { XCB_STACK_MODE_BELOW };
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    client->properties.layer = CLIENT_LAYER_BELOW;
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_STACK_MODE, values);

    wcmd_rem_states(client, 1, "_NET_WM_STATE_ABOVE");
    wcmd_add_states(client, 1, "_NET_WM_STATE_BELOW");

    xcb_flush(client->connection);
}


/* Cycle layer: 'normal -> above -> below -> normal -> above -> ...' */
void wcmd_client_cycle_layer(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client->properties.layer == CLIENT_LAYER_NORMAL) {
        wcmd_client_layer_above(client);
    } else if (client->properties.layer == CLIENT_LAYER_ABOVE) {
        wcmd_client_layer_below(client);
    } else {
        wcmd_client_layer_normal(client);
    }
}


/* Enforce layer stacking order for all clients in a desktop */
void wcmd_desktop_enforce_layers(desktop_td *desktop)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    client_td *c;
    uint32_t raise_vals[1];
    uint32_t lower_vals[1];
    xcb_window_t target;

    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        return;
    }

    raise_vals[0] = XCB_STACK_MODE_ABOVE;
    lower_vals[0] = XCB_STACK_MODE_BELOW;

    /* First pass: lower 'CLIENT_LAYER_BELOW' windows */
    node = cdlist_head(desktop->stacking);
    initial = node;
    if (node != NULL) {
        do {
            c = (client_td *) cdlist_data(node);
            if (c != NULL &&
                    c->properties.layer == CLIENT_LAYER_BELOW) {
                target = wcmd_target_win(c);
                if (target != XCB_WINDOW_NONE) {
                    xcb_configure_window(c->connection, target,
                            XCB_CONFIG_WINDOW_STACK_MODE, lower_vals);
                }
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    /* Second pass: raise 'CLIENT_LAYER_ABOVE' windows */
    node = cdlist_head(desktop->stacking);
    initial = node;
    if (node != NULL) {
        do {
            c = (client_td *) cdlist_data(node);
            if (c != NULL &&
                    c->properties.layer == CLIENT_LAYER_ABOVE) {
                target = wcmd_target_win(c);
                if (target != XCB_WINDOW_NONE) {
                    xcb_configure_window(c->connection, target,
                            XCB_CONFIG_WINDOW_STACK_MODE, raise_vals);
                }
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    if (desktop->connection != NULL) {
        xcb_flush(desktop->connection);
    }
}
