/**
 * @file handler.c
 *
 * @brief X event handler implementations for the window manager core
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
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Render includes */
#include <render/desktop.h>
#include <render/surface.h>
#include <render/text.h>

/* Policy includes */
#include <policy/focus.h>
#include <policy/placement.h>

/* Input includes */
#include <input/drag.h>
#include <input/keyboard.h>
#include <input/mouse.h>

/* Menu includes */
#include <menu/confirm.h>
#include <menu/cycle.h>
#include <menu/popup.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <lifecycle.h>
#include <lookup.h>

/* Local includes */
#include <handler.h>


/* Emit ICCCM synthetic ConfigureNotify for reparented clients */
static void s_handler_send_synthetic_configure_notify(
        xcb_connection_t *connection, client_td *client)
{
    xcb_configure_notify_event_t notify;
    uint16_t left;
    uint16_t top;

    if (connection == NULL || client == NULL || client->window == 0) {
        return;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    top = (uint16_t) client->layout.frame_extents.top;

    notify.response_type = XCB_CONFIGURE_NOTIFY;
    notify.pad0 = 0;
    notify.event = client->window;
    notify.window = client->window;
    notify.above_sibling = XCB_NONE;
    notify.x = (int16_t) (client->layout.geometry.cur.pos.x + left);
    notify.y = (int16_t) (client->layout.geometry.cur.pos.y + top);
    notify.width =
        (uint16_t) ((client->layout.geometry.cur.dim.w > left +
                    (uint16_t) client->layout.frame_extents.right)
                ? (client->layout.geometry.cur.dim.w - left -
                    (uint16_t) client->layout.frame_extents.right)
                : WM_MIN_WINDOW_DIMENSION);
    notify.height =
        (uint16_t) ((client->layout.geometry.cur.dim.h > top +
                    (uint16_t) client->layout.frame_extents.bottom)
                ? (client->layout.geometry.cur.dim.h - top -
                    (uint16_t) client->layout.frame_extents.bottom)
                : WM_MIN_WINDOW_DIMENSION);
    notify.border_width = 0;
    notify.override_redirect = 0;
    notify.pad1 = 0;

    xcb_send_event(connection, 0, client->window,
            XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char *) &notify);
}


/* Handle a 'CONFIGURE_REQUEST' event */
void handler_configure_request(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_configure_request_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    uint16_t mask;
    uint16_t target_mask;
    uint32_t target_values[7];
    int i;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in configure request" \
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Configure request event: window=0x%x, mask=0x%x",
            event->window, event->value_mask);

    mask = event->value_mask &
        (XCB_CONFIG_WINDOW_X            |
         XCB_CONFIG_WINDOW_Y            |
         XCB_CONFIG_WINDOW_WIDTH        |
         XCB_CONFIG_WINDOW_HEIGHT       |
         XCB_CONFIG_WINDOW_BORDER_WIDTH |
         XCB_CONFIG_WINDOW_SIBLING      |
         XCB_CONFIG_WINDOW_STACK_MODE);

    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);

    target_mask = 0;
    i = 0;
    if (client != NULL) {
        bool is_reparented = (client->frame != 0) &&
            client_is_decorated(client);
        bool on_inner = (event->window == client->window);
        bool send_synth = false;
        xcb_window_t target = event->window;
        int32_t req_x = client->layout.geometry.cur.pos.x;
        int32_t req_y = client->layout.geometry.cur.pos.y;
        uint32_t req_w = client->layout.geometry.cur.dim.w;
        uint32_t req_h = client->layout.geometry.cur.dim.h;
        uint16_t left = (uint16_t) client->layout.frame_extents.left;
        uint16_t right = (uint16_t) client->layout.frame_extents.right;
        uint16_t top = (uint16_t) client->layout.frame_extents.top;
        uint16_t bottom = (uint16_t) client->layout.frame_extents.bottom;

        if (is_reparented) {
            target = client->frame;
        }

        if (mask & XCB_CONFIG_WINDOW_X) {
            if (is_reparented && on_inner) {
                req_x = event->x - (int16_t) left;
            } else {
                req_x = event->x;
            }
            target_values[i++] = (uint32_t) req_x;
            target_mask |= XCB_CONFIG_WINDOW_X;
            client->layout.geometry.cur.pos.x = req_x;
            send_synth = is_reparented;
        }
        if (mask & XCB_CONFIG_WINDOW_Y) {
            if (is_reparented && on_inner) {
                req_y = event->y - (int16_t) top;
            } else {
                req_y = event->y;
            }
            target_values[i++] = (uint32_t) req_y;
            target_mask |= XCB_CONFIG_WINDOW_Y;
            client->layout.geometry.cur.pos.y = req_y;
            send_synth = is_reparented;
        }
        if (mask & XCB_CONFIG_WINDOW_WIDTH) {
            if (is_reparented && on_inner) {
                req_w = (uint32_t) event->width + left + right;
            } else {
                req_w = (uint32_t) event->width;
            }
            target_values[i++] = req_w;
            target_mask |= XCB_CONFIG_WINDOW_WIDTH;
            client->layout.geometry.cur.dim.w = req_w;
            send_synth = is_reparented;
        }
        if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
            if (is_reparented && on_inner) {
                req_h = (uint32_t) event->height + top + bottom;
            } else {
                req_h = (uint32_t) event->height;
            }
            target_values[i++] = req_h;
            target_mask |= XCB_CONFIG_WINDOW_HEIGHT;
            client->layout.geometry.cur.dim.h = req_h;
            send_synth = is_reparented;
        }
        if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
            target_values[i++] = (uint32_t) event->border_width;
            target_mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
        }
        if (mask & XCB_CONFIG_WINDOW_SIBLING) {
            target_values[i++] = event->sibling;
            target_mask |= XCB_CONFIG_WINDOW_SIBLING;
        }
        if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
            target_values[i++] = (uint32_t) event->stack_mode;
            target_mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        }

        if (target_mask != 0 && connection != NULL) {
            xcb_configure_window(connection, target,
                    target_mask, target_values);
            if (is_reparented) {
                client_sync_decoration_layout(client);
                if (send_synth) {
                    s_handler_send_synthetic_configure_notify(connection,
                            client);
                }
            }

            xcb_flush(connection);
        }
    } else {
        if (mask & XCB_CONFIG_WINDOW_X) {
            target_values[i++] = (uint32_t) event->x;
            target_mask |= XCB_CONFIG_WINDOW_X;
        }
        if (mask & XCB_CONFIG_WINDOW_Y) {
            target_values[i++] = (uint32_t) event->y;
            target_mask |= XCB_CONFIG_WINDOW_Y;
        }
        if (mask & XCB_CONFIG_WINDOW_WIDTH) {
            target_values[i++] = (uint32_t) event->width;
            target_mask |= XCB_CONFIG_WINDOW_WIDTH;
        }
        if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
            target_values[i++] = (uint32_t) event->height;
            target_mask |= XCB_CONFIG_WINDOW_HEIGHT;
        }
        if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
            target_values[i++] = (uint32_t) event->border_width;
            target_mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
        }
        if (mask & XCB_CONFIG_WINDOW_SIBLING) {
            target_values[i++] = event->sibling;
            target_mask |= XCB_CONFIG_WINDOW_SIBLING;
        }
        if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
            target_values[i++] = (uint32_t) event->stack_mode;
            target_mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        }
        if (target_mask != 0 && connection != NULL) {
            xcb_configure_window(connection, event->window,
                    target_mask, target_values);
            xcb_flush(connection);
        }
    }

    if (surface != NULL) { surface->is_outdated = true; }
    if (desktop != NULL) { desktop->is_outdated = true; }
}


/* Handle a 'CONFIGURE_NOTIFY' event */
void handler_configure_notify(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_configure_notify_event_t *event)
{
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    bool geom_changed;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in configure handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Configure notify event: window=0x%x, geom=%ux%u+%d+%d",
            event->window, event->width, event->height,
            event->x, event->y);

    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);
    if (client != NULL) {
        bool is_frame = (client->frame != 0)
            ? (event->window == client->frame)
            : (event->window == client->window ||
               event->window == client->id);
        bool is_inner = (event->window == client->window);        

        if (is_frame) {
            geom_changed =
                client->layout.geometry.cur.pos.x !=
                    (int32_t) event->x ||
                client->layout.geometry.cur.pos.y !=
                    (int32_t) event->y ||
                client->layout.geometry.cur.dim.w !=
                    (uint32_t) event->width ||
                client->layout.geometry.cur.dim.h !=
                    (uint32_t) event->height;

            client->layout.geometry.cur.pos.x = event->x;
            client->layout.geometry.cur.pos.y = event->y;
            client->layout.geometry.cur.dim.w = event->width;
            client->layout.geometry.cur.dim.h = event->height;

            /* Trigger a re-render only when the frame geometry actually
             * changed.  Guarding with 'geom_changed' prevents the
             * feedback loop where the render itself configures the
             * frame to the same dimensions and the resulting
             * 'ConfigureNotify' would re-mark the desktop as
             * outdated. */
            if (geom_changed) {
                if (surface != NULL) {
                    surface->is_outdated = true;
                }
                if (desktop != NULL) {
                    desktop->is_outdated = true;
                }
            } /* ! if (geom_changed) */

        } else if (is_inner &&
                client->frame != 0 &&
                client_is_decorated(client) &&
                !client_is_fullscreen(client)) {
            int32_t frame_x;
            int32_t frame_y;
            uint32_t frame_w;
            uint32_t frame_h;
            uint16_t left;
            uint16_t right;
            uint16_t top;
            uint16_t bottom;

            left = (uint16_t) client->layout.frame_extents.left;
            right = (uint16_t) client->layout.frame_extents.right;
            top = (uint16_t) client->layout.frame_extents.top;
            bottom = (uint16_t) client->layout.frame_extents.bottom;

            frame_x = client->layout.geometry.cur.pos.x +
                (int32_t) event->x - (int32_t) left;
            frame_y = client->layout.geometry.cur.pos.y +
                (int32_t) event->y - (int32_t) top;
            frame_w = (uint32_t) event->width + left + right;
            frame_h = (uint32_t) event->height + top + bottom;

            geom_changed =
                client->layout.geometry.cur.pos.x != frame_x ||
                client->layout.geometry.cur.pos.y != frame_y ||
                client->layout.geometry.cur.dim.w != frame_w ||
                client->layout.geometry.cur.dim.h != frame_h;

            if (geom_changed) {
                client->layout.geometry.cur.pos.x = frame_x;
                client->layout.geometry.cur.pos.y = frame_y;
                client->layout.geometry.cur.dim.w = frame_w;
                client->layout.geometry.cur.dim.h = frame_h;
                client_sync_decoration_layout(client);
                if (surface != NULL) {
                    surface->is_outdated = true;
                }
                if (desktop != NULL) {
                    desktop->is_outdated = true;
                }
            } /* ! if (geom_changed)  */
        } /* ! if (is_frame) */
    } /* ! if (client) */
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

    /* Refresh work area in case the new client declares struts */
    desktop_update_workarea(desktop,
            surface->properties.dim.w, surface->properties.dim.h);

    place_apply(wm, surface, client);

    if (client->titlebar != 0) {
        xcb_map_window(wm->connection, client->titlebar);
    }
    if (client->frame != 0) {
        xcb_map_window(wm->connection, client->frame);
        xcb_map_window(wm->connection, client->window);
    } else {
        xcb_map_window(wm->connection, event->window);
    }

    if (wm->config->base.windows.focus.is_new_focused) {
        focus_apply(wm->surfaces, surface, desktop, client, true,
                wm->config);
    }

    surface->is_outdated = true;
    desktop->is_outdated = true;
    xcb_flush(wm->connection);

    LOGGER_DEBUG("Mapped and adopted window %#x ('%s') on desktop %u",
            event->window, client->info.name, desktop->id);
}


/* Handle an 'UNMAP_NOTIFY' event */
void handler_unmap_notify(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_unmap_notify_event_t *event)
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
                            desktop->is_outdated = true;

                            if (surface != NULL) {
                                surface->is_outdated = true;
                            }

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
                desktop->is_outdated = true;
                if (surface != NULL) {
                    surface->is_outdated = true;
                }
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
        list_td *surfaces,
        xcb_destroy_notify_event_t *event)
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

                        desktop->is_outdated = true;
                        if (surface != NULL) {
                            surface->is_outdated = true;
                        }
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
            desktop->is_outdated = true;
            if (surface != NULL) {
                surface->is_outdated = true;
            }
        }
    }

    if (desktop != NULL) {
        desktop_action_client_rem(desktop, client);
        /* Refresh work area in case the removed client had struts */
        if (surface != NULL) {
            desktop_update_workarea(desktop,
                    surface->properties.dim.w,
                    surface->properties.dim.h);
        }
    }

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

    if (surface != NULL) { surface->is_outdated = true; }
    if (desktop != NULL) { desktop->is_outdated = true; }

    LOGGER_DEBUG("Removed destroyed window %#x", event->window);
}


/* Handle a 'PROPERTY_NOTIFY' event */
void handler_property_notify(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_property_notify_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in property handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Property notify event: window=0x%x, atom=%u",
            event->window, event->atom);

    if (event->state == XCB_PROPERTY_DELETE) {
        return;
    }

    desktop = NULL;
    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);
    if (client == NULL) {
        return;
    }

    if (event->atom == XCB_ATOM_WM_NAME ||
            (client->ewmh != NULL &&
             event->atom == client->ewmh->_NET_WM_NAME)) {
        lifecycle_refresh_name(client);

        if (surface != NULL) {
            surface->is_outdated = true;
        }

        if (desktop != NULL) {
            desktop->is_outdated = true;
        }
    }
}


/* Handle a 'FOCUS_IN' event */
void handler_focus_in(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_focus_in_event_t *event)
{
    (void) connection;
    (void) surfaces;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in focus handler", L_NARG);
        return;
    }

    if (mouse_enter_focus_is_active()) {
        mouse_enter_focus_clear();
    }
}


/* Handle a 'MAPPING_NOTIFY' event */
void handler_mapping_notify(xcb_key_symbols_t *keysyms,
        list_td *surfaces,
        xcb_mapping_notify_event_t *event,
        const config_td *cfg)
{
    xcb_connection_t *connection = NULL;

    if (keysyms == NULL || event == NULL || cfg == NULL) {
        LOGGER_ERROR("Received null pointer in mapping notify handler",
                L_NARG);
        return;
    }

    if (event->request == XCB_MAPPING_POINTER) {
        return;
    }

    LOGGER_TRACE("Mapping notify: request=%u; refreshing grabs",
            (unsigned int) event->request);

    /* Obtain the XCB connection from the first surface */
    if (surfaces != NULL) {
        list_item_td *head = list_head(surfaces);
        if (head != NULL) {
            surface_td *first = (surface_td *) list_data(head);
            if (first != NULL) {
                connection = first->connection;
            }
        }
    }

    xcb_refresh_keyboard_mapping(keysyms, event);

    if (connection != NULL && surfaces != NULL) {
        for (list_item_td *node = list_head(surfaces); node != NULL;
                node = list_next(node)) {
            surface_td *surface = (surface_td *) list_data(node);
            if (surface == NULL || surface->screen == NULL) {
                continue;
            }
            xcb_ungrab_key(connection,
                    (xcb_keycode_t) XCB_GRAB_ANY,
                    surface->screen->root,
                    (uint16_t) XCB_MOD_MASK_ANY);
        }

        xcb_flush(connection);
    }

    keyboard_load(surfaces, keysyms, cfg);

    if (event->request == XCB_MAPPING_MODIFIER &&
            connection != NULL && surfaces != NULL) {
        for (list_item_td *node = list_head(surfaces); node != NULL;
                node = list_next(node)) {
            surface_td *surface = (surface_td *) list_data(node); 
            if (surface == NULL || surface->screen == NULL) {
                continue;
            }
            xcb_ungrab_button(connection,
                    (uint8_t) XCB_BUTTON_INDEX_ANY,
                    surface->screen->root,
                    (uint16_t) XCB_MOD_MASK_ANY);
        }

        xcb_flush(connection);
        mouse_load(surfaces, cfg);
    }
}


/* Handle an 'EXPOSE' event for decoration repaints */
void handler_expose(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_expose_event_t *event,
        const config_td *cfg)
{
    client_td *client;
    desktop_td *desktop;
    bool is_focused;
    uint16_t left;
    uint16_t right;
    uint16_t title_h;
    uint16_t inner_w;

    if (event == NULL || event->count != 0) {
        return;
    }

    if (connection == NULL || cfg == NULL) {
        return;
    }

    /* Info popup repaint */
    if (popup_is_open() && event->window == popup_window()) {
        popup_repaint(connection, cfg);
        return;
    }

    /* Cycle menu repaint */
    if (cycle_is_open() && event->window == cycle_window()) {
        cycle_draw(connection, cfg);
        return;
    }

    /* Confirmation dialog repaint */
    if (confirm_is_open() && event->window == confirm_window()) {
        confirm_repaint(connection, cfg);
        return;
    }

    client = lookup_find_client(surfaces, event->window,
            NULL, &desktop);
    if (client == NULL) {
        return;
    }

    /* Icon window: repaint caption */
    if (client->icon_window == event->window) {
        if (!(client->properties.flags & CLIENT_FLAG_HIDDEN)) {
            return;
        }
        xcb_change_window_attributes(connection, client->icon_window,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                (const uint32_t[]) {
                    cfg->theme.icon.background_color,
                    cfg->theme.icon.border_color
                });
        xcb_clear_area(connection, 0, client->icon_window, 0, 0, 0, 0);
        if (cfg->theme.icon.is_captioned && client->info.name != NULL) {
            text_renderer_init(connection, cfg->theme.icon.font);
            text_renderer_set_color(
                    cfg->theme.icon.foreground_color,
                    cfg->theme.icon.background_color);
            text_draw_string(connection, client->icon_window, XCB_NONE,
                    2,
                    (int16_t) (WM_ICON_SQUARE_SIZE +
                        WM_ICON_CAPTION_HEIGHT - 2u),
                    client->info.name);
        }

        xcb_flush(connection);
        return;
    }

    is_focused = (desktop != NULL &&
                  desktop->client_active_id == client->id);

    /* Frame-only expose: repaint border and background */
    if (client->frame != 0 && client->frame == event->window) {
        xcb_change_window_attributes(connection, client->frame,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                (const uint32_t[]) {
                    (is_focused) ? cfg->theme.window.active.border_color
                                 : cfg->theme.window.inactive.border_color,
                    (is_focused) ? cfg->theme.window.active.border_color
                                 : cfg->theme.window.inactive.border_color
                });

        xcb_clear_area(connection, 0, client->frame, 0, 0, 0, 0);
        xcb_flush(connection);
        return;
    }

    if (client->titlebar != event->window || client->info.name == NULL) {
        return;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    title_h = client->title_height;
    inner_w = (client->layout.geometry.cur.dim.w > left + right)
        ? (uint16_t) (client->layout.geometry.cur.dim.w - left - right)
        : 1u;

    xcb_change_window_attributes(connection, client->frame,
            XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
            (const uint32_t[]) {
                (is_focused) ? cfg->theme.window.active.border_color
                             : cfg->theme.window.inactive.border_color,
                (is_focused) ? cfg->theme.window.active.border_color
                             : cfg->theme.window.inactive.border_color
            });
    xcb_clear_area(connection, 0, client->frame, 0, 0, 0, 0);

    xcb_change_window_attributes(connection, client->titlebar,
            XCB_CW_BACK_PIXEL,
            (const uint32_t[]) {
                (is_focused) ? cfg->theme.window.active.background_color
                             : cfg->theme.window.inactive.background_color
            });
    xcb_clear_area(connection, 0, client->titlebar, 0, 0, 0, 0);

    text_renderer_init(connection,
            (is_focused) ? cfg->theme.window.active.font
                         : cfg->theme.window.inactive.font);
    text_renderer_set_color(
            (is_focused) ? cfg->theme.window.active.foreground_color
                         : cfg->theme.window.inactive.foreground_color,
            (is_focused) ? cfg->theme.window.active.background_color
                         : cfg->theme.window.inactive.background_color);
    text_draw_string(connection, client->titlebar, XCB_NONE,
            (int16_t) (WM_DECOR_BTN_PAD + WM_DECOR_BTN_SIZE +
                WM_DECOR_BTN_PAD),
            (int16_t) ((title_h > (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD)
                    ? title_h - (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD
                    : title_h),
            client->info.name);

    desktop_draw_titlebar_buttons(connection, client->titlebar,
            inner_w, title_h,
            is_focused, (bool) client_is_sticky(client),
            !client_is_fullscreen(client),
            &cfg->theme);

    xcb_flush(connection);
}
