/**
 * @file input/mouse/cursor.h
 *
 * @brief Resize and move pointer cursor allocation and lookup
 *
 * @defgroup input_mouse Mouse input
 * @ingroup input
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_CURSOR_H
#define INPUT_MOUSE_CURSOR_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>


/* Public interface */
/**
 * @brief Create the eight border-resize cursors used for hover feedback
 *
 * Allocates the cursors once for the whole session (matching the
 * left-pointer cursor @a wm_startup_subscribe_root_events already
 * sets up) so that @a mouse_handle_motion_hover only ever has to
 * look one up,
 * never create one.  Safe to call more than once; only the first call
 * actually allocates anything.
 *
 * @param connection XCB connection used to create the cursors
 *
 * @note Call @a mouse_resize_cursors_destroy at shutdown to free them
 * @note Complexity: @e O(1)
 */
void mouse_resize_cursors_init(xcb_connection_t *connection);

/**
 * @brief Free the cursors created by @c mouse_resize_cursors_init
 *
 * Safe to call even if they were never created.
 *
 * @param connection XCB connection used to free the cursors
 *
 * @note Complexity: @e O(1)
 */
void mouse_resize_cursors_destroy(xcb_connection_t *connection);

/**
 * @brief The plain-pointer cursor, the same one shown for
 *        @c S_RESIZE_ZONE_NONE
 *
 * Meant for a client's own window to be given this cursor explicitly,
 * once, at decoration time (see @a ci_create_decorations in
 * @c client/geom.c), rather than left to inherit whatever the frame's
 * own cursor happens to currently be set to.
 *
 * Explicit beats inherited.  Once the client's own window has its own
 * cursor, the X server shows it the instant the pointer crosses into
 * that window, with no window-manager-side event handling required at
 * all, unlike relying on catching every possible crossing or motion
 * event (which a client that intercepts pointer motion for its own
 * purposes, e.g., to track hover for its own UI, can prevent from ever
 * reaching this window manager in the first place).
 *
 * @return The plain-pointer cursor, or @c 0 if
 *         @a mouse_resize_cursors_init has not run yet
 *
 * @note Complexity: @e O(1)
 */
xcb_cursor_t mouse_plain_cursor(void);

/**
 * @brief The four-way move cursor, shown for the duration of an
 *        interactive window move
 *
 * Passed as @a xcb_grab_pointer's own cursor argument by @a drag_start
 * (see @c input/mouse/drag.c) so the cursor stays the move shape for
 * the whole drag regardless of which window the pointer happens to be
 * over, rather than left to whatever cursor that window's own attribute
 * is separately set to.
 *
 * @return The move cursor, or @c 0 if @a mouse_resize_cursors_init
 *         has not run yet
 *
 * @note Complexity: @e O(1)
 */
xcb_cursor_t mouse_cursor_move(void);

/**
 * @brief The border-resize cursor matching a given resize drag's own
 *        axis/anchor combination
 *
 * Passed as @a xcb_grab_pointer's own cursor argument by @a drag_start
 * for a resize drag, the same way @a mouse_cursor_move is for a move:
 * @p resize_w / @p resize_h say which axis (or both, for a corner) the
 * drag actually changes, and @p anchor_right / @p anchor_bottom say
 * which edge of that axis stays fixed (see @a drag_start_directed's own
 * doc comment in input/mouse/drag.h for their exact meaning), together
 * resolving to exactly one of the eight border cursors
 * @a mouse_resize_cursors_init already loaded.
 *
 * @param resize_w      Whether this drag changes the width
 * @param resize_h      Whether this drag changes the height
 * @param anchor_right  Whether the right edge stays fixed (only
 *                      meaningful when @p resize_w is @c true)
 * @param anchor_bottom Whether the bottom edge stays fixed (only
 *                      meaningful when @p resize_h is @c true)
 *
 * @return Matching resize cursor, or the plain-pointer cursor
 *         (see @a mouse_plain_cursor) when neither @p resize_w nor
 *         @p resize_h is @c true
 *
 * @note Complexity: @e O(1)
 */
xcb_cursor_t mouse_resize_cursor_for_axes(bool resize_w, bool resize_h,
        bool anchor_right, bool anchor_bottom);

/**
 * @brief Recompute and apply the resize-border cursor for a client
 *        window at a given pointer position
 *
 * Shared by @a mouse_handle_motion_hover and @a mouse_hover_poll_tick
 * and @a mouse_handle_enter, since any one kind of event or poll can
 * be the only signal a given transition actually produces.
 *
 * @param connection XCB connection
 * @param surfaces   Every managed surface, to look up the client
 *                   @p window belongs to
 * @param window     Window the crossing, motion, or poll was
 *                   evaluated for
 * @param root_pos   Pointer position in root-window coordinates
 *
 * @return The resolved client @p window belongs to, or @c NULL if it
 *         does not belong to a resizable client
 *
 * @note Complexity: @e O(1)
 */
client_td *mouse_resize_cursor_update(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t window,
        struct position_s root_pos);


#endif  /* ! INPUT_MOUSE_CURSOR_H */
