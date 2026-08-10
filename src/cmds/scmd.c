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
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Menu includes */
#include <menu/notify/desktop.h>

/* Local includes */
#include <cmds/scmd.h>


/**
 * @brief Show the desktop-switch notification for the current desktop
 *
 * Retrieves the desktop name from the active desktop and passes it to
 * the notify module together with the new desktop index.  Does nothing
 * when no config is available on the surface.
 *
 * @param surface Surface whose current desktop just became active
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       the surface
 */
static void s_show_desktop_overlay(surface_td *surface)
{
    desktop_td *desktop;

    if (surface == NULL || surface->connection == NULL ||
            surface->config == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    notify_desktop_show(surface->connection, surface,
            surface->desktop_cur,
            (desktop != NULL) ? desktop->name : "",
            surface->config);
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
    uint32_t old_id;
    uint32_t new_id;

    if (surface == NULL || surface_data == NULL) {
        return;
    }

    new_id = surface_data->new_data.uvalue;
    old_id = surface->desktop_cur;
    if (new_id == old_id) {
        return;     /* Already on this desktop */
    }

    LOGGER_DEBUG("Switching desktop: %u to %u on surface %u",
            old_id, new_id, surface->id);

    surface_clients_hide(surface, old_id);
    if (surface_desktop_select(surface, new_id) != 0) {
        surface_clients_show(surface, old_id);
        return;
    }
    surface_clients_sticky_transfer_all(surface, new_id);
    surface_clients_show(surface, new_id);

    s_show_desktop_overlay(surface);

    surface->is_outdated = true;
    xcb_flush(surface->connection);
}


/**
 * @brief Switch a surface to the next or previous desktop, in
 *        cyclic order
 *
 * Shared by @c scmd_surface_desktop_switch_next and @c scmd_surface_
 * desktop_switch_prev below, which only differ in direction: which
 * of @c surface_desktop_select_next/prev to call, and the log
 * message's own wording.
 *
 * @param surface Surface to switch
 * @param forward @c true to advance to the next desktop, @c false to
 *                go back to the previous one
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktops involved
 */
static void s_switch_cyclic(surface_td *surface, bool forward)
{
    uint32_t old_id;
    bool cycle;

    if (surface == NULL) {
        return;
    }

    old_id = surface->desktop_cur;
    cycle = (surface->config != NULL) ? surface->config->desktop.cycle
                                       : true;

    LOGGER_DEBUG("Switching to %s desktop on surface %u",
            (forward) ? "next" : "previous", surface->id);

    surface_clients_hide(surface, old_id);
    if (forward) {
        surface_desktop_select_next(surface, cycle);
    } else {
        surface_desktop_select_prev(surface, cycle);
    }

    if (surface->desktop_cur != old_id) {
        surface_clients_sticky_transfer_all(surface,
                surface->desktop_cur);
        surface_clients_show(surface, surface->desktop_cur);
        s_show_desktop_overlay(surface);
        surface->is_outdated = true;
        xcb_flush(surface->connection);
    } else {
        /* No switch happened; restore visibility */
        surface_clients_show(surface, old_id);
    }
}


/* Switch to the next desktop */
void scmd_surface_desktop_switch_next(surface_td *surface)
{
    s_switch_cyclic(surface, true);
}


/* Switch to the previous desktop */
void scmd_surface_desktop_switch_prev(surface_td *surface)
{
    s_switch_cyclic(surface, false);
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
