/**
 * @file render/wmicon.c
 *
 * @brief Client-supplied @c _NET_WM_ICON rendering implementation
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
#include <stdlib.h>     /* NULL, malloc, free */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/render.h>
#include <xcb/xcb_renderutil.h>

/* Local includes */
#include <render/wmicon.h>


/** Icon dimensions above this (in either axis) are rejected rather
 *  than allocated for: a well-behaved application never publishes an
 *  icon anywhere near this large, so a value past it is far more
 *  likely a corrupt or hostile property than a legitimate icon */
#define WMICON_MAX_SIDE (512u)


/**
 * @brief Premultiply one straight-alpha ARGB pixel's color channels
 *        by its own alpha
 *
 * @c _NET_WM_ICON stores straight (non-premultiplied) alpha, but the
 * X RENDER extension's @c ARGB32 format expects premultiplied alpha
 * for @c PictOpOver to blend correctly; skipping this step would
 * leave partially transparent pixels too bright wherever the source
 * alpha is below full opacity.
 *
 * @param argb One pixel, packed as @c 0xAARRGGBB
 *
 * @return The same pixel with its R, G, and B channels each scaled by
 *         its A channel
 *
 * @note Complexity: @e O(1)
 */
static uint32_t s_premultiply(uint32_t argb)
{
    uint32_t a = (argb >> 24) & 0xffu;
    uint32_t r = (argb >> 16) & 0xffu;
    uint32_t g = (argb >> 8) & 0xffu;
    uint32_t b = argb & 0xffu;

    r = (r * a) / 0xffu;
    g = (g * a) / 0xffu;
    b = (b * a) / 0xffu;

    return (a << 24) | (r << 16) | (g << 8) | b;
}


/**
 * @brief Upload one icon image to the X server and composite it,
 *        premultiplied and clipped, onto a square area of @p drawable
 *
 * @param connection XCB connection
 * @param pixels     Straight-alpha @c 0xAARRGGBB pixels, @p width
 *                   times @p height of them, row-major
 * @param width      Icon width in pixels
 * @param height     Icon height in pixels
 * @param drawable   Drawable to composite onto
 * @param area_size  Side length of the square area to center in and
 *                   clip to
 *
 * @note Assumes the X server's own image byte order matches the
 *       host's, true of virtually every system this window manager
 *       runs on; a server configured the other way around would see
 *       each pixel's bytes reversed
 * @note Complexity: @e O(p), where @e p is @p width times @p height
 */
static void s_composite_icon(xcb_connection_t *connection,
        const uint32_t *pixels, uint32_t width, uint32_t height,
        xcb_drawable_t drawable, uint16_t area_size)
{
    xcb_screen_t *screen;
    xcb_pixmap_t pixmap;
    xcb_gcontext_t gc;
    const xcb_render_query_pict_formats_reply_t *formats;
    const xcb_render_pictforminfo_t *argb_info;
    const xcb_render_pictvisual_t *visual_info;
    xcb_render_picture_t src_picture;
    xcb_render_picture_t dst_picture;
    xcb_rectangle_t clip_rect;
    uint32_t *premultiplied;
    int16_t dst_x;
    int16_t dst_y;

    if (width == 0u || height == 0u || width > WMICON_MAX_SIDE ||
            height > WMICON_MAX_SIDE) {
        return;
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    if (screen == NULL) {
        return;
    }

    formats = xcb_render_util_query_formats(connection);
    if (formats == NULL) {
        return;
    }

    argb_info = xcb_render_util_find_standard_format(formats,
            XCB_PICT_STANDARD_ARGB_32);
    visual_info = xcb_render_util_find_visual_format(formats,
            screen->root_visual);
    if (argb_info == NULL || visual_info == NULL) {
        return;
    }

    premultiplied = malloc((size_t) width * height * sizeof(uint32_t));
    if (premultiplied == NULL) {
        return;
    }
    for (uint32_t i = 0u; i < width * height; ++i) {
        premultiplied[i] = s_premultiply(pixels[i]);
    }

    pixmap = xcb_generate_id(connection);
    xcb_create_pixmap(connection, 32u, pixmap, screen->root,
            (uint16_t) width, (uint16_t) height);

    gc = xcb_generate_id(connection);
    xcb_create_gc(connection, gc, pixmap, 0u, NULL);
    xcb_put_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap, gc,
            (uint16_t) width, (uint16_t) height, 0, 0, 0u, 32u,
            width * height * (uint32_t) sizeof(uint32_t),
            (const uint8_t *) premultiplied);
    xcb_free_gc(connection, gc);
    free(premultiplied);

    src_picture = xcb_generate_id(connection);
    xcb_render_create_picture(connection, src_picture, pixmap,
            argb_info->id, 0u, NULL);
    xcb_free_pixmap(connection, pixmap);

    dst_picture = xcb_generate_id(connection);
    xcb_render_create_picture(connection, dst_picture, drawable,
            visual_info->format, 0u, NULL);

    /* Clip to the target square so an icon larger than 'area_size'
     * does not spill past it (into a caption strip below it, in the
     * icon-window case this exists for) */
    clip_rect.x = 0;
    clip_rect.y = 0;
    clip_rect.width = area_size;
    clip_rect.height = area_size;
    xcb_render_set_picture_clip_rectangles(connection, dst_picture,
            0, 0, 1u, &clip_rect);

    dst_x = (int16_t) ((area_size > width)
            ? (area_size - width) / 2u : 0u);
    dst_y = (int16_t) ((area_size > height)
            ? (area_size - height) / 2u : 0u);

    xcb_render_composite(connection, XCB_RENDER_PICT_OP_OVER,
            src_picture, XCB_NONE, dst_picture, 0, 0, 0, 0,
            dst_x, dst_y, (uint16_t) width, (uint16_t) height);

    xcb_render_free_picture(connection, src_picture);
    xcb_render_free_picture(connection, dst_picture);
}


/* Fetch and draw a client's own '_NET_WM_ICON' */
void wmicon_draw(xcb_connection_t *connection, xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, xcb_drawable_t drawable, uint16_t area_size)
{
    xcb_ewmh_get_wm_icon_reply_t reply;
    xcb_ewmh_wm_icon_iterator_t iter;
    uint32_t best_width = 0u;
    uint32_t best_height = 0u;
    const uint32_t *best_data = NULL;
    int64_t best_score = -1;

    if (connection == NULL || ewmh == NULL || window == XCB_NONE ||
            drawable == XCB_NONE || area_size == 0u) {
        return;
    }

    if (xcb_ewmh_get_wm_icon_reply(ewmh,
                xcb_ewmh_get_wm_icon(ewmh, window), &reply, NULL) == 0u) {
        return;
    }

    iter = xcb_ewmh_get_wm_icon_iterator(&reply);
    while (iter.rem > 0u) {
        if (iter.width > 0u && iter.height > 0u &&
                iter.width <= WMICON_MAX_SIDE &&
                iter.height <= WMICON_MAX_SIDE) {
            int64_t dw = (iter.width > area_size)
                ? (int64_t) (iter.width - area_size)
                : (int64_t) (area_size - iter.width);
            int64_t dh = (iter.height > area_size)
                ? (int64_t) (iter.height - area_size)
                : (int64_t) (area_size - iter.height);
            int64_t score = dw + dh;

            if (best_score < 0 || score < best_score) {
                best_score = score;
                best_width = iter.width;
                best_height = iter.height;
                best_data = iter.data;
            }
        }
        xcb_ewmh_get_wm_icon_next(&iter);
    }

    if (best_data != NULL) {
        s_composite_icon(connection, best_data, best_width, best_height,
                drawable, area_size);
    }

    xcb_ewmh_get_wm_icon_reply_wipe(&reply);
}
