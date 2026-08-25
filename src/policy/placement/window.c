/**
 * @file policy/placement/window.c
 *
 * @brief Window placement policy implementation
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


/* Cascade sequence: how many clients this policy has placed so
 * far, since the window manager itself started; each new client
 * offsets one step further along the cascade, wrapping back to
 * the top-left corner once it runs past the configured maximum
 * step count (used in place_window_apply_cascade) */
static uint32_t s_cascade_seq = 0;


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
 * Pulled out of @a s_free_rect_grow's obstacle loop so the exact same
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
static void s_free_rect_grow(const desktop_td *desktop,
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
static uint64_t s_score_window_pos(const desktop_td *desktop,
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


/**
 * @brief Offset placement coordinates according to client gravity and
 *        clamp
 *
 * Adjusts the requested top-left placement coordinates so the client's
 * frame is positioned relative to its configured gravity point (e.g.,
 * centering or anchoring the frame by its edge or corner instead of its
 * top-left corner).  After applying the gravity offset, the resulting
 * position is clamped so the frame stays fully within the surface
 * bounds, preferring to keep it at the near edge when it does not fit.
 *
 * @param surface Pointer to the surface providing the placement bounds
 * @param client  Pointer to the client whose gravity and frame size are
 *                used
 * @param x       Pointer to the X coordinate to adjust and clamp in
 *                place
 * @param y       Pointer to the Y coordinate to adjust and clamp in
 *                place
 *
 * @note Complexity: @e O(1)
 */
static void s_place_window_apply_gravity(const surface_td *surface,
        const client_td *client,
        int32_t *restrict x, int32_t *restrict y)
{
    int32_t nx;
    int32_t ny;
    uint32_t sw;
    uint32_t sh;
    uint32_t fw;
    uint32_t fh;

    if (surface == NULL || client == NULL || x == NULL || y == NULL) {
        return;
    }

    nx = *x;
    ny = *y;
    sw = surface->properties.dim.w;
    sh = surface->properties.dim.h;
    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;

    if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_NORTH_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH_EAST) {
        nx -= (int32_t) fw;
    } else if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_NORTH ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_CENTER ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH) {
        nx -= (int32_t) (fw / 2u);
    }

    if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_SOUTH_WEST) {
        ny -= (int32_t) fh;
    } else if (client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_EAST ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_CENTER ||
            client->layout.gravity ==
            (uint16_t) CLIENT_GRAVITY_WEST) {
        ny -= (int32_t) (fh / 2u);
    }

    if (nx < 0) {
        nx = 0;
    } else if ((uint32_t) nx + fw > sw) {
        nx = (sw > fw) ? (int32_t) (sw - fw) : 0;
    }
    if (ny < 0) {
        ny = 0;
    } else if ((uint32_t) ny + fh > sh) {
        ny = (sh > fh) ? (int32_t) (sh - fh) : 0;
    }

    *x = nx;
    *y = ny;
}


/**
 * @brief Clip a workarea rectangle down to whichever physical monitor
 *        it overlaps, on a surface with more than one
 *
 * Falls back to leaving @p out_wa and @p out_screen unclipped (copies
 * of @p wa and @p screen) on a single-monitor surface, or when the
 * intersection against @p monitor is empty (e.g., a monitor entirely
 * covered by a strut): in either case the caller's unclipped rectangle
 * is already the right answer, not an error.
 *
 * @param surface    Surface the clip is against
 * @param wa         Workarea rectangle to clip
 * @param screen     Screen dimensions to clip alongside @p wa
 * @param monitor    Physical monitor to clip against
 * @param out_wa     Receives the clipped workarea
 * @param out_screen Receives the clipped screen dimensions
 *
 * @note Complexity: @e O(1)
 */
static void s_clip_to_monitor(const surface_td *surface,
        const struct geometry_s *wa, const struct dimensions_s *screen,
        monitor_td monitor,
        struct geometry_s *out_wa, struct dimensions_s *out_screen)
{
    struct geometry_s clipped;

    *out_wa = *wa;
    *out_screen = *screen;

    if (surface == NULL || surface->monitor_count <= 1u) {
        return;
    }

    clipped = geom_intersect_rect(wa->pos.x, wa->pos.y,
            wa->dim.w, wa->dim.h,
            monitor.x, monitor.y,
            monitor.w, monitor.h);
    if (clipped.dim.w == 0u || clipped.dim.h == 0u) {
        return;
    }

    *out_wa = clipped;
    out_screen->w = (uint32_t) monitor.x + monitor.w;
    out_screen->h = (uint32_t) monitor.y + monitor.h;
}


/**
 * @brief Resolve the monitor a placement decision should target
 *
 * Under @c CONFIG_PLACEMENT_MONITOR_PRIMARY, always returns
 * @p surface's primary monitor.
 * Under @c CONFIG_PLACEMENT_MONITOR_POINTER (the default), queries the
 * pointer and returns whichever monitor it is currently over, or
 * a degenerate (zero-area) geometry if the query fails; passing that on
 * to @a s_clip_to_monitor is safe, since its intersection against
 * a zero-area rectangle is always empty, which is exactly what makes it
 * leave its inputs unclipped.
 *
 * @param wm             Window manager state, for the pointer query
 * @param surface        Surface to resolve a monitor on
 * @param monitor_policy Which strategy to resolve with
 *
 * @return The resolved monitor's geometry
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors on
 *       @p surface
 */
static monitor_td s_reference_monitor(const wm_td *wm,
        surface_td *surface,
        enum config_placement_monitor_e monitor_policy)
{
    xcb_query_pointer_cookie_t cookie;
    xcb_query_pointer_reply_t *reply;
    monitor_td result = {.x = 0, .y = 0, .w = 0u, .h = 0u};

    if (monitor_policy == CONFIG_PLACEMENT_MONITOR_PRIMARY) {
        return surface_primary_monitor(surface);
    }

    cookie = xcb_query_pointer(wm_connection(wm), surface->screen->root);
    reply = xcb_query_pointer_reply(wm_connection(wm), cookie, NULL);
    if (reply != NULL) {
        result = surface_monitor_for_point(surface,
                (struct position_s) { reply->root_x, reply->root_y });
        free(reply);
    }

    return result;
}


/**
 * @brief Resolve the monitor a placement decision should target,
 *        preferring a related client's monitor over the configured
 *        policy when one is found
 *
 * A dialog should appear next to the window it belongs with, and
 * a fresh window from an application already running elsewhere should
 * appear next to that application, not wherever the pointer or the
 * primary monitor happens to be instead: checked in order, a specific
 * transient parent first (or, for a client transient for the whole
 * group per ICCCM §4.1.2.6, the resolved anchor, then any
 * currently-mapped sibling sharing the same group leader on this same
 * desktop.  Falls through to @a s_reference_monitor unchanged whenever
 * neither search finds a candidate, or the candidate found resolves to
 * a degenerate (zero-area) monitor.
 *
 * @param wm             Window manager state, for the pointer query
 *                       @a s_reference_monitor falls back to
 * @param surface        Surface to resolve a monitor on
 * @param client         Client being placed, or @c NULL to skip both
 *                       searches and go straight to the configured
 *                       policy
 * @param monitor_policy Fallback strategy when no related client is
 *                       found
 *
 * @return The resolved monitor's geometry
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       this desktop
 *
 * @see @a client_group_transient_anchor in @c cmds/client/transient.c
 */
static monitor_td s_reference_monitor_for_client(const wm_td *wm,
        surface_td *surface, const client_td *client,
        enum config_placement_monitor_e monitor_policy)
{
    const client_td *anchor = NULL;

    if (client != NULL) {
        anchor = client->transient_parent;
        if (anchor == NULL && client->is_transient_for_group) {
            anchor = client_group_transient_anchor(client);
        }
    }

    if (anchor == NULL && client != NULL) {
        xcb_window_t leader = client_group_leader(client);

        if (leader != XCB_WINDOW_NONE) {
            desktop_td *desktop =
                surface_desktop_get(surface, surface->desktop_cur);

            if (desktop != NULL && desktop->clients != NULL) {
                void *elem;

                ohtbl_foreach(desktop->clients, elem) {
                    client_td *const sibling = (client_td *) elem;

                    if (sibling != NULL && sibling != client &&
                            client_group_leader(sibling) == leader &&
                            sibling->properties.state !=
                                (uint16_t) CLIENT_STATE_ICONIFIED) {
                        anchor = sibling;
                        break;
                    }
                }
            } /* ! if (!desktop) */
        } /* ! if (leader) */
    }

    if (anchor != NULL) {
        monitor_td result = surface_monitor_for_point(surface,
                anchor->layout.geometry.cur.pos);

        if (result.w > 0u && result.h > 0u) {
            return result;
        }
    }

    return s_reference_monitor(wm, surface, monitor_policy);
}


/**
 * @brief Center a client over its ICCCM §4.1.2.6 @c WM_TRANSIENT_FOR
 *        parent, clamped to that parent's monitor, and configure its
 *        window
 *
 * On success, this fully places the client (configures its window and
 * updates its stored geometry) and returns @c true, so
 * @a place_window_apply has nothing further to do.
 *
 * @param wm      Window manager state
 * @param surface Surface @p client is on
 * @param client  Client being placed
 * @param wa      Surface-wide workarea, for the fallback monitor
 *                clip below
 *
 * @return @c true if @p client was transient and got placed here
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients (see @c lookup_find_client)
 *
 * @see A no-op, returning @c false, when @p client is not transient for
 *      anything, or its declared parent's geometry could not be
 *      resolved at all (neither an already-managed client entry nor
 *      a raw @a xcb_get_geometry reply
 */
static bool s_place_window_transient_centered(const wm_td *wm,
        surface_td *surface, client_td *client,
        struct geometry_s wa)
{
    uint32_t fw;
    uint32_t fh;
    int32_t new_x = 0;
    int32_t new_y = 0;
    bool placed_as_transient = false;
    xcb_window_t target;
    client_td *parent;
    struct geometry_s t_wa;
    struct dimensions_s screen;
    struct dimensions_s t_sz;
    xcb_connection_t *connection = wm_connection(wm);

    if (client->transient_for == XCB_WINDOW_NONE) {
        return false;
    }

    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    parent = NULL;

    /* ICCCM: 'WM_TRANSIENT_FOR' set to the root window means this
     * dialog is transient for its whole application group, not one
     * specific window ("Window Managers should decide" how to handle
     * this, per the spec's wording); prefer
     * centering over whichever currently-mapped sibling shares the
     * same group leader as this client, falling through to the
     * ordinary geometry-based fallback further below (which, for the
     * root window specifically, ends up centering on screen) when no
     * such sibling is currently mapped. */
    if (client->transient_for == surface->screen->root) {
        xcb_window_t leader = client_group_leader(client);

        if (leader != XCB_WINDOW_NONE) {
            desktop_td *desktop =
                surface_desktop_get(surface, surface->desktop_cur);

            if (desktop != NULL && desktop->clients != NULL) {
                void *elem;

                ohtbl_foreach(desktop->clients, elem) {
                    client_td *const sibling = (client_td *) elem;

                    if (sibling != client &&
                            client_group_leader(sibling) == leader &&
                            sibling->properties.state !=
                                (uint16_t) CLIENT_STATE_ICONIFIED) {
                        parent = sibling;
                        break;
                    }
                }
            }
        } /* ! if (leader) */
    }

    /* Prefer the WM's stored frame geometry over
     * 'xcb_get_geometry': after reparenting the parent's inner
     * window lives inside the frame, so 'xcb_get_geometry' would
     * return its position relative to the frame (left, top); not
     * the frame's root-relative screen position.  Using the stored
     * geometry correctly centers the dialog wherever the parent
     * window is on screen. */
    if (parent == NULL) {
        parent = lookup_find_client(wm_surfaces(wm),
                client->transient_for, NULL, NULL);
    }
    if (parent != NULL) {
        int32_t px = parent->layout.geometry.cur.pos.x;
        int32_t py = parent->layout.geometry.cur.pos.y;
        uint32_t pw = parent->layout.geometry.cur.dim.w;
        uint32_t ph = parent->layout.geometry.cur.dim.h;

        new_x = px + ((int32_t) pw - (int32_t) fw) / 2;
        new_y = py + ((int32_t) ph - (int32_t) fh) / 2;
        placed_as_transient = true;
    } else {
        /* Parent not yet managed (or unmanaged window): fall back
         * to 'xcb_get_geometry' on the declared transient-for
         * window */
        xcb_get_geometry_cookie_t pgc;
        xcb_get_geometry_reply_t *pgr;
        pgc = xcb_get_geometry(connection, client->transient_for);
        pgr = xcb_get_geometry_reply(connection, pgc, NULL);
        if (pgr != NULL) {
            new_x = (int32_t) pgr->x +
                    ((int32_t) pgr->width - (int32_t) fw) / 2;
            new_y = (int32_t) pgr->y +
                    ((int32_t) pgr->height - (int32_t) fh) / 2;
            free(pgr);
            placed_as_transient = true;
        }
    }

    if (!placed_as_transient) {
        return false;
    }

    /* Resolved from the dialog's proposed center, not the
     * pointer: it is meant to sit with its parent, wherever
     * that is, regardless of where the pointer happens to be
     * right now. */
    screen.w = surface->properties.dim.w;
    screen.h = surface->properties.dim.h;
    s_clip_to_monitor(surface, &wa, &screen,
            surface_monitor_for_point(surface,
                    (struct position_s) {
                        new_x + (int32_t) (fw / 2u),
                        new_y + (int32_t) (fh / 2u) }),
            &t_wa, &t_sz);

    if (new_x < t_wa.pos.x) { new_x = t_wa.pos.x; }
    if (new_y < t_wa.pos.y) { new_y = t_wa.pos.y; }
    if ((uint32_t) new_x + fw > t_sz.w) {
        new_x = (t_sz.w > fw) ? (int32_t) (t_sz.w - fw) : t_wa.pos.x;
    }
    if ((uint32_t) new_y + fh > t_sz.h) {
        new_y = (t_sz.h > fh) ? (int32_t) (t_sz.h - fh) : t_wa.pos.y;
    }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame : client->window;
    xcb_configure_window(connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {
                (uint32_t) new_x,
                (uint32_t) new_y
            });
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    return true;
}


/**
 * @brief Resolve the workarea and monitor-clipped bounds a placement
 *        calculation needs
 *
 * Shared by @a place_window_apply and @a place_window_apply_cascade so
 * both compute the exact same workarea and monitor bounds for a given
 * client.
 *
 * @param wm         Window manager instance
 * @param surface    Surface the client lives on
 * @param client     Client being placed
 * @param out_wa     Resolved workarea, unclipped to any single
 *                   monitor
 * @param out_mon_wa Workarea, clipped to the reference monitor
 * @param out_mon_sz Screen dimensions, clipped to the reference
 *                   monitor
 *
 * @note Complexity: @e O(1)
 */
static void s_place_workarea(const wm_td *wm, surface_td *surface,
        const client_td *client,
        struct geometry_s *out_wa, struct geometry_s *out_mon_wa,
        struct dimensions_s *out_mon_sz)
{
    struct dimensions_s screen;
    const desktop_td *desktop;

    screen.w = surface->properties.dim.w;
    screen.h = surface->properties.dim.h;

    desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (desktop != NULL && desktop->workarea.dim.w > 0u &&
            desktop->workarea.dim.h > 0u) {
        *out_wa = desktop->workarea;
    } else {
        out_wa->pos.x = 0;
        out_wa->pos.y = 0;
        out_wa->dim = screen;
    }

    s_clip_to_monitor(surface, out_wa, &screen,
            s_reference_monitor_for_client(wm, surface, client,
                    wm_config(wm)->base.windows.monitor_policy),
            out_mon_wa, out_mon_sz);
}


/**
 * @brief Apply gravity, clamp to the workarea, and move the client to
 *        its final resolved position
 *
 * Shared final step of every placement policy: adjusts for window
 * gravity, clamps so the title bar never ends up above the workarea or
 * the physical screen edge, then issues the actual @c ConfigureWindow
 *
 * @param wm      Window manager instance
 * @param surface Surface the client lives on
 * @param client  Client being placed
 * @param wa_pos  Workarea origin, unclipped to any single monitor
 * @param new_pos Policy-resolved position, before gravity/clamping
 *
 * @note Complexity: @e O(1)
 */
static void s_place_window_finalize(const wm_td *wm,
        const surface_td *surface, client_td *client,
        struct position_s wa_pos, struct position_s new_pos)
{
    xcb_window_t target;
    xcb_connection_t *connection = wm_connection(wm);

    s_place_window_apply_gravity(surface, client,
            &new_pos.x, &new_pos.y);

    /* Final safety: gravity adjustments must not push the title bar
     * above the workarea top or above the physical screen edge */
    if (new_pos.y < wa_pos.y) { new_pos.y = wa_pos.y; }
    if (new_pos.x < wa_pos.x) { new_pos.x = wa_pos.x; }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    xcb_configure_window(connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) new_pos.x,
                (uint32_t) new_pos.y});
    client->layout.geometry.cur.pos = new_pos;
}


/**
 * @brief Shared state one @a place_window_smart call threads through
 *        every candidate it tests
 */
struct s_place_window_smart_ctx_s {
    const desktop_td *desktop;    /**< Desktop @c client is placed on */
    /** Client being placed, excluded from the overlap checks */
    const client_td *skip_client;
    uint32_t fw;            /**< Client's frame width */
    uint32_t fh;            /**< Client's frame height */

    const struct geometry_s *tray_rect; /**< Systray rectangle to also
                                             avoid, or @c NULL */

    int32_t min_x;          /**< Leftmost a candidate corner may sit */
    int32_t min_y;          /**< Topmost a candidate corner may sit */
    int32_t max_x;          /**< Rightmost a candidate corner may sit */
    int32_t max_y;          /**< Bottommost a corner may sit */
    int32_t bound_right;    /**< Workarea's physical right edge */
    int32_t bound_bottom;   /**< Workarea's physical bottom edge */
    int32_t center_x;       /**< Workarea center X, tie-break ref */
    int32_t center_y;       /**< Workarea center Y, tie-break ref */
    int32_t best_x;         /**< Best candidate found so far, X */
    int32_t best_y;         /**< Best candidate found so far, Y */
    uint64_t best_cost;     /**< Lowest overlap cost found so far */
    uint64_t best_area;     /**< Largest free area found so far */
    bool has_free_rect;     /**< Whether any candidate so far actually
                                 fit @c client without overlapping
                                 anything */
};


/**
 * @brief Resolve the systray's current on-screen rectangle for one
 *        placement decision, when @c systray.avoid-overlap applies
 *
 * A live rectangle is fetched here, rather than trusting a stale one
 * cached earlier, since the tray can move, resize, or disappear between
 * one placement decision and the next.
 *
 * @param config   Window manager configuration
 * @param surface  Surface the placement decision is for
 * @param out_geom Receives the systray's rectangle when resolved;
 *                 untouched otherwise
 *
 * @return @p out_geom, or @c NULL when @c systray.avoid-overlap does
 *         not apply (see its doc comment, config.h), or the
 *         systray has no on-screen rectangle to report right now
 *
 * @note Complexity: @e O(1)
 */
static const struct geometry_s
    *s_place_window_smart_resolve_tray_rect(const config_td *config,
            const surface_td *surface, struct geometry_s *out_geom)
{
    if (config->base.systray.avoid_overlap &&
            !config->base.systray.reserve_space &&
            systray_get_geometry(surface, out_geom)) {
        return out_geom;
    }
    return NULL;
}


/**
 * @brief Test one candidate top-left corner, clamping it into the
 *        workarea first, keeping it if it beats whichever free
 *        rectangle or overlap cost @p ctx found so far
 *
 * @param ctx Placement context; @c best_x / @c best_y / @c best_cost /
 *            @c best_area / @c has_free_rect updated in place
 * @param x   Candidate left coordinate, not yet clamped
 * @param y   Candidate top coordinate, not yet clamped
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @c ctx->desktop (the cost of one @a s_free_rect_grow /
 *       @a s_score_window_pos pair)
 */
static void s_place_window_smart_test_candidate(
        struct s_place_window_smart_ctx_s *ctx, int32_t x, int32_t y)
{
    uint32_t free_w;
    uint32_t free_h;
    uint64_t cost;

    if (x < ctx->min_x) { x = ctx->min_x; }
    if (x > ctx->max_x) { x = ctx->max_x; }
    if (y < ctx->min_y) { y = ctx->min_y; }
    if (y > ctx->max_y) { y = ctx->max_y; }

    s_free_rect_grow(ctx->desktop, ctx->skip_client, x, y,
            ctx->bound_right, ctx->bound_bottom, ctx->tray_rect,
            &free_w, &free_h);
    if (free_w >= ctx->fw && free_h >= ctx->fh) {
        uint64_t area = (uint64_t) free_w * (uint64_t) free_h;

        if (area > ctx->best_area) {
            ctx->best_area = area;
            ctx->best_x = x + (int32_t) ((free_w - ctx->fw) / 2u);
            ctx->best_y = y + (int32_t) ((free_h - ctx->fh) / 2u);
            ctx->has_free_rect = true;
        }
    }
    cost = s_score_window_pos(ctx->desktop, ctx->skip_client, x, y,
            ctx->fw, ctx->fh, ctx->tray_rect,
            ctx->center_x, ctx->center_y);
    if (cost < ctx->best_cost) {
        ctx->best_cost = cost;
        if (!ctx->has_free_rect) {
            ctx->best_x = x;
            ctx->best_y = y;
        }
    }
}


/**
 * @brief Find a non-overlapping smart position for a newly mapped
 *        client, centered inside the largest genuinely free area
 *        found on the current desktop
 *
 * Tests a bounded set of candidate top-left corners (the workarea
 * center, its four corners, every edge of every visible client
 * already on the desktop, and, when @c systray.avoid-overlap applies
 * (see below), every edge of the tray too), grows the real free
 * rectangle anchored at each one (@a s_free_rect_grow), and keeps the
 * largest.  The client lands centered inside that free rectangle: the
 * breathing room around it comes from how much real free space exists
 * there, not from any fixed margin.  Falls back to whichever candidate
 * has the least overlap when the desktop is too full for any
 * candidate to fit the client at all.
 *
 * The systray, not a real client, is treated as one more obstacle
 * alongside every visible client above, and its edges are tested as
 * candidate anchors the same way every client's edges already are, when
 * @c systray.avoid-overlap is @c true and @c systray.reserve-space is
 * @c false (see either one's comment in @c config.h): both matter
 * equally, since testing the tray's edges as candidates without also
 * shrinking against the tray itself would let a candidate anchored
 * right at its corner overlap it outright, and shrinking against it
 * without testing its edges as candidates would leave real free space
 * sitting right next to the tray untested, unable to ever be found
 * (this second half is what actually went missing at first.  A corner
 * that used to be a genuinely productive candidate, workarea (0, 0)
 * with the tray docked there by default, collapses to zero free area
 * once the tray shrinks against it, and nothing replaced it as
 * a candidate anchored at the tray's edge instead, until this).
 * Fetched fresh from @a systray_get_geometry for this one placement
 * decision, then passed to every @a s_free_rect_grow /
 * @a s_score_window_pos call the same way @p desktop's clients already
 * are.  Affects placement scoring only, nothing about the tray becoming
 * movable, iconifiable, or otherwise actable on the way a real window
 * is.
 *
 * @param wm      Pointer to the window manager singleton
 * @param surface Pointer to the surface where the client will appear
 * @param client  Pointer to the client being placed
 * @param out_x   Output pointer for the selected X coordinate
 * @param out_y   Output pointer for the selected Y coordinate
 *
 * @return @c true if a position was found, @c false otherwise
 *
 * @note Complexity: @e O(n^3) worst case, where @e n is the number of
 *       clients on the current desktop (@e n candidates, each scored by
 *       @a s_free_rect_grow's @e O(n^2))
 */
bool place_window_smart(const wm_td *wm,
        surface_td *surface, client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y)
{
    desktop_td *desktop;
    struct geometry_s wa;
    struct geometry_s mon_wa;
    struct dimensions_s mon_sz;
    struct geometry_s tray_geom;
    struct s_place_window_smart_ctx_s ctx;
    const xcb_connection_t *connection = wm_connection(wm);
    const config_td *config = wm_config(wm);

    if (surface == NULL || client == NULL ||
            out_x == NULL || out_y == NULL || wm == NULL ||
            connection == NULL || config == NULL) {
        return false;
    }

    desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (desktop == NULL) {
        return false;
    }

    /* Resolves the workarea (falling back to the full surface
     * dimensions when none is set) and clips it to whichever physical
     * monitor 'windows.placement.monitor' resolves to, on a surface
     * made of more than one (the common case of several monitors
     * sharing one combined X screen): a new window should land within
     * one monitor, not be scored against the whole combined area, which
     * could place it straddling the seam between two of them.
     * 'wa' (unclipped) is only needed because 's_place_workarea'
     * requires somewhere to write it; every candidate below is tested
     * against 'mon_wa' instead. */
    s_place_workarea(wm, surface, client, &wa, &mon_wa, &mon_sz);

    ctx.desktop = desktop;
    ctx.skip_client = client;
    ctx.fw = client->layout.geometry.cur.dim.w;
    ctx.fh = client->layout.geometry.cur.dim.h;
    ctx.tray_rect = s_place_window_smart_resolve_tray_rect(config,
            surface, &tray_geom);

    /* Candidate range keeps the top-left corner inside the workarea;
     * 'bound_right'/'bound_bottom' are the workarea's physical edges
     * instead, used by 's_free_rect_grow' to grow a free rectangle as
     * far as it genuinely goes, not just as far as a top-left corner
     * could still sit. */
    ctx.min_x = mon_wa.pos.x;
    ctx.min_y = mon_wa.pos.y;
    ctx.max_x = (mon_wa.dim.w > ctx.fw)
        ? mon_wa.pos.x + (int32_t) (mon_wa.dim.w - ctx.fw)
        : mon_wa.pos.x;
    ctx.max_y = (mon_wa.dim.h > ctx.fh)
        ? mon_wa.pos.y + (int32_t) (mon_wa.dim.h - ctx.fh)
        : mon_wa.pos.y;
    ctx.bound_right = mon_wa.pos.x + (int32_t) mon_wa.dim.w;
    ctx.bound_bottom = mon_wa.pos.y + (int32_t) mon_wa.dim.h;

    /* Workarea center used as the distance tie-breaker reference */
    ctx.center_x = mon_wa.pos.x + (int32_t) (mon_wa.dim.w / 2u);
    ctx.center_y = mon_wa.pos.y + (int32_t) (mon_wa.dim.h / 2u);

    ctx.best_x = mon_wa.pos.x;
    ctx.best_y = mon_wa.pos.y;
    ctx.best_cost = UINT64_MAX;
    ctx.best_area = 0u;
    ctx.has_free_rect = false;

    /* Try one candidate top-left corner at a time: the centered seed
     * first (so an empty desktop still lands the first window in the
     * middle of the screen), every corner of the workarea itself, the
     * systray's edges when 'systray.avoid-overlap' applies (see
     * 'ctx.tray_rect''s comment,
     * 's_place_window_smart_resolve_tray_rect'), then every edge of
     * every visible client already on this desktop.  A genuinely free
     * rectangle, whenever one exists, always has at least one edge
     * touching either another obstacle's edge or the workarea's
     * boundary, so these candidates are enough to find it without
     * testing a whole grid of positions in between two obstacles where
     * nothing changes.  Each candidate keeps whichever free rectangle
     * found so far is largest ('s_place_window_smart_test_candidate'):
     * the window ends up centered inside that real free space, not
     * pinned to whichever corner happened to be tried first, so the
     * breathing room around it comes from the free space itself rather
     * than from any fixed margin. */
    s_place_window_smart_test_candidate(&ctx,
            ctx.center_x - (int32_t) (ctx.fw / 2u),
            ctx.center_y - (int32_t) (ctx.fh / 2u));

    for (int ci = 0; ci < 2; ++ci) {
        const int32_t corner_x = (ci == 0) ? ctx.min_x : ctx.max_x;

        for (int cj = 0; cj < 2; ++cj) {
            const int32_t corner_y = (cj == 0) ? ctx.min_y : ctx.max_y;

            s_place_window_smart_test_candidate(&ctx,
                    corner_x, corner_y);
        } /* ! for (cj) */
    } /* ! for (ci) */

    /* The tray, not iterated per client since it is a single fixed
     * obstacle for this whole placement decision (unlike every client
     * edge below, tested once per visible client): without this, the
     * corner candidate that would otherwise land exactly on the
     * tray's corner collapses to zero free area (that same check
     * inside 's_free_rect_shrink_against' above), and no replacement
     * candidate anchored at the tray's edge ever gets tried in its
     * place, so real free space sitting right next to the tray goes
     * completely untested. */
    if (ctx.tray_rect != NULL) {
        const int32_t tx = ctx.tray_rect->pos.x;
        const int32_t ty = ctx.tray_rect->pos.y;
        const int32_t tw = (int32_t) ctx.tray_rect->dim.w;
        const int32_t th = (int32_t) ctx.tray_rect->dim.h;
        const int32_t edge_x[4] = {
            tx + tw, tx - (int32_t) ctx.fw, tx, tx
        };
        const int32_t edge_y[4] = {
            ty, ty, ty + th, ty - (int32_t) ctx.fh
        };

        for (int ei = 0; ei < 4; ++ei) {
            s_place_window_smart_test_candidate(&ctx,
                    edge_x[ei], edge_y[ei]);
        } /* ! for (ei) */
    }

    if (desktop->stacking != NULL && cdlist_size(desktop->stacking) != 0u) {
        cdlist_item_td *node = cdlist_head(desktop->stacking);
        const cdlist_item_td *initial = node;

        if (node != NULL) {
            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);

                if (other != NULL && other != client &&
                        !(other->properties.flags & CLIENT_FLAG_HIDDEN) &&
                        !client_is_locked(other) &&
                        other->properties.state !=
                            (uint16_t) CLIENT_STATE_ICONIFIED) {
                    const int32_t ox = other->layout.geometry.cur.pos.x;
                    const int32_t oy = other->layout.geometry.cur.pos.y;
                    const int32_t ow =
                        (int32_t) other->layout.geometry.cur.dim.w;
                    const int32_t oh =
                        (int32_t) other->layout.geometry.cur.dim.h;
                    const int32_t edge_x[4] = {
                        ox + ow, ox - (int32_t) ctx.fw, ox, ox
                    };
                    const int32_t edge_y[4] = {
                        oy, oy, oy + oh, oy - (int32_t) ctx.fh
                    };

                    for (int ei = 0; ei < 4; ++ei) {
                        s_place_window_smart_test_candidate(&ctx,
                                edge_x[ei], edge_y[ei]);
                    } /* ! for (ei) */
                } /* ! if (!other) */
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        } /* ! if (!node) */
    }

    LOGGER_DEBUG("Smart-placed window (pos=%+d%+d, size=%ux%u," \
            " free-rect=%s, free-area=%llu, tray=%s, wa-pos=%+d%+d," \
            " wa-size=%ux%u)",
            ctx.best_x, ctx.best_y, ctx.fw, ctx.fh,
            (ctx.has_free_rect) ? "yes" : "no",
            (unsigned long long) ctx.best_area,
            (ctx.tray_rect != NULL) ? "yes" : "no",
            mon_wa.pos.x, mon_wa.pos.y, mon_wa.dim.w, mon_wa.dim.h);

    *out_x = ctx.best_x;
    *out_y = ctx.best_y;
    return true;
}


/* Place the client following the cascade policy, unconditionally */
void place_window_apply_cascade(const wm_td *wm,
        surface_td *surface, client_td *client)
{
    const uint32_t cascade_step = 24u;
    uint32_t max_steps;
    uint32_t fw;
    uint32_t fh;
    struct geometry_s wa;
    struct geometry_s mon_wa;
    struct dimensions_s mon_sz;
    int32_t new_x;
    int32_t new_y;

    if (wm == NULL || wm_config(wm) == NULL ||
            surface == NULL || client == NULL) {
        return;
    }

    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    s_place_workarea(wm, surface, client, &wa, &mon_wa, &mon_sz);

    max_steps = (mon_sz.w > fw) ? (mon_sz.w - fw) / cascade_step : 1u;
    if (mon_sz.h > fh) {
        uint32_t my = (mon_sz.h - fh) / cascade_step;

        if (my < max_steps) {
            max_steps = my;
        }
    }

    if (max_steps == 0u) {
        max_steps = 1u;
    }

    /* Cascade starts at the workarea origin, not at (0, 0), so the
     * title bar is never hidden behind a panel or dock */
    new_x = mon_wa.pos.x +
        (int32_t) ((s_cascade_seq % max_steps) * cascade_step);
    new_y = mon_wa.pos.y +
        (int32_t) ((s_cascade_seq % max_steps) * cascade_step);
    s_cascade_seq++;

    s_place_window_finalize(wm, surface, client, wa.pos,
            (struct position_s) { new_x, new_y });
}


/* Apply the configured placement policy to a newly mapped client */
void place_window_apply(const wm_td *wm,
        surface_td *surface, client_td *client)
{
    const uint32_t cascade_step = 24u;
    xcb_query_pointer_cookie_t pointer_cookie;
    struct dimensions_s screen;
    uint32_t fw;
    uint32_t fh;
    struct position_s wa_pos;
    struct dimensions_s wa_dim;
    struct geometry_s mon_wa;
    struct dimensions_s mon_sz;
    struct geometry_s wa_geom;
    int32_t new_x;
    int32_t new_y;
    enum config_placement_policy_e policy;
    desktop_td *desktop;
    xcb_window_t leader;
    bool placed_as_sibling;
    bool ignore_junk_origin_hint;
    xcb_connection_t *connection = wm_connection(wm);
    config_td *config = wm_config(wm);

    if (wm == NULL || config == NULL ||
            surface == NULL || client == NULL) {
        return;
    }

    screen.w = surface->properties.dim.w;
    screen.h = surface->properties.dim.h;
    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    policy = config->base.windows.placement_policy;
    leader = client_group_leader(client);
    placed_as_sibling = false;

    /* Determine the usable workarea (respects panel struts).
     * Fall back to the full screen dimensions when no workarea is
     * set */
    desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (desktop != NULL && desktop->workarea.dim.w > 0u &&
            desktop->workarea.dim.h > 0u) {
        wa_pos = desktop->workarea.pos;
        wa_dim = desktop->workarea.dim;
    } else {
        wa_pos.x = 0;
        wa_pos.y = 0;
        wa_dim = screen;
    }

    /* ICCCM 4.1.2.3: a client-requested position takes priority over
     * every placement policy below, including the transient-centering
     * convenience immediately following this: an explicit position
     * request is the client's most specific, deliberate statement
     * of where it wants to appear, ahead of any convenience default
     * this window manager would otherwise pick on its behalf.
     *
     * Exception: a transient window (one with 'WM_TRANSIENT_FOR' set)
     * requesting exactly (0, 0) is not honored here.  In practice
     * this combination is essentially never a deliberate placement
     * choice on a dialog's part; it is toolkit boilerplate left over
     * from a default 'PPosition'/'USPosition' hint nobody meant to
     * set to a specific value, and honoring it verbatim pins every
     * such dialog to the screen's top-left corner instead of the
     * transient-centered position ICCCM §4.1.2.6 recommends
     * immediately below.  A window that genuinely wants (0, 0) is
     * vanishingly rare among transients specifically, so this narrow
     * exception costs nothing for any other client while fixing that
     * one common, confusing case (a "save changes?"-style prompt
     * landing at the screen corner instead of over its parent). */
    ignore_junk_origin_hint = client->transient_for != XCB_WINDOW_NONE &&
        client->hints_icccm.size.req_pos.x == 0 &&
        client->hints_icccm.size.req_pos.y == 0;
    if (client->hints_icccm.size.has_position &&
            !ignore_junk_origin_hint) {
        s_place_window_finalize(wm, surface, client, wa_pos,
                client->hints_icccm.size.req_pos);
        return;
    }

    /* ICCCM §4.1.2.6: center transient dialogs over their parent */
    if (s_place_window_transient_centered(wm, surface, client,
                (struct geometry_s) { wa_pos, wa_dim })) {
        return;
    }

    /* Cluster windows of the same application: if another currently
     * mapped (non-iconified) client on this desktop shares the same
     * 'WM_CLIENT_LEADER'/'WM_HINTS' group as 'client', place the new
     * window offset from the group instead of running the configured
     * placement policy, so related windows stay visually together.
     * Gated by 'windows.placement.group-related' since not everyone
     * wants this: it can be turned off in configuration.
     *
     * The offset scales with how many siblings already exist (not just
     * whichever one 'ohtbl_foreach' happens to visit first) so that
     * a 3rd, 4th,... window of the same group each land at a further,
     * distinct position instead of every one of them after the 2nd
     * piling up on exactly the same spot as the 2nd. */
    if (leader != XCB_WINDOW_NONE && desktop != NULL &&
            desktop->clients != NULL &&
            config->base.windows.group_related) {
        void *elem;
        client_td *anchor = NULL;
        uint32_t sibling_count = 0u;

        ohtbl_foreach(desktop->clients, elem) {
            client_td *const sibling = (client_td *) elem;

            if (sibling == client ||
                    client_group_leader(sibling) != leader ||
                    sibling->properties.state ==
                        (uint16_t) CLIENT_STATE_ICONIFIED) {
                continue;
            }

            sibling_count++;
            anchor = sibling;
        }

        if (anchor != NULL) {
            new_x = anchor->layout.geometry.cur.pos.x +
                (int32_t) (cascade_step * sibling_count);
            new_y = anchor->layout.geometry.cur.pos.y +
                (int32_t) (cascade_step * sibling_count);
            placed_as_sibling = true;
        }
    }

    /* Clip the workarea (and the screen bound used for edge-clamping)
     * down to whichever physical monitor 'windows.placement.monitor'
     * resolves to, on a surface made of more than one: cascade,
     * centered, and under-mouse below all score or clamp against these
     * two, and without this they would do so against the whole combined
     * area instead of one monitor.  Falls back to the unclipped values
     * (identical to previous behavior) when there is only one monitor
     * or clipping would leave nothing to place into. */
    wa_geom.pos = wa_pos;
    wa_geom.dim = wa_dim;
    s_clip_to_monitor(surface, &wa_geom, &screen,
            s_reference_monitor_for_client(wm, surface, client,
                    config->base.windows.monitor_policy),
            &mon_wa, &mon_sz);

    if (placed_as_sibling) {
        struct geometry_s s_wa;
        struct dimensions_s s_sz;

        /* Resolved from the offset position next to the anchor sibling,
         * not the pointer: a related window is meant to stay with its
         * group, wherever that is. */
        s_clip_to_monitor(surface, &wa_geom, &screen,
                surface_monitor_for_point(surface,
                        (struct position_s) {
                            new_x + (int32_t) (fw / 2u),
                            new_y + (int32_t) (fh / 2u) }),
                &s_wa, &s_sz);

        /* Clamp to the workarea/screen the same way the cascade policy
         * below does, so a sibling near the edge does not push the new
         * window off-screen */
        if (new_x < s_wa.pos.x) { new_x = s_wa.pos.x; }
        if (new_y < s_wa.pos.y) { new_y = s_wa.pos.y; }
        if ((uint32_t) new_x + fw > s_sz.w) {
            new_x = (s_sz.w > fw) ? (int32_t) (s_sz.w - fw) : s_wa.pos.x;
        }
        if ((uint32_t) new_y + fh > s_sz.h) {
            new_y = (s_sz.h > fh) ? (int32_t) (s_sz.h - fh) : s_wa.pos.y;
        }
    } else if (policy == CONFIG_PLACEMENT_POLICY_SMART &&
            place_window_smart(wm, surface, client, &new_x, &new_y)) {
        /* Placement chosen by smart scan */
    } else if (policy == CONFIG_PLACEMENT_POLICY_CASCADE ||
            policy == CONFIG_PLACEMENT_POLICY_SMART) {
        place_window_apply_cascade(wm, surface, client);
        return;
    } else if (policy == CONFIG_PLACEMENT_POLICY_CENTERED) {
        /* Center on the workarea, not on the full screen. */
        new_x = mon_wa.pos.x +
            ((int32_t) mon_wa.dim.w - (int32_t) fw) / 2;
        new_y = mon_wa.pos.y +
            ((int32_t) mon_wa.dim.h - (int32_t) fh) / 2;
        if (new_x < mon_wa.pos.x) { new_x = mon_wa.pos.x; }
        if (new_y < mon_wa.pos.y) { new_y = mon_wa.pos.y; }
    } else if (policy == CONFIG_PLACEMENT_POLICY_UNDER_MOUSE) {
        xcb_query_pointer_reply_t *pointer_reply;

        pointer_cookie = xcb_query_pointer(connection,
                surface->screen->root);
        pointer_reply = xcb_query_pointer_reply(connection,
                pointer_cookie, NULL);

        if (pointer_reply == NULL) {
            LOGGER_WARNING("Failed to query pointer for" \
                    " 'under-mouse' placement; keeping" \
                    " X-server-assigned position", L_NARG);
            return;
        }

        new_x = (int32_t) pointer_reply->root_x - (int32_t) (fw / 2u);
        new_y = (int32_t) pointer_reply->root_y - (int32_t) (fh / 2u);
        if (new_x < mon_wa.pos.x) {
            new_x = mon_wa.pos.x;
        } else if ((uint32_t) new_x + fw > mon_sz.w) {
            new_x = (mon_sz.w > fw)
                ? (int32_t) (mon_sz.w - fw)
                : mon_wa.pos.x;
        }
        if (new_y < mon_wa.pos.y) {
            new_y = mon_wa.pos.y;
        } else if ((uint32_t) new_y + fh > mon_sz.h) {
            new_y = (mon_sz.h > fh)
                ? (int32_t) (mon_sz.h - fh)
                : mon_wa.pos.y;
        }

        free(pointer_reply);
    } else {
        /* "none" or unknown: keep the X-server-assigned position unless
         * the frame title bar would be hidden above the workarea top
         * (e.g., behind a panel) or above the physical screen edge */
        new_x = client->layout.geometry.cur.pos.x;
        new_y = client->layout.geometry.cur.pos.y;

        if (new_x < 0) {
            new_x = 0;
        }

        if (new_y < wa_pos.y) {
            new_y = wa_pos.y;
        }

        if (new_x == client->layout.geometry.cur.pos.x &&
                new_y == client->layout.geometry.cur.pos.y) {
            return;
        }
    }

    s_place_window_finalize(wm, surface, client, wa_pos,
            (struct position_s) { new_x, new_y });
}
