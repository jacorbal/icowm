/**
 * @file policy/placement.c
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
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Utils includes */
#include <utils/geom.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <policy/internal.h>
#include <policy/placement.h>


/**
 * @brief Score a candidate window position against existing clients
 *
 * Iterates visible clients on @p desktop and accumulates an overlap
 * penalty weighted by intersection area.  A small distance-to-center
 * penalty breaks ties in favor of the workarea center.
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
    cdlist_item_td *node;
    uint64_t cost;
    int32_t dx;
    int32_t dy;

    cost = 0u;

    if (desktop != NULL && desktop->stacking != NULL &&
            cdlist_size(desktop->stacking) != 0u) {
        node = cdlist_head(desktop->stacking);
        if (node != NULL) {
            const cdlist_item_td *initial = node;

            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);
                if (other != NULL && other != skip_client &&
                        !(other->properties.flags & CLIENT_FLAG_HIDDEN)) {
                    if (other->properties.state !=
                            (uint16_t) CLIENT_STATE_ICONIFIED) {
                        /* Visible window: high overlap penalty */
                        uint32_t area = geom_intersection_area(x, y,
                                fw, fh,
                                other->layout.geometry.cur.pos.x,
                                other->layout.geometry.cur.pos.y,
                                other->layout.geometry.cur.dim.w,
                                other->layout.geometry.cur.dim.h);
                        cost += (uint64_t) SMART_WIN_COST_PER_WIN_PIXEL *
                            (uint64_t) area;
                    } else if (other->icon_window != 0u &&
                            other->is_icon_mapped &&
                            other->icon_x >= 0 && other->icon_y >= 0) {
                        /* Visible icon: lower overlap penalty */
                        uint32_t area = geom_intersection_area(x, y,
                                fw, fh,
                                (int32_t) other->icon_x,
                                (int32_t) other->icon_y,
                                (uint32_t) SMART_WIN_ICON_SIZE,
                                (uint32_t) SMART_WIN_ICON_SIZE);
                        cost += (uint64_t) SMART_WIN_COST_PER_ICON_PIXEL *
                            (uint64_t) area;
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        }
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
static void s_place_apply_gravity(const surface_td *surface,
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
 * covered by a strut): in either case the caller's own unclipped
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
 * that on to @c s_clip_to_monitor is safe, since its own intersection
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
        result = surface_monitor_for_point(surface, reply->root_x,
                reply->root_y);
        free(reply);
    }

    return result;
}


/**
 * @brief Center a client over its ICCCM §4.1.2.6 @c WM_TRANSIENT_FOR
 *        parent, clamped to that parent's own monitor, and configure
 *        its window
 *
 * A no-op, returning @c false, when @p client is not transient for
 * anything, or its declared parent's geometry could not be resolved
 * at all (neither an already-managed client entry nor a raw
 * @c xcb_get_geometry reply).  On success, this fully places the
 * client (configures its window and updates its stored geometry) and
 * returns @c true, so @c place_apply has nothing further to do.
 *
 * @param wm      Window manager state
 * @param surface Surface @p client is on
 * @param client  Client being placed
 * @param wa_x    Surface-wide workarea origin X, for the fallback
 *                monitor clip below
 * @param wa_y    Surface-wide workarea origin Y
 * @param wa_w    Surface-wide workarea width
 * @param wa_h    Surface-wide workarea height
 *
 * @return @c true if @p client was transient and got placed here
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients (see @c lookup_find_client)
 */
static bool s_place_transient_centered(const wm_td *wm,
        surface_td *surface, client_td *client,
        int32_t wa_x, int32_t wa_y, uint32_t wa_w, uint32_t wa_h)
{
    uint32_t fw;
    uint32_t fh;
    int32_t new_x = 0;
    int32_t new_y = 0;
    bool placed_as_transient = false;
    xcb_window_t target;
    client_td *parent;
    struct geometry_s wa;
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
     * this on their own, per the spec's own wording); prefer
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

    /* Resolved from the dialog's own proposed center, not the
     * pointer: it is meant to sit with its parent, wherever
     * that is, regardless of where the pointer happens to be
     * right now. */
    wa.pos.x = wa_x;
    wa.pos.y = wa_y;
    wa.dim.w = wa_w;
    wa.dim.h = wa_h;
    screen.w = surface->properties.dim.w;
    screen.h = surface->properties.dim.h;
    s_clip_to_monitor(surface, &wa, &screen,
            surface_monitor_for_point(surface,
                    new_x + (int32_t) (fw / 2u),
                    new_y + (int32_t) (fh / 2u)),
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
 * @brief Find a non-overlapping smart position for a newly mapped
 *        client
 *
 * Searches the current desktop from top-left to bottom-right using
 * a fixed grid step and returns the first position whose rectangle does
 * not overlap any currently visible client.
 *
 * @param wm      Pointer to the window manager singleton
 * @param surface Pointer to the surface where the client will appear
 * @param client  Pointer to the client being placed
 * @param out_x   Output pointer for the selected X coordinate
 * @param out_y   Output pointer for the selected Y coordinate
 *
 * @return @c true if a free position was found, @c false otherwise
 *
 * @note Complexity: @e O(g * n), where @e g is the number of grid
 *       positions tested and @e n is the number of clients on the
 *       current desktop
 */
static bool s_place_smart(const wm_td *wm,
        surface_td *surface, client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y)
{
    desktop_td *desktop;
    const uint32_t step = 24u;
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
    int32_t center_x;
    int32_t center_y;
    int32_t best_x;
    int32_t best_y;
    uint64_t best_cost;
    int32_t cx;
    int32_t cy;
    uint64_t cost;
    bool found;
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
    ref_monitor = s_reference_monitor(wm, surface,
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

    /* Candidate range keeps the window fully inside the workarea */
    min_x = wa_x;
    min_y = wa_y;
    max_x = (wa_w > fw) ? wa_x + (int32_t) (wa_w - fw) : wa_x;
    max_y = (wa_h > fh) ? wa_y + (int32_t) (wa_h - fh) : wa_y;

    /* Workarea center used as the distance tie-breaker reference */
    center_x = wa_x + (int32_t) (wa_w / 2u);
    center_y = wa_y + (int32_t) (wa_h / 2u);

    /* Seed with the centered position so an empty desktop still lands
     * the first window in the middle of the screen */
    cx = center_x - (int32_t) (fw / 2u);
    cy = center_y - (int32_t) (fh / 2u);
    if (cx < min_x) { cx = min_x; }
    if (cy < min_y) { cy = min_y; }
    if (cx > max_x) { cx = max_x; }
    if (cy > max_y) { cy = max_y; }

    best_x = cx;
    best_y = cy;
    best_cost = s_score_window_pos(desktop, client, cx, cy, fw, fh,
            center_x, center_y);
    found = (best_cost == 0u);

    /* Grid sweep: score every candidate and keep the minimum-cost one.
     * The first zero-cost candidate found terminates the search early. */
    for (int32_t y = min_y; y <= max_y && !found; y += (int32_t) step) {
        for (int32_t x = min_x;
                x <= max_x && !found;
                x += (int32_t) step) {
            cost = s_score_window_pos(desktop, client, x, y, fw, fh,
                    center_x, center_y);
            if (cost < best_cost) {
                best_cost = cost;
                best_x = x;
                best_y = y;
                found = (cost == 0u);
            }
        }

        /* Right-column guard: ensure 'max_x' is always evaluated */
        if (!found && max_x != min_x) {
            cost = s_score_window_pos(desktop, client, max_x, y, fw, fh,
                    center_x, center_y);
            if (cost < best_cost) {
                best_cost = cost;
                best_x = max_x;
                best_y = y;
                found = (cost == 0u);
            }
        }
    }

    /* Bottom-row guard: ensure 'max_y' is always evaluated */
    if (!found && max_y != min_y) {
        for (int32_t x = min_x;
                x <= max_x && !found;
                x += (int32_t) step) {
            cost = s_score_window_pos(desktop, client, x, max_y, fw, fh,
                    center_x, center_y);
            if (cost < best_cost) {
                best_cost = cost;
                best_x = x;
                best_y = max_y;
                found = (cost == 0u);
            }
        }

        if (!found) {
            cost = s_score_window_pos(desktop, client, max_x, max_y,
                    fw, fh, center_x, center_y);
            if (cost < best_cost) {
                best_cost = cost;
                best_x = max_x;
                best_y = max_y;
            }
        }
    }

    LOGGER_DEBUG("Smart-placed window (pos=%+d%+d, cost=%lu," \
            " wa-pos=%+d%+d, wa-size=%ux%u)",
            best_x, best_y, (unsigned long) best_cost,
            wa_x, wa_y, wa_w, wa_h);

    *out_x = best_x;
    *out_y = best_y;
    return true;
}


/* Apply the configured placement policy to a newly mapped client */
static uint32_t s_cascade_seq = 0;


/**
 * @brief Resolve the workarea and monitor-clipped bounds a placement
 *        calculation needs
 *
 * Shared by @c place_apply and @c place_apply_cascade so both compute
 * the exact same workarea and monitor bounds for a given client
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

    (void) client;
    s_clip_to_monitor(surface, out_wa, &screen,
            s_reference_monitor(wm, surface,
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
 * @param wa_x    Workarea X, unclipped to any single monitor
 * @param wa_y    Workarea Y, unclipped to any single monitor
 * @param new_x   Policy-resolved X position, before gravity/clamping
 * @param new_y   Policy-resolved Y position, before gravity/clamping
 *
 * @note Complexity: @e O(1)
 */
static void s_place_finalize(const wm_td *wm,
        const surface_td *surface, client_td *client,
        int32_t wa_x, int32_t wa_y, int32_t new_x, int32_t new_y)
{
    xcb_window_t target;
    xcb_connection_t *connection = wm_connection(wm);

    s_place_apply_gravity(surface, client, &new_x, &new_y);

    /* Final safety: gravity adjustments must not push the title bar
     * above the workarea top or above the physical screen edge */
    if (new_y < wa_y) { new_y = wa_y; }
    if (new_x < wa_x) { new_x = wa_x; }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    xcb_configure_window(connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) new_x, (uint32_t) new_y});
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
}


/* Place the client following the cascade policy, unconditionally */
void place_apply_cascade(const wm_td *wm,
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

    s_place_finalize(wm, surface, client, wa.pos.x, wa.pos.y, new_x, new_y);
}


/* Apply the configured placement policy to a newly mapped client */
void place_apply(const wm_td *wm,
        surface_td *surface, client_td *client)
{
    const uint32_t cascade_step = 24u;
    xcb_query_pointer_cookie_t pointer_cookie;
    struct dimensions_s screen;
    uint32_t fw;
    uint32_t fh;
    int32_t wa_x;
    int32_t wa_y;
    uint32_t wa_w;
    uint32_t wa_h;
    struct geometry_s mon_wa;
    struct dimensions_s mon_sz;
    struct geometry_s wa_geom;
    int32_t new_x;
    int32_t new_y;
    enum config_placement_policy_e policy;
    desktop_td *desktop;
    xcb_window_t leader;
    bool placed_as_sibling;
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
        wa_x = desktop->workarea.pos.x;
        wa_y = desktop->workarea.pos.y;
        wa_w = desktop->workarea.dim.w;
        wa_h = desktop->workarea.dim.h;
    } else {
        wa_x = 0;
        wa_y = 0;
        wa_w = screen.w;
        wa_h = screen.h;
    }

    /* ICCCM 4.1.2.3: a client-requested position takes priority over
     * every placement policy below, including the transient-centering
     * convenience immediately following this: an explicit position
     * request is the client's own most specific, deliberate statement
     * of where it wants to appear, ahead of any convenience default
     * this window manager would otherwise pick on its behalf. */
    if (client->size_hints.has_position) {
        s_place_finalize(wm, surface, client, wa_x, wa_y,
                client->size_hints.req_x, client->size_hints.req_y);
        return;
    }

    /* ICCCM §4.1.2.6: center transient dialogs over their parent */
    if (s_place_transient_centered(wm, surface, client, wa_x, wa_y,
                wa_w, wa_h)) {
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
     * centered, and under-mouse below all score or clamp against
     * these two, and without this they would do so against the whole
     * combined area instead of one monitor.  Falls back to the
     * unclipped values (identical to previous behavior) when there is
     * only one monitor or clipping would leave nothing to place
     * into. */
    wa_geom.pos.x = wa_x;
    wa_geom.pos.y = wa_y;
    wa_geom.dim.w = wa_w;
    wa_geom.dim.h = wa_h;
    s_clip_to_monitor(surface, &wa_geom, &screen,
            s_reference_monitor(wm, surface,
                    config->base.windows.monitor_policy),
            &mon_wa, &mon_sz);

    if (placed_as_sibling) {
        struct geometry_s s_wa;
        struct dimensions_s s_sz;

        /* Resolved from the offset position next to the anchor
         * sibling, not the pointer: a related window is meant to
         * stay with its group, wherever that is. */
        s_clip_to_monitor(surface, &wa_geom, &screen,
                surface_monitor_for_point(surface,
                        new_x + (int32_t) (fw / 2u),
                        new_y + (int32_t) (fh / 2u)),
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
            s_place_smart(wm, surface, client, &new_x, &new_y)) {
        /* Placement chosen by smart scan */
    } else if (policy == CONFIG_PLACEMENT_POLICY_CASCADE ||
            policy == CONFIG_PLACEMENT_POLICY_SMART) {
        place_apply_cascade(wm, surface, client);
        return;
    } else if (policy == CONFIG_PLACEMENT_POLICY_CENTERED) {
        /* Center on the workarea, not on the full screen. */
        new_x = mon_wa.pos.x + ((int32_t) mon_wa.dim.w - (int32_t) fw) / 2;
        new_y = mon_wa.pos.y + ((int32_t) mon_wa.dim.h - (int32_t) fh) / 2;
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
            new_x = (mon_sz.w > fw) ? (int32_t) (mon_sz.w - fw) : mon_wa.pos.x;
        }
        if (new_y < mon_wa.pos.y) {
            new_y = mon_wa.pos.y;
        } else if ((uint32_t) new_y + fh > mon_sz.h) {
            new_y = (mon_sz.h > fh) ? (int32_t) (mon_sz.h - fh) : mon_wa.pos.y;
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

        if (new_y < wa_y) {
            new_y = wa_y;
        }

        if (new_x == client->layout.geometry.cur.pos.x &&
                new_y == client->layout.geometry.cur.pos.y) {
            return;
        }
    }

    s_place_finalize(wm, surface, client, wa_x, wa_y, new_x, new_y);
}
