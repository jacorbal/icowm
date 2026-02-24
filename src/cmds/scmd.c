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
#include <surface.h>

/* Local includes */
#include <cmds/scmd.h>


/* Add a new desktop */
void scmd_surface_desktop_add(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    /* Logic to add a new desktop */
    // TODO: Implement adding a new desktop
}


/* Remove a desktop */
void scmd_surface_desktop_remove(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    /* Logic to remove a specified desktop */
    // TODO: Implement removal of a desktop
}


/* Switch to another desktop */
void scmd_surface_desktop_switch(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    /* Logic to switch to a specified desktop */
    // TODO: Implement switch to a desktop
}


/* Switch to the next desktop */
void scmd_surface_desktop_switch_next(surface_td *surface)
{
    /* Check if the surface is valid */
    if (surface == NULL) {
        return;
    }

    /* Logic to switch to the next desktop */
    // TODO: Implement switching to the next desktop
}


/* Switch to the previous desktop */
void scmd_surface_desktop_switch_prev(surface_td *surface)
{
    /* Check if the surface is valid */
    if (surface == NULL) {
        return;
    }

    /* Logic to switch to the previous desktop */
    // TODO: Implement switching to the previous desktop
}


/* Toggle fullscreen surface mode */
void scmd_surface_toggle_fullscreen(surface_td *surface)
{
    /* Check if the surface is valid */
    if (surface == NULL) {
        return;
    }

    /* Logic to toggle fullscreen mode */
    // TODO: Implement toggle fullscreen
}


/* Set the screen resolution */
void scmd_surface_set_resolution(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    /* Logic to set screen resolution */
    // TODO: Implement setting of screen resolution
}


/* Set the screen orientation */
void scmd_surface_set_orientation(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    /* Logic to set screen orientation */
    // TODO: Implement setting of screen orientation
}


/* Set surface brightness */
void scmd_surface_set_brightness(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    /* Logic to set the brightness */
    // TODO: Implement setting of brightness
}


/* Set surface contrast */
void scmd_surface_set_contrast(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    /* Logic to set the contrast */
    // TODO: Implement setting of contrast
}


/* Configure screen settings */
void scmd_surface_configure_settings(surface_td *surface,
        action_data_surface_td *surface_data)
{
    /* Check if the surface and data are valid */
    if (surface == NULL || surface_data == NULL) {
        return;
    }

    /* Logic to configure screen settings */
    // TODO: Implement configuration of screen settings
}
