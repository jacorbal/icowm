/**
 * @file cmds/client/ewmh.h
 *
 * @brief Functions writing a client's ICCCM @c WM_STATE and EWMH
 *        @c _NET_WM_STATE properties
 *
 * @defgroup cmds Client, desktop, and surface commands
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_CCMD_EWMH_H
#define CMDS_CCMD_EWMH_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <types/handles.h>


/* ICCCM WM_STATE property values (ICCCM section 4.1.3.1) */
#define CCMD_WM_STATE_WITHDRAWN (0u)
#define CCMD_WM_STATE_NORMAL (1u)
#define CCMD_WM_STATE_ICONIC (3u)


/* Public interface */
/**
 * @brief Write the ICCCM @c WM_STATE property for a client
 *
 * Stores the client state and optional icon window in the legacy
 * @c WM_STATE property expected by pagers, taskbars, and older X11
 * clients.
 *
 * @param client      Pointer to the client
 * @param state       ICCCM window-manager state value
 * @param icon_window Icon window associated with @p state, or
 *                    @c XCB_NONE
 *
 * @note Not EWMH: @c WM_STATE is ICCCM's, and predates it
 * @note Complexity: @e O(n), where @e n is the length of @c WM_STATE
 */
void ccmd_set_wm_state(client_td *client,
        uint32_t state, xcb_window_t icon_window);

/**
 * @brief Remove the ICCCM @c WM_STATE property from a client
 *
 * Deletes the legacy @c WM_STATE property, typically when the client is
 * being withdrawn from window-manager control.
 *
 * @param client Pointer to the client
 *
 * @note Not EWMH: @c WM_STATE is ICCCM's, and predates it
 * @note Complexity: @e O(n), where @e n is the length of @c WM_STATE
 */
void ccmd_clear_wm_state(client_td *client);

/**
 * @brief Republish every @c _NET_WM_STATE atom a client currently
 *        holds, read straight off its fields, in one single XCB
 *        write
 *
 * Openbox's real answer to keeping @c _NET_WM_STATE in sync
 * (confirmed directly against its source, @c client_change_state in
 * @c client.c): rebuild the whole list from scratch every time, from
 * whichever of the client's boolean fields are true right now,
 * rather than reading the property back first to add or remove one
 * specific atom.  See the full reasoning in @c cmds/client/ewmh.c,
 * right above the implementation, for exactly how each atom maps to
 * @p client's fields.
 *
 * @param client Client whose current state to republish
 *
 * @note A null @p client or one with no @c ewmh connection is a
 *       silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_sync_states(client_td *client);

/**
 * @brief Intern an atom name in the X11 system
 *
 * @param connection Pointer to the X11 connection
 * @param name       Name of the atom
 *
 * @return Interned atom ID, or @c XCB_ATOM_NONE on failure
 *
 * @note Not EWMH-specific: interning a name is core X, and any
 *       protocol's atoms come through here
 * @note Complexity: @e O(n), where @e n is the length of @p name
 */
xcb_atom_t ccmd_intern_atom(xcb_connection_t *connection,
        const char *name);

/**
 * @brief Publish @c _NET_FRAME_EXTENTS on the client window
 *
 * Writes the EWMH @c _NET_FRAME_EXTENTS cardinal property so that
 * taskbars and other clients know the exact size of the decoration
 * added around the content window.  No-op when @p client or its
 * @c ewmh connection is null.
 *
 * @param client Pointer to the client
 * @param left   Left frame extent in pixels
 * @param right  Right frame extent in pixels
 * @param top    Top frame extent in pixels (includes titlebar height)
 * @param bottom Bottom frame extent in pixels
 *
 * @note Complexity: @e O(1)
 */
void ccmd_publish_frame_extents(client_td *client,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom);

/**
 * @brief Publish @c _NET_WM_DESKTOP on the client window
 *
 * Writes the EWMH @c _NET_WM_DESKTOP cardinal property so that
 * pagers and taskbars learn which desktop @p client now belongs to,
 * the single place every desktop-move path shares for it instead of
 * each writing the property by hand.  A pinned client publishes the
 * EWMH "all desktops" sentinel regardless of @p desktop_id, since
 * its registration under one particular desktop is bookkeeping this
 * window manager needs, not something an external tool watching
 * this property should ever see change.  No-op when @p client or
 * its @c ewmh connection is null.
 *
 * @param client     Pointer to the client
 * @param desktop_id Desktop @p client now belongs to
 *
 * @note Complexity: @e O(1)
 */
void ccmd_publish_wm_desktop(client_td *client, uint32_t desktop_id);

/**
 * @brief Send a @c _NET_WM_PING probe to a client
 *
 * Builds and sends the @c WM_PROTOCOLS @c ClientMessage EWMH §4.6
 * (and ICCCM §4.2.8, the same envelope @c WM_DELETE_WINDOW and
 * @c WM_TAKE_FOCUS already use) describes, and marks the ping as
 * outstanding on @p client so @c policy/ping.h's periodic scan
 * knows to start counting toward @c WM_EWMH_PING_TIMEOUT_SECONDS.
 * A no-op when @p client does not advertise @c _NET_WM_PING
 * support.
 *
 * @param client Client to probe
 *
 * @note A null @p client or one with no @c ewmh connection is a
 *       silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_ping_send(client_td *client);


#endif  /* ! CMDS_CCMD_EWMH_H */
