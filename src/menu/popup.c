/**
 * @file menu/popup.c
 *
 * @brief Informational client popup window implementation
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
#include <stdio.h>      /* snprintf */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <render/text.h>
#include <surface.h>

/* Default initial values */
#include <defs/wm.h>

/* Local includes */
#include <menu/draw.h>
#include <menu/popup.h>


/** XCB window of the currently visible info popup */
static xcb_window_t s_popup_window = XCB_WINDOW_NONE;

/** Modifier mask that opened the popup (locking bits stripped) */
static uint16_t s_popup_modifier = 0;

/** Keycode that opened the popup */
static xcb_keycode_t s_popup_keycode = 0;

/** Cached text lines; reused when the popup receives an expose event */
static char s_popup_lines[4][WM_INFO_POPUP_LINE_MAX_LEN];


/* Show a popup near the client window with focused-client information */
void popup_show(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, client_td *client,
        uint16_t modifier, xcb_keycode_t keycode, const config_td *cfg)
{
    const char *name;
    const char *class_name;
    const char *instance_name;
    const int16_t width  = 520;
    const int16_t height = 96;
    int16_t x;
    int16_t y;
    int32_t max_x;
    int32_t max_y;
    uint32_t mask;
    uint32_t values[3];

    if (connection == NULL || surface == NULL || desktop == NULL ||
            client == NULL || cfg == NULL ||
            surface->screen == NULL) {
        return;
    }

    name = (client->info.name != NULL) ? client->info.name : "";
    class_name = (client->info.class_name[1] != NULL)
        ? client->info.class_name[1] : "";
    instance_name = (client->info.class_name[0] != NULL)
        ? client->info.class_name[0] : "";

    popup_close(connection);

    /* Position at the client window's own coordinates, clamped to
     * screen */
    x = (int16_t) client->layout.geometry.cur.pos.x;
    y = (int16_t) client->layout.geometry.cur.pos.y;
    max_x = (int32_t) surface->properties.dim.w - (int32_t) width;
    max_y = (int32_t) surface->properties.dim.h - (int32_t) height;
    if ((int32_t) x > max_x) { x = (int16_t) max_x; }
    if ((int32_t) y > max_y) { y = (int16_t) max_y; }
    if (x < 0) { x = 0; }
    if (y < 0) { y = 0; }

    s_popup_modifier = (uint16_t) ((unsigned int) modifier &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));

    s_popup_keycode = keycode;
    s_popup_window = xcb_generate_id(connection);

    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.window.active.background_color;
    values[1] = cfg->theme.window.active.border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE    |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_KEY_PRESS;
    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_popup_window,
            surface->screen->root,
            x, y,
            (uint16_t) width, (uint16_t) height,
            1,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    snprintf(s_popup_lines[0], sizeof(s_popup_lines[0]),
            "name=%s class=%s instance=%s",
            name, class_name, instance_name);
    snprintf(s_popup_lines[1], sizeof(s_popup_lines[1]),
            "window=%#x frame=%#x desktop=%u surface=%u",
            client->window, client->frame, desktop->id, surface->id);
    snprintf(s_popup_lines[2], sizeof(s_popup_lines[2]),
            "geom=%ux%u+%d+%d",
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y);
    snprintf(s_popup_lines[3], sizeof(s_popup_lines[3]),
            "flags=%#x state=%#x",
            client->properties.flags, client->properties.state);

    xcb_map_window(connection, s_popup_window);
    xcb_flush(connection);

    LOGGER_TRACE("Popup shown for client %#x", client->id);
}


/* Destroy the currently visible info popup */
void popup_close(xcb_connection_t *connection)
{
    if (connection == NULL || s_popup_window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_destroy_window(connection, s_popup_window);
    xcb_flush(connection);
    s_popup_window = XCB_WINDOW_NONE;
    s_popup_modifier = 0;
    s_popup_keycode = 0;
}


/* Repaint the info popup from its cached text lines */
void popup_repaint(xcb_connection_t *connection,
        const config_td *cfg)
{
    if (connection == NULL || cfg == NULL ||
            s_popup_window == XCB_WINDOW_NONE) {
        return;
    }

    text_renderer_init(connection,
            cfg->theme.window.active.font);
    menu_draw_label(connection, s_popup_window, 8, 16, s_popup_lines[0]);
    menu_draw_label(connection, s_popup_window, 8, 34, s_popup_lines[1]);
    menu_draw_label(connection, s_popup_window, 8, 52, s_popup_lines[2]);
    menu_draw_label(connection, s_popup_window, 8, 70, s_popup_lines[3]);
    xcb_flush(connection);
}


/* Query whether the info popup is currently visible */
bool popup_is_open(void)
{
    return s_popup_window != XCB_WINDOW_NONE;
}


/* Return the info popup window identifier */
xcb_window_t popup_window(void)
{
    return s_popup_window;
}


/* Return the modifier mask that opened the info popup */
uint16_t popup_modifier(void)
{
    return s_popup_modifier;
}


/* Return the keycode that opened the info popup */
xcb_keycode_t popup_keycode(void)
{
    return s_popup_keycode;
}
