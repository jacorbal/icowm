/**
 * @file render/outline.c
 *
 * @brief Outline stand-in windows, shared by any caller needing to
 *        show a rectangle around a target without touching the
 *        target's geometry
 *
 * The strip-window mechanism itself, with no drag-specific state of
 * its.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Utils includes */
#include <utils/xcb/atom.h>
#include <utils/xcb/window.h>

/* Local includes */
#include <render/outline.h>


/**
 * @brief Create or reconfigure the 4 outline strip windows around
 *        a rectangle
 *
 * Computes each strip's position and size (top, bottom, left,
 * right, in that fixed order) from the target rectangle, then either
 * creates and maps all 4 (@p create true) or reconfigures the
 * already-existing ones (@p create false) to match.
 *
 * @param connection   XCB connection used to create or reconfigure
 *                     the strip windows
 * @param root         Root window the 4 strips are created as
 *                     children of; unused when @p create is @c false
 * @param geom         Rectangle, in root coordinates
 * @param border_width Thickness of each strip, in pixels
 * @param color        Fill color for all 4 strips; unused when
 *                     @p create is @c false
 * @param stack_below  A window every strip is kept stacked below,
 *                     or @c XCB_WINDOW_NONE for no such constraint
 * @param create       @c true to create and map the 4 strip windows,
 *                     @c false to reconfigure the existing ones
 * @param windows      Caller-owned 4-element array: filled in when
 *                     @p create is @c true, read when @c false
 *
 * @note No-op if @p connection is null
 * @note Complexity: @e O(1)
 */
static void s_render_outline_place(xcb_connection_t *connection,
        xcb_window_t root, struct geometry_s geom,
        uint32_t border_width, uint32_t color,
        xcb_window_t stack_below, bool create,
        xcb_window_t windows[4])
{
    uint32_t bw = border_width;
    /* Each strip's (x, y, w, h), in 'windows''s fixed
     * top/bottom/left/right order; width/height floored at 1, since
     * 'xcb_create_window'/'xcb_configure_window' both reject a
     * genuinely zero-sized window outright, which a resize shrinking
     * past the border's thickness would otherwise hand them. */
    uint32_t strip_x[4];
    uint32_t strip_y[4];
    uint32_t strip_w[4];
    uint32_t strip_h[4];
    uint32_t full_w = (geom.dim.w > 0u) ? geom.dim.w : 1u;
    uint32_t full_h = (geom.dim.h > 0u) ? geom.dim.h : 1u;

    if (connection == NULL) {
        return;
    }

    strip_x[0] = (uint32_t) geom.pos.x; /* top */
    strip_y[0] = (uint32_t) geom.pos.y;
    strip_w[0] = full_w;
    strip_h[0] = bw;

    strip_x[1] = (uint32_t) geom.pos.x; /* bottom */
    strip_y[1] = (uint32_t) geom.pos.y +
        ((geom.dim.h > bw) ? geom.dim.h - bw : 0u);
    strip_w[1] = full_w;
    strip_h[1] = bw;

    strip_x[2] = (uint32_t) geom.pos.x; /* left */
    strip_y[2] = (uint32_t) geom.pos.y;
    strip_w[2] = bw;
    strip_h[2] = full_h;

    /* right */
    strip_x[3] = (uint32_t) geom.pos.x +
        ((geom.dim.w > bw) ? geom.dim.w - bw : 0u);
    strip_y[3] = (uint32_t) geom.pos.y;
    strip_w[3] = bw;
    strip_h[3] = full_h;

    for (int i = 0; i < 4; ++i) {
        if (create) {
            uint32_t create_mask;
            uint32_t create_values[2];

            windows[i] = xcb_generate_id(connection);
            create_mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT;
            create_values[0] = color;
            create_values[1] = 1u;

            xcb_create_window(connection,
                    XCB_COPY_FROM_PARENT,
                    windows[i],
                    root,
                    (int16_t) strip_x[i], (int16_t) strip_y[i],
                    (uint16_t) strip_w[i], (uint16_t) strip_h[i],
                    0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    XCB_COPY_FROM_PARENT,
                    create_mask, create_values);
            xcb_map_window(connection, windows[i]);

            /* A strip is a solid-color rectangle repainted on every
             * geometry change of whatever it outlines, several
             * times a second during a drag; asking a compositor to
             * leave it out of its own redirected rendering keeps
             * that repainting cheap and free of the one-frame lag
             * compositing would otherwise add. */
            atom_set_window_bypass_compositor(connection, windows[i]);
        } else if (windows[i] != XCB_WINDOW_NONE) {
            xcb_window_place(windows[i], (int32_t) strip_x[i],
                    (int32_t) strip_y[i], strip_w[i], strip_h[i]);
        }

        /* Re-asserted on every create and every move, not just once
         * at creation: a strip window is otherwise free to end up
         * above whatever 'stack_below' names the moment anything
         * else on screen gets raised in between, this call's
         * only guarantee being where the strip sits relative to that
         * one window, not that it never moves again afterward. */
        if (windows[i] != XCB_WINDOW_NONE &&
                stack_below != XCB_WINDOW_NONE) {
            xcb_window_stack_below(windows[i], stack_below);
        }
    }
}


/* Create and map the initial 4 strip windows outlining a rectangle */
void render_outline_show(xcb_connection_t *connection, xcb_window_t root,
        struct geometry_s geom, uint32_t border_width, uint32_t color,
        xcb_window_t stack_below, xcb_window_t windows[4])
{
    s_render_outline_place(connection, root, geom, border_width, color,
            stack_below, true, windows);
}


/* Move the 4 strip windows to outline a new rectangle */
void render_outline_move(xcb_connection_t *connection,
        struct geometry_s geom, uint32_t border_width,
        xcb_window_t stack_below, xcb_window_t windows[4])
{
    s_render_outline_place(connection, XCB_WINDOW_NONE, geom,
            border_width, 0u, stack_below, false, windows);
}


/* Destroy the 4 strip windows and reset 'windows' to XCB_WINDOW_NONE */
void render_outline_hide(xcb_connection_t *connection,
        xcb_window_t windows[4])
{
    if (connection == NULL || windows[0] == XCB_WINDOW_NONE) {
        return;
    }

    for (int i = 0; i < 4; ++i) {
        xcb_window_destroy(windows[i]);
        windows[i] = XCB_WINDOW_NONE;
    }
}
