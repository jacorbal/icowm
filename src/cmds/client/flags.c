/**
 * @file cmds/client/flags.c
 *
 * @brief Pin, urgency, and allowed-actions state toggles over clients
 *
 * One of the files @c cmds/client/ is made of.
 * Not named @c state.c because @c cmds/client/state.c already holds
 * an unrelated set of client states (fullscreen, shade, maximize).
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
#include <stddef.h>     /* NULL */
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
#include <cmds/client/ewmh.h>
#include <cmds/client/flags.h>
#include <cmds/client/focus.h>
#include <cmds/client/screen.h>
#include <cmds/client/state.h>
#include <cmds/client/transient.h>
#include <cmds/client/visibility.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


/**
 * @brief Pin exactly this one client, ignoring any transient family
 *        it may belong to
 *
 * Holds the single-client half of @a ccmd_client_pin, so
 * that function can redirect to, and cascade across, a transient
 * family (see its comment).
 *
 * @param client Client to pin; must be non-null
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_pin_one(client_td *client)
{
    uint32_t all_desktops;

    client_pin(client);
    ccmd_client_sync_states(client);
    if (xcb_ewmh_connection_get() != NULL) {
        all_desktops = WM_DESKTOP_ID_ALL;
        xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
                client->window, xcb_ewmh_connection_get()->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &all_desktops);
    }

    wm_request_client_redraw(client);
}


/**
 * @brief Pin one transient family member
 *
 * @param member Family member to act on
 * @param ctx    Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_pin_visit(client_td *member, void *ctx)
{
    (void) ctx;

    if (!client_is_pinned(member)) {
        s_ccmd_client_pin_one(member);
    }
}


/**
 * @brief Unpin exactly this one client, ignoring any transient family
 *        it may belong to
 *
 * Holds the single-client half of @a ccmd_client_unpin, so
 * that function can redirect to, and cascade across, a transient
 * family (see its comment).
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
    ccmd_client_sync_states(client);
    if (xcb_ewmh_connection_get() != NULL) {
        xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
                client->window, xcb_ewmh_connection_get()->_NET_WM_DESKTOP,
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
        ccmd_client_unmap_decorated(client, target);

        if (client->icon_window != 0 && client->is_icon_mapped) {
            xcb_window_hide(client->icon_window);
            client->is_icon_mapped = false;
        }

        /* If the unstickied client held focus on the current desktop,
         * transfer focus to the MRU client still on that desktop */
        ccmd_client_focus_fallback(client);
    }

    wm_request_client_redraw(client);
}


/**
 * @brief Unpin one transient family member
 *
 * @param member Family member to act on
 * @param ctx    Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_unpin_visit(client_td *member, void *ctx)
{
    (void) ctx;

    if (client_is_pinned(member) && !client_is_locked(member)) {
        s_ccmd_client_unpin_one(member);
    }
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
 * desktop while its pinned parent now follows the user to
 * every other one would be exactly that kind of split.  Pinning any
 * single member of a transient family here pins the family's
 * top-most ancestor (@a ccmd_client_transient_top_parent) first, then
 * every other member of that same family not already pinned, so the
 * whole group stays together on every desktop from then on.  A
 * client with no transient relatives at all is unaffected.  Its
 * top parent is itself, and no sibling scan finds anything else to
 * cascade to.
 *
 * @param client Client to pin
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the top parent's desktop
 */
void ccmd_client_pin(client_td *client)
{
    client_td *top;

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

    ccmd_client_family_apply(top, s_ccmd_client_pin_visit, NULL);
}


/**
 * @brief Unpin the client from every desktop but its, taking its
 *        whole transient family along with it
 *
 * The matching half of @a ccmd_client_pin's transient-family
 * cascade (see its comment for the full reasoning).  Redirects
 * to the family's top-most ancestor first, unpinning it exactly as
 * this function always has, then unpins every other family member
 * still pinned too, so a family pinned together stays together when
 * unpinned as well.
 *
 * @param client Client to unpin
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the top parent's desktop
 */
void ccmd_client_unpin(client_td *client)
{
    client_td *top;

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

    ccmd_client_family_apply(top, s_ccmd_client_unpin_visit, NULL);
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


/* Override the client's active-state opacity */
void ccmd_client_set_opacity_active(client_td *client, uint8_t percent)
{
    if (client == NULL) {
        return;
    }

    client->opacity_override.is_set_active = true;
    client->opacity_override.active = percent;
    wm_request_client_redraw(client);
}


/* Override the client's inactive-state opacity */
void ccmd_client_set_opacity_inactive(client_td *client,
        uint8_t percent)
{
    if (client == NULL) {
        return;
    }

    client->opacity_override.is_set_inactive = true;
    client->opacity_override.inactive = percent;
    wm_request_client_redraw(client);
}


/* Override the client's border color and width */
void ccmd_client_set_border_override(client_td *client,
        uint32_t color, uint32_t width)
{
    if (client == NULL) {
        return;
    }

    client->border_override.is_set = true;
    client->border_override.color = color;
    client->border_override.width = width;
    wm_request_client_redraw(client);
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
    ccmd_client_sync_states(client);

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
    ccmd_client_sync_states(client);

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
    xcb_ewmh_connection_t *ewmh;

    if (client == NULL || xcb_ewmh_connection_get() == NULL) {
        return;
    }

    ewmh = xcb_ewmh_connection_get();

    /* Actions available to all managed, visible clients */
    actions[n++] = ewmh->_NET_WM_ACTION_CLOSE;
    actions[n++] = ewmh->_NET_WM_ACTION_CHANGE_DESKTOP;

    /* None of moving, resizing or maximizing means anything while a
     * client is fullscreen: it occupies the monitor whole, and the
     * window manager holds it there until the state is dropped.
     * Advertising them anyway told a client it could ask for
     * something that would be refused. */
    if (client_is_resizable(client) && !client_is_fullscreen(client)) {
        actions[n++] = ewmh->_NET_WM_ACTION_MOVE;
        actions[n++] = ewmh->_NET_WM_ACTION_RESIZE;
        actions[n++] = ewmh->_NET_WM_ACTION_MAXIMIZE_HORZ;
        actions[n++] = ewmh->_NET_WM_ACTION_MAXIMIZE_VERT;
    }

    /* Not folded into the 'client_is_resizable' block above, unlike
     * maximize: fullscreen is a WM-forced override of the client's
     * own preferred geometry, not a user-convenience resize the
     * client's fixed size hints have any say over; see
     * 'ccmd_client_fullscreen''s comment for the full reasoning.
     * A DOS-emulation or retro-game window that fixes its size is
     * exactly the case this matters for: some such clients check this
     * very property before ever attempting '_NET_WM_STATE_FULLSCREEN'
     * at all, so advertising it as disallowed here would have kept
     * the fix in 'ccmd_client_fullscreen' itself from ever being
     * reached. */
    actions[n++] = ewmh->_NET_WM_ACTION_FULLSCREEN;

    /* The same predicate 'ccmd_client_iconify' refuses on, so that
     * what is advertised here and what actually happens cannot drift
     * apart */
    if (client_is_iconifiable(client)) {
        actions[n++] = ewmh->_NET_WM_ACTION_MINIMIZE;
    }

    /* All clients may be shaded, sticked, and re-stacked */
    actions[n++] = ewmh->_NET_WM_ACTION_SHADE;
    actions[n++] = ewmh->_NET_WM_ACTION_STICK;
    actions[n++] = ewmh->_NET_WM_ACTION_ABOVE;
    actions[n++] = ewmh->_NET_WM_ACTION_BELOW;
    xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
            client->window, ewmh->_NET_WM_ALLOWED_ACTIONS,
            XCB_ATOM_ATOM, 32, n, actions);
}
