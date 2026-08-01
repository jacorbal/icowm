/**
 * @file handler/configure.c
 *
 * @brief X @c CONFIGURE_REQUEST and @c CONFIGURE_NOTIFY event handlers
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

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <invalidate.h>
#include <logger.h>
#include <surface.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <lookup.h>

/* Commandincludes */
#include <cmds/layer.h>

/* Local includes */
#include <handler.h>


/**
 * @brief Send a synthetic @c ConfigureNotify event to a client window
 *
 * Emits an ICCCM-compliant synthetic @c ConfigureNotify event for
 * reparented clients so they can track their geometry relative to the
 * root window.
 *
 * @param connection XCB connection handle
 * @param client     Target client containing window and geometry data
 *
 * @note No action is taken if @p connection or @p client is null or if
 *       the client window is invalid
 * @note Geometry accounts for frame extents and enforces a minimum
 *       window size (@c WM_MIN_WINDOW_DIMENSION)
 * @note Complexity: @e O(1)
 */
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
        list_td *surfaces, xcb_configure_request_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    uint16_t mask;
    uint16_t target_mask;
    uint32_t target_values[7];
    bool geom_changed;
    bool interactive_geom;
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

    geom_changed = false;
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
        uint16_t geom_mask = XCB_CONFIG_WINDOW_X |
            XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT;

        interactive_geom =
            client->properties.operation == CLIENT_OPERATION_MOVING ||
            client->properties.operation == CLIENT_OPERATION_RESIZING;
        if (interactive_geom && (mask & geom_mask)) {
            mask = (uint16_t) (mask & ~geom_mask);
            if (mask == 0) {
                if (connection != NULL && is_reparented) {
                    s_handler_send_synthetic_configure_notify(connection,
                            client);
                    xcb_flush(connection);
                }
                return;
            }
        }

        if (is_reparented) {
            target = client->frame;
        }

        if (mask & XCB_CONFIG_WINDOW_X) {
            if (is_reparented && on_inner) {
                req_x = event->x - (int16_t) left;
            } else {
                req_x = event->x;
            }

            if (req_x != client->layout.geometry.cur.pos.x) {
                geom_changed = true;
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

            if (req_y < 0) {
                req_y = 0;
            }

            if (req_y != client->layout.geometry.cur.pos.y) {
                geom_changed = true;
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

            if (req_w != client->layout.geometry.cur.dim.w) {
                geom_changed = true;
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

            if (req_h != client->layout.geometry.cur.dim.h) {
                geom_changed = true;
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

    if (geom_changed || (mask & XCB_CONFIG_WINDOW_STACK_MODE)) {
        if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
            wcmd_desktop_enforce_layers(desktop);
        }

        wm_invalidate_surface(surface);
        wm_invalidate_desktop(desktop);
    }
}


/* Handle a 'CONFIGURE_NOTIFY' event */
void handler_configure_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_configure_notify_event_t *event)
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
                wm_invalidate_surface(surface);
                wm_invalidate_desktop(desktop);
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
                wm_invalidate_surface(surface);
                wm_invalidate_desktop(desktop);
            }
        } /* ! if (is_frame) */
    } /* ! if (client) */
}
