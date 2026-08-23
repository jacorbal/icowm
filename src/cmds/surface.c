/**
 * @file cmds/surface.c
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

/* Type includes */
#include <types/direction.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Menu includes */
#include <menu/notify/desktop.h>

/* Local includes */
#include <cmds/surface.h>


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
    const desktop_td *desktop;

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


/**
 * @brief Switch a surface to the desktop in a given compass
 *        direction, in cyclic order
 *
 * Shared by @c scmd_surface_desktop_switch_north and its three
 * siblings below, which only differ in direction: which of @c
 * surface_desktop_select_north/south/east/west to call, and the log
 * message's own wording.
 *
 * @param surface   Surface to switch
 * @param direction Compass direction to switch toward
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktops involved
 */
static void s_switch_cyclic(surface_td *surface,
        enum compass_direction_e direction)
{
    uint32_t old_id;
    bool cycle;
    const char *direction_label;

    if (surface == NULL) {
        return;
    }

    old_id = surface->desktop_cur;
    cycle = (surface->config != NULL)
        ? surface->config->desktops.wrap_at_bounds
        : true;

    switch (direction) {
    case COMPASS_NORTH:
        direction_label = "north";
        break;
    case COMPASS_SOUTH:
        direction_label = "south";
        break;
    case COMPASS_EAST:
        direction_label = "east";
        break;
    case COMPASS_WEST:
        direction_label = "west";
        break;
    }
    LOGGER_DEBUG("Switching to the desktop %s of the current one" \
            " on surface %u", direction_label, surface->id);

    surface_clients_hide(surface, old_id);
    switch (direction) {
    case COMPASS_NORTH:
        surface_desktop_select_north(surface, cycle);
        break;
    case COMPASS_SOUTH:
        surface_desktop_select_south(surface, cycle);
        break;
    case COMPASS_EAST:
        surface_desktop_select_east(surface, cycle);
        break;
    case COMPASS_WEST:
        surface_desktop_select_west(surface, cycle);
        break;
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


/* Switch to another desktop */
void scmd_surface_desktop_switch(surface_td *surface,
        uint32_t desktop_id)
{
    uint32_t old_id;

    if (surface == NULL) {
        return;
    }

    old_id = surface->desktop_cur;
    if (desktop_id == old_id) {
        return;     /* Already on this desktop */
    }

    LOGGER_DEBUG("Switching desktop: %u to %u on surface %u",
            old_id, desktop_id, surface->id);

    surface_clients_hide(surface, old_id);
    if (surface_desktop_select(surface, desktop_id) != 0) {
        surface_clients_show(surface, old_id);
        return;
    }
    surface_clients_sticky_transfer_all(surface, desktop_id);
    surface_clients_show(surface, desktop_id);

    s_show_desktop_overlay(surface);

    surface->is_outdated = true;
    xcb_flush(surface->connection);
}


/* Switch to the desktop north of the current one */
void scmd_surface_desktop_switch_north(surface_td *surface)
{
    s_switch_cyclic(surface, COMPASS_NORTH);
}


/* Switch to the desktop south of the current one */
void scmd_surface_desktop_switch_south(surface_td *surface)
{
    s_switch_cyclic(surface, COMPASS_SOUTH);
}


/* Switch to the desktop east of the current one */
void scmd_surface_desktop_switch_east(surface_td *surface)
{
    s_switch_cyclic(surface, COMPASS_EAST);
}


/* Switch to the desktop west of the current one */
void scmd_surface_desktop_switch_west(surface_td *surface)
{
    s_switch_cyclic(surface, COMPASS_WEST);
}
