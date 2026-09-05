/**
 * @file cmds/surface.h
 *
 * @brief Declaration of actions related to screen surface management
 *
 * @ingroup cmds
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_SCMD_H
#define CMDS_SCMD_H


/* Project includes */
#include <surface.h>


/* Public interface */
/**
 * @brief Switch the current view to a specified desktop
 *
 * @param surface    Pointer to the surface
 * @param desktop_id Target desktop index
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch(surface_td *surface,
        uint32_t desktop_id);

/**
 * @brief Switch the current view to the desktop north of the current
 *        one
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_north(surface_td *surface);

/**
 * @brief Switch the current view to the desktop south of the current
 *        one
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_south(surface_td *surface);

/**
 * @brief Switch the current view to the desktop east of the current
 *        one
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_east(surface_td *surface);

/**
 * @brief Switch the current view to the desktop west of the current
 *        one
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_west(surface_td *surface);

/**
 * @brief Whether the current desktop's viewport still has room to pan
 *        one more screen toward @p direction
 *
 * Shared by @c input/mouse/drag/warp.h (to defer an edge-triggered
 * desktop warp while a pan is still possible instead) and
 * @c input/mouse/drag/pan.h (to decide whether an edge-triggered pan
 * itself is), so neither has to duplicate the clamp math
 * @a scmd_surface_viewport_pan_north and its three siblings already
 * apply.
 *
 * @param surface   Pointer to the surface
 * @param direction Compass direction to check
 *
 * @return Whether panning one more screen toward @p direction would
 *         actually move the viewport
 *
 * @note Complexity: @e O(1)
 */
bool scmd_surface_viewport_pan_available(surface_td *surface,
        enum compass_direction_e direction);

/**
 * @brief Pan the current desktop's viewport one screen north, clamped
 *        at the top of the pannable area
 *
 * A no-op whenever the current desktop's configured 'viewport' is
 * only one screen tall, or the viewport already sits at its northmost
 * origin: unlike @a scmd_surface_desktop_switch_north, this never
 * wraps around and never changes which desktop is current, only where
 * within it the screen is looking.
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_pan_north(surface_td *surface);

/**
 * @brief Pan the current desktop's viewport one screen south, clamped
 *        at the bottom of the pannable area
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_pan_south(surface_td *surface);

/**
 * @brief Pan the current desktop's viewport one screen east, clamped
 *        at the right of the pannable area
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_pan_east(surface_td *surface);

/**
 * @brief Pan the current desktop's viewport one screen west, clamped
 *        at the left of the pannable area
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_pan_west(surface_td *surface);

/**
 * @brief Move the current desktop's viewport straight to an absolute
 *        origin, clamped to the pannable area
 *
 * The @c _NET_DESKTOP_VIEWPORT counterpart to
 * @a scmd_surface_viewport_pan_north and its three siblings: those
 * move by exactly one screen in a compass direction, while this jumps
 * straight to whatever @p x, @p y a pager or other external EWMH
 * client asked for, still translating every non-sticky client by the
 * resulting delta the same way.
 *
 * @param surface Pointer to the surface
 * @param x       Requested viewport origin's X coordinate, in pixels
 * @param y       Requested viewport origin's Y coordinate, in pixels
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_set(surface_td *surface, int32_t x, int32_t y);

/**
 * @brief Jump the current desktop's viewport straight to one of its
 *        configured pages, addressed by a single linear index rather
 *        than an X/Y origin
 *
 * A page's own origin is derived the same way @a scmd_surface_
 * viewport_pan_east and its siblings already derive each one-screen
 * step: @p page's row is its index divided by the configured column
 * count, its column the remainder, each then multiplied by the
 * desktop's own screen-sized dimensions to land on that page's
 * top-left pixel, handed to @a scmd_surface_viewport_set exactly as
 * a pager's absolute request would be.  Out of the configured
 * @c columns * @c rows range, @p page is refused outright rather than
 * clamped into range: unlike a pan or a pager's arbitrary pixel
 * origin, a page index has no meaningful nearest neighbor to fall
 * back to once it no longer names any real page at all.
 *
 * @param surface Pointer to the surface
 * @param page    Zero-based page index, in row-major order across the
 *                configured viewport grid (§2.2's @c columns first,
 *                then @c rows)
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void scmd_surface_viewport_goto(surface_td *surface, uint32_t page);


#endif  /* ! CMDS_SCMD_H */
