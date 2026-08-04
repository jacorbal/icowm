/**
 * @file handler/expose.c
 *
 * @brief X EXPOSE event handler, i.e., decoration and overlay repaints
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
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>

/* Render includes */
#include <render/desktop.h>
#include <render/text.h>

/* Menu includes */
#include <menu/cycle.h>
#include <menu/dialog/quit.h>
#include <menu/notify.h>
#include <menu/popup.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <lookup.h>

/* Local includes */
#include <handler.h>


/* Handle an 'EXPOSE' event for decoration repaints */
void handler_expose(xcb_connection_t *connection,
        list_td *surfaces, xcb_expose_event_t *event,
        const config_td *cfg)
{
    client_td *client;
    desktop_td *desktop;
    bool is_focused;
    bool is_cycle_preview;
    bool use_active_style;
    client_td *cycle_client;
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

    LOGGER_TRACE("Expose event: window=0x%x, region=%ux%u+%d+%d",
            event->window, event->width, event->height,
            event->x, event->y);

    /* Info popup repaint */
    if (popup_is_open() && event->window == popup_window()) {
        popup_repaint(connection, cfg);
        return;
    }

    /* Desktop notify repaint */
    if (notify_desktop_is_open() &&
            event->window == notify_desktop_window()) {
        notify_desktop_repaint(connection, cfg);
        return;
    }

    /* Cycle menu repaint */
    if (cycle_is_open() && event->window == cycle_window()) {
        cycle_draw(connection, cfg);
        return;
    }

    /* Confirmation dialog repaint */
    if (dialog_quit_is_open() && event->window == dialog_quit_window()) {
        dialog_quit_repaint(connection, cfg);
        return;
    }

    client = lookup_find_client(surfaces, event->window,
            NULL, &desktop);
    if (client == NULL) {
        return;
    }

    /* Icon window: repaint caption */
    if (client->icon_window == event->window) {
        cycle_client = cycle_get_selected_client();
        is_cycle_preview = cycle_is_open() && cycle_client == client;

        if (!(client->properties.flags & CLIENT_FLAG_HIDDEN)) {
            return;
        }
        xcb_change_window_attributes(connection, client->icon_window,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                (const uint32_t[]) {
                    (is_cycle_preview)
                        ? cfg->theme.icon.active.background_color
                        : cfg->theme.icon.inactive.background_color,
                    (is_cycle_preview)
                        ? cfg->theme.icon.active.border_color
                        : cfg->theme.icon.inactive.border_color
                });
        xcb_clear_area(connection, 0, client->icon_window, 0, 0, 0, 0);
        if (cfg->theme.icon.general.is_captioned &&
                client->info.name != NULL) {
            const char *caption =
                (client->icon_info.visible_icon_name != NULL &&
                 client->icon_info.visible_icon_name[0] != '\0')
                    ? client->icon_info.visible_icon_name
                    : client->info.name;

            text_renderer_init(connection,
                (is_cycle_preview)
                    ? cfg->theme.icon.active.font
                    : cfg->theme.icon.inactive.font);
            text_renderer_set_color(
                    (is_cycle_preview)
                        ? cfg->theme.icon.active.foreground_color
                        : cfg->theme.icon.inactive.foreground_color,
                    (is_cycle_preview)
                        ? cfg->theme.icon.active.background_color
                        : cfg->theme.icon.inactive.background_color);
            text_draw_string(connection, client->icon_window, XCB_NONE,
                    2,
                    (int16_t) (WM_ICON_SQUARE_SIZE +
                        WM_ICON_CAPTION_HEIGHT - 2u),
                    caption);
        }

        xcb_flush(connection);
        return;
    }

    is_focused = (desktop != NULL &&
                  desktop->client_active_id == client->id);
    cycle_client = cycle_get_selected_client();
    is_cycle_preview = cycle_is_open() && cycle_client == client;
    use_active_style = is_focused || is_cycle_preview;

    /* Frame-only expose: repaint border and background */
    if (client->frame != 0 && client->frame == event->window) {
        desktop_repaint_frame_decoration(connection, client,
                use_active_style, &cfg->theme);
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

    xcb_change_window_attributes(connection, client->titlebar,
            XCB_CW_BACK_PIXEL,
            (const uint32_t[]) {
            (use_active_style)
                    ? cfg->theme.window.active.background_color
                    : cfg->theme.window.inactive.background_color
            });
    xcb_clear_area(connection, 0, client->titlebar, 0, 0, 0, 0);

    text_renderer_init(connection,
            (use_active_style)
                ? cfg->theme.window.active.font
                : cfg->theme.window.inactive.font);
    text_renderer_set_color(
            (use_active_style)
                ? cfg->theme.window.active.foreground_color
                : cfg->theme.window.inactive.foreground_color,
            (use_active_style)
                ? cfg->theme.window.active.background_color
                : cfg->theme.window.inactive.background_color);
    text_draw_string(connection, client->titlebar, XCB_NONE,
            (int16_t) (WM_DECOR_BTN_PAD +
                2u * (WM_DECOR_BTN_SIZE + WM_DECOR_BTN_GAP) +
                WM_DECOR_BTN_GAP),
            (int16_t) ((title_h > (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD)
                    ? title_h - (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD
                    : title_h),
            client->info.name);

    desktop_draw_titlebar_buttons(connection, client->titlebar,
            inner_w, title_h,
            use_active_style, (bool) client_is_sticky(client),
            (client->properties.layer != CLIENT_LAYER_NORMAL),
            (!client_is_fullscreen(client) &&
                (bool) client_is_resizable(client)),
            &cfg->theme);

    xcb_flush(connection);
}
