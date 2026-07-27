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
    const uint16_t step_x = (uint16_t) (icon_w  + margin);
    const uint16_t step_y = (uint16_t) (icon_h  + margin);
    bool vertical;
    uint16_t max_slots;
    bool occupied[256];
    uint16_t chosen;
    uint16_t slot;
    uint64_t border_twice_u64;
    int32_t border_twice;

    if (client == NULL || client->theme == NULL ||
            out_x == NULL || out_y == NULL) {
        return;
    }

    /* Default safe values */
    *out_x = (int16_t) margin;
    *out_y = (int16_t) margin;

    /* Determine orientation */
    vertical = (policy == CONFIG_ICON_PLACEMENT_LEFT ||
                policy == CONFIG_ICON_PLACEMENT_RIGHT);

    /* For smart placement, try to find the first free slot along the
     * bottom edge; fall through to bottom if none is available */
    if (policy == CONFIG_ICON_PLACEMENT_SMART) {
        policy = CONFIG_ICON_PLACEMENT_BOTTOM;
    }

    /* Maximum slots along the relevant axis */
    if (vertical) {
        max_slots = (screen_h > step_y)
            ? (uint16_t) ((screen_h - margin) / step_y)
            : 1u;
    } else {
        max_slots = (screen_w > step_x)
            ? (uint16_t) ((screen_w - margin) / step_x)
            : 1u;
    }

    /* Collect occupied slots from already-placed icon windows */
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
                    /* Determine which slot this icon occupies */
                    slot = 0u;
                    if (vertical) {
                        int32_t rel = (int32_t) other->icon_y
                            - (int32_t) margin;
                        if (rel >= 0) {
                            slot = (uint16_t) (rel / (int32_t) step_y);
                            if (slot < 256u) {
                                occupied[slot] = true;
                            }
                        }
                    } else {
                        int32_t rel = (int32_t) other->icon_x
                            - (int32_t) margin;
                        if (rel >= 0) {
                            slot = (uint16_t) (rel / (int32_t) step_x);
                            if (slot < 256u) {
                                occupied[slot] = true;
                            }
                        }
                    }
                }
                node = cdlist_next(node);
            } while (node != NULL && node != initial);
        } /* ! if (node) */
    }

    /* Find the first unoccupied slot */
    chosen = 0u;
    for (uint32_t i = 0u; i < max_slots && i < 256u; ++i) {
        if (!occupied[i]) {
            chosen = (uint16_t) i;
            break;
        }
        chosen = (uint16_t) i + 1u;
    }
    if (chosen >= max_slots) {
        chosen = (uint16_t) (max_slots - 1u);
    }

        border_twice_u64 =
            (uint64_t) client->theme->icon.border_width * 2u;
        border_twice = (border_twice_u64 > (uint64_t) INT32_MAX)
            ? INT32_MAX
            : (int32_t) border_twice_u64;

    /* Convert slot to pixel coordinates */
    switch (policy) {
        case CONFIG_ICON_PLACEMENT_TOP:
            *out_x = (int16_t) (margin + (uint32_t) chosen * step_x);
            *out_y = (int16_t) margin;
            break;

        case CONFIG_ICON_PLACEMENT_LEFT:
            *out_x = (int16_t) margin;
            *out_y = (int16_t) (margin + (uint32_t) chosen * step_y);
            break;

        case CONFIG_ICON_PLACEMENT_RIGHT:
            *out_x = (int16_t) ((int32_t) screen_w
                    - (int32_t) icon_w
                    - (int32_t) margin
                    - (int32_t) border_twice);
            *out_y = (int16_t) (margin + (uint32_t) chosen * step_y);
            break;

        case CONFIG_ICON_PLACEMENT_BOTTOM:
        case CONFIG_ICON_PLACEMENT_SMART:
            *out_x = (int16_t) (margin + (uint32_t) chosen * step_x);
            *out_y = (int16_t) ((int32_t) screen_h
                        - (int32_t) margin
                        - (int32_t) icon_h
                        - (int32_t) border_twice);
            break;
    }

    /* Clamp to screen */
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
    uint32_t sw;
    uint32_t sh;
    uint32_t fw;
    uint32_t fh;
    int32_t new_x;
    int32_t new_y;
    enum config_placement_policy_e policy;
    xcb_query_pointer_cookie_t pointer_cookie;
    xcb_query_pointer_reply_t *pointer_reply;
    xcb_window_t target;

    if (wm == NULL || wm->config == NULL ||
            surface == NULL || client == NULL) {
        return;
    }

    sw = surface->properties.dim.w;
    sh = surface->properties.dim.h;
    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;
    policy = wm->config->base.windows.placement_policy;

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

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    xcb_configure_window(wm->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) new_x, (uint32_t) new_y});
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
}
