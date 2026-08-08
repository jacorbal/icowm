/**
 * @file render/wmicon.h
 *
 * @brief Client-supplied @c _NET_WM_ICON rendering
 *
 * A single entry point, @c wmicon_draw, that fetches a client's own
 * @c _NET_WM_ICON (the EWMH property most applications publish so a
 * taskbar, pager, or window manager can show the application's own
 * icon rather than a generic placeholder), picks whichever available
 * size is closest to the target it will be drawn at, and composites
 * it there via the X RENDER extension, so translucent edges in the
 * source icon blend correctly instead of leaving a hard square
 * artifact.
 *
 * Scaled to a consistent size regardless of whatever size the source
 * image happened to be (see @c WM_ICON_PIXMAP_SCALE in defs/icon.h),
 * since applications publish wildly differing icon sizes and drawing
 * each one at its own natural size would leave icons looking
 * inconsistent next to one another.
 *
 * The built Picture is cached by the caller (see @c wmicon_cache_td
 * below) across calls, since the same icon is very often redrawn
 * several times in a row without its underlying property ever
 * changing (an unrelated client on the same desktop moving marks the
 * whole desktop for a full repaint, redrawing every other icon on it
 * too; an @c Expose event after a virtual terminal switch; cycling
 * selection away from a client): only the first draw after the
 * property last actually changed does the full fetch, per-pixel
 * premultiply, and pixmap upload; every draw after that just
 * composites the same already-built Picture again.  See
 * @c wmicon_invalidate for when that cache needs to be thrown away.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_WMICON_H
#define RENDER_WMICON_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/render.h>


/**
 * @brief One client's cached, already built @c _NET_WM_ICON Picture
 *
 * Every field is opaque to the caller and managed entirely by
 * @c wmicon_draw and @c wmicon_invalidate; a caller only needs to
 * hold one of these per client (zero-initialized, e.g., by @c calloc,
 * same as every other @c client_td field) and pass a pointer to it
 * into both functions.
 */
typedef struct {
    xcb_render_picture_t picture; /**< 0 when nothing is cached yet */
    uint16_t draw_size;  /**< Side length 'picture' was built to fit
                               within; a mismatch against a later call
                               forces a rebuild */
    uint16_t dest_w;     /**< Actual drawn width, after fitting the
                               source's own aspect ratio within
                               'draw_size'; needed again on every cache
                               hit, since a hit skips re-fetching the
                               property and so has no other way left to
                               know the source's own dimensions */
    uint16_t dest_h;     /**< Actual drawn height; see 'dest_w' */
} wmicon_cache_td;


/* Public interface */
/**
 * @brief Fetch and draw a client's own @c _NET_WM_ICON, centered in
 *        and clipped to a square area of the given drawable
 *
 * A no-op, silently, when the client has not set @c _NET_WM_ICON at
 * all: not every application publishes one, and this is not an error
 * condition the icon window rendering pipeline needs to know about.
 *
 * @param connection XCB connection
 * @param ewmh       EWMH connection, for the typed @c _NET_WM_ICON
 *                   property getter
 * @param window     Client's own window, whose @c _NET_WM_ICON
 *                   property is read (not the icon window itself)
 * @param drawable   Icon window (or other drawable) to composite onto
 * @param area_size  Side length, in pixels, of the square area the
 *                   icon is centered in and clipped to
 * @param cache      This client's cache slot (e.g.,
 *                   @c &client->icon_pixmap_cache); read first to
 *                   check for a usable cached Picture before doing
 *                   any of the fetch/premultiply/upload work, and
 *                   updated whenever that work does end up running,
 *                   so the next call can skip it
 *
 * @note Complexity: @e O(1) on a cache hit; @e O(p) on a cache miss,
 *       where @e p is the pixel count of whichever icon size is
 *       chosen to draw
 */
void wmicon_draw(xcb_connection_t *connection, xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, xcb_drawable_t drawable, uint16_t area_size,
        wmicon_cache_td *cache);

/**
 * @brief Invalidate a cache slot, freeing its cached Picture's X
 *        server resource
 *
 * Call this whenever the @c _NET_WM_ICON property a cache slot was
 * built from might have changed (see the @c PropertyNotify handler in
 * handler/focus.c) or when the client owning the cache slot is being
 * destroyed (see @c client_destroy), so a stale image is never either
 * still drawn or leaked as an unreachable server-side resource.  A
 * no-op if nothing is currently cached in @p cache.
 *
 * @param connection XCB connection
 * @param cache      Cache slot to invalidate; its @c picture is freed
 *                   and reset to 0
 *
 * @note Complexity: @e O(1)
 */
void wmicon_invalidate(xcb_connection_t *connection, wmicon_cache_td *cache);


#endif  /* ! RENDER_WMICON_H */
