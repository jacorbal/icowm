/**
 * @file policy/placement/rect.c
 *
 * @brief Free-rectangle arithmetic the smart placement searches
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
#include <stdlib.h>     /* NULL, free */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/ohtbl.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/pair.h>

/* Command includes */
#include <cmds/client/transient.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <defs/placement.h>
#include <policy/placement/score.h>
#include <policy/placement/window.h>
#include <policy/placement/rect.h>


/**
 * @brief Result of one @a s_free_rect_shrink_against call
 */
enum s_shrink_result_e {
    S_SHRINK_NONE,       /**< The obstacle does not intrude at all */
    S_SHRINK_DONE,       /**< @p right or @p bottom shrank */
    S_SHRINK_COLLAPSED   /**< The obstacle covers the corner itself
                              (or every shrink direction is blocked),
                              leaving nothing free to grow from this
                              corner at all */
};


/**
 * @brief Shrink a growing free rectangle's right or bottom edge
 *        against one obstacle rectangle, on whichever side loses
 *        less area
 *
 * Pulled out of @a placement_free_rect_grow's obstacle loop so the
 * exact same
 * shrink logic applies uniformly whether the obstacle is another client
 * (iterated there) or the systray's on-screen rectangle (checked once
 * more after that loop, when @c windows.  placement's @c "smart" mode
 * is configured to avoid it).
 *
 * @param x0     Corner X coordinate the free rectangle grows from
 * @param y0     Corner Y coordinate the free rectangle grows from
 * @param right  Current right edge; updated in place on a shrink
 * @param bottom Current bottom edge; updated in place on a shrink
 * @param ox1    Obstacle's left edge
 * @param oy1    Obstacle's top edge
 * @param ox2    Obstacle's right edge
 * @param oy2    Obstacle's bottom edge
 *
 * @return @c S_SHRINK_NONE if the obstacle does not intrude at all,
 *         @c S_SHRINK_DONE if @p right or @p bottom shrank,
 *         @c S_SHRINK_COLLAPSED if the obstacle covers the corner
 *         itself (or every shrink direction is blocked), leaving
 *         nothing free to grow from this corner at all
 *
 * @note Complexity: @e O(1)
 */
static enum s_shrink_result_e s_free_rect_shrink_against(
        int32_t x0, int32_t y0,
        int32_t *restrict right, int32_t *restrict bottom,
        int32_t ox1, int32_t oy1, int32_t ox2, int32_t oy2)
{
    bool right_ok;
    bool bottom_ok;
    int32_t via_right;
    int32_t via_bottom;
    uint64_t area_right;
    uint64_t area_bottom;

    if (!(ox2 > x0 && ox1 < *right && oy2 > y0 && oy1 < *bottom)) {
        return S_SHRINK_NONE;
    }
    if (ox1 <= x0 && oy1 <= y0) {
        return S_SHRINK_COLLAPSED;
    }

    right_ok = (ox1 > x0);
    bottom_ok = (oy1 > y0);
    via_right = (right_ok) ? ox1 : *right;
    via_bottom = (bottom_ok) ? oy1 : *bottom;
    area_right = (right_ok)
        ? (uint64_t) (via_right - x0) * (uint64_t) (*bottom - y0)
        : 0u;
    area_bottom = (bottom_ok)
        ? (uint64_t) (*right - x0) * (uint64_t) (via_bottom - y0)
        : 0u;

    if (right_ok && (area_right >= area_bottom || !bottom_ok)) {
        *right = via_right;
    } else if (bottom_ok) {
        *bottom = via_bottom;
    } else {
        return S_SHRINK_COLLAPSED;
    }

    return S_SHRINK_DONE;
}


/**
 * @brief Grow the largest obstacle-free rectangle whose top-left
 *        corner sits at a given point, extending right and down
 *
 * Starts from the full box between the corner and the placement
 * bounds, then repeatedly shrinks it on whichever side loses less
 * area whenever a visible client (or, when @p tray_rect is not
 * @c NULL, the systray) intrudes, until nothing intrudes or the box
 * collapses.  Repeating the whole scan (bounded by the client count
 * on @p desktop, plus one more for @p tray_rect) instead of stopping
 * after one pass catches an obstacle that only starts to intrude once
 * an earlier shrink has already pulled a boundary toward it.
 *
 * @param desktop     Desktop whose clients are checked against
 * @param skip_client Client to ignore (the one being placed)
 * @param x0          Corner X coordinate the rectangle grows from
 * @param y0          Corner Y coordinate the rectangle grows from
 * @param bound_x     Right placement bound the rectangle cannot
 *                    cross
 * @param bound_y     Bottom placement bound the rectangle cannot
 *                    cross
 * @param tray_rect   The systray's current on-screen rectangle to also
 *                    avoid, or @c NULL to skip it (@c systray.
 *                    avoid-overlap is @c false, or has no effect while
 *                    @c systray.reserve-space is @c true
 * @param out_w       Receives the free width found, or 0 if the
 *                    corner itself sits inside another obstacle
 * @param out_h       Receives the free height found, or 0 likewise
 *
 * @note Complexity: @e O(n^2) worst case, where @e n is the number
 *       of clients on @p desktop
 *
 * @see @a place_window_smart
 */
void placement_free_rect_grow(const desktop_td *desktop,
        const client_td *skip_client,
        int32_t x0, int32_t y0, int32_t bound_x, int32_t bound_y,
        const struct geometry_s *tray_rect,
        uint32_t *out_w, uint32_t *out_h)
{
    int32_t right = bound_x;
    int32_t bottom = bound_y;
    uint32_t guard;
    uint32_t guard_max;

    if (out_w == NULL || out_h == NULL) {
        return;
    }
    *out_w = 0u;
    *out_h = 0u;

    if (desktop == NULL || right <= x0 || bottom <= y0) {
        return;
    }

    guard_max = (desktop->stacking != NULL)
        ? (uint32_t) cdlist_size(desktop->stacking) + 1u : 1u;
    if (tray_rect != NULL) {
        guard_max += 1u;
    }

    for (guard = 0u; guard < guard_max; ++guard) {
        cdlist_item_td *node;
        bool shrunk = false;

        if (desktop->stacking != NULL &&
                cdlist_size(desktop->stacking) != 0u) {
            const cdlist_item_td *initial;

            node = cdlist_head(desktop->stacking);
            initial = node;
            if (node != NULL) {
                do {
                    const client_td *other =
                        (const client_td *) cdlist_data(node);

                    if (other != NULL && other != skip_client &&
                            !(other->properties.flags &
                                CLIENT_FLAG_HIDDEN) &&
                            !client_is_locked(other) &&
                            other->properties.state !=
                                (uint16_t) CLIENT_STATE_ICONIFIED) {
                        const int32_t ox1 =
                            other->layout.geometry.cur.pos.x;
                        const int32_t oy1 =
                            other->layout.geometry.cur.pos.y;
                        const int32_t ox2 = ox1 +
                            (int32_t) other->layout.geometry.cur.dim.w;
                        const int32_t oy2 = oy1 +
                            (int32_t) other->layout.geometry.cur.dim.h;
                        const enum s_shrink_result_e r =
                            s_free_rect_shrink_against(x0, y0,
                                    &right, &bottom,
                                    ox1, oy1, ox2, oy2);

                        if (r == S_SHRINK_COLLAPSED) {
                            *out_w = 0u;
                            *out_h = 0u;
                            return;
                        }
                        if (r == S_SHRINK_DONE) {
                            shrunk = true;
                        }
                    }
                    node = cdlist_next(node);
                } while (node != NULL && node != initial);
            }
        }

        if (tray_rect != NULL) {
            const enum s_shrink_result_e r = s_free_rect_shrink_against(
                    x0, y0, &right, &bottom,
                    tray_rect->pos.x, tray_rect->pos.y,
                    tray_rect->pos.x + (int32_t) tray_rect->dim.w,
                    tray_rect->pos.y + (int32_t) tray_rect->dim.h);

            if (r == S_SHRINK_COLLAPSED) {
                *out_w = 0u;
                *out_h = 0u;
                return;
            }
            if (r == S_SHRINK_DONE) {
                shrunk = true;
            }
        }

        if (!shrunk) {
            break;
        }
    } /* ! for (guard) */

    if (right > x0 && bottom > y0) {
        *out_w = (uint32_t) (right - x0);
        *out_h = (uint32_t) (bottom - y0);
    }
}


/**
 * @brief Score a candidate window position against existing clients
 *        and, optionally, the systray
 *
 * The overlap penalty itself (@a place_overlap_score,
 * @c policy/placement/score.h) is shared with
 * @c place_icon_apply's @c CONFIG_ICON_PLACEMENT_SMART search.
 * Only the tie-breaker below, and the optional systray penalty, are
 * specific to window placement.  A small distance-to-center penalty
 * breaks ties in favor of the workarea center, staying much smaller
 * than any overlap penalty so it only matters when two positions have
 * equal overlap cost.
 *
 * @param desktop     Desktop whose clients are inspected
 * @param skip_client Client to ignore (the one being placed)
 * @param x           Candidate left coordinate
 * @param y           Candidate top coordinate
 * @param fw          Candidate width
 * @param fh          Candidate height
 * @param tray_rect   The systray's current on-screen rectangle to
 *                    add the same per-pixel overlap penalty as an
 *                    ordinary window for, or @c NULL to skip it
 *                    (@c systray.avoid-overlap is @c false, or has
 *                    no effect while @c systray.reserve-space is
 *                    @c true; see @a place_window_smart)
 * @param center_x    X coordinate of the workarea center
 * @param center_y    Y coordinate of the workarea center
 *
 * @return Aggregate cost; lower is better; 0 means a perfect position
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
uint64_t placement_score_window_pos(const desktop_td *desktop,
        const client_td *skip_client,
        int32_t x, int32_t y, uint32_t fw, uint32_t fh,
        const struct geometry_s *tray_rect,
        int32_t center_x, int32_t center_y)
{
    uint64_t cost;
    int32_t dx;
    int32_t dy;

    cost = place_overlap_score(desktop, skip_client,
            (struct geometry_s) { { x, y }, { fw, fh } },
            (uint64_t) PLACE_SMART_WIN_COST_PER_WIN_PIXEL,
            (uint64_t) PLACE_SMART_WIN_COST_PER_ICON_PIXEL);

    if (tray_rect != NULL) {
        uint32_t area = geom_intersection_area(x, y, fw, fh,
                tray_rect->pos.x, tray_rect->pos.y,
                tray_rect->dim.w, tray_rect->dim.h);

        cost += (uint64_t) PLACE_SMART_WIN_COST_PER_WIN_PIXEL *
            (uint64_t) PLACE_SMART_WIN_SYSTRAY_COST_MULTIPLIER *
            (uint64_t) area;
    }

    /* Secondary tie-breaker: Manhattan distance from workarea center.
     * Stays much smaller than any window-overlap penalty, so it only
     * matters when two positions have equal overlap cost. */
    dx = (x + (int32_t) (fw / 2u)) - center_x;
    dy = (y + (int32_t) (fh / 2u)) - center_y;
    cost += (uint64_t) ((dx < 0) ? -dx : dx) +
        (uint64_t) ((dy < 0) ? -dy : dy);

    return cost;
}
