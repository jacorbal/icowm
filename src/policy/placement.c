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

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <policy/placement.h>


/**
 * @brief Test whether a candidate rectangle overlaps any visible client
 *
 * Only currently visible, non-iconified clients on @p desktop are
 * considered blocking.
 *
 * @param desktop     Desktop whose clients are inspected
 * @param skip_client Client to ignore during the test
 * @param x           Candidate left coordinate
 * @param y           Candidate top coordinate
 * @param w           Candidate width
 * @param h           Candidate height
 *
 * @return @c true when the candidate intersects a visible client,
 *         @c false otherwise
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static bool s_overlaps_clients(desktop_td *desktop,
        const client_td *skip_client,
        int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;

    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        return false;
    }

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return false;
    }

    initial = node;
    do {
        const client_td *other =
            (const client_td *) cdlist_data(node);
        if (other != NULL && other != skip_client &&
                !(other->properties.flags & CLIENT_FLAG_HIDDEN) &&
                other->properties.state !=
                    (uint16_t) CLIENT_STATE_ICONIFIED &&
                geom_rect_overlap(x, y, w, h,
                        other->layout.geometry.cur.pos.x,
                        other->layout.geometry.cur.pos.y,
                        other->layout.geometry.cur.dim.w,
                        other->layout.geometry.cur.dim.h)) {
            return true;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return false;
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
 *
 * @param client  Pointer to the client whose gravity and frame size are
 *                 used
 *
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


/* Find a non-overlapping smart position for a newly mapped client */
bool place_smart(wm_td *wm, surface_td *surface, client_td *client,
        int32_t *out_x, int32_t *out_y)
{
    desktop_td *desktop;
    const uint32_t step = 24u;
    uint32_t sw;
    uint32_t sh;
    uint32_t fw;
    uint32_t fh;
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
    int32_t cx;
    int32_t cy;

    (void) wm; /* reserved for future use */

    if (surface == NULL || client == NULL ||
            out_x == NULL || out_y == NULL) {
        return false;
    }

    desktop = surface_desktop_get(surface, surface->desktop_cur);
    if (desktop == NULL) {
        return false;
    }

    sw = surface->properties.dim.w;
    sh = surface->properties.dim.h;
    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;

    min_x = (fw > sw) ? -((int32_t) (fw - sw)) : 0;
    min_y = (fh > sh) ? -((int32_t) (fh - sh)) : 0;
    max_x = (sw > fw) ?   (int32_t) (sw - fw)  : 0;
    max_y = (sh > fh) ?   (int32_t) (sh - fh)  : 0;

    /* Try the centre first a window on an otherwise empty desktop lands
     * in the middle of the screen */
    cx = ((int32_t) sw - (int32_t) fw) / 2;
    cy = ((int32_t) sh - (int32_t) fh) / 2;
    if (cx < min_x) { cx = min_x; }
    if (cy < min_y) { cy = min_y; }
    if (!s_overlaps_clients(desktop, client, cx, cy, fw, fh)) {
        *out_x = cx;
        *out_y = cy;
        return true;
    }

    for (int32_t y = min_y; y <= max_y; y += (int32_t) step) {
        for (int32_t x = min_x; x <= max_x; x += (int32_t) step) {
            if (!s_overlaps_clients(desktop, client, x, y, fw, fh)) {
                *out_x = x;
                *out_y = y;
                return true;
            }
        }

        if (max_x != min_x &&
                !s_overlaps_clients(desktop, client, max_x, y, fw, fh)) {
            *out_x = max_x;
            *out_y = y;
            return true;
        }
    }

    if (max_y != min_y) {
        for (int32_t x = min_x; x <= max_x; x += (int32_t) step) {
            if (!s_overlaps_clients(desktop, client,
                    x, max_y, fw, fh)) {
                *out_x = x;
                *out_y = max_y;
                return true;
            }
        }
    }

    if (!s_overlaps_clients(desktop, client, max_x, max_y, fw, fh)) {
        *out_x = max_x;
        *out_y = max_y;
        return true;
    }

    return false;
}


/* Compute the icon window position for a newly iconified client */
void place_icon(const client_td *client, desktop_td *desktop,
        enum config_icon_placement_e policy,
        uint16_t icon_w, uint16_t icon_h,
        uint16_t screen_w, uint16_t screen_h,
        int16_t *out_x, int16_t *out_y)
{
    const uint16_t margin = 8u;
    const uint16_t step_x = (uint16_t) (icon_w + margin);
    const uint16_t step_y = (uint16_t) (icon_h + margin);
    uint64_t border_twice_u64;
    int32_t border_twice;
    uint16_t max_primary;
    bool occupied[256];
    uint16_t chosen;
    uint16_t pri;
    uint16_t sec;

    if (client == NULL || client->theme == NULL ||
            out_x == NULL || out_y == NULL) {
        return;
    }

    *out_x = (int16_t) margin;
    *out_y = (int16_t) margin;

    border_twice_u64 =
        (uint64_t) client->theme->icon.general.border_width * 2u;
    border_twice = (border_twice_u64 > (uint64_t) INT32_MAX)
        ? INT32_MAX : (int32_t) border_twice_u64;

    /* SMART: use BOTTOM as the default smart strategy */
    if (policy == CONFIG_ICON_PLACEMENT_SMART) {
        policy = CONFIG_ICON_PLACEMENT_BOTTOM;
    }

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

    /* Mark occupied slots.  'slot = sec * max_primary + pri' where
     * 'pri' indexes along the edge and 'sec' counts overflow
     * rows/columns */
    for (uint16_t i = 0u; i < 256u; ++i) {
        occupied[i] = false;
    }

    if (desktop != NULL && desktop->stacking != NULL) {
        cdlist_item_td *node = cdlist_head(desktop->stacking);
        cdlist_item_td *initial = node;
        if (node != NULL) {
            do {
                const client_td *other =
                    (const client_td *) cdlist_data(node);
                if (other != NULL && other != client &&
                        other->icon_window != 0 &&
                        other->is_icon_mapped) {
                    int32_t rel_pri;
                    int32_t rel_sec;
                    uint16_t slot;
                    rel_pri = 0;
                    rel_sec = 0;

                    switch (policy) {
                        case CONFIG_ICON_PLACEMENT_TOP:
                            rel_pri = (int32_t) other->icon_x -
                                (int32_t) margin;
                            rel_sec = (int32_t) other->icon_y -
                                (int32_t) margin;
                            if (rel_pri >= 0 && rel_sec >= 0) {
                                pri = (uint16_t) (rel_pri /
                                        (int32_t) step_x);
                                sec = (uint16_t) (rel_sec /
                                        (int32_t) step_y);
                                slot = (uint16_t) (sec * max_primary
                                        + pri);
                                if (slot < 256u) {
                                    occupied[slot] = true;
                                }
                            }
                            break;

                        case CONFIG_ICON_PLACEMENT_BOTTOM:
                        case CONFIG_ICON_PLACEMENT_SMART:
                            rel_pri = (int32_t) other->icon_x -
                                (int32_t) margin;
                            rel_sec = (int32_t) screen_h -
                                (int32_t) margin -
                                (int32_t) icon_h -
                                (int32_t) border_twice -
                                (int32_t) other->icon_y;
                            if (rel_pri >= 0 && rel_sec >= 0) {
                                pri = (uint16_t) (rel_pri /
                                        (int32_t) step_x);
                                sec = (uint16_t) (rel_sec /
                                        (int32_t) step_y);
                                slot = (uint16_t) (sec * max_primary
                                        + pri);
                                if (slot < 256u) {
                                    occupied[slot] = true;
                                }
                            }
                            break;

                        case CONFIG_ICON_PLACEMENT_LEFT:
                            rel_pri = (int32_t) other->icon_y -
                                (int32_t) margin;
                            rel_sec = (int32_t) other->icon_x -
                                (int32_t) margin;
                            if (rel_pri >= 0 && rel_sec >= 0) {
                                pri = (uint16_t) (rel_pri /
                                        (int32_t) step_y);
                                sec = (uint16_t) (rel_sec /
                                        (int32_t) step_x);
                                slot = (uint16_t) (sec * max_primary
                                        + pri);
                                if (slot < 256u) {
                                    occupied[slot] = true;
                                }
                            }
                            break;

                        case CONFIG_ICON_PLACEMENT_RIGHT:
                            rel_pri = (int32_t) other->icon_y -
                                (int32_t) margin;
                            rel_sec = (int32_t) screen_w -
                                (int32_t) margin -
                                (int32_t) icon_w -
                                (int32_t) border_twice -
                                (int32_t) other->icon_x;
                            if (rel_pri >= 0 && rel_sec >= 0) {
                                pri = (uint16_t) (rel_pri /
                                        (int32_t) step_y);
                                sec = (uint16_t) (rel_sec /
                                        (int32_t) step_x);
                                slot = (uint16_t) (sec * max_primary
                                        + pri);
                                if (slot < 256u) {
                                    occupied[slot] = true;
                                }
                            }
                            break;
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        } /* ! if (node) */
    }

    /* Find the first unoccupied slot */
    chosen = 0u;
    for (uint16_t i = 0u; i < 256u; ++i) {
        if (!occupied[i]) {
            chosen = i;
            break;
        }
        chosen = i + 1u;
    }
    if (chosen >= 256u) {
        chosen = 0u;
    }

    /* Convert slot index to pixel coordinates:
     * - 'pri = chosen % max_primary'  (position along the screen edge)
     * - 'sec = chosen / max_primary'  (overflow row/col away from edge) */
    pri = (uint16_t) (chosen % max_primary);
    sec = (uint16_t) (chosen / max_primary);

    switch (policy) {
        case CONFIG_ICON_PLACEMENT_TOP:
            *out_x = (int16_t) (margin + (uint32_t) pri * step_x);
            *out_y = (int16_t) (margin + (uint32_t) sec * step_y);
            break;

        case CONFIG_ICON_PLACEMENT_LEFT:
            *out_x = (int16_t) (margin + (uint32_t) sec * step_x);
            *out_y = (int16_t) (margin + (uint32_t) pri * step_y);
            break;

        case CONFIG_ICON_PLACEMENT_RIGHT:
            *out_x = (int16_t) ((int32_t) screen_w -
                    (int32_t) icon_w -
                    (int32_t) margin -
                    (int32_t) border_twice -
                    (int32_t) sec * (int32_t) step_x);
            *out_y = (int16_t) (margin + (uint32_t) pri * step_y);
            break;

        case CONFIG_ICON_PLACEMENT_BOTTOM:
        case CONFIG_ICON_PLACEMENT_SMART:
            *out_x = (int16_t) (margin + (uint32_t) pri * step_x);
            *out_y = (int16_t) ((int32_t) screen_h -
                    (int32_t) margin -
                    (int32_t) icon_h -
                    (int32_t) border_twice -
                    (int32_t) sec * (int32_t) step_y);
            break;
    }

    if (*out_x < (int16_t) margin) {
        *out_x = (int16_t) margin;
    }
    if (*out_y < (int16_t) margin) {
        *out_y = (int16_t) margin;
    }
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
    int32_t new_x;
    int32_t new_y;
    xcb_window_t target;
    enum config_placement_policy_e policy;

    if (wm == NULL || wm->config == NULL ||
            surface == NULL || client == NULL) {
        return;
    }

    sw = surface->properties.dim.w;
    sh = surface->properties.dim.h;
    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    policy = wm->config->base.windows.placement_policy;

    /* ICCCM §4.1.2.6: center transient dialogs over their parent */
    if (client->transient_for != XCB_WINDOW_NONE) {
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
            if (new_x < 0) { new_x = 0; }
            if (new_y < 0) { new_y = 0; }
            if ((uint32_t) new_x + fw > sw) {
                new_x = (sw > fw) ? (int32_t) (sw - fw) : 0;
            }
            if ((uint32_t) new_y + fh > sh) {
                new_y = (sh > fh) ? (int32_t) (sh - fh) : 0;
            }

            target = (client_is_decorated(client) && client->frame != 0)
                ? client->frame : client->window;
            xcb_configure_window(wm->connection, target,
                    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                    (const uint32_t[]) {
                        (uint32_t) new_x, (uint32_t) new_y});

            client->layout.geometry.cur.pos.x = new_x;
            client->layout.geometry.cur.pos.y = new_y;
            return;
        }
        /* Parent geometry unavailable; fall through to normal policy */
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

        new_x = (int32_t) ((s_cascade_seq % max_steps) * cascade_step);
        new_y = (int32_t) ((s_cascade_seq % max_steps) * cascade_step);
        s_cascade_seq++;
    } else if (policy == CONFIG_PLACEMENT_POLICY_CENTERED) {
        new_x = ((int32_t) sw - (int32_t) fw) / 2;
        new_y = ((int32_t) sh - (int32_t) fh) / 2;
        if (new_x < 0) { new_x = 0; }
        if (new_y < 0) { new_y = 0; }
    } else if (policy == CONFIG_PLACEMENT_POLICY_UNDER_MOUSE) {
        pointer_cookie = xcb_query_pointer(wm->connection,
                surface->screen->root);
        pointer_reply = xcb_query_pointer_reply(wm->connection,
                pointer_cookie, NULL);

        if (pointer_reply == NULL) {
            LOGGER_NOTICE("Failed to query pointer for" \
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
        if (new_y < 0) {
            new_y = 0;
        } else if ((uint32_t) new_y + fh > sh) {
            new_y = (sh > fh) ? (int32_t) (sh - fh) : 0;
        }

        free(pointer_reply);
    } else {
        /* "none" or unknown: keep X-server position unless the frame
         * would start outside the visible top-left screen corner */
        new_x = client->layout.geometry.cur.pos.x;
        new_y = client->layout.geometry.cur.pos.y;

        if (new_x < 0) {
            new_x = 0;
        }

        if (new_y < 0) {
            new_y = 0;
        }

        if (new_x == client->layout.geometry.cur.pos.x &&
                new_y == client->layout.geometry.cur.pos.y) {
            return;
        }
    }

    s_place_apply_gravity(surface, client, &new_x, &new_y);

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    xcb_configure_window(wm->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) new_x, (uint32_t) new_y});
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
}
