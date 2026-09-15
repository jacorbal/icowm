/**
 * @file enact/surface.h
 *
 * @brief Every action this window manager can carry out on a whole
 *        surface, one typed function per action
 *
 * Split out of @c enact.h, alongside @c enact/client.h and
 * @c enact/desktop.h, so a file that only needs surface actions does
 * not also pull in, and rebuild against, every client and desktop
 * action declared alongside it.
 *
 * Each @a enact_surface_* function below is the single place in the
 * whole project where its corresponding action actually happens.
 * A caller anywhere else (a keybinding handler, a menu callback, an
 * EWMH message handler, a rule) calls the matching @a enact_surface_*
 * function directly, with its typed parameters, instead of reaching
 * into @c cmds/surface.h itself.  Searching for an action's enum name
 * always leads back to exactly one function here.
 *
 * @see @c action.h
 * @see @c enact.h
 *
 * @defgroup enact_surface Surface action execution
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef ENACT_SURFACE_H
#define ENACT_SURFACE_H


/* System includes */
#include <stdint.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Switch the surface to a specific desktop
 *
 * @param surface    Surface to switch
 * @param desktop_id Target desktop index
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_surface_desktop_switch(surface_td *surface,
        uint32_t desktop_id);

/**
 * @brief Switch the surface to the desktop north of the current
 *        one, in cyclic order
 *
 * @param surface Surface to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_surface_desktop_switch_north(surface_td *surface);

/**
 * @brief Switch the surface to the desktop south of the current
 *        one, in cyclic order
 *
 * @param surface Surface to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_surface_desktop_switch_south(surface_td *surface);

/**
 * @brief Switch the surface to the desktop east of the current
 *        one, in cyclic order
 *
 * @param surface Surface to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_surface_desktop_switch_east(surface_td *surface);

/**
 * @brief Switch the surface to the desktop west of the current
 *        one, in cyclic order
 *
 * @param surface Surface to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_surface_desktop_switch_west(surface_td *surface);

/**
 * @brief Move the surface's current desktop viewport a whole page
 *        north, clamped at the top of the pannable area
 *
 * The discrete counterpart to @a enact_surface_viewport_pan_north
 * below, which slides by @c viewport.pan-step pixels instead
 *
 * @param surface Surface whose viewport to move
 *
 * @note A no-op where there is no page north of the current one
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_surface_viewport_switch_north(surface_td *surface);

/**
 * @brief Move the surface's current desktop viewport a whole page
 *        south, clamped at the bottom of the pannable area
 *
 * @param surface Surface whose viewport to move
 *
 * @note A no-op where there is no page south of the current one
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_surface_viewport_switch_south(surface_td *surface);

/**
 * @brief Move the surface's current desktop viewport a whole page
 *        east, clamped at the right of the pannable area
 *
 * @param surface Surface whose viewport to move
 *
 * @note A no-op where there is no page east of the current one
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_surface_viewport_switch_east(surface_td *surface);

/**
 * @brief Move the surface's current desktop viewport a whole page
 *        west, clamped at the left of the pannable area
 *
 * @param surface Surface whose viewport to move
 *
 * @note A no-op where there is no page west of the current one
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_surface_viewport_switch_west(surface_td *surface);

/**
 * @brief Pan the surface's current desktop viewport one screen north,
 *        clamped at the top of the pannable area
 *
 * @param surface Surface to pan
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_surface_viewport_pan_north(surface_td *surface);

/**
 * @brief Pan the surface's current desktop viewport one screen south,
 *        clamped at the bottom of the pannable area
 *
 * @param surface Surface to pan
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_surface_viewport_pan_south(surface_td *surface);

/**
 * @brief Pan the surface's current desktop viewport one screen east,
 *        clamped at the right of the pannable area
 *
 * @param surface Surface to pan
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_surface_viewport_pan_east(surface_td *surface);

/**
 * @brief Pan the surface's current desktop viewport one screen west,
 *        clamped at the left of the pannable area
 *
 * @param surface Surface to pan
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_surface_viewport_pan_west(surface_td *surface);

/**
 * @brief Jump the surface's current desktop viewport straight to one
 *        of its configured pages, addressed by a single linear index
 *
 * @param surface Surface to reposition
 * @param page    Zero-based page index; see @a scmd_surface_
 *                viewport_goto (cmds/surface.h) for how it maps onto
 *                the configured viewport grid
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the current desktop
 */
void enact_surface_viewport_goto(surface_td *surface, uint32_t page);

/**
 * @brief Add a new, empty desktop to the end of the surface's
 *        desktop list
 *
 * @param surface Surface to add a desktop to
 *
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_add(surface_td *surface);

/**
 * @brief Remove the surface's last desktop, moving any client
 *        still on it to the new last desktop first
 *
 * A no-op, silently, when only one desktop remains: see
 * @a surface_action_desktop_remove (in @c surface.h) for the exact
 * refusal conditions.
 *
 * @param surface Surface to remove the last desktop from
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop being removed
 */
void enact_surface_desktop_remove(surface_td *surface);

/**
 * @brief Toggle whether panel/tray struts are set aside when computing
 *        this surface's desktops' work areas
 *
 * @param surface Surface to toggle strutless-maximization mode on
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       @p surface
 */
void enact_surface_toggle_strutless_maximize(surface_td *surface);


#endif  /* ! ENACT_SURFACE_H */
