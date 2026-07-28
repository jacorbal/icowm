/**
 * @file cmds/scmd.c
 *
 * @brief Implementation of actions related to screen surface management
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

/* Project includes */
#include <actdata.h>
#include <logger.h>
#include <surface.h>

/* Local includes */
#include <cmds/scmd.h>


/* Find a client by id inside a desktop hash table */
static client_td *s_desktop_find_client(desktop_td *desktop,
        xcb_window_t id)
{
    size_t i;
    if (desktop == NULL || desktop->clients == NULL || id == XCB_NONE) {
        return NULL;
    }
    for (i = 0; i < desktop->clients->positions; ++i) {
        client_td *client;
        if (desktop->clients->table[i] == NULL ||
                desktop->clients->table[i] == desktop->clients->vacated) {
            continue;
        }
        client = (client_td *) desktop->clients->table[i];
        if (client != NULL && client->id == id) {
            return client;
        }
    }
    return NULL;
}


/* Add a new desktop */
void scmd_surface_desktop_add(surface_td *surface,
        action_data_surface_td *surface_data)
{
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    surface_action_desktop_add(surface);
}


/* Remove a desktop */
void scmd_surface_desktop_rem(surface_td *surface,
        action_data_surface_td *surface_data)
{
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    surface_action_desktop_remove(surface);
}


/* Switch to another desktop */
void scmd_surface_desktop_switch(surface_td *surface,
        action_data_surface_td *surface_data)
{
    desktop_td *old_desktop;
    desktop_td *new_desktop;
    client_td *old_active_client;
    xcb_window_t old_active_id;
    uint32_t old_id;
    uint32_t new_id;

    if (surface == NULL || surface_data == NULL) {
        return;
    }

    new_id = surface_data->new_data.uvalue;
    old_id = surface->desktop_cur;
    old_desktop = surface_desktop_get(surface, old_id);
    old_active_id = (old_desktop != NULL)
        ? old_desktop->client_active_id : 0;
    if (new_id == old_id) {
        return;     /* Already on this desktop */
    }

    LOGGER_DEBUG("Switching desktop: %u → %u on surface %u",
            old_id, new_id, surface->id);

    surface_clients_hide(surface, old_id);
    surface_desktop_select(surface, new_id);
    surface_clients_show(surface, new_id);
    new_desktop = surface_desktop_get(surface, new_id);
    old_active_client = s_desktop_find_client(old_desktop, old_active_id);
    if (new_desktop != NULL && old_active_client != NULL &&
            client_is_sticky(old_active_client)) {
        new_desktop->client_active_id = old_active_client->id;
        new_desktop->is_outdated = true;
    }

    surface->is_outdated = true;
    xcb_flush(surface->connection);
}


/* Switch to the next desktop */
void scmd_surface_desktop_switch_next(surface_td *surface)
{
    desktop_td *old_desktop;
    desktop_td *new_desktop;
    client_td *old_active_client;
    xcb_window_t old_active_id;
    uint32_t old_id;

    if (surface == NULL) {
        return;
    }

    old_id = surface->desktop_cur;

    LOGGER_DEBUG("Switching to next desktop on surface %u", surface->id);

    surface_clients_hide(surface, old_id);
    surface_desktop_select_next(surface, true);
    old_desktop = surface_desktop_get(surface, old_id);
    old_active_id = (old_desktop != NULL)
        ? old_desktop->client_active_id : 0;

    if (surface->desktop_cur != old_id) {
        new_desktop = surface_desktop_get(surface, surface->desktop_cur);
        old_active_client = s_desktop_find_client(old_desktop,
                old_active_id);
        if (new_desktop != NULL && old_active_client != NULL &&
                client_is_sticky(old_active_client)) {
            new_desktop->client_active_id = old_active_client->id;
            new_desktop->is_outdated = true;
        }
        surface_clients_show(surface, surface->desktop_cur);
        surface->is_outdated = true;
        xcb_flush(surface->connection);
    } else {
        /* No switch happened; restore visibility */
        surface_clients_show(surface, old_id);
    }
}


/* Switch to the previous desktop */
void scmd_surface_desktop_switch_prev(surface_td *surface)
{
    desktop_td *old_desktop;
    desktop_td *new_desktop;
    client_td *old_active_client;
    xcb_window_t old_active_id;
    uint32_t old_id;

    if (surface == NULL) {
        return;
    }

    old_id = surface->desktop_cur;
    old_desktop = surface_desktop_get(surface, old_id);
    old_active_id = (old_desktop != NULL)
        ? old_desktop->client_active_id : 0;

    LOGGER_DEBUG("Switching to previous desktop on surface %u",
            surface->id);

    surface_clients_hide(surface, old_id);
    surface_desktop_select_prev(surface, true);

    if (surface->desktop_cur != old_id) {
        new_desktop = surface_desktop_get(surface, surface->desktop_cur);
        old_active_client = s_desktop_find_client(old_desktop,
                old_active_id);
        if (new_desktop != NULL && old_active_client != NULL &&
                client_is_sticky(old_active_client)) {
            new_desktop->client_active_id = old_active_client->id;
            new_desktop->is_outdated = true;
        }
        surface_clients_show(surface, surface->desktop_cur);
        surface->is_outdated = true;
        xcb_flush(surface->connection);
    } else {
        /* No switch happened; restore visibility */
        surface_clients_show(surface, old_id);
    }
}


/* Toggle fullscreen surface mode */
void scmd_surface_toggle_fullscreen(surface_td *surface)
{
    if (surface == NULL) {
        return;
    }

    surface_action_toggle_fullsurface(surface);

}


/* Set the screen resolution */
void scmd_surface_set_resolution(surface_td *surface,
        action_data_surface_td *surface_data)
{
    struct dimensions_s resolution;

    if (surface == NULL || surface_data == NULL) {
        return;
    }

    resolution.w = (uint32_t)(surface_data->new_data.uvalue >> 16);
    resolution.h = (uint32_t)(surface_data->new_data.uvalue & 0xFFFFu);
    surface_action_set_resolution(surface, resolution);
}


/* Set the screen orientation */
void scmd_surface_set_orientation(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    surface_action_set_orientation(surface,
            (int) surface_data->new_data.svalue);
}


/* Set surface brightness */
void scmd_surface_set_brightness(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    surface_action_set_brightness(surface,
            (uint16_t) surface_data->new_data.uvalue);
}


/* Set surface contrast */
void scmd_surface_set_contrast(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    surface_action_set_contrast(surface,
            (uint16_t) surface_data->new_data.uvalue);
}


/* Configure screen settings */
void scmd_surface_configure_settings(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    surface_action_configure_settings(surface);
}
