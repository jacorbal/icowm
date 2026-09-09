/**
 * @file handler/message.c
 *
 * @brief X @c CLIENT_MESSAGE event dispatcher
 *
 * Dispatches EWMH and ICCCM client-message events to the appropriate
 * per-protocol handler.  Protocol-level handlers for individual EWMH
 * atoms are implemented in @c handler/ewmh.c.
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/xcb/atom.h>

/* Command includes */
#include <cmds/client/flags.h>
#include <cmds/client/focus.h>
#include <cmds/client/layer.h>
#include <cmds/client/visibility.h>
#include <cmds/surface.h>

/* Policy includes */
#include <policy/focus.h>

/* Render includes */
#include <render/outdate.h>

/* Definition includes */
#include <defs/ewmh.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <handler.h>
#include <handler/internal.h>
#include <logger.h>
#include <lookup.h>
#include <cctl/sn.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>
#include <utils/xcb/connection.h>


/**
 * @brief Look up the client owning a client-message event and, if
 *        found, forward it to a per-message handler
 *
 * Shared by every @c _NET_* client-message case in
 * @a handler_client_message below whose handler takes the same
 * @p (wm, event, client, surface, desktop) shape.  Only the target
 * atom and the handler function differ between them.
 *
 * @param wm      Window manager state
 * @param event   Client-message event to resolve the target client for
 * @param handler Per-message handler to call once the client, its
 *                surface, and its desktop are resolved; not called at
 *                all when no managed client owns @a event->window
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients (for the @a lookup_find_client walk)
 */
static void s_dispatch_to_client_handler(wm_td *wm,
        xcb_client_message_event_t *event,
        void (*handler)(const wm_td *wm,
            xcb_client_message_event_t *event,
            client_td *client, surface_td *surface,
            desktop_td *desktop))
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;

    client = lookup_find_client(wm_surfaces(wm), event->window,
            &surface, &desktop);
    if (client != NULL) {
        handler(wm, event, client, surface, desktop);
    }
}


/* Dispatch a 'ClientMessage' event to the appropriate handler */
void handler_client_message(wm_td *wm, xcb_client_message_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    xcb_atom_t wm_change_state;
    xcb_atom_t net_restack_window;
    xcb_atom_t net_wm_fullscreen_monitors;
    xcb_atom_t net_wm_moveresize;
    xcb_connection_t *connection = wm_connection(wm);
    xcb_ewmh_connection_t *ewmh = wm_ewmh(wm);
    list_td *surfaces = wm_surfaces(wm);
    config_td *config = wm_config(wm);

    if (wm == NULL || event == NULL || ewmh == NULL) {
        return;
    }

    LOGGER_TRACE("Client message (window=0x%x, type=%u)",
            event->window, event->type);

    /* Startup-notification messages are broadcast on a root window by
     * whichever application is signaling its launch progress, not tied
     * to any window this window manager itself owns or manages, so this
     * is checked unconditionally rather than gated behind an ownership
     * check the way the systray dispatch below is; the function itself
     * is cheap to call when the message type does not match, since it
     * just compares two already-interned atoms and
     * returns. */
    cctl_sn_handle_client_message(connection, surfaces, event);

    if (systray_owns_window(event->window)) {
        systray_handle_client_message(wm, event);
        return;
    }

    net_restack_window = atom_intern(connection,
            "_NET_RESTACK_WINDOW", false);
    net_wm_fullscreen_monitors = atom_intern(connection,
            "_NET_WM_FULLSCREEN_MONITORS", false);
    net_wm_moveresize = atom_intern(connection,
            "_NET_WM_MOVERESIZE", false);

    if (event->type == ewmh->_NET_WM_STATE) {
        client = lookup_find_client(surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            hi_handle_net_wm_state(client, event, ewmh,
                    surface, desktop);
        }
        return;
    }

    if (event->type == net_restack_window) {
        s_dispatch_to_client_handler(wm, event,
                hi_handle_net_restack_window);
        return;
    }

    if (event->type == net_wm_fullscreen_monitors) {
        s_dispatch_to_client_handler(wm, event,
                hi_handle_net_wm_fullscreen_monitors);
        return;
    }

    if (event->type == net_wm_moveresize) {
        s_dispatch_to_client_handler(wm, event,
                hi_handle_net_wm_moveresize);
        return;
    }

    if (event->type == ewmh->_NET_ACTIVE_WINDOW) {
        client = lookup_find_client(surfaces, event->window,
                &surface, &desktop);
        if (client != NULL && surface != NULL && desktop != NULL) {
            const desktop_td *const active_desktop =
                lookup_current_desktop(surface);

            /* EWMH's focus-stealing prevention: a client asking for
             * '_NET_ACTIVE_WINDOW' does not automatically deserve real
             * keyboard focus just because it asked.  Its claim is
             * weighed against whichever client already holds focus on
             * the desktop the user is actually looking at right now,
             * comparing each side's 'user_time', kept genuinely current
             * by 'client_update_user_time' (client.c) every time
             * a real, non-synthetic 'KeyPress'/'ButtonPress' actually
             * reaches it, not the one-time '_NET_WM_USER_TIME' snapshot
             * read back when it first mapped.
             *
             * A requesting client whose most recent genuine interaction
             * is not newer than the one already focused has a weaker
             * claim on the user's attention at this exact moment, e.g.,
             * an application that finished some background task and is
             * trying to jump to the front on its, unprompted, minutes
             * after the user last touched it: it is marked urgent
             * instead of stealing focus outright, the same
             * non-intrusive path already used for a client's
             * pre-existing '_NET_WM_STATE_DEMANDS_ATTENTION'
             * announcement (see 's_client_read_pre_existing_state',
             * client.c), and the request is not honored any further;
             * whatever already had focus keeps it undisturbed.
             *
             * Skipped entirely when nobody has genuinely focused
             * anything on the current desktop yet ('client_active_id'
             * still 0), or when the requesting client already is the
             * one currently focused, since neither case has an actual
             * rival claim to weigh this one against.
             *
             * Skipped as well when the message declares itself to come
             * from the user rather than from the program.  EWMH §2.12
             * has the sender state a source indication, and
             * 'WM_SOURCE_USER' is a pager, a taskbar or a notification
             * passing on a click; weighing that against anything would
             * be second-guessing an instruction
             * already given. */
            if (event->data.data32[0] != (uint32_t) WM_SOURCE_USER &&
                    active_desktop != NULL &&
                    active_desktop->client_active_id != 0 &&
                    active_desktop->client_active_id != client->id) {
                const client_td *const active =
                    desktop_find_client_by_id( active_desktop,
                        active_desktop->client_active_id);
                /* The timestamp that decides this is the one inside the
                 * message, which EWMH §3 defines as the requesting
                 * client's last user activity at the moment it asked,
                 * and not this window's tracked 'user_time'.  They are
                 * different things, and the difference is the whole
                 * mechanism: a notification daemon passing on a click
                 * has just been interacted with and sends a current
                 * timestamp, while an application raising itself
                 * unbidden sends the stale one it has carried since the
                 * user last touched it.
                 *
                 * Weighing this window's instead, as this did, asked
                 * the wrong question.  An iconified window has by
                 * definition not been touched lately, so it lost every
                 * comparison it was ever put through and could never be
                 * activated by anything at all, click or no click: the
                 * request was refused and the window left sitting as an
                 * icon, merely marked urgent.
                 *
                 * A zero means a client too old to fill the field in,
                 * which §3 says to ignore, so this window's is used
                 * then as before. */
                const uint32_t asked_at = event->data.data32[1];
                const xcb_window_t asker_active =
                    (xcb_window_t) event->data.data32[2];
                bool hands_over_from_active;

                /* The third field is the requesting client's currently
                 * active toplevel, and EWMH §3 says the window manager
                 * may be likelier to obey when honoring the request
                 * would mean handing focus from one active window to
                 * another.  It is read for exactly that: an asker
                 * naming an active window of its is a program the user
                 * is already working in, asking to bring a second
                 * window of its own forward, which is what the field
                 * exists to mark out from a background program trying
                 * to jump the queue.
                 *
                 * Honored only when the window it names really is the
                 * one holding focus here.  Taken on trust, it would be
                 * a way around the prevention altogether.  Any client
                 * could name the focused window and be let
                 * through. */
                hands_over_from_active = (active != NULL &&
                        asker_active != XCB_WINDOW_NONE &&
                        asker_active == active->window);

                if (active != NULL && !hands_over_from_active &&
                        !client_user_time_is_newer(
                            (asked_at != 0u)
                                ? asked_at : client->user_time,
                            active->user_time)) {
                    /* 'ccmd_client_urge' ('cmds/client/flags.c')
                     * already covers the EWMH state publish and the IPC
                     * broadcast that setting the flag and recomputing
                     * urgency alone would leave out, on top of now also
                     * covering 'wm_outdate_client' itself. */
                    ccmd_client_urge(client);
                    wm_outdate_desktop(desktop);
                    wm_outdate_surface(surface);
                    return;
                }
            }

            /* A hidden client (e.g., minimized to systray) should be
             * restored on the current desktop, not by switching to the
             * desktop where it was originally opened.  For all other
             * non-pinned clients on a different desktop, the
             * traditional behavior of switching to that desktop is
             * preserved. */
            if (!(client->properties.flags & CLIENT_FLAG_PIN) &&
                    surface->desktop_cur != desktop->id) {
                if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
                    desktop_td *cur_desktop;
                    uint32_t cur_id = surface->desktop_cur;

                    cur_desktop = surface_desktop_get(surface, cur_id);
                    if (cur_desktop != NULL && cur_desktop != desktop) {
                        (void) desktop_action_client_move(desktop,
                                cur_desktop, client);
                        xcb_change_property(connection,
                                XCB_PROP_MODE_REPLACE, client->window,
                                ewmh->_NET_WM_DESKTOP,
                                XCB_ATOM_CARDINAL, 32, 1, &cur_id);
                        desktop = cur_desktop;
                    }
                } else {
                    scmd_surface_desktop_switch(surface, desktop->id);
                    desktop = lookup_current_desktop(surface);
                }
            }

            if (client_is_iconified(client)) {
                /* An iconified client on another desktop is reached by
                 * going to that desktop, the same as a visible one: the
                 * icon lives there, and being iconified is not a reason
                 * to move a window out from under the desktop it
                 * belongs to.  It once was brought here instead, which
                 * meant the same request did opposite things depending
                 * on a state the user had not asked about.
                 *
                 * A pinned client needs none of this: it is on every
                 * desktop already, so it is on this one, and there is
                 * nowhere to go. */
                if (!(client->properties.flags & CLIENT_FLAG_PIN) &&
                        surface != NULL && desktop != NULL &&
                        surface->desktop_cur != desktop->id) {
                    scmd_surface_desktop_switch(surface, desktop->id);
                    desktop = lookup_current_desktop(surface);
                }

                ccmd_client_restore(client);
            } else if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
                ccmd_client_unhide(client);
            }

            if (desktop != NULL) {
                focus_apply(surfaces, surface, desktop, client,
                        true, config);
            }

            wm_outdate_surface(surface);
            wm_outdate_desktop(desktop);
        }
        return;
    }

    if (event->type == ewmh->_NET_CLOSE_WINDOW) {
        client = lookup_find_client(surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            ccmd_client_close(client);
        }
        return;
    }

    if (event->type == ewmh->_NET_WM_DESKTOP) {
        s_dispatch_to_client_handler(wm, event,
                hi_handle_net_wm_desktop);
        return;
    }

    if (event->type == ewmh->_NET_CURRENT_DESKTOP) {
        hi_handle_net_current_desktop(wm, event);
        return;
    }

    if (event->type == ewmh->_NET_DESKTOP_VIEWPORT) {
        hi_handle_net_desktop_viewport(wm, event);
        return;
    }

    if (event->type == ewmh->_NET_MOVERESIZE_WINDOW) {
        s_dispatch_to_client_handler(wm, event,
                hi_handle_net_moveresize_window);
        return;
    }

    /* EWMH §5.3: pre-map frame-extents request; reply immediately so
     * the application can size itself before mapping */
    if (event->type == ewmh->_NET_REQUEST_FRAME_EXTENTS) {
        client = lookup_find_client(surfaces, event->window,
                &surface, &desktop);

        if (client != NULL && xcb_ewmh_connection_get() != NULL) {
            uint32_t extents[4];

            extents[0] = (uint32_t) client->layout.frame_extents.left;
            extents[1] = (uint32_t) client->layout.frame_extents.right;
            extents[2] = (uint32_t) client->layout.frame_extents.top;
            extents[3] = (uint32_t) client->layout.frame_extents.bottom;
            xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                    event->window, ewmh->_NET_FRAME_EXTENTS,
                    XCB_ATOM_CARDINAL, 32, 4, extents);
        }
        return;
    }

    /* EWMH §5.13: show/hide all desktop windows */
    if (event->type == ewmh->_NET_SHOWING_DESKTOP) {
        bool show = event->data.data32[0] != 0u;

        for (list_item_td *snode = list_head(surfaces);
                snode != NULL; snode = list_next(snode)) {
            surface_td *const surf = (surface_td *) list_data(snode);

            if (surf == NULL) {
                continue;
            }

            /* Not published here: 'hi_handle_net_showing_desktop' can
             * decide there is nothing to show (an empty desktop, or
             * every client already hidden) and leave 'surf->
             * is_showing_desktop' at its previous value regardless of
             * what 'show' asked for, so publishing the raw request here
             * would tell every pager and taskbar the opposite of what
             * actually happened.  'wm_outdate_surface'/ '_desktop',
             * which that function already calls, get 'wm_ewmh_sync' to
             * publish the real value on the next refresh instead, the
             * same as every other message type
             * in this dispatcher already relies on it for. */
            hi_handle_net_showing_desktop(surf, show);
        }

        return;
    }

    /* EWMH §4.6 & ICCCM §4.2.8: intercept '_NET_WM_PING' pong replies
     * sent from clients back to the root window. */
    if (event->type == ewmh->WM_PROTOCOLS &&
            event->data.data32[0] == (uint32_t) ewmh->_NET_WM_PING) {
        xcb_window_t ping_window = (xcb_window_t) event->data.data32[2];
        client = lookup_find_client(surfaces, ping_window,
                &surface, &desktop);
        if (client != NULL) {
            const bool was_unresponsive =
                (bool) client_is_unresponsive(client);

            client->hints_ewmh.ping.last_reply = event->data.data32[1];
            client->hints_ewmh.ping.is_waiting = false;
            client->hints_ewmh.ping.pending_ticks = 0u;
            client_mark_responsive(client);

            /* Only when the client was being shown as unresponsive and
             * no longer is.  A pong on its own changes nothing that is
             * drawn: it updates three fields nobody paints from, and
             * every ping-capable client answers one every
             * 'WM_EWMH_PING_INTERVAL_SECONDS'.  Marking outdated
             * regardless had the render pass redraw every desktop on
             * that interval, and a decorated client is cleared with
             * exposures on each pass ('s_render_apply_geometry',
             * 'render/desktop.c'), so its window was blanked and left
             * for the application to paint back, once every five
             * seconds, for as long as it was open. */
            if (was_unresponsive) {
                wm_outdate_client(client);
                wm_outdate_surface(surface);
                wm_outdate_desktop(desktop);
            }
        }
        return;
    }

    wm_change_state = atom_intern(connection, "WM_CHANGE_STATE", true);
    if (event->type == wm_change_state &&
            event->data.data32[0] == ICCCM_ICONIC_STATE) {
        client = lookup_find_client(surfaces, event->window,
                &surface, &desktop);
        if (client != NULL) {
            ccmd_client_iconify(client);
            wm_outdate_surface(surface);
            wm_outdate_desktop(desktop);
        }
    }
}
