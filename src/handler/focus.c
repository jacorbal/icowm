/**
 * @file handler/focus.c
 *
 * @brief X @c PROPERTY_NOTIFY, @c FOCUS_IN, and @c MAPPING_NOTIFY event
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
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <invalidate.h>
#include <logger.h>
#include <surface.h>

/* Input includes */
#include <input/keyboard.h>
#include <input/mouse.h>

/* Project includes */
#include <lookup.h>

/* Local includes */
#include <handler.h>


/* Handle a 'PROPERTY_NOTIFY' event */
void handler_property_notify(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_property_notify_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    xcb_ewmh_get_extents_reply_t strut;
    xcb_ewmh_wm_strut_partial_t partial;

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
        client_props_refresh_name(client);
        wm_invalidate_surface(surface);
        wm_invalidate_desktop(desktop);
                return;
    }
    if (client->ewmh != NULL &&
            (event->atom == client->ewmh->_NET_WM_STRUT_PARTIAL ||
             event->atom == client->ewmh->_NET_WM_STRUT)) {
        memset(&strut, 0, sizeof(strut));
        memset(&partial, 0, sizeof(partial));
        if (xcb_ewmh_get_wm_strut_partial_reply(client->ewmh,
                    xcb_ewmh_get_wm_strut_partial(client->ewmh,
                            client->window),
                    &partial, NULL)) {
            client->layout.strut_partial.sides.left =
                (int32_t) partial.left;
            client->layout.strut_partial.sides.right =
                (int32_t) partial.right;
            client->layout.strut_partial.sides.top =
                (int32_t) partial.top;
            client->layout.strut_partial.sides.bottom =
                (int32_t) partial.bottom;
            client->layout.strut_partial.start.left =
                (int32_t) partial.left_start_y;
            client->layout.strut_partial.start.right =
                (int32_t) partial.right_start_y;
            client->layout.strut_partial.start.top =
                (int32_t) partial.top_start_x;
            client->layout.strut_partial.start.bottom =
                (int32_t) partial.bottom_start_x;
            client->layout.strut_partial.end.left =
                (int32_t) partial.left_end_y;
            client->layout.strut_partial.end.right =
                (int32_t) partial.right_end_y;
            client->layout.strut_partial.end.top =
                (int32_t) partial.top_end_x;
            client->layout.strut_partial.end.bottom =
                (int32_t) partial.bottom_end_x;
        } else if (xcb_ewmh_get_wm_strut_reply(client->ewmh,
                    xcb_ewmh_get_wm_strut(client->ewmh, client->window),
                    &strut, NULL)) {
            client->layout.strut_partial.sides.left =
                (int32_t) strut.left;
            client->layout.strut_partial.sides.right =
                (int32_t) strut.right;
            client->layout.strut_partial.sides.top =
                (int32_t) strut.top;
            client->layout.strut_partial.sides.bottom =
                (int32_t) strut.bottom;
        }

        if (desktop != NULL && surface != NULL) {
            desktop_update_workarea(desktop,
                    surface->properties.dim.w,
                    surface->properties.dim.h);
        }

        wm_invalidate_surface(surface);
        wm_invalidate_desktop(desktop);
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
