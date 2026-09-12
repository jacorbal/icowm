/**
 * @file input/mouse/drag/snap.c
 *
 * @brief Edge and peer-window snap math for move/resize drags
 *
 * One of the files @c input/mouse/drag/ is made of;
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
#include <policy/stacking.h>
#include <logger.h>

/* Local includes */
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/snap.h>


/**
 * @brief What a snap walk carries across the windows it considers
 *
 * The best delta found so far on each axis, which is why a snap cannot
 * be worked out one window at a time and thrown away.
 */
struct s_drag_snap_ctx_s {
    int32_t left;           /**< Dragged window's left edge */
    int32_t top;            /**< Its top edge */
    int32_t right;          /**< Its right edge */
    int32_t bottom;         /**< Its bottom edge */
    int32_t snap_window;    /**< Snap distance in force */
    int32_t delta_x;        /**< Best horizontal delta so far */
    int32_t delta_y;        /**< Best vertical delta so far */
};


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
 * Compares two deltas and returns the one whose absolute value is
 * smaller, preserving its original sign.
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
 * @brief Consider one window as something to snap a move against
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_drag_snap_ctx_s this walk carries
 *
 * @note The dragged window itself, and anything hidden or iconified, is
 *       passed over: none of them is an edge the user can see
 * @note Complexity: @e O(1)
 */
static void s_drag_snap_move_visit(client_td *client, void *data)
{
    struct s_drag_snap_ctx_s *const snap_ctx = data;
    int32_t other_left;
    int32_t other_top;
    int32_t other_right;
    int32_t other_bottom;

    if (client == NULL || snap_ctx == NULL || client == s_drag.client ||
            client_is_hidden(client) || client_is_iconified(client)) {
        return;
    }

    other_left = client->layout.geometry.cur.pos.x;
    other_top = client->layout.geometry.cur.pos.y;
    other_right = other_left +
        (int32_t) client->layout.geometry.cur.dim.w;
    other_bottom = other_top +
        (int32_t) client->layout.geometry.cur.dim.h;

    if (s_drag_ranges_close(snap_ctx->top, snap_ctx->bottom,
                other_top, other_bottom, snap_ctx->snap_window)) {
        snap_ctx->delta_x = s_drag_closer_delta(snap_ctx->delta_x,
                other_right - snap_ctx->left);
        snap_ctx->delta_x = s_drag_closer_delta(snap_ctx->delta_x,
                other_right - snap_ctx->right);
        snap_ctx->delta_x = s_drag_closer_delta(snap_ctx->delta_x,
                other_left - snap_ctx->right);
        snap_ctx->delta_x = s_drag_closer_delta(snap_ctx->delta_x,
                other_left - snap_ctx->left);
    }

    if (s_drag_ranges_close(snap_ctx->left, snap_ctx->right,
                other_left, other_right, snap_ctx->snap_window)) {
        snap_ctx->delta_y = s_drag_closer_delta(snap_ctx->delta_y,
                other_bottom - snap_ctx->top);
        snap_ctx->delta_y = s_drag_closer_delta(snap_ctx->delta_y,
                other_bottom - snap_ctx->bottom);
        snap_ctx->delta_y = s_drag_closer_delta(snap_ctx->delta_y,
                other_top - snap_ctx->bottom);
        snap_ctx->delta_y = s_drag_closer_delta(snap_ctx->delta_y,
                other_top - snap_ctx->top);
    }
}


/**
 * @brief Consider one window as something to snap a resize against
 *
 * The same walk as @a s_drag_snap_move_visit, except that a resize
 * moves only the edge being dragged: which one that is decides whether
 * a candidate's edges are compared against this window's near side or
 * its far one.
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_drag_snap_ctx_s this walk carries
 *
 * @note Complexity: @e O(1)
 */
static void s_drag_snap_resize_visit(client_td *client, void *data)
{
    struct s_drag_snap_ctx_s *const snap_ctx = data;
    int32_t other_left;
    int32_t other_top;
    int32_t other_right;
    int32_t other_bottom;

    if (client == NULL || snap_ctx == NULL || client == s_drag.client ||
            client_is_hidden(client) || client_is_iconified(client)) {
        return;
    }

    other_left = client->layout.geometry.cur.pos.x;
    other_top = client->layout.geometry.cur.pos.y;
    other_right = other_left +
        (int32_t) client->layout.geometry.cur.dim.w;
    other_bottom = other_top +
        (int32_t) client->layout.geometry.cur.dim.h;

    if (s_drag_ranges_close(snap_ctx->top, snap_ctx->bottom,
                other_top, other_bottom, snap_ctx->snap_window)) {
        if (s_drag.is_anchor_right) {
            snap_ctx->delta_x = s_drag_closer_delta(snap_ctx->delta_x,
                    other_right - snap_ctx->left);
            snap_ctx->delta_x = s_drag_closer_delta(snap_ctx->delta_x,
                    other_left - snap_ctx->left);
        } else {
            snap_ctx->delta_x = s_drag_closer_delta(snap_ctx->delta_x,
                    other_right - snap_ctx->right);
            snap_ctx->delta_x = s_drag_closer_delta(snap_ctx->delta_x,
                    other_left - snap_ctx->right);
        }
    }

    if (s_drag_ranges_close(snap_ctx->left, snap_ctx->right,
                other_left, other_right, snap_ctx->snap_window)) {
        if (s_drag.is_anchor_bottom) {
            snap_ctx->delta_y = s_drag_closer_delta(snap_ctx->delta_y,
                    other_bottom - snap_ctx->top);
            snap_ctx->delta_y = s_drag_closer_delta(snap_ctx->delta_y,
                    other_top - snap_ctx->top);
        } else {
            snap_ctx->delta_y = s_drag_closer_delta(snap_ctx->delta_y,
                    other_bottom - snap_ctx->bottom);
            snap_ctx->delta_y = s_drag_closer_delta(snap_ctx->delta_y,
                    other_top - snap_ctx->bottom);
        }
    }
}


/**
 * @brief Find the closest work-area edge snap delta across every
 *        monitor on one axis
 *
 * Checks a single window edge position, @p point, on this axis
 * (left/right for @p horizontal, top/bottom otherwise) against every
 * monitor's near @e and far work-area edge on @p desktop, individually:
 * a two-monitor surface has an edge not just at the two ends of the
 * combined span, but also at the boundary between the two, and a panel
 * or taskbar present on only one monitor's edge reduces that one
 * monitor's work area without touching a neighboring, panel-free
 * monitor's full extent (see @a desktop_update_workarea's comment in
 * @c desktop.h for how @c monitor_workareas itself is computed).
 *
 * A monitor only ever counts as a candidate at all when @p cross_near
 * or @p cross_far, the window's span on the @e other axis, genuinely
 * overlaps that monitor's work area on that same other axis (via
 * @a s_drag_ranges_close, the exact same overlap test
 * window-against-window snapping already uses just above in this file):
 * without this, a window sitting entirely on one monitor could
 * otherwise snap to a neighboring monitor's unrelated panel height, one
 * it is nowhere near lining up with at all.  Whichever single
 * candidate, across every monitor's near and far edge together, lands
 * closest to @p current wins, the exact same "closest wins" rule
 * window-against-window snapping already follows in this same file;
 * a move drag calls this once per edge (@p point being its near edge,
 * then its far edge in a separate call) and keeps whichever of the two
 * results is itself closer, since both edges move together, while
 * a resize drag calls this only once, for whichever single edge the
 * anchor lets move at all.
 *
 * @param desktop    Desktop whose @c monitor_workareas to check;
 *                   a @c NULL value or one with no monitors detected
 *                   leaves @p current untouched
 * @param current    Best delta found so far, also this call's return
 *                   value if nothing here beats it
 * @param point      The window's edge position to check, on this axis
 * @param cross_near The window's near edge on the @e other axis
 *                   (top for @p horizontal, left otherwise)
 * @param cross_far  The window's far edge on the @e other axis
 *                   (bottom for @p horizontal, right otherwise)
 * @param snap       Maximum allowed gap for @p cross_near and
 *                   @p cross_far to still count as overlapping
 *                   a monitor's work area on that other axis
 * @param horizontal @c true to check every monitor's work area
 *                   left/right edges, @c false for top/bottom
 *
 * @return The closest delta across @p current and every monitor's near
 *         and far work-area edge on this axis
 *
 * @note Complexity: @e O(m), where @e m is @p desktop's surface's
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
            stacking_count(s_drag.desktop) > 0u) {
        struct s_drag_snap_ctx_s snap_ctx;
        /* One past 'snap_window' itself, not 'snap_window' itself:
         * 'abs(delta) <= snap_window' below is what decides whether
         * a candidate actually applies, so starting exactly at
         * 'snap_window' would make that check pass on the untouched
         * initial value alone whenever no real candidate ever beat it,
         * applying a spurious snap of exactly the snap distance with no
         * nearby window at all responsible for it. */
        int32_t dx = snap_window + 1;
        int32_t dy = dx;

        snap_ctx.left = *x;
        snap_ctx.top = *y;
        snap_ctx.right = right;
        snap_ctx.bottom = bottom;
        snap_ctx.snap_window = snap_window;
        snap_ctx.delta_x = dx;
        snap_ctx.delta_y = dy;
        stacking_walk(s_drag.desktop, s_drag_snap_move_visit,
                &snap_ctx);
        dx = snap_ctx.delta_x;
        dy = snap_ctx.delta_y;

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
     * corner or side the user grabbed: 'is_anchor_right' means the LEFT
     * edge is the one being dragged (the right edge stays put), and
     * symmetrically for 'is_anchor_bottom' and the top edge.  Every
     * delta and snap check below has to target whichever edge that is,
     * not always assume it is the right/bottom edge the way
     * a left-edge-fixed resize would. */
    if (snap_window > 0 && s_drag.desktop != NULL &&
            stacking_count(s_drag.desktop) > 0u) {
        struct s_drag_snap_ctx_s snap_ctx;
        /* See the matching comment in 'drag_snap_move' for why this
         * is 'snap_window + 1', not 'snap_window' itself. */
        int32_t d_horiz = snap_window + 1;
        int32_t d_vert = d_horiz;

        snap_ctx.left = *x;
        snap_ctx.top = *y;
        snap_ctx.right = right;
        snap_ctx.bottom = bottom;
        snap_ctx.snap_window = snap_window;
        snap_ctx.delta_x = d_horiz;
        snap_ctx.delta_y = d_vert;
        stacking_walk(s_drag.desktop, s_drag_snap_resize_visit,
                &snap_ctx);
        d_horiz = snap_ctx.delta_x;
        d_vert = snap_ctx.delta_y;

        LOGGER_TRACE("Resize snap candidates (x=%d, y=%d," \
                " right=%d, bottom=%d," \
                " anchor-right=%d, anchor-bottom=%d," \
                " d-horiz=%d, d-vert=%d, snap-window=%d)",
                *x, *y, right, bottom,
                (int) s_drag.is_anchor_right,
                (int) s_drag.is_anchor_bottom,
                d_horiz, d_vert, snap_window);

        if (s_drag_abs_i32(d_horiz) <= snap_window) {
            if (s_drag.is_anchor_right) {
                *x += d_horiz;
                *width = geom_dim_clamp((int32_t) *width - d_horiz);
            } else {
                *width = geom_dim_clamp((int32_t) *width + d_horiz);
            }
            right = *x + (int32_t) *width;
        }

        if (s_drag_abs_i32(d_vert) <= snap_window) {
            if (s_drag.is_anchor_bottom) {
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
         * drag's two calls per axis in 'drag_snap_move': see
         * 's_drag_monitor_edge_delta''s doc comment for the fuller
         * reasoning behind checking every monitor's near and far edge
         * together, not just the combined surface's two ends. */
        int32_t d_screen_h = snap_screen + 1;
        int32_t d_screen_v = d_screen_h;

        d_screen_h = s_drag_monitor_edge_delta(s_drag.desktop,
                d_screen_h, (s_drag.is_anchor_right) ? *x : right,
                *y, bottom, snap_screen, true);
        if (s_drag_abs_i32(d_screen_h) <= snap_screen) {
            if (s_drag.is_anchor_right) {
                *x += d_screen_h;
                *width = geom_dim_clamp(
                        (int32_t) *width - d_screen_h);
            } else {
                *width = geom_dim_clamp(
                        (int32_t) *width + d_screen_h);
            }
        }

        d_screen_v = s_drag_monitor_edge_delta(s_drag.desktop,
                d_screen_v, (s_drag.is_anchor_bottom) ? *y : bottom,
                *x, right, snap_screen, false);
        if (s_drag_abs_i32(d_screen_v) <= snap_screen) {
            if (s_drag.is_anchor_bottom) {
                *y += d_screen_v;
                *height = geom_dim_clamp(
                        (int32_t) *height - d_screen_v);
            } else {
                *height = geom_dim_clamp(
                        (int32_t) *height + d_screen_v);
            }
        }
    } /* ! if (snap_screen) */
}
