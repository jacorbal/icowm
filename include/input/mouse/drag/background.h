/**
 * @file input/mouse/drag/background.h
 *
 * @brief Drag-to-pan the viewport by clicking and dragging the empty
 *        desktop background
 *
 * A press on the root window that never was over any managed client
 * starts here, entirely independent of @c input/mouse/drag.h's own
 * state machine (which requires a @c client_td to operate on): there is
 * no client to move, only the stage's current desktop's viewport
 * origin.  Motion moves that origin by exactly the pointer's
 * accumulated delta since the press, the opposite way, so the desktop's
 * content visually follows the pointer 1:1, the same way dragging a
 * canvas does in any other paint or map application.  A release with
 * no real movement is treated as the plain click it always was
 * (unfocusing the active client), exactly like a background click
 * behaved before this file existed.
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

#ifndef INPUT_MOUSE_DRAG_BACKGROUND_H
#define INPUT_MOUSE_DRAG_BACKGROUND_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <stage.h>


/**
 * @brief Begin a background-pan drag
 *
 * Records the viewport origin @p stage's current desktop starts at
 * and installs a pointer grab so that motion and release events are
 * delivered reliably, exactly like @a drag_start does for a client
 * drag.
 *
 * @param connection XCB connection
 * @param stage      Stage whose current desktop's viewport pans
 * @param root       Root window on which to grab the pointer
 * @param event_time Timestamp from the triggering button-press event
 * @param root_pos   Root-relative position of the pointer at press time
 *
 * @note A no-op (no grab installed, nothing recorded) when @p stage
 *       has no resolvable current desktop
 * @note Complexity: @e O(1)
 */
void drag_background_start(xcb_connection_t *connection,
        stage_td *stage, xcb_window_t root,
        xcb_timestamp_t event_time, struct position_s root_pos);

/**
 * @brief Update the in-progress background-pan drag on a
 *        motion-notify event
 *
 * Moves the viewport straight to the origin it started this drag at,
 * offset by the pointer's accumulated delta since the press, the
 * opposite way.
 *
 * @param connection XCB connection
 * @param root_pos   Current root-relative position of the pointer
 *
 * @note A no-op when no background-pan drag is active
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the panned desktop (see @a scmd_stage_viewport_set)
 */
void drag_background_update(xcb_connection_t *connection,
        struct position_s root_pos);

/**
 * @brief Finish the background-pan drag on a button-release event
 *
 * When the pointer never moved past the same click-vs-drag threshold
 * an icon drag uses (see @c WM_ICON_DRAG_THRESHOLD), this is treated as
 * the plain background click it always was: the active client, if any,
 * is unfocused.  Releases the pointer grab in either case.
 *
 * @param connection XCB connection
 * @param stages     Stage list, to resolve the active client to
 *                   unfocus on a plain click (may be null, which just
 *                   skips that lookup)
 * @param root_pos Root-relative position of the pointer at release
 *                   time
 *
 * @note A no-op when no background-pan drag is active
 * @note Complexity: @e O(1)
 */
void drag_background_end(xcb_connection_t *connection,
        list_td *stages, struct position_s root_pos);

/**
 * @brief Query whether a background-pan drag is currently active
 *
 * @return @c true when a background-pan drag is in progress
 *
 * @note Complexity: @e O(1)
 */
bool drag_background_is_active(void);


#endif  /* ! INPUT_MOUSE_DRAG_BACKGROUND_H */
