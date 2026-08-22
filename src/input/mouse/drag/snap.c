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
#include <stddef.h>     /* NULL */
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
#include <input/mouse/drag/snap.h>


/**
 * @brief Return the absolute value of a signed 32-bit integer
 *
 * @param value Signed value to take the absolute value of
 *
 * @return @p value if non-negative, otherwise its negation
 *
 * @note Complexity: @e O(1)
 */
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
 * @brief Find the closest work-area edge snap delta across every
 *        monitor on one axis
 *
 * Checks a single window edge position, @p point, on this axis
 * (left/right for @p horizontal, top/bottom otherwise) against
 * every monitor's own near @e and far work-area edge on @p desktop,
 * individually: a two-monitor surface has its own edge not just at
 * the two ends of the combined span, but also at the boundary
 * between the two, and a panel or taskbar present on only one
 * monitor's own edge reduces that one monitor's own work area
 * without touching a neighboring, panel-free monitor's own full
 * extent (see @a desktop_update_workarea's own doc comment,
 * desktop.h, for how @c monitor_workareas itself is computed).  A
 * monitor only ever counts as a candidate at all when @p cross_near/
 * @p cross_far, the window's own span on the @e other axis,
 * genuinely overlaps that monitor's own work area on that same other
 * axis (via @a s_drag_ranges_close, the exact same overlap test
 * window-against-window snapping already uses just above in this
 * file): without this, a window sitting entirely on one monitor
 * could otherwise snap to a neighboring monitor's own unrelated
 * panel height, one it is nowhere near lining up with at all.
 * Whichever single candidate, across every monitor's own near and
 * far edge together, lands closest to @p current wins, the exact
 * same "closest wins" rule window-against-window snapping already
 * follows in this same file; a move drag calls this once per edge
 * (@p point being its own near edge, then its own far edge in a
 * separate call) and keeps whichever of the two results is itself
 * closer, since both edges move together, while a resize drag calls
 * this only once, for whichever single edge the anchor lets move at
 * all.
 *
 * @param desktop    Desktop whose own @c monitor_workareas to check;
 *                    a @c NULL value or one with no monitors detected
 *                    leaves @p current untouched
 * @param current    Best delta found so far, also this call's own
 *                    return value if nothing here beats it
 * @param point      The window's own edge position to check, on
 *                    this axis
 * @param cross_near The window's own near edge on the @e other axis
 *                    (top for @p horizontal, left otherwise)
 * @param cross_far  The window's own far edge on the @e other axis
 *                    (bottom for @p horizontal, right otherwise)
 * @param snap       Maximum allowed gap for @p cross_near/@p
 *                    cross_far to still count as overlapping a given
 *                    monitor's own work area on that other axis
 * @param horizontal @c true to check every monitor's own work area
 *                    left/right edges, @c false for top/bottom
 *
 * @return The closest delta across @p current and every monitor's
 *         own near and far work-area edge on this axis
 *
 * @note Complexity: @e O(m), where @e m is @p desktop's own surface's
 *       monitor count
 */
static int32_t s_drag_monitor_edge_delta(const desktop_td *desktop,
        int32_t current, int32_t point,
        int32_t cross_near, int32_t cross_far, int32_t snap,
        bool horizontal)
{
    if (desktop == NULL) {
        return current;
    }

    for (uint32_t i = 0u; i < desktop->monitor_workarea_count; ++i) {
        int32_t m_near = horizontal
            ? desktop->monitor_workareas[i].pos.x
            : desktop->monitor_workareas[i].pos.y;
        int32_t m_far = m_near + (horizontal
                ? (int32_t) desktop->monitor_workareas[i].dim.w
                : (int32_t) desktop->monitor_workareas[i].dim.h);
        int32_t cross_m_near = horizontal
            ? desktop->monitor_workareas[i].pos.y
            : desktop->monitor_workareas[i].pos.x;
        int32_t cross_m_far = cross_m_near + (horizontal
                ? (int32_t) desktop->monitor_workareas[i].dim.h
                : (int32_t) desktop->monitor_workareas[i].dim.w);

        if (!s_drag_ranges_close(cross_near, cross_far,
                    cross_m_near, cross_m_far, snap)) {
            continue;
        }

        current = s_drag_closer_delta(current, m_near - point);
        current = s_drag_closer_delta(current, m_far - point);
    }

    return current;
}


/* Apply snapping behavior during client movement */
void drag_snap_move(int32_t *restrict x, int32_t *restrict y,
        uint32_t width, uint32_t height)
{
    int32_t snap_window;
    int32_t snap_screen;
    int32_t right;
    int32_t bottom;

    if (x == NULL || y == NULL) {
        return;
    }

    snap_window = (int32_t) s_drag.snap_window;
    snap_screen = (int32_t) s_drag.snap_screen;
    right = *x + (int32_t) width;
    bottom = *y + (int32_t) height;

    if (snap_window > 0 && s_drag.desktop != NULL &&
            s_drag.desktop->stacking != NULL &&
            cdlist_size(s_drag.desktop->stacking) > 0) {
        cdlist_item_td *node;
        const cdlist_item_td *initial;
        /* One past 'snap_window' itself, not 'snap_window' itself:
         * 'abs(delta) <= snap_window' below is what decides whether
         * a candidate actually applies, so starting exactly at
         * 'snap_window' would make that check pass on the untouched
         * initial value alone whenever no real candidate ever beat
         * it, applying a spurious snap of exactly the snap distance
         * with no nearby window at all responsible for it. */
        int32_t dx = snap_window + 1;
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
                                obottom, snap_window)) {
                        dx = s_drag_closer_delta(dx, oright - *x);
                        dx = s_drag_closer_delta(dx, oright - right);
                        dx = s_drag_closer_delta(dx, ox - right);
                        dx = s_drag_closer_delta(dx, ox - *x);
                    }

                    if (s_drag_ranges_close(*x, right, ox,
                                oright, snap_window)) {
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
                " bottom=%d, dx=%d, dy=%d, snap-window=%d)",
                *x, *y, right, bottom, dx, dy, snap_window);

        if (s_drag_abs_i32(dx) <= snap_window) {
            *x += dx;
            right += dx;
        }

        if (s_drag_abs_i32(dy) <= snap_window) {
            *y += dy;
            bottom += dy;
        }
    }

    if (snap_screen > 0) {
        /* One past 'snap_screen' itself; see the matching comment on
         * the window-snap 'dx'/'dy' above for why. */
        int32_t dx_screen = snap_screen + 1;
        int32_t dy_screen = dx_screen;

        dx_screen = s_drag_monitor_edge_delta(s_drag.desktop,
                dx_screen, *x, *y, bottom, snap_screen, true);
        dx_screen = s_drag_monitor_edge_delta(s_drag.desktop,
                dx_screen, right, *y, bottom, snap_screen, true);
        dy_screen = s_drag_monitor_edge_delta(s_drag.desktop,
                dy_screen, *y, *x, right, snap_screen, false);
        dy_screen = s_drag_monitor_edge_delta(s_drag.desktop,
                dy_screen, bottom, *x, right, snap_screen, false);

        if (s_drag_abs_i32(dx_screen) <= snap_screen) {
            *x += dx_screen;
        }

        if (s_drag_abs_i32(dy_screen) <= snap_screen) {
            *y += dy_screen;
        }
    }
}


/* Apply snapping behavior during client resize */
void drag_snap_resize(int32_t *restrict x, int32_t *restrict y,
        uint32_t *restrict width, uint32_t *restrict height)
{
    int32_t snap_window;
    int32_t snap_screen;
    int32_t right;
    int32_t bottom;

    if (x == NULL || y == NULL || width == NULL || height == NULL) {
        return;
    }

    snap_window = (int32_t) s_drag.snap_window;
    snap_screen = (int32_t) s_drag.snap_screen;
    right = *x + (int32_t) *width;
    bottom = *y + (int32_t) *height;

    /* Which edge actually moves as the pointer moves depends on which
     * corner or side the user grabbed: 'anchor_right' means the LEFT
     * edge is the one being dragged (the right edge stays put), and
     * symmetrically for 'anchor_bottom' and the top edge.  Every delta
     * and snap check below has to target whichever edge that is, not
     * always assume it is the right/bottom edge the way a
     * left-edge-fixed resize would. */
    if (snap_window > 0 && s_drag.desktop != NULL &&
            s_drag.desktop->stacking != NULL &&
            cdlist_size(s_drag.desktop->stacking) > 0) {
        cdlist_item_td *node;
        const cdlist_item_td *initial;
        /* See the matching comment in 'drag_snap_move' for why this
         * is 'snap_window + 1', not 'snap_window' itself. */
        int32_t d_horiz = snap_window + 1;
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

                    if (s_drag_ranges_close(*y, bottom, oy,
                                obottom, snap_window)) {
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

                    if (s_drag_ranges_close(*x, right, ox,
                                oright, snap_window)) {
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
                " d-horiz=%d, d-vert=%d, snap-window=%d)",
                *x, *y, right, bottom,
                (int) s_drag.anchor_right, (int) s_drag.anchor_bottom,
                d_horiz, d_vert, snap_window);

        if (s_drag_abs_i32(d_horiz) <= snap_window) {
            if (s_drag.anchor_right) {
                *x += d_horiz;
                *width = geom_dim_clamp((int32_t) *width - d_horiz);
            } else {
                *width = geom_dim_clamp((int32_t) *width + d_horiz);
            }
            right = *x + (int32_t) *width;
        }

        if (s_drag_abs_i32(d_vert) <= snap_window) {
            if (s_drag.anchor_bottom) {
                *y += d_vert;
                *height = geom_dim_clamp((int32_t) *height - d_vert);
            } else {
                *height = geom_dim_clamp((int32_t) *height + d_vert);
            }
            bottom = *y + (int32_t) *height;
        }
    }

    if (snap_screen > 0) {
        /* One past 'snap_screen' itself; see the matching comment on
         * 'd_horiz'/'d_vert' above for why.  Checks only whichever
         * single edge the anchor actually lets move, unlike the move
         * drag's own two calls per axis in 'drag_snap_move': see
         * 's_drag_monitor_edge_delta''s own doc comment for the
         * fuller reasoning behind checking every monitor's own near
         * and far edge together, not just the combined surface's own
         * two ends. */
        int32_t d_screen_h = snap_screen + 1;
        int32_t d_screen_v = d_screen_h;

        d_screen_h = s_drag_monitor_edge_delta(s_drag.desktop,
                d_screen_h, (s_drag.anchor_right) ? *x : right,
                *y, bottom, snap_screen, true);
        if (s_drag_abs_i32(d_screen_h) <= snap_screen) {
            if (s_drag.anchor_right) {
                *x += d_screen_h;
                *width = geom_dim_clamp(
                        (int32_t) *width - d_screen_h);
            } else {
                *width = geom_dim_clamp(
                        (int32_t) *width + d_screen_h);
            }
        }

        d_screen_v = s_drag_monitor_edge_delta(s_drag.desktop,
                d_screen_v, (s_drag.anchor_bottom) ? *y : bottom,
                *x, right, snap_screen, false);
        if (s_drag_abs_i32(d_screen_v) <= snap_screen) {
            if (s_drag.anchor_bottom) {
                *y += d_screen_v;
                *height = geom_dim_clamp(
                        (int32_t) *height - d_screen_v);
            } else {
                *height = geom_dim_clamp(
                        (int32_t) *height + d_screen_v);
            }
        }
    }
}
