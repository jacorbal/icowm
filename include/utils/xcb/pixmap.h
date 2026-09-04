/**
 * @file utils/xcb/pixmap.h
 *
 * @brief Off-screen pixmap buffers standing in for a window
 *
 * A repaint that clears a window and then issues several separate draw
 * requests against it is visible mid-repaint on any window that gets
 * redrawn often enough (a blink timer, continuous mouse motion, or
 * a busy @c Expose source).  The window shows blank, or only partly
 * drawn, for the moment between those requests.
 *
 * Assembling the same content in a pixmap first and copying it onto the
 * window in one request avoids that, since the window itself only ever
 * changes once, already complete.
 *
 * @defgroup xcbpixmap Off-screen pixmap buffers
 * @ingroup utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_XCB_PIXMAP_H
#define UTILS_XCB_PIXMAP_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Create an off-screen buffer sized to stand in for a window
 *
 * @param connection Active XCB connection
 * @param depth      Depth to create the pixmap at, matching whatever
 *                   window it is going to stand in for
 * @param reference  Any drawable on the same screen the pixmap is
 *                   created against; only its screen matters here, not
 *                   its contents
 * @param width      Pixmap width
 * @param height     Pixmap height
 *
 * @return The new pixmap, or @c XCB_NONE when @p width or @p height is
 *         zero
 *
 * @note Complexity: @e O(1)
 */
xcb_pixmap_t xcb_offscreen_buffer_create(xcb_connection_t *connection,
        uint8_t depth, xcb_drawable_t reference,
        uint16_t width, uint16_t height);


#endif  /* ! UTILS_XCB_PIXMAP_H */
