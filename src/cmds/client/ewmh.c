/**
 * @file cmds/client/ewmh.c
 *
 * @brief EWMH and ICCCM window-property management
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
#include <stdlib.h>     /* NULL */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Utils includes */
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>

/* Default initial values */
#include <defs/desktop.h>     /* WM_DESKTOP_ID_ALL */

/* Project includes */
#include <client.h>

/* Local includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/visibility.h>


/* Intern an atom name in the X11 system */
xcb_atom_t ccmd_intern_atom(xcb_connection_t *connection,
        const char *name)
{
    return atom_intern(connection, name, false);
}


/* Republish every @c _NET_WM_STATE atom a client currently holds, read
 * straight off its fields, in one single XCB write */
void ccmd_client_sync_states(client_td *client)
{
    xcb_atom_t states[13];
    uint32_t num = 0;
    xcb_ewmh_connection_t *ewmh;

    if (client == NULL || xcb_ewmh_connection_get() == NULL) {
        return;
    }

    ewmh = xcb_ewmh_connection_get();

    /* Each bit is published on its own, since EWMH holds them
     * independent: a window may be maximized on one axis, on both, or
     * on both while also full screen, and every combination has to read
     * back off the property exactly as it stands. */
    if (client_is_maximized_horz(client)) {
        states[num++] = ewmh->_NET_WM_STATE_MAXIMIZED_HORZ;
    }
    if (client_is_maximized_vert(client)) {
        states[num++] = ewmh->_NET_WM_STATE_MAXIMIZED_VERT;
    }
    if (client_is_fullscreen(client)) {
        states[num++] = ewmh->_NET_WM_STATE_FULLSCREEN;
    }
    if (client_is_iconified(client) || client_is_hidden(client)) {
        states[num++] = ewmh->_NET_WM_STATE_HIDDEN;
    }
    if (client_is_pinned(client)) {
        states[num++] = ewmh->_NET_WM_STATE_STICKY;
    }
    if (client_is_urgent(client)) {
        states[num++] = ewmh->_NET_WM_STATE_DEMANDS_ATTENTION;
    }
    if (client_is_shaded(client)) {
        states[num++] = ewmh->_NET_WM_STATE_SHADED;
    }
    if (client->properties.layer == (uint16_t) CLIENT_LAYER_ABOVE) {
        states[num++] = ewmh->_NET_WM_STATE_ABOVE;
    }
    if (client->properties.layer == (uint16_t) CLIENT_LAYER_BELOW) {
        states[num++] = ewmh->_NET_WM_STATE_BELOW;
    }
    if (client_is_modal(client)) {
        states[num++] = ewmh->_NET_WM_STATE_MODAL;
    }
    if (client_is_focused(client)) {
        states[num++] = ccmd_intern_atom(xcb_connection_get(),
                "_NET_WM_STATE_FOCUSED");
    }
    if (client->properties.flags & CLIENT_FLAG_SKIP_TASKBAR) {
        states[num++] = ewmh->_NET_WM_STATE_SKIP_TASKBAR;
    }
    if (client->properties.flags & CLIENT_FLAG_SKIP_PAGER) {
        states[num++] = ewmh->_NET_WM_STATE_SKIP_PAGER;
    }

    xcb_ewmh_set_wm_state(ewmh, client->window, num, states);
}


/* Write the ICCCM 'WM_STATE' property for a client */
void ccmd_set_wm_state(client_td *client,
        uint32_t state, xcb_window_t icon_window)
{
    xcb_atom_t wm_state;
    uint32_t values[2];

    if (client == NULL || xcb_connection_get() == NULL ||
            client->window == XCB_WINDOW_NONE) {
        return;
    }

    wm_state = ccmd_intern_atom(xcb_connection_get(), "WM_STATE");
    if (wm_state == XCB_ATOM_NONE) {
        return;
    }

    values[0] = state;
    values[1] = icon_window;
    xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
            client->window, wm_state, wm_state, 32, 2, values);
}


/*  Remove the ICCCM 'WM_STATE' property from a client */
void ccmd_clear_wm_state(client_td *client)
{
    xcb_atom_t wm_state;

    if (client == NULL || xcb_connection_get() == NULL ||
            client->window == XCB_WINDOW_NONE) {
        return;
    }

    wm_state = ccmd_intern_atom(xcb_connection_get(), "WM_STATE");
    if (wm_state == XCB_ATOM_NONE) {
        return;
    }

    xcb_delete_property(xcb_connection_get(), client->window, wm_state);
}


/* Publish '_NET_FRAME_EXTENTS' on the client window */
void ccmd_publish_frame_extents(client_td *client,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom)
{
    uint32_t extents[4];

    if (client == NULL || xcb_ewmh_connection_get() == NULL) {
        return;
    }

    extents[0] = left;
    extents[1] = right;
    extents[2] = top;
    extents[3] = bottom;
    xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
            client->window, xcb_ewmh_connection_get()->_NET_FRAME_EXTENTS,
            XCB_ATOM_CARDINAL, 32, 4, extents);
}


/* Publish '_NET_WM_DESKTOP' on the client window */
void ccmd_publish_wm_desktop(client_td *client, uint32_t desktop_id)
{
    uint32_t did;

    if (client == NULL || xcb_ewmh_connection_get() == NULL) {
        return;
    }

    did = (client->properties.flags & CLIENT_FLAG_PIN)
        ? WM_DESKTOP_ID_ALL : desktop_id;
    xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
            client->window, xcb_ewmh_connection_get()->_NET_WM_DESKTOP,
            XCB_ATOM_CARDINAL, 32, 1, &did);
}


/* Send a '_NET_WM_PING' probe to a client */
void ccmd_client_ping_send(client_td *client)
{
    const xcb_ewmh_connection_t *ewmh;
    xcb_client_message_event_t ev;
    uint32_t timestamp;

    if (client == NULL || !client->hints_ewmh.ping.is_supported ||
            xcb_ewmh_connection_get() == NULL) {
        return;
    }

    ewmh = xcb_ewmh_connection_get();

    /* EWMH §4.6: the timestamp only has to let this window manager tell
     * one probe apart from another, never carrying the ICCCM §4.1.7
     * focus-granting weight 'ccmd_client_focus' gives its own, so
     * falling back to 'XCB_CURRENT_TIME' here is harmless even though
     * it stays that way for a window manager that has not seen any real
     * input yet */
    timestamp = (client_last_user_time() != 0u)
        ? client_last_user_time() : (uint32_t) XCB_CURRENT_TIME;

    memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.format = 32;
    ev.window = client->window;
    ev.type = ewmh->WM_PROTOCOLS;
    ev.data.data32[0] = ewmh->_NET_WM_PING;
    ev.data.data32[1] = timestamp;
    ev.data.data32[2] = client->window;
    xcb_send_event(xcb_connection_get(), 0, client->window,
            XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);

    client->hints_ewmh.ping.last_sent = timestamp;
    client->hints_ewmh.ping.is_waiting = true;
}
