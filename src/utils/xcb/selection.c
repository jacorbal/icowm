/**
 * @file utils/xcb/selection.c
 *
 * @brief ICCCM manager-selection acquisition shared by every built-in
 *        selection-owning manager
 *
 * Claiming a @c MANAGER-convention selection (@c _NET_SYSTEM_TRAY_Sn
 * for the systray, @c _XSETTINGS_Sn for the XSETTINGS manager, and any
 * future one) always follows the same three ICCCM steps: set ownership,
 * verify the server actually granted it, and broadcast the standard
 * MANAGER client message on the root window so other tools notice.
 * This one implementation replaces what used to be an identical
 * sequence copied into @c systray/protocol.c and @c xsettings.c.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>     /* free, NULL */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/xcb/reply.h>

/* Local includes */
#include <utils/xcb/selection.h>


/* Acquire an ICCCM manager selection, announcing it on 'root' */
bool util_xcb_acquire_manager_selection(xcb_connection_t *connection,
        xcb_window_t window, xcb_atom_t selection_atom,
        xcb_atom_t manager_atom, xcb_window_t root)
{
    xcb_get_selection_owner_cookie_t owner_cookie;
    xcb_get_selection_owner_reply_t *owner_reply;
    xcb_generic_error_t *owner_error = NULL;
    xcb_client_message_event_t manager_ev;

    xcb_set_selection_owner(connection, window, selection_atom,
            XCB_CURRENT_TIME);

    owner_cookie = xcb_get_selection_owner(connection, selection_atom);
    owner_reply = xcb_get_selection_owner_reply(connection,
            owner_cookie, &owner_error);
    if (owner_reply == NULL || owner_reply->owner != window) {
        xcb_reply_log_error(owner_error,
                "the owner of a selection just claimed");
        free(owner_reply);
        return false;
    }
    free(owner_error);
    free(owner_reply);

    /* ICCCM manager-selection convention: announce ownership on the
     * root window so other tools notice a manager appeared */
    memset(&manager_ev, 0, sizeof(manager_ev));
    manager_ev.response_type = XCB_CLIENT_MESSAGE;
    manager_ev.format = 32;
    manager_ev.window = root;
    manager_ev.type = manager_atom;
    manager_ev.data.data32[0] = XCB_CURRENT_TIME;
    manager_ev.data.data32[1] = selection_atom;
    manager_ev.data.data32[2] = window;
    xcb_send_event(connection, 0, root,
            XCB_EVENT_MASK_STRUCTURE_NOTIFY,
            (const char *) &manager_ev);

    return true;
}
