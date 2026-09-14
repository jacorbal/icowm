/**
 * @file handler/map.c
 *
 * @brief X @c MAP_REQUEST, @c UNMAP_NOTIFY, and @c DESTROY_NOTIFY event
 *        handlers
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

/* Utils includes */
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>

/* Default initial values */
#include <defs/desktop.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Rules includes */
#include <rules.h>

/* Policy includes */
#include <policy/focus.h>
#include <policy/placement/manual.h>
#include <policy/placement/window.h>

/* Input includes */
#include <input/mouse/cursor.h>
#include <input/mouse/drag.h>

/* Render includes */
#include <render/outdate.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Command includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/focus.h>
#include <cmds/client/layer.h>
#include <cmds/client/maximize.h>
#include <cmds/client/screen.h>
#include <cmds/client/state.h>
#include <cmds/client/flags.h>
#include <cmds/client/transient.h>
#include <cmds/client/visibility.h>

/* IPC includes */
#include <ipc.h>

/* Control includes */
#include <cctl/sn.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <memguard.h>
#include <scratchpad.h>
#include <surface.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>
#include <wm/shutdown.h>

/* Local includes */
#include <handler.h>


/**
 * @brief Read @c _NET_WM_STATE again, just before the window is shown
 *
 * @a client_init already read it when the window was adopted, but
 * a client that sets the property around that same moment can be
 * sampled before it gets there.  Nothing reads the property a second
 * time afterwards, so the state was lost for good.  The window came up
 * at its size while every later check still believed it fullscreen,
 * leaving something that could not be moved or resized and was not
 * fullscreen either.
 *
 * Only the two states this function's caller acts on are looked for.
 * The rest of @c _NET_WM_STATE was applied at adoption and does not
 * need doing twice.
 *
 * @param connection XCB connection
 * @param ewmh       EWMH connection
 * @param client     Client whose recorded initial state is refreshed
 *
 * @note Never clears what was already recorded
 * @note A client that set the state early and had it seen is not made
 *       to lose it by a reply that arrives without it
 * @note Complexity: @e O(n), where @e n is the number of atoms the
 *       property holds
 */
static void s_map_refresh_initial_state(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, client_td *client)
{
    xcb_get_property_reply_t *reply;
    xcb_atom_t atom_fullscreen;
    xcb_atom_t atom_max_horz;
    xcb_atom_t atom_max_vert;

    if (connection == NULL || ewmh == NULL || client == NULL) {
        return;
    }

    atom_fullscreen = atom_intern(connection,
            "_NET_WM_STATE_FULLSCREEN", true);
    atom_max_horz = atom_intern(connection,
            "_NET_WM_STATE_MAXIMIZED_HORZ", true);
    atom_max_vert = atom_intern(connection,
            "_NET_WM_STATE_MAXIMIZED_VERT", true);

    reply = xcb_get_property_reply(connection,
            xcb_ewmh_get_wm_state(ewmh, client->window), NULL);
    if (reply == NULL) {
        return;
    }

    if (reply->type == XCB_ATOM_ATOM && reply->format == 32) {
        const xcb_atom_t *atoms;
        uint32_t natoms;
        atoms = (const xcb_atom_t *) xcb_get_property_value(reply);
        natoms = (uint32_t) xcb_get_property_value_length(reply) /
            (uint32_t) sizeof(xcb_atom_t);

        for (uint32_t i = 0u; atoms != NULL && i < natoms; ++i) {
            if (atom_fullscreen != XCB_ATOM_NONE &&
                    atoms[i] == atom_fullscreen) {
                client->hints_ewmh.initial_state.is_fullscreen = true;
            } else if (atom_max_horz != XCB_ATOM_NONE &&
                    atoms[i] == atom_max_horz) {
                client->hints_ewmh.initial_state.is_maximized_horz =
                    true;
            } else if (atom_max_vert != XCB_ATOM_NONE &&
                    atoms[i] == atom_max_vert) {
                client->hints_ewmh.initial_state.is_maximized_vert =
                    true;
            }
        }
    }

    free(reply);
}


/**
 * @brief Finish taking a window under management, once it has a place
 *
 * Everything after the placement decision.  Mapping the frame and the
 * window itself, honouring an initial iconic state, giving focus if the
 * configuration wants new windows to have it, telling the client its
 * screen-relative geometry as ICCCM requires, and announcing the whole
 * thing over IPC.
 *
 * Held apart from the decision that precedes it so that a placement
 * which cannot answer at once has somewhere to hand the rest of the
 * work to.
 *
 * @param wm      Window manager instance
 * @param surface Surface the window is on
 * @param desktop Desktop it belongs to
 * @param client  Client being mapped
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop, from the focus it may grant
 */
static void s_map_finish(const wm_td *wm, surface_td *surface,
        desktop_td *desktop, client_td *client)
{
    cJSON *fields;
    bool want_iconic;
    bool want_fullscreen;
    bool want_maximized_horz;
    bool want_maximized_vert;

    /* ICCCM §4.1.2.4: honor 'WM_HINTS' 'initial_state' when
     * 'IconicState', unless a rule already gave an explicit
     * 'apply.iconified' for this client ('s_rules_defer_state_to_map',
     * in 'rules/apply.c'): the rule speaks for the user's own
     * configuration and so takes precedence over the client's own
     * request either way, the same as 'has_rule_position_locked'
     * already overrides a client-initiated move (client.h). */
    want_iconic = client->has_rule_iconified
        ? client->is_rule_iconified
        : client->hints_icccm.hints.is_initial_iconic;

    if (want_iconic) {
        ccmd_client_iconify(client);
    } else {
        if (client->titlebar != 0) {
            xcb_map_window(xcb_connection_get(), client->titlebar);
        }

        if (client->frame != 0) {
            xcb_map_window(xcb_connection_get(), client->frame);
            xcb_map_window(xcb_connection_get(), client->window);
        } else {
            xcb_map_window(xcb_connection_get(), client->window);
        }

        client_unhide(client);

        /* A client mapping while everything is closing is almost always
         * an application asking whether to save.  Its parent was
         * already brought over by 'wm_shutdown_begin', so this only has
         * to cover the dialog itself, which was placed centered over
         * that parent before any of this ran and could still belong to
         * another desktop or page of its own. */
        if (wm_shutdown_is_in_progress()) {
            wm_shutdown_gather_client(client);
        }

        if (wm_config(wm)->base.windows.focus.focus_new &&
                client_is_focusable(client)) {
            focus_apply(wm_surfaces(wm), surface, desktop, client, true,
                    wm_config(wm));
        }

        /* ICCCM §4.2.3: after both 'place_window_apply' and
         * 'rules_apply' have settled the final frame position, send
         * a synthetic 'ConfigureNotify' with screen-relative
         * coordinates to the client so it knows its true screen
         * position from the outset.
         *
         * For decorated clients the only 'ConfigureNotify' received so
         * far was generated by 'client_decoration_layout_sync' (called
         * inside 'ci_create_decorations'), which carries frame-relative
         * coordinates (x=border, y=titlebar+border) rather than the
         * actual on-screen position, causing misaligned popups and
         * imperfect initial viewport layout.
         *
         * For undecorated clients the X server supplies the correct
         * screen-relative coordinates, but some applications require an
         * explicit 'ConfigureNotify' to commit their initial layout;
         * without it a fragment of the window content can appear
         * outside the configured frame bounds until a focus change or
         * move forces a redraw. */
        client_send_synthetic_configure_notify(xcb_connection_get(),
                client);
        xcb_clear_area(xcb_connection_get(), 1, client->window,
                0, 0, 0, 0);

        /* EWMH's correct way for a client to request fullscreen from
         * the outset (see 'hints_ewmh.initial_state''s doc comment,
         * client.h) rather than waiting for a 'ClientMessage' after
         * mapping.  Deliberately last in this whole block, after the
         * synthetic 'ConfigureNotify' just above:
         * 'ccmd_client_fullscreen' sends its with the true fullscreen
         * geometry, and sending the ordinary one afterward would tell
         * the client its old, pre-fullscreen position and size right
         * after telling it the correct one. */
        /* Re-read rather than trusting what 'client_init' saw.  That
         * read happens when the window is first adopted, and a client
         * that sets '_NET_WM_STATE' around that moment could be sampled
         * before it got there: the state was then lost for good, since
         * nothing reads the property again, and the window came up at
         * its small size while every later check still believed it was
         * fullscreen.  Asking again here, with the window about to be
         * shown, makes the outcome the same whichever order the two
         * happened in. */
        s_map_refresh_initial_state(xcb_connection_get(),
                xcb_ewmh_connection_get(), client);

        /* A rule's own 'apply.fullscreen'/'apply.maximized' (deferred
         * here by 's_rules_defer_state_to_map', for the same
         * not-mapped-yet reason 'want_iconic' above already gives)
         * takes precedence over the client's own EWMH hint for that
         * same field, one boolean overriding both maximize axes at once
         * since 'apply.maximized' itself makes no distinction between
         * them (see 'struct rules_apply_s', rules/internal.h); a client
         * whose hint disagrees loses,
         * same as its 'WM_HINTS' iconic request already can. */
        want_fullscreen = client->has_rule_fullscreen
            ? client->is_rule_fullscreen
            : client->hints_ewmh.initial_state.is_fullscreen;
        want_maximized_horz = client->has_rule_maximized
            ? client->is_rule_maximized
            : client->hints_ewmh.initial_state.is_maximized_horz;
        want_maximized_vert = client->has_rule_maximized
            ? client->is_rule_maximized
            : client->hints_ewmh.initial_state.is_maximized_vert;

        if (want_fullscreen) {
            ccmd_client_fullscreen(client);
        } else if (want_maximized_horz && want_maximized_vert) {
            /* Same reasoning as 'is_fullscreen' just above, for the
             * same EWMH pre-existing-state mechanism applied to
             * 'is_maximized_horz'/'_vert' (client.h) instead; a client
             * requesting both at once is maximized on both axes
             * together, one call, rather than two in sequence each
             * sending its synthetic 'ConfigureNotify' for an
             * intermediate, single-axis geometry the client never
             * actually asked for. */
            ccmd_client_maximize(client);
        } else if (want_maximized_horz) {
            ccmd_client_maximize_horz(client);
        } else if (want_maximized_vert) {
            ccmd_client_maximize_vert(client);
        }

        /* 'apply.shaded'/'apply.hidden' have no EWMH hint counterpart
         * to fall back to, so, unlike the pair just above, these only
         * ever run at all when a rule actually gave one.  Shaded shares
         * the same conflict a rule can hit at 'RULES_TRIGGER_PROPERTY'
         * time in 's_rules_apply_state' ('rules/apply.c', whose comment
         * covers the reasoning this mirrors): undecorated or fullscreen
         * both leave it a silent no-op inside 'ccmd_client_shade'
         * itself, worth a warning here for the same reason it is worth
         * one there.  Iconified taking precedence over shaded needs no
         * such warning here, unlike there, because 'want_iconic' above
         * already sent an iconified client down the other branch of
         * this whole function before either could ever be reached. */
        if (client->has_rule_shaded) {
            if (!client->is_rule_shaded) {
                ccmd_client_unshade(client);
            } else if (!client_is_decorated(client)) {
                LOGGER_WARNING("Rule requests shaded but client is" \
                        " not decorated; shaded ignored", L_NARG);
            } else if (client_is_fullscreen(client)) {
                LOGGER_WARNING("Rule requests shaded while" \
                        " fullscreen is also requested; fullscreen" \
                        " takes precedence, shaded ignored", L_NARG);
            } else {
                ccmd_client_shade(client);
            }
        }

        if (client->has_rule_hidden) {
            if (client->is_rule_hidden) {
                ccmd_client_hide(client);
            } else {
                ccmd_client_unhide(client);
            }
        }
    }

    /* Re-apply layer stacking so newly mapped windows do not obscure
     * clients already assigned to the above layer */
    ccmd_desktop_enforce_layers(desktop);

    wm_outdate_surface(surface);
    wm_outdate_desktop(desktop);

    LOGGER_DEBUG("Mapped and adopted window %#x ('%s') on desktop %u",
            client->window, client->info.name, desktop->id);

    fields = cJSON_CreateObject();
    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "client_id",
                (double) client->id);
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) desktop->id);
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) surface->id);
    }
    ipc_broadcast_event(IPC_EVENT_WINDOW_MAPPED, fields);
}


/**
 * @brief Map a window without adopting it under window manager control
 *
 * Shared by every early-return path in @c handler_map_request below
 * that declines to manage the window (an unresolvable surface or
 * current desktop, @c client_init itself failing, or the client failing
 * to be added to its desktop).  The requesting application gets its
 * window on screen either way, just without a frame or any
 * window-manager tracking.
 *
 * @param connection XCB connection
 * @param window     Window to map as-is
 *
 * @note Complexity: @e O(1)
 */
static void s_map_unmanaged(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_map_window(connection, window);
}


/* Handle a 'MAP_REQUEST' event */
void handler_map_request(const wm_td *wm,
        xcb_map_request_event_t *event)
{
    surface_td *surface;
    desktop_td *desktop;
    desktop_td *origin_desktop;
    client_td *client;
    uint32_t max_clients;
    uint32_t origin_desktop_id;
    xcb_connection_t *connection = wm_connection(wm);
    xcb_ewmh_connection_t *ewmh = wm_ewmh(wm);
    const config_td *config = wm_config(wm);
    list_td *surfaces = wm_surfaces(wm);

    if (wm == NULL || event == NULL) {
        LOGGER_ERROR("Received null pointer in map request handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Map request event (window=0x%x, parent=0x%x)",
            event->window, event->parent);

    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client != NULL) {
        LOGGER_TRACE("Window %#x already managed; re-showing it" \
                " properly rather than mapping it raw", event->window);

        /* A bare 'xcb_map_window' here, this path's previous behavior,
         * left the client's own bookkeeping (still marked hidden, its
         * frame and titlebar left hidden from whichever hide put it
         * there, WM_STATE still whatever it was) entirely stale,
         * correct only for the window itself becoming visible again,
         * not for anything this window manager still believed about it.
         * Harmless, not only correct, when this client was never
         * actually hidden at all, since every one of
         * 'ccmd_client_unhide' own steps is already a no-op against
         * a client that is not. */
        ccmd_client_unhide(client);
        return;
    }

    /* A docked systray icon is not a managed client, so it would
     * otherwise fall through to the generic top-level adoption path
     * below and end up managed as a brand-new decorated client instead
     * of staying a plain docked icon; see
     * 'systray_icon_map_request'. */
    if (systray_icon_map_request(event->window)) {
        return;
    }

    surface = lookup_surface_for_root(surfaces, event->parent);
    if (surface == NULL && !list_is_empty(surfaces)) {
        surface = (surface_td *) list_data(list_head(surfaces));
    }
    if (surface == NULL) {
        LOGGER_ERROR("No surface found for 'MAP_REQUEST' on root %#x",
                event->parent);
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        LOGGER_ERROR("No current desktop on surface %u; mapping without"
                " management", surface->id);

        s_map_unmanaged(connection, event->window);
        return;
    }

    /* A window whose own '_NET_STARTUP_ID' still names a pending launch
     * sequence gets placed on the desktop that launch was requested
     * from, rather than whichever desktop merely happens to be current
     * by the time it finally maps; a slow-starting application would
     * otherwise land wherever the user has since switched to, which is
     * rarely where they meant to open it.  Any window without that
     * property, or whose sequence has already expired or completed,
     * keeps today's behavior unchanged. */
    if (cctl_sn_desktop_for_window(connection, event->window,
                &origin_desktop_id)) {
        origin_desktop = surface_desktop_get(surface, origin_desktop_id);
        if (origin_desktop != NULL) {
            desktop = origin_desktop;
        }
    }

    /* A fallback completion path for a window whose own application
     * never broadcasts a ("remove:" message) of its own (xterm and most
     * other classic X11 applications, unlike most GTK and Qt ones):
     * matched by '_NET_WM_PID' instead, which is independent of the
     * '_NET_STARTUP_ID' placement lookup just above and of whether that
     * one found anything at all, so it runs unconditionally.  See
     * 'cctl_sn_complete_for_pid''s own comment ('cctl/sn.h') for the
     * full reasoning. */
    (void) cctl_sn_complete_for_pid(connection, surfaces, event->window);

    /* Checked before 'client_init' does any of its own (comparatively
     * expensive) setup work, so a client refused here never pays for
     * work that would just be thrown away.  Deliberately left unmapped,
     * unlike every other early-return path in this function that
     * declines to manage a window: an unmanaged-but-mapped window is
     * genuinely broken, not merely undecorated, since it has no frame,
     * is not tracked in any client list, and cannot be moved or closed
     * through IcoWM at all; if it somehow ends up with keyboard focus
     * regardless (a real, mapped top-level window can still receive it,
     * even one IcoWM never decided to manage), later code that assumes
     * "whatever currently has focus is a tracked client" has nothing
     * valid to find, which is exactly the abrupt behavior this comment
     * warns against.  Simply never mapping the window instead means the
     * requesting application is left waiting for a MapNotify that will
     * not come, rather than being handed a window it cannot use through
     * the one channel (the window manager) applications normally rely
     * on for that.  'memguard_max_clients' already reads 0 as
     * "restricted-memory mode is off, no cap", so nothing else needs to
     * check that separately here. */
    max_clients = memguard_max_clients();

    if (max_clients > 0u &&
            ohtbl_size(desktop->clients) >= max_clients) {
        memguard_warn_client_cap(connection, surface,
                config);
        return;
    }

    client = client_init(connection, ewmh, event->window, config);
    if (client == NULL) {
        s_map_unmanaged(connection, event->window);
        return;
    }

    client->screen_id = surface->id;
    client->desktop_id = desktop->id;

    if (desktop_action_client_add(desktop, client) != 0) {
        LOGGER_ERROR("Failed to add client %#x to desktop %u",
                event->window, desktop->id);
        client->window = 0;
        client_destroy(client);
        s_map_unmanaged(connection, event->window);
        return;
    }

    /* Links 'client' into its parent's transient tree, if
     * 'transient_for' names an already-managed client, right after
     * 'client' itself is a genuine managed client (added to its desktop
     * just above): every other family-wide function in this project
     * (top-parent walks, focus redirection, iconify/restore/
     * pin/etc. cascades) relies on this link already being in place to
     * walk real pointers instead of scanning every client on every
     * desktop. */
    client_link_transient(client);

    /* A window opened by a pinned one is pinned with it.  Pinning in
     * this window manager is a family-wide operation: 'ccmd_client_pin'
     * redirects to the family's top-most ancestor and cascades from
     * there, so every member is pinned together.  A member that only
     * appears afterwards, a dialog its parent opens later, was the one
     * case left out, and it stayed behind on the desktop it was born on
     * while its parent travelled: a modal one left that parent
     * unresponsive for a reason not visible anywhere.
     *
     * Asked of the top parent rather than the immediate one, since that
     * is what pinning itself acts on, and only when it really is
     * pinned.  An ordinary window's dialogs stay with it on its own
     * desktop exactly as before. */
    if (!client_is_pinned(client)) {
        const client_td *const top =
            ccmd_client_transient_top_parent(client);

        if (top != NULL && top != client && client_is_pinned(top)) {
            ccmd_client_pin(client);
        }
    }

    /* A window opened by a sticky one is stuck with it, the exact same
     * reasoning as pinning just above and for the same reason:
     * 'ccmd_client_stick' is a family-wide operation too, so every
     * member already in the family is stuck together, but one that only
     * appears afterwards was left out, free to be carried away from its
     * own parent's fixed screen position the moment the
     * viewport next pans. */
    if (!client_is_sticky(client)) {
        const client_td *const top =
            ccmd_client_transient_top_parent(client);

        if (top != NULL && top != client && client_is_sticky(top)) {
            ccmd_client_stick(client);
        }
    }

    scratchpad_position(client, desktop, surface);

    /* Advertise the desktop this client belongs to per EWMH */
    if (ewmh != NULL) {
        uint32_t did = (client->properties.flags & CLIENT_FLAG_PIN)
            ? WM_DESKTOP_ID_ALL : desktop->id;

        xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                client->window, ewmh->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &did);
    }

    /* Refresh work area in case the new client declares struts */
    surface_refresh_workareas(surface);

    /* Apply map-time rules before placement so explicit rule geometry
     * can lock the client position and exempt it from policy
     * placement */
    if (rules_apply(wm, client, &surface, &desktop, RULES_TRIGGER_MAP)) {
        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
    }

    /* Dock and panel windows position themselves, so their geometry is
     * never overridden */
    if (client->properties.type != (uint16_t) CLIENT_TYPE_DOCK &&
            !client->has_rule_position_locked) {
        place_window_apply(wm, surface, client);

        /* 'place_window_apply' just above marked this client if the
         * manual policy really did apply to it, so nothing here has to
         * work out again whether it did.  A window that asked for
         * a position itself, a dialog centered over its parent, or one
         * clustered next to a sibling never reaches that policy and is
         * never marked.  Taking the client leaves it unmapped and hands
         * 's_map_finish' over to be called once someone points at where
         * it goes, so this function must stop here rather than finish
         * the map itself.
         *
         * Never asked about a window that is going to come up as an
         * icon anyway: 's_map_finish' iconifies it instead of showing
         * it, so pointing at a position for it would settle nothing
         * anyone can see. */
        if (!client->hints_icccm.hints.is_initial_iconic &&
                place_manual_enqueue(connection, wm, surface, desktop,
                        client, mouse_cursor_move(), s_map_finish)) {
            /* Not mapping it here is not enough to keep it off the
             * screen.  A client sits in its desktop's list from the
             * moment it is adopted, and the render pass shows every one
             * not marked hidden, so the next pass would put this one up
             * while it is still being asked about.  Paired with the
             * 'client_unhide' that 's_map_finish' does above, which
             * takes the mark off once it is settled. */
            client_hide(client);
            return;
        }
    }
    s_map_finish(wm, surface, desktop, client);
}


/* Handle an 'UNMAP_NOTIFY' event */
void handler_unmap_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_unmap_notify_event_t *event)
{
    client_td *client;
    desktop_td *desktop;
    surface_td *surface;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in unmap handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Unmap notify event (window=0x%x)", event->window);

    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);
    if (client != NULL) {
        if (event->window != client->window) {
            if (client->ignore.unmap > 0) {
                client->ignore.unmap--;
            }
            return;
        }

        if (client->ignore.unmap > 0) {
            client->ignore.unmap--;
            return;
        }

        /* Marked hidden before the fallback call just below, not after:
         * 'ccmd_client_focus' (called from inside
         * 'client_focus_fallback') redirects to whichever mapped
         * transient descendant of the new target should actually
         * receive focus in its place (see 'ccmd_client_focus_target''s
         * comment, 'cmds/client/internal.h'), and that redirect walk
         * excludes a 'CLIENT_FLAG_HIDDEN' candidate specifically so
         * a fallback landing back on 'client''s parent does not find
         * this same withdrawing 'client' here
         * and send real input focus right back onto it. */
        client_hide(client);

        /* Distinct from the plain 'client_hide' above: this client
         * unmapped its own window itself, ICCCM §4.1.4's own account of
         * a client withdrawing, not a window manager or user action
         * hiding it, so it must not be offered back to either automatic
         * discovery (see 'ccmd_client_bring_family',
         * cmds/client/transient.c) or the cycle menu's own listing
         * ('menu/cycle.c') the way a client genuinely hidden by a
         * command is. */
        client_mark_withdrawn(client);

        if (desktop != NULL &&
                desktop->client_active_id == client->id) {
            client_focus_fallback(desktop, surface, client);
        }

        /* When a managed client withdraws itself (for example, to
         * a system tray), unmap every WM-created decoration so later
         * render passes never remap the ghost frame */
        if (client->frame != 0) {
            client->ignore.unmap++;
            xcb_window_hide(client->frame);
        }
        if (client->titlebar != 0) {
            client->ignore.unmap++;
            xcb_window_hide(client->titlebar);
        }
        /* 'properties.state' itself, not just the published EWMH
         * property, must also stop claiming fullscreen here.
         * A fullscreen client that withdraws itself this way previously
         * had only its '_NET_WM_STATE_FULLSCREEN' atom stripped from
         * the property below, with nothing here ever touching
         * 'properties.state' itself, silently leaving the two
         * disagreeing with each other from then on.  Reset to plain
         * normal specifically, not iconified: 'client_hide' just above
         * already marks 'CLIENT_FLAG_HIDDEN', which alone is enough for
         * 'ccmd_client_sync_states' ('cmds/client/ewmh.c') to correctly
         * still publish '_NET_WM_STATE_HIDDEN' below; claiming
         * 'CLIENT_STATE_ICONIFIED' here instead would make
         * 'client_is_iconified' true for a client the window manager
         * itself never actually iconified, with consequences well
         * beyond this one property (the whole transient-family
         * iconify/restore cascade among them). */
        client->properties.state = CLIENT_STATE_NORMAL;

        /* ICCCM §4.1.4: a client unmapping its own window withdraws it,
         * which §4.1.3.1 requires 'WM_STATE' to reflect as
         * 'WithdrawnState', not 'IconicState'.  The latter is only for
         * a window the window manager itself has iconified, still under
         * its management and eligible to be restored with no more than
         * a map request; a client that unmapped itself may never be
         * mapped again at all, and is not being tracked as an icon here
         * ('client_hide' above sets 'CLIENT_FLAG_HIDDEN', a distinct
         * concept from iconified, as the comment right above already
         * explains) */
        ccmd_set_wm_state(client, CCMD_WM_STATE_WITHDRAWN, XCB_NONE);
        ccmd_client_sync_states(client);
        /* 'wm_outdate_client'/'_surface'/'_desktop' directly, not
         * 'wm_request_client_redraw': 'surface' and 'desktop' are
         * already resolved locally above (from the same
         * 'lookup_find_client' call this whole handler already made),
         * so calling the convenience wrapper here would only re-derive
         * both through a redundant lookup, an O(n) scan over the global
         * 'wm->surfaces' for 'surface' alone, for values already
         * sitting in scope. */
        wm_outdate_client(client);
        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);
    }
}


/* Handle a 'DESTROY_NOTIFY' event */
void handler_destroy_notify(wm_td *wm, xcb_connection_t *connection,
        list_td *surfaces, xcb_destroy_notify_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    cJSON *fields;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in destroy handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Destroy notify event (window=0x%x)", event->window);

    /* Before the managed-client lookup below, and unconditionally.
     * A docked systray icon is never a managed client at all, so the
     * early return that lookup takes for an unmanaged window would
     * otherwise leave the destroyed icon in the tray's array forever */
    systray_handle_destroy(wm, event->window);

    client = lookup_find_client(surfaces, event->window,
            &surface, &desktop);
    if (client == NULL) {
        return;
    }

    if (event->window != client->window) {
        return;
    }

    /* Cancel any drag that was using this client */
    if (drag_is_active() && drag_client() == client) {
        drag_cancel(connection, client);
    }

    /* Drop it from the manual-placement queue too, for the same reason.
     * A window destroyed while it was being pointed at, or while
     * waiting its turn, still holds a place in that queue, and the
     * pointer and the keyboard with it if it was the one being asked
     * about. */
    place_manual_cancel_client(connection, client);

    if (desktop != NULL) {
        desktop_action_client_rem(desktop, client);
        /* Refresh work area in case the removed client had struts */
        surface_refresh_workareas(surface);
    }

    /* Falls back AFTER 'client' is already removed from 'desktop', not
     * before: 'ccmd_client_focus' (called from inside this), itself
     * redirects to whichever mapped transient descendant of the new
     * target should actually receive focus in its place (see
     * 'ccmd_client_focus_target''s comment, 'cmds/client/internal.h').
     * With 'client' (the very dialog now closing) still sitting in
     * 'desktop->clients' at the time of that redirect, a fallback
     * landing back on its own parent would find this closing dialog
     * itself as a still-valid-looking transient child, and redirect
     * real input focus right back onto a window about to be destroyed
     * a few lines below, rather than onto the parent the fallback just
     * chose. */
    if (desktop != NULL && desktop->client_active_id == client->id) {
        client_focus_fallback(desktop, surface, client);
    }

    /* ICCCM withdrawn state: remove WM_STATE on unmanage */
    ccmd_clear_wm_state(client);

    /* When the frame is destroyed the X server also destroys all its
     * children ('client->window', 'client->titlebar').  Zero them all
     * out so 'client_destroy' does not issue redundant
     * 'xcb_destroy_window' calls. */
    if (event->window == client->frame) {
        client->frame = 0;
        client->titlebar = 0;
        client->window = 0;
    } else {
        /* The content window was destroyed (e.g., the client process
         * exited or the app closed without a prior 'UnmapNotify').
         * Immediately destroy the WM-created frame (which takes its
         * titlebar child with it) so no ghost frame is left on screen.
         * Increment 'ignore.unmap' so the 'UnmapNotify' the X server
         * generates for the mapped frame is swallowed and does not
         * re-enter the unmap handler.  Zero both pointers to prevent
         * 'client_destroy' from issuing redundant destroy calls. */
        if (connection != NULL && client->frame != 0) {
            client->ignore.unmap++;
            xcb_window_destroy(client->frame);
        }
        client->frame = 0;
        client->titlebar = 0;
        client->window = 0;
    }

    fields = cJSON_CreateObject();
    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "client_id",
                (double) client->id);
        if (desktop != NULL) {
            cJSON_AddNumberToObject(fields, "desktop_id",
                    (double) desktop->id);
        }
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) surface->id);
    }
    ipc_broadcast_event(IPC_EVENT_WINDOW_CLOSED, fields);

    client_destroy(client);

    wm_outdate_surface(surface);
    wm_outdate_desktop(desktop);

    LOGGER_DEBUG("Removed destroyed window %#x", event->window);
}


/* Handle a 'MAP_NOTIFY' event */
void handler_map_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_map_notify_event_t *event)
{
    client_td *client;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in map notify handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Map notify event (window=0x%x," \
            " override-redirect=%u)",
            event->window, event->override_redirect);

    /* Unmanaged override-redirect windows (tooltips, menus) are
     * intentionally ignored; only managed clients need a EWMH resync */
    if (event->override_redirect != 0) {
        return;
    }

    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client != NULL) {
        wm_request_client_redraw(client);

        /* ICCCM §4.2.3: re-send the synthetic 'ConfigureNotify' at
         * 'MapNotify' time so that some programs have the correct
         * screen-relative position in their event queue before their
         * first 'Expose'-driven draw.  The primary fix for the
         * initial-open misalignment is in 'desktop_render_clients',
         * which sends a synthetic 'ConfigureNotify' after every
         * inner-window configure to ensure the screen-relative one is
         * always the last event the client receives. */
        if (event->window == client->window &&
                client->frame != 0 &&
                client_is_decorated(client)) {
            client_send_synthetic_configure_notify(connection, client);
        }

        /* Force the content window to repaint immediately after it
         * becomes visible.  Applications like gVim do not repaint on
         * 'ConfigureNotify' or 'MapNotify' alone; without an 'Expose'
         * event the lower portion of the window may remain blank until
         * the user triggers a focus change or another redraw action.
         * Using 'exposures=1' causes the X server to generate an
         * 'Expose' event so the application redraws the full client
         * area from the outset. */
        if (event->window == client->window) {
            xcb_clear_area(connection, 1, client->window, 0, 0, 0, 0);

            /* Re-assert the plain-pointer cursor 'client_init' already
             * set once on this same window (see client.c).  Many
             * GTK/GDK applications explicitly set their top-level
             * window's cursor as part of their own realization, which
             * can run after (and so silently overwrite) that first
             * assignment; MapNotify, confirming the window has actually
             * become visible, is reliably later than that realization,
             * so setting it again here wins whatever race existed. */
            xcb_change_window_attributes(connection, client->window,
                    XCB_CW_CURSOR,
                    (const uint32_t[]) { mouse_plain_cursor() });
            LOGGER_TRACE("Set cursor (window=0x%x, cursor=0x%x)",
                    client->window, mouse_plain_cursor());
        }
    }
}


/* Handle a 'GRAVITY_NOTIFY' event */
void handler_gravity_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_gravity_notify_event_t *event)
{
    client_td *client;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in gravity notify handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Gravity notify event (window=0x%x, pos=%+d%+d)",
            event->window, event->x, event->y);

    /* The X server repositioned a frame window ('event->window') within
     * root because the screen was resized and the client's win_gravity
     * ('client->layout.gravity') placed it at a non-NW anchor.  Update
     * the cached frame position and re-sync decorations. */
    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client != NULL) {
        client->layout.geometry.cur.pos.x = event->x;
        client->layout.geometry.cur.pos.y = event->y;
        client_decoration_layout_sync(client);
        wm_request_client_redraw(client);

        /* ICCCM §4.2.3: the frame moved, so the client's
         * screen-relative position changed; notify it with correct
         * coordinates */
        if (client->frame != 0 && client_is_decorated(client)) {
            client_send_synthetic_configure_notify(connection, client);
        }
    }
}


/* Handle a 'CIRCULATE_NOTIFY' event */
void handler_circulate_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_circulate_notify_event_t *event)
{
    surface_td *surface;

    (void) connection;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in circulate notify",
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Circulate notify event (window=0x%x, place=%u)",
            event->window, event->place);

    /* The stacking order changed; mark the surface so 'wm_ewmh_sync'
     * updates '_NET_CLIENT_LIST_STACKING' on the next iteration */
    surface = lookup_surface_for_root(surfaces, event->event);
    wm_outdate_surface(surface);
}


/* Handle a 'CIRCULATE_REQUEST' event (ICCCM §4.1.7) */
void handler_circulate_request(xcb_connection_t *connection,
        list_td *surfaces, xcb_circulate_request_event_t *event)
{
    client_td *client;
    desktop_td *desktop;
    xcb_window_t target;
    uint32_t stack_mode;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in circulate request" \
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Circulate request event (window=0x%x, place=%u)",
            event->window, event->place);

    client = lookup_find_client(surfaces, event->window,
            NULL, &desktop);
    if (client == NULL) {
        return;
    }

    target = ccmd_target_win(client);
    stack_mode = (event->place == XCB_PLACE_ON_TOP)
        ? (uint32_t) XCB_STACK_MODE_ABOVE
        : (uint32_t) XCB_STACK_MODE_BELOW;

    xcb_configure_window(connection, target,
            XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);

    /* A raw 'CirculateRequest' is honored above exactly as asked, the
     * same way 'handler_configure_request' honors a plain
     * 'ConfigureRequest' stack-mode change, but without this call
     * afterward that alone would let a normal-layer client circulate
     * itself above an 'above'-layer one, or a 'below'-layer one above
     * a normal client, since nothing else here re-imposes the layer
     * ordering 'ccmd_client_layer_above' and 'ccmd_client_layer_below'
     * otherwise guarantee */
    ccmd_desktop_enforce_layers(desktop);

    wm_request_client_redraw(client);
}
