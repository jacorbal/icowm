/**
 * @file handler/message.c
 *
 * @brief X @c CLIENT_MESSAGE event dispatcher
 *
 * Dispatches EWMH and ICCCM client-message events to the appropriate
 * per-protocol handler.  Protocol-level handlers for individual EWMH
 * atoms are implemented in @c handler/ewmhmsg.c.
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

/* ADT includes */
#include <adt/list.h>

/* Command includes */
#include <cmds/ccmd.h>
#include <cmds/geom.h>
#include <cmds/layer.h>
#include <cmds/scmd.h>
#include <cmds/util.h>

/* Policy includes */
#include <policy/focus.h>

/* Project includes */
#include <actdata.h>
#include <client.h>
#include <desktop.h>
#include <handler.h>
#include <handler/internal.h>
#include <invalidate.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>


/* ICCCM 'WM_CHANGE_STATE' 'IconicState' value */
#define ICCCM_ICONIC_STATE (3)


/**
 * @brief Dispatch a @c ClientMessage event to the appropriate handler
 *
 * Interprets an incoming @c ClientMessage according to EWMH/WM
 * protocols and forwards it to the specific handler function for the
 * target client, surface, or desktop.
 *
 * Recognized messages include:
 * @c _NET_WM_STATE, @c _NET_RESTACK_WINDOW,
 * @c _NET_WM_FULLSCREEN_MONITORS, @c _NET_ACTIVE_WINDOW,
 * @c _NET_CLOSE_WINDOW, @c _NET_WM_DESKTOP,
 * @c _NET_CURRENT_DESKTOP, @c _NET_MOVERESIZE_WINDOW,
 * @c _NET_REQUEST_FRAME_EXTENTS, @c _NET_SHOWING_DESKTOP,
 * @c _NET_WM_PING, and @c WM_CHANGE_STATE.
 *
 * @param wm    Window manager state
 * @param event Raw @c ClientMessage event received from XCB
 *
 * @note Complexity: @e O(1) for dispatch, excluding the cost of any
 *       delegated handler
 */
void handler_client_message(wm_td *wm,
        xcb_client_message_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    xcb_atom_t wm_change_state;
    xcb_atom_t net_restack_window;
    xcb_atom_t net_wm_fullscreen_monitors;
    xcb_intern_atom_reply_t *ia;

    if (wm == NULL || event == NULL || wm->ewmh == NULL) {
        return;
    }

    LOGGER_TRACE("Client message: window=0x%x, type=%u",
            event->window, event->type);

    if (systray_owns_window(event->window)) {
        systray_handle_client_message(wm, event);
        return;
    }

    ia = xcb_intern_atom_reply(wm->connection,
            xcb_intern_atom(wm->connection, 0,
                20, "_NET_RESTACK_WINDOW"), NULL);
    net_restack_window = (ia != NULL) ? ia->atom : XCB_ATOM_NONE;
    free(ia);

    ia = xcb_intern_atom_reply(wm->connection,
            xcb_intern_atom(wm->connection, 0,
                27, "_NET_WM_FULLSCREEN_MONITORS"), NULL);
    net_wm_fullscreen_monitors = (ia != NULL) ? ia->atom : XCB_ATOM_NONE;
    free(ia);

    if (event->type == wm->ewmh->_NET_WM_STATE) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            hi_handle_net_wm_state(client, event, wm->ewmh,
                    surface, desktop);
        }
        return;
    }

    if (event->type == net_restack_window) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            hi_handle_net_restack_window(wm, event, client,
                    surface, desktop);
        }
        return;
    }

    if (event->type == net_wm_fullscreen_monitors) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            hi_handle_net_wm_fullscreen_monitors(wm, event, client,
                    surface, desktop);
        }
        return;
    }

    if (event->type == wm->ewmh->_NET_ACTIVE_WINDOW) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);
        if (client != NULL && surface != NULL && desktop != NULL) {
            /* A hidden client (e.g., minimised to systray) should be
             * restored on the current desktop, not by switching to the
             * desktop where it was originally opened.  For all other
             * non-sticky clients on a different desktop, the
             * traditional behavior of switching to that desktop is
             * preserved. */
            if (!(client->properties.flags & CLIENT_FLAG_STICKY) &&
                    surface->desktop_cur != desktop->id) {
                if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
                    desktop_td *cur_desktop;
                    uint32_t cur_id = surface->desktop_cur;

                    cur_desktop = surface_desktop_get(surface, cur_id);
                    if (cur_desktop != NULL && cur_desktop != desktop) {
                        desktop_action_client_rem(desktop, client);
                        desktop_action_client_add(cur_desktop, client);
                        client->desktop_id = cur_id;
                        if (wm->ewmh != NULL) {
                            xcb_change_property(wm->connection,
                                    XCB_PROP_MODE_REPLACE,
                                    client->window,
                                    wm->ewmh->_NET_WM_DESKTOP,
                                    XCB_ATOM_CARDINAL, 32, 1, &cur_id);
                        }
                        desktop = cur_desktop;
                    }
                } else {
                    action_data_surface_td surface_data;

                    surface_data.surface = surface;
                    surface_data.action_surface =
                        ACTION_SURFACE_DESKTOP_SWITCH;
                    surface_data.new_data.uvalue = desktop->id;
                    scmd_surface_desktop_switch(surface, &surface_data);
                    desktop = lookup_current_desktop(surface);
                }
            }

            if (client->properties.state ==
                    (uint16_t) CLIENT_STATE_ICONIFIED) {
                if (!(client->properties.flags & CLIENT_FLAG_STICKY) &&
                        surface != NULL && desktop != NULL &&
                        surface->desktop_cur != desktop->id) {
                    desktop_td *cur_desktop =
                        surface_desktop_get(surface, surface->desktop_cur);

                    if (cur_desktop != NULL && cur_desktop != desktop) {
                        desktop_action_client_rem(desktop, client);
                        desktop_action_client_add(cur_desktop, client);
                        client->desktop_id = surface->desktop_cur;

                        if (wm->ewmh != NULL) {
                            xcb_change_property(wm->connection,
                                    XCB_PROP_MODE_REPLACE,
                                    client->window,
                                    wm->ewmh->_NET_WM_DESKTOP,
                                    XCB_ATOM_CARDINAL, 32, 1,
                                    &surface->desktop_cur);
                        }
                        desktop = cur_desktop;
                    }
                }

                wcmd_client_restore(client);
            } else if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
                wcmd_client_unhide(client);
            }

            if (desktop != NULL) {
                focus_apply(wm->surfaces, surface, desktop, client,
                        true, wm->config);
            }

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
            hi_handle_net_wm_desktop(wm, event, client,
                    surface, desktop);
        }
        return;
    }

    if (event->type == wm->ewmh->_NET_CURRENT_DESKTOP) {
        hi_handle_net_current_desktop(wm, event);
        return;
    }

    if (event->type == wm->ewmh->_NET_MOVERESIZE_WINDOW) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            hi_handle_net_moveresize_window(wm, event, client,
                    surface, desktop);
        }
        return;
    }

    /* EWMH §5.3: pre-map frame-extents request; reply immediately
     * so the application can size itself before mapping */
    if (event->type == wm->ewmh->_NET_REQUEST_FRAME_EXTENTS) {
        client = lookup_find_client(wm->surfaces, event->window,
                &surface, &desktop);

        if (client != NULL && client->ewmh != NULL) {
            uint32_t extents[4];

            extents[0] = (uint32_t) client->layout.frame_extents.left;
            extents[1] = (uint32_t) client->layout.frame_extents.right;
            extents[2] = (uint32_t) client->layout.frame_extents.top;
            extents[3] = (uint32_t) client->layout.frame_extents.bottom;
            xcb_change_property(wm->connection, XCB_PROP_MODE_REPLACE,
                    event->window, wm->ewmh->_NET_FRAME_EXTENTS,
                    XCB_ATOM_CARDINAL, 32, 4, extents);
            xcb_flush(wm->connection);
        }
        return;
    }

    /* EWMH §5.13: show/hide all desktop windows */
    if (event->type == wm->ewmh->_NET_SHOWING_DESKTOP) {
        bool show = event->data.data32[0] != 0u;
        surface_td *surf;

        for (list_item_td *snode = list_head(wm->surfaces);
                snode != NULL; snode = list_next(snode)) {
            surf = (surface_td *) list_data(snode);

            if (surf == NULL) {
                continue;
            }
            hi_handle_net_showing_desktop(surf, show);
            xcb_ewmh_set_showing_desktop(wm->ewmh,
                    (int) surf->id, (show) ? 1u : 0u);
        }

        xcb_flush(wm->connection);
        return;
    }

    /* EWMH §4.6 & ICCCM §4.2.8: intercept '_NET_WM_PING' pong replies
     * sent from clients back to the root window. */
    if (event->type == wm->ewmh->WM_PROTOCOLS &&
            event->data.data32[0] == (uint32_t) wm->ewmh->_NET_WM_PING) {
        xcb_window_t ping_window = (xcb_window_t) event->data.data32[2];
        client = lookup_find_client(wm->surfaces, ping_window,
                &surface, &desktop);
        if (client != NULL) {
            client->last_ping_reply = event->data.data32[1];
            client_unset_unresponsive(client);
            wm_invalidate_surface(surface);
            wm_invalidate_desktop(desktop);
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
    }
}
