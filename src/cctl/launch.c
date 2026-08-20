/**
 * @file cctl/launch.c
 *
 * @brief Program launch dispatch implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdio.h>      /* snprintf, NULL */

/* Project includes */
#include <desktop.h>
#include <lookup.h>
#include <surface.h>

/* Default initial values */
#include <defs/uistr.h>

/* Project includes */
#include <i18n.h>

/* Menu includes */
#include <menu/dialog/info.h>

/* Local includes */
#include <cctl/launch.h>


/* Build and enqueue a launch event for a desktop */
void cctl_launch_dispatch(surface_td *surface, const char *restrict prog,
        const char *restrict class_name)
{
    desktop_td *desktop;
    int result;

    if (surface == NULL || prog == NULL || prog[0] == '\0') {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }
    if (class_name != NULL && class_name[0] != '\0') {
        result = desktop_action_process_launch_with_class(desktop,
                prog, class_name, NULL);
    } else {
        result = desktop_action_process_launch(desktop, prog);
    }
    if (result == -2 && surface->connection != NULL &&
            surface->config != NULL) {
        char msg[256];

        (void) snprintf(msg, sizeof(msg),
                _(STR_LAUNCH_COMMAND_NOT_FOUND_FMT), prog);
        dialog_info_show(surface->connection, surface,
                surface->config, msg, MENU_MSG_LEVEL_WARNING);
    }
}
