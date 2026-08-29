/**
 * @file cmds/client/workarea.h
 *
 * @brief Client-relative workarea resolution declarations
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

#ifndef CMDS_CCMD_WORKAREA_H
#define CMDS_CCMD_WORKAREA_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Resolve the on-screen workarea for whichever monitor a
 *        client currently sits on
 *
 * Resolves @p client's surface and desktop from the global @c wm
 * singleton, then clips the desktop's workarea (already adjusted for
 * panel/dock struts) down to whichever physical monitor @p client's
 * own center point currently falls on.  A client pinned to every
 * desktop uses its surface's currently shown desktop instead, since
 * it has no single desktop of its own.
 *
 * Used by both positioning (centering, moving to a corner) and
 * maximize/fullscreen sizing: the resolved rectangle is identical
 * either way, only what each caller does with it differs.
 *
 * @param client Client to resolve the workarea for
 * @param out_x  Receives the workarea's own left edge (may be @c NULL)
 * @param out_y  Receives the workarea's own top edge (may be @c NULL)
 * @param out_w  Receives the workarea's own width
 * @param out_h  Receives the workarea's own height
 *
 * @return @c true on success, @c false if any part of the lookup
 *         fails (surface not found, desktop not found, no workarea
 *         known yet, or the clipped area is empty); callers fall
 *         back to @a ccmd_screen_dim's raw screen size in that case
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
bool ccmd_client_resolve_workarea(client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y,
        uint16_t *restrict out_w, uint16_t *restrict out_h);


#endif  /* ! CMDS_CCMD_WORKAREA_H */
