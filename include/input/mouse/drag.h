/**
 * @file input/mouse/drag.h
 *
 * @brief Mouse drag-operation state and interface
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_DRAG_H
#define INPUT_MOUSE_DRAG_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>


/* Public interface */
/**
 * @brief Begin a drag operation for a managed client window
 *
 * Records the drag start state (client pointer, operation type, initial
 * pointer position, initial client geometry, and screen bounds for edge
 * snap) and installs a pointer grab so that motion and release events
 * are delivered reliably.
 *
 * @param connection XCB connection
 * @param root       Root window on which to grab the pointer
 * @param client     Client being moved or resized
 * @param desktop    Desktop that owns @p client (may be null)
 * @param operation  @c CLIENT_OPERATION_MOVING or
 *                   @c CLIENT_OPERATION_RESIZING
 * @param event_time Timestamp from the triggering button-press event
 * @param root_x     Root-relative X of the pointer at press time
 * @param root_y     Root-relative Y of the pointer at press time
 * @param screen_w   Screen width in pixels (0 to disable snap)
 * @param screen_h   Screen height in pixels (0 to disable snap)
 * @param snap       Snap distance in pixels (0 to disable snap)
 *
 * @note Complexity: @e O(1)
 */
void drag_start(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        enum window_operation_e operation,
        xcb_timestamp_t event_time,
        int16_t root_x, int16_t root_y,
        uint32_t screen_w, uint32_t screen_h,
        uint32_t snap);

/**
 * @brief Begin a drag operation for an icon window
 *
 * Like @a drag_start, but the drag target is the icon window of
 * @p client rather than the decorated client frame.
 *
 * @param connection XCB connection
 * @param root       Root window on which to grab the pointer
 * @param client     Client whose icon window is being dragged
 * @param icon_x     Current icon window X (screen-relative)
 * @param icon_y     Current icon window Y (screen-relative)
 * @param event_time Timestamp from the triggering button-press event
 * @param root_x     Root-relative X of the pointer at press time
 * @param root_y     Root-relative Y of the pointer at press time
 *
 * @note Complexity: @e O(1)
 */
void drag_start_icon(xcb_connection_t *connection,
        xcb_window_t root,
        client_td *client,
        int32_t icon_x, int32_t icon_y,
        xcb_timestamp_t event_time,
        int16_t root_x, int16_t root_y);

/**
 * @brief Update the in-progress drag on a motion-notify event
 *
 * Applies the accumulated pointer delta to the client (move or resize)
 * by sending the appropriate client event.  For icon drags, moves the
 * icon window directly via @c xcb_configure_window.
 *
 * @param connection XCB connection
 * @param root_x     Current root-relative X of the pointer
 * @param root_y     Current root-relative Y of the pointer
 *
 * @note Complexity: @e O(1)
 */
void drag_update(xcb_connection_t *connection,
        int16_t root_x, int16_t root_y);

/**
 * @brief Finish the drag on a button-release event
 *
 * For icon drags, decides whether the pointer displacement exceeds the
 * click threshold: if not, restores the iconified client and focuses
 * it; if yes, persists the new icon position.  For normal drags, just
 * resets the drag state.  Releases the pointer grab in all cases.
 *
 * @param connection XCB connection
 * @param surface    Surface that owns the dragged client (may be null)
 * @param desktop    Desktop that owns the dragged client (may be null)
 * @param root_x     Root-relative X of the pointer at release time
 * @param root_y     Root-relative Y of the pointer at release time
 *
 * @note Complexity: @e O(1)
 */
void drag_end(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        int16_t root_x, int16_t root_y);

/**
 * @brief Cancel an in-progress drag when the dragged client disappears
 *
 * Resets all drag state without performing any client action and
 * releases the pointer grab.  Called by the destroy-notify handler when
 * the dragged client's window is destroyed mid-drag.
 *
 * @param connection XCB connection
 * @param client     Client that triggered the cancel (compared against
 *                   the current drag client; no-op if it does not match)
 *
 * @note Complexity: @e O(1)
 */
void drag_cancel(xcb_connection_t *connection, const client_td *client);

/**
 * @brief Query whether a drag operation is currently active
 *
 * @return @c true when a move or resize drag is in progress
 *
 * @note Complexity: @e O(1)
 */
bool drag_is_active(void);

/**
 * @brief Query whether the active drag is on an icon window
 *
 * @return @c true when the active drag is moving an icon window
 *
 * @note Complexity: @e O(1)
 */
bool drag_is_icon_drag(void);

/**
 * @brief Query whether a window is the active drag overlay window
 *
 * @param window X window identifier to compare
 *
 * @return @c true when @p window is the drag overlay window
 *
 * @note Complexity: @e O(1)
 */
bool drag_is_overlay_window(xcb_window_t window);

/**
 * @brief Return the client currently being dragged, or @c NULL
 *
 * @return Pointer to the dragged @c client_td, or @c NULL
 *
 * @note Complexity: @e O(1)
 */
client_td *drag_client(void);

/**
 * @brief Repaint the active drag overlay window
 *
 * Redraws the current geometry text into the overlay window created for
 * interactive move/resize feedback.
 *
 * @param connection XCB connection
 *
 * @note Complexity: @e O(1)
 */
void drag_repaint_overlay(xcb_connection_t *connection);

/**
 * @brief Return the current drag position
 *
 * Writes the most-recently applied target position into @p x and @p y.
 * For a window move this is the frame top-left; for an icon drag this
 * is the icon window top-left.  Both values are zero when no drag is
 * active.
 *
 * @param x Output X coordinate (may be null)
 * @param y Output Y coordinate (may be null)
 *
 * @note Complexity: @e O(1)
 */
void drag_current_pos(int32_t *x, int32_t *y);


#endif  /* ! INPUT_MOUSE_DRAG_H */
