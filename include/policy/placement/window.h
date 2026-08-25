/**
 * @file policy/placement/window.h
 *
 * @brief Window placement policy declarations
 *
 * Smart-placement search and the policy dispatcher used when a new
 * client is mapped.  Both functions receive the window manager context
 * explicitly so they remain independent compilation units.
 *
 * @ingroup policy
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_PLACEMENT_WINDOW_H
#define POLICY_PLACEMENT_WINDOW_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Apply the configured placement policy to a newly mapped client
 *
 * Selects and applies the placement algorithm configured in @p wm:
 * @c smart, @c cascade, @c centered, or @c under-mouse.
 *
 * @param wm      Pointer to the window manager singleton
 * @param surface Pointer to the surface that will host the client
 * @param client  Pointer to the client to place
 *
 * @note Complexity: @e O(g * n) in the smart case, @e O(1) otherwise,
 *       where @e g is the number of grid positions tested and @e n is
 *       the number of clients on the desktop
 */
void place_window_apply(const wm_td *wm,
        surface_td *surface, client_td *client);

/**
 * @brief Place the client following the cascade policy, unconditionally
 *
 * Ignores @p windows.placement.policy entirely and always steps the
 * client to the next cascade slot, regardless of which policy is
 * actually configured.  Meant for callers that need a predictable,
 * non-overlapping spread across several clients in a row (see
 * @a enact_desktop_clients_rearrange), not for placing a single newly
 * mapped client, which should call @a place_window_apply instead
 *
 * @param wm      Window manager instance
 * @param surface Surface the client lives on
 * @param client  Client to place
 *
 * @note Complexity: @e O(1)
 */
void place_window_apply_cascade(const wm_td *wm,
        surface_td *surface, client_td *client);

/**
 * @brief Find a non-overlapping smart position for a newly mapped
 *        client
 *
 * Searches the current desktop from top-left to bottom-right using
 * a fixed grid step and returns the first position whose rectangle does
 * not overlap any currently visible client.
 *
 * @param wm      Pointer to the window manager singleton
 * @param surface Pointer to the surface where the client will appear
 * @param client  Pointer to the client being placed
 * @param out_x   Output pointer for the selected X coordinate
 * @param out_y   Output pointer for the selected Y coordinate
 *
 * @return @c true if a free position was found, @c false otherwise
 *
 * @note Complexity: @e O(g * n), where @e g is the number of grid
 *       positions tested and @e n is the number of clients on the
 *       current desktop
 */
bool place_window_smart(const wm_td *wm,
        surface_td *surface, client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y);


#endif  /* ! POLICY_PLACEMENT_WINDOW_H */
