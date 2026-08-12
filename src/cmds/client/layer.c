/**
 * @file cmds/client/layer.c
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
#include <logger.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/layer.h>
#include <cmds/client/internal.h>


/* Raise the client to the top of the stacking order */
void ccmd_client_raise(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Raising client window=0x%x", client->window);

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        (void) desktop_action_client_send_front(desktop, client);
        ccmd_desktop_enforce_layers(desktop);
    } else {
        uint32_t values[] = { XCB_STACK_MODE_ABOVE };
        xcb_window_t target = ccmd_target_win(client);
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_STACK_MODE, values);
        xcb_flush(client->connection);
    }

}


/* Lower the client to the bottom of the stacking order */
void ccmd_client_lower(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Lowering client window=0x%x", client->window);

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        (void) desktop_action_client_send_back(desktop, client);
        ccmd_desktop_enforce_layers(desktop);
    } else {
        uint32_t values[] = { XCB_STACK_MODE_BELOW };
        xcb_window_t target = ccmd_target_win(client);
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_STACK_MODE, values);
        xcb_flush(client->connection);
    }
}


/**
 * @brief Enforce layer stacking and request a redraw after a client's
 *        layer changes
 *
 * Shared by @c ccmd_client_layer_above, @c ccmd_client_layer_normal,
 * and @c ccmd_client_layer_below below, which only differ in the new
 * @c client->properties.layer value and which @c _NET_WM_STATE atoms
 * to add or remove for it.
 *
 * @param client  Client whose layer just changed
 * @param desktop Desktop @p client is on, or @c NULL to skip
 *                re-enforcing layer stacking
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop (see @c ccmd_desktop_enforce_layers)
 */
static void s_client_layer_finish(client_td *client, desktop_td *desktop)
{
    if (desktop != NULL) {
        ccmd_desktop_enforce_layers(desktop);
    }

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Place the client in the above layer */
void ccmd_client_layer_above(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Setting client window=0x%x to layer 'above'",
            client->window);
    client->properties.layer = CLIENT_LAYER_ABOVE;

    ccmd_rem_states(client, 1, "_NET_WM_STATE_BELOW");
    ccmd_add_states(client, 1, "_NET_WM_STATE_ABOVE");

    desktop = wm_get_client_desktop(client);
    s_client_layer_finish(client, desktop);
}


/* Place the client in the normal (default) layer */
void ccmd_client_layer_normal(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Setting client window=0x%x to layer 'normal'",
            client->window);
    client->properties.layer = CLIENT_LAYER_NORMAL;

    ccmd_rem_states(client, 2,
            "_NET_WM_STATE_ABOVE",
            "_NET_WM_STATE_BELOW");

    desktop = wm_get_client_desktop(client);
    s_client_layer_finish(client, desktop);
}


/* Place the client in the below layer */
void ccmd_client_layer_below(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Setting client window=0x%x to layer 'below'",
            client->window);
    client->properties.layer = CLIENT_LAYER_BELOW;

    ccmd_rem_states(client, 1, "_NET_WM_STATE_ABOVE");
    ccmd_add_states(client, 1, "_NET_WM_STATE_BELOW");

    desktop = wm_get_client_desktop(client);
    s_client_layer_finish(client, desktop);
}


/* Cycle layer: 'normal -> above -> below -> normal -> above -> ...' */
void ccmd_client_cycle_layer(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client->properties.layer == CLIENT_LAYER_NORMAL) {
        ccmd_client_layer_above(client);
    } else if (client->properties.layer == CLIENT_LAYER_ABOVE) {
        ccmd_client_layer_below(client);
    } else {
        ccmd_client_layer_normal(client);
    }
}


/* Enforce layer stacking order for all clients in a desktop */
void ccmd_desktop_enforce_layers(desktop_td *desktop)
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
                target = ccmd_target_win(c);
                if (target != XCB_WINDOW_NONE) {

                    if (prev_target == XCB_WINDOW_NONE) {
                        /* The first client found across the whole
                         * loop, whichever layer it happens to be in:
                         * anchor it explicitly just above the tray
                         * (if any) rather than an unqualified 'below'
                         * with no sibling.  An unqualified 'below'
                         * claims the absolute bottom of the entire
                         * sibling stack, out from under the desktop
                         * icons and tray already sitting there --
                         * this was previously only done when that
                         * first client was genuinely in the BELOW
                         * layer (on the mistaken assumption that a
                         * desktop with no BELOW-layer clients at all
                         * had "no business" anchoring to the tray),
                         * which left every desktop with no BELOW
                         * clients (the common case) sending its first
                         * NORMAL-layer client to the absolute bottom
                         * instead, visibly shoving the icons and tray
                         * up out of the way on every call (client
                         * creation, raise/lower, and desktop
                         * activation all call this). */
                        xcb_window_t tray_below = systray_below_window();

                        if (tray_below != XCB_WINDOW_NONE) {
                            xcb_configure_window(c->connection, target,
                                    XCB_CONFIG_WINDOW_SIBLING |
                                    XCB_CONFIG_WINDOW_STACK_MODE,
                                    (const uint32_t[]) {
                                    tray_below,
                                    XCB_STACK_MODE_ABOVE
                                    });
                        } else {
                            xcb_configure_window(c->connection, target,
                                    XCB_CONFIG_WINDOW_STACK_MODE,
                                    (const uint32_t[]) {
                                    XCB_STACK_MODE_BELOW
                                    });
                        }
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
