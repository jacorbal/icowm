/**
 * @file stage/workareas.c
 *
 * @brief Per-desktop work-area recomputation for a stage
 *
 * One of the files @c stage/ is made of (see @c stage.c's comment
 * for why).  Its single function today, kept separate from
 * @c stage/desktops.c regardless: work area is a rendering/placement
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
#include <stage.h>
#include <stage/desktop.h>
#include <stage/workarea.h>


/**
 * @brief Recompute one desktop's work area
 *
 * @param desktop Desktop reached by the walk
 * @param data    The stage it belongs to
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 *       reserving space on @p desktop
 */
static void s_workarea_update_visit(desktop_td *desktop, void *data)
{
    const stage_td *const stage = data;

    if (stage == NULL) {
        return;
    }

    desktop_update_workarea(desktop, stage,
            (stage->config != NULL)
                ? &stage->config->desktops : NULL,
            systray_get_reserved_strut(stage),
            stage->strutless_maximize);
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
 *        every one of a stage's desktops
 *
 * @a ccmd_client_refill_maximized (cmds/client/geom.c) resolves and
 * applies one client's workarea fresh; run here for every client on
 * every desktop @p stage owns, so an already-maximized window visibly
 * grows or shrinks to match whatever its workarea just became, rather
 * than silently staying at whatever size it already was until the user
 * happens to un-maximize and re-maximize it by hand.
 *
 * @param stage Stage whose maximized clients should be re-filled
 *
 * @note No-op if @p stage or its desktop list is @c NULL
 * @note Complexity: @e O(n), where @e n is the total number of clients
 *       across every one of @p stage's desktops
 */
static void s_stage_refill_maximized_clients(stage_td *stage)
{
    cdlist_item_td *dnode;
    const cdlist_item_td *dinitial;

    if (stage == NULL || stage->desktops == NULL) {
        return;
    }

    dnode = cdlist_head(stage->desktops);
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


/* Recompute the work area for every desktop on a stage */
void stage_workarea_refresh_all(stage_td *stage)
{

    if (stage == NULL) {
        return;
    }

    stage_desktop_walk_all(stage, s_workarea_update_visit, stage);

    /* Every path that recomputes a stage's work areas (an XRandR
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
    s_stage_refill_maximized_clients(stage);
    scratchpad_reposition(stage);
}
