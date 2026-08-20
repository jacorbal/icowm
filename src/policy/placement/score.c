/**
 * @file policy/placement/score.c
 *
 * @brief Shared overlap-scoring core for SMART placement implementation
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
#include <stdint.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <desktop.h>

/* Local includes */
#include <policy/internal.h>
#include <policy/placement/score.h>


/* Accumulate overlap-penalty cost for a candidate rectangle against
 * every visible, unlocked client on a desktop */
uint64_t place_overlap_score(const desktop_td *desktop,
        const client_td *skip_client, struct geometry_s candidate,
        uint64_t win_pixel_cost, uint64_t icon_pixel_cost)
{
    cdlist_item_td *node;
    uint64_t cost = 0u;

    if (desktop != NULL && desktop->stacking != NULL &&
            cdlist_size(desktop->stacking) != 0u) {
        node = cdlist_head(desktop->stacking);
        if (node != NULL) {
            const cdlist_item_td *initial = node;

            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);

                if (other != NULL && other != skip_client &&
                        !(other->properties.flags & CLIENT_FLAG_HIDDEN) &&
                        !client_is_locked(other)) {
                    if (other->properties.state !=
                            (uint16_t) CLIENT_STATE_ICONIFIED) {
                        /* Visible window */
                        uint32_t area = geom_intersection_area(
                                candidate.pos.x, candidate.pos.y,
                                candidate.dim.w, candidate.dim.h,
                                other->layout.geometry.cur.pos.x,
                                other->layout.geometry.cur.pos.y,
                                other->layout.geometry.cur.dim.w,
                                other->layout.geometry.cur.dim.h);
                        cost += win_pixel_cost * (uint64_t) area;
                    } else if (other->icon_window != 0u &&
                            other->is_icon_mapped &&
                            other->icon_x >= 0 && other->icon_y >= 0) {
                        /* Visible icon */
                        uint32_t area = geom_intersection_area(
                                candidate.pos.x, candidate.pos.y,
                                candidate.dim.w, candidate.dim.h,
                                (int32_t) other->icon_x,
                                (int32_t) other->icon_y,
                                (uint32_t) SMART_WIN_ICON_SIZE,
                                (uint32_t) SMART_WIN_ICON_SIZE);
                        cost += icon_pixel_cost * (uint64_t) area;
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }
    }

    return cost;
}
