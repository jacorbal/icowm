/**
 * @file policy/placement.h
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

#ifndef POLICY_PLACEMENT_H
#define POLICY_PLACEMENT_H


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
bool place_smart(const wm_td *wm,
        surface_td *surface, client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y);

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
void place_apply(const wm_td *wm,
        surface_td *surface, client_td *client);

/**
 * @brief Place the client following the cascade policy, unconditionally
 *
 * Ignores @p windows.placement.policy entirely and always steps the
 * client to the next cascade slot, regardless of which policy is
 * actually configured.  Meant for callers that need a predictable,
 * non-overlapping spread across several clients in a row (see
 * @a enact_desktop_clients_rearrange), not for placing a single newly
 * mapped client, which should call @a place_apply instead
 *
 * @param wm      Window manager instance
 * @param surface Surface the client lives on
 * @param client  Client to place
 *
 * @note Complexity: @e O(1)
 */
void place_apply_cascade(const wm_td *wm,
        surface_td *surface, client_td *client);

/**
 * @brief Compute the icon window position for a newly iconified client
 *
 * Chooses an X/Y coordinate for @p client's icon window according to
 * @p policy, the current screen dimensions, and the positions of
 * already-placed icon windows on @p desktop.
 *
 * @param client   Pointer to the client being iconified (must not be null)
 * @param desktop  Desktop to inspect for existing icon positions;
 *                 may be null (treated as empty desktop)
 * @param policy   Icon placement policy from configuration
 * @param icon_w   Width of the icon window in pixels
 * @param icon_h   Height of the icon window in pixels
 * @param screen_w Screen width in pixels
 * @param screen_h Screen height in pixels
 * @param out_x    Output X coordinate
 * @param out_y    Output Y coordinate
 *
 * @note Complexity: @e O(n), where @e n is the number of iconified
 *       clients already placed on @p desktop
 */
void place_icon(const client_td *client, desktop_td *desktop,
        enum config_icon_placement_e policy,
        uint16_t icon_w, uint16_t icon_h,
        uint16_t screen_w, uint16_t screen_h,
        int16_t *restrict out_x, int16_t *restrict out_y);

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
 *                 currently used to also push horizontally)
 * @param io_y     Icon's proposed Y position; read, and overwritten
 *                 with the adjusted position if pushed
 * @param icon_w   Icon width, in pixels
 * @param icon_h   Icon height, in pixels
 * @param tray_x   Tray's own current rectangle
 * @param tray_y   See @p tray_x
 * @param tray_w   Also see @p tray_x
 * @param tray_h   Told you to see @p tray_x
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
bool icon_avoid_systray_overlap(const int16_t *restrict io_x,
        int16_t *restrict io_y,
        uint16_t icon_w, uint16_t icon_h,
        int32_t tray_x, int32_t tray_y, uint16_t tray_w, uint16_t tray_h,
        const struct geometry_s *workarea);


#endif  /* ! POLICY_PLACEMENT_H */
