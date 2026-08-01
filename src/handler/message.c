/**

 *
 * @brief X @c CLIENT_MESSAGE event handler
 *
 * Processes EWMH and ICCCM client-message events so that applications
 * can request state changes (fullscreen, maximize, close, desktop
 * switch, &c.) and have them honoured by the window manager.
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
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/list.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <handler.h>
#include <invalidate.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Command includes */
#include <cmds/ccmd.h>
#include <cmds/geom.h>
#include <cmds/layer.h>
#include <cmds/scmd.h>


/* '_NET_WM_STATE' action values (EWMH section 5.8) */
#define WM_STATE_ACTION_REMOVE (0)
#define WM_STATE_ACTION_ADD    (1)
#define WM_STATE_ACTION_TOGGLE (2)

/* ICCCM 'WM_CHANGE_STATE' 'IconicState' value */
#define ICCCM_ICONIC_STATE (3)

/* '_NET_MOVERESIZE_WINDOW' flag bits (EWMH § 5.11) */
#define MOVERESIZE_FLAG_X      (1u << 8)
#define MOVERESIZE_FLAG_Y      (1u << 9)
#define MOVERESIZE_FLAG_WIDTH  (1u << 10)
#define MOVERESIZE_FLAG_HEIGHT (1u << 11)


/**
 * @brief Dispatch a single EWMH @c _NET_WM_STATE atom for a given action
 *
 * Evaluates the requested state atom and applies the corresponding
 * window command to the client, handling fullscreen, maximization,
 * stacking layer, sticky, shaded, hidden and urgent states according to
 * the EWMH @c _NET_WM_STATE specification.
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
        xcb_ewmh_connection_t *ewmh)
{
    bool is_add;
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

    if (is_fullscreen) {
        if (action == WM_STATE_ACTION_ADD) {
            wcmd_client_fullscreen(client);
        } else if (action == WM_STATE_ACTION_REMOVE) {
            wcmd_client_unfullscreen(client);
        } else {
            wcmd_client_toggle_fullscreen(client);
        }
        return;
    }

    if (is_max_h) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             client->properties.state != CLIENT_STATE_MAXIMIZED_HORZ);
        if (is_add) {
            wcmd_client_maximize_horz(client);
        } else {
            wcmd_client_restore(client);
        }
        return;
    }

    if (is_max_v) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             client->properties.state != CLIENT_STATE_MAXIMIZED_VERT);
        if (is_add) {
            wcmd_client_maximize_vert(client);
        } else {
            wcmd_client_restore(client);
        }
        return;
    }

    if (is_above) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             client->properties.layer != CLIENT_LAYER_ABOVE);
        if (is_add) {
            wcmd_client_layer_above(client);
        } else {
            wcmd_client_layer_normal(client);
        }
        return;
    }

    if (is_below) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             client->properties.layer != CLIENT_LAYER_BELOW);
        if (is_add) {
            wcmd_client_layer_below(client);
        } else {
            wcmd_client_layer_normal(client);
        }
        return;
    }

    if (is_sticky) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             !(client->properties.flags & CLIENT_FLAG_STICKY));
        if (is_add) {
            wcmd_client_sticky(client);
        } else {
            wcmd_client_unsticky(client);
        }
        return;
    }

    if (is_shaded) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             !(client->properties.flags & CLIENT_FLAG_SHADED));
        if (is_add) {
            wcmd_client_shade(client);
        } else {
            wcmd_client_unshade(client);
        }
        return;
    }

    if (is_hidden) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             !(client->properties.flags & CLIENT_FLAG_HIDDEN));
        if (is_add) {
            wcmd_client_iconify(client);
        } else {
            wcmd_client_restore(client);
        }
        return;
    }

    if (is_urgent) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             !(client->properties.flags & CLIENT_FLAG_URGENT));
        if (is_add) {
            wcmd_client_set_urgent(client);
        } else {
            wcmd_client_clear_urgent(client);
        }
        return;
    }

    if (is_skip_taskbar) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             !(client->properties.flags & CLIENT_FLAG_SKIP_TASKBAR));
        if (is_add) {
            client_set_skip_taskbar(client);
        } else {
            client_unset_skip_taskbar(client);
        }
        return;
    }

    if (is_skip_pager) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             !(client->properties.flags & CLIENT_FLAG_SKIP_PAGER));
        if (is_add) {
            client_set_skip_pager(client);
        } else {
            client_unset_skip_pager(client);
        }
        return;
    }
}


/**
 * @brief Handle a @c _NET_WM_STATE client message
 *
 * Decodes the requested @c _NET_WM_STATE action and state atoms from
 * the client-message event and applies the corresponding state changes
 * to the client, including combined horizontal and vertical
 * maximization.
 *
 * @param client  Pointer to the target client
 * @param event   Pointer to the received @c CLIENT_MESSAGE event
 * @param ewmh    Pointer to the EWMH connection handle
 * @param surface Pointer to the surface that owns the client
 * @param desktop Pointer to the desktop where the client resides
 *
 * @note Invalidates @p surface and @p desktop when the client state
 *       changes, so they can be repainted by the compositor
 * @note No-op if any of @p client, @p event or @p ewmh are null
 * @note Complexity: @e O(1)
 *
 * @see s_handle_wm_state_atom
 */
static void s_handle_net_wm_state(client_td *client,
        xcb_client_message_event_t *event,
        xcb_ewmh_connection_t *ewmh,
        surface_td *surface, desktop_td *desktop)
{
    uint32_t action;
    xcb_atom_t atom1;
    xcb_atom_t atom2;
    bool both_max;

    if (client == NULL || event == NULL || ewmh == NULL) {
        return;
    }

    action = event->data.data32[0];
    atom1 = (xcb_atom_t) event->data.data32[1];
    atom2 = (xcb_atom_t) event->data.data32[2];

    both_max = ((atom1 == ewmh->_NET_WM_STATE_MAXIMIZED_HORZ &&
                 atom2 == ewmh->_NET_WM_STATE_MAXIMIZED_VERT) ||
                (atom1 == ewmh->_NET_WM_STATE_MAXIMIZED_VERT &&
                 atom2 == ewmh->_NET_WM_STATE_MAXIMIZED_HORZ));

    if (both_max) {
        if (action == WM_STATE_ACTION_ADD) {
            wcmd_client_maximize(client);
        } else if (action == WM_STATE_ACTION_REMOVE) {
            wcmd_client_restore(client);
        } else {
            if (client->properties.state == CLIENT_STATE_MAXIMIZED) {
                wcmd_client_restore(client);
            } else {
                wcmd_client_maximize(client);
            }
        }
        wm_invalidate_surface(surface);
        wm_invalidate_desktop(desktop);
        return;
    }

    if (atom1 != XCB_ATOM_NONE) {
        s_handle_wm_state_atom(client, atom1, action, ewmh);
    }
    if (atom2 != XCB_ATOM_NONE) {
        s_handle_wm_state_atom(client, atom2, action, ewmh);
    }

    wm_invalidate_surface(surface);
    wm_invalidate_desktop(desktop);
}


/**
 * @brief Handle a @c _NET_CURRENT_DESKTOP client message
 *
 * Processes a request to change the current desktop on a surface by
 * updating the desktop index and scheduling the necessary surface
 * actions so that the new desktop becomes active.
 *
 * @param wm    Pointer to the window manager context
 * @param event Pointer to the received @c CLIENT_MESSAGE event
 *
 * @note No-op if @p wm or @p event are null, or if the root surface for
 *       @p event->window cannot be found
 * @note Complexity: @e O(1)
 */
static void s_handle_net_current_desktop(wm_td *wm,
        xcb_client_message_event_t *event)
{
    surface_td *surface;
    action_data_surface_td surface_data;

    if (wm == NULL || event == NULL) {
        return;
    }

    surface = lookup_surface_for_root(wm->surfaces, event->window);
    if (surface == NULL) {
        return;
    }

    surface_data.surface = surface;
    surface_data.action_surface = ACTION_SURFACE_DESKTOP_SWITCH;
    surface_data.new_data.uvalue = event->data.data32[0];

    scmd_surface_desktop_switch(surface, &surface_data);
    wm_invalidate_surface(surface);
}


/**
 * @brief Handle a @c _NET_WM_DESKTOP client message
 *
 * Moves a client from its current desktop to the target desktop
 * requested by the EWMH @c _NET_WM_DESKTOP client-message, updating the
 * client's desktop id and mapping state as required.
 *
 * @param wm          Pointer to the window manager context
 * @param event       Pointer to the received @c CLIENT_MESSAGE event
 * @param client      Pointer to the client to be moved
 * @param surface     Pointer to the surface that owns the client
 * @param src_desktop Pointer to the client's current desktop
 *
 * @note If the target desktop is the same as @p src_desktop or does not
 *       exist, the request is ignored
 * @note Invalidates @p surface, @p src_desktop and the target desktop
 *       after a successful move
 * @note Complexity: @e O(1)
 */
static void s_handle_net_wm_desktop(wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface,
        desktop_td *src_desktop)
{
    uint32_t target_id;
    desktop_td *tgt_desktop;

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

    if (surface->desktop_cur != target_id) {
        xcb_unmap_window(wm->connection, client->window);
        if (client->frame != 0) {
            xcb_unmap_window(wm->connection, client->frame);
        }
    }

    client->desktop_id = target_id;

    wm_invalidate_surface(surface);
    wm_invalidate_desktop(src_desktop);
    wm_invalidate_desktop(tgt_desktop);
}


/* Handle a '_NET_MOVERESIZE_WINDOW' client message */
static void s_handle_net_moveresize_window(wm_td *wm,
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
    int i;

    if (wm == NULL || event == NULL || client == NULL) {
        return;
    }

    flags = (uint32_t) event->data.data32[0];
    req_x = (int32_t) event->data.data32[1];
    req_y = (int32_t) event->data.data32[2];
    req_w = (uint32_t) event->data.data32[3];
    req_h = (uint32_t) event->data.data32[4];

    /* Per EWMH, width/height are inner client dimensions.  Add frame
     * extents to get the outer frame size when the client is
     * decorated */
    if ((flags & MOVERESIZE_FLAG_WIDTH) &&
            client_is_decorated(client) && client->frame != 0) {
        req_w += (uint32_t) client->layout.frame_extents.left
               + (uint32_t) client->layout.frame_extents.right;
    }

    if ((flags & MOVERESIZE_FLAG_HEIGHT) &&
            client_is_decorated(client) && client->frame != 0) {
        req_h += (uint32_t) client->layout.frame_extents.top
               + (uint32_t) client->layout.frame_extents.bottom;
    }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame : client->window;
    target_mask = 0;
    size_changed = false;
    i = 0;

    if (flags & MOVERESIZE_FLAG_X) {
        target_values[i++] = (uint32_t) req_x;
        target_mask |= XCB_CONFIG_WINDOW_X;
        client->layout.geometry.cur.pos.x = req_x;
    }

    if (flags & MOVERESIZE_FLAG_Y) {
        if (req_y < 0) {
            req_y = 0;
        }
        target_values[i++] = (uint32_t) req_y;
        target_mask |= XCB_CONFIG_WINDOW_Y;
        client->layout.geometry.cur.pos.y = req_y;
    }

    if (flags & MOVERESIZE_FLAG_WIDTH) {
        if (req_w < WM_MIN_WINDOW_DIMENSION) {
            req_w = WM_MIN_WINDOW_DIMENSION;
        }
        target_values[i++] = req_w;
        target_mask |= XCB_CONFIG_WINDOW_WIDTH;
        client->layout.geometry.cur.dim.w = req_w;
        size_changed = true;
    }

    if (flags & MOVERESIZE_FLAG_HEIGHT) {
        if (req_h < WM_MIN_WINDOW_DIMENSION) {
            req_h = WM_MIN_WINDOW_DIMENSION;
        }
        target_values[i++] = req_h;
        target_mask |= XCB_CONFIG_WINDOW_HEIGHT;
        client->layout.geometry.cur.dim.h = req_h;
        size_changed = true;
    }

    if (target_mask != 0) {
        xcb_configure_window(wm->connection, target,
                target_mask, target_values);
        if (size_changed &&
                client_is_decorated(client) && client->frame != 0) {
            client_sync_decoration_layout(client);
        }
        xcb_flush(wm->connection);
        wm_invalidate_surface(surface);
        wm_invalidate_desktop(desktop);
    }
}


/* Handle a 'CLIENT_MESSAGE' event */
void handler_client_message(wm_td *wm,
        xcb_client_message_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    xcb_atom_t wm_change_state;
    xcb_intern_atom_reply_t *ia;

    if (wm == NULL || event == NULL || wm->ewmh == NULL) {
        return;
    }

    LOGGER_TRACE("Client message: window=0x%x, type=%u",
            event->window, event->type);

    if (event->type == wm->ewmh->_NET_WM_STATE) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            s_handle_net_wm_state(client, event, wm->ewmh,
                    surface, desktop);
        }
        return;
    }

    if (event->type == wm->ewmh->_NET_ACTIVE_WINDOW) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);
        if (client != NULL && desktop != NULL) {
            wcmd_client_focus(client);
            desktop->client_active_id = client->id;
            wm_invalidate_surface(surface);
            wm_invalidate_desktop(desktop);
        }
        return;
    }

    if (event->type == wm->ewmh->_NET_CLOSE_WINDOW) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            wcmd_client_close(client);
        }
        return;
    }

    if (event->type == wm->ewmh->_NET_WM_DESKTOP) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            s_handle_net_wm_desktop(wm, event, client, surface, desktop);
        }
        return;
    }

    if (event->type == wm->ewmh->_NET_CURRENT_DESKTOP) {
        s_handle_net_current_desktop(wm, event);
        return;
    }

    if (event->type == wm->ewmh->_NET_MOVERESIZE_WINDOW) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            s_handle_net_moveresize_window(wm, event, client,
                    surface, desktop);
        }
        return;
    }

    ia = xcb_intern_atom_reply(wm->connection,
            xcb_intern_atom(wm->connection, 1, 15, "WM_CHANGE_STATE"),
            NULL);
    if (ia != NULL) {
        wm_change_state = ia->atom;
        free(ia);
        if (event->type == wm_change_state &&
                event->data.data32[0] == ICCCM_ICONIC_STATE) {
            client = lookup_find_client(wm->surfaces, event->window,
                    &surface, &desktop);
            if (client != NULL) {
                wcmd_client_iconify(client);
                wm_invalidate_surface(surface);
                wm_invalidate_desktop(desktop);
            }
        }
    } /* ! if (!ia) */
}
