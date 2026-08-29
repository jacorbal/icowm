/**
 * @file policy/placement/smart.h
 *
 * @brief Smart placement: the search for the least-covered spot
 *
 * Walks the positions a client could take and keeps the one
 * overlapping least of what is already on screen, which is what
 * 'smart' means here.  The arithmetic it walks with lives in
 * @c policy/placement/rect.h, and the area it walks over comes from
 * @c policy/placement/monitor.h.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_PLACEMENT_SMART_H
#define POLICY_PLACEMENT_SMART_H

/* System includes */
#include <stdint.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>

/* Local includes */
#include <defs/placement.h>


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
bool place_window_smart(const wm_td *wm,
        surface_td *surface, client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y);


#endif  /* ! POLICY_PLACEMENT_SMART_H */
