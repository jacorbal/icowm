/**
 * @file cmds/client/visibility.c
 *
 * @brief Iconify, hide, and unhide actions over clients
 *
 * Split out of what used to be a single, flat @c cmds/client/basic.c.
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
#include <stdlib.h>     /* NULL, free */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Default initial values */
#include <defs/desktop.h>
#include <defs/icon.h>

/* Windows & icons policy includes */
#include <policy/placement.h>

/* Utils includes */
#include <utils/geom.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <ipc.h>
#include <lookup.h>
#include <render/outdate.h>
#include <systray.h>
#include <wm.h>
#include <wm/kill.h>

/* Local includes */
#include <cmds/client/basic.h>
#include <cmds/client/internal.h>


/* Iconize the client */
void ccmd_client_iconify(client_td *client)
{
    xcb_window_t target;
    xcb_get_property_reply_t *handled_reply;
    xcb_atom_t handled_atom;
    xcb_atom_t wm_state_atom;
    xcb_atom_t net_wm_state_atom;
    xcb_atom_t icon_geom_atom;
    xcb_atom_t skip_atoms[2];
    uint16_t icon_h_out;
    bool skip_icon_win;

    if (client == NULL || client_is_locked(client)) {
        return;
    }

    /* Remember the state this client is in right now (normal,
     * maximized in any of its three variants, or fullscreen) so
     * 'ccmd_client_restore' can later re-enter that exact state
     * instead of always landing back on plain normal: "A window
     * manager may implement [additional states] as proper substates
     * of NormalState and IconicState, or it may treat them as
     * independent flags, allowing e.g., a maximized window to be
     * iconified and to re-appear as maximized upon de-iconification"
     * (X Desktop Group, 2013, "Extended Window Manager Hints", v1.5,
     * §2.1.1).  Guarded against an already-iconified client calling
     * this again, which would otherwise overwrite the real remembered
     * state with 'CLIENT_STATE_ICONIFIED' itself. */
    if (client->properties.state != (uint16_t) CLIENT_STATE_ICONIFIED) {
        client->properties.pre_iconify_state = client->properties.state;
    }

    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    /* Un-fullscreen first, the same reasoning as unshading above: an
     * iconified client's geometry is meant to be restored to its
     * pre-iconify size later (see the 'client_geometry_save' call
     * just below), and while still fullscreen that size is the whole
     * screen, not the window's real one. */
    if (client_is_fullscreen(client)) {
        ccmd_client_unfullscreen(client);
    }

    target = ccmd_target_win(client);
    /* Only remember the geometry to restore to if it is not already
     * a maximized state's geometry: iconifying a maximized window must
     * not overwrite the true pre-maximize geometry already held in
     * 'layout.geometry.old' (see 'client_is_maximized_any' and the
     * matching guard in the 'ccmd_client_maximize*' functions), or
     * un-iconifying it later would restore it at the maximized size
     * instead of its original one */
    if (!client_is_maximized_any(client)) {
        client_geometry_save(client);
    }

    /* EWMH: if a pager sets '_NET_WM_HANDLED_ICONS' on the root window,
     * it manages icon display itself; the window manager must not
     * create icon windows */
    handled_atom = ccmd_intern_atom(client->connection,
            "_NET_WM_HANDLED_ICONS");
    handled_reply = xcb_get_property_reply(client->connection,
            xcb_get_property(client->connection, 0, client->parent_id,
                handled_atom, XCB_ATOM_CARDINAL, 0, 1), NULL);
    skip_icon_win = (handled_reply != NULL &&
            xcb_get_property_value_length(handled_reply) > 0);

    free(handled_reply);

    /* Compute icon height once, shared by icon window creation
     * below and the '_NET_WM_ICON_GEOMETRY' property published
     * further down */
    icon_h_out = (uint16_t) (WM_ICON_SQUARE_SIZE +
            ((client->theme->icon.is_captioned)
             ? WM_ICON_CAPTION_HEIGHT
             : 0u));

    if (!skip_icon_win) {
        ccmd_client_ensure_icon_window(client, icon_h_out);
    }

    /* Account for the 'UnmapNotify' events that 'handler_unmap_notify'
     * must skip, the same reasoning as 'ccmd_client_hide''s own
     * identical comment: two events arrive for the unmapped target
     * ('SubstructureNotify' on parent + 'StructureNotify' on target)
     * and one additional event for the titlebar via the frame's
     * 'SubstructureNotify'.  If 'target' is the frame, the content
     * window is also unmapped explicitly below, producing two more
     * events for 'client->window'.  Without this, an iconified
     * decorated client's own content-window 'UnmapNotify' reaches
     * 'handler_unmap_notify' with 'ignore_unmap' still zero, which
     * that handler reads as the client withdrawing itself rather
     * than the window manager iconifying it. */
    client->ignore_unmap += 2u;
    if (client->titlebar != 0) {
        client->ignore_unmap += 1u;
    }
    if (target != client->window) {
        client->ignore_unmap += 2u;
    }

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }

    xcb_unmap_window(client->connection, target);
    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }

    if (!skip_icon_win) {
        xcb_window_t tray_below;
        uint32_t wm_state_vals[2];
        uint32_t icon_geom[4];

        xcb_map_window(client->connection, client->icon_window);

        /* Icons are meant to sit even lower than the tray whenever it
         * is in the 'below' layer, "stuck to the desktop": stack just
         * below it explicitly, rather than via an unqualified 'below'
         * with no sibling, which would only put the icon under the
         * tray by coincidence of restack order rather than guarantee
         * it.  The other direction (the tray restacking after icons
         * already exist) is handled on the tray's own side; see
         * 'systray_layout_restack'. */
        tray_below = systray_below_window();
        if (tray_below != XCB_WINDOW_NONE) {
            xcb_configure_window(client->connection, client->icon_window,
                    XCB_CONFIG_WINDOW_SIBLING |
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const uint32_t[]) {
                    tray_below, XCB_STACK_MODE_BELOW
                    });
        } else {
            xcb_configure_window(client->connection, client->icon_window,
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const uint32_t[]) { XCB_STACK_MODE_BELOW });
        }
        client->is_icon_mapped = true;

        /* ICCCM §4.1.3: mark the WM icon window as Withdrawn so pagers
         * that scan window trees treat it as unmanaged */
        wm_state_atom = ccmd_intern_atom(client->connection, "WM_STATE");
        wm_state_vals[0] = CCMD_WM_STATE_WITHDRAWN;
        wm_state_vals[1] = XCB_NONE;

        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->icon_window, wm_state_atom, wm_state_atom,
                32, 2, wm_state_vals);

        /* EWMH: tell pagers and taskbars to skip the WM icon window */
        net_wm_state_atom = ccmd_intern_atom(client->connection,
                "_NET_WM_STATE");
        skip_atoms[0] = ccmd_intern_atom(client->connection,
                "_NET_WM_STATE_SKIP_PAGER");
        skip_atoms[1] = ccmd_intern_atom(client->connection,
                "_NET_WM_STATE_SKIP_TASKBAR");
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->icon_window, net_wm_state_atom,
                XCB_ATOM_ATOM, 32, 2, skip_atoms);

        /* EWMH §5.9: publish icon geometry on the client window so
         * taskbars can animate the iconify transition */
        icon_geom[0] = (uint32_t) client->icon_x;
        icon_geom[1] = (uint32_t) client->icon_y;
        icon_geom[2] = WM_ICON_SQUARE_SIZE;
        icon_geom[3] = icon_h_out;
        icon_geom_atom = ccmd_intern_atom(client->connection,
                "_NET_WM_ICON_GEOMETRY");
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, icon_geom_atom,
                XCB_ATOM_CARDINAL, 32, 4, icon_geom);
    }

    client_hide(client);
    client->properties.state = CLIENT_STATE_ICONIFIED;

    ccmd_set_wm_state(client, CCMD_WM_STATE_ICONIC,
            (skip_icon_win) ? XCB_NONE : client->icon_window);

    /* Iconify per EWMH: window hidden with '_NET_WM_STATE_HIDDEN'.
     * Icon display handled by pager/desktop */
    ccmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    ccmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");

    ccmd_client_focus_fallback(client);

    /* Ensure taskbars reflect the iconified state even when the client
     * was not the active one.  Function 'ccmd_client_focus_fallback'
     * only marks is_outdated when it changes focus, so a non-active
     * iconification would otherwise not trigger wm_ewmh_sync. */
    wm_request_client_redraw(client);

    xcb_flush(client->connection);
}


/* Hide the client (minimize, but not iconify) */
void ccmd_client_hide(client_td *client)
{
    xcb_window_t target;
    surface_td *surface;

    if (client == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(client->screen_id);
    if (surface != NULL && surface->showing_desktop) {
        surface->showing_desktop = false;
    }

    target = ccmd_target_win(client);

    /* Account for the 'UnmapNotify' events that 'handler_unmap_notify'
     * must skip.  Two events arrive for the unmapped target
     * ('SubstructureNotify' on parent + 'StructureNotify' on target)
     * and one additional event for the titlebar via the frame's
     * 'SubstructureNotify'.  If 'target' is the frame, the content
     * window is also unmapped explicitly below, producing two more
     * events for 'client->window'. */
    client->ignore_unmap += 2u;
    if (client->titlebar != 0) {
        client->ignore_unmap += 1u;
    }
    if (target != client->window) {
        client->ignore_unmap += 2u;
    }

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }

    xcb_unmap_window(client->connection, target);

    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }

    client_hide(client);

    ccmd_set_wm_state(client, CCMD_WM_STATE_ICONIC, XCB_NONE);
    ccmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    ccmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");

    ccmd_client_focus_fallback(client);
    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/* Show (unhide) the client */
void ccmd_client_unhide(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = ccmd_target_win(client);

    if (client->titlebar != 0) {
        xcb_map_window(client->connection, client->titlebar);
    }

    xcb_map_window(client->connection, target);

    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_unhide(client);

    ccmd_set_wm_state(client, CCMD_WM_STATE_NORMAL, XCB_NONE);
    ccmd_rem_states(client, 1, "_NET_WM_STATE_HIDDEN");

    /* Raise the unhidden client to the top of the desktop stacking
     * order and give it real input focus, matching the deiconify
     * behavior, so that clicking a hidden window in the window menu
     * immediately activates it for keyboard input */
    if (client_is_focusable(client)) {
        desktop_td *const desktop = wm_get_client_desktop(client);
        if (desktop != NULL) {
            desktop->client_active_id = client->id;
            desktop->focus_dirty = true;
            (void) desktop_action_client_send_front(desktop, client);
            desktop->is_outdated = true;
        }
        ccmd_client_focus(client);
    }
    wm_request_client_redraw(client);
}
