/**
 * @file input/mouse/drag/outline.h
 *
 * @brief Outline stand-in windows used by a non-solid drag
 *
 * Declares the functions @c drag/outline.c exposes for @c drag.c and
 * @c drag/warp.c to call directly; nothing outside the drag subsystem
 * calls any of these.
 *
 * @note This header is private to @c input/mouse/drag/ (and
 *       @c input/mouse/drag.c, which orchestrates every drag/ file) and
 *       must not be included outside of them
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MOUSE_DRAG_OUTLINE_H
#define INPUT_MOUSE_DRAG_OUTLINE_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Types includes */
#include <types/pair.h>


/**
 * @brief Begin an outline-mode drag: create and map the initial 4
 *        strip windows
 *
 * @param connection X connection
 * @param geom       Initial rectangle, in root coordinates
 *
 * @note No-op if @p connection is null
 * @note Complexity: @e O(1)
 */
void drag_outline_start(xcb_connection_t *connection,
        struct geometry_s geom);

/**
 * @brief Move the outline stand-in's 4 strip windows to a new
 *        rectangle
 *
 * @param connection X connection
 * @param geom       New rectangle, in root coordinates
 *
 * @note No-op if @p connection is null
 * @note Complexity: @e O(1)
 */
void drag_outline_move(xcb_connection_t *connection,
        struct geometry_s geom);

/**
 * @brief End an outline-mode drag: destroy the 4 strip windows
 *
 * @param connection X connection
 *
 * @note No-op if @p connection is null, or no outline drag is active
 * @note Complexity: @e O(1)
 */
void drag_outline_end(xcb_connection_t *connection);


#endif  /* ! INPUT_MOUSE_DRAG_OUTLINE_H */
