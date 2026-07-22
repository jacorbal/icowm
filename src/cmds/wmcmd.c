/**
 * @file cmds/wmcmd.c
 *
 * @brief Implementation of actions related to the windowm manager
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
#include <stddef.h>

/* Project includes */
#include <wm.h>

/* Local includes */
#include <cmds/wmcmd.h>


/* Reload current configuration */
bool wmcmd_configuration_reload(wm_td *wm)
{
    if (wm == NULL) {
        return false;
    }

    return (wm_action_config_reload() == 0);
}


/* Save current configuration */
bool wmcmd_configuration_save(wm_td *wm)
{
        if (wm == NULL) {
        return false;
    }

    return (wm_action_config_save() == 0);
}


/* Add new surface */
void wmcmd_surface_add(wm_td *wm, action_data_wm_td *wm_data)
{
    if (wm == NULL) {
        return;
    }

    (void) wm_data;
    wm_action_surface_ins();
}


/* Remove surface */
void wmcmd_surface_remove(wm_td *wm, action_data_wm_td *wm_data)
{
    if (wm == NULL) {
        return;
    }

    (void) wm_data;
    wm_action_surface_rem();
}


/* Prepare to exit the window manager */
void wmcmd_exit(wm_td *wm)
{
    if (wm == NULL) {
        return;
    }

    wm_request_stop();
}
