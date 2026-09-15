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
 * Four questions are settled before the configured policy is consulted
 * at all, in this order: whether the window is a splash screen, whether
 * it asked for a position itself, whether it is a transient to be
 * centered over its parent, and whether another window of the same
 * application is already on screen for it to join.  Any one of them
 * answering settles the matter, the earlier taking precedence over the
 * later.
 *
 * Only a window none of them claimed reaches the policy itself, one of
 * @c smart, @c cascade, @c centered, @c under-mouse or @c manual.  One
 * that finds nowhere to put it falls back on the cascade, and a policy
 * this file does not recognize leaves the window where the X server
 * put it.
 *
 * @param wm      Window manager instance
 * @param surface Surface that will host the client
 * @param client  Client to place
 *
 * @note Complexity: @e O(g * n) under @c smart and @c manual, and
 *       @e O(n) otherwise, where @e g is the number of grid positions
 *       tested and @e n is the number of clients on the desktop, the
 *       latter being what the search for an application sibling costs
 *       whichever policy follows it
 */
void place_window_apply(const wm_td *wm,
        surface_td *surface, client_td *client);

/**
 * @brief Place the client following the cascade policy, unconditionally
 *
 * Ignores @c windows.placement.policy entirely and steps the client one
 * pace on from wherever this policy last placed one, whichever policy
 * is actually configured.  Meant for callers wanting a predictable,
 * non-overlapping spread across several clients in a row (see
 * @a enact_desktop_client_rearrange_all), and not for placing a single
 * newly mapped client, which @a place_window_apply is for.
 *
 * @param wm      Window manager instance
 * @param surface Surface the client lives on
 * @param client  Client to place
 *
 * @note Complexity: @e O(1)
 */
void place_window_apply_cascade(const wm_td *wm,
        surface_td *surface, client_td *client);


#endif  /* ! POLICY_PLACEMENT_WINDOW_H */
