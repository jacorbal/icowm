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


/* Recompute the work area for every desktop on a surface */
void surface_workarea_refresh_all(surface_td *surface)
{

    if (surface == NULL) {
        return;
    }

    surface_desktop_walk_all(surface, s_workarea_update_visit, surface);

    /* Every path that recomputes a surface's work areas (an XRandR
     * resolution change, a dock or panel appearing or disappearing, and
     * every other one) needs to reach this too: without it, the
     * scratchpad stayed positioned against whatever work area was in
     * effect when it was last placed, however that later changed, until
     * the underlying process happened to exit
     * on its own. */
    scratchpad_reposition(surface);
}
