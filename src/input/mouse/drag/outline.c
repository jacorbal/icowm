/**
 * @file input/mouse/drag/outline.c
 *
 * @brief Outline stand-in windows used by a non-solid drag
 *
 * Split out of what used to be a single, flat @c input/mouse/drag.c;
 * see @c drag/internal.h for why.  A thin adapter over @c render/
 * outline.c's own shared strip-window mechanism, supplying this
 * subsystem's own state (@c s_drag.root, @c s_drag.client's
 * configured active border color, @c s_drag.outline_windows) so
 * every one of this file's own callers keeps working unchanged.
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

/* Default initial values */
#include <defs/input.h>

/* Project includes */
#include <client.h>
#include <render/outline.h>

/* Local includes */
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/outline.h>


/* Begin an outline-mode drag: create and map the initial 4 strip
 * windows */
void drag_outline_start(xcb_connection_t *connection,
        struct geometry_s geom)
{
    uint32_t color = (s_drag.client != NULL &&
            s_drag.client->config != NULL)
        ? s_drag.client->config->theme.window.active.border.color
        : 0u;

    render_outline_show(connection, s_drag.root, geom,
            (uint32_t) WM_DRAG_OUTLINE_BORDER_WIDTH, color,
            XCB_WINDOW_NONE, s_drag.outline_windows);
}


/* Move the outline stand-in's own 4 strip windows to a new
 * rectangle */
void drag_outline_move(xcb_connection_t *connection,
        struct geometry_s geom)
{
    render_outline_move(connection, geom,
            (uint32_t) WM_DRAG_OUTLINE_BORDER_WIDTH,
            XCB_WINDOW_NONE, s_drag.outline_windows);
}


/* End an outline-mode drag: destroy the 4 strip windows */
void drag_outline_end(xcb_connection_t *connection)
{
    render_outline_hide(connection, s_drag.outline_windows);
}
