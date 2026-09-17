/**
 * @file cmds/client/resize.c
 *
 * @brief Client resize command implementation
 *
 * One of the files @c cmds/client/ is made of.  Covers resizing
 * a client, including the @c _NET_WM_SYNC_REQUEST throttling pipeline
 * that paces an ongoing interactive resize against the client's redraw
 * acknowledgements.
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

/* Default initial values */
#include <defs/client.h>     /* WM_SYNC_MAX_WAIT_TICKS */

/* Utils includes */
#include <utils/geom.h>
#include <utils/xcb/connection.h>

/* Project includes */
#include <client.h>
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/move.h>
#include <cmds/client/resize.h>
#include <cmds/client/screen.h>
#include <cmds/client/state.h>


/**
 * @brief Configure a client to the given frame geometry
 *
 * The single actual configure point used by @a ccmd_client_resize,
 * whether the request is applied right away (an unsynchronized client,
 * or the first step of a synchronized one) or later, once a pending
 * @c _NET_WM_SYNC_REQUEST acknowledgement arrives.
 *
 * @param client   Window to resize
 * @param req_geom Requested frame position and dimensions
 *
 * @see @a ccmd_client_resize_flush_pending
 */
static void s_ccmd_resize_configure(client_td *client,
        struct geometry_s req_geom)
{
    xcb_window_t target;
    uint16_t mask;
    uint32_t ext_w;
    uint32_t ext_h;
    uint32_t content_w;
    uint32_t content_h;

    /* A second barrier against whatever 'req_geom' the caller handed
     * down, on top of whichever one that caller may already have
     * applied of its own: 'client_size_constrain' ('client.h') expects
     * its width/height in terms of the client's content window, what
     * its 'WM_NORMAL_HINTS' actually describe (ICCCM §4.1.2.3), not the
     * frame dimensions 'req_geom' carries here, so the round trip
     * through content space mirrors the one 's_drag_resize_geometry'
     * ('input/mouse/drag.c') and 's_kbd_resize_apply'
     * ('input/kbd/interact.c') already make for the same reason.
     *
     * 'geom_dim_clamp' ('utils/geom.h') runs last as a plain sanity
     * floor and ceiling completely independent of any hint the client
     * declared, which matters because both fields are 'uint32_t':
     * a caller that ever let a subtraction go negative before storing
     * it here would hand this function an enormous value rather than
     * a negative one, and 'client_size_constrain' alone only clamps
     * against a maximum the client actually declared, which most never
     * do */
    ext_w = (uint32_t) client->layout.frame_extents.left +
        (uint32_t) client->layout.frame_extents.right;
    ext_h = (uint32_t) client->layout.frame_extents.top +
        (uint32_t) client->layout.frame_extents.bottom;
    content_w = (req_geom.dim.w > ext_w) ? req_geom.dim.w - ext_w : 0u;
    content_h = (req_geom.dim.h > ext_h) ? req_geom.dim.h - ext_h : 0u;

    client_size_constrain(client, &content_w, &content_h);

    req_geom.dim.w = geom_dim_clamp((int32_t) (content_w + ext_w));
    req_geom.dim.h = geom_dim_clamp((int32_t) (content_h + ext_h));

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
     * leaving stale content until the next user-triggered redraw.
     *
     * Skipped for a client already using '_NET_WM_SYNC_REQUEST':
     * that protocol already tells it exactly when to redraw, via the
     * sync request this same call chain sends further down in
     * 'ccmd_client_resize', so forcing a clear here on top of that,
     * every single step of an interactive resize, only blanks
     * a compositing client's (Chromium, Electron) own content out
     * from under it a moment before it repaints on its own, seen as
     * a brief flicker on every step rather than a smooth resize. */
    if (!client->hints_ewmh.sync.is_supported ||
            !wm_sync_is_available()) {
        xcb_clear_area(xcb_connection_get(), 1, client->window,
                0, 0, 0, 0);
    }

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
    client_send_synthetic_configure_notify(xcb_connection_get(), client);
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
 * @param client Client to notify; must have
 *               @c hints_ewmh.sync.is_supported
 */
static void s_ccmd_resize_send_sync_request(client_td *client)
{
    xcb_client_message_event_t ev;
    const xcb_ewmh_connection_t *const ewmh =
        xcb_ewmh_connection_get();

    client->hints_ewmh.sync.value += 1u;
    client->hints_ewmh.sync.is_waiting = true;
    client->hints_ewmh.sync.wait_ticks = 0u;

    if (ewmh == NULL) {
        return;
    }

    memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.format = 32;
    ev.window = client->window;
    ev.type = ewmh->WM_PROTOCOLS;
    ev.data.data32[0] = ewmh->_NET_WM_SYNC_REQUEST;
    ev.data.data32[1] = XCB_CURRENT_TIME;
    ev.data.data32[2] = client->hints_ewmh.sync.value;
    ev.data.data32[3] = 0u;    /* high 32 bits: always 0 at our scale */
    xcb_send_event(xcb_connection_get(), 0, client->window,
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
 * Resizing is forbidden outright only where neither axis has anything
 * free to resize at all: fully maximized (both axes) or fullscreen.
 * A client maximized on just one axis, that is,
 * @c CLIENT_STATE_MAXIMIZED_HORZ or @c CLIENT_STATE_MAXIMIZED_VERT, is
 * deliberately let through here: its free axis stays genuinely
 * resizable, matching every one of this project's interactive resize
 * entry points (mouse border drag via @a drag_start_resize_axis_locked,
 * its matching mouse-bound keybinding, and keyboard resize in
 * @c input/kbd/interact.c), each of which already freezes the maximized
 * axis's own dimension at its current value before ever calling down to
 * this function.
 *
 * Refusing the whole call here regardless silently drops every live
 * resize update a solid drag sends along the way, and strands
 * a non-solid (outline) drag's final call off screen for good (see
 * @c drag_end's comment on @c enact_client_resize_force, in
 * @c input/mouse/drag.c), since that call exists specifically to bring
 * the real window back from where a non-solid drag parks it for the
 * drag's duration, and this same refusal silently swallowed that too.
 * A shaded client is first restored so the requested size applies to
 * the normal window geometry instead of the rolled-up titlebar.
 *
 * @param client Client about to be resized
 *
 * @return @c true if resizing @p client is currently allowed
 *
 * @note Complexity: @e O(1)
 */
static bool s_ccmd_resize_allowed(client_td *client)
{
    if (client_is_fullscreen(client) ||
            client_is_maximized(client)) {
        return false;
    }

    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    return true;
}


void ccmd_client_resize(client_td *client, struct geometry_s geom)
{
    bool synced;

    if (client == NULL || !s_ccmd_resize_allowed(client)) {
        return;
    }

    synced = client->hints_ewmh.sync.is_supported &&
        wm_sync_is_available();

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


/* Resize the client to new dimensions immediately */
void ccmd_client_resize_force(client_td *client, struct geometry_s geom)
{
    if (client == NULL || !s_ccmd_resize_allowed(client)) {
        return;
    }

    /* Discard any geometry left queued by an earlier, still-
     * unacknowledged exchange: applying this call's geometry below
     * already supersedes it, and leaving it set would let a late
     * 'AlarmNotify' for that older exchange silently revert this one
     * the next time 'ccmd_client_resize_flush_pending' runs */
    client->hints_ewmh.sync.has_pending = false;

    if (client->hints_ewmh.sync.is_supported && wm_sync_is_available()) {
        /* Still tells a sync-aware client about the new size (so its
         * own internal counter stays in step), but this call itself
         * never waits on or queues behind that acknowledgment the way
         * 'ccmd_client_resize' does; see the header's comment for when
         * this is the right call to make instead */
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
