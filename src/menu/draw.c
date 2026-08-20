/**
 * @file menu/draw.c
 *
 * @brief Shared drawing primitives for window manager menu windows
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */


/* System includes */
#include <stdint.h>
#include <stddef.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* Render includes */
#include <render/text.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Local includes */
#include <menu/draw.h>


/* Fill a horizontal row rectangle with a solid color */
void menu_draw_row_bg(xcb_connection_t *connection,
        xcb_window_t window,
        uint32_t color, int16_t row_y, uint16_t row_h, uint16_t w)
{
    xcb_gcontext_t gc;
    xcb_rectangle_t rect;
    uint32_t gc_vals[1];

    if (connection == NULL || window == XCB_WINDOW_NONE) {
        return;
    }

    gc = xcb_generate_id(connection);
    gc_vals[0] = color;
    xcb_create_gc(connection, gc, window, XCB_GC_FOREGROUND, gc_vals);

    rect.x = 0;
    rect.y = row_y;
    rect.width = w;
    rect.height = row_h;
    xcb_poly_fill_rectangle(connection, window, gc, 1, &rect);
    xcb_free_gc(connection, gc);
}


/* Draw a text label at the given position */
void menu_draw_label(xcb_connection_t *connection,
        xcb_window_t window, struct position_s pos, const char *text)
{
    if (connection == NULL || text == NULL) {
        return;
    }

    text_draw_string(connection, window, XCB_NONE, pos, text);
}


/* Measure the pixel width of a text string */
uint16_t menu_draw_measure(const char *text)
{
    if (text == NULL) {
        return 0;
    }

    return text_string_measure(text);
}


/* Truncate a buffer in place until it measures no wider than max_w */
void menu_draw_truncate(char *buf, uint16_t max_w)
{
    size_t len;

    if (buf == NULL || buf[0] == '\0' ||
            menu_draw_measure(buf) <= max_w) {
        return;
    }

    len = safe_strlen(buf);
    while (len > 0u && menu_draw_measure(buf) > max_w) {
        --len;
        buf[len] = '\0';
    }
}
