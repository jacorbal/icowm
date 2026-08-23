/**
 * @file cmds/client/visibility.c
 *
 * @brief Iconify, hide, and unhide actions over clients
 *
 * Split out of what used to be a single, flat @c cmds/client/basic.c.
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
 * @brief Iconize exactly this one client, ignoring any transient
 *        family it may belong to
 *
 * Split out of what used to be the whole of @a ccmd_client_iconify so
 * that function can redirect to, and cascade across, a transient
 * family (see its own doc comment) while still sharing this single
 * client's worth of ICCCM/EWMH bookkeeping with the top-level, family-
 * unaware call sites (@c handler/map.c's own initial-iconic handling
 * among them) that only ever operate on one already-resolved client
 * and have no family to cascade to in the first place.
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

    /* Remember the state this client is in right now (normal,
     * maximized in any of its three variants, or fullscreen) so
     * 'ccmd_client_restore' can later re-enter that exact state
     * instead of always landing back on plain normal: "A window
     * manager may implement [additional states] as proper substates
     * of NormalState and IconicState, or it may treat them as
     * independent flags, allowing e.g., a maximized window to be
     * iconified and to re-appear as maximized upon de-iconification"
     * (X Desktop Group, 2013, "Extended Window Manager Hints", v1.5,
     * §2.1.1).  Guarded against an already-iconified client calling
     * this again, which would otherwise overwrite the real remembered
     * state with 'CLIENT_STATE_ICONIFIED' itself. */
    if (client->properties.state != (uint16_t) CLIENT_STATE_ICONIFIED) {
        client->properties.pre_iconify_state = client->properties.state;
    }

    if (client_is_shaded(client)) {
        ccmd_client_unshade(client);
    }

    /* Un-fullscreen first, the same reasoning as unshading above: an
     * iconified client's geometry is meant to be restored to its
     * pre-iconify size later (see the 'client_geometry_save' call
     * just below), and while still fullscreen that size is the whole
     * screen, not the window's real one. */
    if (client_is_fullscreen(client)) {
        ccmd_client_unfullscreen(client);
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
    handled_atom = ccmd_intern_atom(client->connection,
            "_NET_WM_HANDLED_ICONS");
    handled_reply = xcb_get_property_reply(client->connection,
            xcb_get_property(client->connection, 0, client->parent_id,
                handled_atom, XCB_ATOM_CARDINAL, 0, 1), NULL);

    /* A client that both asks to be left out of the taskbar/cycle
     * list AND belongs to a transient family with some other,
     * further-up member to represent it (its own top parent differs
     * from itself) gets no icon box of its own here: that top parent
     * already gets one (see 'ccmd_client_iconify''s own cascading
     * doc comment), and a second, unselectable box for this same
     * family sitting right next to it would be pure visual clutter
     * with no purpose, exactly the "one icon per window instead of
     * one per group" clutter this guard exists to avoid.  Excluded
     * from this specifically when this client's own top parent is
     * itself (a standalone client with no family to fall back on):
     * skipping its only icon box there would leave it with no way
     * back at all, taskbar-excluded and icon-less both. */
    skip_icon_win = (handled_reply != NULL &&
            xcb_get_property_value_length(handled_reply) > 0) ||
        ((client->properties.flags & CLIENT_FLAG_SKIP_TASKBAR) != 0 &&
         ccmd_client_transient_top_parent(client) != client);

    free(handled_reply);

    /* Compute icon height once, shared by icon window creation
     * below and the '_NET_WM_ICON_GEOMETRY' property published
     * further down */
    icon_h_out = (uint16_t) (WM_ICON_SQUARE_SIZE +
            ((client->config->theme.icon.is_captioned)
             ? WM_ICON_CAPTION_HEIGHT
             : 0u));

    if (!skip_icon_win) {
        ccmd_client_ensure_icon_window(client, icon_h_out);
    }

    /* Account for the 'UnmapNotify' events that 'handler_unmap_notify'
     * must skip, the same reasoning as 'ccmd_client_hide''s own
     * identical comment: two events arrive for the unmapped target
     * ('SubstructureNotify' on parent + 'StructureNotify' on target)
     * and one additional event for the titlebar via the frame's
     * 'SubstructureNotify'.  If 'target' is the frame, the content
     * window is also unmapped explicitly below, producing two more
     * events for 'client->window'.  Without this, an iconified
     * decorated client's own content-window 'UnmapNotify' reaches
     * 'handler_unmap_notify' with 'ignore_unmap' still zero, which
     * that handler reads as the client withdrawing itself rather
     * than the window manager iconifying it. */
    ccmd_client_unmap_decorated(client, client->connection, target);
    if (target != client->window) {
        client->ignore.unmap += 2u;
        xcb_unmap_window(client->connection, client->window);
    }

    if (!skip_icon_win) {
        xcb_window_t tray_below;
        uint32_t wm_state_vals[2];
        uint32_t icon_geom[4];

        xcb_map_window(client->connection, client->icon_window);

        /* Icons are meant to sit even lower than the tray whenever it
         * is in the 'below' layer, "stuck to the desktop": stack just
         * below it explicitly, rather than via an unqualified 'below'
         * with no sibling, which would only put the icon under the
         * tray by coincidence of restack order rather than guarantee
         * it.  The other direction (the tray restacking after icons
         * already exist) is handled on the tray's own side; see
         * 'systray_layout_restack'. */
        tray_below = systray_below_window();
        if (tray_below != XCB_WINDOW_NONE) {
            xcb_configure_window(client->connection, client->icon_window,
                    XCB_CONFIG_WINDOW_SIBLING |
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const uint32_t[]) {
                    tray_below, XCB_STACK_MODE_BELOW
                    });
        } else {
            xcb_configure_window(client->connection, client->icon_window,
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const uint32_t[]) { XCB_STACK_MODE_BELOW });
        }
        client->is_icon_mapped = true;

        /* ICCCM §4.1.3: mark the WM icon window as Withdrawn so pagers
         * that scan window trees treat it as unmanaged */
        wm_state_atom = ccmd_intern_atom(client->connection, "WM_STATE");
        wm_state_vals[0] = CCMD_WM_STATE_WITHDRAWN;
        wm_state_vals[1] = XCB_NONE;

        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->icon_window, wm_state_atom, wm_state_atom,
                32, 2, wm_state_vals);

        /* EWMH: tell pagers and taskbars to skip the WM icon window */
        net_wm_state_atom = ccmd_intern_atom(client->connection,
                "_NET_WM_STATE");
        skip_atoms[0] = ccmd_intern_atom(client->connection,
                "_NET_WM_STATE_SKIP_PAGER");
        skip_atoms[1] = ccmd_intern_atom(client->connection,
                "_NET_WM_STATE_SKIP_TASKBAR");
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->icon_window, net_wm_state_atom,
                XCB_ATOM_ATOM, 32, 2, skip_atoms);

        /* EWMH §5.9: publish icon geometry on the client window so
         * taskbars can animate the iconify transition */
        icon_geom[0] = (uint32_t) client->icon_pos.x;
        icon_geom[1] = (uint32_t) client->icon_pos.y;
        icon_geom[2] = WM_ICON_SQUARE_SIZE;
        icon_geom[3] = icon_h_out;
        icon_geom_atom = ccmd_intern_atom(client->connection,
                "_NET_WM_ICON_GEOMETRY");
        xcb_change_property(client->connection, XCB_PROP_MODE_REPLACE,
                client->window, icon_geom_atom,
                XCB_ATOM_CARDINAL, 32, 4, icon_geom);
    }

    client_hide(client);
    client->properties.state = CLIENT_STATE_ICONIFIED;

    ccmd_set_wm_state(client, CCMD_WM_STATE_ICONIC,
            (skip_icon_win) ? XCB_NONE : client->icon_window);

    /* Iconify per EWMH: window hidden with '_NET_WM_STATE_HIDDEN'.
     * Icon display handled by pager/desktop */
    ccmd_client_sync_states(client);

    ccmd_client_focus_fallback(client);

    /* Ensure taskbars reflect the iconified state even when the client
     * was not the active one.  Function 'ccmd_client_focus_fallback'
     * only marks is_outdated when it changes focus, so a non-active
     * iconification would otherwise not trigger wm_ewmh_sync. */
    wm_request_client_redraw(client);

    xcb_flush(client->connection);
}


/**
 * @brief Hide exactly this one client (minimize, but not iconify),
 *        ignoring any transient family it may belong to
 *
 * Split out of what used to be the whole of @a ccmd_client_hide so
 * that function can redirect to, and cascade across, a transient
 * family (see its own doc comment).
 *
 * @param client Client to hide; must be non-null
 *
 * @note Complexity: @e O(1)
 */
static void s_ccmd_client_hide_one(client_td *client)
{
    xcb_window_t target;
    surface_td *surface;

    surface = wm_get_surface_by_id(client->screen_id);
    if (surface != NULL && surface->is_showing_desktop) {
        surface->is_showing_desktop = false;
    }

    target = ccmd_target_win(client);

    /* Account for the 'UnmapNotify' events that 'handler_unmap_notify'
     * must skip.  Two events arrive for the unmapped target
     * ('SubstructureNotify' on parent + 'StructureNotify' on target)
     * and one additional event for the titlebar via the frame's
     * 'SubstructureNotify'.  If 'target' is the frame, the content
     * window is also unmapped explicitly below, producing two more
     * events for 'client->window'. */
    ccmd_client_unmap_decorated(client, client->connection, target);
    if (target != client->window) {
        client->ignore.unmap += 2u;
        xcb_unmap_window(client->connection, client->window);
    }

    client_hide(client);

    ccmd_set_wm_state(client, CCMD_WM_STATE_ICONIC, XCB_NONE);
    ccmd_client_sync_states(client);

    ccmd_client_focus_fallback(client);
    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/**
 * @brief Show (unhide) exactly this one client, ignoring any
 *        transient family it may belong to
 *
 * Split out of what used to be the whole of @a ccmd_client_unhide so
 * that function can redirect to, and cascade across, a transient
 * family (see its own doc comment).
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
        xcb_map_window(client->connection, client->titlebar);
    }

    xcb_map_window(client->connection, target);

    if (target != client->window) {
        xcb_map_window(client->connection, client->window);
    }

    client_unhide(client);

    ccmd_set_wm_state(client, CCMD_WM_STATE_NORMAL, XCB_NONE);
    ccmd_client_sync_states(client);

    /* Raise the unhidden client to the top of the desktop stacking
     * order and give it real input focus, matching the deiconify
     * behavior, so that clicking a hidden window in the window menu
     * immediately activates it for keyboard input */
    if (client_is_focusable(client)) {
        desktop_td *const desktop = wm_get_client_desktop(client);
        if (desktop != NULL) {
            desktop->client_active_id = client->id;
            desktop->is_focus_dirty = true;
            (void) desktop_action_client_send_front(desktop, client);
            desktop->is_outdated = true;
        }
        ccmd_client_focus(client);
    }
    wm_request_client_redraw(client);
}


/**
 * @brief Unmap a client's decoration target, correctly pre-arming
 *        @c ignore.unmap first
 *
 * Every place in this project that unmaps a client window-manager-
 * side (iconifying, hiding for another desktop, sending it
 * elsewhere) shares this exact same two-step shape: increment
 * @c client->ignore.unmap by however many @c UnmapNotify events the
 * unmap below is about to generate, THEN issue the unmap itself, so
 * @a handler_unmap_notify (@c handler/map.c) correctly recognizes
 * this as a window-manager-initiated unmap rather than the client
 * withdrawing itself.  Two events always arrive for @p target itself
 * (its own @c StructureNotify plus its parent's own
 * @c SubstructureNotify); one further event arrives for the
 * titlebar, if present, via the frame's own @c SubstructureNotify.
 *
 * A caller whose own @p target can differ from @p client->window
 * (the frame, when decorated, rather than the bare content window)
 * and that also needs the content window itself unmapped separately
 * (@a s_ccmd_client_iconify_one and @a s_ccmd_client_hide_one above
 * are the only two such callers today) still has to account for,
 * and issue, that additional unmap on its own right after calling
 * this: two more events arrive for @c client->window in that case,
 * matching this same two-events-per-window rule, and this function
 * only knows about the one @p target it was actually given.
 *
 * @param client     Client being unmapped; its own @c ignore.unmap is
 *                    incremented here
 * @param connection Connection to issue the unmap requests on
 * @param target     Window to unmap (the frame if decorated, the
 *                    bare content window otherwise; see @a ccmd_
 *                    target_win, cmds/client/screen.c)
 *
 * @note A null @p client or @p connection is a silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_unmap_decorated(client_td *client,
        xcb_connection_t *connection, xcb_window_t target)
{
    if (client == NULL || connection == NULL) {
        return;
    }

    client->ignore.unmap += 2u;
    if (client->titlebar != 0) {
        client->ignore.unmap += 1u;
        xcb_unmap_window(connection, client->titlebar);
    }
    xcb_unmap_window(connection, target);
}


/**
 * @brief Iconize the client, taking its whole transient family down
 *        with it
 *
 * ICCCM §4.1.2.6 dialogs (a "save changes?" prompt, say) are meant to
 * live and die with the window they belong to, not persist as their
 * own independent, separately-iconified entity: iconizing any single
 * member of a transient family here redirects to, and iconizes, the
 * family's own top-most ancestor (@a ccmd_client_transient_top_parent)
 * first, then every other member of that same family still mapped and
 * not yet iconified, so the whole group vanishes into one grouped icon
 * together and comes back together too (see @a ccmd_client_restore's
 * own matching half of this).  A client with no transient relatives at
 * all is unaffected: its own top parent is itself, and no sibling scan
 * finds anything else to cascade to.
 *
 * @param client Window to iconify
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the top parent's own desktop
 */
void ccmd_client_iconify(client_td *client)
{
    client_td *top;
    size_t count;
    client_td **siblings;

    if (client == NULL || client_is_locked(client)) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    if (!client_is_iconified(top)) {
        s_ccmd_client_iconify_one(top);
    }

    siblings = ccmd_client_transient_family_snapshot_anywhere(top,
            &count);
    if (siblings != NULL) {
        for (size_t i = 0; i < count; i++) {
            if (!client_is_iconified(siblings[i]) &&
                    !client_is_locked(siblings[i])) {
                s_ccmd_client_iconify_one(siblings[i]);
            }
        }

        free(siblings);
    }
}


/**
 * @brief Hide the client (minimize, but not iconify), taking its
 *        whole transient family down with it
 *
 * A hidden client without a taskbar or window-list entry showing it
 * (unlike an iconified one, which always keeps its own icon) has no
 * way back if it is a transient dialog left behind on its own,
 * separately hidden: nothing on screen still points at it, and
 * nothing will ever unhide it again.  Hiding any single member of a
 * transient family here redirects to, and hides, the family's own
 * top-most ancestor (@a ccmd_client_transient_top_parent) first, then
 * every other member of that same family still visible, the same
 * cascade @a ccmd_client_iconify's own doc comment already covers in
 * full for iconifying, so the whole group disappears together and
 * comes back together too (see @a ccmd_client_unhide's own matching
 * half of this).  A client with no transient relatives at all is
 * unaffected: its own top parent is itself, and no sibling scan finds
 * anything else to cascade to.
 *
 * @param client Window to hide
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the top parent's own desktop
 */
void ccmd_client_hide(client_td *client)
{
    client_td *top;
    size_t count;
    client_td **siblings;

    if (client == NULL) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    if (!client_is_hidden(top)) {
        s_ccmd_client_hide_one(top);
    }

    siblings = ccmd_client_transient_family_snapshot_anywhere(top,
            &count);
    if (siblings != NULL) {
        for (size_t i = 0; i < count; i++) {
            if (!client_is_hidden(siblings[i]) &&
                    !client_is_locked(siblings[i])) {
                s_ccmd_client_hide_one(siblings[i]);
            }
        }

        free(siblings);
    }
}


/**
 * @brief Show (unhide) the client, taking its whole transient family
 *        back with it
 *
 * The matching half of @a ccmd_client_hide's own transient-family
 * cascade (see its own doc comment for the full reasoning): redirects
 * to the family's top-most ancestor, then unhides every other family
 * member still hidden, so a family hidden together comes back
 * together too.  Every other family member is unhidden before the
 * top parent's own unhide, not after, the same ordering @a ccmd_
 * client_restore already uses and for the same reason (see its own
 * doc comment, cmds/client/focus.c): the top parent's own focus-
 * granting step redirects through @a ccmd_client_focus_target to
 * whichever transient dialog should actually end up focused, which
 * only finds that dialog if it is already mapped by the time this
 * reaches that step.
 *
 * @param client Client to unhide
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the top parent's own desktop
 */
void ccmd_client_unhide(client_td *client)
{
    client_td *top;
    size_t count;
    client_td **siblings;

    if (client == NULL) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    siblings = ccmd_client_transient_family_snapshot_anywhere(top,
            &count);
    if (siblings != NULL) {
        for (size_t i = 0; i < count; i++) {
            if (client_is_hidden(siblings[i]) &&
                    !client_is_locked(siblings[i])) {
                s_ccmd_client_unhide_one(siblings[i]);
            }
        }

        free(siblings);
    }

    if (client_is_hidden(top)) {
        s_ccmd_client_unhide_one(top);
    }
}
