/**
 * @file policy/placement/icon.c
 *
 * @brief Icon placement policy implementation
 *
 * Implements @c place_icon_apply, which computes the screen position
 * for a newly iconified client window, and
 * @c place_icon_avoid_systray_overlap, which pushes an already-
 * proposed icon position away from the systray's own current
 * rectangle.  Kept apart from @c policy/placement/window.c so that
 * file stays focused on floating/smart window placement.
 *
 * @note "Tiling" here is the classic 1980s/90s window-manager sense
 *       (TWM, FVWM, and similar), i.e., arranging iconified windows'
 *       own icon markers into a non-overlapping grid on the desktop, as
 *       @a place_icon_apply does.  Therefore, it is unrelated to the
 *       modern "tiling window manager" sense of tiling the application
 *       windows themselves, for IcoWM gently places those as floating
 *       windows.
 *
 * @see @c policy/placement/window.c
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Types includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <policy/stacking.h>
#include <logger.h>

/* Local includes */
#include <defs/placement.h>
#include <policy/placement/icon.h>
#include <policy/placement/score.h>


/**
 * @brief Whether an icon-sized rectangle at (@p ix, @p iy) overlaps any
 *        already-occupied one
 *
 * Used by @a place_icon_apply to reject a candidate slot the moment
 * it shares any area at all with an existing icon, rather than only
 * rejecting the exact grid cell that icon's own position happens to
 * fall into.
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
static bool s_place_icon_rect_overlaps_any(int32_t ix, int32_t iy,
        uint16_t icon_w, uint16_t icon_h, int32_t border_twice,
        const int32_t *restrict occ_x, const int32_t *restrict occ_y,
        uint16_t occ_count)
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
 * @brief What @a s_icon_occupied_visit is gathering into
 */
struct s_icon_occupied_ctx_s {
    const client_td *skip_client;   /**< Client being placed */
    int32_t *xs;                    /**< Occupied icon x positions */
    int32_t *ys;                    /**< Occupied icon y positions */
    uint16_t capacity;              /**< How many the arrays hold */
    uint16_t count;                 /**< How many have been noted */
    bool has_warned;                /**< Whether the cap was reported */
    uint32_t desktop_id;            /**< Desktop, for that one report */
};


/**
 * @brief Note where one iconified client's own icon already sits
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_icon_occupied_ctx_s being filled
 *
 * @note Past the arrays' own capacity further icons are left out of
 *       the overlap check rather than the arrays grown without bound,
 *       and that is reported once so an unexpectedly icon-heavy
 *       desktop is at least visible in the log
 * @note Complexity: @e O(1)
 */
static void s_icon_occupied_visit(client_td *client, void *data)
{
    struct s_icon_occupied_ctx_s *const occupied_ctx = data;

    if (client == NULL || occupied_ctx == NULL ||
            client == occupied_ctx->skip_client ||
            client->icon_window == 0u ||
            !client_is_iconified(client)) {
        return;
    }

    if (occupied_ctx->count >= occupied_ctx->capacity) {
        if (!occupied_ctx->has_warned) {
            LOGGER_WARNING("More than %u iconified clients on" \
                    " desktop %u; overlap checking stops counting" \
                    " past this many",
                    (unsigned int) occupied_ctx->capacity,
                    (unsigned int) occupied_ctx->desktop_id);
            occupied_ctx->has_warned = true;
        }
        return;
    }

    occupied_ctx->xs[occupied_ctx->count] = client->icon_pos.x;
    occupied_ctx->ys[occupied_ctx->count] = client->icon_pos.y;
    occupied_ctx->count++;
}


/**
 * @brief Convert a slot index to its top-left pixel position for one of
 *        the non-SMART edge-anchored placement policies
 *
 * Shared by @c place_icon_apply's own slot search (which needs every
 * candidate slot's pixel position to test for overlap) and its final
 * conversion of whichever slot search ends up choosing, so the two can
 * never disagree about what a given slot index actually means on
 * screen.
 *
 * @param policy       Placement edge; @c CONFIG_ICON_PLACEMENT_SMART is
 *                     not valid here (handled entirely separately)
 * @param slot         Slot index to convert
 * @param max_primary  Number of slots along the primary (edge) axis
 * @param margin       Fixed margin from the screen edge
 * @param step_x       Horizontal slot pitch (icon width plus margin)
 * @param step_y       Vertical slot pitch (icon height plus margin)
 * @param icon_dim     Icon width/height, in pixels
 * @param screen_dim   Screen (or monitor) width/height
 * @param border_twice Icon border width, doubled (both sides)
 * @param out_pos      Receives the slot's own top-left corner
 *
 * @note Complexity: @e O(1)
 *
 * @see @a s_place_icon_rect_overlaps_any
 */
static void s_place_icon_slot_to_pixel(enum config_icon_placement_e policy,
        uint16_t slot, uint16_t max_primary, uint16_t margin,
        uint16_t step_x, uint16_t step_y,
        struct dimensions_s icon_dim, struct dimensions_s screen_dim,
        int32_t border_twice, struct position_s *restrict out_pos)
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
    out_pos->x = (int32_t) margin;
    out_pos->y = (int32_t) margin;

    switch (policy) {
        case CONFIG_ICON_PLACEMENT_LEFT:
            out_pos->x = (int32_t) margin +
                (int32_t) sec * (int32_t) step_x;
            out_pos->y = (int32_t) margin +
                (int32_t) pri * (int32_t) step_y;
            break;

        case CONFIG_ICON_PLACEMENT_RIGHT:
            out_pos->x = (int32_t) screen_dim.w - (int32_t) icon_dim.w -
                (int32_t) margin - border_twice -
                (int32_t) sec * (int32_t) step_x;
            out_pos->y = (int32_t) margin +
                (int32_t) pri * (int32_t) step_y;
            break;

        case CONFIG_ICON_PLACEMENT_BOTTOM:
            out_pos->x = (int32_t) margin +
                (int32_t) pri * (int32_t) step_x;
            out_pos->y = (int32_t) screen_dim.h - (int32_t) margin -
                (int32_t) icon_dim.h - border_twice -
                (int32_t) sec * (int32_t) step_y;
            break;

        case CONFIG_ICON_PLACEMENT_TOP:
        case CONFIG_ICON_PLACEMENT_SMART:
            out_pos->x = (int32_t) margin +
                (int32_t) pri * (int32_t) step_x;
            out_pos->y = (int32_t) margin +
                (int32_t) sec * (int32_t) step_y;
            break;
    }
}


/* Compute the icon window position for a newly iconified client */
void place_icon_apply(const client_td *client, desktop_td *desktop,
        enum config_icon_placement_e policy,
        struct dimensions_s icon_dim,
        struct dimensions_s screen_dim,
        struct position_s *restrict out_pos)
{
    const uint16_t margin = (uint16_t) WM_ICON_GRID_MARGIN;
    const uint16_t step_x = (uint16_t) (icon_dim.w + margin);
    const uint16_t step_y = (uint16_t) (icon_dim.h + margin);
    uint64_t border_twice_u64;
    int32_t border_twice;
    uint16_t max_primary;
    int32_t occ_x[256];
    int32_t occ_y[256];
    uint16_t occ_count;
    struct s_icon_occupied_ctx_s occupied_ctx;
    uint16_t chosen;
    int32_t ix;
    int32_t iy;

    if (client == NULL || client->config == NULL || out_pos == NULL) {
        return;
    }

    out_pos->x = margin;
    out_pos->y = margin;

    border_twice_u64 =
        (uint64_t) client->config->theme.icon.active.border.width * 2u;
    border_twice = (border_twice_u64 > (uint64_t) INT32_MAX)
        ? INT32_MAX : (int32_t) border_twice_u64;

    /* Collect every already-mapped icon's own position once, shared by
     * both the SMART and non-SMART branches below: each candidate slot
     * is tested against these via 's_place_icon_rect_overlaps_any'
     * (real pixel overlap) rather than against a precomputed grid-index
     * table, so a candidate only a few pixels into an existing icon's
     * footprint is correctly rejected even when that icon's own saved
     * position is not itself grid-aligned (e.g., it sits on a monitor
     * whose own origin does not fall on a 'step_x'/'step_y' multiple of
     * this one, or a stale position momentarily left behind by a config
     * or theme change since it was last placed). */
    occ_count = 0u;
    occupied_ctx.skip_client = client;
    occupied_ctx.xs = occ_x;
    occupied_ctx.ys = occ_y;
    occupied_ctx.capacity = 256u;
    occupied_ctx.count = 0u;
    occupied_ctx.has_warned = false;
    occupied_ctx.desktop_id = (desktop != NULL) ? desktop->id : 0u;
    stacking_walk(desktop, s_icon_occupied_visit, &occupied_ctx);
    occ_count = occupied_ctx.count;

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
        max_primary = (screen_dim.w > step_x)
            ? (uint16_t) ((screen_dim.w - margin) / step_x) : 1u;
        if (max_primary == 0u) {
            max_primary = 1u;
        }

        iw_full = (border_twice > 0)
            ? (uint32_t) icon_dim.w + (uint32_t) border_twice
            : (uint32_t) icon_dim.w;
        ih_full = (border_twice > 0)
            ? (uint32_t) icon_dim.h + (uint32_t) border_twice
            : (uint32_t) icon_dim.h;

        /* Score every free slot by window overlap + compactness.
         * 'sec' (overflow row) is used as a compactness tie-breaker:
         * lower 'sec' means closer to the screen edge */
        chosen = 0u;
        for (uint16_t i = 0u; i < 256u; ++i) {
            struct position_s pos;

            s_place_icon_slot_to_pixel(CONFIG_ICON_PLACEMENT_BOTTOM, i,
                    max_primary, margin, step_x, step_y,
                    icon_dim, screen_dim, border_twice, &pos);
            ix = pos.x;
            iy = pos.y;

            if (iy < (int32_t) margin) {
                /* Slot is off the top of the screen; skip */
                continue;
            }

            if (s_place_icon_rect_overlaps_any(ix, iy,
                        (uint16_t) icon_dim.w, (uint16_t) icon_dim.h,
                        border_twice, occ_x, occ_y, occ_count)) {
                continue;
            }

            s = (uint16_t) (i / max_primary);

            /* Compactness: prefer slots near the screen edge */
            cost = (uint64_t) s *
                (uint64_t) PLACE_SMART_ICON_COST_PER_OVERFLOW_ROW;

            /* Overlap penalty against visible windows only: icon-vs-
             * icon collision is rejected outright above
             * ('s_place_icon_rect_overlaps_any'), not soft-costed
             * here, hence 'icon_pixel_cost' 0 below.  The shared core
             * (@a place_overlap_score, policy/placement/score.h)
             * already excludes a locked client (the scratchpad) from
             * counting as an obstacle. */
            cost += place_overlap_score(desktop, client,
                    (struct geometry_s) {
                        { ix, iy }, { iw_full, ih_full } },
                    (uint64_t) PLACE_SMART_ICON_COST_PER_WIN_PIXEL, 0u);

            if (cost < best_cost) {
                best_cost = cost;
                chosen = i;
                if (cost == 0u) {
                    break; /* perfect slot found */
                }
            }
        }

        /* Convert chosen slot to pixel coordinates (BOTTOM layout) */
        s_place_icon_slot_to_pixel(CONFIG_ICON_PLACEMENT_BOTTOM,
                chosen, max_primary, margin, step_x, step_y,
                icon_dim, screen_dim, border_twice, out_pos);
        pri = (uint16_t) (chosen % max_primary);
        sec = (uint16_t) (chosen / max_primary);

        if (out_pos->x < (int16_t) margin) {
            out_pos->x = (int16_t) margin;
        }
        if (out_pos->y < (int16_t) margin) {
            out_pos->y = (int16_t) margin;
        }

        LOGGER_DEBUG("Smart-placed icon (slot=%u, pri=%u, sec=%u," \
                " pos=%+d%+d)",
                (unsigned) chosen, (unsigned) pri, (unsigned) sec,
                (int) out_pos->x, (int) out_pos->y);
        return;
    }

    /* Non-smart policies: original slot-based placement */

    /* Number of slots along the primary axis: columns for TOP/BOTTOM,
     * rows for LEFT/RIGHT.  Secondary axis (overflow) is unlimited. */
    if (policy == CONFIG_ICON_PLACEMENT_LEFT ||
            policy == CONFIG_ICON_PLACEMENT_RIGHT) {
        max_primary = (screen_dim.h > step_y)
            ? (uint16_t) ((screen_dim.h - margin) / step_y) : 1u;
    } else {
        max_primary = (screen_dim.w > step_x)
            ? (uint16_t) ((screen_dim.w - margin) / step_x) : 1u;
    }
    if (max_primary == 0u) {
        max_primary = 1u;
    }

    /* Find the first slot whose own pixel footprint does not overlap
     * any already-mapped icon's, testing real overlap via
     * 's_place_icon_rect_overlaps_any' rather than a precomputed
     * grid-index table for the same reason the SMART branch above does;
     * see its comment on 'occ_x'/'occ_y'. */
    chosen = 0u;
    for (uint16_t i = 0u; i < 256u; ++i) {
        struct position_s pos;

        s_place_icon_slot_to_pixel(policy, i, max_primary, margin, step_x,
                step_y, icon_dim, screen_dim, border_twice, &pos);
        ix = pos.x;
        iy = pos.y;

        if (!s_place_icon_rect_overlaps_any(ix, iy,
                    (uint16_t) icon_dim.w, (uint16_t) icon_dim.h,
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
    s_place_icon_slot_to_pixel(policy, chosen, max_primary, margin,
            step_x, step_y, icon_dim, screen_dim,
            border_twice, out_pos);

    if (out_pos->x < (int16_t) margin) {
        out_pos->x = (int16_t) margin;
    }
    if (out_pos->y < (int16_t) margin) {
        out_pos->y = (int16_t) margin;
    }
}


/* Push an icon's own proposed position away from the systray's current
 * rectangle, if the two would overlap there */
bool place_icon_avoid_systray_overlap(const int16_t *restrict io_x,
        int16_t *restrict io_y,
        struct dimensions_s icon_dim,
        struct geometry_s tray,
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
                icon_dim.w, icon_dim.h,
                tray.pos.x, tray.pos.y, tray.dim.w, tray.dim.h) == 0u) {
        return false;
    }

    if (workarea != NULL) {
        workarea_top = workarea->pos.y;
        workarea_bottom = workarea->pos.y + (int32_t) workarea->dim.h;
    } else {
        workarea_top = INT32_MIN;
        workarea_bottom = INT32_MAX;
    }

    tray_mid_y = tray.pos.y + (int32_t) (tray.dim.h / 2u);
    tray_in_upper_half = (workarea != NULL)
        ? (tray_mid_y < workarea_top + (int32_t) (workarea->dim.h / 2u))
        : true;

    new_y = (tray_in_upper_half)
        ? tray.pos.y + (int32_t) tray.dim.h + (int32_t) WM_ICON_SYSTRAY_GAP
        : tray.pos.y - (int32_t) icon_dim.h - (int32_t) WM_ICON_SYSTRAY_GAP;

    if (new_y < workarea_top) {
        new_y = workarea_top;
    }
    if (new_y + (int32_t) icon_dim.h > workarea_bottom) {
        new_y = workarea_bottom - (int32_t) icon_dim.h;
    }

    *io_y = (int16_t) new_y;
    return true;
}
