/**
 * @file wm/action.c
 *
 * @brief Window manager action dispatchers implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>

/* Local includes */
#include <wm.h>
#include <wm/internal.h>


/* Reload the configuration */
int wm_action_config_reload(void)
{
    LOGGER_DEBUG("Reloading configuration", L_NARG);

    if (wm == NULL || wm->config == NULL) {
        LOGGER_ERROR("Window manager is not initialized", L_NARG);
        return 1;
    }

    if (config_load(wm->config, wm->config_dir_prefix) != 0) {
        LOGGER_ERROR("Failed to reload configuration", L_NARG);
        return 1;
    }

    for (list_item_td *snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *s = (surface_td *) list_data(snode);
        struct config_base_s *cb = &(wm->config->base);

        if (s->id >= cb->screen_count) {
            continue;
        }

        for (uint32_t i = 0; i < s->desktop_count; ++i) {
            desktop_td *d = surface_desktop_get(s, i);

            if (d == NULL || i >= cb->screens[s->id].desktop_count) {
                continue;
            }

            d->background.is_image = false;
            d->background.bg.color =
                cb->screens[s->id].desktops[i].settings.background.color;
            d->is_outdated = true;
        }

        s->is_outdated = true;
    }

    LOGGER_INFO("Configuration reloaded successfully", L_NARG);
    return 0;
}


/* Save the current configuration */
int wm_action_config_save(void)
{
    LOGGER_DEBUG("Saving configuration", L_NARG);

    if (wm == NULL || wm->config == NULL) {
        LOGGER_ERROR("Window manager is not initialized", L_NARG);
        return 1;
    }

    LOGGER_NOTICE("Configuration saving is not yet implemented", L_NARG);
    return 1;
}


/* Insert a surface into the surface list */
int wm_action_surface_ins(void)
{
    LOGGER_DEBUG("Inserting new surface", L_NARG);

    if (wm == NULL) {
        LOGGER_ERROR("Window manager is not initialized", L_NARG);
        return 1;
    }

    LOGGER_NOTICE("Dynamic surface insertion is not yet implemented",
            L_NARG);
    return 1;
}


/* Remove a surface from the surface list */
int wm_action_surface_rem(void)
{
    LOGGER_DEBUG("Removing surface", L_NARG);

    if (wm == NULL) {
        LOGGER_ERROR("Window manager is not initialized", L_NARG);
        return 1;
    }

    LOGGER_NOTICE("Dynamic surface removal is not yet implemented",
            L_NARG);
    return 1;
}


/* Perform exit actions before stopping the window manager */
int wm_action_exit(void)
{
    LOGGER_DEBUG("Executing exit actions", L_NARG);

    if (wm == NULL) {
        return 1;
    }

    wm_request_stop();
    return 0;
}
