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
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <utils/geom.h>
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
    int32_t mx = 0;
    int32_t my = 0;
    int32_t x;
    int32_t y;
    xcb_window_t target;
    monitor_td monitor;

    if (client == NULL || client_is_maximized(client) ||
            client_is_fullscreen(client)) {
        return;
    }

    if (wcmd_client_monitor(client, NULL, &monitor)) {
        mx = monitor.x;
        my = monitor.y;
        sw = geom_clamp_dim((int32_t) monitor.w);
        sh = geom_clamp_dim((int32_t) monitor.h);
    } else if (!wcmd_screen_dim(client, &sw, &sh)) {
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
    x += mx;
    y += my;

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) x, (uint32_t) y});
    client->layout.geometry.cur.pos.x = x;
    client->layout.geometry.cur.pos.y = y;
    client->rule_position_locked = false;
}


/* Move the client to a specific monitor on its own surface */
void wcmd_client_move_to_monitor(client_td *client, uint32_t monitor_index)
{
    surface_td *surface = NULL;
    monitor_td cur_monitor;
    monitor_td target_monitor;
    xcb_window_t target;
    int32_t new_x;
    int32_t new_y;
    uint32_t idx;

    if (client == NULL) {
        return;
    }

    if (!wcmd_client_monitor(client, &surface, &cur_monitor) ||
            surface == NULL || surface->monitor_count == 0u) {
        return;
    }

    idx = monitor_index;
    if (idx >= surface->monitor_count) {
        LOGGER_WARNING("Move-to-monitor targets monitor %u, which" \
                " does not exist on surface %u (%u monitor(s));" \
                " falling back to monitor 0", monitor_index,
                surface->id, surface->monitor_count);
        idx = 0u;
    }
    target_monitor = surface->monitors[idx];

    if (target_monitor.x == cur_monitor.x &&
            target_monitor.y == cur_monitor.y) {
        return;
    }

    new_x = client->layout.geometry.cur.pos.x -
        cur_monitor.x + target_monitor.x;
    new_y = client->layout.geometry.cur.pos.y -
        cur_monitor.y + target_monitor.y;

    /* Clamp so the window stays fully on the target monitor even if
     * it is smaller than the one the client came from */
    if (new_x < target_monitor.x) {
        new_x = target_monitor.x;
    } else if ((uint32_t) (new_x - target_monitor.x) +
            client->layout.geometry.cur.dim.w > target_monitor.w) {
        new_x = (target_monitor.w > client->layout.geometry.cur.dim.w)
            ? target_monitor.x + (int32_t) (target_monitor.w -
                    client->layout.geometry.cur.dim.w)
            : target_monitor.x;
    }
    if (new_y < target_monitor.y) {
        new_y = target_monitor.y;
    } else if ((uint32_t) (new_y - target_monitor.y) +
            client->layout.geometry.cur.dim.h > target_monitor.h) {
        new_y = (target_monitor.h > client->layout.geometry.cur.dim.h)
            ? target_monitor.y + (int32_t) (target_monitor.h -
                    client->layout.geometry.cur.dim.h)
            : target_monitor.y;
    }

    target = wcmd_target_win(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) new_x, (uint32_t) new_y});
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    client->rule_position_locked = false;
    wm_request_client_redraw(client);
}


/* Move the client to the next monitor on its own surface */
void wcmd_client_move_to_next_monitor(client_td *client)
{
    surface_td *surface = NULL;
    monitor_td cur_monitor;
    uint32_t cur_idx = 0u;

    if (client == NULL) {
        return;
    }

    if (!wcmd_client_monitor(client, &surface, &cur_monitor) ||
            surface == NULL || surface->monitor_count <= 1u) {
        return;
    }

    for (uint32_t i = 0u; i < surface->monitor_count; ++i) {
        if (surface->monitors[i].x == cur_monitor.x &&
                surface->monitors[i].y == cur_monitor.y) {
            cur_idx = i;
            break;
        }
    }

    wcmd_client_move_to_monitor(client,
            (cur_idx + 1u) % surface->monitor_count);
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


/**
 * @brief Find the maximize target area for a client's own monitor
 *
 * Resolves @p client's surface and desktop from the global @c wm
 * singleton, then clips the desktop's workarea (already adjusted for
 * panel/dock struts) down to whichever physical monitor @p client's
 * own center point currently falls on.  A client pinned to every
 * desktop uses its surface's currently shown desktop instead, since
 * it has no single desktop of its own.
 *
 * @param client Client to find the maximize target area for
 * @param out_x  Receives the target area's left edge (may be @c NULL)
 * @param out_y  Receives the target area's top edge (may be @c NULL)
 * @param out_w  Receives the target area's width
 * @param out_h  Receives the target area's height
 *
 * @return @c true on success, @c false if any part of the lookup
 *         fails (surface not found, desktop not found, no workarea
 *         known yet, or the clipped area is empty); callers fall
 *         back to @c wcmd_screen_dim's raw screen size in that case
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
static bool s_client_monitor_workarea(client_td *client,
        int32_t *out_x, int32_t *out_y,
        uint16_t *out_w, uint16_t *out_h)
{
    surface_td *surface = NULL;
    desktop_td *desktop;
    monitor_td monitor;
    struct geometry_s clipped;
    uint32_t did;

    if (client == NULL || out_w == NULL || out_h == NULL) {
        return false;
    }

    if (!wcmd_client_monitor(client, &surface, &monitor)) {
        return false;
    }

    did = (client->desktop_id == WM_DESKTOP_ID_ALL) ?
        surface->desktop_cur : client->desktop_id;
    desktop = surface_desktop_get(surface, did);
    if (desktop == NULL || desktop->workarea.dim.w == 0u ||
            desktop->workarea.dim.h == 0u) {
        return false;
    }

    clipped = geom_intersect_rect(
            desktop->workarea.pos.x, desktop->workarea.pos.y,
            desktop->workarea.dim.w, desktop->workarea.dim.h,
            monitor.x, monitor.y,
            monitor.w, monitor.h);
    if (clipped.dim.w == 0u || clipped.dim.h == 0u) {
        return false;
    }

    if (out_x != NULL) {
        *out_x = clipped.pos.x;
    }
    if (out_y != NULL) {
        *out_y = clipped.pos.y;
    }
    *out_w = geom_clamp_dim((int32_t) clipped.dim.w);
    *out_h = geom_clamp_dim((int32_t) clipped.dim.h);

    return true;
}


/* Maximize the client horizontally, or restore if already horizontally
 * maximized */
void wcmd_client_maximize_horz(client_td *client)
{
    int32_t mx = 0;
    uint16_t sw;
    uint16_t unused_h;
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    if (!s_client_monitor_workarea(client, &mx, NULL, &sw, &unused_h) &&
            !wcmd_screen_dim(client, &sw, NULL)) {
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
    /* Only remember the geometry to restore to if it is not already
     * a maximized state's geometry: switching from vertical-only
     * maximize to horizontal must not overwrite the true pre-maximize
     * geometry already held in 'layout.geometry.old' (see
     * 'client_is_maximized_any'), or restoring later would land at
     * whichever partial-maximize size happened to be current, instead
     * of the window's original one */
    if (!client_is_maximized_any(client)) {
        client_geometry_save(client);
    }

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X |
            XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH,
            (const uint32_t[]) {
                (uint32_t) mx,
                (uint32_t) client->layout.geometry.cur.pos.y,
                (uint32_t) sw
            });

    client->layout.geometry.cur.pos.x = mx;
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
    int32_t my = 0;
    uint16_t sh;
    uint16_t unused_w;
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    if (!s_client_monitor_workarea(client, NULL, &my, &unused_w, &sh) &&
            !wcmd_screen_dim(client, NULL, &sh)) {
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
    /* Only remember the geometry to restore to if it is not already
     * a maximized state's geometry: switching from horizontal-only
     * maximize to vertical must not overwrite the true pre-maximize
     * geometry already held in 'layout.geometry.old' (see
     * 'client_is_maximized_any'), or restoring later would land at
     * whichever partial-maximize size happened to be current, instead
     * of the window's original one */
    if (!client_is_maximized_any(client)) {
        client_geometry_save(client);
    }

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X |
            XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                (uint32_t) client->layout.geometry.cur.pos.x,
                (uint32_t) my,
                (uint32_t) sh
            });

    client->layout.geometry.cur.pos.y = my;
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
    int32_t mx = 0;
    int32_t my = 0;
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

    /* Toggle: only restore if already fully maximized; a window
     * maximized on just one axis (horizontal or vertical) falls
     * through to the "maximize" branch below instead, completing it
     * to full maximize on the other axis too. */
    if (client->properties.state == CLIENT_STATE_MAXIMIZED) {
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

    if (!s_client_monitor_workarea(client, &mx, &my, &sw, &sh) &&
            !wcmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    target = wcmd_target_win(client);
    /* Only remember the geometry to restore to if it is not already
     * a maximized state's geometry: switching from horizontal-only or
     * vertical-only maximize to full maximize must not overwrite the
     * true pre-maximize geometry already held in
     * 'layout.geometry.old' (see 'client_is_maximized_any'), or
     * restoring later would land at whichever partial-maximize size
     * happened to be current, instead of the window's original one */
    if (!client_is_maximized_any(client)) {
        client_geometry_save(client);
    }

    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X     |
            XCB_CONFIG_WINDOW_Y     |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT,
            (const uint32_t[]) {
                (uint32_t) mx, (uint32_t) my,
                (uint32_t) sw, (uint32_t) sh
            });
    client->layout.geometry.cur.pos.x = mx;
    client->layout.geometry.cur.pos.y = my;
    client->layout.geometry.cur.dim.w = (uint32_t) sw;
    client->layout.geometry.cur.dim.h = (uint32_t) sh;
    client->properties.state = CLIENT_STATE_MAXIMIZED;

    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 2,
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
    wm_request_client_redraw(client);
}
