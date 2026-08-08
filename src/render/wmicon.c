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
#include <defs/icon.h>
#include <render/wmicon.h>


/** Icon dimensions above this (in either axis) are rejected rather
 *  than allocated for: a well-behaved application never publishes an
 *  icon anywhere near this large, so a value past it is far more
 *  likely a corrupt or hostile property than a legitimate icon */
#define WMICON_MAX_SIDE (512u)

/** Standard X RENDER filter name requested for scaling; smooths out
 *  both directions (a small source icon scaled up, or a large one
 *  scaled down) far better than the nearest-neighbor sampling used by
 *  default */
#define WMICON_FILTER_NAME "bilinear"


/**
 * @brief Cached picture-format query, reused across calls
 *
 * @c xcb_render_util_query_formats is a genuine round trip to the X
 * server (see @c render/glyph.c, which caches it the same way, for
 * the same reason), and the set of supported picture formats a
 * connection offers never changes for the life of that connection, so
 * querying it again on every icon rebuilt is pure waste.  Never freed:
 * held for the life of the process, the same as @c render/glyph.c's
 * own copy of this same cache.
 */
static const xcb_render_query_pict_formats_reply_t *s_formats = NULL;
static xcb_connection_t *s_formats_connection = NULL;


/**
 * @brief Convert a plain floating-point value to the 16.16 fixed-point
 *        representation the X RENDER extension's transform matrices
 *        use
 *
 * @param value Value to convert
 *
 * @return @p value in 16.16 fixed-point form
 *
 * @note Complexity: @e O(1)
 */
static xcb_render_fixed_t s_double_to_fixed(double value)
{
    return (xcb_render_fixed_t) (value * 65536.0);
}


/**
 * @brief Get the picture-format query for @p connection, resolving
 *        and caching it first if this is the first call for it
 *
 * @param connection XCB connection
 *
 * @return The cached query result, or @c NULL on failure
 *
 * @note Complexity: @e O(1) once resolved for @p connection
 */
static const xcb_render_query_pict_formats_reply_t *s_get_formats(
        xcb_connection_t *connection)
{
    if (s_formats == NULL || s_formats_connection != connection) {
        s_formats = xcb_render_util_query_formats(connection);
        s_formats_connection = connection;
    }
    return s_formats;
}


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
 * @brief Upload one icon image to the X server as a premultiplied,
 *        scaled, transform-ready Picture
 *
 * Scaled to fit within a @p draw_size by @p draw_size box, preserving
 * its own aspect ratio (so a non-square source is letterboxed rather
 * than stretched); @p draw_size smaller than the icon-graphic area it
 * will later be centered and clipped within (see @c s_composite_cache)
 * is what leaves the small margin around every icon (see
 * @c WM_ICON_PIXMAP_SCALE in defs/icon.h), and is also what makes
 * every icon the same size on screen regardless of whatever size the
 * source image happened to be.
 *
 * @param connection XCB connection
 * @param pixels     Straight-alpha @c 0xAARRGGBB pixels, @p width
 *                   times @p height of them, row-major
 * @param width      Icon width in pixels
 * @param height     Icon height in pixels
 * @param draw_size  Side length of the box the image is scaled to fit
 *                   within
 * @param out_dest_w Receives the actual scaled width, after fitting
 *                   the source's own aspect ratio within @p draw_size
 * @param out_dest_h Receives the actual scaled height; see
 *                   @p out_dest_w
 *
 * @return The built Picture, owned by the caller from this point on
 *         (see @c wmicon_invalidate to free it), or @c XCB_NONE on
 *         failure
 *
 * @note Assumes the X server's own image byte order matches the
 *       host's, true of virtually every system this window manager
 *       runs on; a server configured the other way around would see
 *       each pixel's bytes reversed
 * @note Complexity: @e O(p), where @e p is @p width times @p height
 */
static xcb_render_picture_t s_build_icon_picture(
        xcb_connection_t *connection, const uint32_t *pixels,
        uint32_t width, uint32_t height, uint16_t draw_size,
        uint16_t *out_dest_w, uint16_t *out_dest_h)
{
    xcb_screen_t *screen;
    xcb_pixmap_t pixmap;
    xcb_gcontext_t gc;
    const xcb_render_query_pict_formats_reply_t *formats;
    const xcb_render_pictforminfo_t *argb_info;
    xcb_render_picture_t src_picture;
    xcb_render_transform_t transform;
    uint32_t *premultiplied;
    double scale_w;
    double scale_h;
    double scale;
    uint16_t dest_w;
    uint16_t dest_h;

    if (width == 0u || height == 0u || width > WMICON_MAX_SIDE ||
            height > WMICON_MAX_SIDE || draw_size == 0u) {
        return XCB_NONE;
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    if (screen == NULL) {
        return XCB_NONE;
    }

    formats = s_get_formats(connection);
    if (formats == NULL) {
        return XCB_NONE;
    }

    argb_info = xcb_render_util_find_standard_format(formats,
            XCB_PICT_STANDARD_ARGB_32);
    if (argb_info == NULL) {
        return XCB_NONE;
    }

    scale_w = (double) draw_size / (double) width;
    scale_h = (double) draw_size / (double) height;
    scale = (scale_w < scale_h) ? scale_w : scale_h;
    dest_w = (uint16_t) ((double) width * scale + 0.5);
    dest_h = (uint16_t) ((double) height * scale + 0.5);
    if (dest_w == 0u) {
        dest_w = 1u;
    }
    if (dest_h == 0u) {
        dest_h = 1u;
    }

    premultiplied = malloc((size_t) width * height * sizeof(uint32_t));
    if (premultiplied == NULL) {
        return XCB_NONE;
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

    /* The transform maps each destination pixel back to the source
     * pixel it samples, so its scale factors are the inverse of
     * 'scale' above (source size divided by the drawn size, not the
     * other way around).  Set once here and never touched again: it
     * stays attached to 'src_picture' for as long as the cache keeps
     * that Picture around, so a later cache hit does not need to
     * reapply it. */
    transform.matrix11 = s_double_to_fixed((double) width / dest_w);
    transform.matrix12 = 0;
    transform.matrix13 = 0;
    transform.matrix21 = 0;
    transform.matrix22 = s_double_to_fixed((double) height / dest_h);
    transform.matrix23 = 0;
    transform.matrix31 = 0;
    transform.matrix32 = 0;
    transform.matrix33 = s_double_to_fixed(1.0);
    xcb_render_set_picture_transform(connection, src_picture, transform);
    xcb_render_set_picture_filter(connection, src_picture,
            (uint16_t) (sizeof(WMICON_FILTER_NAME) - 1u),
            WMICON_FILTER_NAME, 0u, NULL);

    *out_dest_w = dest_w;
    *out_dest_h = dest_h;
    return src_picture;
}


/**
 * @brief Composite an already built icon Picture onto a square area
 *        of @p drawable, centered and clipped
 *
 * @param connection XCB connection
 * @param src_picture Already built Picture (see @c s_build_icon_picture
 *                   or a cache hit); left untouched, still owned by
 *                   whichever cache slot it came from
 * @param dest_w     Width @p src_picture was built to draw at
 * @param dest_h     Height @p src_picture was built to draw at
 * @param drawable   Drawable to composite onto
 * @param area_size  Side length of the square area to center in and
 *                   clip to
 *
 * @note Complexity: @e O(1); the expensive per-pixel work already
 *       happened whenever @p src_picture was originally built
 */
static void s_composite_cached(xcb_connection_t *connection,
        xcb_render_picture_t src_picture, uint16_t dest_w,
        uint16_t dest_h, xcb_drawable_t drawable, uint16_t area_size)
{
    xcb_screen_t *screen;
    const xcb_render_query_pict_formats_reply_t *formats;
    const xcb_render_pictvisual_t *visual_info;
    xcb_render_picture_t dst_picture;
    xcb_rectangle_t clip_rect;
    int16_t dst_x;
    int16_t dst_y;

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    if (screen == NULL) {
        return;
    }

    formats = s_get_formats(connection);
    if (formats == NULL) {
        return;
    }

    visual_info = xcb_render_util_find_visual_format(formats,
            screen->root_visual);
    if (visual_info == NULL) {
        return;
    }

    dst_picture = xcb_generate_id(connection);
    xcb_render_create_picture(connection, dst_picture, drawable,
            visual_info->format, 0u, NULL);

    /* Clip to the target square; with 'dest_w'/'dest_h' already at
     * most 'area_size' (see 's_build_icon_picture'), this should
     * rarely trim anything in practice, but stays as a defensive
     * backstop against a scale computation bug rather than letting
     * one spill the icon into, for example, a caption strip below the
     * square in the icon-window case */
    clip_rect.x = 0;
    clip_rect.y = 0;
    clip_rect.width = area_size;
    clip_rect.height = area_size;
    xcb_render_set_picture_clip_rectangles(connection, dst_picture,
            0, 0, 1u, &clip_rect);

    dst_x = (int16_t) ((area_size > dest_w)
            ? (area_size - dest_w) / 2u : 0u);
    dst_y = (int16_t) ((area_size > dest_h)
            ? (area_size - dest_h) / 2u : 0u);

    xcb_render_composite(connection, XCB_RENDER_PICT_OP_OVER,
            src_picture, XCB_NONE, dst_picture, 0, 0, 0, 0,
            dst_x, dst_y, dest_w, dest_h);

    xcb_render_free_picture(connection, dst_picture);
}


/* Fetch and draw a client's own '_NET_WM_ICON' */
void wmicon_draw(xcb_connection_t *connection, xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, xcb_drawable_t drawable, uint16_t area_size,
        wmicon_cache_td *cache)
{
    xcb_ewmh_get_wm_icon_reply_t reply;
    xcb_ewmh_wm_icon_iterator_t iter;
    uint16_t draw_size;
    uint32_t best_width = 0u;
    uint32_t best_height = 0u;
    const uint32_t *best_data = NULL;
    int64_t best_score = -1;
    xcb_render_picture_t built;
    uint16_t dest_w = 0u;
    uint16_t dest_h = 0u;

    if (connection == NULL || ewmh == NULL || window == XCB_NONE ||
            drawable == XCB_NONE || area_size == 0u || cache == NULL) {
        return;
    }

    draw_size = (uint16_t) ((double) area_size * WM_ICON_PIXMAP_SCALE);
    if (draw_size == 0u) {
        draw_size = 1u;
    }

    /* Cache hit: the same Picture already built for this exact
     * 'draw_size' is reused as-is, skipping the property fetch,
     * per-pixel premultiply, and pixmap upload entirely. */
    if (cache->picture != XCB_NONE && cache->draw_size == draw_size) {
        s_composite_cached(connection, cache->picture, cache->dest_w,
                cache->dest_h, drawable, area_size);
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
            int64_t dw = (iter.width > draw_size)
                ? (int64_t) (iter.width - draw_size)
                : (int64_t) (draw_size - iter.width);
            int64_t dh = (iter.height > draw_size)
                ? (int64_t) (iter.height - draw_size)
                : (int64_t) (draw_size - iter.height);
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

    built = (best_data != NULL)
        ? s_build_icon_picture(connection, best_data, best_width,
                best_height, draw_size, &dest_w, &dest_h)
        : XCB_NONE;

    xcb_ewmh_get_wm_icon_reply_wipe(&reply);

    if (built == XCB_NONE) {
        return;
    }

    wmicon_invalidate(connection, cache);
    cache->picture = built;
    cache->draw_size = draw_size;
    cache->dest_w = dest_w;
    cache->dest_h = dest_h;

    s_composite_cached(connection, cache->picture, cache->dest_w,
            cache->dest_h, drawable, area_size);
}


/* Invalidate a cache slot, freeing its cached Picture's X server
 * resource */
void wmicon_invalidate(xcb_connection_t *connection, wmicon_cache_td *cache)
{
    if (cache == NULL || cache->picture == XCB_NONE) {
        return;
    }

    if (connection != NULL) {
        xcb_render_free_picture(connection, cache->picture);
    }
    cache->picture = XCB_NONE;
    cache->draw_size = 0u;
    cache->dest_w = 0u;
    cache->dest_h = 0u;
}
