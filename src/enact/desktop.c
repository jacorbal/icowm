/**
 * @file enact/desktop.c
 *
 * @brief Every desktop-level action this window manager can carry
 *        out, one typed function per action
 *
 * One of the files @c enact/ is made of; see
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

/* Default initial values */
#include <defs/desktop.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <policy/stacking.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* IPC includes */
#include <ipc.h>

/* Command includes */
#include <cmds/client/focus.h>
#include <cmds/client/transient.h>
#include <cmds/client/visibility.h>
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
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


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
 * Holds the single-client half of
 * @a enact_desktop_client_send, so that function can redirect to, and
 * cascade across, a transient family (see its comment) while
 * still sharing this single client's worth of desktop-move plumbing
 * with the top-level, family-unaware call it makes on the family's
 * top parent and on every other member in turn.
 *
 * @param desktop Client's current desktop; must be non-null
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
    bool unmapped_main = false;
    bool unmapped_icon = false;

    LOGGER_TRACE("Sending client window=0x%x from desktop %u to" \
            " desktop %u", client->window, desktop->id, target->id);

    surface = wm_get_surface_by_id(client->screen_id);

    /* If the client is currently visible on the active desktop, unmap
     * it immediately so it disappears from the source desktop without
     * waiting for the user to switch away */
    if (surface != NULL &&
            desktop->id == surface->desktop_cur &&
            !(client->properties.flags & CLIENT_FLAG_HIDDEN) &&
            !client_is_iconified(client)) {
        win_target = (client_is_decorated(client) && client->frame != 0)
            ? client->frame : client->window;
        ccmd_client_unmap_decorated(client, win_target);
        unmapped_main = true;
        if (client->icon_window != 0 && client->is_icon_mapped) {
            xcb_window_hide(client->icon_window);
            client->is_icon_mapped = false;
            unmapped_icon = true;
        }
    }

    /* Moved to 'target' before the fallback call just below, not
     * after: 'ccmd_client_focus' (called from inside
     * 'client_focus_fallback') redirects to whichever mapped
     * transient descendant of the new fallback target should
     * actually receive focus in its place, as
     * 'ccmd_client_focus_target''s comment in
     * 'cmds/client/transient.h' describes, and
     * that redirect walk would otherwise still find 'client' sitting
     * in 'desktop->clients' at the moment of the search, even though
     * it is already on its way to 'target'; the same reasoning
     * behind the matching reorder in 'handler_destroy_notify' and
     * 'handler_unmap_notify' (handler/map.c). */
    desktop_action_client_rem(desktop, client);
    if (desktop_action_client_add(target, client) != 0) {
        /* 'target' refused it (a resource exhaustion or a genuine
         * hashtable-insert failure; see 'desktop_action_client_add's
         * comment, desktop.h): put it back exactly where it
         * came from, undoing the unmap above too if it happened,
         * rather than leaving the client registered under neither
         * desktop, or invisible on the very desktop it is actually
         * still on. */
        LOGGER_WARNING("Failed to move client window=0x%x to" \
                " desktop %u; leaving it on desktop %u instead",
                client->window, target->id, desktop->id);
        (void) desktop_action_client_add(desktop, client);
        if (unmapped_main) {
            win_target = (client_is_decorated(client) &&
                    client->frame != 0)
                ? client->frame : client->window;
            xcb_window_show(win_target);
            if (win_target != client->window) {
                xcb_window_show(client->window);
            }
            if (unmapped_icon) {
                xcb_window_show(client->icon_window);
                client->is_icon_mapped = true;
            }
        }
        return;
    }
    client->desktop_id = target->id;

    /* Remembered here as 'target''s active client, the same
     * memory 'surface_clients_show' (surface/actions/clients.c)
     * reads back whenever this desktop next becomes visible, so a
     * client just sent here is what greets a user arriving later,
     * exactly as if it had always been the thing they cared about
     * on this desktop, rather than something they have to go hunt
     * for.  Left unset for a genuinely unfocusable client (the same
     * gate 'surface_clients_show' itself re-checks on the read side
     * regardless, gracefully falling through to
     * 'client_focus_fallback''s guess if this one somehow no
     * longer qualifies
     * by the time it is actually read), so it never becomes the
     * remembered target only to be silently skipped over later.
     * Deliberately unconditional otherwise, overwriting whatever
     * 'target' already remembered even when it was not empty.  A
     * client someone just deliberately placed here is a reasonable
     * thing to consider more relevant on arrival than whatever was
     * last active before it showed up, matching how a freshly opened
     * window already becomes a desktop's new active client. */
    if (client_is_focusable(client)) {
        target->client_active_id = client->id;
        target->is_focus_dirty = true;
    }

    /* Published here, once, for every caller of this whole desktop-
     * move mechanism alike (the "Send to desktop" menu, the move-to-
     * desktop keybind, and any rule with its 'apply.desktop'),
     * rather than each duplicating this same publish on its own:
     * an EWMH-aware external tool (a taskbar or pager) watching
     * '_NET_WM_DESKTOP' needs to learn about the reassignment
     * regardless of which of those actually triggered it.  A pinned
     * client keeps publishing the EWMH "all desktops" sentinel
     * instead of any one real index, unaffected by which desktop it
     * is actually registered under (see 'ccmd_client_bring_family's
     * comment, cmds/client/transient.c, for the fuller
     * reasoning on why a pinned client's registration and its
     * own published desktop can differ like this). */
    if (xcb_ewmh_connection_get() != NULL) {
        uint32_t did = (client->properties.flags & CLIENT_FLAG_PIN)
            ? WM_DESKTOP_ID_ALL : target->id;

        xcb_change_property(xcb_connection_get(), XCB_PROP_MODE_REPLACE,
                client->window, xcb_ewmh_connection_get()->_NET_WM_DESKTOP,
                XCB_ATOM_CARDINAL, 32, 1, &did);
    }

    /* If 'client' was the source desktop's active client, hand
     * focus there off to whatever else on that desktop qualifies,
     * the same way closing, hiding, or iconifying the active client
     * already does everywhere else in this project (see
     * 's_client_focus_fallback''s comment); without this,
     * the source desktop's 'client_active_id' was left pointing at a
     * client no longer even in its list, and because the client
     * is unmapped above when it was visible, the X server's real
     * keyboard focus was left on a now-unmapped window instead of
     * transferring to another visible one, rather than silently
     * doing nothing as an already-inactive client being sent away
     * correctly does. */
    if (desktop->client_active_id == client->id) {
        client_focus_fallback(desktop, surface, client);
    }

    enact_broadcast_client_event(client, IPC_EVENT_CLIENT_DESKTOP_CHANGED);
}


/**
 * @brief What @a s_desktop_rearrange_visit carries across the desktop
 */
struct s_rearrange_ctx_s {
    /** Window manager, needed to find a transient's parent */
    const wm_td *wm;
    surface_td *surface;    /**< Surface being rearranged */
    bool is_single_spot;    /**< Whether the policy has one spot only */
    bool is_first;          /**< Whether this is the first client */
};


/**
 * @brief Place one client afresh while rearranging a desktop
 *
 * Every client goes through the same general-purpose placement engine
 * a newly mapped window does, not a rearrange-only routine, so a
 * transient dialog among them is re-centered over its parent per
 * ICCCM §4.1.2.6 rather than moved by the configured policy.  That
 * parent can live on another surface, which is why this needs the
 * whole @c wm_td rather than a desktop.
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_rearrange_ctx_s this walk carries
 *
 * @note The "centered" and "under-mouse" policies resolve to a single
 *       spot, so only the first client uses the configured policy and
 *       the rest cascade; otherwise they would all land on each other
 * @note Complexity: @e O(n), the placement engine's cost
 */
static void s_desktop_rearrange_visit(client_td *client, void *data)
{
    struct s_rearrange_ctx_s *const rearrange_ctx = data;

    if (client == NULL || rearrange_ctx == NULL ||
            client_is_locked(client)) {
        return;
    }

    if (rearrange_ctx->is_single_spot && !rearrange_ctx->is_first) {
        place_window_apply_cascade(rearrange_ctx->wm,
                rearrange_ctx->surface, client);
    } else {
        place_window_apply(rearrange_ctx->wm, rearrange_ctx->surface,
                client);
    }
    rearrange_ctx->is_first = false;
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
     * 'loop_refresh' only calls 'surface_render_all_desktops' at all
     * when this desktop's surface is itself outdated (see
     * 'enact_desktop_show', right below, for the same pattern).
     * Without this, the new color never actually repaints until
     * something else marks the surface outdated for an unrelated
     * reason, e.g., switching desktops away and back. */
    surface->is_outdated = true;
    s_broadcast_desktop_event(desktop,
            IPC_EVENT_DESKTOP_BACKGROUND_CHANGED);
}


/* Toggle whether the desktop's surface shows the desktop */
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
    s_broadcast_desktop_event(desktop,
            (show) ? IPC_EVENT_DESKTOP_SHOWN : IPC_EVENT_DESKTOP_HIDDEN);
}


/**
 * @brief Send the client to another desktop, taking its whole
 *        transient family with it
 *
 * The desktop-move counterpart to @a ccmd_client_iconify's
 * transient-family cascade (see its comment, cmds/client/
 * visibility.c, for the full reasoning).  Redirects to the family's
 * top-most ancestor first, moving it exactly as this function always
 * has, then moves every other member of that same family too, so a
 * "save changes?" prompt (or any other transient dialog) never ends
 * up left behind on the old desktop, stranded apart from the parent
 * window it belongs to and cannot meaningfully be used without.  A
 * client with no transient relatives at all is unaffected.  Its
 * top parent is itself, and no sibling scan finds anything else to
 * move alongside it.
 *
 * @param desktop Client's current desktop
 * @param client  Window to move
 * @param target  Desktop to move it to
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the top parent's desktop
 */
void enact_desktop_client_send(const desktop_td *desktop,
        client_td *client, desktop_td *target)
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
    enact_broadcast_client_event(client, IPC_EVENT_STACKING_CHANGED);
}


/* Re-apply the configured placement policy to every client on the
 * desktop */
void enact_desktop_clients_rearrange(const wm_td *wm,
        surface_td *surface, const desktop_td *desktop)
{
    struct s_rearrange_ctx_s rearrange_ctx;
    enum config_placement_policy_e policy;
    bool single_spot_policy;
    config_td *config = wm_config(wm);

    if (wm == NULL || config == NULL || surface == NULL ||
            desktop == NULL) {
        return;
    }

    policy = config->base.windows.placement_policy;
    single_spot_policy =
        (policy == CONFIG_PLACEMENT_POLICY_CENTERED) ||
        (policy == CONFIG_PLACEMENT_POLICY_UNDER_MOUSE) ||
        (policy == CONFIG_PLACEMENT_POLICY_MANUAL);

    rearrange_ctx.wm = wm;
    rearrange_ctx.surface = surface;
    rearrange_ctx.is_single_spot = single_spot_policy;
    rearrange_ctx.is_first = true;
    stacking_walk(desktop, s_desktop_rearrange_visit, &rearrange_ctx);

}


/* Iconify every client on the desktop */
void enact_desktop_clients_iconify_all(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_clients_iconify_all(desktop);
}


/* Restore every iconified client on the desktop */
void enact_desktop_clients_deiconify_all(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    desktop_action_clients_deiconify_all(desktop);
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
}
