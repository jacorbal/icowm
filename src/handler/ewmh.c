/**
 * @file handler/ewmh.c
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
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Command includes */
#include <cmds/ccmd.h>
#include <cmds/geom.h>
#include <cmds/layer.h>
#include <cmds/scmd.h>
#include <cmds/util.h>

/* Policy includes */
#include <policy/focus.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <actdata.h>
#include <client.h>
#include <desktop.h>
#include <handler/internal.h>
#include <invalidate.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>


/* '_NET_WM_STATE' action values (EWMH section 5.8) */
#define WM_STATE_ACTION_REMOVE (0)
#define WM_STATE_ACTION_ADD    (1)
#define WM_STATE_ACTION_TOGGLE (2)

/* '_NET_MOVERESIZE_WINDOW' flag bits (EWMH §5.11) */
#define MOVERESIZE_FLAG_X      (1u << 8)
#define MOVERESIZE_FLAG_Y      (1u << 9)
#define MOVERESIZE_FLAG_WIDTH  (1u << 10)
#define MOVERESIZE_FLAG_HEIGHT (1u << 11)

/* '_NET_RESTACK_WINDOW' detail values (EWMH §4.3) */
#define RESTACK_DETAIL_ABOVE     (0u)
#define RESTACK_DETAIL_BELOW     (1u)
#define RESTACK_DETAIL_TOP_IF    (2u)
#define RESTACK_DETAIL_BOTTOM_IF (3u)
#define RESTACK_DETAIL_OPPOSITE  (4u)


/**
 * @brief Intern a custom atom and return @c XCB_ATOM_NONE on failure
 *
 * @param connection XCB connection used to intern the atom
 * @param name       Atom name string to intern
 *
 * @return Interned atom identifier, or @c XCB_ATOM_NONE on failure
 *
 * @note Complexity: @e O(1)
 */
static xcb_atom_t s_intern_atom(xcb_connection_t *connection,
        const char *name)
{
    xcb_intern_atom_reply_t *ia;
    xcb_atom_t atom = XCB_ATOM_NONE;

    if (connection == NULL || name == NULL) {
        return XCB_ATOM_NONE;
    }

    ia = xcb_intern_atom_reply(connection,
            xcb_intern_atom(connection, 0,
                (uint16_t) safe_strlen(name), name),
            NULL);
    if (ia != NULL) {
        atom = ia->atom;
        free(ia);
    }

    return atom;
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
        if (!client_is_resizable(client)) {
            return;
        }

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
        if (!client_is_resizable(client)) {
            return;
        }

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
        if (!client_is_resizable(client)) {
            return;
        }

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

    if (is_modal) {
        is_add = (action == WM_STATE_ACTION_ADD) ||
            (action == WM_STATE_ACTION_TOGGLE &&
             !client_is_modal(client));
        if (is_add) {
            client_set_modal(client);
            wcmd_add_states(client, 1, "_NET_WM_STATE_MODAL");
        } else {
            client_unset_modal(client);
            wcmd_rem_states(client, 1, "_NET_WM_STATE_MODAL");
        }
        return;
    }
}


/* Handle a '_NET_WM_STATE' client message */
void hi_handle_net_wm_state(client_td *client,
        xcb_client_message_event_t *event,
        xcb_ewmh_connection_t *ewmh,
        surface_td *surface, desktop_td *desktop)
{
    uint32_t action;
    xcb_atom_t atom1;
    xcb_atom_t atom2;

    if (client == NULL || event == NULL || ewmh == NULL) {
        return;
    }

    action = event->data.data32[0];
    atom1  = (xcb_atom_t) event->data.data32[1];
    atom2  = (xcb_atom_t) event->data.data32[2];

    s_handle_wm_state_atom(client, atom1, action, ewmh);
    if (atom2 != XCB_ATOM_NONE) {
        s_handle_wm_state_atom(client, atom2, action, ewmh);
    }

    wm_invalidate_surface(surface);
    wm_invalidate_desktop(desktop);
}


/* Handle a '_NET_CURRENT_DESKTOP' client message */
void hi_handle_net_current_desktop(wm_td *wm,
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


/* Handle a '_NET_WM_DESKTOP' client message */
void hi_handle_net_wm_desktop(wm_td *wm,
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
            xcb_unmap_window(wm->connection, client->titlebar);
        }
        xcb_unmap_window(wm->connection, target);
    }

    client->desktop_id = target_id;

    if (wm->ewmh != NULL) {
        xcb_change_property(wm->connection, XCB_PROP_MODE_REPLACE,
                client->window, wm->ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &target_id);
    }

    wm_invalidate_surface(surface);
    wm_invalidate_desktop(src_desktop);
    wm_invalidate_desktop(tgt_desktop);
}


/* Handle a '_NET_MOVERESIZE_WINDOW' client message */
void hi_handle_net_moveresize_window(wm_td *wm,
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

    (void) i;

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


/* Apply a @c _NET_SHOWING_DESKTOP request to one surface */
void hi_handle_net_showing_desktop(surface_td *surface, bool show)
{
    desktop_td *desktop;
    cdlist_item_td *node;
    cdlist_item_td *initial;
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
    node = (desktop->stacking != NULL)
        ? cdlist_head(desktop->stacking)
        : NULL;
    if (node != NULL) {
        initial = node;
        do {
            client_td *client = (client_td *) cdlist_data(node);
            if (client != NULL &&
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
        node = (desktop->stacking != NULL)
            ? cdlist_head(desktop->stacking)
            : NULL;
        if (node == NULL || !any_visible) {
            show = false;
        }
    }

    if (!show && surface->showing_desktop) {
        node = (desktop->stacking != NULL)
            ? cdlist_head(desktop->stacking)
            : NULL;
        if (node == NULL) {
            return;
        }

        initial = node;
        do {
            client_td *client = (client_td *) cdlist_data(node);
            if (client != NULL &&
                    (client->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    client->properties.state !=
                        (uint16_t) CLIENT_STATE_ICONIFIED) {
                client_unset_hidden(client);
                changed_hidden_state = true;
                wcmd_set_wm_state(client, WCMD_WM_STATE_NORMAL,
                        XCB_NONE);
                wcmd_rem_states(client, 1, "_NET_WM_STATE_HIDDEN");
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
        surface_clients_show(surface, surface->desktop_cur);
    } else {
        node = (desktop->stacking != NULL)
            ? cdlist_head(desktop->stacking)
            : NULL;
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
            if (client != NULL &&
                    !(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
                    client->properties.state !=
                        (uint16_t) CLIENT_STATE_ICONIFIED) {
                client_set_hidden(client);
                changed_hidden_state = true;
                wcmd_set_wm_state(client, WCMD_WM_STATE_ICONIC,
                        XCB_NONE);
                wcmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);

        xcb_set_input_focus(surface->connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_CURRENT_TIME);
    }

    surface->showing_desktop = show && changed_hidden_state;
    wm_invalidate_surface(surface);
    wm_invalidate_desktop(desktop);
}


/* Handle a '_NET_RESTACK_WINDOW' client message */
void hi_handle_net_restack_window(wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop)
{
    uint32_t detail;
    xcb_window_t sibling;
    xcb_window_t target;
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

    if (detail == RESTACK_DETAIL_BELOW) {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = XCB_STACK_MODE_BELOW;
    } else if (detail == RESTACK_DETAIL_TOP_IF) {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = XCB_STACK_MODE_TOP_IF;
    } else if (detail == RESTACK_DETAIL_BOTTOM_IF) {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = XCB_STACK_MODE_BOTTOM_IF;
    } else if (detail == RESTACK_DETAIL_OPPOSITE) {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = XCB_STACK_MODE_OPPOSITE;
    } else {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = XCB_STACK_MODE_ABOVE;
    }

    (void) i;

    xcb_configure_window(wm->connection, target, mask, values);
    xcb_flush(wm->connection);
    wm_invalidate_surface(surface);
    wm_invalidate_desktop(desktop);
}


/* Handle a '_NET_WM_FULLSCREEN_MONITORS' client message */
void hi_handle_net_wm_fullscreen_monitors(wm_td *wm,
        xcb_client_message_event_t *event,
        client_td *client, surface_td *surface, desktop_td *desktop)
{
    uint32_t monitors[4];

    if (wm == NULL || event == NULL || client == NULL ||
            wm->ewmh == NULL) {
        return;
    }

    monitors[0] = event->data.data32[0];
    monitors[1] = event->data.data32[1];
    monitors[2] = event->data.data32[2];
    monitors[3] = event->data.data32[3];

    xcb_change_property(wm->connection, XCB_PROP_MODE_REPLACE,
            client->window,
            s_intern_atom(wm->connection,
                "_NET_WM_FULLSCREEN_MONITORS"),
            XCB_ATOM_CARDINAL, 32, 4, monitors);

    if (client->properties.state ==
            (uint16_t) CLIENT_STATE_FULLSCREEN) {
        wcmd_client_fullscreen(client);
    }

    wm_invalidate_surface(surface);
    wm_invalidate_desktop(desktop);
}
