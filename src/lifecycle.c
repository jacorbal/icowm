/**
 * @file lifecycle.c
 *
 * @brief Window manager startup scan, client name refresh, and launch
 *        dispatch
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/list.h>

/* Default initial values */
#include <defs/config.h>

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <lookup.h>
#include <priority.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <lifecycle.h>


/* Adopt all pre-existing mapped windows at window manager startup */
void lifecycle_scan_existing(wm_td *wm)
{
    if (wm == NULL) {
        return;
    }

    for (list_item_td *node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        xcb_query_tree_cookie_t qt_cookie;
        xcb_query_tree_reply_t *qt_reply;
        xcb_window_t *children;
        int nchildren;

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        qt_cookie = xcb_query_tree(wm->connection,
                surface->screen->root);
        qt_reply = xcb_query_tree_reply(wm->connection,
                qt_cookie, NULL);
        if (qt_reply == NULL) {
            continue;
        }

        children = xcb_query_tree_children(qt_reply);
        nchildren = xcb_query_tree_children_length(qt_reply);

        for (int i = 0; i < nchildren; ++i) {
            xcb_get_window_attributes_cookie_t ac;
            xcb_get_window_attributes_reply_t *ar;

            ac = xcb_get_window_attributes(wm->connection, children[i]);
            ar = xcb_get_window_attributes_reply(
                    wm->connection, ac, NULL);

            if (ar == NULL) {
                continue;
            }

            if (!ar->override_redirect &&
                    ar->map_state == XCB_MAP_STATE_VIEWABLE) {
                desktop_td *desktop =
                    lookup_current_desktop(surface);
                if (desktop != NULL) {
                    client_td *client = client_manage(
                            wm->connection, wm->ewmh,
                            children[i], &wm->config->theme,
                            &wm->config->base);
                    if (client != NULL) {
                        client->screen_id = surface->id;
                        client->desktop_id = desktop->id;
                        desktop_action_client_add(desktop, client);
                        if (wm->ewmh != NULL) {
                            uint32_t did = desktop->id;
                            xcb_change_property(wm->connection,
                                    XCB_PROP_MODE_REPLACE,
                                    client->window,
                                    wm->ewmh->_NET_WM_DESKTOP,
                                    XCB_ATOM_CARDINAL, 32, 1, &did);
                        }
                        surface->is_outdated = true;
                    }
                }
            }

            free(ar);
        }

        free(qt_reply);
    }

    xcb_flush(wm->connection);
}


/* Dispatch a program-launch event on the active desktop */
int lifecycle_send_desktop_launch(desktop_td *desktop,
        const char *command)
{
    action_td action;
    action_data_desktop_td *data;
    event_td *event;

    if (desktop == NULL || command == NULL || command[0] == '\0') {
        LOGGER_WARNING("Cannot launch: command is null or empty",
                L_NARG);
        return -1;
    }

    action.type = ACTION_TYPE_DESKTOP;
    action.object.desktop = ACTION_DESKTOP_COMMAND_LAUNCH;

    data = action_data_desktop_init(desktop, action.object.desktop);
    if (data == NULL) {
        LOGGER_ERROR("Failed to allocate desktop action data", L_NARG);
        return 1;
    }
    /* Command pointer originates from persistent config; valid for the
     * lifetime of the event queue */
    data->new_data.str = (char *) command;

    event = event_init((void *) desktop, (void *) data,
            action, PRIORITY_NORMAL);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create desktop launch event", L_NARG);
        action_data_desktop_destroy(data);
        return 1;
    }

    if (eventq_add(event) != 0) {
        LOGGER_ERROR("Failed to queue desktop launch event", L_NARG);
        event_destroy(event);
        return 1;
    }

    return 0;
}


/* Build and enqueue a launch event for a desktop */
void lifecycle_dispatch_launch(surface_td *surface, const char *prog)
{
    desktop_td *desktop;

    if (surface == NULL || prog == NULL || prog[0] == '\0') {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop != NULL) {
        (void) lifecycle_send_desktop_launch(desktop, prog);
    }
}
