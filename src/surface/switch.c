/**
 * @file surface/switch.c
 *
 * @brief Surface desktop management to add and remove desktops
 *
 * Implements the surface-level operations that create or destroy
 * desktops, including the fullscreen-surface toggle.  Switching the
 * current desktop itself lives in @c cmds/surface.c, alongside the
 * sticky-client transfer a real desktop switch also needs; client
 * visibility management and RandR operations live in
 * @c surface/actions.c.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>


/**
 * @brief Mark every one of a surface's own desktops as outdated
 *
 * @c desktop_repaint_titlebar_content (render/desktop.c) recomputes
 * whether the pin button belongs on a client's own titlebar
 * (@c hide_pin) fresh every time it runs, from @p surface's own
 * current @c desktop_count, but only actually runs for a desktop
 * whose own @c is_outdated is set (@a surface_render_current_desktop,
 * render/surface.c).  @a surface_action_desktop_add / @c _remove only
 * ever marked @p surface itself outdated, not any of its individual
 * desktops, which left every existing desktop's own clients showing
 * a stale pin button (present or missing) until some unrelated event
 * (a focus change, in practice) happened to mark that one specific
 * desktop outdated on its own.
 *
 * @param surface Surface whose own desktops should all be marked
 *                outdated
 *
 * @note No-op if @p surface or its own desktop list is @c NULL
 * @note Complexity: @e O(n), where @e n is @p surface's own desktop
 *       count
 */
static void s_surface_mark_all_desktops_outdated(surface_td *surface)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (surface == NULL || surface->desktops == NULL) {
        return;
    }

    node = cdlist_head(surface->desktops);
    if (node == NULL) {
        return;
    }

    initial = node;
    do {
        desktop_td *desktop = (desktop_td *) cdlist_data(node);

        if (desktop != NULL) {
            desktop->is_outdated = true;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);
}


/* Add a new desktop to the surface */
int surface_action_desktop_add(surface_td *surface)
{
    desktop_td *desktop;

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    /* 'config_base->screens[screen_id].desktops[desktop_id]'
     * (desktop.c, 's_desktop_read_config_settings' and its own
     * caller) is a fixed-size 'CONFIG_MAX_DESKTOPS' array indexed by
     * this new desktop's own ID, itself always 'desktop_count'
     * before the increment below; refused outright once that would
     * reach or exceed the array's own real capacity, rather than
     * indexing past its end. */
    if (surface->desktop_count >= (uint32_t) CONFIG_MAX_DESKTOPS) {
        LOGGER_NOTICE("Cannot add another desktop to surface %u:" \
                " already at the configured maximum of %d",
                surface->id, CONFIG_MAX_DESKTOPS);
        return 1;
    }

    LOGGER_DEBUG("Adding new desktop to surface %u", surface->id);

    desktop = desktop_init(surface->connection,
            surface->ewmh,
            surface->id,
            surface->desktop_count,
            &(surface->config->base),
            &(surface->config->theme));
    if (desktop == NULL) {
        LOGGER_ERROR("Failed to initialize new desktop on surface %u",
                surface->id);
        return 1;
    }

    if (surface_desktop_add(surface, desktop) != 0) {
        LOGGER_ERROR("Failed to add desktop to surface %u", surface->id);
        desktop_destroy(desktop);
        return 1;
    }

    s_surface_mark_all_desktops_outdated(surface);
    surface->is_outdated = true;

    return 0;
}


/**
 * @brief Move every client still on @p from_desktop to
 *        @p to_desktop, updating EWMH @c _NET_WM_DESKTOP along
 *        the way
 *
 * Reads @c cdlist_head repeatedly rather than snapshotting the list
 * first: each iteration's own @a desktop_action_client_rem already
 * shrinks @p from_desktop's own stacking list by one, so the next
 * head is always the next client still needing to move, with no
 * separate bound on how many there can be.
 *
 * @param from_desktop Desktop being emptied
 * @param to_desktop   Desktop every client moves to
 *
 * @note No-op if either desktop is @c NULL, or if @p from_desktop
 *       has no clients to begin with
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p from_desktop
 */
static void s_surface_desktop_evacuate(desktop_td *from_desktop,
        desktop_td *to_desktop)
{
    if (from_desktop == NULL || to_desktop == NULL ||
            from_desktop->stacking == NULL) {
        return;
    }

    while (cdlist_size(from_desktop->stacking) > 0) {
        cdlist_item_td *head = cdlist_head(from_desktop->stacking);
        client_td *client = (client_td *) cdlist_data(head);

        if (client == NULL) {
            break;
        }

        desktop_action_client_rem(from_desktop, client);
        desktop_action_client_add(to_desktop, client);
        client->desktop_id = to_desktop->id;

        /* A pinned client's own '_NET_WM_DESKTOP' is already the
         * EWMH 'all desktops' sentinel, set once by 'ccmd_client_pin'
         * and never meant to track a specific desktop again; only a
         * genuinely single-desktop client needs this property
         * brought in line with where it actually landed. */
        if (!client_is_pinned(client) && client->ewmh != NULL &&
                client->connection != NULL) {
            xcb_change_property(client->connection,
                    XCB_PROP_MODE_REPLACE, client->window,
                    client->ewmh->_NET_WM_DESKTOP, XCB_ATOM_CARDINAL,
                    32, 1, &to_desktop->id);
        }
    }
}


/* Remove the last desktop from the surface */
int surface_action_desktop_remove(surface_td *surface)
{
    cdlist_item_td *tail_item;
    cdlist_item_td *fallback_item;
    desktop_td *desktop;
    desktop_td *fallback;
    bool was_current;

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Removing desktop from surface %u", surface->id);

    /* Need at least two desktops to remove one */
    if (surface->desktop_count <= 1) {
        LOGGER_NOTICE("Cannot remove the last desktop on surface %u",
                surface->id);
        return 1;
    }

    tail_item = cdlist_tail(surface->desktops);
    if (tail_item == NULL) {
        return 1;
    }

    desktop = (desktop_td *) cdlist_data(tail_item);
    if (desktop == NULL) {
        return 1;
    }

    /* The desktop immediately before the tail becomes both the new
     * tail once this one is gone, and the fallback home for any
     * client still on it: 'desktop_destroy' (via
     * 'surface_desktop_rem' below) frees its own 'clients' hash
     * table through a 'client_destroy' callback on every entry left
     * in it, which would otherwise silently destroy every real,
     * live application window still on this desktop instead of just
     * the virtual desktop container itself. */
    fallback_item = cdlist_prev(tail_item);
    fallback = (fallback_item != NULL)
        ? (desktop_td *) cdlist_data(fallback_item) : NULL;
    if (fallback == NULL) {
        LOGGER_ERROR("No fallback desktop available on surface %u",
                surface->id);
        return 1;
    }

    was_current = (desktop->id == surface->desktop_cur);
    if (was_current) {
        surface_clients_hide(surface, surface->desktop_cur);
    }

    s_surface_desktop_evacuate(desktop, fallback);

    /* If the desktop to be removed is the current one, switch first */
    if (was_current) {
        surface_desktop_select_prev(surface, false);
        surface_clients_show(surface, surface->desktop_cur);
    }

    if (surface_desktop_rem(surface, desktop->id) != 0) {
        LOGGER_ERROR("Failed to remove desktop from surface %u",
                surface->id);
        return 1;
    }

    s_surface_mark_all_desktops_outdated(surface);
    surface->is_outdated = true;

    return 0;
}


/* Toggle full-surface mode */
int surface_action_toggle_fullsurface(surface_td *surface)
{
    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Toggling full-surface mode on surface %u",
            surface->id);

    surface->fullsurface = !surface->fullsurface;

    /* Recompute every desktop's own work area right away: struts are
     * now folded in, or set aside, differently than a moment ago (see
     * 'desktop_update_workarea''s own 'ignore_struts' parameter,
     * desktop.h), and nothing else is guaranteed to trigger that
     * recomputation on its own until some unrelated event (a client
     * mapping, an RandR change, and so on) happens to call
     * 'surface_refresh_workareas' next. */
    surface_refresh_workareas(surface);

    surface->is_outdated = true;
    xcb_flush(surface->connection);

    return 0;
}
