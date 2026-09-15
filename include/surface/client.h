/**
 * @file surface/client.h
 *
 * @brief Bulk client operations across a surface's desktops: hide,
 *        show, transfer pinned clients, and reflow stray ones
 *
 * Split out of @c surface.h, alongside its sibling @c surface
 * headers, so a file that only needs one of these bulk operations
 * does not also pull in, and rebuild against, every other unrelated
 * surface concern (desktop, viewport, monitor, workarea, action)
 * declared in the same file.
 *
 * @see @p surface_s, in @c surface.h
 *
 * @defgroup surface_client Surface bulk client operations
 * @ingroup surface
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SURFACE_CLIENT_H
#define SURFACE_CLIENT_H


/* System includes */
#include <stdint.h>

/* Project includes */
#include <surface.h>


/**
 * @brief Unmap all non-pinned client windows belonging to a desktop
 *
 * Iterates the stacking list of the specified desktop and calls
 * @a xcb_unmap_window for each client that does not have the
 * @c CLIENT_FLAG_PIN flag set.  Used when switching away from a desktop
 * to hide its windows.
 *
 * @param surface    Pointer to the surface that owns the desktop
 * @param desktop_id ID of the desktop whose clients should be hidden
 *
 * @note Complexity: @e O(n), where @e n is the number of stacked
 *       clients on the desktop
 */
void surface_client_hide_all(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Map all visible client windows belonging to a desktop
 *
 * Iterates the stacking list of the specified desktop and calls
 * @a xcb_map_window for each client that is neither hidden
 * (@c CLIENT_FLAG_HIDDEN) nor iconified (@c CLIENT_STATE_ICONIFIED).
 * Used when switching to a desktop to reveal its windows.
 *
 * @param surface    Pointer to the surface that owns the desktop
 * @param desktop_id ID of the desktop whose clients should be shown
 *
 * @note Complexity: @e O(n), where @e n is the number of stacked
 *       clients on the desktop
 */
void surface_client_show_all(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Move all pinned clients from every other desktop to @p to_id
 *
 * Iterates all desktops on the surface and relocates any client that
 * carries the @c CLIENT_FLAG_PIN flag to the desktop identified by
 * @p to_id.  Called during desktop switches so that pinned windows are
 * present in the new desktop's stacking list and therefore respond to
 * keyboard shortcuts and focus management on the destination desktop.
 *
 * @param surface Pointer to the surface that owns all desktops
 * @param to_id   ID of the desktop to which pinned clients are moved
 *
 * @note Complexity: @e O(d * n), where @e d is the number of desktops
 *       and @e n is the average number of clients per desktop
 */
void surface_client_pinned_transfer_all(surface_td *surface,
        uint32_t to_id);

/**
 * @brief Reposition clients left with no overlap against any known
 *        monitor
 *
 * Iterates all desktops and their stacking lists.  A client whose frame
 * (or client window when undecorated) still overlaps at least one of
 * @p surface's monitors (see @p surface->monitors) is left untouched,
 * even if it is not fully contained within a single one.  A window
 * legitimately spanning two adjacent monitors must not be "corrected"
 * just for straddling their seam.  Only a client with no overlap
 * against any current monitor at all (typically because the one it used
 * to be on was disconnected, or a RandR layout change left it in a gap)
 * is moved, clamped into whichever monitor @a surface_monitor_for_point
 * resolves for its center point so that at least a minimum strip of the
 * window remains visible there.  Both the X server geometry and the
 * cached @p client->layout.geometry.cur are updated.
 *
 * @param surface Pointer to the surface whose clients will be reflowed
 *
 * @note Complexity: @e O(d * n * m), where @e d is the number of
 *       desktops, @e n is the average number of clients per desktop,
 *       and @e m is @p surface->monitor_count
 */
void surface_client_reflow_all(surface_td *surface);


#endif /* SURFACE_CLIENT_H */
