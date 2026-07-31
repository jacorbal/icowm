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

/* XCB includes */
#include <xcb/xcb.h>
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
