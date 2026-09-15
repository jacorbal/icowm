/**
 * @file surface/workareas.c
 *
 * @brief Per-desktop work-area recomputation for a surface
 *
 * One of the files @c surface/ is made of (see @c surface.c's comment
 * for why).  Its single function today, kept separate from
 * @c surface/desktops.c regardless: work area is a rendering/placement
 * concern, not desktop-list navigation, and giving it its file now
 * means anything that grows this concern later (a dedicated per-monitor
 * refresh entry point, say) already has the right home to grow into
 * instead of first needing its split.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */

/* ADT includes */
#include <adt/cdlist.h>

/* Policy includes */
#include <policy/stacking.h>

/* Command includes */
#include <cmds/client/maximize.h>

/* Project includes */
#include <desktop.h>
#include <systray.h>
#include <scratchpad.h>

/* Local includes */
#include <surface.h>
#include <surface/desktop.h>
#include <surface/workarea.h>


/**
 * @brief Recompute one desktop's work area
 *
 * @param desktop Desktop reached by the walk
 * @param data    The surface it belongs to
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 *       reserving space on @p desktop
 */
static void s_workarea_update_visit(desktop_td *desktop, void *data)
{
    const surface_td *const surface = data;

    if (surface == NULL) {
        return;
    }

    desktop_update_workarea(desktop, surface,
            (surface->config != NULL)
                ? &surface->config->desktops : NULL,
            systray_get_reserved_strut(surface),
            surface->strutless_maximize);
}


/**
 * @brief Re-apply one client's maximized geometry
 *
 * @param client Client reached by the walk
 * @param data   Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_client_refill_visit(client_td *client, void *data)
{
    (void) data;

    if (client != NULL) {
        ccmd_client_refill_maximized(client);
    }
}


/**
 * @brief Re-fill every already-maximized client's geometry across
 *        every one of a surface's desktops
 *
 * @a ccmd_client_refill_maximized (cmds/client/geom.c) resolves and
 * applies one client's workarea fresh; run here for every client on
 * every desktop @p surface owns, so an already-maximized window visibly
 * grows or shrinks to match whatever its workarea just became, rather
 * than silently staying at whatever size it already was until the user
 * happens to un-maximize and re-maximize it by hand.
 *
 * @param surface Surface whose maximized clients should be re-filled
 *
 * @note No-op if @p surface or its desktop list is @c NULL
 * @note Complexity: @e O(n), where @e n is the total number of clients
 *       across every one of @p surface's desktops
 */
static void s_surface_refill_maximized_clients(surface_td *surface)
{
    cdlist_item_td *dnode;
    const cdlist_item_td *dinitial;

    if (surface == NULL || surface->desktops == NULL) {
        return;
    }

    dnode = cdlist_head(surface->desktops);
    if (dnode == NULL) {
        return;
    }

    dinitial = dnode;
    do {
        const desktop_td *const d = (desktop_td *) cdlist_data(dnode);

        stacking_walk(d, s_client_refill_visit, NULL);
        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}


/* Recompute the work area for every desktop on a surface */
void surface_workarea_refresh_all(surface_td *surface)
{

    if (surface == NULL) {
        return;
    }

    surface_desktop_walk_all(surface, s_workarea_update_visit, surface);

    /* Every path that recomputes a surface's work areas (an XRandR
     * resolution change, a dock or panel appearing or disappearing, and
     * every other one) needs to reach both of these too, so neither
     * caller has to remember to trigger them separately:
     *
     * - without the first, an already-maximized window kept whatever
     *   size it had when it was maximized, however its workarea later
     *   changed, until the user happened to un-maximize and re-maximize
     *   it by hand;
     * - without the second, the scratchpad stayed positioned against
     *   whatever work area was in effect when it was last placed,
     *   however that later changed, until the underlying process
     *   happened to exit on its own. */
    s_surface_refill_maximized_clients(surface);
    scratchpad_reposition(surface);
}
