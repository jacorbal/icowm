/**
 * @file cmds/geom.c
 *
 * @brief Client geometry command implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <actdata.h>
#include <client.h>
#include <wm.h>

/* Local includes */
#include <cmds/geom.h>
#include <cmds/util.h>


/* Move the client to a new position */
void wcmd_client_move(client_td *client,
        action_data_client_td *client_data)
{
    xcb_window_t target;

    if (client == NULL || client_data == NULL) {
        return;
    }

    target = wcmd_target_win(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {
                (uint32_t) client_data->new_data.geometry.pos.x,
                (uint32_t) client_data->new_data.geometry.pos.y
            });
    client->layout.geometry.cur.pos =
        client_data->new_data.geometry.pos;
}


/* Center the client on its current screen */
void wcmd_client_center(client_td *client)
{
    uint16_t sw;
    uint16_t sh;
    int32_t x;
    int32_t y;
    xcb_window_t target;

    if (client == NULL || !wcmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    target = wcmd_target_win(client);
    x = ((int32_t) sw - (int32_t) client->layout.geometry.cur.dim.w) / 2;
    y = ((int32_t) sh - (int32_t) client->layout.geometry.cur.dim.h) / 2;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) x, (uint32_t) y});
    client->layout.geometry.cur.pos.x = x;
    client->layout.geometry.cur.pos.y = y;
}


/* Resize the client to new dimensions */
void wcmd_client_resize(client_td *client,
        action_data_client_td *client_data)
{
    xcb_window_t target;
    uint32_t req_w;
    uint32_t req_h;

    if (client == NULL || client_data == NULL) {
        return;
    }

    req_w = client_data->new_data.geometry.dim.w;
    req_h = client_data->new_data.geometry.dim.h;
    /* Apply ICCCM WM_NORMAL_HINTS constraints if available */
    if (client->size_hints.valid) {
        /* Clamp to minimum and maximum */
        if (client->size_hints.min_w > 0 &&
                req_w < (uint32_t) client->size_hints.min_w) {
            req_w = (uint32_t) client->size_hints.min_w;
        }
        if (client->size_hints.max_w > 0 &&
                req_w > (uint32_t) client->size_hints.max_w) {
            req_w = (uint32_t) client->size_hints.max_w;
        }
        if (client->size_hints.min_h > 0 &&
                req_h < (uint32_t) client->size_hints.min_h) {
            req_h = (uint32_t) client->size_hints.min_h;
        }
        if (client->size_hints.max_h > 0 &&
                req_h > (uint32_t) client->size_hints.max_h) {
            req_h = (uint32_t) client->size_hints.max_h;
        }

        /* Align to character-grid increments (e.g., 'xterm' cols./rows) */
        if (client->size_hints.inc_w > 1) {
            uint32_t base = (client->size_hints.base_w > 0)
                ? (uint32_t) client->size_hints.base_w : 0u;
            uint32_t inc  = (uint32_t) client->size_hints.inc_w;
            uint32_t over = (req_w > base) ? (req_w - base) : 0u;
            req_w = base + (over / inc) * inc;
        }
        if (client->size_hints.inc_h > 1) {
            uint32_t base = (client->size_hints.base_h > 0)
                ? (uint32_t) client->size_hints.base_h : 0u;
            uint32_t inc  = (uint32_t) client->size_hints.inc_h;
            uint32_t over = (req_h > base) ? (req_h - base) : 0u;
            req_h = base + (over / inc) * inc;
        }

        /* Ensure the minimum is still met after rounding */
        if (client->size_hints.min_w > 0 &&
                req_w < (uint32_t) client->size_hints.min_w) {
            req_w = (uint32_t) client->size_hints.min_w;
        }
        if (client->size_hints.min_h > 0 &&
                req_h < (uint32_t) client->size_hints.min_h) {
            req_h = (uint32_t) client->size_hints.min_h;
        }
    }

    target = wcmd_target_win(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {req_w, req_h});
    client->layout.geometry.cur.dim.w = req_w;
    client->layout.geometry.cur.dim.h = req_h;
}


/* Maximize the client horizontally */
void wcmd_client_maximize_horz(client_td *client)
{
    uint16_t sw;
    xcb_window_t target;

    if (client == NULL || !wcmd_screen_dim(client, &sw, NULL)) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_save(client);

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X |
            XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH,
            (const uint32_t[]) {
                0,
                (uint32_t) client->layout.geometry.cur.pos.y,
                (uint32_t) sw
            });

    client->layout.geometry.cur.pos.x = 0;
    client->layout.geometry.cur.dim.w = sw;
    client->properties.state = CLIENT_STATE_MAXIMIZED_HORZ;

    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_MAXIMIZED_VERT");
    wcmd_add_states(client, 1, "_NET_WM_STATE_MAXIMIZED_HORZ");
    wm_request_client_redraw(client);
}


/* Maximize the client vertically */
void wcmd_client_maximize_vert(client_td *client)
{
    uint16_t sh;
    xcb_window_t target;

    if (client == NULL || !wcmd_screen_dim(client, NULL, &sh)) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_save(client);

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X |
            XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                (uint32_t) client->layout.geometry.cur.pos.x,
                0,
                (uint32_t) sh
            });

    client->layout.geometry.cur.pos.y = 0;
    client->layout.geometry.cur.dim.h = sh;
    client->properties.state = CLIENT_STATE_MAXIMIZED_VERT;

    wcmd_rem_states(client, 2,
            "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_MAXIMIZED_HORZ");
    wcmd_add_states(client, 1, "_NET_WM_STATE_MAXIMIZED_VERT");
    wm_request_client_redraw(client);
}


/* Maximize the client entirely */
void wcmd_client_maximize(client_td *client)
{
    uint16_t sw;
    uint16_t sh;
    xcb_window_t target;

    if (client == NULL || !wcmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_save(client);

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X     |
            XCB_CONFIG_WINDOW_Y     |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {0, 0, (uint32_t) sw, (uint32_t) sh});
    client->layout.geometry.cur.pos.x = 0;
    client->layout.geometry.cur.pos.y = 0;
    client->layout.geometry.cur.dim.w = sw;
    client->layout.geometry.cur.dim.h = sh;
    client->properties.state = CLIENT_STATE_MAXIMIZED_VERT;

    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 2,
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
    wm_request_client_redraw(client);
}
