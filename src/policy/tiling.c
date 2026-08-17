/**
 * @file policy/tiling.c
 *
 * @brief Icon placement policy implementation
 *
 * Implements @c place_icon, which computes the screen position for
 * a newly iconified client window.  Extracted from
 * @c policy/placement.c to keep that file focused on floating/smart
 * window placement.
 *
 * @note "Tiling" here is the classic 1980s/90s window-manager sense
 *       (TWM, FVWM, and similar), i.e., arranging iconified windows'
 *       own icon markers into a non-overlapping grid on the desktop, as
 *       @a place_icon does.  Therefore, it is unrelated to the modern
 *       "tiling window manager" sense of tiling the application windows
 *       themselves, for IcoWM gently places those as floating windows.
 *
 * @see @c policy/placement.c
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
#include <stddef.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Default initial values */
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <policy/internal.h>
#include <policy/placement.h>



/**
 * @brief Whether an icon-sized rectangle at (@p ix, @p iy) overlaps any
 *        already-occupied one
 *
 * Used by @a place_icon to reject a candidate slot the moment it shares
 * any area at all with an existing icon, rather than only rejecting the
 * exact grid cell that icon's own position happens to fall into.
 *
 * An icon's saved @c icon_x / @c icon_y is not guaranteed to be
 * grid-aligned relative to whichever monitor a new icon is being placed
 * on (each monitor's own margin/step grid starts fresh from its own
 * origin), so comparing grid-cell indices instead of real pixel overlap
 * can miss a genuine, if partial, overlap.
 *
 * @param ix           Candidate rectangle's left edge
 * @param iy           Candidate rectangle's top edge
 * @param icon_w       Icon width, in pixels
 * @param icon_h       Icon height, in pixels
 * @param border_twice Icon border width, doubled (both sides); added to
 *                     @p icon_w / @p icon_h the same way the
 *                     candidate's own on-screen footprint is grown by
 *                     it elsewhere in this file
 * @param occ_x        Occupied rectangles' left edges
 * @param occ_y        Occupied rectangles' top edges
 * @param occ_count    Number of entries in @p occ_x / @p occ_y
 *
 * @return @c true if the candidate rectangle overlaps any occupied one
 *
 * @note Complexity: @e O(n), where @e n is @p occ_count
 */
static bool s_icon_rect_overlaps_any(int32_t ix, int32_t iy,
        uint16_t icon_w, uint16_t icon_h, int32_t border_twice,
        const int32_t *occ_x, const int32_t *occ_y, uint16_t occ_count)
{
    uint32_t iw_full = (border_twice > 0)
        ? (uint32_t) icon_w + (uint32_t) border_twice
        : (uint32_t) icon_w;
    uint32_t ih_full = (border_twice > 0)
        ? (uint32_t) icon_h + (uint32_t) border_twice
        : (uint32_t) icon_h;

    for (uint16_t k = 0u; k < occ_count; ++k) {
        if (geom_intersection_area(ix, iy, iw_full, ih_full,
                    occ_x[k], occ_y[k], iw_full, ih_full) > 0u) {
            return true;
        }
    }
    return false;
}


/**
 * @brief Convert a slot index to its top-left pixel position for one of
 *        the non-SMART edge-anchored placement policies
 *
 * Shared by @c place_icon's own slot search (which needs every
 * candidate slot's pixel position to test for overlap; see
 * @c s_icon_rect_overlaps_any) and its final conversion of whichever
 * slot search ends up choosing, so the two can never disagree about
 * what a given slot index actually means on screen.
 *
 * @param policy       Placement edge; @c CONFIG_ICON_PLACEMENT_SMART is
 *                     not valid here (handled entirely separately)
 * @param slot         Slot index to convert
 * @param max_primary  Number of slots along the primary (edge) axis
 * @param margin       Fixed margin from the screen edge
 * @param step_x       Horizontal slot pitch (icon width plus margin)
 * @param step_y       Vertical slot pitch (icon height plus margin)
 * @param icon_w       Icon width, in pixels
 * @param icon_h       Icon height, in pixels
 * @param screen_w     Screen (or monitor) width
 * @param screen_h     Screen (or monitor) height
 * @param border_twice Icon border width, doubled (both sides)
 * @param out_ix       Receives the slot's left edge
 * @param out_iy       Receives the slot's top edge
 *
 * @note Complexity: @e O(1)
 */
static void s_icon_slot_to_pixel(enum config_icon_placement_e policy,
        uint16_t slot, uint16_t max_primary, uint16_t margin,
        uint16_t step_x, uint16_t step_y, uint16_t icon_w,
        uint16_t icon_h, uint16_t screen_w, uint16_t screen_h,
        int32_t border_twice, int32_t *out_ix, int32_t *out_iy)
{
    uint16_t pri = (uint16_t) (slot % max_primary);
    uint16_t sec = (uint16_t) (slot / max_primary);

    /* Deliberately no 'default:' below (see the switch itself).
     * GCC cannot prove that exhaustive over every value the enum's
     * underlying integer type could hold, only over the named members,
     * so '-Wmaybe-uninitialized' sees a path where neither output is
     * written.  This plain assignment ahead of the switch closes that
     * path without adding a 'default:' case, which would silently
     * swallow a future enum member added without a case here instead of
     * letting '-Wswitch' catch the omission. */
    *out_ix = (int32_t) margin;
    *out_iy = (int32_t) margin;

    switch (policy) {
        case CONFIG_ICON_PLACEMENT_LEFT:
            *out_ix = (int32_t) margin + (int32_t) sec * (int32_t) step_x;
            *out_iy = (int32_t) margin + (int32_t) pri * (int32_t) step_y;
            break;

        case CONFIG_ICON_PLACEMENT_RIGHT:
            *out_ix = (int32_t) screen_w - (int32_t) icon_w -
                (int32_t) margin - border_twice -
                (int32_t) sec * (int32_t) step_x;
            *out_iy = (int32_t) margin + (int32_t) pri * (int32_t) step_y;
            break;

        case CONFIG_ICON_PLACEMENT_BOTTOM:
            *out_ix = (int32_t) margin + (int32_t) pri * (int32_t) step_x;
            *out_iy = (int32_t) screen_h - (int32_t) margin -
                (int32_t) icon_h - border_twice -
                (int32_t) sec * (int32_t) step_y;
            break;

        case CONFIG_ICON_PLACEMENT_TOP:
        case CONFIG_ICON_PLACEMENT_SMART:
            *out_ix = (int32_t) margin + (int32_t) pri * (int32_t) step_x;
            *out_iy = (int32_t) margin + (int32_t) sec * (int32_t) step_y;
            break;
    }
}


/* Compute the icon window position for a newly iconified client */
void place_icon(const client_td *client, desktop_td *desktop,
        enum config_icon_placement_e policy,
        uint16_t icon_w, uint16_t icon_h,
        uint16_t screen_w, uint16_t screen_h,
        int16_t *out_x, int16_t *out_y)
{
    const uint16_t margin = (uint16_t) WM_ICON_GRID_MARGIN;
    const uint16_t step_x = (uint16_t) (icon_w + margin);
    const uint16_t step_y = (uint16_t) (icon_h + margin);
    uint64_t border_twice_u64;
    int32_t border_twice;
    uint16_t max_primary;
    int32_t occ_x[256];
    int32_t occ_y[256];
    uint16_t occ_count;
    uint16_t chosen;
    int32_t ix;
    int32_t iy;

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

    /* Collect every already-mapped icon's own position once, shared by
     * both the SMART and non-SMART branches below: each candidate slot
     * is tested against these via 's_icon_rect_overlaps_any' (real
     * pixel overlap) rather than against a precomputed grid-index
     * table, so a candidate only a few pixels into an existing icon's
     * footprint is correctly rejected even when that icon's own saved
     * position is not itself grid-aligned (e.g., it sits on a monitor
     * whose own origin does not fall on a 'step_x'/'step_y' multiple of
     * this one, or a stale position momentarily left behind by a config
     * or theme change since it was last placed). */
    occ_count = 0u;
    if (desktop != NULL && desktop->stacking != NULL) {
        cdlist_item_td *node = cdlist_head(desktop->stacking);
        const cdlist_item_td *initial = node;
        if (node != NULL) {
            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);
                if (other != NULL && other != client &&
                        other->icon_window != 0u &&
                        other->is_icon_mapped &&
                        occ_count < 256u) {
                    occ_x[occ_count] = other->icon_x;
                    occ_y[occ_count] = other->icon_y;
                    ++occ_count;
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }
    }

    /* SMART uses BOTTOM layout for slot indexing: slots are numbered
     * from the bottom-left corner, growing right then up.  Score every
     * free slot by its overlap with visible windows and pick the one
     * with the lowest cost instead of blindly taking the first
     * available slot. */
    if (policy == CONFIG_ICON_PLACEMENT_SMART) {
        uint64_t best_cost = UINT64_MAX;
        uint64_t cost;
        uint16_t s;
        uint32_t iw_full;
        uint32_t ih_full;
        uint16_t pri;
        uint16_t sec;

        /* Compute max_primary for BOTTOM layout */
        max_primary = (screen_w > step_x)
            ? (uint16_t) ((screen_w - margin) / step_x) : 1u;
        if (max_primary == 0u) {
            max_primary = 1u;
        }

        iw_full = (border_twice > 0)
            ? (uint32_t) icon_w + (uint32_t) border_twice
            : (uint32_t) icon_w;
        ih_full = (border_twice > 0)
            ? (uint32_t) icon_h + (uint32_t) border_twice
            : (uint32_t) icon_h;

        /* Score every free slot by window overlap + compactness.
         * 'sec' (overflow row) is used as a compactness tie-breaker:
         * lower 'sec' means closer to the screen edge */
        chosen = 0u;
        for (uint16_t i = 0u; i < 256u; ++i) {
            s_icon_slot_to_pixel(CONFIG_ICON_PLACEMENT_BOTTOM, i,
                    max_primary, margin, step_x, step_y, icon_w,
                    icon_h, screen_w, screen_h, border_twice,
                    &ix, &iy);

            if (iy < (int32_t) margin) {
                /* Slot is off the top of the screen; skip */
                continue;
            }

            if (s_icon_rect_overlaps_any(ix, iy, icon_w, icon_h,
                        border_twice, occ_x, occ_y, occ_count)) {
                continue;
            }

            s = (uint16_t) (i / max_primary);

            /* Compactness: prefer slots near the screen edge */
            cost = (uint64_t) s *
                (uint64_t) SMART_ICON_COST_PER_OVERFLOW_ROW;

            /* Penalty for overlap with visible windows */
            if (desktop != NULL && desktop->stacking != NULL) {
                cdlist_item_td *node = cdlist_head(desktop->stacking);
                const cdlist_item_td *initial = node;
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

        /* Convert chosen slot to pixel coordinates (BOTTOM layout) */
        s_icon_slot_to_pixel(CONFIG_ICON_PLACEMENT_BOTTOM, chosen,
                max_primary, margin, step_x, step_y, icon_w, icon_h,
                screen_w, screen_h, border_twice, &ix, &iy);
        *out_x = (int16_t) ix;
        *out_y = (int16_t) iy;
        pri = (uint16_t) (chosen % max_primary);
        sec = (uint16_t) (chosen / max_primary);

        if (*out_x < (int16_t) margin) {
            *out_x = (int16_t) margin;
        }
        if (*out_y < (int16_t) margin) {
            *out_y = (int16_t) margin;
        }

        LOGGER_DEBUG("Smart-placed icon (slot=%u, pri=%u, sec=%u," \
                " pos=%+d%+d)",
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

    /* Find the first slot whose own pixel footprint does not overlap
     * any already-mapped icon's, testing real overlap via
     * 's_icon_rect_overlaps_any' rather than a precomputed grid-index
     * table for the same reason the SMART branch above does; see its
     * own comment on 'occ_x'/'occ_y'. */
    chosen = 0u;
    for (uint16_t i = 0u; i < 256u; ++i) {
        s_icon_slot_to_pixel(policy, i, max_primary, margin, step_x,
                step_y, icon_w, icon_h, screen_w, screen_h,
                border_twice, &ix, &iy);

        if (!s_icon_rect_overlaps_any(ix, iy, icon_w, icon_h,
                    border_twice, occ_x, occ_y, occ_count)) {
            chosen = i;
            break;
        }
        chosen = i + 1u;
    }
    if (chosen >= 256u) {
        chosen = 0u;
    }

    /* Convert the chosen slot to pixel coordinates the same way its own
     * candidacy was tested above, so the two can never disagree */
    s_icon_slot_to_pixel(policy, chosen, max_primary, margin,
            step_x, step_y, icon_w, icon_h, screen_w, screen_h,
            border_twice, &ix, &iy);
    *out_x = (int16_t) ix;
    *out_y = (int16_t) iy;

    if (*out_x < (int16_t) margin) {
        *out_x = (int16_t) margin;
    }
    if (*out_y < (int16_t) margin) {
        *out_y = (int16_t) margin;
    }
}


/* Push an icon's own proposed position away from the systray's current
 * rectangle, if the two would overlap there */
bool icon_avoid_systray_overlap(const int16_t *io_x, int16_t *io_y,
        uint16_t icon_w, uint16_t icon_h,
        int32_t tray_x, int32_t tray_y, uint16_t tray_w, uint16_t tray_h,
        const struct geometry_s *workarea)
{
    int32_t workarea_top;
    int32_t workarea_bottom;
    int32_t tray_mid_y;
    bool tray_in_upper_half;
    int32_t new_y;

    if (io_x == NULL || io_y == NULL) {
        return false;
    }

    if (geom_intersection_area(*io_x, *io_y,
                (uint32_t) icon_w, (uint32_t) icon_h,
                tray_x, tray_y, tray_w, tray_h) == 0u) {
        return false;
    }

    if (workarea != NULL) {
        workarea_top = workarea->pos.y;
        workarea_bottom = workarea->pos.y + (int32_t) workarea->dim.h;
    } else {
        workarea_top = INT32_MIN;
        workarea_bottom = INT32_MAX;
    }

    tray_mid_y = tray_y + (int32_t) (tray_h / 2u);
    tray_in_upper_half = (workarea != NULL)
        ? (tray_mid_y < workarea_top + (int32_t) (workarea->dim.h / 2u))
        : true;

    new_y = (tray_in_upper_half)
        ? tray_y + (int32_t) tray_h + (int32_t) WM_ICON_SYSTRAY_GAP
        : tray_y - (int32_t) icon_h - (int32_t) WM_ICON_SYSTRAY_GAP;

    if (new_y < workarea_top) {
        new_y = workarea_top;
    }
    if (new_y + (int32_t) icon_h > workarea_bottom) {
        new_y = workarea_bottom - (int32_t) icon_h;
    }

    *io_y = (int16_t) new_y;
    return true;
}
