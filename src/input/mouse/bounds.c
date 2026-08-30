/**
 * @file input/mouse/bounds.c
 *
 * @brief Shared resize-border bounds computation implementation
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* Default initial values */
#include <defs/input.h>

/* Project includes */
#include <client.h>

/* Local includes */
#include <input/mouse/bounds.h>


/**
 * @brief The adaptive grab margin for one edge's actual border
 *        width
 *
 * @param border_width That edge's actual border width, in pixels;
 *                      negative treated as 0
 *
 * @return @p border_width itself if already at least
 *         @c WM_RESIZE_GRAB_THRESHOLD, otherwise exactly
 *         @c WM_RESIZE_GRAB_THRESHOLD
 *
 * @note Complexity: @e O(1)
 */
static int32_t s_grab_margin(int32_t border_width)
{
    if (border_width < 0) {
        border_width = 0;
    }

    return (border_width < WM_RESIZE_GRAB_THRESHOLD)
        ? WM_RESIZE_GRAB_THRESHOLD
        : border_width;
}


/* Compute a client's resize border and adaptive grab margins */
im_resize_bounds_td im_bounds_resize(const client_td *client)
{
    im_resize_bounds_td bounds;
    int32_t border_left;
    int32_t border_right;
    int32_t border_top;
    int32_t border_bottom;

    bounds.left = 0;
    bounds.top = 0;
    bounds.right = 0;
    bounds.bottom = 0;
    bounds.margin_left = 0;
    bounds.margin_top = 0;
    bounds.margin_right = 0;
    bounds.margin_bottom = 0;
    bounds.has_titlebar_row = false;
    bounds.titlebar_row_top = 0;
    bounds.titlebar_row_bottom = 0;

    if (client == NULL) {
        return bounds;
    }

    bounds.left = client->layout.geometry.cur.pos.x;
    bounds.top = client->layout.geometry.cur.pos.y;
    bounds.right = bounds.left +
        (int32_t) client->layout.geometry.cur.dim.w;
    bounds.bottom = bounds.top +
        (int32_t) client->layout.geometry.cur.dim.h;

    if (client->frame != 0) {
        border_left = client->layout.frame_extents.left;
        border_right = client->layout.frame_extents.right;
        border_bottom = client->layout.frame_extents.bottom;
        border_top = client->layout.frame_extents.top -
            (int32_t) client->title_height;

        if (client->title_height > 0u) {
            bounds.has_titlebar_row = true;
            bounds.titlebar_row_top = bounds.top +
                s_grab_margin(border_top);
            bounds.titlebar_row_bottom = bounds.top +
                (int32_t) client->layout.frame_extents.top;
        }
    } else {
        int32_t theme_border = (int32_t)
            client_border_width(client, true, false);

        border_left = theme_border;
        border_right = theme_border;
        border_top = theme_border;
        border_bottom = theme_border;
    }

    bounds.margin_left = s_grab_margin(border_left);
    bounds.margin_right = s_grab_margin(border_right);
    bounds.margin_top = s_grab_margin(border_top);
    bounds.margin_bottom = s_grab_margin(border_bottom);

    return bounds;
}
