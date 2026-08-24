/**
 * @file surface/workareas.c
 *
 * @brief Per-desktop work-area recomputation for a surface
 *
 * Split out of what used to be a single, flat @c surface.c; see
 * @c surface.c's own doc comment for why.  Its own single function
 * today, kept separate from @c surface/desktops.c regardless: work
 * area is a rendering/placement concern, not desktop-list navigation,
 * and giving it its own file now means anything that grows this
 * concern later (a dedicated per-monitor refresh entry point, say)
 * already has the right home to grow into instead of first needing
 * its own split.
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

/* Local includes */
#include <scratchpad.h>
#include <surface.h>


/* Recompute the work area for every desktop on a surface */
void surface_refresh_workareas(surface_td *surface)
{
    cdlist_item_td *dnode;

    if (surface == NULL) {
        return;
    }

    cdlist_foreach(surface->desktops, dnode) {
        desktop_td *const d = (desktop_td *) cdlist_data(dnode);

        if (d != NULL) {
            desktop_update_workarea(d,
                    surface,
                    (surface->config != NULL)
                        ? &((surface->config)->desktops) : NULL,
                    systray_get_reserved_strut(surface),
                    surface->strutless_maximize);
        }
    }

    /* Every path that recomputes a surface's own work areas (an
     * XRandR resolution change, a dock or panel appearing or
     * disappearing, and every other one) needs to reach this too:
     * without it, the scratchpad stayed positioned against whatever
     * work area was in effect when it was last placed, however that
     * later changed, until the underlying process happened to exit
     * on its own. */
    scratchpad_reposition(surface);
}
