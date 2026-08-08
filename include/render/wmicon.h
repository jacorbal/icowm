/**
 * @file render/wmicon.h
 *
 * @brief Client-supplied @c _NET_WM_ICON rendering
 *
 * A single entry point, @c wmicon_draw, that fetches a client's own
 * @c _NET_WM_ICON (the EWMH property most applications publish so a
 * taskbar, pager, or window manager can show the application's own
 * icon rather than a generic placeholder), picks whichever available
 * size is closest to the icon window's square icon-graphic area, and
 * composites it there via the X RENDER extension, so translucent
 * edges in the source icon blend correctly instead of leaving a hard
 * square artifact.
 *
 * Deliberately not cached: unlike @c render/glyph.c, where the same
 * handful of glyphs get redrawn constantly, an iconified client's
 * icon is redrawn only on discrete events (iconifying it, a cycle
 * selection change, a theme reload), which does not happen often
 * enough for the fetch-and-composite cost to be worth the added
 * complexity of tracking when a cached icon goes stale.
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
 * @param ewmh       EWMH connection, for the typed
 *                   @c _NET_WM_ICON property getter
 * @param window     Client's own window, whose @c _NET_WM_ICON
 *                   property is read (not the icon window itself)
 * @param drawable   Icon window (or other drawable) to composite
 *                   onto
 * @param area_size  Side length, in pixels, of the square area the
 *                   icon is centered in and clipped to
 *
 * @note Complexity: @e O(p), where @e p is the pixel count of
 *       whichever icon size is chosen to draw
 */
void wmicon_draw(xcb_connection_t *connection, xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, xcb_drawable_t drawable, uint16_t area_size);


#endif  /* ! RENDER_WMICON_H */
