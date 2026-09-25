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

/* Utils includes */
#include <utils/xcb/connection.h>

/* Stage includes */
#include <stage/monitor.h>

/* Project includes */
#include <client.h>
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/screen.h>


/* Return the frame when decorated, the client window otherwise */
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
bool ccmd_screen_dim(const client_td *client, uint16_t *restrict out_w,
        uint16_t *restrict out_h)
{
    xcb_screen_iterator_t iter;

    if (client == NULL || (out_w == NULL && out_h == NULL)) {
        return false;
    }

    iter = xcb_setup_roots_iterator(xcb_get_setup(xcb_connection_get()));
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
bool ccmd_client_monitor(client_td *client, stage_td **out_stage,
        monitor_td *out_monitor)
{
    stage_td *stage = NULL;
    struct position_s center_pos;
    list_td *stages = wm_get_stages();

    if (client == NULL || out_monitor == NULL || stages == NULL) {
        return false;
    }

    for (list_item_td *node = list_head(stages);
            node != NULL; node = list_next(node)) {
        stage_td *const s = (stage_td *) list_data(node);

        if (s != NULL && s->id == client->screen_id) {
            stage = s;
            break;
        }
    }
    if (stage == NULL) {
        return false;
    }

    center_pos.x = client->layout.geometry.cur.pos.x +
        (int32_t) (client->layout.geometry.cur.dim.w / 2u);
    center_pos.y = client->layout.geometry.cur.pos.y +
        (int32_t) (client->layout.geometry.cur.dim.h / 2u);
    *out_monitor = stage_monitor_for_point(stage, center_pos);

    if (out_stage != NULL) {
        *out_stage = stage;
    }

    return true;
}
