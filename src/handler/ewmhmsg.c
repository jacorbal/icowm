/**
 * @file handler/ewmhmsg.c
 *
 * @brief EWMH client-message sub-handlers
 *
 * Implements the per-message handler functions that are called from
 * @c handler_client_message in @c handler/message.c when a
 * @c CLIENT_MESSAGE event arrives for a specific EWMH atom.
 *
 * Separated from @c handler/message.c to keep that file focused on
 * dispatching while this module owns the protocol-level interpretation
 * of individual EWMH requests.
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
#include <stddef.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Command includes */
#include <cmds/client/basic.h>
#include <cmds/client/geom.h>
#include <cmds/client/layer.h>
#include <cmds/surface.h>

/* Input includes */
#include <input/mouse/drag.h>

/* Policy includes */
#include <policy/focus.h>

/* Utils includes */
#include <utils/xcb/atom.h>

/* Render includes */
#include <render/outdate.h>

/* Default initial values */
#include <defs/client.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <handler/internal.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Default initial values */
#include <defs/ewmh.h>


/**
 * @brief Resolve a @c _NET_WM_STATE action into "should this state
 *        end up set"
 *
 * Shared by every state @c s_handle_wm_state_atom below reacts to:
 * @c WM_STATE_ACTION_ADD always resolves to @c true, @c WM_STATE_
 * ACTION_REMOVE always to @c false, and @c WM_STATE_ACTION_TOGGLE
 * resolves to whichever @p already_set is not; each caller only
 * differs in how @p already_set itself is determined (a @c state or
 * @c layer comparison, a flag bit, or an existing @c client_is_*
 * predicate), which stays inline at each call site rather than
 * becoming yet another small function pointer to thread through.
 *
 * @param action      One of @c WM_STATE_ACTION_ADD/REMOVE/TOGGLE
 * @param already_set Whether the state in question is currently set
 *
 * @return @c true if the state should end up set
 *
 * @note Complexity: @e O(1)
 */
static bool s_wm_state_resolve_add(uint32_t action, bool already_set)
{
    return (action == WM_STATE_ACTION_ADD) ||
        (action == WM_STATE_ACTION_TOGGLE && !already_set);
}


/**
 * @brief Dispatch a single EWMH @c _NET_WM_STATE atom for a given action
 *
 * @param client     Pointer to the client being updated
 * @param state_atom EWMH @c _NET_WM_STATE atom to process
 * @param action     Requested state transition:
 *                   @c WM_STATE_ACTION_ADD;
 *                   @c WM_STATE_ACTION_REMOVE; or
 *                   @c WM_STATE_ACTION_TOGGLE
 * @param ewmh       Pointer to the EWMH connection handle
 *
 * @note No-op if @p client or @p ewmh are null
 * @note Complexity: @e O(1)
 */
static void s_handle_wm_state_atom(client_td *client,
        xcb_atom_t state_atom, uint32_t action,
        const xcb_ewmh_connection_t *ewmh)
{
    bool is_fullscreen;
    bool is_max_h;
    bool is_max_v;
    bool is_above;
    bool is_below;
    bool is_sticky;
    bool is_shaded;
    bool is_hidden;
    bool is_urgent;
    bool is_skip_taskbar;
    bool is_skip_pager;
    bool is_modal;

    if (client == NULL || ewmh == NULL) {
        return;
    }

    is_fullscreen = (state_atom == ewmh->_NET_WM_STATE_FULLSCREEN);
    is_max_h = (state_atom == ewmh->_NET_WM_STATE_MAXIMIZED_HORZ);
    is_max_v = (state_atom == ewmh->_NET_WM_STATE_MAXIMIZED_VERT);
    is_above = (state_atom == ewmh->_NET_WM_STATE_ABOVE);
    is_below = (state_atom == ewmh->_NET_WM_STATE_BELOW);
    is_sticky = (state_atom == ewmh->_NET_WM_STATE_STICKY);
    is_shaded = (state_atom == ewmh->_NET_WM_STATE_SHADED);
    is_hidden = (state_atom == ewmh->_NET_WM_STATE_HIDDEN);
    is_urgent = (state_atom == ewmh->_NET_WM_STATE_DEMANDS_ATTENTION);
    is_skip_taskbar = (state_atom == ewmh->_NET_WM_STATE_SKIP_TASKBAR);
    is_skip_pager = (state_atom == ewmh->_NET_WM_STATE_SKIP_PAGER);
    is_modal = (state_atom == ewmh->_NET_WM_STATE_MODAL);

    if (is_fullscreen) {
        /* No 'client_is_resizable' gate; see 'ccmd_client_fullscreen''s
         * own comment for why fullscreen is deliberately exempt from
         * it, unlike the maximize handling right below. */
        if (action == WM_STATE_ACTION_ADD) {
            ccmd_client_fullscreen(client);
        } else if (action == WM_STATE_ACTION_REMOVE) {
            ccmd_client_unfullscreen(client);
        } else {
            ccmd_client_toggle_fullscreen(client);
        }
        return;
    }

    if (is_max_h) {
        if (!client_is_resizable(client)) {
            return;
        }

        if (s_wm_state_resolve_add(action,
                    client->properties.state ==
                        CLIENT_STATE_MAXIMIZED_HORZ)) {
            ccmd_client_maximize_horz(client);
        } else {
            ccmd_client_restore(client);
        }
        return;
    }

    if (is_max_v) {
        if (!client_is_resizable(client)) {
            return;
        }

        if (s_wm_state_resolve_add(action,
                    client->properties.state ==
                        CLIENT_STATE_MAXIMIZED_VERT)) {
            ccmd_client_maximize_vert(client);
        } else {
            ccmd_client_restore(client);
        }
        return;
    }

    if (is_above) {
        if (s_wm_state_resolve_add(action,
                    client->properties.layer == CLIENT_LAYER_ABOVE)) {
            ccmd_client_layer_above(client);
        } else {
            ccmd_client_layer_normal(client);
        }
        return;
    }

    if (is_below) {
        if (s_wm_state_resolve_add(action,
                    client->properties.layer == CLIENT_LAYER_BELOW)) {
            ccmd_client_layer_below(client);
        } else {
            ccmd_client_layer_normal(client);
        }
        return;
    }

    if (is_sticky) {
        if (s_wm_state_resolve_add(action,
                    (client->properties.flags &
                        CLIENT_FLAG_PIN) != 0u)) {
            ccmd_client_pin(client);
        } else {
            ccmd_client_unpin(client);
        }
        return;
    }

    if (is_shaded) {
        if (s_wm_state_resolve_add(action,
                    (client->properties.flags &
                        CLIENT_FLAG_SHADED) != 0u)) {
            ccmd_client_shade(client);
        } else {
            ccmd_client_unshade(client);
        }
        return;
    }

    if (is_hidden) {
        if (s_wm_state_resolve_add(action,
                    (client->properties.flags &
                        CLIENT_FLAG_HIDDEN) != 0u)) {
            ccmd_client_iconify(client);
        } else {
            ccmd_client_restore(client);
        }
        return;
    }

    if (is_urgent) {
        if (s_wm_state_resolve_add(action,
                    (client->properties.flags &
                        CLIENT_FLAG_URGENT) != 0u)) {
            ccmd_client_urge(client);
        } else {
            ccmd_client_unurge(client);
        }
        return;
    }

    if (is_skip_taskbar) {
        if (s_wm_state_resolve_add(action,
                    (client->properties.flags &
                        CLIENT_FLAG_SKIP_TASKBAR) != 0u)) {
            client_skip_taskbar(client);
        } else {
            client_unskip_taskbar(client);
        }
        return;
    }

    if (is_skip_pager) {
        if (s_wm_state_resolve_add(action,
                    (client->properties.flags &
                        CLIENT_FLAG_SKIP_PAGER) != 0u)) {
            client_skip_pager(client);
        } else {
            client_unskip_pager(client);
        }
        return;
    }

    if (is_modal) {
        if (s_wm_state_resolve_add(action, client_is_modal(client))) {
            client_mark_modal(client);
            ccmd_add_states(client, 1, "_NET_WM_STATE_MODAL");
        } else {
            client_unmark_modal(client);
            ccmd_rem_states(client, 1, "_NET_WM_STATE_MODAL");
        }
        return;
    }
}


/**
 * @brief Map a @c _NET_WM_MOVERESIZE direction to the anchor and
 *        per-axis resize flags @a drag_start_directed expects
 *
 * @param direction         One of the eight
 *                          @c XCB_EWMH_WM_MOVERESIZE_SIZE_* values
 * @param out_anchor_right  Set to whether the right edge stays fixed
 * @param out_anchor_bottom Set to whether the bottom edge stays fixed
 * @param out_resize_w      Set to whether this direction resizes the
 *                          width at all
 * @param out_resize_h      Set to whether this direction resizes the
 *                          height at all
 *
 * @note Complexity: @e O(1)
 */
static void s_moveresize_direction_to_anchor(uint32_t direction,
        bool *restrict out_anchor_right, bool *restrict out_anchor_bottom,
        bool *restrict out_resize_w, bool *restrict out_resize_h)
{
    switch (direction) {
        case XCB_EWMH_WM_MOVERESIZE_SIZE_TOPLEFT:
            *out_anchor_right = true;
            *out_anchor_bottom = true;
            *out_resize_w = true;
            *out_resize_h = true;
            break;
        case XCB_EWMH_WM_MOVERESIZE_SIZE_TOP:
            *out_anchor_right = false;
            *out_anchor_bottom = true;
            *out_resize_w = false;
            *out_resize_h = true;
            break;
        case XCB_EWMH_WM_MOVERESIZE_SIZE_TOPRIGHT:
            *out_anchor_right = false;
            *out_anchor_bottom = true;
            *out_resize_w = true;
            *out_resize_h = true;
            break;
        case XCB_EWMH_WM_MOVERESIZE_SIZE_RIGHT:
            *out_anchor_right = false;
            *out_anchor_bottom = false;
            *out_resize_w = true;
            *out_resize_h = false;
            break;
        case XCB_EWMH_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
            *out_anchor_right = false;
            *out_anchor_bottom = false;
            *out_resize_w = true;
            *out_resize_h = true;
            break;
        case XCB_EWMH_WM_MOVERESIZE_SIZE_BOTTOM:
            *out_anchor_right = false;
            *out_anchor_bottom = false;
            *out_resize_w = false;
            *out_resize_h = true;
            break;
        case XCB_EWMH_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
            *out_anchor_right = true;
            *out_anchor_bottom = false;
            *out_resize_w = true;
            *out_resize_h = true;
            break;
        case XCB_EWMH_WM_MOVERESIZE_SIZE_LEFT:
        default:
            *out_anchor_right = true;
            *out_anchor_bottom = false;
            *out_resize_w = true;
            *out_resize_h = false;
            break;
    }
}


/* Handle a '_NET_WM_STATE' client message */
void hi_handle_net_wm_state(client_td *client,
        xcb_client_message_event_t *event,
        const xcb_ewmh_connection_t *ewmh,
        surface_td *surface, desktop_td *desktop)
{
    uint32_t action;
    xcb_atom_t atom1;
    xcb_atom_t atom2;

    if (client == NULL || event == NULL || ewmh == NULL) {
        return;
    }

    action = event->data.data32[0];
    atom1 = (xcb_atom_t) event->data.data32[1];
    atom2 = (xcb_atom_t) event->data.data32[2];

    s_handle_wm_state_atom(client, atom1, action, ewmh);
    if (atom2 != XCB_ATOM_NONE) {
        s_handle_wm_state_atom(client, atom2, action, ewmh);
    }

    wm_outdate_surface(surface);
    wm_outdate_desktop(desktop);
}


/* Handle a '_NET_CURRENT_DESKTOP' client message */
void hi_handle_net_current_desktop(const wm_td *wm,
        xcb_client_message_event_t *event)
{
    surface_td *surface;

    if (wm == NULL || event == NULL) {
        return;
    }

    surface = lookup_surface_for_root(wm_surfaces(wm), event->window);
    if (surface == NULL) {
        return;
    }

    scmd_surface_desktop_switch(surface, event->data.data32[0]);
    wm_outdate_surface(surface);
}


/* Handle a '_NET_WM_DESKTOP' client message */
void hi_handle_net_wm_desktop(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface,
        desktop_td *src_desktop)
{
    uint32_t target_id;
    desktop_td *tgt_desktop;
    xcb_connection_t *connection = wm_connection(wm);

    if (wm == NULL || event == NULL || client == NULL) {
        return;
    }

    target_id = event->data.data32[0];
    tgt_desktop = surface_desktop_get(surface, target_id);
    if (tgt_desktop == NULL || tgt_desktop == src_desktop) {
        return;
    }

    desktop_action_client_rem(src_desktop, client);
    desktop_action_client_add(tgt_desktop, client);

    /* Same reasoning as the desktop-warp fix in input/mouse/drag.c:
     * 'desktop_action_client_rem'/'_add' alone never touch the
     * client's own recorded 'desktop_id', so anything reading a
     * client's desktop from that field directly (the window list
     * menu's own per-desktop grouping foremost among them; see
     * winlist.c) would keep showing this client under the desktop it
     * just left, even though a pager or taskbar sending this very
     * message already expects it moved. */
    client->desktop_id = target_id;

    if (surface->desktop_cur != target_id) {
        xcb_window_t target =
            (client_is_decorated(client) && client->frame != 0)
            ? client->frame
            : client->window;

        /* Account for the 'UnmapNotify' events so
         * 'handler_unmap_notify' does not treat this WM-initiated unmap
         * as a client self-close and set 'CLIENT_FLAG_HIDDEN'.  Two
         * events arrive for the unmapped target (0SubstructureNotify'
         * on parent + 'StructureNotify' on target) and one additional
         * event for the titlebar via the frame's
         * 'SubstructureNotify'. */
        client->ignore_unmap += 2u;
        if (client->titlebar != 0) {
            client->ignore_unmap += 1u;
            xcb_unmap_window(connection, client->titlebar);
        }
        xcb_unmap_window(connection, target);
    }

    client->desktop_id = target_id;

    if (wm_ewmh(wm) != NULL) {
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                client->window, wm_ewmh(wm)->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &target_id);
    }

    wm_outdate_surface(surface);
    wm_outdate_desktop(src_desktop);
    wm_outdate_desktop(tgt_desktop);
}


/* Handle a '_NET_MOVERESIZE_WINDOW' client message */
void hi_handle_net_moveresize_window(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop)
{
    uint32_t flags;
    int32_t req_x;
    int32_t req_y;
    uint32_t req_w;
    uint32_t req_h;
    uint16_t target_mask;
    uint32_t target_values[4];
    xcb_window_t target;
    bool size_changed;
    xcb_connection_t *connection = wm_connection(wm);
    int i;

    if (wm == NULL || event == NULL || client == NULL) {
        return;
    }

    /* Unlike 'ccmd_client_shade'/'ccmd_client_fullscreen'/
     * 'ccmd_client_maximize', which restore an iconified client
     * automatically because entering any of those states is itself
     * the visible change being asked for, a plain geometry request
     * has no visible effect on a client that is not currently mapped
     * to begin with; silently un-iconifying it would be a surprising
     * side effect of a request that leaves every other client's own
     * visibility untouched. */
    if (client_is_iconified(client)) {
        return;
    }

    flags = (uint32_t) event->data.data32[0];
    req_x = (int32_t) event->data.data32[1];
    req_y = (int32_t) event->data.data32[2];
    req_w = (uint32_t) event->data.data32[3];
    req_h = (uint32_t) event->data.data32[4];

    if ((flags & WM_MOVERESIZE_FLAG_WIDTH) &&
            client_is_decorated(client) && client->frame != 0) {
        req_w += (uint32_t) client->layout.frame_extents.left
               + (uint32_t) client->layout.frame_extents.right;
    }

    if ((flags & WM_MOVERESIZE_FLAG_HEIGHT) &&
            client_is_decorated(client) && client->frame != 0) {
        req_h += (uint32_t) client->layout.frame_extents.top
               + (uint32_t) client->layout.frame_extents.bottom;
    }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame : client->window;
    target_mask = 0;
    size_changed = false;
    i = 0;

    if (flags & WM_MOVERESIZE_FLAG_X) {
        target_values[i++] = (uint32_t) req_x;
        target_mask |= XCB_CONFIG_WINDOW_X;
        client->layout.geometry.cur.pos.x = req_x;
    }

    if (flags & WM_MOVERESIZE_FLAG_Y) {
        if (req_y < 0) {
            req_y = 0;
        }
        target_values[i++] = (uint32_t) req_y;
        target_mask |= XCB_CONFIG_WINDOW_Y;
        client->layout.geometry.cur.pos.y = req_y;
    }

    if (flags & WM_MOVERESIZE_FLAG_WIDTH) {
        if (req_w < WM_MIN_WINDOW_DIMENSION) {
            req_w = WM_MIN_WINDOW_DIMENSION;
        }
        target_values[i++] = req_w;
        target_mask |= XCB_CONFIG_WINDOW_WIDTH;
        client->layout.geometry.cur.dim.w = req_w;
        size_changed = true;
    }

    if (flags & WM_MOVERESIZE_FLAG_HEIGHT) {
        if (req_h < WM_MIN_WINDOW_DIMENSION) {
            req_h = WM_MIN_WINDOW_DIMENSION;
        }
        target_values[i++] = req_h;
        target_mask |= XCB_CONFIG_WINDOW_HEIGHT;
        client->layout.geometry.cur.dim.h = req_h;
        size_changed = true;
    }

    (void) i;

    if (target_mask != 0) {
        xcb_configure_window(connection, target,
                target_mask, target_values);
        if (size_changed &&
                client_is_decorated(client) && client->frame != 0) {
            client_decoration_layout_sync(client);
        }
        xcb_flush(connection);
        wm_outdate_client(client);
        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
    }
}


/* Apply a @c _NET_SHOWING_DESKTOP request to one surface */
void hi_handle_net_showing_desktop(surface_td *surface, bool show)
{
    desktop_td *desktop;
    cdlist_item_td *stacking_head;
    cdlist_item_td *node;
    const cdlist_item_td *initial;
    bool any_visible;
    bool changed_hidden_state;

    if (surface == NULL || surface->connection == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    any_visible = false;
    changed_hidden_state = false;
    /* 'desktop->stacking' is only ever iterated below, never modified,
     * so its head is stable across the whole function; computed once
     * here rather than re-deriving the same conditional expression
     * every time a fresh pass over the list is about to start. */
    stacking_head = (desktop->stacking != NULL)
        ? cdlist_head(desktop->stacking)
        : NULL;
    node = stacking_head;
    if (node != NULL) {
        initial = node;
        do {
            const client_td *client = (client_td *) cdlist_data(node);
            if (client != NULL && !client_is_locked(client) &&
                    !(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    client->properties.state !=
                        (uint16_t) CLIENT_STATE_ICONIFIED) {
                any_visible = true;
                break;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    if (show && !surface->showing_desktop) {
        node = stacking_head;
        if (node == NULL || !any_visible) {
            show = false;
        }
    }

    if (!show && surface->showing_desktop) {
        node = stacking_head;
        if (node == NULL) {
            return;
        }

        initial = node;
        do {
            client_td *client = (client_td *) cdlist_data(node);
            if (client != NULL && !client_is_locked(client) &&
                    (client->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    client->properties.state !=
                        (uint16_t) CLIENT_STATE_ICONIFIED) {
                client_unhide(client);
                changed_hidden_state = true;
                ccmd_set_wm_state(client, CCMD_WM_STATE_NORMAL,
                        XCB_NONE);
                ccmd_rem_states(client, 1, "_NET_WM_STATE_HIDDEN");
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
        surface_clients_show(surface, surface->desktop_cur);
    } else {
        node = stacking_head;
        if (node == NULL) {
            return;
        }

        /* Unmap the windows first, then mark them hidden.
         * 'surface_clients_hide' skips clients that already have
         * 'CLIENT_FLAG_HIDDEN' set, so the flag must be applied only
         * after the unmap call. */
        surface_clients_hide(surface, surface->desktop_cur);
        initial = node;
        do {
            client_td *client = (client_td *) cdlist_data(node);
            if (client != NULL && !client_is_locked(client) &&
                    !(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    client->properties.state !=
                        (uint16_t) CLIENT_STATE_ICONIFIED) {
                client_hide(client);
                changed_hidden_state = true;
                ccmd_set_wm_state(client, CCMD_WM_STATE_ICONIC,
                        XCB_NONE);
                ccmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);

        xcb_set_input_focus(surface->connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_CURRENT_TIME);
    }

    surface->showing_desktop = show && changed_hidden_state;
    wm_outdate_surface(surface);
    wm_outdate_desktop(desktop);
}


/* Handle a '_NET_RESTACK_WINDOW' client message */
void hi_handle_net_restack_window(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop)
{
    uint32_t detail;
    xcb_window_t sibling;
    xcb_window_t target;
    xcb_connection_t *connection = wm_connection(wm);
    uint16_t mask = 0;
    uint32_t values[2];
    int i = 0;

    if (wm == NULL || event == NULL || client == NULL) {
        return;
    }

    detail = event->data.data32[2];
    sibling = (xcb_window_t) event->data.data32[1];
    target = (client->frame != 0 && client_is_decorated(client))
        ? client->frame : client->window;

    if (sibling != XCB_NONE) {
        mask |= XCB_CONFIG_WINDOW_SIBLING;
        values[i++] = sibling;
    }

    if (detail == WM_RESTACK_DETAIL_BELOW) {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = XCB_STACK_MODE_BELOW;
    } else if (detail == WM_RESTACK_DETAIL_TOP_IF) {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = XCB_STACK_MODE_TOP_IF;
    } else if (detail == WM_RESTACK_DETAIL_BOTTOM_IF) {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = XCB_STACK_MODE_BOTTOM_IF;
    } else if (detail == WM_RESTACK_DETAIL_OPPOSITE) {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = XCB_STACK_MODE_OPPOSITE;
    } else {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = XCB_STACK_MODE_ABOVE;
    }

    (void) i;

    xcb_configure_window(connection, target, mask, values);
    xcb_flush(connection);
    wm_outdate_surface(surface);
    wm_outdate_desktop(desktop);
}


/* Handle a '_NET_WM_FULLSCREEN_MONITORS' client message */
void hi_handle_net_wm_fullscreen_monitors(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop)
{
    uint32_t monitors[4];
    xcb_connection_t *connection = wm_connection(wm);

    if (wm == NULL || event == NULL || client == NULL ||
            wm_ewmh(wm) == NULL) {
        return;
    }

    monitors[0] = event->data.data32[0];
    monitors[1] = event->data.data32[1];
    monitors[2] = event->data.data32[2];
    monitors[3] = event->data.data32[3];

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
            client->window,
            atom_intern(connection,
                "_NET_WM_FULLSCREEN_MONITORS", false),
            XCB_ATOM_CARDINAL, 32, 4, monitors);

    if (client->properties.state ==
            (uint16_t) CLIENT_STATE_FULLSCREEN) {
        ccmd_client_fullscreen(client);
    }

    wm_outdate_surface(surface);
    wm_outdate_desktop(desktop);
}


/* Handle a '_NET_WM_MOVERESIZE' client message */
void hi_handle_net_wm_moveresize(const wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop)
{
    uint32_t direction;
    int32_t x_root;
    int32_t y_root;
    uint32_t screen_w;
    uint32_t screen_h;
    uint32_t snap;
    bool anchor_right;
    bool anchor_bottom;
    bool resize_w;
    bool resize_h;
    xcb_connection_t *connection = wm_connection(wm);
    config_td *config = wm_config(wm);

    (void) desktop;

    if (wm == NULL || event == NULL || client == NULL ||
            surface == NULL || surface->screen == NULL ||
            config == NULL) {
        return;
    }

    direction = event->data.data32[2];

    if (direction == XCB_EWMH_WM_MOVERESIZE_CANCEL) {
        if (drag_is_active()) {
            drag_cancel(connection, client);
        }
        return;
    }

    if (direction == XCB_EWMH_WM_MOVERESIZE_MOVE_KEYBOARD ||
            direction == XCB_EWMH_WM_MOVERESIZE_SIZE_KEYBOARD) {
        return;
    }

    x_root = (int32_t) event->data.data32[0];
    y_root = (int32_t) event->data.data32[1];
    if (x_root < INT16_MIN) {
        x_root = INT16_MIN;
    } else if (x_root > INT16_MAX) {
        x_root = INT16_MAX;
    }
    if (y_root < INT16_MIN) {
        y_root = INT16_MIN;
    } else if (y_root > INT16_MAX) {
        y_root = INT16_MAX;
    }

    screen_w = surface->properties.dim.w;
    screen_h = surface->properties.dim.h;
    snap = config->base.windows.snap;

    if (direction == XCB_EWMH_WM_MOVERESIZE_MOVE) {
        drag_start(connection, surface->screen->root, client,
                wm_get_client_desktop(client),
                CLIENT_OPERATION_MOVING,
                XCB_CURRENT_TIME,
                (int16_t) x_root, (int16_t) y_root,
                screen_w, screen_h, snap);
        return;
    }

    if (!client_is_resizable(client)) {
        return;
    }

    s_moveresize_direction_to_anchor(direction, &anchor_right,
            &anchor_bottom, &resize_w, &resize_h);

    drag_start_directed(connection, surface->screen->root,
            client, wm_get_client_desktop(client),
            XCB_CURRENT_TIME,
            (int16_t) x_root, (int16_t) y_root,
            screen_w, screen_h, snap,
            anchor_right, anchor_bottom, resize_w, resize_h);
}
