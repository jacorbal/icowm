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

/* Utils includes */
#include <utils/geom.h>

/* Default initial values */
#include <defs/wm.h>     /* WM_ICON_SQUARE_SIZE */

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
 * @brief Cost weights for the smart window placement scorer
 *
 * These constants define the relative penalty of overlapping a visible
 * window vs. overlapping an icon vs. being far from the workarea
 * center.  Overlap penalties are multiplied by the intersection area
 * (pixels), so even a 1-pixel overlap with a visible window is worth
 * thousands of distance units, ensuring non-overlapping positions are
 * always strongly preferred.
 */
#define SMART_WIN_COST_PER_WIN_PIXEL (8192u)
#define SMART_WIN_COST_PER_ICON_PIXEL (1024u)

/**
 * @brief Fallback icon dimension used by the window scorer when the
 *        exact icon size is not tracked in the client structure
 */
#define SMART_WIN_ICON_SIZE (WM_ICON_SQUARE_SIZE)

/**
 * @brief Cost weights for the smart icon placement scorer
 *
 * Overlap with any visible (non-iconified) window is penalized heavily.
 * The overflow-row penalty keeps icons compact near the preferred edge,
 * i.e., each row away from the edge adds a small, predictable cost.
 */
#define SMART_ICON_COST_PER_WIN_PIXEL (256u)
#define SMART_ICON_COST_PER_OVERFLOW_ROW (1u)


/**
 * @brief Score a candidate window position against existing clients
 *
 * Iterates visible clients on @p desktop and accumulates an overlap
 * penalty weighted by intersection area.  A small distance-to-centre
 * penalty breaks ties in favour of the workarea centre.
 *
 * @param desktop     Desktop whose clients are inspected
 * @param skip_client Client to ignore (the one being placed)
 * @param x           Candidate left coordinate
 * @param y           Candidate top coordinate
 * @param fw          Candidate width
 * @param fh          Candidate height
 * @param center_x    X coordinate of the workarea centre
 * @param center_y    Y coordinate of the workarea centre
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
    cdlist_item_td *initial;
    uint64_t cost;
    int32_t dx;
    int32_t dy;

    cost = 0u;

    if (desktop != NULL && desktop->stacking != NULL &&
            cdlist_size(desktop->stacking) != 0u) {
        node = cdlist_head(desktop->stacking);
        if (node != NULL) {
            initial = node;
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

    /* Secondary tie-breaker: Manhattan distance from workarea centre.
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
        const client_td *client, int32_t *x, int32_t *y)
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


/* Find the best-scoring smart position for a newly mapped client */
bool place_smart(wm_td *wm, surface_td *surface, client_td *client,
        int32_t *out_x, int32_t *out_y)
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

    (void) wm; /* reserved for future use */

    if (surface == NULL || client == NULL ||
            out_x == NULL || out_y == NULL) {
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

    /* Candidate range keeps the window fully inside the workarea */
    min_x = wa_x;
    min_y = wa_y;
    max_x = (wa_w > fw) ? wa_x + (int32_t) (wa_w - fw) : wa_x;
    max_y = (wa_h > fh) ? wa_y + (int32_t) (wa_h - fh) : wa_y;

    /* Workarea centre used as the distance tie-breaker reference */
    center_x = wa_x + (int32_t) (wa_w / 2u);
    center_y = wa_y + (int32_t) (wa_h / 2u);

    /* Seed with the centred position so an empty desktop still lands
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

    LOGGER_DEBUG("smart-place win: pos=(%d,%d) cost=%lu wa=(%d,%d %ux%u)",
            best_x, best_y, (unsigned long) best_cost,
            wa_x, wa_y, wa_w, wa_h);

    *out_x = best_x;
    *out_y = best_y;
    return true;
}


/* Apply the configured placement policy to a newly mapped client */
void place_apply(wm_td *wm, surface_td *surface, client_td *client)
{
    static uint32_t s_cascade_seq = 0;
    const uint32_t cascade_step = 24u;
    uint32_t max_steps;
    uint32_t my;
    xcb_query_pointer_cookie_t pointer_cookie;
    xcb_query_pointer_reply_t *pointer_reply;
    uint32_t sw;
    uint32_t sh;
    uint32_t fw;
    uint32_t fh;
    int32_t wa_x;
    int32_t wa_y;
    uint32_t wa_w;
    uint32_t wa_h;
    int32_t new_x;
    int32_t new_y;
    xcb_window_t target;
    enum config_placement_policy_e policy;
    desktop_td *desktop;

    if (wm == NULL || wm->config == NULL ||
            surface == NULL || client == NULL) {
        return;
    }

    sw = surface->properties.dim.w;
    sh = surface->properties.dim.h;
    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    policy = wm->config->base.windows.placement_policy;

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
        wa_w = sw;
        wa_h = sh;
    }

    /* ICCCM §4.1.2.6: center transient dialogs over their parent */
    if (client->transient_for != XCB_WINDOW_NONE) {
        bool placed_as_transient = false;

        /* Prefer the WM's stored frame geometry over
         * 'xcb_get_geometry': after reparenting the parent's inner
         * window lives inside the frame, so 'xcb_get_geometry' would
         * return its position relative to the frame (left, top); not
         * the frame's root-relative screen position.  Using the stored
         * geometry correctly centres the dialog wherever the parent
         * window is on screen. */
        client_td *parent = lookup_find_client(wm->surfaces,
                client->transient_for, NULL, NULL);
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
            pgc = xcb_get_geometry(wm->connection, client->transient_for);
            pgr = xcb_get_geometry_reply(wm->connection, pgc, NULL);
            if (pgr != NULL) {
                new_x = (int32_t) pgr->x +
                        ((int32_t) pgr->width - (int32_t) fw) / 2;
                new_y = (int32_t) pgr->y +
                        ((int32_t) pgr->height - (int32_t) fh) / 2;
                free(pgr);
                placed_as_transient = true;
            }
        }

        if (placed_as_transient) {
            if (new_x < 0) { new_x = 0; }
            if (new_y < wa_y) { new_y = wa_y; }
            if ((uint32_t) new_x + fw > sw) {
                new_x = (sw > fw) ? (int32_t) (sw - fw) : 0;
            }
            if ((uint32_t) new_y + fh > sh) {
                new_y = (sh > fh) ? (int32_t) (sh - fh) : wa_y;
            }

            target = (client_is_decorated(client) && client->frame != 0)
                ? client->frame : client->window;
            xcb_configure_window(wm->connection, target,
                    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                    (const uint32_t[]) {
                        (uint32_t) new_x,
                        (uint32_t) new_y
                    });
            client->layout.geometry.cur.pos.x = new_x;
            client->layout.geometry.cur.pos.y = new_y;
            return;
        }
    }

    if (policy == CONFIG_PLACEMENT_POLICY_SMART &&
            place_smart(wm, surface, client, &new_x, &new_y)) {
        /* Placement chosen by smart scan */
    } else if (policy == CONFIG_PLACEMENT_POLICY_CASCADE ||
            policy == CONFIG_PLACEMENT_POLICY_SMART) {
        max_steps = (sw > fw) ? (sw - fw) / cascade_step : 1u;
        if (sh > fh) {
            my = (sh - fh) / cascade_step;
            if (my < max_steps) {
                max_steps = my;
            }
        }

        if (max_steps == 0u) {
            max_steps = 1u;
        }

        /* Cascade starts at the workarea origin, not at (0, 0), so
         * the title bar is never hidden behind a panel or dock */
        new_x =
            wa_x + (int32_t) ((s_cascade_seq % max_steps) * cascade_step);
        new_y =
            wa_y + (int32_t) ((s_cascade_seq % max_steps) * cascade_step);
        s_cascade_seq++;
    } else if (policy == CONFIG_PLACEMENT_POLICY_CENTERED) {
        /* Centre on the workarea, not on the full screen. */
        new_x = wa_x + ((int32_t) wa_w - (int32_t) fw) / 2;
        new_y = wa_y + ((int32_t) wa_h - (int32_t) fh) / 2;
        if (new_x < wa_x) { new_x = wa_x; }
        if (new_y < wa_y) { new_y = wa_y; }
    } else if (policy == CONFIG_PLACEMENT_POLICY_UNDER_MOUSE) {
        pointer_cookie = xcb_query_pointer(wm->connection,
                surface->screen->root);
        pointer_reply = xcb_query_pointer_reply(wm->connection,
                pointer_cookie, NULL);

        if (pointer_reply == NULL) {
            LOGGER_WARNING("Failed to query pointer for" \
                    " 'under-mouse' placement; keeping" \
                    " X-server-assigned position", L_NARG);
            return;
        }

        new_x = (int32_t) pointer_reply->root_x - (int32_t) (fw / 2u);
        new_y = (int32_t) pointer_reply->root_y - (int32_t) (fh / 2u);
        if (new_x < 0) {
            new_x = 0;
        } else if ((uint32_t) new_x + fw > sw) {
            new_x = (sw > fw) ? (int32_t) (sw - fw) : 0;
        }
        if (new_y < wa_y) {
            new_y = wa_y;
        } else if ((uint32_t) new_y + fh > sh) {
            new_y = (sh > fh) ? (int32_t) (sh - fh) : wa_y;
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

    s_place_apply_gravity(surface, client, &new_x, &new_y);

    /* Final safety: gravity adjustments must not push the title bar
     * above the workarea top or above the physical screen edge */
    if (new_y < wa_y) { new_y = wa_y; }
    if (new_x < wa_x) { new_x = wa_x; }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    xcb_configure_window(wm->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) new_x, (uint32_t) new_y});
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
}
