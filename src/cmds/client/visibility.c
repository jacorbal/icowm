/**
 * @file cmds/client/visibility.c
 *
 * @brief Iconify, hide, and unhide actions over clients
 *
 * One of the files @c cmds/client/ is made of.
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

/* Utils includes */
#include <utils/geom.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>

/* Windows & icons policy includes */
#include <policy/placement/window.h>

/* Render includes */
#include <render/outdate.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <ipc.h>
#include <lookup.h>
#include <systray.h>
#include <wm.h>

/* Command includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/focus.h>
#include <cmds/client/icon.h>
#include <cmds/client/screen.h>
#include <cmds/client/state.h>
#include <cmds/client/transient.h>

/* Local includes */
#include <cmds/client/visibility.h>


/**
 * @brief Iconize exactly this one client, ignoring any transient family
 *        it may belong to
 *
 * Holds the single-client half of @a ccmd_client_iconify, so that
 * function can redirect to, and cascade across, a transient family (see
 * its comment) while still sharing this single client's worth of
 * ICCCM/EWMH bookkeeping with the top-level, family- unaware call sites
 * (@c handler/map.c's initial-iconic handling among them) that only
 * ever operate on one already-resolved client and have no family to
 * cascade to in the first place.
 *
 * @param client Client to iconize; must be non-null and unlocked
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_iconify_one(client_td *client)
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

    /* Nothing is remembered here on purpose.  The state bits are
     * independent, so whatever this client holds it goes on holding
     * while iconified, and 'ccmd_client_restore' has only to clear the
     * iconified bit and put the geometry back: "A window manager may
     * implement [additional states] as proper substates of NormalState
     * and IconicState, or it may treat them as independent flags,
     * allowing e.g., a maximized window to be iconified and to
     * re-appear as maximized upon de-iconification" (X Desktop Group,
     * 2013, "Extended Window Manager Hints", v1.5,
     * §2.1.1). */
    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    /* Leave full screen for its geometry alone, then put the state bit
     * straight back.  An iconified client's geometry is meant to be
     * restored to its pre-iconify size later (see the
     * 'client_geometry_save' call just below), and while still full
     * screen that size is the whole screen rather than the window's
     * own; leaving it is what puts the real geometry back, along with
     * the decoration and the frame extents.
     *
     * The bit itself is another matter, and belongs to the client:
     * asking to be iconified says nothing about full screen, so
     * dropping it here would discard a state nobody asked to lose,
     * which is exactly what the quotation above forbids.  Restored with
     * the bit still standing, 'ccmd_client_restore' re-enters full
     * screen and the window comes back as it went away. */
    if (client_is_fullscreen(client)) {
        ccmd_client_unfullscreen(client);
        client->properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
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
    handled_atom = ccmd_intern_atom(xcb_connection_get(),
            "_NET_WM_HANDLED_ICONS");
    handled_reply = xcb_get_property_reply(xcb_connection_get(),
            xcb_get_property(xcb_connection_get(), 0, client->parent_id,
                handled_atom, XCB_ATOM_CARDINAL, 0, 1), NULL);

    /* A client that both asks to be left out of the taskbar/cycle list
     * AND belongs to a transient family with some other, further-up
     * member to represent it (its top parent differs from itself) gets
     * no icon box of its own here.  That top parent already gets one
     * (see 'ccmd_client_iconify''s cascading doc comment), and
     * a second, unselectable box for this same family sitting right
     * next to it would be pure visual clutter with no purpose, exactly
     * the "one icon per window instead of one per group" clutter this
     * guard exists to avoid.  Excluded from this specifically when this
     * client's top parent is itself (a standalone client with no family
     * to fall back on): skipping its only icon box there would leave it
     * with no way back at all, taskbar-excluded and icon-less both. */
    skip_icon_win = (handled_reply != NULL &&
            xcb_get_property_value_length(handled_reply) > 0) ||
        ((client->properties.flags & CLIENT_FLAG_SKIP_TASKBAR) != 0 &&
         ccmd_client_transient_top_parent(client) != client);

    free(handled_reply);

    /* Compute icon height once, shared by icon window creation below
     * and the '_NET_WM_ICON_GEOMETRY' property published further down */
    icon_h_out = (uint16_t) (WM_ICON_SQUARE_SIZE +
            ((client->config->theme.icon.is_captioned)
             ? WM_ICON_CAPTION_HEIGHT
             : 0u));

    if (!skip_icon_win) {
        ccmd_client_ensure_icon_window(client, icon_h_out);
    }

    /* Account for the 'UnmapNotify' events that 'handler_window_unmap_notify'
     * must skip, the same reasoning as 'ccmd_client_hide''s identical
     * comment: two events arrive for the unmapped target
     * ('SubstructureNotify' on parent + 'StructureNotify' on target)
     * and one additional event for the titlebar via the frame's
     * 'SubstructureNotify'.  If 'target' is the frame, the content
     * window is also unmapped explicitly below, producing two more
     * events for 'client->window'.  Without this, an iconified
     * decorated client's content-window 'UnmapNotify' reaches
     * 'handler_window_unmap_notify' with 'ignore_unmap' still zero, which that
     * handler reads as the client withdrawing itself rather than the
     * window manager iconifying it. */
    ccmd_client_unmap_decorated(client, target);
    if (target != client->window) {
        client->ignore.unmap += 2u;
        xcb_window_hide(client->window);
    }

    if (!skip_icon_win) {
        xcb_window_t tray_below;
        uint32_t wm_state_vals[2];
        uint32_t icon_geom[4];

        xcb_window_show(client->icon_window);

        /* Icons are meant to sit even lower than the tray whenever it
         * is in the 'below' layer, "stuck to the desktop": stack just
         * below it explicitly, rather than via an unqualified 'below'
         * with no sibling, which would only put the icon under the tray
         * by coincidence of restack order rather than guarantee it.
         * The other direction (the tray restacking after icons already
         * exist) is handled on the tray's side; see
         * 'systray_layout_restack'. */
        tray_below = systray_below_window();
        if (tray_below != XCB_WINDOW_NONE) {
            xcb_window_stack_below(client->icon_window, tray_below);
        } else {
            xcb_window_lower(client->icon_window);
        }
        client->is_icon_mapped = true;

        /* ICCCM §4.1.3: mark the WM icon window as Withdrawn so pagers
         * that scan window trees treat it as unmanaged */
        wm_state_atom = ccmd_intern_atom(xcb_connection_get(), "WM_STATE");
        wm_state_vals[0] = CCMD_WM_STATE_WITHDRAWN;
        wm_state_vals[1] = XCB_NONE;

        xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
                client->icon_window, wm_state_atom, wm_state_atom,
                32, 2, wm_state_vals);

        /* EWMH: tell pagers and taskbars to skip the WM icon window */
        net_wm_state_atom = ccmd_intern_atom(xcb_connection_get(),
                "_NET_WM_STATE");
        skip_atoms[0] = ccmd_intern_atom(xcb_connection_get(),
                "_NET_WM_STATE_SKIP_PAGER");
        skip_atoms[1] = ccmd_intern_atom(xcb_connection_get(),
                "_NET_WM_STATE_SKIP_TASKBAR");
        xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
                client->icon_window, net_wm_state_atom,
                XCB_ATOM_ATOM, 32, 2, skip_atoms);

        /* EWMH §5.9: publish icon geometry on the client window so
         * taskbars can animate the iconify transition */
        icon_geom[0] = (uint32_t) client->icon_pos.x;
        icon_geom[1] = (uint32_t) client->icon_pos.y;
        icon_geom[2] = WM_ICON_SQUARE_SIZE;
        icon_geom[3] = icon_h_out;
        icon_geom_atom = ccmd_intern_atom(xcb_connection_get(),
                "_NET_WM_ICON_GEOMETRY");
        xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
                client->window, icon_geom_atom,
                XCB_ATOM_CARDINAL, 32, 4, icon_geom);
    }

    client_hide(client);

    /* Added rather than assigned: a maximized window that is iconified
     * is still maximized, only hidden, and EWMH says nothing that would
     * let one state discard the other. */
    client->properties.state |= (uint16_t) CLIENT_STATE_ICONIFIED;

    ccmd_set_wm_state(client, CCMD_WM_STATE_ICONIC,
            (skip_icon_win) ? XCB_NONE : client->icon_window);

    /* Iconify per EWMH: window hidden with '_NET_WM_STATE_HIDDEN'.
     * Icon display handled by pager/desktop */
    ccmd_client_sync_states(client);

    ccmd_client_focus_fallback(client);

    /* Ensure taskbars reflect the iconified state even when the client
     * was not the active one.  Function 'ccmd_client_focus_fallback'
     * only marks 'is_outdated' when it changes focus, so a non-active
     * iconification would otherwise not trigger 'wm_ewmh_sync'. */
    wm_request_client_redraw(client);

}


/**
 * @brief Whether hiding @p client would still do anything
 *
 * A plain hidden client (no icon, nothing mapped at all) has nothing
 * left for a further hide request to affect.  An iconified client
 * counts as already hidden too (see @a s_ccmd_client_iconify_one, which
 * calls @a client_hide as part of iconifying), yet still shows an icon
 * on screen; hiding it further means removing that icon, so a hide
 * request still has real work to do for it.
 *
 * @param client Client to test
 *
 * @return @c true if a hide request against @p client is not a no-op
 *
 * @note Complexity: @e O(1)
 */
static bool s_client_hide_still_applies(const client_td *client)
{
    return !client_is_hidden(client) || client_is_iconified(client);
}


/**
 * @brief Hide exactly this one client (minimize, but not iconify),
 *        ignoring any transient family it may belong to
 *
 * Holds the single-client half of @a ccmd_client_hide, so that function
 * can redirect to, and cascade across, a transient family (see its
 * comment).
 *
 * @param client Client to hide; must be non-null
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_hide_one(client_td *client)
{
    xcb_window_t target;
    stage_td *stage;

    /* An iconified client is already hidden by way of being iconified
     * (see 's_ccmd_client_iconify_one', which calls 'client_hide'
     * itself), with no content mapped for the general path below to
     * unmap; its icon is the only thing on screen, and hiding it
     * further means removing that instead, leaving it hidden outright
     * with no representation at all. */
    if (client_is_iconified(client)) {
        if (client->icon_window != 0 && client->is_icon_mapped) {
            xcb_window_hide(client->icon_window);
            client->is_icon_mapped = false;
        }

        client->properties.state &=
            (uint16_t) ~(uint16_t) CLIENT_STATE_ICONIFIED;

        ccmd_set_wm_state(client, CCMD_WM_STATE_ICONIC, XCB_NONE);
        ccmd_client_sync_states(client);
        wm_request_client_redraw(client);
        return;
    }

    stage = wm_get_stage_by_id(client->screen_id);
    if (stage != NULL && stage->is_showing_desktop) {
        stage->is_showing_desktop = false;
    }

    target = ccmd_target_win(client);

    /* Account for the 'UnmapNotify' events that 'handler_window_unmap_notify'
     * must skip.  Two events arrive for the unmapped target
     * ('SubstructureNotify' on parent + 'StructureNotify' on target)
     * and one additional event for the titlebar via the frame's
     * 'SubstructureNotify'.  If 'target' is the frame, the content
     * window is also unmapped explicitly below, producing two more
     * events for 'client->window'. */
    ccmd_client_unmap_decorated(client, target);
    if (target != client->window) {
        client->ignore.unmap += 2u;
        xcb_window_hide(client->window);
    }

    client_hide(client);

    ccmd_set_wm_state(client, CCMD_WM_STATE_ICONIC, XCB_NONE);
    ccmd_client_sync_states(client);

    ccmd_client_focus_fallback(client);
    wm_request_client_redraw(client);
}


/**
 * @brief Iconify one transient family member
 *
 * @param member Family member to act on
 * @param ctx    Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_iconify_visit(client_td *member, void *ctx)
{
    (void) ctx;

    if (client_is_locked(member)) {
        return;
    }

    /* Same reasoning as 'ccmd_client_iconify''s own check just below:
     * a transient family member is hidden along with the rest of the
     * family, never iconified into a real icon box of its own. */
    if (client_is_transient(member)) {
        if (!client_is_hidden(member)) {
            s_ccmd_client_hide_one(member);
        }
        return;
    }

    if (!client_is_iconified(member)) {
        s_ccmd_client_iconify_one(member);
    }
}


/**
 * @brief Hide one transient family member
 *
 * @param member Family member to act on
 * @param ctx    Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_hide_visit(client_td *member, void *ctx)
{
    (void) ctx;

    if (s_client_hide_still_applies(member) &&
            !client_is_locked(member)) {
        s_ccmd_client_hide_one(member);
    }
}


/**
 * @brief Show (unhide) exactly this one client, ignoring any transient
 *        family it may belong to
 *
 * Holds the single-client half of @a ccmd_client_unhide, so that
 * function can redirect to, and cascade across, a transient family (see
 * its comment).
 *
 * @param client Client to unhide; must be non-null
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_unhide_one(client_td *client)
{
    xcb_window_t target;

    target = ccmd_target_win(client);

    if (client->titlebar != 0) {
        xcb_window_show(client->titlebar);
    }

    xcb_window_show(target);

    if (target != client->window) {
        xcb_window_show(client->window);
    }

    client_unhide(client);

    ccmd_set_wm_state(client, CCMD_WM_STATE_NORMAL, XCB_NONE);
    ccmd_client_sync_states(client);

    /* An unhidden window becomes the one in use, matching the deiconify
     * behavior, so that picking a hidden window from the window menu
     * activates it for keyboard input straight away */
    ccmd_client_make_active(client);
    wm_request_client_redraw(client);
}


/**
 * @brief Unhide one transient family member
 *
 * @param member Family member to act on
 * @param ctx    Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_unhide_visit(client_td *member, void *ctx)
{
    (void) ctx;

    if (client_is_hidden(member) && !client_is_locked(member)) {
        s_ccmd_client_unhide_one(member);
    }
}


/* Unmap a client's decoration target, correctly pre-arming
 * 'ignore.unmap' first */
void ccmd_client_unmap_decorated(client_td *client,
        xcb_window_t target)
{
    if (client == NULL) {
        return;
    }

    client->ignore.unmap += 2u;
    if (client->titlebar != 0) {
        client->ignore.unmap += 1u;
        xcb_window_hide(client->titlebar);
    }
    xcb_window_hide(target);
}


/* Iconize the client, taking its whole transient family down with it */
void ccmd_client_iconify(client_td *client)
{
    client_td *top;

    /* Refused for a window this window manager has already told the
     * display it will not do this to, by leaving
     * '_NET_WM_ACTION_MINIMIZE' out of that window's
     * '_NET_WM_ALLOWED_ACTIONS' (see
     * 'ccmd_client_update_allowed_actions').
     *
     * A panel is what this keeps out in practice, and it is refused
     * here rather than at each of the several callers so that every
     * route in, the titlebar button, the key binding, the window menu,
     * the IPC command and the iconify-all action alike, obeys the same
     * rule. */
    if (client == NULL || client_is_locked(client) ||
            !client_is_iconifiable(client)) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    if (!client_is_iconified(top)) {
        s_ccmd_client_iconify_one(top);
    }

    ccmd_client_family_apply(top, s_ccmd_client_iconify_visit, NULL);
}


/* Hide the client (minimize, but not iconify), taking its whole
 * transient family down with it */
void ccmd_client_hide(client_td *client)
{
    client_td *top;

    if (client == NULL) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    if (s_client_hide_still_applies(top)) {
        s_ccmd_client_hide_one(top);
    }

    ccmd_client_family_apply(top, s_ccmd_client_hide_visit, NULL);
}


/* Show (unhide) the client, taking its whole transient family back
 * with it */
void ccmd_client_unhide(client_td *client)
{
    client_td *top;

    if (client == NULL) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    ccmd_client_family_apply(top, s_ccmd_client_unhide_visit, NULL);

    if (client_is_hidden(top)) {
        s_ccmd_client_unhide_one(top);
    }
}
