/**
 * @file input/mouse/drag/snap.c
 *
 * @brief Edge and peer-window snap math for move/resize drags
 *
 * Split out of what used to be a single, flat @c input/mouse/drag.c;
 * see @c drag/internal.h for why.
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

/* ADT includes */
#include <adt/cdlist.h>

/* Utils includes */
#include <utils/geom.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <input/mouse/drag/internal.h>


static int32_t s_drag_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}


/**
 * @brief Select the delta with the smaller absolute magnitude
 *
 * Compares two deltas and returns the one whose absolute value
 * is smaller, preserving its original sign.
 *
 * @param current   Current best delta
 * @param candidate Candidate delta to compare
 *
 * @return The delta with the smaller absolute value
 *
 * @note Complexity: @e O(1)
 */
static int32_t s_drag_closer_delta(int32_t current, int32_t candidate)
{
    if (s_drag_abs_i32(candidate) < s_drag_abs_i32(current)) {
        return candidate;
    }

    return current;
}


/**
 * @brief Check whether two 1-D ranges overlap or are within snap
 *        distance
 *
 * Determines if two intervals either overlap or are closer than a given
 * snapping threshold, allowing near-alignment behavior.
 *
 * @param start_a Start of first range
 * @param end_a   End of first range
 * @param start_b Start of second range
 * @param end_b   End of second range
 * @param snap    Maximum allowed gap for ranges to be considered
 *                "close"
 *
 * @return @c true if ranges overlap or are within @p snap distance,
 *         otherwise @c false
 *
 * @note Complexity: @e O(1)
 */
static bool s_drag_ranges_close(int32_t start_a, int32_t end_a,
        int32_t start_b, int32_t end_b, int32_t snap)
{
    return !(end_a < start_b - snap || end_b < start_a - snap);
}


/**
 * @brief Apply snapping behavior during client movement
 *
 * Adjusts the proposed position of a moving client so it "snaps" to
 * nearby window edges or screen boundaries when within a configurable
 * threshold.  It compares the moving window against other visible,
 * non-iconified clients on the same desktop and computes the smallest
 * adjustment needed to align edges.
 *
 * Snapping is applied independently along both axes and also considers
 * screen edges if available.
 *
 * @param x      Pointer to the proposed X coordinate (updated in place)
 * @param y      Pointer to the proposed Y coordinate (updated in place)
 * @param width  Width of the moving client
 * @param height Height of the moving client
 *
 * @note Requires a valid global @c s_drag context
 * @note Complexity: @e O(n), where @e n is the number of clients in the
 *       stacking list
 */
void drag_snap_move(int32_t *restrict x, int32_t *restrict y,
        uint32_t width, uint32_t height)
{
    int32_t snap;
    int32_t right;
    int32_t bottom;

    if (x == NULL || y == NULL || s_drag.snap == 0) {
        return;
    }

    snap = (int32_t) s_drag.snap;
    right = *x + (int32_t) width;
    bottom = *y + (int32_t) height;

    if (s_drag.desktop != NULL && s_drag.desktop->stacking != NULL &&
            cdlist_size(s_drag.desktop->stacking) > 0) {
        cdlist_item_td *node;
        const cdlist_item_td *initial;
        /* One past 'snap' itself, not 'snap' itself: 'abs(delta) <=
         * snap' below is what decides whether a candidate actually
         * applies, so starting exactly at 'snap' would make that
         * check pass on the untouched initial value alone whenever no
         * real candidate ever beat it, applying a spurious snap of
         * exactly the snap distance with no nearby window at all
         * responsible for it. */
        int32_t dx = snap + 1;
        int32_t dy = dx;

        node = cdlist_head(s_drag.desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);

                if (other != NULL && other != s_drag.client &&
                        !client_is_hidden(other) &&
                        !client_is_iconified(other)) {
                    int32_t ox = other->layout.geometry.cur.pos.x;
                    int32_t oy = other->layout.geometry.cur.pos.y;
                    int32_t oright = ox +
                        (int32_t) other->layout.geometry.cur.dim.w;
                    int32_t obottom = oy +
                        (int32_t) other->layout.geometry.cur.dim.h;

                    if (s_drag_ranges_close(*y, bottom, oy,
                                obottom, snap)) {
                        dx = s_drag_closer_delta(dx, oright - *x);
                        dx = s_drag_closer_delta(dx, oright - right);
                        dx = s_drag_closer_delta(dx, ox - right);
                        dx = s_drag_closer_delta(dx, ox - *x);
                    }

                    if (s_drag_ranges_close(*x, right, ox,
                                oright, snap)) {
                        dy = s_drag_closer_delta(dy, obottom - *y);
                        dy = s_drag_closer_delta(dy, obottom - bottom);
                        dy = s_drag_closer_delta(dy, oy - bottom);
                        dy = s_drag_closer_delta(dy, oy - *y);
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }

        LOGGER_TRACE("Move snap candidates (x=%d, y=%d, right=%d," \
                " bottom=%d, dx=%d, dy=%d, snap=%d)",
                *x, *y, right, bottom, dx, dy, snap);

        if (s_drag_abs_i32(dx) <= snap) {
            *x += dx;
            right += dx;
        }

        if (s_drag_abs_i32(dy) <= snap) {
            *y += dy;
            bottom += dy;
        }
    }

    if (s_drag.screen_w > 0 &&
            s_drag_abs_i32(*x) <= snap) {
        right -= *x;
        *x = 0;
    }

    if (s_drag.screen_h > 0 &&
            s_drag_abs_i32(*y) <= snap) {
        bottom -= *y;
        *y = 0;
    }

    if (s_drag.screen_w > 0 &&
            s_drag_abs_i32(right -
                (int32_t) s_drag.screen_w) <= snap) {
        *x = (int32_t) s_drag.screen_w - (int32_t) width;
    }

    if (s_drag.screen_h > 0 &&
            s_drag_abs_i32(bottom -
                (int32_t) s_drag.screen_h) <= snap) {
        *y = (int32_t) s_drag.screen_h - (int32_t) height;
    }
}


void drag_snap_resize(int32_t *restrict x, int32_t *restrict y,
        uint32_t *restrict width, uint32_t *restrict height)
{
    int32_t snap;
    int32_t right;
    int32_t bottom;

    if (x == NULL || y == NULL || width == NULL || height == NULL ||
            s_drag.snap == 0) {
        return;
    }

    snap = (int32_t) s_drag.snap;
    right = *x + (int32_t) *width;
    bottom = *y + (int32_t) *height;

    /* Which edge actually moves as the pointer moves depends on which
     * corner or side the user grabbed: 'anchor_right' means the LEFT
     * edge is the one being dragged (the right edge stays put), and
     * symmetrically for 'anchor_bottom' and the top edge.  Every delta
     * and snap check below has to target whichever edge that is, not
     * always assume it is the right/bottom edge the way a
     * left-edge-fixed resize would. */
    if (s_drag.desktop != NULL && s_drag.desktop->stacking != NULL &&
            cdlist_size(s_drag.desktop->stacking) > 0) {
        cdlist_item_td *node;
        const cdlist_item_td *initial;
        /* See the matching comment in 'drag_snap_move' for why this
         * is 'snap + 1', not 'snap' itself. */
        int32_t d_horiz = snap + 1;
        int32_t d_vert = d_horiz;

        node = cdlist_head(s_drag.desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);

                if (other != NULL && other != s_drag.client &&
                        !client_is_hidden(other) &&
                        !client_is_iconified(other)) {
                    int32_t ox = other->layout.geometry.cur.pos.x;
                    int32_t oy = other->layout.geometry.cur.pos.y;
                    int32_t oright = ox +
                        (int32_t) other->layout.geometry.cur.dim.w;
                    int32_t obottom = oy +
                        (int32_t) other->layout.geometry.cur.dim.h;

                    if (s_drag_ranges_close(*y, bottom,
                                oy, obottom, snap)) {
                        if (s_drag.anchor_right) {
                            d_horiz = s_drag_closer_delta(d_horiz,
                                    oright - *x);
                            d_horiz = s_drag_closer_delta(d_horiz,
                                    ox - *x);
                        } else {
                            d_horiz = s_drag_closer_delta(d_horiz,
                                    oright - right);
                            d_horiz = s_drag_closer_delta(d_horiz,
                                    ox - right);
                        }
                    }

                    if (s_drag_ranges_close(*x, right,
                                ox, oright, snap)) {
                        if (s_drag.anchor_bottom) {
                            d_vert = s_drag_closer_delta(d_vert,
                                    obottom - *y);
                            d_vert = s_drag_closer_delta(d_vert,
                                    oy - *y);
                        } else {
                            d_vert = s_drag_closer_delta(d_vert,
                                    obottom - bottom);
                            d_vert = s_drag_closer_delta(d_vert,
                                    oy - bottom);
                        }
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }

        LOGGER_TRACE("Resize snap candidates (x=%d, y=%d, right=%d," \
                " bottom=%d, anchor-right=%d, anchor-bottom=%d," \
                " d-horiz=%d, d-vert=%d, snap=%d)",
                *x, *y, right, bottom,
                (int) s_drag.anchor_right, (int) s_drag.anchor_bottom,
                d_horiz, d_vert, snap);

        if (s_drag_abs_i32(d_horiz) <= snap) {
            if (s_drag.anchor_right) {
                *x += d_horiz;
                *width = geom_clamp_dim((int32_t) *width - d_horiz);
            } else {
                *width = geom_clamp_dim((int32_t) *width + d_horiz);
            }
            right = *x + (int32_t) *width;
        }

        if (s_drag_abs_i32(d_vert) <= snap) {
            if (s_drag.anchor_bottom) {
                *y += d_vert;
                *height = geom_clamp_dim((int32_t) *height - d_vert);
            } else {
                *height = geom_clamp_dim((int32_t) *height + d_vert);
            }
            bottom = *y + (int32_t) *height;
        }
    }

    if (s_drag.screen_w > 0) {
        if (s_drag.anchor_right) {
            /* Dragging the left edge: it can snap to the screen's own
             * left edge, which a resize never checked for before. */
            if (s_drag_abs_i32(*x) <= snap) {
                *width = geom_clamp_dim((int32_t) *width + *x);
                *x = 0;
            }
        } else if (s_drag_abs_i32(right -
                    (int32_t) s_drag.screen_w) <= snap) {
            *width = geom_clamp_dim((int32_t) s_drag.screen_w - *x);
        }
    }

    if (s_drag.screen_h > 0) {
        if (s_drag.anchor_bottom) {
            /* Dragging the top edge: same reasoning as the left edge
             * above, snapping to the screen's own top edge. */
            if (s_drag_abs_i32(*y) <= snap) {
                *height = geom_clamp_dim((int32_t) *height + *y);
                *y = 0;
            }
        } else if (s_drag_abs_i32(bottom -
                    (int32_t) s_drag.screen_h) <= snap) {
            *height = geom_clamp_dim((int32_t) s_drag.screen_h - *y);
        }
    }
}
