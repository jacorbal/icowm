/**
 * @file render/outline.h
 *
 * @brief Outline stand-in windows, shared by any caller needing to show
 *        a rectangle around a target without touching the target's
 *        geometry
 *
 * The strip-window mechanism itself, with no drag-specific state of
 * its.  A caller owns its 4-element @c xcb_window_t array (initialized
 * to @c XCB_WINDOW_NONE before first use) and passes it to every call
 * below; this file itself keeps none of that state.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_OUTLINE_H
#define RENDER_OUTLINE_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Types includes */
#include <types/pair.h>


/**
 * @brief Create and map the 4 strip windows outlining a rectangle
 *
 * Each strip sits just inside @p geom's edge (top, bottom, left,
 * right, in that fixed order), so the outline never extends beyond
 * @p geom itself, and never touches any real client's geometry:
 * these are entirely separate, @c override_redirect windows layered
 * on top.
 *
 * @param connection   X connection
 * @param root         Root window the 4 strips are created as
 *                     children of
 * @param geom         Rectangle to outline, in root coordinates
 * @param border_width Thickness of each strip, in pixels
 * @param color        Fill color for all 4 strips
 * @param stack_below  A window every strip is kept stacked below
 *                     (never covering it), or @c XCB_WINDOW_NONE for
 *                     no such constraint
 * @param windows      Caller-owned 4-element array, initialized to
 *                     @c XCB_WINDOW_NONE before this call; filled in
 *                     with the 4 new window IDs
 *
 * @note No-op if @p connection is null
 * @note Complexity: @e O(1)
 */
void render_outline_show(xcb_connection_t *connection, xcb_window_t root,
        struct geometry_s geom, uint32_t border_width, uint32_t color,
        xcb_window_t stack_below, xcb_window_t windows[4]);

/**
 * @brief Move the 4 strip windows to outline a new rectangle
 *
 * @param connection   X connection
 * @param geom         New rectangle to outline, in root coordinates
 * @param border_width Thickness of each strip, in pixels
 * @param stack_below  A window every strip is kept stacked below (never
 *                     covering it), or @c XCB_WINDOW_NONE for no such
 *                     constraint
 * @param windows      The same 4-element array @a render_outline_show
 *                     filled in; entries still @c XCB_WINDOW_NONE are
 *                     skipped
 *
 * @note No-op if @p connection is null
 * @note Complexity: @e O(1)
 */
void render_outline_move(xcb_connection_t *connection,
        struct geometry_s geom, uint32_t border_width,
        xcb_window_t stack_below, xcb_window_t windows[4]);

/**
 * @brief Destroy the 4 strip windows and reset @p windows to
 *        @c XCB_WINDOW_NONE
 *
 * @param connection X connection
 * @param windows    The same 4-element array @a render_outline_show
 *                   filled in
 *
 * @note No-op if @p connection is null, or @p windows' first entry is
 *       already @c XCB_WINDOW_NONE
 * @note Complexity: @e O(1)
 */
void render_outline_hide(xcb_connection_t *connection,
        xcb_window_t windows[4]);


#endif  /* ! RENDER_OUTLINE_H */
