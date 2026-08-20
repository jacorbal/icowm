/**
 * @file input/mouse/drag/icon.h
 *
 * @brief Everything specific to dragging an icon window
 *
 * @ingroup input_mouse
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_DRAG_ICON_H
#define INPUT_MOUSE_DRAG_ICON_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <desktop.h>


/**
 * @brief Begin a drag operation for an icon window
 *
 * Like @a drag_start (@c input/mouse/drag.h), but the drag target is
 * the icon window of @p client rather than the decorated client
 * frame.
 *
 * @param connection XCB connection
 * @param root       Root window on which to grab the pointer
 * @param client     Client whose icon window is being dragged
 * @param desktop    Desktop @p client currently sits on; needed for
 *                   @p desktops.warp_on_edge_drag (see
 *                   @a drag_warp_tick, @c drag/warp.h), the same as
 *                   @a drag_start's own @p desktop parameter
 * @param icon_x     Current icon window X (screen-relative)
 * @param icon_y     Current icon window Y (screen-relative)
 * @param event_time Timestamp from the triggering button-press event
 * @param root_x     Root-relative X of the pointer at press time
 * @param root_y     Root-relative Y of the pointer at press time
 * @param screen_w   Surface width, for edge snapping and
 *                   @p desktops.warp's own edge detection
 * @param screen_h   Surface height, for the same reason
 *
 * @note Complexity: @e O(1)
 */
void drag_icon_start(xcb_connection_t *connection,
        xcb_window_t root,
        client_td *client, desktop_td *desktop,
        int32_t icon_x, int32_t icon_y,
        xcb_timestamp_t event_time,
        int16_t root_x, int16_t root_y,
        uint32_t screen_w, uint32_t screen_h);

/**
 * @brief Query whether the active drag is on an icon window
 *
 * @return @c true when the active drag is moving an icon window
 *
 * @note Complexity: @e O(1)
 */
bool drag_is_icon_drag(void);

/**
 * @brief Return the full icon-window height for a dragged client
 *
 * Computes the icon height from the base icon square size and adds the
 * caption height when the client theme uses captioned icons.
 *
 * @param client Client whose icon height is requested
 *
 * @return Total icon-window height in pixels
 *
 * @note Complexity: @e O(1)
 */
uint16_t drag_icon_height(const client_td *client);


#endif  /* ! INPUT_MOUSE_DRAG_ICON_H */
