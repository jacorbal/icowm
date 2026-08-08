/**
 * @file policy/tiling.c
 *
 * @brief Icon placement policy implementation
 *
 * Implements @c place_icon, which computes the screen position for
 * a newly iconified client window.  Extracted from
 * @c policy/placement.c to keep that file focused on floating/smart
 * window placement.
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
#include <stdlib.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <policy/internal.h>
#include <policy/placement.h>



/* Compute the icon window position for a newly iconified client */
void place_icon(const client_td *client, desktop_td *desktop,
        enum config_icon_placement_e policy,
        uint16_t icon_w, uint16_t icon_h,
        uint16_t screen_w, uint16_t screen_h,
        int16_t *out_x, int16_t *out_y)
{
    const uint16_t margin = 8u;
    const uint16_t step_x = (uint16_t) (icon_w + margin);
    const uint16_t step_y = (uint16_t) (icon_h + margin);
    uint64_t border_twice_u64;
    int32_t border_twice;
    uint16_t max_primary;
    bool occupied[256];
    uint16_t chosen;
    uint16_t pri;
    uint16_t sec;

    if (client == NULL || client->theme == NULL ||
            out_x == NULL || out_y == NULL) {
        return;
    }

    *out_x = (int16_t) margin;
    *out_y = (int16_t) margin;

    border_twice_u64 =
        (uint64_t) client->theme->icon.active.border.width * 2u;
    border_twice = (border_twice_u64 > (uint64_t) INT32_MAX)
        ? INT32_MAX : (int32_t) border_twice_u64;

    /* SMART uses BOTTOM layout for slot indexing: slots are numbered
     * from the bottom-left corner, growing right then up.  Score every
     * free slot by its overlap with visible windows and pick the one
     * with the lowest cost instead of blindly taking the first
     * available slot. */
    if (policy == CONFIG_ICON_PLACEMENT_SMART) {
        /* Compute max_primary for BOTTOM layout */
        max_primary = (screen_w > step_x)
            ? (uint16_t) ((screen_w - margin) / step_x) : 1u;
        if (max_primary == 0u) {
            max_primary = 1u;
        }

        /* Mark occupied slots using BOTTOM reverse-mapping */
        for (uint16_t i = 0u; i < 256u; ++i) {
            occupied[i] = false;
        }

        if (desktop != NULL && desktop->stacking != NULL) {
            cdlist_item_td *node = cdlist_head(desktop->stacking);
            cdlist_item_td *initial = node;
            if (node != NULL) {
                do {
                    const client_td *other =
                        (const client_td *) cdlist_data(node);
                    if (other != NULL && other != client &&
                            other->icon_window != 0u &&
                            other->is_icon_mapped) {
                        int32_t rel_pri = (int32_t) other->icon_x -
                            (int32_t) margin;
                        int32_t rel_sec = (int32_t) screen_h -
                            (int32_t) margin -
                            (int32_t) icon_h -
                            border_twice -
                            (int32_t) other->icon_y;
                        if (rel_pri >= 0 && rel_sec >= 0) {
                            uint16_t p = (uint16_t) (rel_pri /
                                    (int32_t) step_x);
                            uint16_t s = (uint16_t) (rel_sec /
                                    (int32_t) step_y);
                            uint16_t slot =
                                (uint16_t) (s * max_primary + p);
                            if (slot < 256u) {
                                occupied[slot] = true;
                            }
                        }
                    }
                    node = cdlist_next(node);
                } while (node != NULL && node != initial);
            }
        }

        /* Score every free slot by window overlap + compactness.
         * 'sec' (overflow row) is used as a compactness tie-breaker:
         * lower 'sec' means closer to the screen edge */
        chosen = 0u;
        {
            uint64_t best_cost = UINT64_MAX;
            uint64_t cost;
            uint16_t p;
            uint16_t s;
            int32_t ix;
            int32_t iy;
            uint32_t iw_full;
            uint32_t ih_full;
            uint16_t i;

            iw_full = (border_twice > 0)
                ? (uint32_t) icon_w + (uint32_t) border_twice
                : (uint32_t) icon_w;
            ih_full = (border_twice > 0)
                ? (uint32_t) icon_h + (uint32_t) border_twice
                : (uint32_t) icon_h;

            for (i = 0u; i < 256u; ++i) {
                if (occupied[i]) {
                    continue;
                }

                p = (uint16_t) (i % max_primary);
                s = (uint16_t) (i / max_primary);
                ix = (int32_t) margin +
                    (int32_t) p * (int32_t) step_x;
                iy = (int32_t) screen_h -
                    (int32_t) margin -
                    (int32_t) icon_h -
                    border_twice -
                    (int32_t) s * (int32_t) step_y;

                if (iy < (int32_t) margin) {
                    /* Slot is off the top of the screen; skip */
                    continue;
                }

                /* Compactness: prefer slots near the screen edge */
                cost = (uint64_t) s *
                    (uint64_t) SMART_ICON_COST_PER_OVERFLOW_ROW;

                /* Penalty for overlap with visible windows */
                if (desktop != NULL && desktop->stacking != NULL) {
                    cdlist_item_td *node = cdlist_head(desktop->stacking);
                    cdlist_item_td *initial = node;
                    if (node != NULL) {
                        do {
                            const client_td *other =
                                (const client_td *) cdlist_data(node);
                            if (other != NULL && other != client &&
                                    !(other->properties.flags &
                                        CLIENT_FLAG_HIDDEN) &&
                                    other->properties.state !=
                                    (uint16_t) CLIENT_STATE_ICONIFIED) {
                                uint32_t area = geom_intersection_area(
                                        ix, iy, iw_full, ih_full,
                                        other->layout.geometry.cur.pos.x,
                                        other->layout.geometry.cur.pos.y,
                                        other->layout.geometry.cur.dim.w,
                                        other->layout.geometry.cur.dim.h);
                                cost += (uint64_t)
                                    SMART_ICON_COST_PER_WIN_PIXEL *
                                    (uint64_t) area;
                            }
                            node = cdlist_next(node);
                        } while (node != NULL && node != initial);
                    }
                }

                if (cost < best_cost) {
                    best_cost = cost;
                    chosen = i;
                    if (cost == 0u) {
                        break; /* perfect slot found */
                    }
                }
            }
        }

        /* Convert chosen slot to pixel coordinates (BOTTOM layout) */
        pri = (uint16_t) (chosen % max_primary);
        sec = (uint16_t) (chosen / max_primary);
        *out_x = (int16_t) ((int32_t) margin +
                (int32_t) pri * (int32_t) step_x);
        *out_y = (int16_t) ((int32_t) screen_h -
                (int32_t) margin -
                (int32_t) icon_h -
                border_twice -
                (int32_t) sec * (int32_t) step_y);

        if (*out_x < (int16_t) margin) {
            *out_x = (int16_t) margin;
        }
        if (*out_y < (int16_t) margin) {
            *out_y = (int16_t) margin;
        }

        LOGGER_DEBUG("Smart-placed icon (slot=%u, pri=%u, sec=%u," \
                " pos=%d+%d)",
                (unsigned) chosen, (unsigned) pri, (unsigned) sec,
                (int) *out_x, (int) *out_y);
        return;
    }

    /* Non-smart policies: original slot-based placement */

    /* Number of slots along the primary axis: columns for TOP/BOTTOM,
     * rows for LEFT/RIGHT.  Secondary axis (overflow) is unlimited. */
    if (policy == CONFIG_ICON_PLACEMENT_LEFT ||
            policy == CONFIG_ICON_PLACEMENT_RIGHT) {
        max_primary = (screen_h > step_y)
            ? (uint16_t) ((screen_h - margin) / step_y) : 1u;
    } else {
        max_primary = (screen_w > step_x)
            ? (uint16_t) ((screen_w - margin) / step_x) : 1u;
    }
    if (max_primary == 0u) {
        max_primary = 1u;
    }

    /* Mark occupied slots: 'slot = sec * max_primary + pri' where
     * 'pri' indexes along the edge, and 'sec' counts overflow
     * rows/columns */
    for (uint16_t i = 0u; i < 256u; ++i) {
        occupied[i] = false;
    }

    if (desktop != NULL && desktop->stacking != NULL) {
        cdlist_item_td *node = cdlist_head(desktop->stacking);
        cdlist_item_td *initial = node;
        if (node != NULL) {
            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);
                if (other != NULL && other != client &&
                        other->icon_window != 0 &&
                        other->is_icon_mapped) {
                    int32_t rel_pri;
                    int32_t rel_sec;
                    uint16_t slot;
                    rel_pri = 0;
                    rel_sec = 0;

                    switch (policy) {
                        case CONFIG_ICON_PLACEMENT_TOP:
                            rel_pri = (int32_t) other->icon_x -
                                (int32_t) margin;
                            rel_sec = (int32_t) other->icon_y -
                                (int32_t) margin;
                            if (rel_pri >= 0 && rel_sec >= 0) {
                                pri = (uint16_t) (rel_pri /
                                        (int32_t) step_x);
                                sec = (uint16_t) (rel_sec /
                                        (int32_t) step_y);
                                slot = (uint16_t) (sec * max_primary
                                        + pri);
                                if (slot < 256u) {
                                    occupied[slot] = true;
                                }
                            }
                            break;

                        case CONFIG_ICON_PLACEMENT_BOTTOM:
                            rel_pri = (int32_t) other->icon_x -
                                (int32_t) margin;
                            rel_sec = (int32_t) screen_h -
                                (int32_t) margin -
                                (int32_t) icon_h -
                                border_twice -
                                (int32_t) other->icon_y;
                            if (rel_pri >= 0 && rel_sec >= 0) {
                                pri = (uint16_t) (rel_pri /
                                        (int32_t) step_x);
                                sec = (uint16_t) (rel_sec /
                                        (int32_t) step_y);
                                slot = (uint16_t) (sec * max_primary
                                        + pri);
                                if (slot < 256u) {
                                    occupied[slot] = true;
                                }
                            }
                            break;

                        case CONFIG_ICON_PLACEMENT_LEFT:
                            rel_pri = (int32_t) other->icon_y -
                                (int32_t) margin;
                            rel_sec = (int32_t) other->icon_x -
                                (int32_t) margin;
                            if (rel_pri >= 0 && rel_sec >= 0) {
                                pri = (uint16_t) (rel_pri /
                                        (int32_t) step_y);
                                sec = (uint16_t) (rel_sec /
                                        (int32_t) step_x);
                                slot = (uint16_t) (sec * max_primary
                                        + pri);
                                if (slot < 256u) {
                                    occupied[slot] = true;
                                }
                            }
                            break;

                        case CONFIG_ICON_PLACEMENT_RIGHT:
                            rel_pri = (int32_t) other->icon_y -
                                (int32_t) margin;
                            rel_sec = (int32_t) screen_w -
                                (int32_t) margin -
                                (int32_t) icon_w -
                                border_twice -
                                (int32_t) other->icon_x;
                            if (rel_pri >= 0 && rel_sec >= 0) {
                                pri = (uint16_t) (rel_pri /
                                        (int32_t) step_y);
                                sec = (uint16_t) (rel_sec /
                                        (int32_t) step_x);
                                slot = (uint16_t) (sec * max_primary
                                        + pri);
                                if (slot < 256u) {
                                    occupied[slot] = true;
                                }
                            }
                            break;

                        case CONFIG_ICON_PLACEMENT_SMART:
                            /* handled above; unreachable here */
                            break;
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        } /* ! if (node) */
    }

    /* Find the first unoccupied slot */
    chosen = 0u;
    for (uint16_t i = 0u; i < 256u; ++i) {
        if (!occupied[i]) {
            chosen = i;
            break;
        }
        chosen = i + 1u;
    }
    if (chosen >= 256u) {
        chosen = 0u;
    }

    /* Convert slot index to pixel coordinates:
     * - 'pri = chosen % max_primary'  (position along the screen edge)
     * - 'sec = chosen / max_primary'  (overflow row/col away from edge) */
    pri = (uint16_t) (chosen % max_primary);
    sec = (uint16_t) (chosen / max_primary);

    switch (policy) {
        case CONFIG_ICON_PLACEMENT_TOP:
            *out_x = (int16_t) (margin + (uint32_t) pri * step_x);
            *out_y = (int16_t) (margin + (uint32_t) sec * step_y);
            break;

        case CONFIG_ICON_PLACEMENT_LEFT:
            *out_x = (int16_t) (margin + (uint32_t) sec * step_x);
            *out_y = (int16_t) (margin + (uint32_t) pri * step_y);
            break;

        case CONFIG_ICON_PLACEMENT_RIGHT:
            *out_x = (int16_t) ((int32_t) screen_w -
                    (int32_t) icon_w -
                    (int32_t) margin -
                    border_twice -
                    (int32_t) sec * (int32_t) step_x);
            *out_y = (int16_t) (margin + (uint32_t) pri * step_y);
            break;

        case CONFIG_ICON_PLACEMENT_BOTTOM:
            *out_x = (int16_t) (margin + (uint32_t) pri * step_x);
            *out_y = (int16_t) ((int32_t) screen_h -
                    (int32_t) margin -
                    (int32_t) icon_h -
                    border_twice -
                    (int32_t) sec * (int32_t) step_y);
            break;

        case CONFIG_ICON_PLACEMENT_SMART:
            /* handled above; unreachable here */
            break;
    }

    if (*out_x < (int16_t) margin) {
        *out_x = (int16_t) margin;
    }
    if (*out_y < (int16_t) margin) {
        *out_y = (int16_t) margin;
    }
}
