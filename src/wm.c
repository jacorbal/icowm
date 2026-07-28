/**
 * @file wm.c
 *
 * @brief Window manager public API
 *
 * Owns the singleton @c wm_td instance and implements the public
 * functions declared in @c wm.h.  All internal subsystem logic has
 * been moved to dedicated modules (loop, startup, handler, lifecycle,
 * lookup, focus, drag, keyboard, mouse, menu/cycle, menu/popup).
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
#include <stdlib.h>     /* NULL, free, malloc */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/list.h>

/* Render includes */
#include <render/text.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <eventq.h>
#include <logger.h>
#include <lookup.h>
#include <loop.h>
#include <startup.h>
#include <surface.h>

/* Local includes */
#include <wm.h>


/* Though variable static dost often lurk near,
 * In shadows of scope, few e’er call thee their own,
 * Thy global existence, to none dost bring fear,
 * A sentinel watching, though thou art alone. */
static wm_td *wm = NULL;    /**< Pointer to the singleton instance of
                                 the window manager */


/* Initialize a window manager instance */
int wm_start(const char *display_name, const char *config_dir_prefix)
{
    uint32_t screens_detected;
    xcb_screen_iterator_t it;

    LOGGER_DEBUG("Initializing window manager", L_NARG);

    if (wm != NULL) {
        return -1;
    }

    wm = malloc(sizeof(wm_td));
    if (wm == NULL) {
        LOGGER_FATAL("Failed to allocate memory for window manager",
                L_NARG);
        return 1;
    }

    LOGGER_DEBUG("Opening X display", L_NARG);
    wm->connection = xcb_connect(display_name,
            (int *) &(wm->screenp));

    if (xcb_connection_has_error(wm->connection)) {
        if (display_name == NULL) {
            LOGGER_FATAL("Failed to open X display", L_NARG);
        } else {
            LOGGER_FATAL("Failed to open X display '%s'",
                    display_name);
        }
        free(wm);
        wm = NULL;
        return 2;
    }

    LOGGER_TRACE("Allocating memory for EWMH connection", L_NARG);
    wm->ewmh = malloc(sizeof(xcb_ewmh_connection_t));
    if (wm->ewmh == NULL) {
        LOGGER_FATAL("Error allocating memory for EWMH connection",
                L_NARG);
        xcb_disconnect(wm->connection);
        free(wm);
        wm = NULL;
        return 1;
    }

    if (!xcb_ewmh_init_atoms_replies(wm->ewmh,
                xcb_ewmh_init_atoms(wm->connection, wm->ewmh),
                NULL)) {
        LOGGER_ERROR("Error initializating EWMH atoms", L_NARG);
    }

    wm->config = config_init();
    if (wm->config == NULL) {
        xcb_disconnect(wm->connection);
        xcb_ewmh_connection_wipe(wm->ewmh);
        free(wm->ewmh);
        free(wm);
        wm = NULL;
        return 3;
    }

    LOGGER_TRACE("Loading configuration into window manager", L_NARG);
    wm->config_dir_prefix = config_dir_prefix;
    config_load(wm->config, wm->config_dir_prefix);

    if (eventq_start() != 0) {
        LOGGER_FATAL("Failed to initialize event priority queue",
                L_NARG);
        config_destroy(wm->config);
        xcb_disconnect(wm->connection);
        xcb_ewmh_connection_wipe(wm->ewmh);
        free(wm->ewmh);
        free(wm);
        wm = NULL;
        return 4;
    }

    it = xcb_setup_roots_iterator(xcb_get_setup(wm->connection));
    screens_detected = 0;
    for (; it.rem > 0; xcb_screen_next(&it)) {
        screens_detected++;
    }

    if (screens_detected == 0) {
        LOGGER_FATAL("No screens detected", L_NARG);
        config_destroy(wm->config);
        eventq_stop();
        xcb_disconnect(wm->connection);
        xcb_ewmh_connection_wipe(wm->ewmh);
        free(wm->ewmh);
        free(wm);
        wm = NULL;
        return 5;
    } else {
        LOGGER_INFO("Detected screen %u as preferred", wm->screenp);
    }

    LOGGER_TRACE("Allocating memory for surface structures", L_NARG);
    wm->surfaces = list_init((void(*)(void *)) surface_destroy);
    if (wm->surfaces == NULL) {
        LOGGER_FATAL("Failed to allocate memory for surfaces array",
                L_NARG);
        eventq_stop();
        config_destroy(wm->config);
        xcb_disconnect(wm->connection);
        xcb_ewmh_connection_wipe(wm->ewmh);
        free(wm->ewmh);
        free(wm);
        wm = NULL;
        return 5;
    }

    LOGGER_TRACE("Initializing surface structures", L_NARG);
    if (wm->config->base.screen_count != screens_detected) {
        LOGGER_NOTICE("Detected %u screen(s); %u" \
                " specified in the configuration file",
                screens_detected, wm->config->base.screen_count);
        if (wm->config->base.screen_count >= screens_detected ||
                wm->config->base.screen_count == 0) {
            wm->config->base.screen_count = screens_detected;
        }
        LOGGER_INFO("Setting number of screens to %u",
                wm->config->base.screen_count);
    } else {
        LOGGER_DEBUG("Setting number of screens to %u",
                wm->config->base.screen_count);
    }

    for (unsigned int i = 0; i < screens_detected; ++i) {
        uint32_t desktops_count =
            wm->config->base.screens[i].desktop_count;
        surface_td *surface =
            surface_init(wm->connection, wm->ewmh,
                    (uint32_t) i, desktops_count, wm->config);
        if (surface == NULL) {
            LOGGER_FATAL("Failed to initialize surface %u", i);
            list_destroy(wm->surfaces);
            eventq_stop();
            config_destroy(wm->config);
            xcb_disconnect(wm->connection);
            xcb_ewmh_connection_wipe(wm->ewmh);
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 6;
        }

        LOGGER_TRACE("Inserting surface %u into surface list", i);
        if (list_ins_next(wm->surfaces, list_tail(wm->surfaces),
                    (const void *) surface) != 0) {
            LOGGER_FATAL("Failed to insert surface %u" \
                    " into surface list", i);
            surface_destroy(surface);
            list_destroy(wm->surfaces);
            eventq_stop();
            config_destroy(wm->config);
            xcb_disconnect(wm->connection);
            xcb_ewmh_connection_wipe(wm->ewmh);
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 7;
        }

        surface->desktop_count =
            wm->config->base.screens[i].desktop_count;
        surface->desktop_cur =
            wm->config->base.screens[i].desktop_inaugural;

        LOGGER_TRACE("Setting desktop %u as the startup desktop" \
                " on surface %u", surface->desktop_cur, i);
    }

    if (startup_subscribe_root_events(wm) != 0) {
        list_destroy(wm->surfaces);
        eventq_stop();
        config_destroy(wm->config);
        xcb_disconnect(wm->connection);
        xcb_ewmh_connection_wipe(wm->ewmh);
        free(wm->ewmh);
        free(wm);
        wm = NULL;
        return 8;
    }

    LOGGER_TRACE("Setting running status flag to 'true'", L_NARG);
    wm->is_running = true;
    loop_run(wm);

    return 0;
}


/* Destroy window manager instance */
int wm_stop(void)
{
    LOGGER_DEBUG("Deallocating structure for window manager", L_NARG);

    if (wm == NULL) {
        return 1;
    }

    eventq_stop();

    LOGGER_TRACE("Deallocating surfaces in window manager", L_NARG);
    list_destroy(wm->surfaces);

    LOGGER_TRACE("Deallocating EWMH structure", L_NARG);
    xcb_ewmh_connection_wipe(wm->ewmh);
    free(wm->ewmh);

    config_destroy(wm->config);
    text_renderer_destroy();

    LOGGER_TRACE("Closing X display", L_NARG);
    xcb_disconnect(wm->connection);

    LOGGER_TRACE("Destroying window manager", L_NARG);
    free(wm);
    wm = NULL;

    LOGGER_DEBUG("Window manager has been destroyed", L_NARG);

    return 0;
}


/* Request a graceful stop of the main window manager loop */
int wm_request_stop(void)
{
    if (wm == NULL) {
        return 1;
    }

    wm->is_running = false;
    return 0;
}


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


/* Retrieve the desktop that currently contains the given client */
desktop_td *wm_get_client_desktop(const client_td *client)
{
    desktop_td *desktop = NULL;

    if (client == NULL || wm == NULL) {
        return NULL;
    }

    (void) lookup_find_client(wm->surfaces, client->id, NULL, &desktop);
    return desktop;
}
