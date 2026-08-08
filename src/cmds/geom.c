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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/sync.h>

/* Default initial values */
#include <defs/client.h>     /* WM_SYNC_MAX_WAIT_TICKS */

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

    if (client == NULL || client_data == NULL ||
            client_is_maximized(client) || client_is_fullscreen(client)) {
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

    if (client == NULL || client_is_maximized(client) ||
            client_is_fullscreen(client) ||
            !wcmd_screen_dim(client, &sw, &sh)) {
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


/**
 * @brief Configure a client to the given frame geometry
 *
 * The single actual configure point used by @c wcmd_client_resize,
 * whether the request is applied right away (an unsynchronized client,
 * or the first step of a synchronized one) or later, once a pending
 * @c _NET_WM_SYNC_REQUEST acknowledgement arrives (see
 * @c wcmd_client_resize_flush_pending).
 *
 * @param client Window to resize
 * @param req_x  Requested frame X
 * @param req_y  Requested frame Y
 * @param req_w  Requested frame width
 * @param req_h  Requested frame height
 */
static void s_wcmd_resize_configure(client_td *client,
        int32_t req_x, int32_t req_y, uint32_t req_w, uint32_t req_h)
{
    xcb_window_t target;
    uint16_t mask;

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

    /* Use 'exposures=1' so the X server generates an 'Expose' event and
     * the client redraws the newly exposed area immediately after
     * a non-interactive (keyboard or programmatic) resize, rather than
     * leaving stale content until the next user-triggered redraw */
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
     * However applications that size themselves on character increments
     * rely on receiving 'ConfigureNotify' to recompute their internal
     * layout; without an explicit notification after a non-interactive
     * (keyboard or programmatic) resize they do not redraw the newly
     * exposed region, leaving a fragment of stale content visible until
     * the next user-triggered repaint.  Send the synthetic event
     * unconditionally so every client always receives the definitive
     * geometry notification. */
    client_send_synthetic_configure_notify(client->connection, client);
    }


/**
 * @brief Send the next @c _NET_WM_SYNC_REQUEST counter value to
 *        a client
 *
 * Increments the client's local shadow counter and sends the matching
 * @c WM_PROTOCOLS @c ClientMessage (EWMH @c _NET_WM_SYNC_REQUEST),
 * asking the client to redraw for that frame and set its XSync counter
 * to the same value once done.  Marks the client as waiting for the
 * corresponding @c AlarmNotify.
 *
 * @param client Client to notify; must have @c has_net_wm_sync_request
 */
static void s_wcmd_resize_send_sync_request(client_td *client)
{
    xcb_client_message_event_t ev;

    client->sync_value += 1u;
    client->sync_waiting = true;
    client->sync_wait_ticks = 0u;

    if (client->ewmh == NULL) {
        return;
    }

    memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.format = 32;
    ev.window = client->window;
    ev.type = client->ewmh->WM_PROTOCOLS;
    ev.data.data32[0] = client->ewmh->_NET_WM_SYNC_REQUEST;
    ev.data.data32[1] = XCB_CURRENT_TIME;
    ev.data.data32[2] = client->sync_value;
    ev.data.data32[3] = 0u;    /* high 32 bits: always 0 at our scale */
    xcb_send_event(client->connection, 0, client->window,
            XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);
}


/**
 * @brief Dispatch one resize step for a @c _NET_WM_SYNC_REQUEST client
 *
 * Sends the sync request for this frame, then applies the geometry
 * right away: the window manager does not itself block waiting for the
 * acknowledgement before configuring, since that would stall the whole
 * event loop.  What actually throttles the client (and so avoids it
 * falling behind and tearing) is that further requests arriving before
 * the matching @c AlarmNotify are queued as a single pending geometry
 * in @c wcmd_client_resize rather than dispatched immediately, so at
 * most one unacknowledged frame is ever in flight.
 *
 * @param client Client to resize
 * @param req_x  Requested frame X
 * @param req_y  Requested frame Y
 * @param req_w  Requested frame width
 * @param req_h  Requested frame height
 */
static void s_wcmd_resize_dispatch_synced(client_td *client,
        int32_t req_x, int32_t req_y, uint32_t req_w, uint32_t req_h)
{
    s_wcmd_resize_send_sync_request(client);
    s_wcmd_resize_configure(client, req_x, req_y, req_w, req_h);
}


/* Resize the client to new dimensions */
void wcmd_client_resize(client_td *client,
        action_data_client_td *client_data)
{
    int32_t req_x;
    int32_t req_y;
    uint32_t req_w;
    uint32_t req_h;
    bool synced;

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

    synced = client->has_net_wm_sync_request && wm_sync_available();

    if (!synced) {
        s_wcmd_resize_configure(client, req_x, req_y, req_w, req_h);
        return;
    }

    if (!client->sync_waiting) {
        s_wcmd_resize_dispatch_synced(client, req_x, req_y, req_w, req_h);
        return;
    }

    /* Already waiting on the previous request's 'AlarmNotify': queue
     * this geometry instead of piling up unacknowledged configures,
     * unless the client has already gone 'WM_SYNC_MAX_WAIT_TICKS'
     * attempts without acknowledging, in which case give up waiting and
     * apply this one directly, so an unresponsive client can never
     * freeze interactive resize */
    client->sync_wait_ticks += 1u;
    if (client->sync_wait_ticks > (uint8_t) WM_SYNC_MAX_WAIT_TICKS) {
        client->sync_has_pending = false;
        s_wcmd_resize_dispatch_synced(client, req_x, req_y, req_w, req_h);
        return;
    }

    client->sync_pending_geom.x = req_x;
    client->sync_pending_geom.y = req_y;
    client->sync_pending_geom.w = req_w;
    client->sync_pending_geom.h = req_h;
    client->sync_has_pending = true;
}


/* Apply a client's pending '_NET_WM_SYNC_REQUEST'-throttled resize */
void wcmd_client_resize_flush_pending(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client->sync_waiting = false;
    client->sync_wait_ticks = 0u;

    if (!client->sync_has_pending) {
        return;
    }

    client->sync_has_pending = false;
    s_wcmd_resize_dispatch_synced(client,
            client->sync_pending_geom.x, client->sync_pending_geom.y,
            client->sync_pending_geom.w, client->sync_pending_geom.h);
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
