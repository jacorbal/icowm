/**
 * @file place.h
 *
 * @brief Window placement policy declarations
 *
 * Smart-placement search and the policy dispatcher used when a new
 * client is mapped.  Both functions receive the window manager context
 * explicitly so they remain independent compilation units.
 *
 * @ingroup place Placement
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef PLACE_H
#define PLACE_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Project includes */
#include <client.h>
#include <surface.h>
#include <wm.h>


/* Public interface */
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
bool place_smart(wm_td *wm, surface_td *surface, client_td *client,
        int32_t *out_x, int32_t *out_y);

/**
 * @brief Apply the configured placement policy to a newly mapped client
 *
 * Selects and applies the placement algorithm configured in @p wm:
 * @c smart, @c cascade, @c centered, or @c under-mouse.  A fixed
 * override flag in the configuration causes centered placement
 * regardless of the policy.
 *
 * @param wm      Pointer to the window manager singleton
 * @param surface Pointer to the surface that will host the client
 * @param client  Pointer to the client to place
 *
 * @note Complexity: @e O(g * n) in the smart case, @e O(1) otherwise,
 *       where @e g is the number of grid positions tested and @e n is
 *       the number of clients on the desktop
 */
void place_apply(wm_td *wm, surface_td *surface, client_td *client);


#endif  /* ! PLACE_H */
