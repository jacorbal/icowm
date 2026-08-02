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
#include <wm.h>

/* Local includes */
#include <cmds/layer.h>
#include <cmds/util.h>


/* Raise the client to the top of the stacking order */
void wcmd_client_raise(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        (void) desktop_action_client_send_front(desktop, client);
        wcmd_desktop_enforce_layers(desktop);
    } else {
        uint32_t values[] = { XCB_STACK_MODE_ABOVE };
        xcb_window_t target = wcmd_target_win(client);
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_STACK_MODE, values);
        xcb_flush(client->connection);
    }

}


/* Lower the client to the bottom of the stacking order */
void wcmd_client_lower(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        (void) desktop_action_client_send_back(desktop, client);
        wcmd_desktop_enforce_layers(desktop);
    } else {
        uint32_t values[] = { XCB_STACK_MODE_BELOW };
        xcb_window_t target = wcmd_target_win(client);
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_STACK_MODE, values);
        xcb_flush(client->connection);
    }

}


/* Place the client in the above layer */
void wcmd_client_layer_above(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }
    client->properties.layer = CLIENT_LAYER_ABOVE;

    wcmd_rem_states(client, 1, "_NET_WM_STATE_BELOW");
    wcmd_add_states(client, 1, "_NET_WM_STATE_ABOVE");

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        wcmd_desktop_enforce_layers(desktop);
    }

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Place the client in the normal (default) layer */
void wcmd_client_layer_normal(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    client->properties.layer = CLIENT_LAYER_NORMAL;

    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_ABOVE",
            "_NET_WM_STATE_BELOW");

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        wcmd_desktop_enforce_layers(desktop);
    }

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Place the client in the below layer */
void wcmd_client_layer_below(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    client->properties.layer = CLIENT_LAYER_BELOW;

    wcmd_rem_states(client, 1, "_NET_WM_STATE_ABOVE");
    wcmd_add_states(client, 1, "_NET_WM_STATE_BELOW");

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        wcmd_desktop_enforce_layers(desktop);
    }

    wm_request_client_redraw(client);
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
    enum client_layer_e layer_order[] = {
        CLIENT_LAYER_BELOW,
        CLIENT_LAYER_NORMAL,
        CLIENT_LAYER_ABOVE
    };
    xcb_window_t prev_target;
    xcb_window_t target;

    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        return;
    }

    prev_target = XCB_WINDOW_NONE;
    for (size_t li = 0;
            li < sizeof(layer_order) / sizeof(layer_order[0]);
            ++li) {
        node = cdlist_head(desktop->stacking);
        initial = node;
        if (node == NULL) {
            continue;
        }

        do {
            c = (client_td *) cdlist_data(node);
            if (c != NULL &&
                    c->properties.layer == (uint16_t) layer_order[li]) {
                target = wcmd_target_win(c);
                if (target != XCB_WINDOW_NONE) {

                    if (prev_target == XCB_WINDOW_NONE) {
                        xcb_configure_window(c->connection, target,
                                XCB_CONFIG_WINDOW_STACK_MODE,
                                (const uint32_t[]) {
                                XCB_STACK_MODE_BELOW
                                });
                    } else {
                        xcb_configure_window(c->connection, target,
                                XCB_CONFIG_WINDOW_SIBLING |
                                XCB_CONFIG_WINDOW_STACK_MODE,
                                (const uint32_t[]) {
                                prev_target,
                                XCB_STACK_MODE_ABOVE
                                });
                    }
                    prev_target = target;
                }
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    if (desktop->connection != NULL) {
        xcb_flush(desktop->connection);
    }
}
