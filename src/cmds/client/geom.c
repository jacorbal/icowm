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

/* Type includes */
#include <types/pair.h>

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


/**
 * @brief Apply a client's geometry to its target window in a single
 *        XCB call
 *
 * Openbox's own real answer to configuring a window's geometry
 * (confirmed directly against its source, @c client_configure in @c
 * client.c): one shared function every geometry-changing operation
 * funnels through, rather than each one building its own @c
 * xcb_configure_window values array by hand.  @c XCB_CONFIG_WINDOW_*
 * bit values themselves fix the order @c xcb_configure_window's own
 * values array must list whichever fields @p mask selects in (@c X @c
 * <@c Y @c <@c WIDTH @c <@c HEIGHT @c <@c BORDER_WIDTH, confirmed
 * directly against @c xproto.h), the exact ordering every one of this
 * function's own former call sites had to get right by hand, on its
 * own, every single time; this function gets it right once.
 *
 * Deliberately narrow in scope: only the single @c xcb_configure_
 * window call itself, nothing about updating @p client's own tracked
 * @c layout.geometry.cur fields to match, which stays each caller's
 * own concern, since which fields to track (and anything else a
 * caller needs alongside, such as clearing @c has_rule_position_
 * locked) genuinely varies from one call site to the next in ways a
 * single shared function covering both would only obscure.
 *
 * @param client       Client whose target window to configure
 * @param target       Window to configure; @a ccmd_target_win's own
 *                      result, the frame for a decorated client or
 *                      the bare content window otherwise
 * @param mask         Bitwise OR of whichever @c XCB_CONFIG_WINDOW_X/
 *                      @c _Y/@c _WIDTH/@c _HEIGHT/@c _BORDER_WIDTH
 *                      bits are actually changing; a field whose own
 *                      bit is not set here is never read at all,
 *                      whatever @p x/@p y/@p w/@p h/@p border_width
 *                      themselves happen to hold
 * @param x            New X position, only applied if @c XCB_CONFIG_
 *                      WINDOW_X is set in @p mask
 * @param y            New Y position, only applied if @c XCB_CONFIG_
 *                      WINDOW_Y is set in @p mask
 * @param w            New width, only applied if @c XCB_CONFIG_
 *                      WINDOW_WIDTH is set in @p mask
 * @param h            New height, only applied if @c XCB_CONFIG_
 *                      WINDOW_HEIGHT is set in @p mask
 * @param border_width New native border width, only applied if @c
 *                      XCB_CONFIG_WINDOW_BORDER_WIDTH is set in
 *                      @p mask
 *
 * @note A null @p client, one with no connection, or a @c XCB_WINDOW_
 *       NONE @p target is a silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_apply_geometry(client_td *client, xcb_window_t target,
        uint16_t mask, int32_t x, int32_t y, uint32_t w, uint32_t h,
        uint32_t border_width)
{
    uint32_t values[5];
    uint32_t num = 0;

    if (client == NULL || client->connection == NULL ||
            target == XCB_WINDOW_NONE) {
        return;
    }

    if (mask & (uint16_t) XCB_CONFIG_WINDOW_X) {
        values[num++] = (uint32_t) x;
    }
    if (mask & (uint16_t) XCB_CONFIG_WINDOW_Y) {
        values[num++] = (uint32_t) y;
    }
    if (mask & (uint16_t) XCB_CONFIG_WINDOW_WIDTH) {
        values[num++] = w;
    }
    if (mask & (uint16_t) XCB_CONFIG_WINDOW_HEIGHT) {
        values[num++] = h;
    }
    if (mask & (uint16_t) XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        values[num++] = border_width;
    }

    xcb_configure_window(client->connection, target, mask, values);
}


/* Move the client to a new position */
/* Move the client to a new position */
void ccmd_client_move(client_td *client, struct position_s pos)
{
    xcb_window_t target;

    if (client == NULL || client_is_maximized(client) ||
            client_is_fullscreen(client)) {
        return;
    }

    target = ccmd_target_win(client);
    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y,
            pos.x, pos.y, 0u, 0u, 0u);
    client->layout.geometry.cur.pos.x = pos.x;
    client->layout.geometry.cur.pos.y = pos.y;
    client->has_rule_position_locked = false;
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

    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y,
            x, y, 0u, 0u, 0u);
    client->layout.geometry.cur.pos.x = x;
    client->layout.geometry.cur.pos.y = y;
    client->has_rule_position_locked = false;
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
    ccmd_client_apply_geometry(client, target,
            (uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y,
            new_x, new_y, 0u, 0u, 0u);
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    client->has_rule_position_locked = false;
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


/* Move the client to the previous monitor on its own surface */
void ccmd_client_move_to_prev_monitor(client_td *client)
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
            (cur_idx + surface->monitor_count - 1u) %
                surface->monitor_count);
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
 * @param client   Window to resize
 * @param req_geom Requested frame position and dimensions
 */
static void s_ccmd_resize_configure(client_td *client,
        struct geometry_s req_geom)
{
    xcb_window_t target;
    uint16_t mask;

    target = ccmd_target_win(client);
    mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    if (req_geom.pos.x != client->layout.geometry.cur.pos.x ||
            req_geom.pos.y != client->layout.geometry.cur.pos.y) {
        mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    }

    ccmd_client_apply_geometry(client, target, mask,
            req_geom.pos.x, req_geom.pos.y,
            req_geom.dim.w, req_geom.dim.h, 0u);

    client->layout.geometry.cur = req_geom;

    /* For decorated (reparented) clients the inner window must be
     * repositioned and resized to match the new frame dimensions.
     * 'ccmd_client_resize' is the single configure point, called
     * directly for every resize, interactive or not. */
    client_decoration_layout_sync(client);

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
     * from 'client_decoration_layout_sync'; the synthetic event
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
 * @param client Client to notify; must have @c hints_ewmh.sync.is_supported
 */
static void s_ccmd_resize_send_sync_request(client_td *client)
{
    xcb_client_message_event_t ev;

    client->hints_ewmh.sync.value += 1u;
    client->hints_ewmh.sync.is_waiting = true;
    client->hints_ewmh.sync.wait_ticks = 0u;

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
    ev.data.data32[2] = client->hints_ewmh.sync.value;
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
 * @param client   Client to resize
 * @param req_geom Requested frame position and dimensions
 */
static void s_ccmd_resize_dispatch_synced(client_td *client,
        struct geometry_s req_geom)
{
    s_ccmd_resize_send_sync_request(client);
    s_ccmd_resize_configure(client, req_geom);
}


/**
 * @brief Shared precondition check for either resize entry point
 *
 * Resizing is forbidden outright only where neither axis has
 * anything free to resize at all: fully maximized (both axes) or
 * fullscreen.  A client maximized on just one axis (@c
 * CLIENT_STATE_MAXIMIZED_HORZ/@c _VERT) is deliberately let through
 * here: its own free axis stays genuinely resizable, matching every
 * one of this project's own interactive resize entry points (mouse
 * border drag via @a drag_start_resize_axis_locked, its matching
 * mouse-bound keybinding, and keyboard resize in @c input/kbd/
 * interact.c), each of which already freezes the maximized axis's
 * own dimension at its current value before ever calling down to
 * this function; refusing the whole call here regardless, the way
 * this check used to, silently dropped every live resize update a
 * solid drag sent along the way, and stranded a non-solid (outline)
 * drag's own final call off screen for good (see @c drag_end's own
 * comment on @c enact_client_resize_force, @c input/mouse/drag.c),
 * since that call exists specifically to bring the real window back
 * from where a non-solid drag parks it for the drag's own duration,
 * and this same refusal silently swallowed that too.  A shaded
 * client is first restored so the requested size applies to the
 * normal window geometry instead of the rolled-up titlebar.
 *
 * @param client Client about to be resized
 *
 * @return @c true if resizing @p client is currently allowed
 *
 * @note Complexity: @e O(1)
 */
static bool s_ccmd_resize_allowed(client_td *client)
{
    if (client->properties.state == (uint16_t) CLIENT_STATE_FULLSCREEN ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED) {
        return false;
    }

    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    return true;
}


/* Resize the client to new dimensions */
void ccmd_client_resize(client_td *client, struct geometry_s geom)
{
    bool synced;

    if (client == NULL || !s_ccmd_resize_allowed(client)) {
        return;
    }

    synced = client->hints_ewmh.sync.is_supported && wm_sync_is_available();

    if (!synced) {
        s_ccmd_resize_configure(client, geom);
        return;
    }

    if (!client->hints_ewmh.sync.is_waiting) {
        s_ccmd_resize_dispatch_synced(client, geom);
        return;
    }

    /* Already waiting on the previous request's 'AlarmNotify': queue
     * this geometry instead of piling up unacknowledged configures,
     * unless the client has already gone 'WM_SYNC_MAX_WAIT_TICKS'
     * attempts without acknowledging, in which case give up waiting and
     * apply this one directly, so an unresponsive client can never
     * freeze interactive resize */
    client->hints_ewmh.sync.wait_ticks += 1u;
    if (client->hints_ewmh.sync.wait_ticks >
            (uint8_t) WM_SYNC_MAX_WAIT_TICKS) {
        client->hints_ewmh.sync.has_pending = false;
        s_ccmd_resize_dispatch_synced(client, geom);
        return;
    }

    client->hints_ewmh.sync.pending_geom = geom;
    client->hints_ewmh.sync.has_pending = true;
}


/* Resize the client to new dimensions immediately, bypassing any
 * in-flight sync throttling */
void ccmd_client_resize_force(client_td *client, struct geometry_s geom)
{
    if (client == NULL || !s_ccmd_resize_allowed(client)) {
        return;
    }

    /* Discard any geometry left queued by an earlier, still-
     * unacknowledged exchange: applying this call's own geometry
     * below already supersedes it, and leaving it set would let a
     * late 'AlarmNotify' for that older exchange silently revert
     * this one the next time 'ccmd_client_resize_flush_pending' runs */
    client->hints_ewmh.sync.has_pending = false;

    if (client->hints_ewmh.sync.is_supported && wm_sync_is_available()) {
        /* Still tells a sync-aware client about the new size (so its
         * own internal counter stays in step), but this call itself
         * never waits on or queues behind that acknowledgment the
         * way 'ccmd_client_resize' does; see the header's own doc
         * comment for when this is the right call to make instead */
        s_ccmd_resize_dispatch_synced(client, geom);
    } else {
        s_ccmd_resize_configure(client, geom);
    }
}


/* Apply a client's pending '_NET_WM_SYNC_REQUEST'-throttled resize */
void ccmd_client_resize_flush_pending(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client->hints_ewmh.sync.is_waiting = false;
    client->hints_ewmh.sync.wait_ticks = 0u;

    if (!client->hints_ewmh.sync.has_pending) {
        return;
    }

    client->hints_ewmh.sync.has_pending = false;
    s_ccmd_resize_dispatch_synced(client,
            client->hints_ewmh.sync.pending_geom);
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
    *out_w = geom_dim_clamp((int32_t) clipped.dim.w);
    *out_h = geom_dim_clamp((int32_t) clipped.dim.h);

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
    border = 2u * client_border_width(client, is_active, false);
    mask = 0u;

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

    if (mask != 0u) {
        ccmd_client_apply_geometry(client, target, mask,
                client->layout.geometry.cur.pos.x,
                client->layout.geometry.cur.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h, 0u);
    }

    if (client->frame != 0) {
        client_decoration_layout_sync(client);
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


/**
 * @brief Maximize a client on one axis or both, or restore/demote/
 *        complete depending on its current maximize state
 *
 * The shared implementation behind @c ccmd_client_maximize, @c ccmd_
 * client_maximize_horz, and @c ccmd_client_maximize_vert, each now a
 * thin wrapper passing its own fixed @p dir; matches Openbox's own
 * @c client_maximize (@c client.c), which takes the identical @p dir
 * convention for the identical reason: one function, one place the
 * demote/complete/fresh-maximize decision is made, rather than the
 * same three-way branch (see below) duplicated once per axis.
 *
 * Unlike Openbox, which tracks @c max_horz and @c max_vert as two
 * independent booleans, this project's own @c properties.state is a
 * single, mutually exclusive value (@c CLIENT_STATE_NORMAL, @c
 * _MAXIMIZED, @c _MAXIMIZED_HORZ, or @c _MAXIMIZED_VERT), so "is the
 * horizontal axis currently maximized" is derived (@c state @c == @c
 * MAXIMIZED @c || @c state @c == @c MAXIMIZED_HORZ) rather than read
 * directly off a field of its own; @p dir @c == @c 0 (both axes)
 * still only ever has the two cases Openbox's own top-level toggle
 * does (already fully maximized, so restore; anything else, so
 * maximize both, overriding whatever partial state was there), while
 * @p dir @c == @c 1 or @c 2 (one axis only) has the same three cases
 * every one of this project's former three-way per-axis branches
 * already had: demote this axis alone if it is the one currently
 * maximized (restoring from @c layout.geometry.old, keeping the
 * other axis exactly as it is), complete to full maximize if the
 * other axis is the one currently maximized (folding this axis in
 * from the workarea without disturbing the other), or maximize this
 * axis alone fresh otherwise (saving the pre-maximize geometry first,
 * unless some maximized state already holds it).
 *
 * @param client Client to maximize
 * @param dir    @c 0 for both axes, @c 1 for horizontal only, @c 2
 *               for vertical only
 *
 * @note A null @p client, or the precheck in @a s_ccmd_maximize_
 *       precheck failing, is a silent no-op
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_maximize_dir(client_td *client, int dir)
{
    int32_t mx = 0;
    int32_t my = 0;
    uint16_t sw = 0;
    uint16_t sh = 0;
    xcb_window_t target;
    const desktop_td *own_desktop;
    bool is_active;
    uint32_t border;
    bool want_horz = (dir == 0 || dir == 1);
    bool horz_now;
    bool vert_now;

    if (!s_ccmd_maximize_precheck(client)) {
        return;
    }

    horz_now = client->properties.state == CLIENT_STATE_MAXIMIZED ||
        client->properties.state == CLIENT_STATE_MAXIMIZED_HORZ;
    vert_now = client->properties.state == CLIENT_STATE_MAXIMIZED ||
        client->properties.state == CLIENT_STATE_MAXIMIZED_VERT;
    target = ccmd_target_win(client);

    /* Toggle: both axes fully maximized already restores to normal;
     * any other current state (normal, or maximized on just one
     * axis) falls through to maximizing both below instead,
     * overriding whatever partial state was there. */
    if (dir == 0 && horz_now && vert_now) {
        client_geometry_restore(client);
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                client->layout.geometry.cur.pos.x,
                client->layout.geometry.cur.pos.y,
                client->layout.geometry.cur.dim.w,
                client->layout.geometry.cur.dim.h, 0u);
        client->properties.state = CLIENT_STATE_NORMAL;
        ccmd_client_sync_states(client);
        wm_request_client_redraw(client);
        return;
    }

    /* Demote this single axis alone, restoring it from the saved
     * pre-maximize geometry and leaving the other axis exactly as it
     * currently is: fully maximized demotes to the other axis alone,
     * and this axis alone demotes to normal. Only reachable for a
     * single-axis 'dir'; 'dir == 0' either already returned above
     * (both axes maximized) or falls through to maximizing both
     * below regardless of any single axis's own current state. */
    if (dir != 0 && ((dir == 1 && horz_now) || (dir == 2 && vert_now))) {
        bool was_full = client->properties.state == CLIENT_STATE_MAXIMIZED;

        if (dir == 1) {
            client->layout.geometry.cur.pos.x =
                client->layout.geometry.old.pos.x;
            client->layout.geometry.cur.dim.w =
                client->layout.geometry.old.dim.w;
            ccmd_client_apply_geometry(client, target,
                    (uint16_t) XCB_CONFIG_WINDOW_X |
                        (uint16_t) XCB_CONFIG_WINDOW_WIDTH,
                    client->layout.geometry.cur.pos.x, 0,
                    client->layout.geometry.cur.dim.w, 0u, 0u);
            client->properties.state = (was_full)
                ? CLIENT_STATE_MAXIMIZED_VERT : CLIENT_STATE_NORMAL;
        } else {
            client->layout.geometry.cur.pos.y =
                client->layout.geometry.old.pos.y;
            client->layout.geometry.cur.dim.h =
                client->layout.geometry.old.dim.h;
            ccmd_client_apply_geometry(client, target,
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                        (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                    0, client->layout.geometry.cur.pos.y,
                    0u, client->layout.geometry.cur.dim.h, 0u);
            client->properties.state = (was_full)
                ? CLIENT_STATE_MAXIMIZED_HORZ : CLIENT_STATE_NORMAL;
        }
        ccmd_client_sync_states(client);
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
     * (equivalently for height).  'client_border_width' is 0 for a
     * decorated client, so this only ever actually shrinks the
     * target for an undecorated one; kept within the workarea/
     * monitor rect 'sw'/'sh' just resolved above, rather than
     * spilling its own border past its own right/bottom edge. */
    own_desktop = wm_get_client_desktop(client);
    is_active = own_desktop != NULL &&
        own_desktop->client_active_id == client->id;
    border = 2u * client_border_width(client, is_active, false);
    sw = (uint16_t) ((sw > border) ? sw - border : 0u);
    sh = (uint16_t) ((sh > border) ? sh - border : 0u);

    /* Complete this single axis to full maximize: the other axis is
     * already the one currently maximized, so fold this one in from
     * the workarea without disturbing it. Only reachable for a
     * single-axis 'dir'. */
    if (dir == 1 && vert_now) {
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH,
                mx, 0, sw, 0u, 0u);
        client->layout.geometry.cur.pos.x = mx;
        client->layout.geometry.cur.dim.w = sw;
        client->properties.state = CLIENT_STATE_MAXIMIZED;
        ccmd_client_sync_states(client);
        wm_request_client_redraw(client);
        return;
    }
    if (dir == 2 && horz_now) {
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                0, my, 0u, sh, 0u);
        client->layout.geometry.cur.pos.y = my;
        client->layout.geometry.cur.dim.h = sh;
        client->properties.state = CLIENT_STATE_MAXIMIZED;
        ccmd_client_sync_states(client);
        wm_request_client_redraw(client);
        return;
    }

    /* Maximize fresh: either both axes at once ('dir == 0', which can
     * only still reach here with neither axis currently fully
     * maximized), or this single axis alone with the other left
     * exactly as it is. Only remember the geometry to restore to if
     * it is not already a maximized state's geometry, or restoring
     * later would land at whichever partial-maximize size happened
     * to be current instead of the window's true original one. */
    if (!client_is_maximized_any(client)) {
        client_geometry_save(client);
    }

    if (dir == 0) {
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                mx, my, sw, sh, 0u);
        client->layout.geometry.cur.pos.x = mx;
        client->layout.geometry.cur.pos.y = my;
        client->layout.geometry.cur.dim.w = sw;
        client->layout.geometry.cur.dim.h = sh;
        client->properties.state = CLIENT_STATE_MAXIMIZED;
    } else if (want_horz) {
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_WIDTH,
                mx, client->layout.geometry.cur.pos.y, sw, 0u, 0u);
        client->layout.geometry.cur.pos.x = mx;
        client->layout.geometry.cur.dim.w = sw;
        client->properties.state = CLIENT_STATE_MAXIMIZED_HORZ;
    } else {
        ccmd_client_apply_geometry(client, target,
                (uint16_t) XCB_CONFIG_WINDOW_X |
                    (uint16_t) XCB_CONFIG_WINDOW_Y |
                    (uint16_t) XCB_CONFIG_WINDOW_HEIGHT,
                client->layout.geometry.cur.pos.x, my, 0u, sh, 0u);
        client->layout.geometry.cur.pos.y = my;
        client->layout.geometry.cur.dim.h = sh;
        client->properties.state = CLIENT_STATE_MAXIMIZED_VERT;
    }

    ccmd_client_sync_states(client);
    wm_request_client_redraw(client);
}


/* Maximize the client horizontally, or restore/demote/complete
 * depending on its current maximize state */
void ccmd_client_maximize_horz(client_td *client)
{
    s_ccmd_client_maximize_dir(client, 1);
}


/* Maximize the client vertically, or restore/demote/complete
 * depending on its current maximize state */
void ccmd_client_maximize_vert(client_td *client)
{
    s_ccmd_client_maximize_dir(client, 2);
}


/* Demote a single axis's maximize state alone, without touching
 * geometry at all; see this function's own Doxygen comment in
 * cmds/client/geom.h for why a caller would ever want that split */
void ccmd_client_demote_axis_state(client_td *client, int dir)
{
    bool was_full;

    if (client == NULL) {
        return;
    }

    was_full = client->properties.state == CLIENT_STATE_MAXIMIZED;
    if (dir == 1) {
        client->properties.state = (was_full)
            ? CLIENT_STATE_MAXIMIZED_VERT : CLIENT_STATE_NORMAL;
    } else {
        client->properties.state = (was_full)
            ? CLIENT_STATE_MAXIMIZED_HORZ : CLIENT_STATE_NORMAL;
    }
    ccmd_client_sync_states(client);
}


/* Maximize the client entirely, or restore it if already maximized */
void ccmd_client_maximize(client_td *client)
{
    s_ccmd_client_maximize_dir(client, 0);
}

