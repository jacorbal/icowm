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

/* Default initial values */
#include <defs/desktop.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Rules includes */
#include <rules.h>

/* Policy includes */
#include <policy/focus.h>
#include <policy/placement/window.h>

/* Input includes */
#include <input/mouse/cursor.h>
#include <input/mouse/drag.h>

/* Render includes */
#include <render/outdate.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <memguard.h>
#include <scratchpad.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>

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

/* Project includes */
#include <lookup.h>
#include <utils/xcb/atom.h>
#include <surface.h>

/* Local includes */
#include <handler.h>


/**
 * @brief Read @c _NET_WM_STATE again, just before the window is shown
 *
 * @a client_init already read it when the window was adopted, but a
 * client that sets the property around that same moment can be sampled
 * before it gets there.  Nothing reads the property a second time
 * afterwards, so the state was lost for good: the window came up at
 * its own size while every later check still believed it fullscreen,
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
 * @note Never clears what was already recorded: a client that set the
 *       state early and had it seen is not made to lose it by a reply
 *       that arrives without it
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
    const xcb_atom_t *atoms;
    uint32_t natoms;

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
 * @brief Map a window without adopting it under window manager control
 *
 * Shared by every early-return path in @c handler_map_request below
 * that declines to manage the window (an unresolvable surface or
 * current desktop, @c client_init itself failing, or the client
 * failing to be added to its desktop): the requesting application
 * gets its window on screen either way, just without a frame or any
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
    xcb_flush(connection);
}


/* Handle a 'MAP_REQUEST' event */
void handler_map_request(const wm_td *wm,
        xcb_map_request_event_t *event)
{
    surface_td *surface;
    desktop_td *desktop;
    client_td *client;
    uint32_t max_clients;
    cJSON *fields;
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

    if (lookup_find_client(surfaces,
                event->window, NULL, NULL) != NULL) {
        LOGGER_TRACE("Window %#x already managed; mapping directly",
                event->window);

        s_map_unmanaged(connection, event->window);
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

    /* Checked before 'client_init' does any of its own (comparatively
     * expensive) setup work, so a client refused here never pays for
     * work that would just be thrown away.  Deliberately left
     * unmapped, unlike every other early-return path in this function
     * that declines to manage a window: an unmanaged-but-mapped
     * window is genuinely broken, not merely undecorated, since it
     * has no frame, is not tracked in any client list, and cannot be
     * moved or closed through IcoWM at all; if it somehow ends up
     * with keyboard focus regardless (a real, mapped top-level window
     * can still receive it, even one IcoWM never decided to manage),
     * later code that assumes "whatever currently has focus is a
     * tracked client" has nothing valid to find, which is exactly
     * what produced the abrupt, broken behavior this comment used to
     * defend against creating in the first place.  Simply never
     * mapping the window instead means the requesting application is
     * left waiting for a MapNotify that will not come, rather than
     * being handed a window it cannot use through the one channel
     * (the window manager) applications normally rely on for that.
     * 'memguard_max_clients' already reads 0 as "restricted-memory
     * mode is off, no cap", so nothing else needs to check that
     * separately here. */
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

    /* Links 'client' into its own parent's transient tree, if
     * 'transient_for' names an already-managed client, right after
     * 'client' itself is a genuine managed client (added to its own
     * desktop just above): every other family-wide function in this
     * project (top-parent walks, focus redirection, iconify/restore/
     * pin/etc. cascades) relies on this link already being in place
     * to walk real pointers instead of scanning every client on every
     * desktop. */
    client_link_transient(client);

    /* A window opened by a pinned one is pinned with it.  Pinning in
     * this window manager is a family-wide operation: 'ccmd_client_pin'
     * redirects to the family's top-most ancestor and cascades from
     * there, so every member is pinned together.  A member that only
     * appears afterwards, a dialog its parent opens later, was the one
     * case left out, and it stayed behind on the desktop it was born
     * on while its parent travelled: a modal one left that parent
     * unresponsive for a reason not visible anywhere.
     *
     * Asked of the top parent rather than the immediate one, since
     * that is what pinning itself acts on, and only when it really is
     * pinned: an ordinary window's dialogs stay with it on its own
     * desktop exactly as before. */
    if (!client_is_pinned(client)) {
        const client_td *const top =
            ccmd_client_transient_top_parent(client);

        if (top != NULL && top != client && client_is_pinned(top)) {
            ccmd_client_pin(client);
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

    /* Dock and panel windows position themselves, so their
     * geometry is never overridden */
    if (client->properties.type != (uint16_t) CLIENT_TYPE_DOCK &&
            !client->has_rule_position_locked) {
        place_window_apply(wm, surface, client);
    }

    /* ICCCM §4.1.2.4: honor 'WM_HINTS' 'initial_state' when
     * 'IconicState' */
    if (client->hints_icccm.hints.is_initial_iconic) {
        ccmd_client_iconify(client);
    } else {
        if (client->titlebar != 0) {
            xcb_map_window(connection, client->titlebar);
        }

        if (client->frame != 0) {
            xcb_map_window(connection, client->frame);
            xcb_map_window(connection, client->window);
        } else {
            xcb_map_window(connection, event->window);
        }

        client_unhide(client);

        if (config->base.windows.focus.focus_new &&
                client_is_focusable(client)) {
            focus_apply(surfaces, surface, desktop, client, true,
                    config);
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
        client_send_synthetic_configure_notify(connection, client);
        xcb_clear_area(connection, 1, client->window, 0, 0, 0, 0);

        /* EWMH's own correct way for a client to request fullscreen
         * from the outset (see 'hints_ewmh.initial_state''s own doc
         * comment, client.h) rather than waiting for a 'ClientMessage'
         * after mapping.  Deliberately last in this whole block, after
         * the synthetic 'ConfigureNotify' just above:
         * 'ccmd_client_fullscreen' sends its own with the true
         * fullscreen geometry, and sending the ordinary one afterward
         * would tell the client its old, pre-fullscreen position and
         * size right after telling it the correct one. */
        /* Re-read rather than trusting what 'client_init' saw.  That
         * read happens when the window is first adopted, and a client
         * that sets '_NET_WM_STATE' around that moment could be
         * sampled before it got there: the state was then lost for
         * good, since nothing reads the property again, and the
         * window came up at its own small size while every later
         * check still believed it was fullscreen.  Asking again here,
         * with the window about to be shown, makes the outcome the
         * same whichever order the two happened in. */
        s_map_refresh_initial_state(connection, ewmh, client);

        if (client->hints_ewmh.initial_state.is_fullscreen) {
            ccmd_client_fullscreen(client);
        } else if (client->hints_ewmh.initial_state.is_maximized_horz &&
                client->hints_ewmh.initial_state.is_maximized_vert) {
            /* Same reasoning as 'is_fullscreen' just above, for
             * the same EWMH pre-existing-state mechanism applied to
             * 'is_maximized_horz'/'_vert' (client.h) instead; a
             * client requesting both at once is maximized on both
             * axes together, one call, rather than two in sequence
             * each sending its own synthetic 'ConfigureNotify' for an
             * intermediate, single-axis geometry the client never
             * actually asked for. */
            ccmd_client_maximize(client);
        } else if (client->hints_ewmh.initial_state.is_maximized_horz) {
            ccmd_client_maximize_horz(client);
        } else if (client->hints_ewmh.initial_state.is_maximized_vert) {
            ccmd_client_maximize_vert(client);
        }
    }

    /* Re-apply layer stacking so newly mapped windows do not obscure
     * clients already assigned to the above layer */
    ccmd_desktop_enforce_layers(desktop);

    wm_outdate_surface(surface);
    wm_outdate_desktop(desktop);
    xcb_flush(connection);

    LOGGER_DEBUG("Mapped and adopted window %#x ('%s') on desktop %u",
            event->window, client->info.name, desktop->id);

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


/* Handle an 'UNMAP_NOTIFY' event */
void handler_unmap_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_unmap_notify_event_t *event)
{
    client_td *client;
    desktop_td *desktop;
    surface_td *surface;

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

        /* Marked hidden before the fallback call just below, not
         * after: 'ccmd_client_focus' (called from inside
         * 'client_focus_fallback') redirects to whichever mapped
         * transient descendant of the new target should actually
         * receive focus in its place (see 'ccmd_client_focus_target'
         * 's comment, cmds/client/internal.h), and that
         * redirect walk excludes a 'CLIENT_FLAG_HIDDEN' candidate
         * specifically so a fallback landing back on 'client''s own
         * parent does not find this same withdrawing 'client' here
         * and send real input focus right back onto it. */
        client_hide(client);

        if (desktop != NULL &&
                desktop->client_active_id == client->id) {
            client_focus_fallback(desktop, surface, client);
        }

        /* When a managed client withdraws itself (for example, to
         * a system tray), unmap every WM-created decoration so
         * later render passes never remap the ghost frame */
        if (client->frame != 0) {
            client->ignore.unmap++;
            xcb_unmap_window(client->connection, client->frame);
        }
        if (client->titlebar != 0) {
            client->ignore.unmap++;
            xcb_unmap_window(client->connection, client->titlebar);
        }
        /* 'properties.state' itself, not just the published EWMH
         * property, must also stop claiming fullscreen here: a
         * fullscreen client that withdraws itself this way previously
         * had only its own '_NET_WM_STATE_FULLSCREEN' atom stripped
         * from the property below, with nothing here ever touching
         * 'properties.state' itself, silently leaving the two
         * disagreeing with each other from then on.  Reset to plain
         * normal specifically, not iconified: 'client_hide' just
         * above already marks 'CLIENT_FLAG_HIDDEN', which alone is
         * enough for 'ccmd_client_sync_states' (cmds/client/ewmh.c)
         * to correctly still publish '_NET_WM_STATE_HIDDEN' below;
         * claiming 'CLIENT_STATE_ICONIFIED' here instead would make
         * 'client_is_iconified' true for a client the window manager
         * itself never actually iconified, with consequences well
         * beyond this one property (the whole transient-family
         * iconify/restore cascade among them). */
        client->properties.state = CLIENT_STATE_NORMAL;

        ccmd_set_wm_state(client, CCMD_WM_STATE_ICONIC, XCB_NONE);
        ccmd_client_sync_states(client);
        /* 'wm_outdate_client'/'_surface'/'_desktop' directly, not
         * 'wm_request_client_redraw': 'surface' and 'desktop' are
         * already resolved locally above (from the same
         * 'lookup_find_client' call this whole handler already made),
         * so calling the convenience wrapper here would only re-derive
         * both through a redundant lookup, an O(n) scan over the
         * global 'wm->surfaces' for 'surface' alone, for values
         * already sitting in scope. */
        wm_outdate_client(client);
        wm_outdate_surface(surface);
        wm_outdate_desktop(desktop);

        if (connection != NULL) {
            xcb_flush(connection);
        }
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

    /* Before the managed-client lookup below, and unconditionally: a
     * docked systray icon is never a managed client at all, so the
     * early return that lookup takes for an unmanaged window would
     * otherwise leave the destroyed icon in the tray's own array
     * forever */
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

    if (desktop != NULL) {
        desktop_action_client_rem(desktop, client);
        /* Refresh work area in case the removed client had struts */
        surface_refresh_workareas(surface);
    }

    /* Falls back AFTER 'client' is already removed from 'desktop',
     * not before: 'ccmd_client_focus' (called from inside this),
     * itself redirects to whichever mapped transient descendant of
     * the new target should actually receive focus in its place
     * (see 'ccmd_client_focus_target''s comment, cmds/
     * client/internal.h).  With 'client' (the very dialog now
     * closing) still sitting in 'desktop->clients' at the time of
     * that redirect, a fallback landing back on its own parent would
     * find this closing dialog itself as a still-valid-looking
     * transient child, and redirect real input focus right back onto
     * a window about to be destroyed a few lines below, rather than
     * onto the parent the fallback just chose. */
    if (desktop != NULL && desktop->client_active_id == client->id) {
        client_focus_fallback(desktop, surface, client);
        if (connection != NULL) {
            xcb_flush(connection);
        }
    }

    /* ICCCM withdrawn state: remove WM_STATE on unmanage */
    ccmd_clear_wm_state(client);

    /* When the frame is destroyed the X server also destroys all its
     * children ('client->window', 'client->titlebar').  Zero them all
     * out so client_destroy does not issue redundant
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
         * client_destroy from issuing redundant destroy calls. */
        if (connection != NULL && client->frame != 0) {
            client->ignore.unmap++;
            xcb_destroy_window(connection, client->frame);
            xcb_flush(connection);
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

            /* Re-assert the plain-pointer cursor 'client_init'
             * already set once on this same window (see client.c).
             * Many GTK/GDK applications explicitly set their own
             * top-level window's cursor as part of their own
             * realization, which can run after (and so silently
             * overwrite) that first assignment; MapNotify, confirming
             * the window has actually become visible, is reliably
             * later than that realization, so setting it again here
             * wins whatever race existed. */
            xcb_change_window_attributes(connection, client->window,
                    XCB_CW_CURSOR,
                    (const uint32_t[]) { mouse_plain_cursor() });
            LOGGER_TRACE("Set cursor (window=0x%x, cursor=0x%x)",
                    client->window, mouse_plain_cursor());
        }
        xcb_flush(connection);
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
     * ('client->layout.gravity') placed it at a non-NW anchor.
     * Update the cached frame position and re-sync decorations. */
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
    xcb_window_t target;
    uint32_t stack_mode;

    if (event == NULL) {
        LOGGER_ERROR("Received null pointer in circulate request" \
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Circulate request event (window=0x%x, place=%u)",
            event->window, event->place);

    client = lookup_find_client(surfaces, event->window, NULL, NULL);
    if (client == NULL) {
        return;
    }

    target = ccmd_target_win(client);
    stack_mode = (event->place == XCB_PLACE_ON_TOP)
        ? (uint32_t) XCB_STACK_MODE_ABOVE
        : (uint32_t) XCB_STACK_MODE_BELOW;

    xcb_configure_window(connection, target,
            XCB_CONFIG_WINDOW_STACK_MODE, &stack_mode);
    xcb_flush(connection);
    wm_request_client_redraw(client);
}
