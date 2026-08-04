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
#include <cmds/ccmd.h>
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
    client->rule_position_locked = false;
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
    client->rule_position_locked = false;
}


/* Resize the client to new dimensions */
void wcmd_client_resize(client_td *client,
        action_data_client_td *client_data)
{
    xcb_window_t target;
    int32_t req_x;
    int32_t req_y;
    uint32_t req_w;
    uint32_t req_h;
    uint16_t mask;

    if (client == NULL || client_data == NULL) {
        return;
    }

    /* Resizing is forbidden while the client is maximized or
     * fullscreen; shaded clients are first restored so the requested
     * size applies to the normal window geometry instead of the
     * rolled-up titlebar */
    if (client->properties.state == (uint16_t) CLIENT_STATE_FULLSCREEN ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED_VERT ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ) {
        return;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    req_x = client_data->new_data.geometry.pos.x;
    req_y = client_data->new_data.geometry.pos.y;

    /* The event data was already constrained by
     * 'client_send_event_resize' (size hints, increments, min/max)
     * before being queued.  Use the pre-constrained frame-space values
     * directly so we do not apply increment snapping a second time.
     * A second pass would force the unchanged axis (e.g., height when
     * only width is being resized by keyboard) onto the increment grid
     * from whatever the program reported via its own ConfigureRequest,
     * causing the window to shrink on every resize keypress. */
    req_w = client_data->new_data.geometry.dim.w;
    req_h = client_data->new_data.geometry.dim.h;

    target = wcmd_target_win(client);
    mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    if (req_x != client->layout.geometry.cur.pos.x ||
            req_y != client->layout.geometry.cur.pos.y) {
        mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    }

    xcb_configure_window(client->connection, target,
            mask,
            (mask == (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT))
                ? (const uint32_t[]) {req_w, req_h}
                : (const uint32_t[]) {
                    (uint32_t) req_x,
                    (uint32_t) req_y,
                    req_w,
                    req_h
                });

    client->layout.geometry.cur.pos.x = req_x;
    client->layout.geometry.cur.pos.y = req_y;
    client->layout.geometry.cur.dim.w = req_w;
    client->layout.geometry.cur.dim.h = req_h;

    /* For decorated (reparented) clients the inner window must be
     * repositioned and resized to match the new frame dimensions.
     * Previously this was handled by the immediate
     * 'xcb_configure_window' path in 'client_send_event_resize', but
     * non-interactive resizes now go through the event queue
     * exclusively so 'wcmd_client_resize' is the single configure
     * point. */
    client_sync_decoration_layout(client);

    /* Use 'exposures=1' so the X server generates an Expose event and
     * the client (e.g., gVim) redraws the newly exposed area
     * immediately after a non-interactive (keyboard or programmatic)
     * resize, rather than leaving stale content until the next
     * user-triggered redraw. */
    xcb_clear_area(client->connection, 1, client->window, 0, 0, 0, 0);

    /* Mark the client's desktop as outdated so the frame decoration
     * (titlebar background, text, border grips) is repainted on the
     * next render pass to match the new frame size */
    wm_request_client_redraw(client);

    /* ICCCM §4.2.3: send a synthetic 'ConfigureNotify' with
     * screen-relative coordinates so the application always knows its
     * true on-screen position and content-area size.
     *
     * For decorated (reparented) clients the X server delivers
     * a frame-relative 'ConfigureNotify' (x=border, y=titlebar+border)
     * from 'client_sync_decoration_layout'; the synthetic event
     * overrides that with screen-relative coordinates.
     *
     * For undecorated clients there is no reparenting, so the X server
     * would normally supply the correct screen-relative coordinates.
     * However applications like gVim that size themselves on character
     * increments rely on receiving 'ConfigureNotify' to recompute their
     * internal layout; without an explicit notification after
     * a non-interactive (keyboard or programmatic) resize they do not
     * redraw the newly exposed region, leaving a fragment of stale
     * content visible until the next user-triggered repaint.  Send the
     * synthetic event unconditionally so every client always receives
     * the definitive geometry notification. */
    client_send_synthetic_configure_notify(client->connection, client);
}


/* Maximize the client horizontally, or restore if already horizontally
 * maximized */
void wcmd_client_maximize_horz(client_td *client)
{
    uint16_t sw;
    xcb_window_t target;

    if (client == NULL || !wcmd_screen_dim(client, &sw, NULL)) {
        return;
    }

    if (!client_is_resizable(client) || client_is_fullscreen(client)) {
        return;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    /* Toggle: if already maximized horizontally, restore saved geometry */
    if (client->properties.state == CLIENT_STATE_MAXIMIZED_HORZ) {
        target = wcmd_target_win(client);
        client_geometry_restore(client);
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH,
                (const uint32_t[]) {
                    (uint32_t) client->layout.geometry.cur.pos.x,
                    client->layout.geometry.cur.dim.w
                });
        client->properties.state = CLIENT_STATE_NORMAL;
        wcmd_rem_states(client, 1, "_NET_WM_STATE_MAXIMIZED_HORZ");
        wm_request_client_redraw(client);
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


/* Maximize the client vertically, or restore if already vertically
 * maximized */
void wcmd_client_maximize_vert(client_td *client)
{
    uint16_t sh;
    xcb_window_t target;

    if (client == NULL || !wcmd_screen_dim(client, NULL, &sh)) {
        return;
    }

    if (!client_is_resizable(client) || client_is_fullscreen(client)) {
        return;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    if (client->properties.state == CLIENT_STATE_MAXIMIZED_VERT) {
        target = wcmd_target_win(client);
        client_geometry_restore(client);
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_X     |
                XCB_CONFIG_WINDOW_Y     |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    (uint32_t) client->layout.geometry.cur.pos.x,
                    (uint32_t) client->layout.geometry.cur.pos.y,
                    client->layout.geometry.cur.dim.w,
                    client->layout.geometry.cur.dim.h
                });
        client->properties.state = CLIENT_STATE_NORMAL;
        wcmd_rem_states(client, 1, "_NET_WM_STATE_MAXIMIZED_VERT");
        wm_request_client_redraw(client);
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


/* Maximize the client entirely, or restore it if already maximized */
void wcmd_client_maximize(client_td *client)
{
    uint16_t sw;
    uint16_t sh;
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    if (!client_is_resizable(client) || client_is_fullscreen(client)) {
        return;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    /* Toggle: if already maximized (fully or vertically), restore */
    if (client->properties.state == CLIENT_STATE_MAXIMIZED ||
            client->properties.state == CLIENT_STATE_MAXIMIZED_VERT) {
        target = wcmd_target_win(client);
        client_geometry_restore(client);
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_X     |
                XCB_CONFIG_WINDOW_Y     |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                (uint32_t) client->layout.geometry.cur.pos.x,
                (uint32_t) client->layout.geometry.cur.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h
                });
        client->properties.state = CLIENT_STATE_NORMAL;
        wcmd_rem_states(client, 2,
                "_NET_WM_STATE_MAXIMIZED_HORZ",
                "_NET_WM_STATE_MAXIMIZED_VERT");
        wm_request_client_redraw(client);
        return;
    }

    if (!wcmd_screen_dim(client, &sw, &sh)) {
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
    client->layout.geometry.cur.dim.w = (uint32_t) sw;
    client->layout.geometry.cur.dim.h = (uint32_t) sh;
    client->properties.state = CLIENT_STATE_MAXIMIZED;

    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 2,
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
    wm_request_client_redraw(client);
}
