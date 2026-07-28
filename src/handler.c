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
#include <adt/list.h>

/* Defs includes */
#include <defs/wm.h>

/* Render includes */
#include <render/desktop.h>
#include <render/surface.h>
#include <render/text.h>

/* Windows & icons policy includes */
#include <policy/focus.h>
#include <policy/placement.h>

/* Input includes */
#include <input/drag.h>
#include <input/keyboard.h>
#include <input/mouse.h>

/* Menu includes */
#include <menu/cycle.h>
#include <menu/popup.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <lifecycle.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <handler.h>


/* Handle a 'CONFIGURE_REQUEST' event */
void handler_configure_request(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_configure_request_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    uint16_t mask;
    uint32_t values[7];
    int i = 0;

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

    if (mask & XCB_CONFIG_WINDOW_X) {
        values[i++] = (uint32_t) event->x;
        if (client != NULL) {
            client->layout.geometry.cur.pos.x = event->x;
        }
    }
    if (mask & XCB_CONFIG_WINDOW_Y) {
        values[i++] = (uint32_t) event->y;
        if (client != NULL) {
            client->layout.geometry.cur.pos.y = event->y;
        }
    }
    if (mask & XCB_CONFIG_WINDOW_WIDTH) {
        values[i++] = (uint32_t) event->width;
        if (client != NULL) {
            client->layout.geometry.cur.dim.w = event->width;
        }
    }
    if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
        values[i++] = (uint32_t) event->height;
        if (client != NULL) {
            client->layout.geometry.cur.dim.h = event->height;
        }
    }
    if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        values[i++] = (uint32_t) event->border_width;
    }
    if (mask & XCB_CONFIG_WINDOW_SIBLING) {
        values[i++] = event->sibling;
    }
    if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        values[i++] = (uint32_t) event->stack_mode;
    }

    if (mask != 0 && connection != NULL) {
        xcb_configure_window(connection, event->window, mask, values);
        xcb_flush(connection);
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

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in configure handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Configure notify event: window=0x%x, geom=%ux%u+%d+%d",
            event->window, event->width, event->height,
            event->x, event->y);

    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client != NULL) {
        bool is_frame = (client->frame != 0)
            ? (event->window == client->frame)
            : (event->window == client->window ||
               event->window == client->id);
        if (is_frame) {
            client->layout.geometry.cur.pos.x = event->x;
            client->layout.geometry.cur.pos.y = event->y;
            client->layout.geometry.cur.dim.w = event->width;
            client->layout.geometry.cur.dim.h = event->height;
        }
    }
}


/* Handle a 'MAP_REQUEST' event */
void handler_map_request(wm_td *wm, xcb_map_request_event_t *event)
{
    client_td *client;
    desktop_td *desktop;
    surface_td *surface;

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
    desktop_td *desktop;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in unmap handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Unmap notify event: window=0x%x", event->window);

    client = lookup_find_client(surfaces, event->window, NULL, &desktop);
    if (client != NULL) {
        if (event->window != client->window &&
                event->window != client->frame) {
            return;
        }
        if (desktop != NULL &&
                desktop->client_active_id == client->id) {
            desktop->client_active_id = 0;
        }
    }
}


/* Handle a 'DESTROY_NOTIFY' event */
void handler_destroy_notify(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_destroy_notify_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in destroy handler", L_NARG);
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
    }

    if (desktop != NULL) {
        desktop_action_client_rem(desktop, client);
    }

    client->window = 0;
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

    if (event->atom == XCB_ATOM_WM_NAME) {
        client = lookup_find_client(surfaces, event->window,
                &surface, NULL);
        if (client != NULL) {
            lifecycle_refresh_name(client);
            if (surface != NULL) {
                surface->is_outdated = true;
            }
        }
    }
}


/* Handle a 'FOCUS_IN' event */
void handler_focus_in(xcb_connection_t *connection,
        list_td *surfaces,
        xcb_focus_in_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in focus handler", L_NARG);
        return;
    }

    client = lookup_find_client(surfaces, event->event,
            &surface, &desktop);
    if (client == NULL || desktop == NULL) {
        return;
    }

    if (desktop->client_active_id != client->id) {
        desktop->client_active_id = client->id;
        desktop->is_outdated = true;
        if (surface != NULL) {
            surface->is_outdated = true;
        }
    }
}


/* Handle a 'MAPPING_NOTIFY' event */
void handler_mapping_notify(xcb_key_symbols_t *keysyms,
        list_td *surfaces,
        xcb_mapping_notify_event_t *event,
        const config_td *cfg)
{
    list_item_td *node;
    surface_td   *surface;
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
        for (node = list_head(surfaces); node != NULL;
                node = list_next(node)) {
            surface = (surface_td *) list_data(node);
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
        for (node = list_head(surfaces); node != NULL;
                node = list_next(node)) {
            surface = (surface_td *) list_data(node);
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
    client_td  *client;
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

    client = lookup_find_client(surfaces, event->window,
            NULL, &desktop);
    if (client == NULL) {
        return;
    }

    /* Icon window: repaint caption */
    if (client->icon_window == event->window) {
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
            text_draw_string(connection, client->icon_window,
                    XCB_NONE,
                    2,
                    (int16_t) (WM_ICON_SQUARE_SIZE +
                        WM_ICON_CAPTION_HEIGHT - 2u),
                    client->info.name);
        }
        xcb_flush(connection);
        return;
    }

    if (client->titlebar != event->window ||
            client->info.name == NULL) {
        return;
    }

    is_focused = (desktop != NULL &&
                  desktop->client_active_id == client->id);

    if (client->frame == event->window) {
        xcb_change_window_attributes(connection, client->frame,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                (const uint32_t[]) {
                    is_focused ? cfg->theme.window.active.border_color
                               : cfg->theme.window.inactive.border_color,
                    is_focused ? cfg->theme.window.active.border_color
                               : cfg->theme.window.inactive.border_color
                });
        xcb_clear_area(connection, 0, client->frame, 0, 0, 0, 0);
        xcb_flush(connection);
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
                is_focused ? cfg->theme.window.active.border_color
                           : cfg->theme.window.inactive.border_color,
                is_focused ? cfg->theme.window.active.border_color
                           : cfg->theme.window.inactive.border_color
            });
    xcb_clear_area(connection, 0, client->frame, 0, 0, 0, 0);

    xcb_change_window_attributes(connection, client->titlebar,
            XCB_CW_BACK_PIXEL,
            (const uint32_t[]) {
                is_focused ? cfg->theme.window.active.background_color
                           : cfg->theme.window.inactive.background_color
            });
    xcb_clear_area(connection, 0, client->titlebar, 0, 0, 0, 0);

    text_renderer_init(connection,
            is_focused ? cfg->theme.window.active.font
                       : cfg->theme.window.inactive.font);
    text_renderer_set_color(
            is_focused ? cfg->theme.window.active.foreground_color
                       : cfg->theme.window.inactive.foreground_color,
            is_focused ? cfg->theme.window.active.background_color
                       : cfg->theme.window.inactive.background_color);
    text_draw_string(connection, client->titlebar, XCB_NONE,
            (int16_t) (WM_DECOR_BTN_PAD +
                WM_DECOR_BTN_SIZE +
                WM_DECOR_BTN_PAD),
            (int16_t) ((title_h > (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD)
                    ? title_h - (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD
                    : title_h),
            client->info.name);

    desktop_draw_titlebar_buttons(connection, client->titlebar,
            inner_w, title_h,
            is_focused, (bool) client_is_sticky(client),
            &cfg->theme);

    xcb_flush(connection);
}
