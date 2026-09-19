/**
 * @file stage/client.h
 *
 * @brief Bulk client operations across a stage's desktops: hide,
 *        show, transfer pinned clients, and reflow stray ones
 *
 * Split out of @c stage.h, alongside its sibling @c stage
 * headers, so a file that only needs one of these bulk operations
 * does not also pull in, and rebuild against, every other unrelated
 * stage concern (desktop, viewport, monitor, workarea, action)
 * declared in the same file.
 *
 * @see @p stage_s, in @c stage.h
 *
 * @defgroup stage_client Stage bulk client operations
 * @ingroup stage
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef STAGE_CLIENT_H
#define STAGE_CLIENT_H


/* System includes */
#include <stdint.h>

/* Project includes */
#include <stage.h>


/**
 * @brief Unmap all non-pinned client windows belonging to a desktop
 *
 * Iterates the stacking list of the specified desktop and calls
 * @a xcb_unmap_window for each client that does not have the
 * @c CLIENT_FLAG_PIN flag set.  Used when switching away from a desktop
 * to hide its windows.
 *
 * @param stage      Pointer to the stage that owns the desktop
 * @param desktop_id ID of the desktop whose clients should be hidden
 *
 * @note Complexity: @e O(n), where @e n is the number of stacked
 *       clients on the desktop
 */
void stage_client_hide_all(stage_td *stage, uint32_t desktop_id);

/**
 * @brief Map all visible client windows belonging to a desktop
 *
 * Iterates the stacking list of the specified desktop and calls
 * @a xcb_map_window for each client that is neither hidden
 * (@c CLIENT_FLAG_HIDDEN) nor iconified (@c CLIENT_STATE_ICONIFIED).
 * Used when switching to a desktop to reveal its windows.
 *
 * @param stage      Pointer to the stage that owns the desktop
 * @param desktop_id ID of the desktop whose clients should be shown
 *
 * @note Complexity: @e O(n), where @e n is the number of stacked
 *       clients on the desktop
 */
void stage_client_show_all(stage_td *stage, uint32_t desktop_id);

/**
 * @brief Map a single client the way showing its whole desktop would
 *
 * For a client that arrives on the desktop already shown, which the
 * next @a stage_client_show_all would otherwise be the first to map.
 * Leaves focus and the rest of the desktop alone.
 *
 * @param stage  Stage the client is on
 * @param client Client to map
 *
 * @note A no-op for a hidden or iconified client
 * @note Complexity: @e O(1)
 */
void stage_client_show_one(stage_td *stage, client_td *client);

/**
 * @brief Move all pinned clients from every other desktop to @p to_id
 *
 * Iterates all desktops on the stage and relocates any client that
 * carries the @c CLIENT_FLAG_PIN flag to the desktop identified by
 * @p to_id.  Called during desktop switches so that pinned windows are
 * present in the new desktop's stacking list and therefore respond to
 * keyboard shortcuts and focus management on the destination desktop.
 *
 * @param stage Pointer to the stage that owns all desktops
 * @param to_id ID of the desktop to which pinned clients are moved
 *
 * @note Complexity: @e O(d * n), where @e d is the number of desktops
 *       and @e n is the average number of clients per desktop
 */
void stage_client_pinned_transfer_all(stage_td *stage,
        uint32_t to_id);

/**
 * @brief Reposition clients left with no overlap against any known
 *        monitor
 *
 * Iterates all desktops and their stacking lists.  A client whose frame
 * (or client window when undecorated) still overlaps at least one of
 * @p stage's monitors (see @p stage->monitors) is left untouched,
 * even if it is not fully contained within a single one.  A window
 * legitimately spanning two adjacent monitors must not be "corrected"
 * just for straddling their seam.  Only a client with no overlap
 * against any current monitor at all (typically because the one it used
 * to be on was disconnected, or a RandR layout change left it in a gap)
 * is moved, clamped into whichever monitor @a stage_monitor_for_point
 * resolves for its center point so that at least a minimum strip of the
 * window remains visible there.  Both the X server geometry and the
 * cached @p client->layout.geometry.cur are updated.
 *
 * @param stage Pointer to the stage whose clients will be reflowed
 *
 * @note Complexity: @e O(d * n * m), where @e d is the number of
 *       desktops, @e n is the average number of clients per desktop,
 *       and @e m is @p stage->monitor_count
 */
void stage_client_reflow_all(stage_td *stage);


#endif /* STAGE_CLIENT_H */
