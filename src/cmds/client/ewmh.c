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

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Utils includes */
#include <utils/xcb/atom.h>

/* Project includes */
#include <client.h>

/* Local includes */
#include <cmds/client/basic.h>
#include <cmds/client/internal.h>


/* Intern an atom name in the X11 system */
xcb_atom_t ccmd_intern_atom(xcb_connection_t *connection,
        const char *name)
{
    return atom_intern(connection, name, false);
}


/**
 * @brief Republish every @c _NET_WM_STATE atom a client currently
 *        holds, read straight off its own fields, in one single XCB
 *        write
 *
 * Openbox's own real answer to keeping @c _NET_WM_STATE in sync
 * (confirmed directly against its source, @c client_change_state in
 * @c client.c): rebuild the whole list from scratch every time, from
 * whichever of the client's own boolean fields are true right now,
 * rather than reading the property back first to add or remove one
 * specific atom from whatever was already there.  @a ccmd_add_states
 * and @a ccmd_rem_states did the opposite: a read (one XCB round
 * trip) followed by a merge and a write, on every single call, at
 * every one of the dozens of call sites across this project that
 * change some piece of a client's own state.  This function needs
 * only the write: every state below already has its own single
 * source of truth living directly on @p client itself (@c
 * properties.state, @c properties.layer, or a @c CLIENT_FLAG_* bit),
 * so there is nothing to read back and merge with in the first
 * place.
 *
 * Called once, after whichever single field actually changed has
 * already been updated, by every caller that used to call @a ccmd_
 * add_states/@a ccmd_rem_states directly; @a ccmd_add_states and
 * @a ccmd_rem_states themselves no longer exist; every one of their
 * old call sites now sets its own underlying field first (most
 * already did, right alongside the old add/rem call, since the
 * property was only ever meant to mirror that field to begin with)
 * and calls this instead.
 *
 * @c _NET_WM_STATE_MAXIMIZED_HORZ/@c _VERT do not map to a single
 * bit each the way most of the others do: @c properties.state holds
 * one of @c CLIENT_STATE_MAXIMIZED (both axes), @c _MAXIMIZED_HORZ,
 * or @c _MAXIMIZED_VERT (one axis) as three distinct, mutually
 * exclusive values, so each of the two atoms is published whenever
 * @c properties.state matches either the combined value or its own
 * single-axis one.  @c _NET_WM_STATE_HIDDEN similarly covers two
 * separate concepts this project tracks apart from each other
 * internally (@c properties.state @c == @c CLIENT_STATE_ICONIFIED,
 * and @c CLIENT_FLAG_HIDDEN, a client hidden without being
 * iconified; see @a ccmd_client_hide, cmds/client/visibility.c) but
 * that EWMH itself does not distinguish, so it is published whenever
 * either one holds.
 *
 * @c _NET_WM_STATE_FOCUSED has no matching field on @c xcb_ewmh_
 * connection_t (a newer, less universally standard extension than
 * the rest), so it is the one atom here still resolved through @a
 * ccmd_intern_atom rather than read directly off @p client->ewmh;
 * @a atom_intern's own internal cache (@c utils/xcb/atom.c) already
 * makes every call after the very first one a plain lookup, no XCB
 * round trip, so this costs nothing extra on every later sync.
 *
 * @param client Client whose current state to republish
 *
 * @note A null @p client or one with no @c ewmh connection is a
 *       silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_sync_states(client_td *client)
{
    xcb_atom_t states[13];
    uint32_t num = 0;

    if (client == NULL || client->ewmh == NULL) {
        return;
    }

    if (client->properties.state == (uint16_t) CLIENT_STATE_MAXIMIZED ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ) {
        states[num++] = client->ewmh->_NET_WM_STATE_MAXIMIZED_HORZ;
    }
    if (client->properties.state == (uint16_t) CLIENT_STATE_MAXIMIZED ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED_VERT) {
        states[num++] = client->ewmh->_NET_WM_STATE_MAXIMIZED_VERT;
    }
    if (client->properties.state ==
            (uint16_t) CLIENT_STATE_FULLSCREEN) {
        states[num++] = client->ewmh->_NET_WM_STATE_FULLSCREEN;
    }
    if (client->properties.state ==
                (uint16_t) CLIENT_STATE_ICONIFIED ||
            client_is_hidden(client)) {
        states[num++] = client->ewmh->_NET_WM_STATE_HIDDEN;
    }
    if (client_is_pinned(client)) {
        states[num++] = client->ewmh->_NET_WM_STATE_STICKY;
    }
    if (client_is_urgent(client)) {
        states[num++] = client->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION;
    }
    if (client_is_shaded(client)) {
        states[num++] = client->ewmh->_NET_WM_STATE_SHADED;
    }
    if (client->properties.layer == (uint16_t) CLIENT_LAYER_ABOVE) {
        states[num++] = client->ewmh->_NET_WM_STATE_ABOVE;
    }
    if (client->properties.layer == (uint16_t) CLIENT_LAYER_BELOW) {
        states[num++] = client->ewmh->_NET_WM_STATE_BELOW;
    }
    if (client_is_modal(client)) {
        states[num++] = client->ewmh->_NET_WM_STATE_MODAL;
    }
    if (client_is_focused(client)) {
        states[num++] = ccmd_intern_atom(client->connection,
                "_NET_WM_STATE_FOCUSED");
    }
    if (client->properties.flags & CLIENT_FLAG_SKIP_TASKBAR) {
        states[num++] = client->ewmh->_NET_WM_STATE_SKIP_TASKBAR;
    }
    if (client->properties.flags & CLIENT_FLAG_SKIP_PAGER) {
        states[num++] = client->ewmh->_NET_WM_STATE_SKIP_PAGER;
    }

    xcb_ewmh_set_wm_state(client->ewmh, client->window, num, states);
    xcb_flush(client->connection);
}


/* Write the ICCCM 'WM_STATE' property for a client */
void ccmd_set_wm_state(client_td *client,
        uint32_t state, xcb_window_t icon_window)
{
    xcb_atom_t wm_state;
    uint32_t values[2];

    if (client == NULL || client->connection == NULL ||
            client->window == XCB_WINDOW_NONE) {
        return;
    }

    wm_state = ccmd_intern_atom(client->connection, "WM_STATE");
    if (wm_state == XCB_ATOM_NONE) {
        return;
    }

    values[0] = state;
    values[1] = icon_window;
    xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
            client->window, wm_state, wm_state, 32, 2, values);
    xcb_flush(client->connection);
}


/*  Remove the ICCCM 'WM_STATE' property from a client */
void ccmd_clear_wm_state(client_td *client)
{
    xcb_atom_t wm_state;

    if (client == NULL || client->connection == NULL ||
            client->window == XCB_WINDOW_NONE) {
        return;
    }

    wm_state = ccmd_intern_atom(client->connection, "WM_STATE");
    if (wm_state == XCB_ATOM_NONE) {
        return;
    }

    xcb_delete_property(client->connection, client->window, wm_state);
    xcb_flush(client->connection);
}


/* Publish '_NET_FRAME_EXTENTS' on the client window */
void ccmd_publish_frame_extents(client_td *client,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom)
{
    uint32_t extents[4];

    if (client == NULL || client->ewmh == NULL) {
        return;
    }

    extents[0] = left;
    extents[1] = right;
    extents[2] = top;
    extents[3] = bottom;
    xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
            client->window, client->ewmh->_NET_FRAME_EXTENTS,
            XCB_ATOM_CARDINAL, 32, 4, extents);
}
