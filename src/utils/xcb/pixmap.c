/**
 * @file utils/xcb/pixmap.c
 *
 * @brief Off-screen pixmap buffers standing in for a window
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

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <utils/xcb/pixmap.h>


/* Create an off-screen buffer sized to stand in for a window */
xcb_pixmap_t xcb_offscreen_buffer_create(xcb_connection_t *connection,
        uint8_t depth, xcb_drawable_t reference,
        uint16_t width, uint16_t height)
{
    xcb_pixmap_t buffer;

    if (width == 0u || height == 0u) {
        return XCB_NONE;
    }

    buffer = xcb_generate_id(connection);
    xcb_create_pixmap(connection, depth, buffer, reference,
            width, height);

    return buffer;
}
