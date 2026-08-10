/**
 * @file input/mouse/internal.h
 *
 * @brief Private cross-file declarations shared across the mouse
 *        input subsystem
 *
 * Splitting @c src/input/mouse/event.c into per-topic files (resize
 * cursors, hover polling, titlebar interaction, and the main
 * press/release/enter dispatch) still leaves three functions each
 * topic's own file exposes for at least one of the others to call
 * directly, since the underlying resize-cursor and hover state is
 * genuinely shared, not duplicated per file the way, say,
 * @c menu/dialog/confirm.c and @c menu/dialog/fortune.c each own
 * their own separate state.
 *
 * @note This header is private to the mouse input subsystem and must
 *       not be included outside of @c src/input/mouse/, for it is
 *       NOT part of the public API in @c input/mouse.h
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

/* Project includes */
#include <client.h>


/**
 * @brief Recompute and apply the resize-border cursor for a client
 *        window at a given pointer position
 *
 * Shared by @c mouse_handle_motion_hover and @c mouse_hover_poll_tick
 * (in hover.c) and @c mouse_handle_enter (in event.c), since any one
 * kind of event or poll can be the only signal a given transition
 * actually produces; see the implementation's own doc comment in
 * cursor.c for the full reasoning.
 *
 * @param connection XCB connection
 * @param surfaces   Every managed surface, to look up the client
 *                   @p window belongs to
 * @param window     Window the crossing, motion, or poll was
 *                   evaluated for
 * @param root_x     Pointer X position in root-window coordinates
 * @param root_y     Pointer Y position in root-window coordinates
 *
 * @return The resolved client @p window belongs to, or @c NULL if it
 *         does not belong to a resizable client
 *
 * @note Complexity: @e O(1)
 */
client_td *im_update_resize_cursor(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t window, int16_t root_x,
        int16_t root_y);

/**
 * @brief Start (or clear) hover-poll tracking of a window's resize
 *        cursor
 *
 * Called from @c mouse_handle_enter (in event.c) whenever the pointer
 * crosses into a window: an undecorated client has no separate frame
 * to fall back on, so moving from its border to its interior happens
 * entirely within one window, with no further @c EnterNotify for that
 * transition to catch; periodic polling (see @c mouse_hover_poll_tick
 * in hover.c) is the only way to still re-evaluate the cursor there.
 *
 * @param window Window to track, or @c XCB_WINDOW_NONE to stop
 *               tracking (the common case: most entered windows do
 *               not need this fallback at all)
 *
 * @note Complexity: @e O(1)
 */
void im_hover_track(xcb_window_t window);


#endif  /* ! INPUT_MOUSE_INTERNAL_H */
