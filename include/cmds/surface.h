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


#endif  /* ! CMDS_SCMD_H */
