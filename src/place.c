/**
 * @file place.c
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
#include <place.h>


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
    int32_t y;

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
    max_x = (sw > fw) ? (int32_t) (sw - fw) : 0;
    max_y = (sh > fh) ? (int32_t) (sh - fh) : 0;

    for (y = min_y; y <= max_y; y += (int32_t) step) {
        int32_t x;
        for (x = min_x; x <= max_x; x += (int32_t) step) {
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
        int32_t x;
        for (x = min_x; x <= max_x; x += (int32_t) step) {
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


/* Apply the configured placement policy to a newly mapped client */
void place_apply(wm_td *wm, surface_td *surface, client_td *client)
{
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

    if (wm->config->base.windows.placement.is_centered) {
        new_x = ((int32_t) sw - (int32_t) fw) / 2;
        new_y = ((int32_t) sh - (int32_t) fh) / 2;
        if (new_x < 0) { new_x = 0; }
        if (new_y < 0) { new_y = 0; }
    } else {
        if (policy == CONFIG_PLACEMENT_POLICY_SMART &&
                place_smart(wm, surface, client, &new_x, &new_y)) {
            /* Placement chosen by smart scan */
        } else if (policy == CONFIG_PLACEMENT_POLICY_CASCADE ||
                policy == CONFIG_PLACEMENT_POLICY_SMART) {
            static uint32_t s_cascade_seq = 0;
            const uint32_t cascade_step = 24u;
            uint32_t max_steps;

            max_steps = (sw > fw) ? (sw - fw) / cascade_step : 1u;
            if (sh > fh) {
                uint32_t my = (sh - fh) / cascade_step;
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
            xcb_query_pointer_cookie_t pointer_cookie;
            xcb_query_pointer_reply_t *pointer_reply;

            pointer_cookie = xcb_query_pointer(wm->connection,
                    surface->screen->root);
            pointer_reply = xcb_query_pointer_reply(wm->connection,
                    pointer_cookie, NULL);
            if (pointer_reply == NULL) {
                LOGGER_NOTICE("Failed to query pointer for"
                        " 'under-mouse' placement; keeping"
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
            /* "none" or unknown: keep the X-server-assigned position */
            return;
        }
    }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    xcb_configure_window(wm->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) new_x, (uint32_t) new_y});
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
}
