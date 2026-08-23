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
 * @brief Grow the largest obstacle-free rectangle whose top-left
 *        corner sits at a given point, extending right and down
 *
 * Starts from the full box between the corner and the placement
 * bounds, then repeatedly shrinks it on whichever side loses less
 * area whenever a visible client intrudes, until nothing intrudes
 * or the box collapses.  Repeating the whole scan (bounded by the
 * client count on @p desktop) instead of stopping after one pass
 * catches a client that only starts to intrude once an earlier
 * shrink has already pulled a boundary toward it.
 *
 * @param desktop     Desktop whose clients are checked against
 * @param skip_client Client to ignore (the one being placed)
 * @param x0          Corner X coordinate the rectangle grows from
 * @param y0          Corner Y coordinate the rectangle grows from
 * @param bound_x     Right placement bound the rectangle cannot
 *                     cross
 * @param bound_y     Bottom placement bound the rectangle cannot
 *                     cross
 * @param out_w       Receives the free width found, or 0 if the
 *                     corner itself sits inside another client
 * @param out_h       Receives the free height found, or 0 likewise
 *
 * @note Complexity: @e O(n^2) worst case, where @e n is the number
 *       of clients on @p desktop
 */
static void s_free_rect_grow(const desktop_td *desktop,
        const client_td *skip_client,
        int32_t x0, int32_t y0, int32_t bound_x, int32_t bound_y,
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

    for (guard = 0u; guard < guard_max; ++guard) {
        cdlist_item_td *node;
        const cdlist_item_td *initial;
        bool shrunk = false;

        if (desktop->stacking == NULL ||
                cdlist_size(desktop->stacking) == 0u) {
            break;
        }

        node = cdlist_head(desktop->stacking);
        initial = node;
        if (node == NULL) {
            break;
        }

        do {
            const client_td *other =
                (const client_td *) cdlist_data(node);

            if (other != NULL && other != skip_client &&
                    !(other->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    !client_is_locked(other) &&
                    other->properties.state !=
                        (uint16_t) CLIENT_STATE_ICONIFIED) {
                const int32_t ox1 = other->layout.geometry.cur.pos.x;
                const int32_t oy1 = other->layout.geometry.cur.pos.y;
                const int32_t ox2 = ox1 +
                    (int32_t) other->layout.geometry.cur.dim.w;
                const int32_t oy2 = oy1 +
                    (int32_t) other->layout.geometry.cur.dim.h;

                if (ox2 > x0 && ox1 < right &&
                        oy2 > y0 && oy1 < bottom) {
                    if (ox1 <= x0 && oy1 <= y0) {
                        /* Covers the corner itself: nothing free
                         * grows from here at all. */
                        *out_w = 0u;
                        *out_h = 0u;
                        return;
                    } else {
                        const bool right_ok = (ox1 > x0);
                        const bool bottom_ok = (oy1 > y0);
                        const int32_t via_right =
                            right_ok ? ox1 : right;
                        const int32_t via_bottom =
                            bottom_ok ? oy1 : bottom;
                        const uint64_t area_right = right_ok
                            ? (uint64_t) (via_right - x0) *
                                (uint64_t) (bottom - y0)
                            : 0u;
                        const uint64_t area_bottom = bottom_ok
                            ? (uint64_t) (right - x0) *
                                (uint64_t) (via_bottom - y0)
                            : 0u;

                        if (right_ok &&
                                (area_right >= area_bottom ||
                                    !bottom_ok)) {
                            right = via_right;
                        } else if (bottom_ok) {
                            bottom = via_bottom;
                        } else {
                            *out_w = 0u;
                            *out_h = 0u;
                            return;
                        }
                        shrunk = true;
                    }
                }
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);

        if (!shrunk) {
            break;
        }
    }

    if (right > x0 && bottom > y0) {
        *out_w = (uint32_t) (right - x0);
        *out_h = (uint32_t) (bottom - y0);
    }
}


/**
 * @brief Score a candidate window position against existing clients
 *
 * The overlap penalty itself (@a place_overlap_score,
 * @c policy/placement/score.h) is shared with
 * @c place_icon_apply's @c CONFIG_ICON_PLACEMENT_SMART search;
 * only the tie-breaker below is specific to window placement.  A
 * small distance-to-center penalty breaks ties in favor of the
 * workarea center, staying much smaller than any overlap penalty so
 * it only matters when two positions have equal overlap cost.
 *
 * @param desktop     Desktop whose clients are inspected
 * @param skip_client Client to ignore (the one being placed)
 * @param x           Candidate left coordinate
 * @param y           Candidate top coordinate
 * @param fw          Candidate width
 * @param fh          Candidate height
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
        int32_t center_x, int32_t center_y)
{
    uint64_t cost;
    int32_t dx;
    int32_t dy;

    cost = place_overlap_score(desktop, skip_client,
            (struct geometry_s) { { x, y }, { fw, fh } },
            (uint64_t) PLACE_SMART_WIN_COST_PER_WIN_PIXEL,
            (uint64_t) PLACE_SMART_WIN_COST_PER_ICON_PIXEL);

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
        const client_td *client, int32_t *restrict x, int32_t *restrict y)
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
 * covered by a strut): in either case the caller's unclipped
 * rectangle is already the right answer, not an error.
 *
 * @param surface     Surface the clip is against
 * @param wa          Workarea rectangle to clip
 * @param screen      Screen dimensions to clip alongside @p wa
 * @param monitor     Physical monitor to clip against
 * @param out_wa      Receives the clipped workarea
 * @param out_screen  Receives the clipped screen dimensions
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
 * Under @c CONFIG_PLACEMENT_MONITOR_PRIMARY, always returns @p
 * surface's primary monitor.  Under @c
 * CONFIG_PLACEMENT_MONITOR_POINTER (the default), queries the
 * pointer and returns whichever monitor it is currently over,
 * or a degenerate (zero-area) geometry if the query fails; passing
 * that on to @c s_clip_to_monitor is safe, since its intersection
 * against a zero-area rectangle is always empty, which is exactly
 * what makes it leave its inputs unclipped.
 *
 * @param wm             Window manager state, for the pointer query
 * @param surface        Surface to resolve a monitor on
 * @param monitor_policy Which strategy to resolve with
 *
 * @return The resolved monitor's geometry
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors
 *       on @p surface
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
 * A dialog should appear next to the window it belongs with, and a
 * fresh window from an application already running elsewhere should
 * appear next to that application, not wherever the pointer or the
 * primary monitor happens to be instead: checked in order, a
 * specific transient parent first (or, for a client transient for
 * the whole group per ICCCM §4.1.2.6, the resolved anchor; see
 * @a client_group_transient_anchor, cmds/client/transient.c), then
 * any currently-mapped sibling sharing the same group leader on this
 * same desktop.  Falls through to @a s_reference_monitor unchanged
 * whenever neither search finds a candidate, or the candidate found
 * resolves to a degenerate (zero-area) monitor.
 *
 * @param wm             Window manager state, for the pointer query
 *                        @a s_reference_monitor falls back to
 * @param surface        Surface to resolve a monitor on
 * @param client          Client being placed, or @c NULL to skip both
 *                        searches and go straight to the configured
 *                        policy
 * @param monitor_policy Fallback strategy when no related client is
 *                        found
 *
 * @return The resolved monitor's geometry
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       this desktop
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
            }
        }
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
 *        parent, clamped to that parent's monitor, and configure
 *        its window
 *
 * A no-op, returning @c false, when @p client is not transient for
 * anything, or its declared parent's geometry could not be resolved
 * at all (neither an already-managed client entry nor a raw
 * @c xcb_get_geometry reply).  On success, this fully places the
 * client (configures its window and updates its stored geometry) and
 * returns @c true, so @c place_window_apply has nothing further to do.
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
        }
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
 * Shared by @c place_window_apply and @c place_window_apply_cascade so
 * both compute the exact same workarea and monitor bounds for a given
 * client.
 *
 * @param wm          Window manager instance
 * @param surface     Surface the client lives on
 * @param client      Client being placed
 * @param out_wa      Resolved workarea, unclipped to any single
 *                    monitor
 * @param out_mon_wa  Workarea, clipped to the reference monitor
 * @param out_mon_sz  Screen dimensions, clipped to the reference
 *                    monitor
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
 * @brief Find a non-overlapping smart position for a newly mapped
 *        client, centered inside the largest genuinely free area
 *        found on the current desktop
 *
 * Tests a bounded set of candidate top-left corners (the workarea
 * center, its four corners, and every edge of every visible client
 * already on the desktop), grows the real free rectangle anchored
 * at each one (@a s_free_rect_grow), and keeps the largest.  The
 * client lands centered inside that free rectangle: the breathing
 * room around it comes from how much real free space exists there,
 * not from any fixed margin.  Falls back to whichever candidate has
 * the least overlap when the desktop is too full for any candidate
 * to fit the client at all.
 *
 * @param wm      Pointer to the window manager singleton
 * @param surface Pointer to the surface where the client will appear
 * @param client  Pointer to the client being placed
 * @param out_x   Output pointer for the selected X coordinate
 * @param out_y   Output pointer for the selected Y coordinate
 *
 * @return @c true if a position was found, @c false otherwise
 *
 * @note Complexity: @e O(n^3) worst case, where @e n is the number
 *       of clients on the current desktop (@e n candidates, each
 *       scored by @a s_free_rect_grow's @e O(n^2))
 */
bool place_window_smart(const wm_td *wm,
        surface_td *surface, client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y)
{
    desktop_td *desktop;
    int32_t wa_x;
    int32_t wa_y;
    uint32_t wa_w;
    uint32_t wa_h;
    uint32_t fw;
    uint32_t fh;
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
    int32_t bound_right;
    int32_t bound_bottom;
    int32_t center_x;
    int32_t center_y;
    int32_t best_x;
    int32_t best_y;
    uint64_t best_cost;
    uint64_t best_area;
    int32_t cx;
    int32_t cy;
    bool have_free_rect;
    monitor_td ref_monitor;
    struct geometry_s wa_geom;
    struct dimensions_s screen;
    struct dimensions_s unused_screen;
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

    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;

    /* Use workarea when available; fall back to full surface dimensions.
     * The workarea respects strut reservations from panels and docks. */
    if (desktop->workarea.dim.w > 0u && desktop->workarea.dim.h > 0u) {
        wa_x = desktop->workarea.pos.x;
        wa_y = desktop->workarea.pos.y;
        wa_w = desktop->workarea.dim.w;
        wa_h = desktop->workarea.dim.h;
    } else {
        wa_x = 0;
        wa_y = 0;
        wa_w = surface->properties.dim.w;
        wa_h = surface->properties.dim.h;
    }

    /* Clip the workarea down to whichever physical monitor
     * 'windows.placement.monitor' resolves to, on a surface made of
     * more than one (the common case of several monitors sharing one
     * combined X screen): a new window should land within one
     * monitor, not be scored against the whole combined area, which
     * could place it straddling the seam between two of them.  Falls
     * back to the unclipped workarea above when there is only one
     * monitor or clipping would leave nothing to place into (e.g., a
     * monitor entirely covered by a strut). */
    ref_monitor = s_reference_monitor_for_client(wm, surface, client,
            config->base.windows.monitor_policy);
    wa_geom.pos.x = wa_x;
    wa_geom.pos.y = wa_y;
    wa_geom.dim.w = wa_w;
    wa_geom.dim.h = wa_h;
    screen.w = wa_w;
    screen.h = wa_h;
    s_clip_to_monitor(surface, &wa_geom, &screen, ref_monitor,
            &wa_geom, &unused_screen);
    wa_x = wa_geom.pos.x;
    wa_y = wa_geom.pos.y;
    wa_w = wa_geom.dim.w;
    wa_h = wa_geom.dim.h;

    /* Candidate range keeps the top-left corner inside the workarea;
     * 'bound_right'/'bound_bottom' are the workarea's physical
     * edges instead, used below to grow a free rectangle as far as
     * it genuinely goes, not just as far as a top-left corner could
     * still sit. */
    min_x = wa_x;
    min_y = wa_y;
    max_x = (wa_w > fw) ? wa_x + (int32_t) (wa_w - fw) : wa_x;
    max_y = (wa_h > fh) ? wa_y + (int32_t) (wa_h - fh) : wa_y;
    bound_right = wa_x + (int32_t) wa_w;
    bound_bottom = wa_y + (int32_t) wa_h;

    /* Workarea center used as the distance tie-breaker reference */
    center_x = wa_x + (int32_t) (wa_w / 2u);
    center_y = wa_y + (int32_t) (wa_h / 2u);

    best_x = wa_x;
    best_y = wa_y;
    best_cost = UINT64_MAX;
    best_area = 0u;
    have_free_rect = false;

    /* Try one candidate top-left corner at a time: the centered seed
     * first (so an empty desktop still lands the first window in the
     * middle of the screen), every corner of the workarea itself,
     * then every edge of every visible client already on this
     * desktop.  A genuinely free rectangle, whenever one exists,
     * always has at least one edge touching either another client's
     * edge or the workarea's boundary, so these candidates are
     * enough to find it without testing a whole grid of positions in
     * between two clients where nothing changes.  For each corner,
     * grow the actual free rectangle anchored there
     * (@a s_free_rect_grow) and keep whichever one found so far is
     * largest: the window ends up centered inside that real free
     * space, not pinned to whichever corner happened to be tried
     * first, so the breathing room around it comes from the free
     * space itself rather than from any fixed margin. */
    cx = center_x - (int32_t) (fw / 2u);
    cy = center_y - (int32_t) (fh / 2u);
    if (cx < min_x) { cx = min_x; }
    if (cy < min_y) { cy = min_y; }
    if (cx > max_x) { cx = max_x; }
    if (cy > max_y) { cy = max_y; }

    {
        uint32_t free_w;
        uint32_t free_h;
        uint64_t cost;

        s_free_rect_grow(desktop, client, cx, cy,
                bound_right, bound_bottom, &free_w, &free_h);
        if (free_w >= fw && free_h >= fh) {
            uint64_t area = (uint64_t) free_w * (uint64_t) free_h;

            if (area > best_area) {
                best_area = area;
                best_x = cx + (int32_t) ((free_w - fw) / 2u);
                best_y = cy + (int32_t) ((free_h - fh) / 2u);
                have_free_rect = true;
            }
        }
        cost = s_score_window_pos(desktop, client, cx, cy, fw, fh,
                center_x, center_y);
        if (cost < best_cost) {
            best_cost = cost;
        }
    }

    {
        const int32_t corners_x[2] = { min_x, max_x };
        const int32_t corners_y[2] = { min_y, max_y };

        for (int ci = 0; ci < 2; ++ci) {
            for (int cj = 0; cj < 2; ++cj) {
                uint32_t free_w;
                uint32_t free_h;
                uint64_t cost;
                int32_t x = corners_x[ci];
                int32_t y = corners_y[cj];

                s_free_rect_grow(desktop, client, x, y,
                        bound_right, bound_bottom, &free_w, &free_h);
                if (free_w >= fw && free_h >= fh) {
                    uint64_t area = (uint64_t) free_w * (uint64_t) free_h;

                    if (area > best_area) {
                        best_area = area;
                        best_x = x + (int32_t) ((free_w - fw) / 2u);
                        best_y = y + (int32_t) ((free_h - fh) / 2u);
                        have_free_rect = true;
                    }
                }
                cost = s_score_window_pos(desktop, client, x, y,
                        fw, fh, center_x, center_y);
                if (cost < best_cost) {
                    best_cost = cost;
                    if (!have_free_rect) {
                        best_x = x;
                        best_y = y;
                    }
                }
            }
        }
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
                        ox + ow, ox - (int32_t) fw, ox, ox
                    };
                    const int32_t edge_y[4] = {
                        oy, oy, oy + oh, oy - (int32_t) fh
                    };

                    for (int ei = 0; ei < 4; ++ei) {
                        uint32_t free_w;
                        uint32_t free_h;
                        uint64_t cost;
                        int32_t x = edge_x[ei];
                        int32_t y = edge_y[ei];

                        if (x < min_x) { x = min_x; }
                        if (x > max_x) { x = max_x; }
                        if (y < min_y) { y = min_y; }
                        if (y > max_y) { y = max_y; }

                        s_free_rect_grow(desktop, client, x, y,
                                bound_right, bound_bottom,
                                &free_w, &free_h);
                        if (free_w >= fw && free_h >= fh) {
                            uint64_t area =
                                (uint64_t) free_w * (uint64_t) free_h;

                            if (area > best_area) {
                                best_area = area;
                                best_x = x +
                                    (int32_t) ((free_w - fw) / 2u);
                                best_y = y +
                                    (int32_t) ((free_h - fh) / 2u);
                                have_free_rect = true;
                            }
                        }
                        cost = s_score_window_pos(desktop, client, x, y,
                                fw, fh, center_x, center_y);
                        if (cost < best_cost) {
                            best_cost = cost;
                            if (!have_free_rect) {
                                best_x = x;
                                best_y = y;
                            }
                        }
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }
    }

    LOGGER_DEBUG("Smart-placed window (pos=%+d%+d, free-rect=%s," \
            " wa-pos=%+d%+d, wa-size=%ux%u)",
            best_x, best_y, have_free_rect ? "yes" : "no",
            wa_x, wa_y, wa_w, wa_h);

    *out_x = best_x;
    *out_y = best_y;
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
     * Fall back to the full screen dimensions when no workarea is set */
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
