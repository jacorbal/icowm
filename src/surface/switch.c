/**
 * @file surface/switch.c
 *
 * @brief Surface desktop management to add, remove, and switch desktops
 *
 * Implements the surface-level operations that create or destroy
 * desktops and switch the active desktop, including the fullscreen-
 * surface toggle.  Client visibility management and RandR operations
 * live in @c surface/actions.c.
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


/* Add a new desktop to the surface */
int surface_action_desktop_add(surface_td *surface)
{
    desktop_td *desktop;

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
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

    surface->is_outdated = true;

    return 0;
}


/* Remove the last desktop from the surface */
int surface_action_desktop_remove(surface_td *surface)
{
    cdlist_item_td *tail_item;
    const desktop_td *desktop;

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

    /* If the desktop to be removed is the current one, switch first */
    if (desktop->id == surface->desktop_cur) {
        surface_clients_hide(surface, surface->desktop_cur);
        surface_desktop_select_prev(surface, false);
        surface_clients_show(surface, surface->desktop_cur);
    }

    if (surface_desktop_rem(surface, desktop->id) != 0) {
        LOGGER_ERROR("Failed to remove desktop from surface %u",
                surface->id);
        return 1;
    }

    surface->is_outdated = true;

    return 0;
}


/* Switch to a specific desktop by ID */
int surface_action_desktop_switch(surface_td *surface,
        uint32_t desktop_id)
{
    uint32_t old_id;

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Switching to desktop %u on surface %u",
            desktop_id, surface->id);

    old_id = surface->desktop_cur;
    if (desktop_id == old_id) {
        return 0;
    }

    surface_clients_hide(surface, old_id);
    if (surface_desktop_select(surface, desktop_id) != 0) {
        /* Restore visibility on failure */
        surface_clients_show(surface, old_id);
        LOGGER_ERROR("Failed to switch to desktop %u on surface %u",
                desktop_id, surface->id);
        return 1;
    }

    surface_clients_show(surface, desktop_id);
    surface->is_outdated = true;
    xcb_flush(surface->connection);

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
    surface->is_outdated = true;
    xcb_flush(surface->connection);

    return 0;
}
