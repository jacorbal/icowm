/**
 * @file handler/map.c
 *
 * @brief X @c MAP_REQUEST, @c UNMAP_NOTIFY, and @c DESTROY_NOTIFY event
 *        handlers
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
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <invalidate.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Policy includes */
#include <policy/focus.h>
#include <policy/placement.h>

/* Input includes */
#include <input/drag.h>

/* Command includes */
#include <cmds/ccmd.h>
#include <cmds/layer.h>
#include <cmds/util.h>

/* Project includes */
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <handler.h>


/* Handle a 'MAP_REQUEST' event */
void handler_map_request(wm_td *wm, xcb_map_request_event_t *event)
{
    surface_td *surface;
    desktop_td *desktop;
    client_td *client;

    if (wm == NULL || event == NULL) {
        LOGGER_ERROR("Received null pointer in map request handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Map request event: window=0x%x, parent=0x%x",
            event->window, event->parent);

    if (lookup_find_client(wm->surfaces,
                event->window, NULL, NULL) != NULL) {
        LOGGER_TRACE("Window %#x already managed; mapping directly",
                event->window);

        xcb_map_window(wm->connection, event->window);
        xcb_flush(wm->connection);
        return;
    }

    surface = lookup_surface_for_root(wm->surfaces, event->parent);
    if (surface == NULL && !list_is_empty(wm->surfaces)) {
        surface = (surface_td *) list_data(list_head(wm->surfaces));
    }
    if (surface == NULL) {
        LOGGER_ERROR("No surface found for 'MAP_REQUEST' on root %#x",
                event->parent);
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        LOGGER_ERROR("No current desktop on surface %u; mapping without"
                " management", surface->id);

        xcb_map_window(wm->connection, event->window);
        xcb_flush(wm->connection);
        return;
    }

    client = client_manage(wm->connection, wm->ewmh,
            event->window, &wm->config->theme, &wm->config->base);
    if (client == NULL) {
        xcb_map_window(wm->connection, event->window);
        xcb_flush(wm->connection);
        return;
    }

    client->screen_id = surface->id;
    client->desktop_id = desktop->id;

    if (desktop_action_client_add(desktop, client) != 0) {
        LOGGER_ERROR("Failed to add client %#x to desktop %u",
                event->window, desktop->id);
        client->window = 0;
        client_destroy(client);
        xcb_map_window(wm->connection, event->window);
        xcb_flush(wm->connection);
        return;
    }

    /* Advertise the desktop this client belongs to per EWMH */
    if (wm->ewmh != NULL) {
        uint32_t did = (client->properties.flags & CLIENT_FLAG_STICKY)
            ? WM_DESKTOP_ID_ALL : desktop->id;

        xcb_change_property(wm->connection, XCB_PROP_MODE_REPLACE,
                client->window, wm->ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &did);
    }

    /* Refresh work area in case the new client declares struts */
    surface_refresh_workareas(surface);

    /* Dock/panel windows self-position; do not override their geometry */
    if (client->properties.type != (uint16_t) CLIENT_TYPE_DOCK) {
        place_apply(wm, surface, client);
    }

    /* ICCCM §4.1.2.4: honor 'WM_HINTS' initial_state when 'IconicState' */
    if (client->initial_iconic) {
        wcmd_client_iconify(client);
    } else {
        if (client->titlebar != 0) {
            xcb_map_window(wm->connection, client->titlebar);
        }

        if (client->frame != 0) {
            xcb_map_window(wm->connection, client->frame);
            xcb_map_window(wm->connection, client->window);
        } else {
            xcb_map_window(wm->connection, event->window);
        }

        if (wm->config->base.windows.focus.is_new_focused &&
                client_is_focusable(client)) {
            focus_apply(wm->surfaces, surface, desktop, client, true,
                    wm->config);
        }
    }

    /* Re-apply layer stacking so newly mapped windows do not obscure
     * clients already assigned to the above layer */
    wcmd_desktop_enforce_layers(desktop);

    wm_invalidate_surface(surface);
    wm_invalidate_desktop(desktop);
    xcb_flush(wm->connection);

    LOGGER_DEBUG("Mapped and adopted window %#x ('%s') on desktop %u",
            event->window, client->info.name, desktop->id);
}


/* Handle an 'UNMAP_NOTIFY' event */
void handler_unmap_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_unmap_notify_event_t *event)
{
    client_td *client;
    client_td *c;
    desktop_td *desktop;
    surface_td *surface;
    cdlist_item_td *node;
    cdlist_item_td *initial;
    bool focus_set;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in unmap handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Unmap notify event: window=0x%x", event->window);

    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);
    if (client != NULL) {
        if (event->window != client->window) {
            if (client->ignore_unmap > 0) {
                client->ignore_unmap--;
            }
            return;
        }

        if (client->ignore_unmap > 0) {
            client->ignore_unmap--;
            return;
        }

        if (desktop != NULL &&
                desktop->client_active_id == client->id) {
            desktop->client_active_id = 0;

            focus_set = false;
            /* Restore focus to the most recently used visible client */
            if (desktop->stacking != NULL) {
                node = cdlist_tail(desktop->stacking);
                initial = node;
                if (node != NULL) {
                    do {
                        c = (client_td *) cdlist_data(node);
                        if (c != NULL && c != client &&
                                !(c->properties.flags &
                                    CLIENT_FLAG_HIDDEN) &&
                                !client_is_shaded(c) &&
                                c->properties.state !=
                                    (uint16_t) CLIENT_STATE_ICONIFIED &&
                                    (c->properties.flags &
                                 CLIENT_FLAG_FOCUSABLE)) {
                            desktop->client_active_id = c->id;
                            xcb_set_input_focus(connection,
                                    XCB_INPUT_FOCUS_PARENT,
                                    c->window, XCB_CURRENT_TIME);
                            wm_invalidate_desktop(desktop);
                            wm_invalidate_surface(surface);
                            focus_set = true;
                            break;
                        }
                        node = cdlist_prev(node);
                    } while (node != NULL && node != initial);
                }
            }

            /* No suitable client found; release focus so keyboard grabs
             * on the root window keep firing after the last window
             * closes */
            if (!focus_set && connection != NULL) {
                xcb_set_input_focus(connection,
                        XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_CURRENT_TIME);
                wm_invalidate_desktop(desktop);
                wm_invalidate_surface(surface);
            }
        }

        /* Unmap decoration windows so they do not float without content.
         * Increment ignore_unmap for each WM-initiated unmap so the
         * resulting 'UnmapNotify' events do not re-enter this handler. */
        if (client->frame != 0) {
            client->ignore_unmap++;
            xcb_unmap_window(client->connection, client->frame);
        }
        if (client->titlebar != 0) {
            client->ignore_unmap++;
            xcb_unmap_window(client->connection, client->titlebar);
        }
        if (connection != NULL) {
            xcb_flush(connection);
        }
    }
}


/* Handle a 'DESTROY_NOTIFY' event */
void handler_destroy_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_destroy_notify_event_t *event)
{
    client_td *client;
    client_td *c;
    surface_td *surface;
    desktop_td *desktop;
    cdlist_item_td *node;
    cdlist_item_td *initial;
    bool focus_set;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in destroy handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Destroy notify event: window=0x%x", event->window);

    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);
    if (client == NULL) {
        return;
    }

    if (event->window != client->window) {
        return;
    }

    /* Cancel any drag that was using this client */
    if (drag_is_active() && drag_client() == client) {
        drag_cancel(connection, client);
    }

    if (desktop != NULL && desktop->client_active_id == client->id) {
        desktop->client_active_id = 0;
        focus_set = false;

        /* Restore focus to the most recently used visible client.
         * Must happen before 'desktop_action_client_rem' removes the
         * client from the stacking list so it can be skipped by
         * pointer. */
        if (desktop->stacking != NULL) {
            node = cdlist_tail(desktop->stacking);
            initial = node;
            if (node != NULL) {
                do {
                    c = (client_td *) cdlist_data(node);
                    if (c != NULL && c != client &&
                            !(c->properties.flags &
                                CLIENT_FLAG_HIDDEN) &&
                            !client_is_shaded(c) &&
                            c->properties.state !=
                            (uint16_t) CLIENT_STATE_ICONIFIED &&
                            (c->properties.flags &
                                 CLIENT_FLAG_FOCUSABLE)) {
                        desktop->client_active_id = c->id;
                        xcb_set_input_focus(connection,
                                XCB_INPUT_FOCUS_PARENT,
                                c->window, XCB_CURRENT_TIME);
                        wm_invalidate_desktop(desktop);
                        wm_invalidate_surface(surface);
                        focus_set = true;
                        break;
                    }
                    node = cdlist_prev(node);
                } while (node != NULL && node != initial);
            }
        }

        /* No suitable client found; release focus so keyboard grabs on
         * the root window keep firing after the last window closes */
        if (!focus_set && connection != NULL) {
            xcb_set_input_focus(connection,
                    XCB_INPUT_FOCUS_POINTER_ROOT,
                    XCB_INPUT_FOCUS_POINTER_ROOT,
                    XCB_CURRENT_TIME);
            xcb_flush(connection);
            wm_invalidate_desktop(desktop);
            wm_invalidate_surface(surface);
        }
    }

    if (desktop != NULL) {
        desktop_action_client_rem(desktop, client);
        /* Refresh work area in case the removed client had struts */
        surface_refresh_workareas(surface);
    }

    /* ICCCM withdrawn state: remove WM_STATE on unmanage */
    wcmd_clear_wm_state(client);

    /* When the frame is destroyed the X server also destroys all its
     * children ('client->window', 'client->titlebar').  Zero them all
     * out so client_destroy does not issue redundant
     * 'xcb_destroy_window' calls. */
    if (event->window == client->frame) {
        client->frame = 0;
        client->titlebar = 0;
        client->window = 0;
    } else {
        /* The content window was destroyed (e.g., the client process
         * exited or the app closed without a prior 'UnmapNotify').
         * Immediately destroy the WM-created frame (which takes its
         * titlebar child with it) so no ghost frame is left on screen.
         * Increment 'ignore_unmap' so the 'UnmapNotify' the X server
         * generates for the mapped frame is swallowed and does not
         * re-enter the unmap handler.  Zero both pointers to prevent
         * client_destroy from issuing redundant destroy calls. */
        if (connection != NULL && client->frame != 0) {
            client->ignore_unmap++;
            xcb_destroy_window(connection, client->frame);
            xcb_flush(connection);
        }
        client->frame = 0;
        client->titlebar = 0;
        client->window = 0;
    }
    client_destroy(client);

    wm_invalidate_surface(surface);
    wm_invalidate_desktop(desktop);

    LOGGER_DEBUG("Removed destroyed window %#x", event->window);
}
