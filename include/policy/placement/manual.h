/**
 * @file policy/placement/manual.h
 *
 * @brief Manual placement: the position the person picks themselves
 *
 * The oldest placement policy there is, and the one every other exists
 * to avoid: rather than the window manager choosing a spot, the person
 * is shown an outline and puts the window where they want it.
 *
 * @defgroup placementmanual Manual placement
 * @ingroup policy
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_PLACEMENT_MANUAL_H
#define POLICY_PLACEMENT_MANUAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Type includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Pick where a window goes, asking the person to point at it
 *
 * @param wm      Window manager instance
 * @param surface Surface the window will appear on
 * @param client  Client being placed
 * @param out_x   Where the chosen X coordinate is written
 * @param out_y   Where the chosen Y coordinate is written
 *
 * @return @c true when a position was chosen
 *
 * @note Answers @c false for now, the interactive part not being
 *       written yet, which leaves the caller on its fallback and makes
 *       this policy behave as the smart one until it is
 * @note Complexity: @e O(1)
 */
bool place_window_manual(const wm_td *wm, surface_td *surface,
        client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y);


#endif  /* ! POLICY_PLACEMENT_MANUAL_H */
