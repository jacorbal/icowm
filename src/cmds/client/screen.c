/**
 * @file cmds/client/screen.c
 *
 * @brief Screen, monitor, and decoration-target resolution
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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <surface.h>

/* Internal includes */
#include <wm/internal.h>     /* the global 'wm' singleton */

/* Local includes */
#include <cmds/client/internal.h>


/* Return the frame window when decorated, otherwise the client window */
xcb_window_t ccmd_target_win(client_td *client)
{
    if (client == NULL) {
        return XCB_WINDOW_NONE;
    }
    if (client_is_decorated(client) && client->frame != 0) {
        return client->frame;
    }
    return client->window;
}


/* Retrieve the pixel dimensions of the client's current screen */
bool ccmd_screen_dim(client_td *client, uint16_t *out_w, uint16_t *out_h)
{
    xcb_screen_iterator_t iter;

    if (client == NULL || (out_w == NULL && out_h == NULL)) {
        return false;
    }

    iter = xcb_setup_roots_iterator(xcb_get_setup(client->connection));
    for (uint32_t i = 0; i < client->screen_id && iter.rem > 0; ++i) {
        xcb_screen_next(&iter);
    }
    if (iter.rem == 0 || iter.data == NULL) {
        return false;
    }

    if (out_w != NULL) {
        *out_w = iter.data->width_in_pixels;
    }
    if (out_h != NULL) {
        *out_h = iter.data->height_in_pixels;
    }
    return true;
}


/* Find which monitor a client is currently on */
bool ccmd_client_monitor(client_td *client, surface_td **out_surface,
        monitor_td *out_monitor)
{
    surface_td *surface = NULL;
    int32_t center_x;
    int32_t center_y;

    if (client == NULL || out_monitor == NULL ||
            wm == NULL || wm->surfaces == NULL) {
        return false;
    }

    for (list_item_td *node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *s = (surface_td *) list_data(node);

        if (s != NULL && s->id == client->screen_id) {
            surface = s;
            break;
        }
    }
    if (surface == NULL) {
        return false;
    }

    center_x = client->layout.geometry.cur.pos.x +
        (int32_t) (client->layout.geometry.cur.dim.w / 2u);
    center_y = client->layout.geometry.cur.pos.y +
        (int32_t) (client->layout.geometry.cur.dim.h / 2u);
    *out_monitor = surface_monitor_for_point(surface, center_x, center_y);

    if (out_surface != NULL) {
        *out_surface = surface;
    }

    return true;
}
