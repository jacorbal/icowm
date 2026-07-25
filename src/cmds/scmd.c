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


/**
 * @brief Switch the current desktop in a given direction on a surface
 *
 * Hides the current desktop's clients, calls @p select_fn to move the
 * selection, and shows the new desktop's clients if the selection
 * changed.  If the selection did not change (e.g., only one desktop or
 * wrap disabled), visibility is restored.
 *
 * @param surface   Target surface; no-op if @c NULL
 * @param select_fn Direction function
 * @param direction Human-readable direction label for the log message
 *
 * @note Complexity: @e O(1) amortised, for desktop list traversal is
 *       bounded by the number of desktops
 *
 * @see @a surface_desktop_select_next and @a surface_desktop_select_prev
 */
static void s_surface_desktop_switch_direction(surface_td *surface,
        int (*select_fn)(surface_td *, bool), const char *direction)
{
    uint32_t old_id;

    if (surface == NULL) {
        return;
    }

    old_id = surface->desktop_cur;

    LOGGER_DEBUG("Switching to %s desktop on surface %u",
            direction, surface->id);

    surface_clients_hide(surface, old_id);
    select_fn(surface, true);

    if (surface->desktop_cur != old_id) {
        surface_clients_show(surface, surface->desktop_cur);
        surface->is_outdated = true;
        xcb_flush(surface->connection);
    } else {
        /* No switch happened; restore visibility */
        surface_clients_show(surface, old_id);
    }
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

    LOGGER_DEBUG("Switching desktop from %u to %u on surface %u",
            old_id, new_id, surface->id);

    surface_clients_hide(surface, old_id);
    surface_desktop_select(surface, new_id);
    surface_clients_show(surface, new_id);

    surface->is_outdated = true;
    xcb_flush(surface->connection);
}


/* Switch to the next desktop */
void scmd_surface_desktop_switch_next(surface_td *surface)
{
    s_surface_desktop_switch_direction(surface,
            surface_desktop_select_next, "next");
}


/* Switch to the previous desktop */
void scmd_surface_desktop_switch_prev(surface_td *surface)
{
    s_surface_desktop_switch_direction(surface,
            surface_desktop_select_prev, "previous");
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

    resolution.w = (uint32_t) (surface_data->new_data.uvalue >> 16);
    resolution.h = (uint32_t) (surface_data->new_data.uvalue & 0xFFFFu);
    surface_action_set_resolution(surface, resolution);
}


/* Set the screen orientation */
void scmd_surface_set_orientation(surface_td *surface,
        action_data_surface_td *surface_data)
{
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
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    surface_action_configure_settings(surface);
}
