/**
 * @file cmds/client/geom.c
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
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <utils/geom.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/basic.h>
#include <cmds/client/geom.h>
#include <cmds/client/internal.h>


/* Move the client to a new position */
/* Move the client to a new position */
void ccmd_client_move(client_td *client, int32_t x, int32_t y)
{
    xcb_window_t target;

    if (client == NULL || client_is_maximized(client) ||
            client_is_fullscreen(client)) {
        return;
    }

    target = ccmd_target_win(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) { (uint32_t) x, (uint32_t) y });
    client->layout.geometry.cur.pos.x = x;
    client->layout.geometry.cur.pos.y = y;
    client->rule_position_locked = false;
}


/* Center the client on its current screen */
void ccmd_client_center(client_td *client)
{
    uint16_t sw;
    uint16_t sh;
    int32_t mx = 0;
    int32_t my = 0;
    int32_t x;
    int32_t y;
    xcb_window_t target;

    if (client == NULL || client_is_maximized(client) ||
            client_is_fullscreen(client)) {
        return;
    }

    /* Centers within the workarea of whichever monitor 'client'
     * currently sits on, not its raw dimensions: consistent with
     * every other quick-position command in this project (maximize,
     * smart placement, transient centering, and now the keyboard's
     * own corner moves in 'ik_handle_move', input/kbd/interact.c),
     * none of which would tuck a client under a panel or the tray
     * reserving space at that same edge. */
    if (!ccmd_client_monitor_workarea(client, &mx, &my, &sw, &sh) &&
            !ccmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    target = ccmd_target_win(client);
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
void ccmd_client_move_to_monitor(client_td *client, uint32_t monitor_index)
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

    if (!ccmd_client_monitor(client, &surface, &cur_monitor) ||
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

    target = ccmd_target_win(client);
    xcb_configure_window(client->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) new_x, (uint32_t) new_y});
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    client->rule_position_locked = false;
    wm_request_client_redraw(client);
}


/* Move the client to the next monitor on its own surface */
void ccmd_client_move_to_next_monitor(client_td *client)
{
    surface_td *surface = NULL;
    monitor_td cur_monitor;
    uint32_t cur_idx = 0u;

    if (client == NULL) {
        return;
    }

    if (!ccmd_client_monitor(client, &surface, &cur_monitor) ||
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

    ccmd_client_move_to_monitor(client,
            (cur_idx + 1u) % surface->monitor_count);
}


/**
 * @brief Configure a client to the given frame geometry
 *
 * The single actual configure point used by @c ccmd_client_resize,
 * whether the request is applied right away (an unsynchronized client,
 * or the first step of a synchronized one) or later, once a pending
 * @c _NET_WM_SYNC_REQUEST acknowledgement arrives (see
 * @c ccmd_client_resize_flush_pending).
 *
 * @param client Window to resize
 * @param req_x  Requested frame X
 * @param req_y  Requested frame Y
 * @param req_w  Requested frame width
 * @param req_h  Requested frame height
 */
static void s_ccmd_resize_configure(client_td *client,
        int32_t req_x, int32_t req_y, uint32_t req_w, uint32_t req_h)
{
    xcb_window_t target;
    uint16_t mask;

    target = ccmd_target_win(client);
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
     * 'ccmd_client_resize' is the single configure point, called
     * directly for every resize, interactive or not. */
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
static void s_ccmd_resize_send_sync_request(client_td *client)
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
 * in @c ccmd_client_resize rather than dispatched immediately, so at
 * most one unacknowledged frame is ever in flight.
 *
 * @param client Client to resize
 * @param req_x  Requested frame X
 * @param req_y  Requested frame Y
 * @param req_w  Requested frame width
 * @param req_h  Requested frame height
 */
static void s_ccmd_resize_dispatch_synced(client_td *client,
        int32_t req_x, int32_t req_y, uint32_t req_w, uint32_t req_h)
{
    s_ccmd_resize_send_sync_request(client);
    s_ccmd_resize_configure(client, req_x, req_y, req_w, req_h);
}


/* Resize the client to new dimensions */
/* Resize the client to new dimensions */
void ccmd_client_resize(client_td *client, int32_t x, int32_t y,
        uint32_t w, uint32_t h)
{
    bool synced;

    if (client == NULL) {
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
        ccmd_client_unshade(client);
    }

    synced = client->has_net_wm_sync_request && wm_sync_available();

    if (!synced) {
        s_ccmd_resize_configure(client, x, y, w, h);
        return;
    }

    if (!client->sync_waiting) {
        s_ccmd_resize_dispatch_synced(client, x, y, w, h);
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
        s_ccmd_resize_dispatch_synced(client, x, y, w, h);
        return;
    }

    client->sync_pending_geom.x = x;
    client->sync_pending_geom.y = y;
    client->sync_pending_geom.w = w;
    client->sync_pending_geom.h = h;
    client->sync_has_pending = true;
}


/* Apply a client's pending '_NET_WM_SYNC_REQUEST'-throttled resize */
void ccmd_client_resize_flush_pending(client_td *client)
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
    s_ccmd_resize_dispatch_synced(client,
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
 *         back to @c ccmd_screen_dim's raw screen size in that case
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
bool ccmd_client_monitor_workarea(client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y,
        uint16_t *restrict out_w, uint16_t *restrict out_h)
{
    surface_td *surface = NULL;
    const desktop_td *desktop;
    monitor_td monitor;
    struct geometry_s clipped;
    uint32_t did;

    if (client == NULL || out_w == NULL || out_h == NULL) {
        return false;
    }

    if (!ccmd_client_monitor(client, &surface, &monitor)) {
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


/**
 * @brief Re-fill an already-maximized client's own geometry against
 *        its current workarea
 *
 * A maximized client's own geometry, grown or shrunk in place, is
 * only ever right immediately after actually maximizing it: anything
 * that later changes what its own workarea resolves to (a panel
 * mapped or unmapped, @c desktops.margins reloaded, or the surface's
 * own strutless-maximization mode,
 * @a surface_action_toggle_strutless_maximize,
 * surface.h, toggled) leaves it still filling wherever the OLD
 * workarea was, not the new one, until something re-applies its
 * maximize geometry from scratch.  This does exactly that: resolved
 * against @a ccmd_client_monitor_workarea (the same resolution
 * @a ccmd_client_maximize itself already uses), so the client ends up
 * exactly refilling the workarea as it now stands, the same as if it
 * had only just been maximized.
 *
 * Only the axis (or axes) @p client's own @c properties.state
 * actually names gets touched: a client maximized on one axis alone
 * keeps its own other axis exactly as it already was, rather than
 * growing it to fill the workarea too and silently turning a
 * horizontal- or vertical-only maximize into a full one.
 *
 * @param client Client to re-fill; a no-op unless it is currently
 *               maximized on at least one axis
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_refill_maximized(client_td *client)
{
    int32_t mx = 0;
    int32_t my = 0;
    uint16_t sw;
    uint16_t sh;
    const desktop_td *own_desktop;
    bool is_active;
    xcb_window_t target;
    bool touch_x;
    bool touch_y;
    uint32_t border;
    uint16_t mask;
    uint32_t values[4];
    int n;

    if (client == NULL || !client_is_maximized_any(client)) {
        return;
    }

    if (!ccmd_client_monitor_workarea(client, &mx, &my, &sw, &sh)) {
        return;
    }

    own_desktop = wm_get_client_desktop(client);
    is_active = own_desktop != NULL &&
        own_desktop->client_active_id == client->id;

    target = ccmd_target_win(client);
    touch_x = client->properties.state !=
        (uint16_t) CLIENT_STATE_MAXIMIZED_VERT;
    touch_y = client->properties.state !=
        (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    border = 2u * client_border_width(client, is_active);
    mask = 0u;
    n = 0;

    sw = (uint16_t) ((sw > border) ? sw - border : 0u);
    sh = (uint16_t) ((sh > border) ? sh - border : 0u);

    if (touch_x) {
        mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH;
        client->layout.geometry.cur.pos.x = mx;
        client->layout.geometry.cur.dim.w = sw;
    }
    if (touch_y) {
        mask |= XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT;
        client->layout.geometry.cur.pos.y = my;
        client->layout.geometry.cur.dim.h = sh;
    }

    /* 'xcb_configure_window' requires its own value list in ascending
     * 'XCB_CONFIG_WINDOW_*' bit order (X, Y, then WIDTH, HEIGHT);
     * built here explicitly rather than indexed by bit position so
     * skipping the untouched axis's own two fields (X+WIDTH or
     * Y+HEIGHT) still leaves the fields that are set in the right
     * order. */
    if (mask & XCB_CONFIG_WINDOW_X) {
        values[n++] = (uint32_t) client->layout.geometry.cur.pos.x;
    }
    if (mask & XCB_CONFIG_WINDOW_Y) {
        values[n++] = (uint32_t) client->layout.geometry.cur.pos.y;
    }
    if (mask & XCB_CONFIG_WINDOW_WIDTH) {
        values[n++] = client->layout.geometry.cur.dim.w;
    }
    if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
        values[n++] = client->layout.geometry.cur.dim.h;
    }

    if (mask != 0u) {
        xcb_configure_window(client->connection, target, mask, values);
    }

    if (client->frame != 0) {
        client_sync_decoration_layout(client);
    }

    client_send_synthetic_configure_notify(client->connection, client);
    wm_request_client_redraw(client);
}


/**
 * @brief Precondition checks shared by @c ccmd_client_maximize,
 *        @c ccmd_client_maximize_horz, and @c ccmd_client_maximize_vert,
 *        restoring an iconified client and unshading a shaded one
 *        along the way
 *
 * @param client Client about to be maximized, on one axis or both
 *
 * @return @c true if the caller should proceed (the client is
 *         resizable, not fullscreen, and any prior iconified or
 *         shaded state has already been cleared); @c false if
 *         @p client is @c NULL or the maximize should be refused
 *         outright
 *
 * @note Complexity: @e O(1)
 */
static bool s_ccmd_maximize_precheck(client_td *client)
{
    if (client == NULL) {
        return false;
    }

    if (!client_is_resizable(client) || client_is_fullscreen(client)) {
        return false;
    }

    /* An iconified client's own target window is unmapped and its
     * icon window stands in for it; maximizing it in place here
     * would map the frame back while the icon window is still up,
     * the same reasoning as the identical guard in
     * 'ccmd_client_shade' and 'ccmd_client_fullscreen' (state.c). */
    if (client_is_iconified(client)) {
        ccmd_client_restore(client);
    }

    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    return true;
}


/* Maximize the client horizontally, or restore/demote/complete
 * depending on its current maximize state */
void ccmd_client_maximize_horz(client_td *client)
{
    int32_t mx = 0;
    uint16_t sw;
    uint16_t unused_h;
    xcb_window_t target;
    const desktop_td *own_desktop;
    bool is_active;
    uint32_t border;

    if (client == NULL) {
        return;
    }

    if (!ccmd_client_monitor_workarea(client, &mx, NULL, &sw, &unused_h) &&
            !ccmd_screen_dim(client, &sw, NULL)) {
        return;
    }

    /* Same reservation 'ccmd_client_maximize' above already makes,
     * for the same reason (see its own comment there); only the
     * horizontal axis is at stake here, so only 'sw' needs it. */
    own_desktop = wm_get_client_desktop(client);
    is_active = own_desktop != NULL &&
        own_desktop->client_active_id == client->id;
    border = 2u * client_border_width(client, is_active);

    sw = (uint16_t) ((sw > border) ? sw - border : 0u);

    if (!s_ccmd_maximize_precheck(client)) {
        return;
    }

    target = ccmd_target_win(client);

    /* Toggling the horizontal axis off restores just that axis from
     * 'layout.geometry.old', leaving the vertical one exactly as it
     * currently is, rather than the full 'client_geometry_restore'
     * the pure horizontal-only case used to call, which would
     * overwrite a still-maximized vertical axis too: fully maximized
     * demotes to vertical-only, and horizontal-only demotes to
     * normal. */
    if (client->properties.state == CLIENT_STATE_MAXIMIZED ||
            client->properties.state == CLIENT_STATE_MAXIMIZED_HORZ) {
        bool was_full =
            (client->properties.state == CLIENT_STATE_MAXIMIZED);

        client->layout.geometry.cur.pos.x =
            client->layout.geometry.old.pos.x;
        client->layout.geometry.cur.dim.w =
            client->layout.geometry.old.dim.w;
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH,
                (const uint32_t[]) {
                    (uint32_t) client->layout.geometry.cur.pos.x,
                    client->layout.geometry.cur.dim.w
                });
        client->properties.state = (was_full)
            ? CLIENT_STATE_MAXIMIZED_VERT : CLIENT_STATE_NORMAL;
        ccmd_rem_states(client, 1, "_NET_WM_STATE_MAXIMIZED_HORZ");
        wm_request_client_redraw(client);
        return;
    }

    /* Already vertically maximized: complete to full maximize instead
     * of overwriting the state with a fresh horizontal-only one,
     * which would otherwise strand the vertical maximize's own
     * Y/height with no state left recording it, and announce only
     * 'MAXIMIZED_HORZ' over EWMH despite the window ending up
     * covering the workarea on both axes. */
    if (client->properties.state == CLIENT_STATE_MAXIMIZED_VERT) {
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH,
                (const uint32_t[]) {
                    (uint32_t) mx,
                    (uint32_t) sw
                });
        client->layout.geometry.cur.pos.x = mx;
        client->layout.geometry.cur.dim.w = sw;
        client->properties.state = CLIENT_STATE_MAXIMIZED;
        ccmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
        ccmd_add_states(client, 1, "_NET_WM_STATE_MAXIMIZED_HORZ");
        wm_request_client_redraw(client);
        return;
    }

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

    ccmd_rem_states(client, 2,
            "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_MAXIMIZED_VERT");
    ccmd_add_states(client, 1, "_NET_WM_STATE_MAXIMIZED_HORZ");
    wm_request_client_redraw(client);
}


/* Maximize the client vertically, or restore/demote/complete
 * depending on its current maximize state */
void ccmd_client_maximize_vert(client_td *client)
{
    int32_t my = 0;
    uint16_t sh;
    uint16_t unused_w;
    xcb_window_t target;
    const desktop_td *own_desktop;
    bool is_active;
    uint32_t border;

    if (client == NULL) {
        return;
    }

    if (!ccmd_client_monitor_workarea(client, NULL, &my, &unused_w, &sh) &&
            !ccmd_screen_dim(client, NULL, &sh)) {
        return;
    }

    /* Same reservation 'ccmd_client_maximize' above already makes,
     * for the same reason (see its own comment there); only the
     * vertical axis is at stake here, so only 'sh' needs it. */
    own_desktop = wm_get_client_desktop(client);
    is_active = own_desktop != NULL &&
        own_desktop->client_active_id == client->id;
    border = 2u * client_border_width(client, is_active);

    sh = (uint16_t) ((sh > border) ? sh - border : 0u);

    if (!s_ccmd_maximize_precheck(client)) {
        return;
    }

    target = ccmd_target_win(client);

    /* Toggling the vertical axis off restores just that axis from
     * 'layout.geometry.old', leaving the horizontal one exactly as it
     * currently is, rather than the full 'client_geometry_restore'
     * the pure vertical-only case used to call, which would overwrite
     * a still-maximized horizontal axis too: fully maximized demotes
     * to horizontal-only, and vertical-only demotes to normal. */
    if (client->properties.state == CLIENT_STATE_MAXIMIZED ||
            client->properties.state == CLIENT_STATE_MAXIMIZED_VERT) {
        bool was_full =
            (client->properties.state == CLIENT_STATE_MAXIMIZED);

        client->layout.geometry.cur.pos.y =
            client->layout.geometry.old.pos.y;
        client->layout.geometry.cur.dim.h =
            client->layout.geometry.old.dim.h;
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    (uint32_t) client->layout.geometry.cur.pos.y,
                    client->layout.geometry.cur.dim.h
                });
        client->properties.state = (was_full)
            ? CLIENT_STATE_MAXIMIZED_HORZ : CLIENT_STATE_NORMAL;
        ccmd_rem_states(client, 1, "_NET_WM_STATE_MAXIMIZED_VERT");
        wm_request_client_redraw(client);
        return;
    }

    /* Already horizontally maximized: complete to full maximize
     * instead of overwriting the state with a fresh vertical-only
     * one, which would otherwise strand the horizontal maximize's own
     * X/width with no state left recording it, and announce only
     * 'MAXIMIZED_VERT' over EWMH despite the window ending up
     * covering the workarea on both axes. */
    if (client->properties.state == CLIENT_STATE_MAXIMIZED_HORZ) {
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    (uint32_t) my,
                    (uint32_t) sh
                });
        client->layout.geometry.cur.pos.y = my;
        client->layout.geometry.cur.dim.h = sh;
        client->properties.state = CLIENT_STATE_MAXIMIZED;
        ccmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
        ccmd_add_states(client, 1, "_NET_WM_STATE_MAXIMIZED_VERT");
        wm_request_client_redraw(client);
        return;
    }

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

    ccmd_rem_states(client, 2,
            "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_MAXIMIZED_HORZ");
    ccmd_add_states(client, 1, "_NET_WM_STATE_MAXIMIZED_VERT");
    wm_request_client_redraw(client);
}


/* Maximize the client entirely, or restore it if already maximized */
void ccmd_client_maximize(client_td *client)
{
    int32_t mx = 0;
    int32_t my = 0;
    uint16_t sw;
    uint16_t sh;
    xcb_window_t target;
    const desktop_td *own_desktop;
    bool is_active;
    uint32_t border;

    if (client == NULL) {
        return;
    }

    if (!s_ccmd_maximize_precheck(client)) {
        return;
    }

    /* Toggle: only restore if already fully maximized; a window
     * maximized on just one axis (horizontal or vertical) falls
     * through to the "maximize" branch below instead, completing it
     * to full maximize on the other axis too. */
    if (client->properties.state == CLIENT_STATE_MAXIMIZED) {
        target = ccmd_target_win(client);
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
        ccmd_rem_states(client, 2,
                "_NET_WM_STATE_MAXIMIZED_HORZ",
                "_NET_WM_STATE_MAXIMIZED_VERT");
        wm_request_client_redraw(client);
        return;
    }

    if (!ccmd_client_monitor_workarea(client, &mx, &my, &sw, &sh) &&
            !ccmd_screen_dim(client, &sw, &sh)) {
        return;
    }

    /* Per the X11 protocol (ConfigureWindow), 'x'/'y' name a window's
     * own top-left corner including its native border, if any, drawn
     * growing rightward/downward from there: the full on-screen
     * footprint of a client with one reaches all the way to
     * 'x + 2 * border + w', 2 * border wider/taller than 'w' alone
     * (equivalently for height).
     *
     * 'client_border_width' (obviously in 'client.h') is 0 for
     * a decorated client (its own frame is always created with a native
     * border of 0; its themed margin is already fully accounted for
     * elsewhere, in its own frame dimensions), so this only ever
     * actually shrinks the target for an undecorated one.
     *
     * Keeping its own full footprint within the workarea/monitor rect
     * 'sw'/'sh' just resolved above, rather than spilling its own
     * border past its own right/bottom edge. */
    own_desktop = wm_get_client_desktop(client);
    is_active = own_desktop != NULL &&
        own_desktop->client_active_id == client->id;
    border = 2u * client_border_width(client, is_active);

    sw = (uint16_t) ((sw > border) ? sw - border : 0u);
    sh = (uint16_t) ((sh > border) ? sh - border : 0u);

    target = ccmd_target_win(client);
    /* Only remember the geometry to restore to if it is not already
     * a maximized state's geometry: switching from horizontal-only or
     * vertical-only maximize to full maximize must not overwrite the
     * true pre-maximize geometry already held in 'layout.geometry.old'
     * (see 'client_is_maximized_any'), or restoring later would land at
     * whichever partial-maximize size happened to be current, instead
     * of the window's original one */
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

    ccmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    ccmd_add_states(client, 2,
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");
    wm_request_client_redraw(client);
}
