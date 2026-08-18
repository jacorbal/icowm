/**
 * @file menu/dialog.c
 *
 * @brief Layout constants and drawing helpers shared by every modal
 *        dialog type
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <surface.h>

/* Local includes */
#include <menu/dialog.h>


/* Return the greater of two 'uint16_t' values */
uint16_t dlgutil_u16max(uint16_t a, uint16_t b)
{
    return (a > b) ? a : b;
}


/* Draw a solid border outline around a button */
void dlgutil_button_border_draw(xcb_connection_t *connection,
        xcb_window_t window, uint32_t color, uint32_t width,
        int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    xcb_gcontext_t gc;
    xcb_rectangle_t rect;

    if (connection == NULL || window == XCB_WINDOW_NONE ||
            w == 0u || h == 0u || width == 0u) {
        return;
    }

    gc = xcb_generate_id(connection);
    xcb_create_gc(connection, gc, window,
            XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH,
            (const uint32_t[]) { color, width });

    rect.x = (int16_t) (x + (int16_t) (width / 2u));
    rect.y = (int16_t) (y + (int16_t) (width / 2u));
    rect.width = (uint16_t) (w - width);
    rect.height = (uint16_t) (h - width);
    xcb_poly_rectangle(connection, window, gc, 1, &rect);
    xcb_free_gc(connection, gc);
}


/* Resolve the monitor a dialog should size and center itself against */
monitor_td dlgutil_resolve_monitor(xcb_connection_t *connection,
        const surface_td *surface)
{
    xcb_query_pointer_cookie_t cookie;
    monitor_td monitor = {.x = 0, .y = 0, .w = 0u, .h = 0u};

    if (surface == NULL) {
        return monitor;
    }

    if (connection != NULL && surface->screen != NULL) {
        xcb_query_pointer_reply_t *reply;

        cookie = xcb_query_pointer(connection, surface->screen->root);
        reply = xcb_query_pointer_reply(connection, cookie, NULL);
        if (reply != NULL) {
            monitor = surface_monitor_for_point(surface,
                    reply->root_x, reply->root_y);
            free(reply);
        }
    }

    if (monitor.w == 0u || monitor.h == 0u) {
        monitor.x = 0;
        monitor.y = 0;
        monitor.w = surface->properties.dim.w;
        monitor.h = surface->properties.dim.h;
    }

    return monitor;
}


/* Center a dialog of the given size on its target monitor */
void menu_dialog_center(xcb_connection_t *connection,
        const surface_td *surface,
        uint16_t width, uint16_t height,
        int16_t *restrict out_x, int16_t *restrict out_y)
{
    monitor_td monitor;

    if (out_x == NULL || out_y == NULL) {
        return;
    }

    if (surface == NULL) {
        *out_x = 0;
        *out_y = 0;
        return;
    }

    monitor = dlgutil_resolve_monitor(connection, surface);

    *out_x = (int16_t) (monitor.x + (int32_t) ((monitor.w > width)
            ? (monitor.w - width) / 2u : 0u));
    *out_y = (int16_t) (monitor.y + (int32_t) ((monitor.h > height)
            ? (monitor.h - height) / 2u : 0u));
}
