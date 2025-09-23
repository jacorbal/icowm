/**
 * @file cmds/wmcmd.c
 *
 * @brief Implementation of actions related to the windowm manager
 */
/*
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stddef.h>

/* Project includes */
#include <wm.h>

/* Local includes */
#include <cmds/wmcmd.h>


/* Reload current configuration */
bool wmcmd_configuration_reload(wm_td *wm)
{
    /* TODO: Implement logic to reload the current configuration */

    return true;
}


/* Save current configuration */
bool wmcmd_configuration_save(wm_td *wm)
{
    /* TODO: Implement logic to save the current configuration */

    return true;
}


/* Add new surface */
void wmcmd_surface_add(wm_td *wm, action_data_wm_td *wm_data)
{
    /* TODO: Implement logic to add a new surface */
}


/* Remove surface */
void wmcmd_surface_remove(wm_td *wm, action_data_wm_td *wm_data)
{
    /* TODO: Implement logic to remove a surface */
}


/* Prepare to exit the window manager */
void wmcmd_exit(wm_td *wm)
{
    /* TODO: Implement logic to cleanly exit the window manager */
}
