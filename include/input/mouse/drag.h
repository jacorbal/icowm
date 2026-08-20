/**
 * @file input/mouse/drag.h
 *
 * @brief Core mouse drag-operation state machine
 *
 * Only the generic lifecycle every kind of drag shares (start, update,
 * end, cancel, and the small set of queries that apply regardless of
 * what is being dragged).  Icon-specific concerns live in
 * @c drag/icon.h, the feedback overlay window's own concerns in
 * @c drag/overlay.h, and edge-triggered desktop warping in
 * @c drag/warp.h.
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

/* Types includes */
#include <types/pair.h>


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
 * @param root_pos   Root-relative position of the pointer at press time
 * @param screen_dim Screen dimensions in pixels ((0, 0) to disable snap)
 * @param snap       Snap distance in pixels (0 to disable snap)
 *
 * @note Complexity: @e O(1)
 */
void drag_start(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        enum window_operation_e operation,
        xcb_timestamp_t event_time,
        struct position_s root_pos,
        struct dimensions_s screen_dim,
        uint32_t snap);

/**
 * @brief Begin a resize drag with an explicit anchor, rather than one
 *        @a drag_start would infer from @p root_pos
 *
 * For @c _NET_WM_MOVERESIZE (see @a hi_handle_net_wm_moveresize in
 * @c handler/ewmhmsg.c).  The requesting client names which edge or
 * corner it wants resized directly, rather than icowm inferring one
 * from where the pointer happens to be, since that position (wherever
 * the client's own custom resize grip was clicked) has no fixed
 * relationship to the client's actual border the way a normal
 * border-drag's position does.  Calls @a drag_start itself for
 * everything else (state recording, the pointer grab), then overwrites
 * just the anchor and per-axis resize flags it would otherwise have
 * inferred; every existing caller of @a drag_start itself is completely
 * unaffected.
 *
 * @param connection    XCB connection
 * @param root          Root window on which to grab the pointer
 * @param client        Client being resized
 * @param desktop       Desktop that owns @p client (may be null)
 * @param event_time    Timestamp from the triggering request
 * @param root_pos      Root-relative position of the pointer at
 *                      request time
 * @param screen_dim    Screen dimensions in pixels ((0, 0) to disable
 *                      snap)
 * @param snap          Snap distance in pixels (0 to disable snap)
 * @param anchor_right  @c true if the right edge stays fixed (a left,
 *                      top-left, or bottom-left drag)
 * @param anchor_bottom @c true if the bottom edge stays fixed (a top,
 *                      top-left, or top-right drag)
 * @param resize_w      @c true if this direction changes the width
 * @param resize_h      @c true if this direction changes the height
 *
 * @note Complexity: @e O(1)
 */
void drag_start_directed(xcb_connection_t *connection,
        xcb_window_t root, client_td *client, desktop_td *desktop,
        xcb_timestamp_t event_time,
        struct position_s root_pos,
        struct dimensions_s screen_dim,
        uint32_t snap,
        bool anchor_right, bool anchor_bottom,
        bool resize_w, bool resize_h);

/**
 * @brief Begin a resize drag, locking out whichever axis (or axes)
 *        @p axis_w_locked / @p axis_h_locked mark as unavailable
 *
 * For a client maximized on one axis only (horizontal or vertical; see
 * @a client_is_maximized_horz / @a client_is_maximized_vert), that axis
 * is snapped exactly to its workarea edge, so it has nothing left to
 * drag it wider or narrower with, the same way a fully maximized or
 * fullscreen client cannot be resized at all.  The other, still-free
 * axis keeps working exactly as a normal border drag would.  Calls @c
 * drag_start for everything else (state recording, the pointer grab,
 * and its own normal per-axis inference from @p root_pos), then clears
 * whichever axis flag(s) @p axis_w_locked / @p axis_h_locked ask for;
 * if that leaves neither axis resizable at all (the grab point was
 * only ever near the locked edge), the drag is cancelled outright via
 * @a drag_cancel rather than left running inert.
 *
 * @param connection    XCB connection
 * @param root          Root window on which to grab the pointer
 * @param client        Client being resized
 * @param desktop       Desktop that owns @p client (may be null)
 * @param event_time    Timestamp from the triggering request
 * @param root_pos      Root-relative position of the pointer at
 *                      request time
 * @param screen_dim    Screen dimensions in pixels ((0, 0) to disable
 *                      snap)
 * @param snap          Snap distance in pixels (0 to disable snap)
 * @param axis_w_locked @c true to force the width axis unresizable
 *                      regardless of where @p root_pos fell
 * @param axis_h_locked @c true to force the height axis unresizable
 *                      regardless of where @p root_pos fell
 *
 * @note Cfr. Karp, O'Reilly, & Mott, 2005, 'Windows XP in a Nutshell',
 *       2nd ed., ch. 2: "Maximized windows can't be moved or resized"
 * @note Complexity: @e O(1)
 */
void drag_start_resize_axis_locked(xcb_connection_t *connection,
        xcb_window_t root, client_td *client, desktop_td *desktop,
        xcb_timestamp_t event_time,
        struct position_s root_pos,
        struct dimensions_s screen_dim,
        uint32_t snap,
        bool axis_w_locked, bool axis_h_locked);

/**
 * @brief Update the in-progress drag on a motion-notify event
 *
 * Applies the accumulated pointer delta to the client (move or resize)
 * by sending the appropriate client event.  For icon drags, moves the
 * icon window directly via @c xcb_configure_window.
 *
 * @param connection XCB connection
 * @param root_pos   Current root-relative position of the pointer
 *
 * @note Complexity: @e O(1)
 */
void drag_update(xcb_connection_t *connection,
        struct position_s root_pos);

/**
 * @brief Finish the drag on a button-release event
 *
 * For icon drags, decides whether the pointer displacement exceeds the
 * click threshold.  If not, restores the iconified client and focuses
 * it; if yes, persists the new icon position.  For normal drags, just
 * resets the drag state.  Releases the pointer grab in all cases.
 *
 * @param connection XCB connection
 * @param surface    Surface that owns the dragged client (may be null)
 * @param desktop    Desktop that owns the dragged client (may be null)
 * @param root_pos   Root-relative position of the pointer at release
 *                   time
 *
 * @note Complexity: @e O(1)
 */
void drag_end(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        struct position_s root_pos);

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
 * @brief Return the client currently being dragged, or @c NULL
 *
 * @return Pointer to the dragged @c client_td, or @c NULL
 *
 * @note Complexity: @e O(1)
 */
client_td *drag_client(void);


#endif  /* ! INPUT_MOUSE_DRAG_H */
