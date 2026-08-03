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
#include <input/mouse/drag.h>

/* Command includes */
#include <cmds/ccmd.h>
#include <cmds/layer.h>
#include <cmds/util.h>

/* Project includes */
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <handler.h>


/**
 * @brief Restore focus after the active client disappears from a desktop
 *
 * Selects the most recent visible focusable client in reverse stacking
 * order and focuses it.  If none is found, focus is released to pointer
 * root so keyboard grabs continue to work.
 */
static void s_restore_focus_after_client_loss(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, client_td *lost_client)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    bool focus_set = false;

    if (desktop == NULL || desktop->stacking == NULL) {
        return;
    }

    node = cdlist_tail(desktop->stacking);
    initial = node;
    if (node != NULL) {
        do {
            client_td *c = (client_td *) cdlist_data(node);
            if (c != NULL && c != lost_client &&
                    !(c->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    !client_is_shaded(c) &&
                    c->properties.state !=
                        (uint16_t) CLIENT_STATE_ICONIFIED &&
                    (c->properties.flags & CLIENT_FLAG_FOCUSABLE)) {
                desktop->client_active_id = c->id;

                if (connection != NULL) {
                    xcb_set_input_focus(connection,
                            XCB_INPUT_FOCUS_PARENT,
                            c->window, XCB_CURRENT_TIME);
                }
                wm_invalidate_desktop(desktop);
                wm_invalidate_surface(surface);
                focus_set = true;
                break;
            }
            node = cdlist_prev(node);
        } while (node != NULL && node != initial);
    }

    if (!focus_set && connection != NULL) {
        xcb_set_input_focus(connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_CURRENT_TIME);
        wm_invalidate_desktop(desktop);
        wm_invalidate_surface(surface);
    }
}


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

    /* ICCCM §4.1.2.4: honor 'WM_HINTS' 'initial_state' when
     * 'IconicState' */
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

        /* ICCCM §4.2.3: immediately after the frame is positioned by
         * place_apply and the windows are mapped, send a synthetic
         * 'ConfigureNotify' with screen-relative coordinates to the
         * client so it knows its true screen position from the outset.
         * Without this the only 'ConfigureNotify' the client has seen
         * was generated by 'client_sync_decoration_layout' (called
         * inside 'ci_create_decorations'), which carries frame-relative
         * coordinates (x=border, y=titlebar+border) rather than the
         * actual on-screen position, causing misaligned popups and
         * imperfect initial viewport layout. */
        if (client->frame != 0 && client_is_decorated(client)) {
            client_send_synthetic_configure_notify(wm->connection, client);
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
    desktop_td *desktop;
    surface_td *surface;

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
            s_restore_focus_after_client_loss(connection, surface,
                    desktop, client);
        }

        /* Unmap decoration windows so they do not float without
         * content.  Increment ignore_unmap for each WM-initiated unmap
         * so the resulting 'UnmapNotify' events do not re-enter this
         * handler. */
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
    surface_td *surface;
    desktop_td *desktop;

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
        s_restore_focus_after_client_loss(connection, surface,
                desktop, client);
        if (connection != NULL) {
            xcb_flush(connection);
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


/* Handle a 'MAP_NOTIFY' event */
void handler_map_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_map_notify_event_t *event)
{
    client_td *client;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in map notify handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Map notify event: window=0x%x, override_redirect=%u",
            event->window, event->override_redirect);

    /* Unmanaged override-redirect windows (tooltips, menus) are
     * intentionally ignored; only managed clients need a EWMH resync */
    if (event->override_redirect != 0) {
        return;
    }

    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client != NULL) {
        wm_request_client_redraw(client);

        /* ICCCM §4.2.3: re-send the synthetic 'ConfigureNotify' at
         * 'MapNotify' time so that some programs have the correct
         * screen-relative position in their event queue before their
         * first 'Expose'-driven draw.  The primary fix for the
         * initial-open misalignment is in 'desktop_render_clients',
         * which sends a synthetic 'ConfigureNotify' after every
         * inner-window configure to ensure the screen-relative one is
         * always the last event the client receives. */
        if (event->window == client->window &&
                client->frame != 0 &&
                client_is_decorated(client)) {
            client_send_synthetic_configure_notify(connection, client);
            xcb_flush(connection);
        }
    }
}


/* Handle a 'GRAVITY_NOTIFY' event */
void handler_gravity_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_gravity_notify_event_t *event)
{
    client_td *client;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in gravity notify handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Gravity notify event: window=0x%x, pos=%d+%d",
            event->window, event->x, event->y);

    /* The X server repositioned a frame window ('event->window') within
     * root because the screen was resized and the client's win_gravity
     * ('client->layout.gravity') placed it at a non-NW anchor.
     * Update the cached frame position and re-sync decorations. */
    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client != NULL) {
        client->layout.geometry.cur.pos.x = event->x;
        client->layout.geometry.cur.pos.y = event->y;
        client_sync_decoration_layout(client);
        wm_request_client_redraw(client);

        /* ICCCM §4.2.3: the frame moved, so the client's
         * screen-relative position changed; notify it with correct
         * coordinates */
        if (client->frame != 0 && client_is_decorated(client)) {
            client_send_synthetic_configure_notify(connection, client);
        }
    }
}


/* Handle a 'CIRCULATE_NOTIFY' event */
void handler_circulate_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_circulate_notify_event_t *event)
{
    surface_td *surface;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in circulate notify",
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Circulate notify event: window=0x%x, place=%u",
            event->window, event->place);

    /* The stacking order changed; mark the surface so 'wm_ewmh_sync'
     * updates '_NET_CLIENT_LIST_STACKING' on the next iteration */
    surface = lookup_surface_for_root(surfaces, event->event);
    wm_invalidate_surface(surface);
}


/* Handle a 'CIRCULATE_REQUEST' event (ICCCM §4.1.7) */
void handler_circulate_request(xcb_connection_t *connection,
        list_td *surfaces, xcb_circulate_request_event_t *event)
{
    client_td *client;
    xcb_window_t target;
    uint32_t stack_mode;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in circulate request" \
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Circulate request event: window=0x%x, place=%u",
            event->window, event->place);

    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    stack_mode = (event->place == XCB_PLACE_ON_TOP)
        ? (uint32_t) XCB_STACK_MODE_ABOVE
        : (uint32_t) XCB_STACK_MODE_BELOW;

    xcb_configure_window(connection, target,
            XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
    xcb_flush(connection);
    wm_request_client_redraw(client);
}
