/**
 * @file cmds/client/workarea.c
 *
 * @brief Client-relative workarea resolution
 *
 * One of the files @c cmds/client/ is made of.  Its single function
 * today, kept separate from @c move.c/@c maximize.c regardless: shared
 * almost evenly by positioning (centering, moving to a corner) and
 * maximize/fullscreen sizing alike, it belongs to neither one on its
 * own.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <utils/geom.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/screen.h>
#include <cmds/client/workarea.h>


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
 * @param out_x  Receives the workarea's left edge (may be @c NULL)
 * @param out_y  Receives the workarea's top edge (may be @c NULL)
 * @param out_w  Receives the workarea's width
 * @param out_h  Receives the workarea's height
 *
 * @return @c true on success, @c false if any part of the lookup
 *         fails (surface not found, desktop not found, no workarea
 *         known yet, or the clipped area is empty); callers fall
 *         back to @c ccmd_screen_dim's raw screen size in that case
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
bool ccmd_client_resolve_workarea(client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y,
        uint16_t *restrict out_w, uint16_t *restrict out_h)
{
    surface_td *surface = NULL;
    const desktop_td *desktop;
    monitor_td monitor;
    struct geometry_s clipped;
    uint32_t did;

    if (client == NULL || out_w == NULL || out_h == NULL) {
        return false;
    }

    if (!ccmd_client_monitor(client, &surface, &monitor)) {
        return false;
    }

    did = (client->desktop_id == WM_DESKTOP_ID_ALL)
        ? surface->desktop_cur
        : client->desktop_id;
    desktop = surface_desktop_get(surface, did);
    if (desktop == NULL || desktop->workarea.dim.w == 0u ||
            desktop->workarea.dim.h == 0u) {
        return false;
    }

    clipped = geom_intersect_rect(
            desktop->workarea.pos.x, desktop->workarea.pos.y,
            desktop->workarea.dim.w, desktop->workarea.dim.h,
            monitor.x, monitor.y,
            monitor.w, monitor.h);
    if (clipped.dim.w == 0u || clipped.dim.h == 0u) {
        return false;
    }

    if (out_x != NULL) {
        *out_x = clipped.pos.x;
    }
    if (out_y != NULL) {
        *out_y = clipped.pos.y;
    }
    *out_w = geom_dim_clamp((int32_t) clipped.dim.w);
    *out_h = geom_dim_clamp((int32_t) clipped.dim.h);

    return true;
}
