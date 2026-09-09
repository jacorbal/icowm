/**
 * @file render/wmicon.c
 *
 * @brief Client-supplied icon rendering: @c _NET_WM_ICON (EWMH), with
 *        an ICCCM @c WM_HINTS fallback
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
#include <stdlib.h>     /* NULL, malloc, free */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/render.h>
#include <xcb/xcb_renderutil.h>
#include <defs/icon.h>

/* Default initial values */
#include <defs/icon.h>

/* Local includes */
#include <render/wmicon.h>


/**
 * @brief Icon dimensions above this (in either axis) are rejected
 *        rather than allocated for
 *
 * A well-behaved application never publishes an icon anywhere near this
 * large, so a value past it is far more likely a corrupt or hostile
 * property than a legitimate icon.
 */
#define WMICON_MAX_SIDE (512u)

/**
 * @brief Standard X RENDER filter name requested for scaling
 *
 * Smooths out both directions (a small source icon scaled up, or
 * a large one scaled down) far better than the nearest-neighbor
 * sampling used by default.
 */
#define WMICON_FILTER_NAME "bilinear"


/**
 * @brief Cached picture-format query, reused across calls
 *
 * @a xcb_render_util_query_formats is a genuine round trip to the
 * X server, and the set of supported picture formats a connection
 * offers never changes for the life of that connection, so querying it
 * again on every icon rebuilt is pure waste.
 *
 * @note Held for the life of the current connection, the same as
 *       @c render/glyph.c's copy of this same cache; freed and
 *       re-queried only if @c s_get_formats is ever called again with
 *       a different connection, which does not currently happen in
 *       practice
 *
 * @see @c render/glyph.c, which caches it the same way, for the same
 *      reason
 */
static const xcb_render_query_pict_formats_reply_t *s_formats = NULL;
static xcb_connection_t *s_formats_connection = NULL;

static xcb_gcontext_t s_default_icon_gc = XCB_NONE;
static xcb_connection_t *s_default_icon_gc_connection = NULL;



/**
 * @brief Convert a plain floating-point value to the 16.16 fixed-point
 *        representation the X RENDER extension's transform matrices use
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
 * @brief Get the picture-format query for @p connection, resolving and
 *        caching it first if this is the first call for it
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
        if (s_formats != NULL) {
            free((void *) s_formats);
        }
        s_formats = xcb_render_util_query_formats(connection);
        s_formats_connection = connection;
    }
    return s_formats;
}


/**
 * @brief Premultiply one straight-alpha ARGB pixel's color channels by
 *        its alpha
 *
 * @c _NET_WM_ICON stores straight (non-premultiplied) alpha, but the
 * X RENDER extension's @c ARGB32 format expects premultiplied alpha for
 * @c PictOpOver to blend correctly; skipping this step would leave
 * partially transparent pixels too bright wherever the source alpha is
 * below full opacity.
 *
 * @param argb One pixel, packed as @c 0xAARRGGBB
 *
 * @return The same pixel with its @c R, @c G, and @c B channels each
 *         scaled by its @c A channel
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
 * @brief Set the aspect-preserving scale transform and bilinear filter
 *        on an already-built source Picture
 *
 * Shared by @a s_build_icon_picture (the @c _NET_WM_ICON path, source
 * pixels just uploaded into a freshly created Picture) and
 * @a s_build_icccm_icon_picture (the @c WM_HINTS fallback path, source
 * Picture wrapping a pixmap the client itself already owns).  Both need
 * the exact same "fit within @p draw_size, preserving aspect ratio"
 * transform applied afterward, so a non-square source is letterboxed
 * rather than stretched, and every icon ends up the same size on screen
 * regardless of whatever size the source image happened to be.
 *
 * @param connection   XCB connection
 * @param picture      Already created Picture to set the transform and
 *                     filter on
 * @param src_dim      Source width/height, in pixels, @p picture was
 *                     built from
 * @param draw_size    Side length of the box the image is scaled to fit
 *                     within
 * @param out_dest_dim Receives the actual scaled width/height, after
 *                     fitting @p src_dim's aspect ratio within
 *                     @p draw_size
 *
 * @note Complexity: @e O(1)
 */
static void s_icon_scale_apply(xcb_connection_t *connection,
        xcb_render_picture_t picture, struct dimensions_s src_dim,
        uint16_t draw_size, struct dimensions_s *restrict out_dest_dim)
{
    xcb_render_transform_t transform;
    double scale_w;
    double scale_h;
    double scale;

    scale_w = (double) draw_size / (double) src_dim.w;
    scale_h = (double) draw_size / (double) src_dim.h;
    scale = (scale_w < scale_h) ? scale_w : scale_h;
    out_dest_dim->w = (uint16_t) ((double) src_dim.w * scale + 0.5);
    out_dest_dim->h = (uint16_t) ((double) src_dim.h * scale + 0.5);
    if (out_dest_dim->w == 0u) {
        out_dest_dim->w = 1u;
    }
    if (out_dest_dim->h == 0u) {
        out_dest_dim->h = 1u;
    }

    /* The transform maps each destination pixel back to the source
     * pixel it samples, so its scale factors are the inverse of 'scale'
     * above (source size divided by the drawn size, not the other way
     * around).  Set once here and never touched again: it stays
     * attached to 'picture' for as long as the cache keeps that Picture
     * around, so a later cache hit does not need to reapply it. */
    transform.matrix11 = s_double_to_fixed(
            (double) src_dim.w / out_dest_dim->w);
    transform.matrix12 = 0;
    transform.matrix13 = 0;
    transform.matrix21 = 0;
    transform.matrix22 = s_double_to_fixed(
            (double) src_dim.h / out_dest_dim->h);
    transform.matrix23 = 0;
    transform.matrix31 = 0;
    transform.matrix32 = 0;
    transform.matrix33 = s_double_to_fixed(1.0);
    xcb_render_set_picture_transform(connection, picture, transform);
    xcb_render_set_picture_filter(connection, picture,
            (uint16_t) (sizeof(WMICON_FILTER_NAME) - 1u),
            WMICON_FILTER_NAME, 0u, NULL);
}


/**
 * @brief Upload one icon image to the X server as a premultiplied,
 *        scaled, transform-ready Picture
 *
 * Scaled to fit within a @p draw_size by @p draw_size box, preserving
 * its aspect ratio (so a non-square source is letterboxed rather than
 * stretched).  @p draw_size smaller than the icon-graphic area it will
 * later be centered and clipped within is what leaves the small margin
 * around every icon (@c WM_ICON_PIXMAP_SCALE_PERCENT in
 * @c defs/icon.h), and is also what makes every icon the same size on
 * screen regardless of whatever size the source image happened to be.
 *
 * @param connection XCB connection
 * @param pixels     Straight-alpha @c 0xAARRGGBB pixels, @p width times
 *                   @p height of them, row-major
 * @param width      Icon width in pixels
 * @param height     Icon height in pixels
 * @param draw_size  Side length of the box the image is scaled to fit
 *                   within
 * @param out_dest_w Receives the actual scaled width, after fitting
 *                   the source's aspect ratio within @p draw_size
 * @param out_dest_h Receives the actual scaled height; see
 *                   @p out_dest_w
 *
 * @return The built Picture, owned by the caller from this point on, or
 *         @c XCB_NONE on failure
 *
 * @see @a wmicon_invalidate, to free the returned Picture
 * @see @a s_composite_cached, which this Picture is later composited
 *      through
 *
 * @note Assumes the X server's image byte order matches the host's,
 *       true of virtually every system this window manager runs on;
 *       a server configured the other way around would see each pixel's
 *       bytes reversed
 * @note Complexity: @e O(p), where @e p is @p width times @p height
 */
static xcb_render_picture_t s_build_icon_picture(
        xcb_connection_t *connection, const uint32_t *pixels,
        uint32_t width, uint32_t height, uint16_t draw_size,
        uint16_t *restrict out_dest_w, uint16_t *restrict out_dest_h)
{
    xcb_screen_t *screen;
    xcb_pixmap_t pixmap;
    xcb_gcontext_t gc;
    const xcb_render_query_pict_formats_reply_t *formats;
    const xcb_render_pictforminfo_t *argb_info;
    xcb_render_picture_t src_picture;
    uint32_t *premultiplied;
    struct dimensions_s dest_dim;

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

    s_icon_scale_apply(connection, src_picture,
            (struct dimensions_s) { width, height }, draw_size,
            &dest_dim);
    *out_dest_w = (uint16_t) dest_dim.w;
    *out_dest_h = (uint16_t) dest_dim.h;
    return src_picture;
}


/**
 * @brief Result of a successful @a s_build_icccm_icon_picture call
 */
typedef struct {
    xcb_render_picture_t src;   /**< Color source: either the client's
                                     own @p icon_pixmap (depth greater
                                     than 1), or a solid-black fill
                                     standing in for one (depth 1,
                                     where @p icon_pixmap is a stencil
                                     rather than a color image) */
    xcb_render_picture_t mask;  /**< RENDER mask to composite @p src
                                     through, or @c XCB_NONE for an
                                     opaque rectangle: the depth-1
                                     @p icon_pixmap itself, reused
                                     directly as its own mask, when
                                     @p icon_pixmap is depth 1;
                                     otherwise @p icon_mask, if the
                                     client set one, else @c XCB_NONE */
} s_icccm_icon_td;


/**
 * @brief Build a RENDER Picture (and optional mask) from a client's
 *        ICCCM @c WM_HINTS icon hint
 *
 * Used only as a fallback when the client has not published
 * @c _NET_WM_ICON at all.  Anumber of still-common applications never
 * adopted that EWMH property and only ever set this older ICCCM one
 * instead (@c xterm, via its @c iconHint resource, is the canonical
 * example).  ICCCM formally specifies @p icon_pixmap as depth 1 (a
 * stencil, painted here in solid black through @p icon_pixmap itself
 * used as the RENDER mask), but XTerm's @c icon_pixmap is depth
 * 24 in practice, so both forms are handled.  A depth-1 @p icon_pixmap
 * is treated as ICCCM specifies, anything deeper is treated as a real
 * color image using the screen's root visual format (the depth
 * every application creating an unadorned pixmap like this virtually
 * always uses), clipped to @p icon_mask's shape when the client also
 * set one.
 *
 * Unlike @a s_build_icon_picture, @p icon_pixmap (and @p icon_mask) are
 * pixmaps the client itself owns, not ones this function allocates:
 * they are wrapped in a Picture directly, with no upload of their own,
 * and never freed here.
 *
 * @param connection XCB connection
 * @param window     Client window whose @c WM_HINTS is read
 * @param draw_size  Side length of the box the image is scaled to fit
 *                   within
 * @param out_dest_w Receives the actual scaled width
 * @param out_dest_h Receives the actual scaled height
 *
 * @return The built source and (optional) mask Pictures, both owned by
 *         the caller from this point on; @p result.src is @c XCB_NONE
 *         if the client set no usable @p icon_pixmap at all
 *
 * @see @a wmicon_invalidate, to free the returned Pictures
 * @see @a s_icon_scale_apply
 *
 * @note Complexity: @e O(1); no per-pixel work happens here, unlike
 *       @a s_build_icon_picture, since the source pixmap already
 *       lives server-side
 */
static s_icccm_icon_td s_build_icccm_icon_picture(
        xcb_connection_t *connection, xcb_window_t window,
        uint16_t draw_size, uint16_t *restrict out_dest_w,
        uint16_t *restrict out_dest_h)
{
    s_icccm_icon_td result = { XCB_NONE, XCB_NONE };
    xcb_icccm_wm_hints_t hints;
    xcb_get_geometry_reply_t *pixmap_geom;
    xcb_screen_t *screen;
    const xcb_render_query_pict_formats_reply_t *formats;
    xcb_render_color_t black = { 0u, 0u, 0u, 0xffffu };

    if (!xcb_icccm_get_wm_hints_reply(connection,
                xcb_icccm_get_wm_hints(connection, window),
                &hints, NULL) ||
            !(hints.flags & XCB_ICCCM_WM_HINT_ICON_PIXMAP) ||
            hints.icon_pixmap == XCB_NONE) {
        return result;
    }

    pixmap_geom = xcb_get_geometry_reply(connection,
            xcb_get_geometry(connection, hints.icon_pixmap), NULL);
    if (pixmap_geom == NULL || pixmap_geom->width == 0u ||
            pixmap_geom->height == 0u ||
            pixmap_geom->width > WMICON_MAX_SIDE ||
            pixmap_geom->height > WMICON_MAX_SIDE) {
        free(pixmap_geom);
        return result;
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    formats = s_get_formats(connection);
    if (screen == NULL || formats == NULL) {
        free(pixmap_geom);
        return result;
    }

    if (pixmap_geom->depth == 1u) {
        /* ICCCM's literal spec: a 1-bit stencil, painted in a fixed
         * solid color rather than the icon's (nonexistent) colors.
         * 'icon_pixmap' becomes its mask; any separately set
         * 'icon_mask' is not consulted in this case, since a 1-bit
         * source is already its shape.
         *
         * The solid fill is an infinitely repeating single pixel with
         * no size or aspect ratio of its own, so it never needs a scale
         * transform, only 'result.mask' (the actual bitmap) does. */
        const xcb_render_pictforminfo_t *mask_info =
            xcb_render_util_find_standard_format(formats,
                    XCB_PICT_STANDARD_A_1);
        struct dimensions_s dest_dim;

        if (mask_info == NULL) {
            free(pixmap_geom);
            return result;
        }

        result.mask = xcb_generate_id(connection);
        xcb_render_create_picture(connection, result.mask,
                hints.icon_pixmap, mask_info->id, 0u, NULL);
        s_icon_scale_apply(connection, result.mask,
                (struct dimensions_s) { pixmap_geom->width,
                    pixmap_geom->height },
                draw_size, &dest_dim);
        *out_dest_w = (uint16_t) dest_dim.w;
        *out_dest_h = (uint16_t) dest_dim.h;

        result.src = xcb_generate_id(connection);
        xcb_render_create_solid_fill(connection, result.src, black);
    } else {
        /* What XTerm itself actually publishes despite ICCCM
         * specifying depth 1: a real color pixmap, assumed to use the
         * screen's root visual, since an application creating
         * a plain pixmap like this (not tied to any particular window)
         * has no other depth/visual to reasonably pick. */
        const xcb_render_pictvisual_t *visual_info =
            xcb_render_util_find_visual_format(formats,
                    screen->root_visual);
        struct dimensions_s dest_dim;

        if (visual_info == NULL) {
            free(pixmap_geom);
            return result;
        }

        result.src = xcb_generate_id(connection);
        xcb_render_create_picture(connection, result.src,
                hints.icon_pixmap, visual_info->format, 0u, NULL);
        s_icon_scale_apply(connection, result.src,
                (struct dimensions_s) { pixmap_geom->width,
                    pixmap_geom->height },
                draw_size, &dest_dim);
        *out_dest_w = (uint16_t) dest_dim.w;
        *out_dest_h = (uint16_t) dest_dim.h;

        if ((hints.flags & XCB_ICCCM_WM_HINT_ICON_MASK) &&
                hints.icon_mask != XCB_NONE) {
            const xcb_render_pictforminfo_t *mask_info =
                xcb_render_util_find_standard_format(formats,
                        XCB_PICT_STANDARD_A_1);
            struct dimensions_s mask_dest_dim;

            if (mask_info != NULL) {
                result.mask = xcb_generate_id(connection);
                xcb_render_create_picture(connection, result.mask,
                        hints.icon_mask, mask_info->id, 0u, NULL);
                /* 'icon_mask' is defined to share 'icon_pixmap's
                 * dimensions, so this recomputes the identical
                 * 'dest_w'/'dest_h' already returned above; discarded
                 * into throwaway locals rather than passed
                 * 'out_dest_w'/ 'out_dest_h' again, so the caller's
                 * copies (set from 'icon_pixmap' just above) are never
                 * second- guessed by a mask whose geometry turned out
                 * to disagree. */
                s_icon_scale_apply(connection, result.mask,
                        (struct dimensions_s) { pixmap_geom->width,
                            pixmap_geom->height },
                        draw_size, &mask_dest_dim);
            }
        }
    }

    free(pixmap_geom);
    return result;
}


/**
 * @brief Composite an already built icon Picture onto a square area of
 *        @p drawable, centered and clipped
 *
 * @param connection   XCB connection
 * @param src_picture  Already built @c Picture, from
 *                     @a s_build_icon_picture or a cache hit; left
 *                     untouched, still owned by whichever cache slot it
 *                     came from
 * @param mask_picture RENDER mask to composite @p src_picture through,
 *                     or @c XCB_NONE to composite it as a fully opaque
 *                     rectangle
 * @param dest_dim     Width/height @p src_picture was built to draw at
 * @param drawable     Drawable to composite onto
 * @param offset_pos   Position, within @p drawable, of the square
 *                     area's top-left corner
 * @param area_size    Side length of the square area to center in and
 *                     clip to
 *
 * @see @c s_icccm_icon_td, for when @p mask_picture comes from
 *
 * @note Complexity: @e O(1); the expensive per-pixel work already
 *       happened whenever @p src_picture was originally built
 */
static void s_composite_cached(xcb_connection_t *connection,
        xcb_render_picture_t src_picture,
        xcb_render_picture_t mask_picture, struct dimensions_s dest_dim,
        xcb_drawable_t drawable,
        struct position_s offset_pos, uint16_t area_size)
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

    /* Clip to the target square; with 'dest_dim' already at most
     * 'area_size' (vid. 's_build_icon_picture'), this should rarely
     * trim anything in practice, but stays as a defensive backstop
     * against a scale computation bug rather than letting one spill the
     * icon into, for example, a caption strip below the square in the
     * icon-window case, or a neighboring row's square when several
     * icons share one drawable (see 'wmicon_draw_at'). */
    clip_rect.x = (int16_t) offset_pos.x;
    clip_rect.y = (int16_t) offset_pos.y;
    clip_rect.width = area_size;
    clip_rect.height = area_size;
    xcb_render_set_picture_clip_rectangles(connection, dst_picture,
            0, 0, 1u, &clip_rect);

    dst_x = (int16_t) (offset_pos.x + ((area_size > dest_dim.w)
            ? (int16_t) ((area_size - dest_dim.w) / 2u) : 0));
    dst_y = (int16_t) (offset_pos.y + ((area_size > dest_dim.h)
            ? (int16_t) ((area_size - dest_dim.h) / 2u) : 0));

    xcb_render_composite(connection, XCB_RENDER_PICT_OP_OVER,
            src_picture, mask_picture, dst_picture, 0, 0, 0, 0,
            dst_x, dst_y, (uint16_t) dest_dim.w, (uint16_t) dest_dim.h);

    xcb_render_free_picture(connection, dst_picture);
}


/**
 * @brief Get the persistent graphics context the default icon is drawn
 *        with, creating it first if this is the first call for
 *        @p connection
 *
 * Never freed: held for the life of the process, the same as
 * @a s_get_formats's cached query above.  Its foreground color is
 * set fresh before every single fill (see @a s_draw_default_icon), so
 * reusing this same graphics context across calls never leaves a stale
 * color behind even when the caller's theme colors change between
 * one call and the next.  Only the underlying X server resource itself
 * is what gets reused here, never any color state on it.
 *
 * @param connection XCB connection
 * @param drawable   Any drawable of the depth this graphics context
 *                   will later be used against; only consulted the
 *                   first time this is called for @p connection
 *
 * @return The cached graphics context
 *
 * @note Complexity: @e O(1)
 */
static xcb_gcontext_t s_get_default_icon_gc(xcb_connection_t *connection,
        xcb_drawable_t drawable)
{
    if (s_default_icon_gc == XCB_NONE ||
            s_default_icon_gc_connection != connection) {
        s_default_icon_gc = xcb_generate_id(connection);
        xcb_create_gc(connection, s_default_icon_gc, drawable, 0u, NULL);
        s_default_icon_gc_connection = connection;
    }
    return s_default_icon_gc;
}


/**
 * @brief Draw a small default icon for a client with no usable icon of
 *        its
 *
 * It represents a generic window, three plain filled rectangles, no
 * text or curves, an outer frame in @p frame_color, a titlebar-like
 * strip along its top edge in that same color, and a body in
 * @p bg_color filling the rest.  Drawn procedurally, from plain XCB
 * fill-rectangle requests, rather than from an embedded bitmap image,
 * so it costs no extra storage in the binary and no image load at
 * startup.  The shape is also chosen for that same reason, plain
 * rectangles scale to any size without looking distorted or blurred,
 * unlike a bitmap that would need scaling, and stay legible at any size
 * this ends up drawn at, from a handful of pixels in a menu row up to
 * a full desktop icon square.  It reads unmistakably as a placeholder
 * rather than a broken real icon.
 *
 * @see @a wmicon_draw_at's comment in @c render/wmicon.h, for why
 *      @p frame_color and @p bg_color come from the caller instead of
 *      being resolved here
 * @see @a s_get_default_icon_gc, for the graphics context this draws
 *      with
 *
 * @param connection  XCB connection
 * @param drawable    Drawable to draw into
 * @param pos         Offset within @p drawable of the icon's
 *                    top-left corner
 * @param size        Side length, in pixels, of the (square) icon
 * @param frame_color Frame/titlebar color
 * @param bg_color    Body color
 *
 * @note No-op when @p size is @c 0
 * @note Complexity: @e O(1)
 */
static void s_draw_default_icon(xcb_connection_t *connection,
        xcb_drawable_t drawable, struct position_s pos, uint16_t size,
        uint32_t frame_color, uint32_t bg_color)
{
    xcb_gcontext_t gc;
    uint16_t margin;
    xcb_rectangle_t frame_rect;

    if (size == 0u) {
        return;
    }

    /* A proportion of 'size', not a fixed pixel count, so the frame
     * stays visible as a distinct ring even at the smallest sizes this
     * icon is ever drawn at.  A fixed 1px margin would all but vanish
     * at, say, 12px, without eating a disproportionate share of a large
     * desktop icon square either.  Halved and clamped so two margins
     * (left and right, or top and bottom) never meet or cross, even at
     * a 1-2px 'size' too small to fit both a visible frame and any body
     * at all. */
    margin = (uint16_t) ((size + 9u) / 10u);
    if (margin == 0u) {
        margin = 1u;
    }
    if ((uint16_t) (margin * 2u) >= size) {
        margin = (uint16_t) (size / 2u);
    }

    gc = s_get_default_icon_gc(connection, drawable);
    xcb_change_gc(connection, gc, XCB_GC_FOREGROUND,
            (const uint32_t[]) { frame_color });

    frame_rect.x = (int16_t) pos.x;
    frame_rect.y = (int16_t) pos.y;
    frame_rect.width = size;
    frame_rect.height = size;
    xcb_poly_fill_rectangle(connection, drawable, gc, 1, &frame_rect);

    if (size > (uint16_t) (2u * margin)) {
        xcb_rectangle_t body_rect;
        uint16_t titlebar_h;
        uint16_t inner = (uint16_t) (size - 2u * margin);

        /* Roughly a titlebar's proportion of a real decorated
         * window, not an exact match to any theme value.  This icon is
         * drawn well below the size 'window.titlebar.height' itself
         * assumes, and staying purely proportional to 'size' keeps it
         * looking right at every size rather than just the one or two
         * it might have been tuned against. */
        titlebar_h = (uint16_t) ((inner * 3u) / 10u);

        xcb_change_gc(connection, gc, XCB_GC_FOREGROUND,
                (const uint32_t[]) { bg_color });
        body_rect.x = (int16_t) (pos.x + margin);
        body_rect.y = (int16_t) (pos.y + margin + titlebar_h);
        body_rect.width = inner;
        body_rect.height = (uint16_t) (inner - titlebar_h);
        xcb_poly_fill_rectangle(connection, drawable, gc, 1,
                &body_rect);
    }

    /* No 'xcb_free_gc' here: 'gc' is the persistent cache from
     * 's_get_default_icon_gc', reused across every call rather than
     * freed after this one. */
}


/**
 * @brief Draw the default icon centered within @p area_size, the same
 *        way @a s_composite_cached centers a real icon's @p draw_size
 *        square within it
 *
 * A thin wrapper over @a s_draw_default_icon, factored out since both
 * @a wmicon_draw_at's cache-hit path and its cache-miss path need
 * this exact same centering arithmetic.
 *
 * @param connection  XCB connection
 * @param drawable    Drawable to draw into
 * @param pos         Offset, within @p drawable, of the icon square's
 *                    top-left corner
 * @param area_size   Side length of the square the icon is centered
 *                    within
 * @param draw_size   Side length of the (square) icon itself
 * @param frame_color Frame/titlebar color
 * @param bg_color    Body color
 *
 * @note Complexity: @e O(1)
 */
static void s_draw_default_icon_centered(xcb_connection_t *connection,
        xcb_drawable_t drawable, struct position_s pos,
        uint16_t area_size, uint16_t draw_size, uint32_t frame_color,
        uint32_t bg_color)
{
    struct position_s offset_pos;

    offset_pos.x = pos.x + ((area_size > draw_size)
            ? (int16_t) ((area_size - draw_size) / 2u) : 0);
    offset_pos.y = pos.y + ((area_size > draw_size)
            ? (int16_t) ((area_size - draw_size) / 2u) : 0);

    s_draw_default_icon(connection, drawable, offset_pos,
            draw_size, frame_color, bg_color);
}


/* Fetch and draw a client's icon, EWMH first, ICCCM as fallback, at
 * an explicit offset within 'drawable' */
void wmicon_draw_at(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        xcb_drawable_t drawable, struct position_s pos,
        uint16_t area_size, uint32_t frame_color, uint32_t bg_color,
        wmicon_cache_td *cache)
{
    xcb_ewmh_get_wm_icon_reply_t reply;
    xcb_ewmh_wm_icon_iterator_t iter;
    uint16_t draw_size;
    const uint32_t *best_data = NULL;
    xcb_render_picture_t built = XCB_NONE;
    xcb_render_picture_t built_mask = XCB_NONE;
    uint16_t dest_w = 0u;
    uint16_t dest_h = 0u;
    bool have_ewmh_icon;

    if (connection == NULL || ewmh == NULL || window == XCB_NONE ||
            drawable == XCB_NONE || area_size == 0u || cache == NULL) {
        return;
    }

    draw_size = (uint16_t)
        (((uint32_t) area_size * WM_ICON_PIXMAP_SCALE_PERCENT) / 100u);
    if (draw_size == 0u) {
        draw_size = 1u;
    }

    /* Cache hit: the same 'Picture' already built for this exact
     * 'draw_size' is reused as-is, skipping the property fetch,
     * per-pixel premultiply, and pixmap upload entirely. */
    if (cache->picture != XCB_NONE && cache->draw_size == draw_size) {
        s_composite_cached(connection, cache->picture,
                cache->mask_picture,
                (struct dimensions_s) { cache->dest_w, cache->dest_h },
                drawable, pos, area_size);
        return;
    }

    /* Cache hit for the "no icon" case: a previous call already fetched
     * both properties and found neither usable, so this redraws the
     * default icon straight away instead of repeating both fetches only
     * to reach that same answer again. */
    if (cache->has_no_icon && cache->draw_size == draw_size) {
        s_draw_default_icon_centered(connection, drawable, pos,
                area_size, draw_size, frame_color, bg_color);
        return;
    }

    have_ewmh_icon = xcb_ewmh_get_wm_icon_reply(ewmh,
            xcb_ewmh_get_wm_icon(ewmh, window), &reply, NULL) != 0u;

    if (have_ewmh_icon) {
        uint32_t best_width = 0u;
        uint32_t best_height = 0u;
        int64_t best_score = -1;
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
    }

    /* No '_NET_WM_ICON' at all, or one present but with no usable entry
     * in it.  Fall back to the older ICCCM 'WM_HINTS' icon hint rather
     * than drawing nothing, since a number of still-common applications
     * (see 's_build_icccm_icon_picture') only ever publish that one. */
    if (built == XCB_NONE) {
        s_icccm_icon_td icccm_icon = s_build_icccm_icon_picture(
                connection, window, draw_size, &dest_w, &dest_h);

        built = icccm_icon.src;
        built_mask = icccm_icon.mask;
    }

    if (built == XCB_NONE) {
        /* Neither property gave a usable icon.  Cache that fact (see
         * 'has_no_icon''s comment in 'render/wmicon.h') before drawing
         * the default icon, so the next call redraws it straight away
         * instead of repeating both fetches. */
        wmicon_invalidate(connection, cache);
        cache->draw_size = draw_size;
        cache->has_no_icon = true;
        s_draw_default_icon_centered(connection, drawable, pos,
                area_size, draw_size, frame_color, bg_color);
        return;
    }

    wmicon_invalidate(connection, cache);
    cache->picture = built;
    cache->mask_picture = built_mask;
    cache->draw_size = draw_size;
    cache->dest_w = dest_w;
    cache->dest_h = dest_h;

    s_composite_cached(connection, cache->picture, cache->mask_picture,
            (struct dimensions_s) { cache->dest_w, cache->dest_h },
            drawable, pos, area_size);
}


/* Thin wrapper over 'wmicon_draw_at' with its offset fixed at (0, 0);
 * see this function's Doxygen comment in wmicon.h */
void wmicon_draw(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, xcb_drawable_t drawable,
        uint16_t area_size, uint32_t frame_color, uint32_t bg_color,
        wmicon_cache_td *cache)
{
    wmicon_draw_at(connection, ewmh, window, drawable,
            (struct position_s) { 0, 0 }, area_size,
            frame_color, bg_color, cache);
}


/* Invalidate a cache slot, freeing its cached Picture's X server
 * resource */
void wmicon_invalidate(xcb_connection_t *connection,
        wmicon_cache_td *cache)
{
    if (cache == NULL) {
        return;
    }

    if (connection != NULL) {
        if (cache->picture != XCB_NONE) {
            xcb_render_free_picture(connection, cache->picture);
        }
        if (cache->mask_picture != XCB_NONE) {
            xcb_render_free_picture(connection, cache->mask_picture);
        }
    }
    cache->picture = XCB_NONE;
    cache->mask_picture = XCB_NONE;
    cache->draw_size = 0u;
    cache->dest_w = 0u;
    cache->dest_h = 0u;
    cache->has_no_icon = false;
}


/* Release the cached picture-format query and default-icon graphics
 * context held across all 'wmicon_draw' calls */
void wmicon_renderer_destroy(void)
{
    if (s_formats != NULL) {
        free((void *) s_formats);
        s_formats = NULL;
    }
    s_formats_connection = NULL;

    if (s_default_icon_gc != XCB_NONE &&
            s_default_icon_gc_connection != NULL) {
        xcb_free_gc(s_default_icon_gc_connection, s_default_icon_gc);
    }
    s_default_icon_gc = XCB_NONE;
    s_default_icon_gc_connection = NULL;
}
