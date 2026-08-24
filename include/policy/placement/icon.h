/**
 * @file policy/placement/icon.h
 *
 * @brief Icon placement policy declarations
 *
 * Computes an iconified client's own icon-window position, and pushes
 * an already-proposed icon position away from the systray's own
 * current rectangle, if the two would overlap there.
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

#ifndef POLICY_PLACEMENT_ICON_H
#define POLICY_PLACEMENT_ICON_H

/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>

/* Project includes */
#include <config.h>



/**
 * @brief Compute the icon window position for a newly iconified client
 *
 * Chooses an X/Y coordinate for @p client's icon window according to
 * @p policy, the current screen dimensions, and the positions of
 * already-placed icon windows on @p desktop.
 *
 * @param client     Pointer to the client being iconified (must not be
 *                   null)
 * @param desktop    Desktop to inspect for existing icon positions;
 *                   may be null (treated as empty desktop)
 * @param policy     Icon placement policy from configuration
 * @param icon_dim   Icon window's own width/height, in pixels
 * @param screen_dim Screen dimensions, in pixels
 * @param out_pos    Output X/Y coordinate
 *
 * @note Complexity: @e O(n), where @e n is the number of iconified
 *       clients already placed on @p desktop
 */
void place_icon_apply(const client_td *client, desktop_td *desktop,
        enum config_icon_placement_e policy,
        struct dimensions_s icon_dim,
        struct dimensions_s screen_dim,
        struct position_s *restrict out_pos);


/**
 * @brief Push an icon's own proposed position away from the systray's
 *        current rectangle, if the two would overlap there
 *
 * Direction-aware, unlike always pushing toward one fixed edge: pushes
 * below the tray's own bottom edge when the tray sits in the upper half
 * of @p workarea, or above its own top edge when the tray sits in the
 * lower half, so the icon is never pushed toward whichever edge the
 * tray already occupies (which, near a screen edge, could otherwise
 * push the icon straight off the visible workarea entirely, pushing
 * "further down" would leave the icon below the workarea's own bottom
 * edge, off-screen or inside a reserved margin, rather than clear of
 * the tray at all).
 *
 * A small fixed gap (@c WM_ICON_SYSTRAY_GAP, @c defs/icon.h) is left
 * between the two either way, so the icon does not end up sitting flush
 * against the tray's own edge.  The result is then clamped to stay
 * fully within @p workarea's own vertical bounds regardless, in case
 * the tray's own height leaves less room than the icon and its gap
 * together need.
 *
 * @param io_x     Icon's proposed X position; read but never adjusted
 *                 by this function (the tray's own width is not
 *                 currently used to also push horizontally).  Kept as
 *                 its own separate, @c const-qualified parameter
 *                 rather than folded into a @c struct position_s
 *                 alongside @p io_y, so this read-only guarantee stays
 *                 compiler-enforced rather than merely documented
 * @param io_y     Icon's proposed Y position; read, and overwritten
 *                 with the adjusted position if pushed
 * @param icon_dim Icon's own width/height, in pixels
 * @param tray     Tray's own current rectangle
 * @param workarea Desktop's own current work area; a @c NULL skips the
 *                 final clamp and assumes the tray sits in the upper
 *                 half, same as an unknown workarea would in practice
 *                 always place it
 *
 * @return Status of the operation
 * @retval  true if @p io_y was adjusted (the icon did overlap the
 *               tray's own rectangle at its proposed position)
 * @retval false if left untouched
 *
 * @note Complexity: @e O(1)
 */
bool place_icon_avoid_systray_overlap(
        const int16_t *restrict io_x, int16_t *restrict io_y,
        struct dimensions_s icon_dim,
        struct geometry_s tray,
        const struct geometry_s *workarea);


#endif  /* ! POLICY_PLACEMENT_ICON_H */
