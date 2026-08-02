/**
 * @file cmds/ccmd.c
 *
 * @brief Implementation on executions over clients using the XCB
 *        interface while updating EWMH and ICCCM hints
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
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Utils includes */
#include <utils/safemem.h>

/* Default initial values */
#include <defs/wm.h>

/* Windows & icons policy includes */
#include <policy/placement.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <lookup.h>
#include <wm.h>

/* Local includes */
#include <cmds/ccmd.h>
#include <cmds/util.h>
#include <cmds/geom.h>
#include <cmds/layer.h>
#include <cmds/meta.h>


/**
 * @brief Transfer focus away from a client that is leaving the current
 *        visible focus chain
 *
 * Picks the most recently used visible focusable client on the current
 * desktop and focuses it.  If no such client exists, focus is released
 * to the pointer root so global grabs continue working.
 *
 * @param client Client that is being hidden or iconified
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       current desktop
 */
static void s_client_focus_fallback(client_td *client)
{
    surface_td *surface;
    desktop_td *desktop;
    cdlist_item_td *node;
    cdlist_item_td *initial;
    client_td *next_focus;

    if (client == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(client->screen_id);
    if (surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL || desktop->client_active_id != client->id) {
        return;
    }

    desktop->client_active_id = 0;
    next_focus = NULL;

    if (desktop->stacking != NULL && cdlist_size(desktop->stacking) > 0) {
        node = cdlist_tail(desktop->stacking);
        initial = node;
        if (node != NULL) {
            do {
                client_td *candidate = (client_td *) cdlist_data(node);
                if (candidate != NULL && candidate != client &&
                        !(candidate->properties.flags &
                            CLIENT_FLAG_HIDDEN) &&
                        !client_is_shaded(candidate) &&
                        candidate->properties.state !=
                            (uint16_t) CLIENT_STATE_ICONIFIED &&
                        (candidate->properties.flags &
                         CLIENT_FLAG_FOCUSABLE)) {
                    next_focus = candidate;
                    break;
                }
                node = cdlist_prev(node);
            } while (node != NULL && node != initial);
        } /* ! if (node) */
    }

    if (next_focus != NULL) {
        desktop->client_active_id = next_focus->id;
        (void) desktop_action_client_send_front(desktop, next_focus);
        wcmd_client_focus(next_focus);
    } else {
        xcb_set_input_focus(client->connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_CURRENT_TIME);
    }

    desktop->is_outdated = true;
    surface->is_outdated = true;
}


/* Perform the action to close the client */
void wcmd_client_close(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* ICCCM §4.2.8: send a 'WM_DELETE_WINDOW' 'ClientMessage' when the
     * client advertises support in 'WM_PROTOCOLS'; fall back to
     * 'xcb_destroy_window' only when it does not */
    if (client->has_wm_delete_window && client->ewmh != NULL) {
        xcb_client_message_event_t ev;

        memset(&ev, 0, sizeof(ev));
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.window = client->window;
        ev.type = client->ewmh->WM_PROTOCOLS;
        ev.data.data32[0] = client->wm_delete_atom;
        ev.data.data32[1] = XCB_CURRENT_TIME;
        xcb_send_event(client->connection, 0, client->window,
                XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);
    } else {
        /* Client does not support 'WM_DELETE_WINDOW'; destroy directly */
        xcb_destroy_window(client->connection, client->window);
    }
}


/* Forcibly kill the client's X connection */
void wcmd_client_kill(client_td *client)
{
    if (client == NULL) {
        return;
    }

    /* Unlike 'wcmd_client_close' (a request to destroy a single window
     * resource), 'xcb_kill_client' terminates the owning client's
     * ENTIRE connection to the X server.  Meant as a last resort for
     * unresponsive clients that ignore a normal close request. */
    xcb_kill_client(client->connection, client->window);
}


/* Restore a client to its normal state */
void wcmd_client_restore(client_td *client)
{
    xcb_window_t target;
    xcb_atom_t icon_geom_atom;

    if (client == NULL) {
        return;
    }

    /* Fullscreen clients must be un-fullscreened first so the
     * decoration and EWMH atom are cleaned up properly */
    if (client_is_fullscreen(client)) {
        wcmd_client_unfullscreen(client);
        return;
    }

    target = wcmd_target_win(client);
    client_geometry_restore(client);

    if (client->icon_window != 0) {
        xcb_destroy_window(client->connection, client->icon_window);
        client->icon_window = 0;
        client->is_icon_mapped = false;
    }
    if (client->titlebar != 0) {
        xcb_map_window(client->connection, client->titlebar);
    }
    xcb_map_window(client->connection, target);
    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_unset_hidden(client);
    client->properties.state = CLIENT_STATE_NORMAL;

    wcmd_set_wm_state(client, WCMD_WM_STATE_NORMAL, XCB_NONE);

    /* EWMH §5.9: remove icon geometry hint when restoring to normal */
    icon_geom_atom = wcmd_intern_atom(client->connection,
            "_NET_WM_ICON_GEOMETRY");
    xcb_delete_property(client->connection, client->window,
            icon_geom_atom);

    wcmd_rem_states(client, 3,
            "_NET_WM_STATE_HIDDEN",
            "_NET_WM_STATE_MAXIMIZED_HORZ",
            "_NET_WM_STATE_MAXIMIZED_VERT");

    wm_request_client_redraw(client);
}


/* Focus a client */
void wcmd_client_focus(client_td *client)
{
    uint32_t border_color;

    if (client == NULL) {
        return;
    }

    /* ICCCM §4.2.7: only call 'SetInputFocus' when the client's input
     * model accepts it ('WM_HINTS' input field, default 'true').
     * Clients that set 'input=false' rely solely on the 'WM_TAKE_FOCUS'
     * message to direct keyboard focus to themselves. */
    if (client->wm_input_hint) {
        xcb_set_input_focus(client->connection, XCB_INPUT_FOCUS_PARENT,
                            client->window, XCB_CURRENT_TIME);
    }

    /* ICCCM §4.2.7: send 'WM_TAKE_FOCUS' 'ClientMessage' when the
     * client has registered that protocol.  This covers both the
     * Locally Active and Globally Active input models. */
    if (client->has_wm_take_focus && client->ewmh != NULL) {
        xcb_client_message_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.window = client->window;
        ev.type = client->ewmh->WM_PROTOCOLS;
        ev.data.data32[0] = client->wm_take_focus_atom;
        ev.data.data32[1] = XCB_CURRENT_TIME;
        xcb_send_event(client->connection, 0, client->window,
                XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);
    }

    /* EWMH: advertise keyboard focus via '_NET_WM_STATE_FOCUSED' */
    wcmd_add_states(client, 1, "_NET_WM_STATE_FOCUSED");

    xcb_map_window(client->connection, client->window);
    if ((!client_is_decorated(client) || client->frame == 0) &&
            client->theme != NULL) {
        border_color = client->theme->window.active.border_color;
        xcb_change_window_attributes(client->connection, client->window,
                XCB_CW_BORDER_PIXEL, &border_color);
    }

    if (client->ewmh != NULL) {
        xcb_ewmh_set_active_window(client->ewmh,
                (int) client->screen_id,
                client->window);
    }
}


/* Unfocus the client */
void wcmd_client_unfocus(client_td *client)
{
    uint32_t border_color;

    if (client == NULL) {
        return;
    }

    /* EWMH: clear '_NET_WM_STATE_FOCUSED' when the window loses focus */
    wcmd_rem_states(client, 1, "_NET_WM_STATE_FOCUSED");

    client_unfocus(client);
    if ((!client_is_decorated(client) || client->frame == 0) &&
            client->theme != NULL) {
        border_color = client->theme->window.inactive.border_color;
        xcb_change_window_attributes(client->connection, client->window,
                XCB_CW_BORDER_PIXEL, &border_color);
    }

    if (client->ewmh != NULL) {
        xcb_ewmh_set_active_window(client->ewmh,
                (int) client->screen_id,
                XCB_NONE);
    }
}


/* Move client */
void wcmd_client_iconify(client_td *client)
{
    xcb_window_t target;
    uint32_t mask;
    uint32_t values[3];
    xcb_get_property_reply_t *handled_reply;
    xcb_atom_t handled_atom;
    xcb_atom_t wm_state_atom;
    xcb_atom_t net_wm_state_atom;
    xcb_atom_t icon_geom_atom;
    xcb_atom_t skip_atoms[2];
    uint32_t wm_state_vals[2];
    uint32_t icon_geom[4];
    uint16_t icon_h_out;
    bool skip_icon_win;

    if (client == NULL) {
        return;
    }

    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    target = wcmd_target_win(client);
    client_geometry_save(client);

    /* EWMH: if a pager sets '_NET_WM_HANDLED_ICONS' on the root window,
     * it manages icon display itself; the window manager must not
     * create icon windows */
    handled_atom = wcmd_intern_atom(client->connection,
            "_NET_WM_HANDLED_ICONS");
    handled_reply = xcb_get_property_reply(client->connection,
            xcb_get_property(client->connection, 0, client->parent_id,
                handled_atom, XCB_ATOM_CARDINAL, 0, 1), NULL);
    skip_icon_win = (handled_reply != NULL &&
            xcb_get_property_value_length(handled_reply) > 0);

    free(handled_reply);

    /* Compute icon height once for use in both branches */
    icon_h_out = (uint16_t) (WM_ICON_SQUARE_SIZE +
            ((client->theme->icon.general.is_captioned)
             ? WM_ICON_CAPTION_HEIGHT
             : 0u));

    if (!skip_icon_win) {
        if (client->icon_window == 0) {
            uint16_t screen_w;
            uint16_t screen_h;
            int16_t ix;
            int16_t iy;
            enum config_icon_placement_e policy =
                CONFIG_ICON_PLACEMENT_BOTTOM;

            screen_w = 1024u;
            screen_h = 768u;

            if (wcmd_screen_dim(client, &screen_w, &screen_h)) {
                /* dimensions updated */
            }

            if (client->config_base != NULL) {
                policy = client->config_base->icons.placement_policy;
            }

            /* Re-use saved position when the client was already iconified
             * once and manually repositioned by the user */
            if (client->icon_x >= 0 && client->icon_y >= 0) {
                ix = client->icon_x;
                iy = client->icon_y;
            } else {
                place_icon(client, wm_get_client_desktop(client), policy,
                        WM_ICON_SQUARE_SIZE, icon_h_out,
                        screen_w, screen_h,
                        &ix, &iy);
                client->icon_x = ix;
                client->icon_y = iy;
            }

            client->icon_window = xcb_generate_id(client->connection);
            mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
                XCB_CW_EVENT_MASK;

            values[0] = client->theme->icon.inactive.background_color;
            values[1] = client->theme->icon.inactive.border_color;
            values[2] = XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_MOTION;

            xcb_create_window(client->connection,
                    XCB_COPY_FROM_PARENT,
                    client->icon_window,
                    client->parent_id,
                    ix, iy,
                    (uint16_t) WM_ICON_SQUARE_SIZE, icon_h_out,
                    (uint16_t) client->theme->icon.general.border_width,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    XCB_COPY_FROM_PARENT,
                    mask, values);
        } else {
            /* Re-map at the saved position (may have been dragged) */
            xcb_configure_window(client->connection, client->icon_window,
                    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                    (const uint32_t[]) {
                    (uint32_t) client->icon_x,
                    (uint32_t) client->icon_y
                    });
        }
    }

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }

    xcb_unmap_window(client->connection, target);
    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }

    if (!skip_icon_win) {
        xcb_map_window(client->connection, client->icon_window);
        xcb_configure_window(client->connection, client->icon_window,
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const uint32_t[]) { XCB_STACK_MODE_BELOW });
        client->is_icon_mapped = true;

        /* ICCCM §4.1.3: mark the WM icon window as Withdrawn so pagers
         * that scan window trees treat it as unmanaged */
        wm_state_atom = wcmd_intern_atom(client->connection, "WM_STATE");
        wm_state_vals[0] = WCMD_WM_STATE_WITHDRAWN;
        wm_state_vals[1] = XCB_NONE;

        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->icon_window, wm_state_atom, wm_state_atom,
                32, 2, wm_state_vals);

        /* EWMH: tell pagers and taskbars to skip the WM icon window */
        net_wm_state_atom = wcmd_intern_atom(client->connection,
                "_NET_WM_STATE");
        skip_atoms[0] = wcmd_intern_atom(client->connection,
                "_NET_WM_STATE_SKIP_PAGER");
        skip_atoms[1] = wcmd_intern_atom(client->connection,
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
        icon_geom_atom = wcmd_intern_atom(client->connection,
                "_NET_WM_ICON_GEOMETRY");
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, icon_geom_atom,
                XCB_ATOM_CARDINAL, 32, 4, icon_geom);
    }

    client_set_hidden(client);
    client->properties.state = CLIENT_STATE_ICONIFIED;

    wcmd_set_wm_state(client, WCMD_WM_STATE_ICONIC,
            (skip_icon_win) ? XCB_NONE : client->icon_window);

    /* Iconify per EWMH: window hidden with '_NET_WM_STATE_HIDDEN'.
     * Icon display handled by pager/desktop */
    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");

    s_client_focus_fallback(client);

    /* Ensure taskbars reflect the iconified state even when the client
     * was not the active one.  Function `s_client_focus_fallback` only
     * marks is_outdated when it changes focus, so a non-active
     * iconification would otherwise not trigger wm_ewmh_sync. */
    wm_request_client_redraw(client);

    xcb_flush(client->connection);
}


/* Hide the client (minimize, but not iconify) */
void wcmd_client_hide(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);

    if (client->titlebar != 0) {
        xcb_unmap_window(client->connection, client->titlebar);
    }

    xcb_unmap_window(client->connection, target);

    if (target != client->window) {
        xcb_unmap_window(client->connection, client->window);
    }

    client_set_hidden(client);

    wcmd_set_wm_state(client, WCMD_WM_STATE_ICONIC, XCB_NONE);
    wcmd_rem_states(client, 1, "_NET_WM_STATE_FULLSCREEN");
    wcmd_add_states(client, 1, "_NET_WM_STATE_HIDDEN");

    s_client_focus_fallback(client);
    wm_request_client_redraw(client);
}


/* Show (unhide) the client */
void wcmd_client_unhide(client_td *client)
{
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    target = wcmd_target_win(client);

    if (client->titlebar != 0) {
        xcb_map_window(client->connection, client->titlebar);
    }

    xcb_map_window(client->connection, target);

    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_unset_hidden(client);

    wcmd_set_wm_state(client, WCMD_WM_STATE_NORMAL, XCB_NONE);
    wcmd_rem_states(client, 1, "_NET_WM_STATE_HIDDEN");
}


/* Set client sticky mode */
void wcmd_client_sticky(client_td *client)
{
    uint32_t all_desktops;

    if (client == NULL) {
        return;
    }

    client_set_sticky(client);
    wcmd_add_states(client, 1, "_NET_WM_STATE_STICKY");
    if (client->ewmh != NULL) {
        all_desktops = WM_DESKTOP_ID_ALL;
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, client->ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &all_desktops);
    }

    wm_request_client_redraw(client);
}


/* Remove client sticky mode */
void wcmd_client_unsticky(client_td *client)
{
    desktop_td *owner_desktop;
    desktop_td *current_desktop;
    surface_td *surface;
    xcb_window_t target;

    if (client == NULL) {
        return;
    }

    client_unset_sticky(client);
    wcmd_rem_states(client, 1, "_NET_WM_STATE_STICKY");
    if (client->ewmh != NULL) {
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, client->ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &client->desktop_id);
    }

    owner_desktop = wm_get_client_desktop(client);
    surface = wm_get_surface_by_id(client->screen_id);
    current_desktop = (surface != NULL)
        ? lookup_current_desktop(surface)
        : NULL;
    if (owner_desktop != NULL && current_desktop != NULL &&
            owner_desktop->id != current_desktop->id) {
        target = wcmd_target_win(client);

        /* Increment 'ignore_unmap' to prevent 'handler_unmap_notify'
         * from treating the WM-initiated unmaps as client self-closes.
         * The frame 'unmap' implicitly unmaps its child, so only one
         * extra increment is needed when target is the frame. */
        client->ignore_unmap += 1u;

        if (target != client->window) {
            client->ignore_unmap += 1u;
        }

        if (client->titlebar != 0) {
            xcb_unmap_window(client->connection, client->titlebar);
        }

        xcb_unmap_window(client->connection, target);

        if (client->icon_window != 0 && client->is_icon_mapped) {
            xcb_unmap_window(client->connection, client->icon_window);
            client->is_icon_mapped = false;
        }

        /* If the unstickied client held focus on the current desktop,
         * transfer focus to the MRU client still on that desktop */
        s_client_focus_fallback(client);
    }

    wm_request_client_redraw(client);
}


/* Toggle stickiness */
void wcmd_client_toggle_sticky(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client_is_sticky(client)) {
        wcmd_client_unsticky(client);
    } else {
        wcmd_client_sticky(client);
    }
}


/* Raise the client to the top */
void wcmd_client_set_urgent(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_set_urgent(client);
    wcmd_add_states(client, 1, "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Clear client urgency */
void wcmd_client_clear_urgent(client_td *client)
{
    if (client == NULL) {
        return;
    }

    client_unset_urgent(client);
    wcmd_rem_states(client, 1, "_NET_WM_STATE_DEMANDS_ATTENTION");
}


/* Publish '_NET_WM_ALLOWED_ACTIONS' based on the client's current
 * properties */
void wcmd_client_update_allowed_actions(client_td *client)
{
    xcb_atom_t actions[12];
    uint32_t n = 0u;

    if (client == NULL || client->ewmh == NULL) {
        return;
    }

    /* Actions available to all managed, visible clients */
    actions[n++] = client->ewmh->_NET_WM_ACTION_CLOSE;
    actions[n++] = client->ewmh->_NET_WM_ACTION_CHANGE_DESKTOP;
    if (client_is_resizable(client)) {
        actions[n++] = client->ewmh->_NET_WM_ACTION_MOVE;
        actions[n++] = client->ewmh->_NET_WM_ACTION_RESIZE;
        actions[n++] = client->ewmh->_NET_WM_ACTION_MAXIMIZE_HORZ;
        actions[n++] = client->ewmh->_NET_WM_ACTION_MAXIMIZE_VERT;
        actions[n++] = client->ewmh->_NET_WM_ACTION_FULLSCREEN;
    }

    if (client_is_focusable(client)) {
        actions[n++] = client->ewmh->_NET_WM_ACTION_MINIMIZE;
    }

    /* All clients may be shaded, sticked, and re-stacked */
    actions[n++] = client->ewmh->_NET_WM_ACTION_SHADE;
    actions[n++] = client->ewmh->_NET_WM_ACTION_STICK;
    actions[n++] = client->ewmh->_NET_WM_ACTION_ABOVE;
    actions[n++] = client->ewmh->_NET_WM_ACTION_BELOW;
    xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
            client->window, client->ewmh->_NET_WM_ALLOWED_ACTIONS,
            XCB_ATOM_ATOM, 32, n, actions);
}
