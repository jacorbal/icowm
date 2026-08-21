/**
 * @file cmds/client/flags.c
 *
 * @brief Pin, urgency, and allowed-actions state toggles over clients
 *
 * Split out of what used to be a single, flat @c cmds/client/basic.c.
 * Not named @c state.c to avoid colliding with the existing @c
 * cmds/client/state.c (fullscreen/shade/maximize), an unrelated set
 * of client states already split out on its own before this session.
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

/* JSON includes */
#include <cjson/cJSON.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Default initial values */
#include <defs/desktop.h>
#include <defs/icon.h>

/* Windows & icons policy includes */
#include <policy/placement/window.h>

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

/* Local includes */
#include <cmds/client/basic.h>
#include <cmds/client/internal.h>


/**
 * @brief Pin exactly this one client, ignoring any transient family
 *        it may belong to
 *
 * Split out of what used to be the whole of @a ccmd_client_pin so
 * that function can redirect to, and cascade across, a transient
 * family (see its own doc comment).
 *
 * @param client Client to pin; must be non-null
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_pin_one(client_td *client)
{
    uint32_t all_desktops;

    client_pin(client);
    ccmd_add_states(client, 1, "_NET_WM_STATE_STICKY");
    if (client->ewmh != NULL) {
        all_desktops = WM_DESKTOP_ID_ALL;
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, client->ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &all_desktops);
    }

    wm_request_client_redraw(client);
}


/**
 * @brief Unpin exactly this one client, ignoring any transient family
 *        it may belong to
 *
 * Split out of what used to be the whole of @a ccmd_client_unpin so
 * that function can redirect to, and cascade across, a transient
 * family (see its own doc comment).
 *
 * @param client Client to unpin; must be non-null and unlocked
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_unpin_one(client_td *client)
{
    const desktop_td *owner_desktop;
    const desktop_td *current_desktop;
    surface_td *surface;
    xcb_window_t target;

    client_unpin(client);
    ccmd_rem_states(client, 1, "_NET_WM_STATE_STICKY");
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
        target = ccmd_target_win(client);

        /* Account for the 'UnmapNotify' events that
         * 'handler_unmap_notify' must skip.  Two events arrive for the
         * unmapped target ('SubstructureNotify' on parent
         * + 'StructureNotify' on target) and one additional event for
         * the titlebar via the frame's 'SubstructureNotify'. */
        client->ignore.unmap += 2u;
        if (client->titlebar != 0) {
            client->ignore.unmap += 1u;
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
        ccmd_client_focus_fallback(client);
    }

    wm_request_client_redraw(client);
}


/**
 * @brief Pin the client to every desktop, taking its whole transient
 *        family along with it
 *
 * ICCCM §4.1.2.6 dialogs and the window they belong to are, for
 * every purpose this whole session's transient-family cascade has
 * already covered (iconify, restore, desktop moves), treated as one
 * single unit that can never be split across desktops; pin state is
 * no different, since a "save changes?" prompt left behind on one
 * desktop while its own pinned parent now follows the person to
 * every other one would be exactly that kind of split.  Pinning any
 * single member of a transient family here pins the family's own
 * top-most ancestor (@a ccmd_client_transient_top_parent) first, then
 * every other member of that same family not already pinned, so the
 * whole group stays together on every desktop from then on.  A
 * client with no transient relatives at all is unaffected: its own
 * top parent is itself, and no sibling scan finds anything else to
 * cascade to.
 *
 * @param client Client to pin
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the top parent's own desktop
 */
void ccmd_client_pin(client_td *client)
{
    client_td *top;
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    if (!client_is_pinned(top)) {
        s_ccmd_client_pin_one(top);
    }

    desktop = wm_get_client_desktop(top);
    if (desktop != NULL && desktop->clients != NULL) {
        size_t capacity = ohtbl_size(desktop->clients);
        client_td **siblings = malloc(capacity * sizeof(*siblings));
        size_t count = 0;

        /* Collected into a snapshot array first, rather than calling
         * 's_ccmd_client_pin_one' directly from inside this same
         * 'ohtbl_foreach' pass below; see 'ccmd_client_iconify''s own
         * matching comment (cmds/client/visibility.c) for the full
         * reasoning: iterating and mutating 'desktop->clients' at
         * once is undefined behavior for 'ohtbl_foreach'. */
        if (siblings != NULL) {
            void *elem;

            ohtbl_foreach(desktop->clients, elem) {
                client_td *const sibling = (client_td *) elem;

                if (sibling != NULL && sibling != top &&
                        !client_is_pinned(sibling) &&
                        ccmd_client_transient_top_parent(sibling) ==
                            top) {
                    siblings[count] = sibling;
                    count++;
                }
            }

            for (size_t i = 0; i < count; i++) {
                s_ccmd_client_pin_one(siblings[i]);
            }

            free(siblings);
        }
    }
}


/**
 * @brief Unpin the client from every desktop but its own, taking its
 *        whole transient family along with it
 *
 * The matching half of @a ccmd_client_pin's own transient-family
 * cascade (see its own doc comment for the full reasoning): redirects
 * to the family's top-most ancestor first, unpinning it exactly as
 * this function always has, then unpins every other family member
 * still pinned too, so a family pinned together stays together when
 * unpinned as well.
 *
 * @param client Client to unpin
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the top parent's own desktop
 */
void ccmd_client_unpin(client_td *client)
{
    client_td *top;
    desktop_td *desktop;

    if (client == NULL || client_is_locked(client)) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    if (client_is_pinned(top) && !client_is_locked(top)) {
        s_ccmd_client_unpin_one(top);
    }

    desktop = wm_get_client_desktop(top);
    if (desktop != NULL && desktop->clients != NULL) {
        size_t capacity = ohtbl_size(desktop->clients);
        client_td **siblings = malloc(capacity * sizeof(*siblings));
        size_t count = 0;

        /* Collected into a snapshot array first, rather than calling
         * 's_ccmd_client_unpin_one' directly from inside this same
         * 'ohtbl_foreach' pass below; see 'ccmd_client_iconify''s own
         * matching comment (cmds/client/visibility.c) for the full
         * reasoning: iterating and mutating 'desktop->clients' at
         * once is undefined behavior for 'ohtbl_foreach'. */
        if (siblings != NULL) {
            void *elem;

            ohtbl_foreach(desktop->clients, elem) {
                client_td *const sibling = (client_td *) elem;

                if (sibling != NULL && sibling != top &&
                        client_is_pinned(sibling) &&
                        !client_is_locked(sibling) &&
                        ccmd_client_transient_top_parent(sibling) ==
                            top) {
                    siblings[count] = sibling;
                    count++;
                }
            }

            for (size_t i = 0; i < count; i++) {
                s_ccmd_client_unpin_one(siblings[i]);
            }

            free(siblings);
        }
    }
}


/* Toggle stickiness */
void ccmd_client_toggle_pin(client_td *client)
{
    const surface_td *surface;

    if (client == NULL || client_is_locked(client)) {
        return;
    }

    /* Meaningless with only one desktop: "visible on every desktop"
     * and "visible on this one desktop" are the exact same thing when
     * there is only the one, so there is nothing to actually toggle.
     * The menu entry for this is already hidden in that case (see
     * 'wincmenu.c', which omits the whole "Send to desktop" submenu
     * it lives in), but the keyboard shortcut has no such menu to
     * hide behind, so it is guarded here instead, at the one place
     * both of them ultimately call through. */
    surface = wm_get_surface_by_id(client->screen_id);
    if (surface != NULL && surface->desktop_count <= 1u) {
        return;
    }

    if (client_is_pinned(client)) {
        ccmd_client_unpin(client);
    } else {
        ccmd_client_pin(client);
    }
}


/* Raise the client to the top */
void ccmd_client_urge(client_td *client)
{
    desktop_td *desktop;
    cJSON *fields;

    if (client == NULL) {
        return;
    }

    client_urge(client);
    ccmd_add_states(client, 1, "_NET_WM_STATE_DEMANDS_ATTENTION");

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        desktop_action_recompute_urgent(desktop);
    }
    wm_outdate_client(client);

    fields = cJSON_CreateObject();
    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "client_id",
                (double) client->id);
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) client->desktop_id);
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) client->screen_id);
    }
    ipc_broadcast_event(IPC_EVENT_URGENCY_SET, fields);
}


/* Clear client urgency */
void ccmd_client_unurge(client_td *client)
{
    desktop_td *desktop;
    cJSON *fields;

    if (client == NULL) {
        return;
    }

    client_unurge(client);
    ccmd_rem_states(client, 1, "_NET_WM_STATE_DEMANDS_ATTENTION");

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        desktop_action_recompute_urgent(desktop);
    }
    wm_outdate_client(client);

    fields = cJSON_CreateObject();
    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "client_id",
                (double) client->id);
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) client->desktop_id);
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) client->screen_id);
    }
    ipc_broadcast_event(IPC_EVENT_URGENCY_CLEARED, fields);
}


/* Publish '_NET_WM_ALLOWED_ACTIONS' based on the client's current
 * properties */
void ccmd_client_update_allowed_actions(client_td *client)
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
    }

    /* Not folded into the 'client_is_resizable' block above, unlike
     * maximize: fullscreen is a WM-forced override of the client's
     * own preferred geometry, not a user-convenience resize the
     * client's own fixed size hints have any say over; see
     * 'ccmd_client_fullscreen''s own comment for the full reasoning.
     * A DOS-emulation or retro-game window that fixes its own size is
     * exactly the case this matters for: some such clients check this
     * very property before ever attempting '_NET_WM_STATE_FULLSCREEN'
     * at all, so advertising it as disallowed here would have kept
     * the fix in ccmd_client_fullscreen itself from ever being
     * reached. */
    actions[n++] = client->ewmh->_NET_WM_ACTION_FULLSCREEN;

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
