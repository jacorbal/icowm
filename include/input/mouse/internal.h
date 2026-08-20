/**
 * @file input/mouse/internal.h
 *
 * @brief Private cross-file declarations shared between the resize-
 *        cursor and hover-poll topic files
 *
 * Splitting the mouse input subsystem into per-topic files (resize
 * cursors in @c cursor.c, hover polling in @c hover.c, and the press/
 * release/enter dispatch, itself further split into
 * @c input/mouse/event/press.c, @c event/release.c, and
 * @c event/enter.c) still leaves the two functions declared below,
 * each one topic's own file exposes for the other (and for
 * @c event/enter.c) to call directly, since the underlying resize-
 * cursor and hover state is genuinely shared, not duplicated per file
 * the way, say, @c menu/dialog/confirm.c and @c menu/dialog/
 * fortune.c each own their own separate state.
 *
 * The drag subsystem used to share this same header, before it grew
 * its own dedicated one per drag/ file (@c drag/overlay.h,
 * @c drag/snap.h, @c drag/outline.h, @c drag/warp.h) plus
 * @c drag/internal.h for its own singleton state alone: nothing under
 * @c input/mouse/event/, @c cursor.c, or @c hover.c ever actually
 * called into any of it, so keeping it here served no purpose beyond
 * a second, silently drifting copy of declarations @c drag/
 * internal.h already had right.
 *
 * @note This header is private to the mouse input subsystem and must
 *       not be included outside of @c src/input/mouse/, for it is
 *       NOT part of the public API in @c input/mouse/bind.h,
 *       @c input/mouse/event.h, @c input/mouse/hover.h, or
 *       @c input/mouse/cursor.h
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_INTERNAL_H
#define INPUT_MOUSE_INTERNAL_H


/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>


/**
 * @brief Recompute and apply the resize-border cursor for a client
 *        window at a given pointer position
 *
 * Shared by @a mouse_handle_motion_hover and @a mouse_hover_poll_tick
 * (in @c hover.c) and @a mouse_handle_enter (in @c event/enter.c),
 * since any one kind of event or poll can be the only signal a given
 * transition actually produces
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
client_td *im_update_resize_cursor(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t window,
        struct position_s root_pos);

/**
 * @brief Start (or clear) hover-poll tracking of a window's resize
 *        cursor
 *
 * Called from @a mouse_handle_enter (in @c event/enter.c) whenever
 * the
 * pointer crosses into a window: an undecorated client has no separate
 * frame to fall back on, so moving from its border to its interior
 * happens entirely within one window, with no further @c EnterNotify
 * for that transition to catch; periodic polling is the only way to
 * still re-evaluate the cursor there.
 *
 * @param window Window to track, or @c XCB_WINDOW_NONE to stop
 *               tracking (the common case: most entered windows do
 *               not need this fallback at all)
 *
 * @note Complexity: @e O(1)
 *
 * @see @a mouse_hover_poll_tick in @c hover.c
 */
void im_hover_track(xcb_window_t window);


#endif  /* ! INPUT_MOUSE_INTERNAL_H */
