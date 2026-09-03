/**
 * @file render/wmicon.h
 *
 * @brief Client-supplied icon rendering: @c _NET_WM_ICON (EWMH), with
 *        an ICCCM @c WM_HINTS fallback
 *
 * A single entry point, @c wmicon_draw, that fetches a client's
 * @c _NET_WM_ICON (the EWMH property most applications publish so
 * a taskbar, pager, or window manager can show the application's
 * icon rather than a generic placeholder), picks whichever available
 * size is closest to the target it will be drawn at, and composites it
 * there via the X RENDER extension, so translucent edges in the source
 * icon blend correctly instead of leaving a hard square artifact.
 *
 * A client that never published @c _NET_WM_ICON at all (some still
 * don't; @c xterm is the canonical example, offering only its
 * @c iconHint resource) falls back to whatever @c WM_HINTS icon hint it
 * did set instead of drawing nothing.
 *
 * Scaled to a consistent size regardless of whatever size the source
 * image happened to be (cfr. @c WM_ICON_PIXMAP_SCALE_PERCENT in
 * @c defs/icon.h), since applications publish wildly differing icon
 * sizes and drawing each one at its natural size would leave icons
 * looking inconsistent next to one another.
 *
 * The built @c Picture is cached by the caller across calls, since the
 * same icon is very often redrawn several times in a row without its
 * underlying property ever changing (an unrelated client on the same
 * desktop moving marks the whole desktop for a full repaint, redrawing
 * every other icon on it too, an @c Expose event after a virtual
 * terminal switch, cycling selection away from a client).  Only the
 * first draw after the property last actually changed does the full
 * fetch, per-pixel premultiply, and pixmap upload.  Every draw after
 * that just composites the same already-built @c Picture again.
 * A client confirmed to have neither icon property set is cached the
 * same way, so its default icon gets redrawn on every later call
 * without repeating either property fetch.
 *
 * @see @c wmicon_cache_td below, for the cache itself
 * @see @a wmicon_invalidate, for when that cache needs to be thrown
 *      away
 *
 * @ingroup render
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
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/render.h>

/* Types includes */
#include <types/pair.h>


/**
 * @brief One client's cached icon state, an already built @c Picture or
 *        a confirmed absence of one
 *
 * Every field is opaque to the caller and managed entirely by
 * @a wmicon_draw and @a wmicon_invalidate; a caller only needs to hold
 * one of these per client (zero-initialized, e.g., by @c calloc, same
 * as every other @c client_td field) and pass a pointer to it into both
 * functions.
 */
typedef struct {
    xcb_render_picture_t picture;   /**< 0 when nothing is cached */
    xcb_render_picture_t mask_picture;  /**< 0 unless the cached
                                             @p picture came from the
                                             ICCCM @c WM_HINTS fallback
                                             and that client also set
                                             an @p icon_mask; composited
                                             as the RENDER mask
                                             alongside @p picture */
    uint16_t draw_size;  /**< Side length @p picture was built to fit
                              within, or the default icon was last drawn
                              at when @p has_no_icon is set; a mismatch
                              against a later call forces a rebuild
                              either way */
    uint16_t dest_w;     /**< Actual drawn width, after fitting the
                              source's own aspect ratio within
                              @p draw_size; needed again on every cache
                              hit, since a hit skips re-fetching the
                              property and so has no other way left to
                              know the source's own dimensions */
    uint16_t dest_h;     /**< Actual drawn height; see @p dest_w */
    bool has_no_icon;    /**< Set once a fetch confirms @p window has
                              neither a usable @c _NET_WM_ICON nor
                              a usable @c WM_HINTS icon, so the next
                              call can redraw the default icon straight
                              away instead of repeating both property
                              fetches only to reach the same answer
                              again */
} wmicon_cache_td;


/* Public interface */
/**
 * @brief Fetch and draw a client's icon, centered in and clipped to
 *        a square area of the given drawable
 *
 a Prefers the EWMH @c _NET_WM_ICON property (an array of ARGB32 images
 * at several sizes, letting the closest one to @p area_size be picked).
 *
 * When a client has not published that, falls back to the older ICCCM
 * @c WM_HINTS icon hint instead, since a number of still-common
 * applications (@c xterm among them, via its @c iconHint resource)
 * only ever set the latter.  That fallback supports both forms ICCCM
 * allows, a 1-bit-deep @p icon_pixmap rendered as a solid-color stencil
 * (ICCCM's literal specification), and a full-depth one (what
 * @c xterm itself actually publishes, despite ICCCM specifying depth 1)
 * rendered as a plain color image.  Either is clipped to @p icon_mask's
 * own shape when the client also set one.  When neither property is set
 * at all (not every application publishes an icon, and that is not an
 * error condition the icon window rendering pipeline needs to know
 * about), draws a small default icon instead of leaving the square
 * blank.
 *
 * A thin wrapper over @c wmicon_draw_at with its offset fixed at
 * @c (0, 0); the drawable this draws into is assumed to belong to this
 * one icon alone (e.g., an iconified client's icon window), as
 * opposed to @c wmicon_draw_at's use case of one icon among several
 * sharing a single larger drawable.
 *
 * @param connection  XCB connection
 * @param ewmh        EWMH connection, for the typed @c _NET_WM_ICON
 *                    property getter
 * @param window      Client's window, whose @c _NET_WM_ICON and
 *                    @c WM_HINTS properties are read (not the icon
 *                    window itself)
 * @param drawable    Icon window (or other drawable) to composite onto
 * @param area_size   Side length, in pixels, of the square area the
 *                    icon is centered in and clipped to
 * @param frame_color Default icon's frame/titlebar color, used only
 *                    when @p window has no usable icon of its own
 * @param bg_color    Default icon's body color (same caveat)
 * @param cache       This client's cache slot; read first to check for
 *                    a usable cached @c Picture before doing any of the
 *                    fetch/premultiply/upload work, and updated
 *                    whenever that work does end up running, so the
 *                    next call can skip it
 *
 * @note Complexity: @e O(1) on a cache hit; @e O(p) on a cache miss,
 *       where @e p is the pixel count of whichever icon size is chosen
 *       to draw
 *
 * @see @a wmicon_draw_at for what that default icon looks like and why
 */
void wmicon_draw(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        xcb_drawable_t drawable, uint16_t area_size,
        uint32_t frame_color, uint32_t bg_color,
        wmicon_cache_td *cache);

/**
 * @brief Like @a wmicon_draw, but composites at an explicit offset
 *        within @p drawable instead of always at its origin
 *
 * For a caller that draws several icons into one shared window at
 * different positions (e.g., one per row of a menu listing), rather
 * than each icon owning its dedicated drawable the way an
 * iconified client's icon window does.  Everything else, the
 * EWMH/ICCCM fallback, the cache, the centering and clipping within the
 * @p area_size square, behaves exactly as in @a wmicon_draw.
 * @p x and @p y are simply where that square's top-left corner
 * sits within @p drawable instead of always @c (0, 0).
 *
 * When @p window has no usable @c _NET_WM_ICON or @c WM_HINTS icon of
 * its, draws a small default icon instead of leaving the square
 * blank.  It represents a generic window, an outer frame in
 * @p frame_color, with a titlebar-like strip along its top edge in that
 * same color, and a body in @p bg_color between the two.  It is drawn
 * procedurally, with plain filled rectangles only, rather than from an
 * embedded image, so it costs no extra storage or loading and stays
 * legible at every size this ends up drawn at, from a handful of pixels
 * in a menu row up to a full desktop icon square.
 *
 * @p frame_color and @p bg_color are deliberately taken from the caller
 * rather than from a theme pointer resolved here.  Which colors count
 * as active versus inactive (or a menu row's selected/unselected
 * pair, not even the same @c config_theme_style_s shape) is already
 * worked out at every call site, and duplicating that logic in a module
 * with no theme access of its own would only risk drifting out of sync
 * with it.
 *
 * @param connection  XCB connection
 * @param ewmh        EWMH connection, for the typed @c _NET_WM_ICON
 *                    property getter
 * @param window      Client's window, whose @c _NET_WM_ICON and
 *                    @c WM_HINTS properties are read (not the icon
 *                    window itself)
 * @param drawable    Drawable to composite onto
 * @param pos         Offset, within @p drawable, of the icon square's
 *                    own top-left corner
 * @param area_size   Side length, in pixels, of the square area the
 *                    icon is centered in and clipped to
 * @param frame_color Default icon's frame/titlebar color
 * @param bg_color    Default icon's body color
 * @param cache       This client's cache slot; see @a wmicon_draw
 *
 * @note Complexity: @e O(1) on a cache hit; @e O(p) on a cache miss,
 *       where @e p is the pixel count of whichever icon size is
 *       chosen to draw
 */
void wmicon_draw_at(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        xcb_drawable_t drawable, struct position_s pos,
        uint16_t area_size, uint32_t frame_color, uint32_t bg_color,
        wmicon_cache_td *cache);

/**
 * @brief Invalidate a cache slot, freeing its cached @c Picture's
 *        X server resource
 *
 * Call this whenever the @c _NET_WM_ICON or @c WM_HINTS property
 * a cache slot was built from might have changed, or when the client
 * owning the cache slot is being destroyed, so a stale image is never
 * either still drawn or leaked as an unreachable server-side resource.
 *
 * @param connection XCB connection
 * @param cache      Cache slot to invalidate; its @c picture and
 *                   @c mask_picture are freed and reset to @c 0, and
 *                   @c has_no_icon is reset to @c false
 *
 * @see The @c PropertyNotify handler in @c handler/focus.c
 * @see @a client_destroy
 *
 * @note A no-op if nothing is currently cached in @p cache
 * @note Complexity: @e O(1)
 */
void wmicon_invalidate(xcb_connection_t *connection,
        wmicon_cache_td *cache);

/**
 * @brief Release the cached picture-format query and default-icon
 *        graphics context held across all @c wmicon_draw calls
 *
 * Call this once, alongside @a text_renderer_destroy, when the window
 * manager is shutting down, so nothing this file cached at the
 * connection level outlives the connection it was queried for.
 *
 * @note A no-op if @a wmicon_draw was never called
 * @note Complexity: @e O(1)
 */
void wmicon_renderer_destroy(void);


#endif  /* ! RENDER_WMICON_H */
