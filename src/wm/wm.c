/**
 * @file wm/wm.c
 *
 * @brief Window manager singleton lifecycle and query helpers
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
#include <stdio.h>      /* snprintf */
#include <stdint.h>
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy, strlen */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/list.h>

/* Session includes */
#include <session/session.h>

/* Rules includes */
#include <rules/rules.h>

/* Render includes */
#include <render/text.h>

/* Default initial values */
#include <defs/ewmh.h>
#include <defs/wm.h>

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
#include <wm/internal.h>


/* Though variable static dost often lurk near,
 * In shadows of scope, few e'er call thee their own,
 * Thy global existence, to none dost bring fear,
 * A sentinel watching, though thou art alone. */
wm_td *wm = NULL;   /**< Singleton window manager instance */


/**
 * @brief Release every initialized window-manager subsystem
 *
 * Frees only the members that were successfully initialized so it can
 * be used from partial startup failure paths as well as normal
 * teardown.
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces currently stored in @c wm->surfaces
 */
/* Safe cleanup of the wm structure; safe to call at any point */
static void s_wm_cleanup(void)
{
    if (wm == NULL) {
        return;
    }

    if (wm->session != NULL) {
        session_run_hook(wm->session, wm->connection,
                SESSION_HOOK_EXIT);
    }

    text_renderer_destroy();

    if (wm->surfaces != NULL) {
        list_destroy(wm->surfaces);
        wm->surfaces = NULL;
    }

    if (wm->rules != NULL) {
        rules_destroy(wm->rules);
        wm->rules = NULL;
    }

    if (wm->session != NULL) {
        session_destroy(wm->session);
        wm->session = NULL;
    }

    eventq_stop();  /* Safe even if eventq was never started */

    if (wm->config != NULL) {
        config_destroy(wm->config);
        wm->config = NULL;
    }

    if (wm->ewmh != NULL) {
        xcb_ewmh_connection_wipe(wm->ewmh);
        free(wm->ewmh);
        wm->ewmh = NULL;
    }

    if (wm->connection != NULL) {
        if (wm->ewmh_support_win != XCB_NONE) {
            xcb_destroy_window(wm->connection, wm->ewmh_support_win);
            wm->ewmh_support_win = XCB_NONE;
        }
        xcb_disconnect(wm->connection);
        wm->connection = NULL;
    }

    free(wm);
    wm = NULL;
}


/* Initialize a window manager instance */
int wm_start(const char *display_name, const char *config_dir_prefix)
{
    uint32_t screens_detected;
    uint32_t screens_managed;
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

    /* Zero-initialize all pointer fields so s_wm_cleanup can check
     * each one safely during any subsequent error path */
    wm->connection = NULL;
    wm->ewmh = NULL;
    wm->ewmh_support_win = XCB_NONE;
    wm->config = NULL;
    wm->rules = NULL;
    wm->session = NULL;
    wm->surfaces = NULL;
    wm->randr_available = false;
    wm->randr_base_event = 0u;

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
        s_wm_cleanup();
        return 2;
    }

    LOGGER_DEBUG("Allocating memory for EWMH connection", L_NARG);
    wm->ewmh = malloc(sizeof(xcb_ewmh_connection_t));
    if (wm->ewmh == NULL) {
        LOGGER_FATAL("Failed to allocate memory for EWMH connection",
                L_NARG);
        s_wm_cleanup();
        return 1;
    }

    if (!xcb_ewmh_init_atoms_replies(wm->ewmh,
                xcb_ewmh_init_atoms(wm->connection, wm->ewmh),
                NULL)) {
        LOGGER_FATAL("Failed to initialize EWMH atoms", L_NARG);
        s_wm_cleanup();
        return 10;
    }

    wm->config = config_init();
    if (wm->config == NULL) {
        s_wm_cleanup();
        return 3;
    }

    LOGGER_DEBUG("Loading configuration into window manager", L_NARG);
    wm->config_dir_prefix = config_dir_prefix;
    config_load(wm->config, wm->config_dir_prefix);

    wm->rules = rules_init();
    if (wm->rules != NULL) {
        (void) rules_load(wm->rules, wm->config_dir_prefix);
    }

    wm->session = session_init();
    if (wm->session != NULL) {
        (void) session_load(wm->session, wm->config_dir_prefix);
    }

    if (eventq_start() != 0) {
        LOGGER_FATAL("Failed to initialize event queue", L_NARG);
        s_wm_cleanup();
        return 4;
    }

    it = xcb_setup_roots_iterator(xcb_get_setup(wm->connection));
    screens_detected = 0;
    for (; it.rem > 0; xcb_screen_next(&it)) {
        screens_detected++;
    }

    if (screens_detected == 0) {
        LOGGER_FATAL("No screens detected", L_NARG);
        s_wm_cleanup();
        return 5;
    } else {
        LOGGER_INFO("Detected screen %u as preferred", wm->screenp);
    }

    LOGGER_DEBUG("Allocating memory for surface structures", L_NARG);
    wm->surfaces = list_init((void(*)(void *)) surface_destroy);
    if (wm->surfaces == NULL) {
        LOGGER_FATAL("Failed to allocate memory for surfaces array",
                L_NARG);
        s_wm_cleanup();
        return 6;
    }

    LOGGER_DEBUG("Initializing surface structures", L_NARG);
    if (wm->config->base.screen_count != screens_detected) {
        LOGGER_NOTICE("Detected %u screen(s); %u" \
                " specified in the configuration file",
                screens_detected, wm->config->base.screen_count);
        if (wm->config->base.screen_count >= screens_detected ||
                wm->config->base.screen_count == 0) {
            wm->config->base.screen_count = screens_detected;
        }
        LOGGER_DEBUG("Setting number of screens to %u",
                wm->config->base.screen_count);
    } else {
        LOGGER_DEBUG("Setting number of screens to %u",
                wm->config->base.screen_count);
    }

    if (wm->config->base.screen_count > CONFIG_MAX_SCREENS) {
        LOGGER_NOTICE("Configured %u screen(s), but this build supports" \
                " up to %u; clamping",
                wm->config->base.screen_count, CONFIG_MAX_SCREENS);
        wm->config->base.screen_count = CONFIG_MAX_SCREENS;
    }
    screens_managed = wm->config->base.screen_count;

    for (unsigned int i = 0; i < screens_managed; ++i) {
        uint32_t desktops_count =
            wm->config->base.screens[i].desktop_count;
        surface_td *surface =
            surface_init(wm->connection, wm->ewmh,
                    (uint32_t) i, desktops_count, wm->config);
        if (surface == NULL) {
            LOGGER_FATAL("Failed to initialize surface %u", i);
            s_wm_cleanup();
            return 7;
        }

        LOGGER_TRACE("Inserting surface %u into surface list", i);
        if (list_ins_next(wm->surfaces, list_tail(wm->surfaces),
                    (const void *) surface) != 0) {
            LOGGER_FATAL("Failed to insert surface %u" \
                    " into surface list", i);
            surface_destroy(surface);
            s_wm_cleanup();
            return 8;
        }

        surface->desktop_count =
            wm->config->base.screens[i].desktop_count;
        surface->desktop_cur =
            wm->config->base.screens[i].desktop_inaugural;

        LOGGER_TRACE("Setting desktop %u as the startup desktop" \
                " on surface %u", surface->desktop_cur, i);
    }

    if (startup_subscribe_root_events(wm) != 0) {
        s_wm_cleanup();
        return 9;
    }
    (void) startup_randr_init(wm);
    (void) startup_subscribe_randr_events(wm);

    if (wm_ewmh_init() != 0) {
        LOGGER_WARNING("Failed to initialize EWMH root metadata",
                L_NARG);
    }
    wm_ewmh_sync();

    LOGGER_DEBUG("Setting running status flag to 'true'", L_NARG);
    wm->is_running = true;
    if (wm->session != NULL) {
        session_run_hook(wm->session, wm->connection, SESSION_HOOK_START);
    }
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

    LOGGER_DEBUG("Running window manager cleanup", L_NARG);
    s_wm_cleanup();

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


/* Return the managed surface with the given identifier */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    if (wm == NULL || wm->surfaces == NULL) {
        return NULL;
    }

    for (list_item_td *snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);
        if (surface != NULL && surface->id == surface_id) {
            return surface;
        }
    }

    return NULL;
}


/* Mark the client owner desktop and surface as outdated */
void wm_request_client_redraw(const client_td *client)
{
    desktop_td *desktop;

    if (client == NULL || wm == NULL || wm->surfaces == NULL) {
        return;
    }

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        desktop->is_outdated = true;
    }

    for (list_item_td *snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);

        if (surface == NULL) {
            continue;
        }
        if (surface->id == client->screen_id) {
            surface->is_outdated = true;
            break;
        }
    } /* ! for (snode) */
}


/* Mark all surfaces and desktops as outdated */
void wm_request_full_redraw(void)
{
    if (wm == NULL || wm->surfaces == NULL) {
        return;
    }

    for (list_item_td *snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);

        if (surface == NULL) {
            continue;
        }

        surface->is_outdated = true;

        for (uint32_t did = 0; did < surface->desktop_count; ++did) {
            desktop_td *desktop = surface_desktop_get(surface, did);
            if (desktop != NULL) {
                desktop->is_outdated = true;
            }
        }
    } /* ! for (snode) */
}
