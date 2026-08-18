/**
 * @file input/mouse/drag/outline.c
 *
 * @brief Outline stand-in windows used by a non-solid drag
 *
 * Split out of what used to be a single, flat @c input/mouse/drag.c;
 * see @c drag/internal.h for why.
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

/* Default initial values */
#include <defs/input.h>

/* Project includes */
#include <client.h>

/* Local includes */
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/outline.h>


/**
 * @brief Create or reconfigure the 4 outline strip windows around
 *        a rectangle
 *
 * Computes each strip's own position and size (top, bottom, left,
 * right, in that fixed order) from the target rectangle, then either
 * creates and maps all 4 (@p create true) or reconfigures the
 * already-existing ones (@p create false) to match.
 *
 * @param connection XCB connection used to create or reconfigure the
 *                   strip windows
 * @param x      Left edge of the rectangle, in root coordinates
 * @param y      Top edge of the rectangle, in root coordinates
 * @param w      Width of the rectangle
 * @param h      Height of the rectangle
 * @param create @c true to create and map the 4 strip windows,
 *               @c false to reconfigure the existing ones
 *
 * @note No-op if @p connection is null
 * @note Complexity: @e O(1)
 */
static void s_drag_outline_place(xcb_connection_t *connection,
        int32_t x, int32_t y, uint32_t w, uint32_t h, bool create)
{
    uint32_t bw = (uint32_t) WM_DRAG_OUTLINE_BORDER_WIDTH;
    /* Each strip's own (x, y, w, h), in 'outline_windows''s own fixed
     * top/bottom/left/right order; width/height floored at 1, since
     * 'xcb_create_window'/'xcb_configure_window' both reject a
     * genuinely zero-sized window outright, which a resize shrinking
     * past the border's own thickness would otherwise hand them. */
    uint32_t strip_x[4];
    uint32_t strip_y[4];
    uint32_t strip_w[4];
    uint32_t strip_h[4];
    uint32_t full_w = (w > 0u) ? w : 1u;
    uint32_t full_h = (h > 0u) ? h : 1u;

    if (connection == NULL) {
        return;
    }

    strip_x[0] = (uint32_t) x; /* top */
    strip_y[0] = (uint32_t) y;
    strip_w[0] = full_w;
    strip_h[0] = bw;

    strip_x[1] = (uint32_t) x; /* bottom */
    strip_y[1] = (uint32_t) y + ((h > bw) ? h - bw : 0u);
    strip_w[1] = full_w;
    strip_h[1] = bw;

    strip_x[2] = (uint32_t) x; /* left */
    strip_y[2] = (uint32_t) y;
    strip_w[2] = bw;
    strip_h[2] = full_h;

    strip_x[3] = (uint32_t) x + ((w > bw) ? w - bw : 0u); /* right */
    strip_y[3] = (uint32_t) y;
    strip_w[3] = bw;
    strip_h[3] = full_h;

    for (int i = 0; i < 4; ++i) {
        if (create) {
            uint32_t create_mask;
            uint32_t create_values[2];

            s_drag.outline_windows[i] = xcb_generate_id(connection);
            create_mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT;
            create_values[0] = (s_drag.client != NULL &&
                    s_drag.client->theme != NULL)
                ? s_drag.client->theme->window.active.border.color
                : 0u;
            create_values[1] = 1u;

            xcb_create_window(connection,
                    XCB_COPY_FROM_PARENT,
                    s_drag.outline_windows[i],
                    s_drag.root,
                    (int16_t) strip_x[i], (int16_t) strip_y[i],
                    (uint16_t) strip_w[i], (uint16_t) strip_h[i],
                    0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    XCB_COPY_FROM_PARENT,
                    create_mask, create_values);
            xcb_map_window(connection, s_drag.outline_windows[i]);
        } else if (s_drag.outline_windows[i] != XCB_WINDOW_NONE) {
            const uint32_t vals[4] = {
                strip_x[i], strip_y[i], strip_w[i], strip_h[i]
            };

            xcb_configure_window(connection, s_drag.outline_windows[i],
                    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                    vals);
        }
    }
    xcb_flush(connection);
}


/* Begin an outline-mode drag: create and map the initial 4 strip
 * windows */
void drag_outline_start(xcb_connection_t *connection,
        int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    s_drag_outline_place(connection, x, y, w, h, true);
}


/* Move the outline stand-in's own 4 strip windows to a new
 * rectangle */
void drag_outline_move(xcb_connection_t *connection,
        int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    s_drag_outline_place(connection, x, y, w, h, false);
}


/* End an outline-mode drag: destroy the 4 strip windows */
void drag_outline_end(xcb_connection_t *connection)
{
    if (connection == NULL ||
            s_drag.outline_windows[0] == XCB_WINDOW_NONE) {
        return;
    }

    for (int i = 0; i < 4; ++i) {
        xcb_destroy_window(connection, s_drag.outline_windows[i]);
        s_drag.outline_windows[i] = XCB_WINDOW_NONE;
    }
    xcb_flush(connection);
}
