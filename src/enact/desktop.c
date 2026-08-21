/**
 * @file enact/desktop.c
 *
 * @brief Every desktop-level action this window manager can carry
 *        out, one typed function per action
 *
 * Split out of what used to be a single, flat @c enact.c; see
 * @c enact/internal.h for why.
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
#include <stdlib.h>     /* free */

/* ADT includes */
#include <adt/cdlist.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* IPC includes */
#include <ipc.h>

/* Command includes */
#include <cmds/client/basic.h>
#include <cmds/client/geom.h>
#include <cmds/surface.h>

/* Menu includes */
#include <menu/cycle.h>
#include <menu/dialog/info.h>

/* Policy includes */
#include <policy/placement/window.h>

/* Handler includes */
#include <handler/internal.h>

/* Local includes */
#include <enact.h>
#include <enact/internal.h>


/**
 * @brief Broadcast an event whose payload is just the standard
 *        desktop/surface identifier pair, with no specific client
 *        involved
 *
 * @param desktop Desktop the event happened to
 * @param type    Which event this is
 *
 * @note A null desktop is a silent no-op
 * @note Complexity: @e O(1)
 */
static void s_broadcast_desktop_event(desktop_td *desktop,
        uint32_t type)
{
    cJSON *fields;

    if (desktop == NULL) {
        return;
    }

    fields = cJSON_CreateObject();
    if (fields != NULL) {
        cJSON_AddNumberToObject(fields, "desktop_id",
                (double) desktop->id);
        cJSON_AddNumberToObject(fields, "surface_id",
                (double) desktop->screen_id);
    }
    ipc_broadcast_event(type, fields);
}


/**
 * @brief Send exactly this one client from one desktop to another,
 *        ignoring any transient family it may belong to
 *
 * Split out of what used to be the whole of @a enact_desktop_client_
 * send so that function can redirect to, and cascade across, a
 * transient family (see its own doc comment) while still sharing
 * this single client's worth of desktop-move plumbing with the
 * top-level, family-unaware call it makes on the family's own top
 * parent and on every other member in turn.
 *
 * @param desktop Client's own current desktop; must be non-null
 * @param client  Client to move; must be non-null
 * @param target  Desktop to move it to; must be non-null
 *
 * @note Complexity: @e O(1)
 */
static void s_enact_desktop_client_send_one(desktop_td *desktop,
        client_td *client, desktop_td *target)
{
    surface_td *surface;
    xcb_window_t win_target;

    LOGGER_TRACE("Sending client window=0x%x from desktop %u to" \
            " desktop %u", client->window, desktop->id, target->id);

    surface = wm_get_surface_by_id(client->screen_id);

    /* If the client is currently visible on the active desktop, unmap
     * it immediately so it disappears from the source desktop without
     * waiting for the user to switch away */
    if (surface != NULL &&
            desktop->id == surface->desktop_cur &&
            !(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
            client->properties.state != (uint16_t) CLIENT_STATE_ICONIFIED) {
        win_target = (client_is_decorated(client) && client->frame != 0)
            ? client->frame : client->window;
        ccmd_client_unmap_decorated(client, surface->connection,
                win_target);
        if (client->icon_window != 0 && client->is_icon_mapped) {
            xcb_unmap_window(surface->connection, client->icon_window);
            client->is_icon_mapped = false;
        }
        xcb_flush(surface->connection);
    }

    /* Moved to 'target' before the fallback call just below, not
     * after: 'ccmd_client_focus' (called from inside
     * 'client_focus_fallback') redirects to whichever mapped
     * transient descendant of the new fallback target should
     * actually receive focus in its place (see 'ccmd_client_
     * focus_target''s own doc comment, cmds/client/basic.h), and
     * that redirect walk would otherwise still find 'client' sitting
     * in 'desktop->clients' at the moment of the search, even though
     * it is already on its way to 'target'; the same reasoning
     * behind the matching reorder in 'handler_destroy_notify' and
     * 'handler_unmap_notify' (handler/map.c). */
    desktop_action_client_rem(desktop, client);
    desktop_action_client_add(target, client);
    client->desktop_id = target->id;

    /* If 'client' was the source desktop's own active client, hand
     * focus there off to whatever else on that desktop qualifies,
     * the same way closing, hiding, or iconifying the active client
     * already does everywhere else in this project (see
     * 's_client_focus_fallback''s own doc comment); without this,
     * the source desktop's 'client_active_id' was left pointing at a
     * client no longer even in its own list, and because the client
     * is unmapped above when it was visible, the X server's own real
     * keyboard focus was left on a now-unmapped window instead of
     * transferring to another visible one, rather than silently
     * doing nothing as an already-inactive client being sent away
     * correctly does. */
    if (desktop->client_active_id == client->id) {
        client_focus_fallback(desktop, surface, client);
    }

    xcb_flush(desktop->connection);
    enact_broadcast_client_event(client, IPC_EVENT_CLIENT_DESKTOP_CHANGED);
}


/* 'action_desktop_e' */

void enact_desktop_set_background(desktop_td *desktop, uint32_t color)
{
    surface_td *surface;

    if (desktop == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(desktop->screen_id);
    if (surface == NULL) {
        return;
    }

    desktop->background.is_image = false;
    desktop->background.use_root_pixmap = false;
    desktop->background.bg.color = color;
    desktop->is_outdated = true;
    /* Marking only 'desktop->is_outdated' is not enough on its own:
     * 'loop_update' only calls 'surface_render_all_desktops' at all
     * when this desktop's own surface is itself outdated (see
     * 'enact_desktop_show', right below, for the same pattern).
     * Without this, the new color never actually repaints until
     * something else marks the surface outdated for an unrelated
     * reason, e.g., switching desktops away and back. */
    surface->is_outdated = true;
    xcb_flush(desktop->connection);
    s_broadcast_desktop_event(desktop,
            IPC_EVENT_DESKTOP_BACKGROUND_CHANGED);
}


/* Toggle whether the desktop's own surface shows the desktop */
void enact_desktop_show(desktop_td *desktop, bool show)
{
    surface_td *surface;

    if (desktop == NULL) {
        return;
    }

    surface = wm_get_surface_by_id(desktop->screen_id);
    if (surface == NULL) {
        return;
    }

    hi_handle_net_showing_desktop(surface, show);
    xcb_flush(surface->connection);
    s_broadcast_desktop_event(desktop,
            (show) ? IPC_EVENT_DESKTOP_SHOWN : IPC_EVENT_DESKTOP_HIDDEN);
}


/**
 * @brief Send the client to another desktop, taking its whole
 *        transient family with it
 *
 * The desktop-move counterpart to @a ccmd_client_iconify's own
 * transient-family cascade (see its own doc comment, cmds/client/
 * visibility.c, for the full reasoning): redirects to the family's
 * top-most ancestor first, moving it exactly as this function always
 * has, then moves every other member of that same family too, so a
 * "save changes?" prompt (or any other transient dialog) never ends
 * up left behind on the old desktop, stranded apart from the parent
 * window it belongs to and cannot meaningfully be used without.  A
 * client with no transient relatives at all is unaffected: its own
 * top parent is itself, and no sibling scan finds anything else to
 * move alongside it.
 *
 * @param desktop Client's own current desktop
 * @param client  Window to move
 * @param target  Desktop to move it to
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the top parent's own desktop
 */
void enact_desktop_client_send(desktop_td *desktop, client_td *client,
        desktop_td *target)
{
    client_td *top;
    desktop_td *top_desktop;
    client_td **siblings;
    size_t count;

    if (desktop == NULL || client == NULL || target == NULL) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    top_desktop = wm_get_client_desktop(top);
    if (top_desktop == NULL) {
        return;
    }

    s_enact_desktop_client_send_one(top_desktop, top, target);

    count = 0;
    siblings = ccmd_client_transient_family_snapshot(top_desktop, top,
            &count);
    if (siblings != NULL) {
        for (size_t i = 0; i < count; i++) {
            s_enact_desktop_client_send_one(top_desktop, siblings[i],
                    target);
        }

        free(siblings);
    }
}


/* Send a client to the front of the desktop's window stack */
void enact_desktop_client_send_front(desktop_td *desktop,
        client_td *client)
{
    if (desktop == NULL || client == NULL) {
        return;
    }

    (void) desktop_action_client_send_front(desktop, client);
    xcb_flush(desktop->connection);
    enact_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
}


/* Send a client to the back of the desktop's window stack */
void enact_desktop_client_send_back(desktop_td *desktop,
        client_td *client)
{
    if (desktop == NULL || client == NULL) {
        return;
    }

    (void) desktop_action_client_send_back(desktop, client);
    xcb_flush(desktop->connection);
    enact_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
}


/* Re-apply the configured placement policy to every client on the
 * desktop */
void enact_desktop_clients_rearrange(const wm_td *wm,
        surface_td *surface, desktop_td *desktop)
{
    cdlist_item_td *node;
    enum config_placement_policy_e policy;
    bool single_spot_policy;
    bool is_first;
    config_td *config = wm_config(wm);

    if (wm == NULL || config == NULL || surface == NULL ||
            desktop == NULL) {
        return;
    }

    policy = config->base.windows.placement_policy;
    single_spot_policy =
        (policy == CONFIG_PLACEMENT_POLICY_CENTERED) ||
        (policy == CONFIG_PLACEMENT_POLICY_UNDER_MOUSE);
    is_first = true;

    node = cdlist_head(desktop->stacking);
    if (node != NULL) {
        /* 'desktop->stacking' is circular (see 'cdlist_next''s comment
         * in 'adt/cdlist.h').  Its own tail wraps back to its own head
         * rather than ever handing back a null, so a caller has to
         * remember where it started and stop once it gets back there,
         * the same 'initial' pattern already used to walk this same
         * list elsewhere (e.g., 'desktop_action_client_rem' in
         * 'desktop/dclient.c').
         *
         * A plain 'for (...; node != NULL; ...)' loop over it, as this
         * one used to be, never terminates for a non-empty desktop: it
         * silently spins inside this one call forever, which blocks the
         * whole event loop (this function's own caller runs
         * synchronously from it) from ever processing another key
         * press, mouse click, or menu, until the process is killed from
         * outside. */
        const cdlist_item_td *initial = node;

        do {
            client_td *const client = (client_td *) cdlist_data(node);

            if (client != NULL && !client_is_locked(client)) {
                /* Every client on the desktop goes through
                 * 'place_window_apply'/'place_window_apply_cascade',
                 * the same general-purpose placement engine a newly
                 * mapped window is run through, not a simplified
                 * rearrange-only positioning routine.  That means
                 * a transient dialog among them (a client with its own
                 * 'transient_for' set) is not repositioned by the
                 * configured placement policy below at all.
                 *
                 * 'place_window_apply' re-centers it over its own
                 * parent per ICCCM §4.1.2.6 instead, the same as it
                 * would have been placed there in the first place.
                 * Finding that parent is why this function needs the
                 * full 'wm_td' rather than just 'desktop' or 'config'.
                 * The parent can live on a different surface entirely,
                 * so locating it means searching 'wm->surfaces' as
                 * a whole (see 's_place_window_transient_centered' in
                 * 'policy/placement/window.c'). */

                /* 'centered'/'under-mouse' always resolve to the exact
                 * same single spot, so every client after the first
                 * would land stacked on top of one another; only the
                 * first client uses the real configured policy, the
                 * rest fall back to cascade so the desktop ends up
                 * spread out instead of piled up */
                if (single_spot_policy && !is_first) {
                    place_window_apply_cascade(wm, surface, client);
                } else {
                    place_window_apply(wm, surface, client);
                }
                is_first = false;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    xcb_flush(surface->connection);
}


/* Iconify every client on the desktop */
void enact_desktop_clients_iconify_all(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_clients_iconify_all(desktop);
    xcb_flush(desktop->connection);
}


/* Restore every iconified client on the desktop */
void enact_desktop_clients_deiconify_all(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_clients_deiconify_all(desktop);
    xcb_flush(desktop->connection);
}


/* Cycle input focus to the next non-iconified client */
void enact_desktop_cycle_clients_active(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg)
{
    if (surface == NULL || desktop == NULL) {
        return;
    }

    cycle_init(connection, surface, desktop, false, 1,
            modifier, cfg);
    cycle_draw(connection, cfg);
    xcb_flush(connection);
}


/* Cycle input focus to the previous non-iconified client */
void enact_desktop_cycle_clients_prev(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg)
{
    if (surface == NULL || desktop == NULL) {
        return;
    }

    cycle_init(connection, surface, desktop, false, -1,
            modifier, cfg);
    cycle_draw(connection, cfg);
    xcb_flush(connection);
}


/* Cycle input focus to the next iconified client */
void enact_desktop_cycle_clients_icons_next(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg)
{
    if (surface == NULL || desktop == NULL) {
        return;
    }

    cycle_init(connection, surface, desktop, true, 1,
            modifier, cfg);
    cycle_draw(connection, cfg);
    xcb_flush(connection);
}


/* Cycle input focus to the previous iconified client */
void enact_desktop_cycle_clients_icons_prev(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg)
{
    if (surface == NULL || desktop == NULL) {
        return;
    }

    cycle_init(connection, surface, desktop, true, -1,
            modifier, cfg);
    cycle_draw(connection, cfg);
    xcb_flush(connection);
}
