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

/* Default initial values */
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


/* Though variable static dost often lurk near,
 * In shadows of scope, few e’er call thee their own,
 * Thy global existence, to none dost bring fear,
 * A sentinel watching, though thou art alone. */
static wm_td *wm = NULL;    /**< Pointer to the singleton instance of
                                 the window manager */


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
static void s_wm_cleanup(void)
{
    if (wm == NULL) {
        return;
    }

    if (wm->surfaces != NULL) {
        list_destroy(wm->surfaces);
        wm->surfaces = NULL;
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


/* Compute and publish _NET_WORKAREA for one managed surface */
static void s_wm_sync_workarea(surface_td *surface)
{
    uint32_t did;
    xcb_ewmh_geometry_t *workareas;

    if (surface == NULL || surface->ewmh == NULL ||
            surface->desktop_count == 0) {
        return;
    }

    workareas = malloc(sizeof(xcb_ewmh_geometry_t) *
            surface->desktop_count);
    if (workareas == NULL) {
        return;
    }

    for (did = 0; did < surface->desktop_count; ++did) {
        desktop_td *desktop = surface_desktop_get(surface, did);

        if (desktop == NULL) {
            workareas[did].x = 0u;
            workareas[did].y = 0u;
            workareas[did].width = surface->properties.dim.w;
            workareas[did].height = surface->properties.dim.h;
            continue;
        }

        workareas[did].x = (desktop->workarea.pos.x > 0)
            ? (uint32_t) desktop->workarea.pos.x
            : 0u;
        workareas[did].y = (desktop->workarea.pos.y > 0)
            ? (uint32_t) desktop->workarea.pos.y
            : 0u;
        workareas[did].width = desktop->workarea.dim.w;
        workareas[did].height = desktop->workarea.dim.h;
    }

    xcb_ewmh_set_workarea(surface->ewmh, (int) surface->id,
            (uint32_t) surface->desktop_count, workareas);
    free(workareas);
}


/* Compute and publish '_NET_CLIENT_LIST*' for one managed surface */
static void s_wm_sync_client_lists(surface_td *surface)
{
    uint32_t did;
    size_t total_clients = 0u;
    size_t idx = 0u;
    xcb_window_t *client_list;
    xcb_window_t *stacking_list;

    if (surface == NULL || surface->ewmh == NULL) {
        return;
    }

    for (did = 0; did < surface->desktop_count; ++did) {
        desktop_td *desktop = surface_desktop_get(surface, did);

        if (desktop != NULL && desktop->clients != NULL) {
            total_clients += ohtbl_size(desktop->clients);
        }
    }

    if (total_clients == 0u) {
        xcb_ewmh_set_client_list(surface->ewmh, (int) surface->id,
                0u, NULL);
        xcb_ewmh_set_client_list_stacking(surface->ewmh,
                (int) surface->id, 0u, NULL);
        return;
    }

    client_list = malloc(sizeof(xcb_window_t) * total_clients);
    stacking_list = malloc(sizeof(xcb_window_t) * total_clients);
    if (client_list == NULL || stacking_list == NULL) {
        free(client_list);
        free(stacking_list);
        return;
    }

    for (did = 0; did < surface->desktop_count; ++did) {
        void *elem;

        desktop_td *desktop = surface_desktop_get(surface, did);
        if (desktop == NULL || desktop->clients == NULL) {
            continue;
        }

        ohtbl_foreach(desktop->clients, elem) {
            client_td *client = (client_td *) elem;

            if (client->window != XCB_NONE && idx < total_clients) {
                client_list[idx++] = client->window;
            }
        }
    }

    xcb_ewmh_set_client_list(surface->ewmh, (int) surface->id,
            (uint32_t) idx, client_list);

    idx = 0u;
    for (did = 0; did < surface->desktop_count; ++did) {
        desktop_td *desktop = surface_desktop_get(surface, did);
        cdlist_item_td *node;
        cdlist_item_td *initial;

        if (desktop == NULL || desktop->stacking == NULL ||
                cdlist_size(desktop->stacking) == 0) {
            continue;
        }

        node = cdlist_head(desktop->stacking);
        initial = node;
        if (node == NULL) {
            continue;
        }

        do {
            client_td *client = (client_td *) cdlist_data(node);
            if (client != NULL && client->window != XCB_NONE &&
                    idx < total_clients) {
                stacking_list[idx++] = client->window;
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    xcb_ewmh_set_client_list_stacking(surface->ewmh,
            (int) surface->id, (uint32_t) idx, stacking_list);

    free(client_list);
    free(stacking_list);
}


/* Create and publish root EWMH metadata required by compliant clients */
int wm_ewmh_init(void)
{
    xcb_atom_t supported_atoms[24];
    uint32_t n_supported = 0u;
    list_item_td *snode;
    xcb_window_t support;

    if (wm == NULL || wm->connection == NULL || wm->ewmh == NULL) {
        return 1;
    }

    support = xcb_generate_id(wm->connection);
    xcb_create_window(wm->connection,
            XCB_COPY_FROM_PARENT,
            support,
            xcb_setup_roots_iterator(xcb_get_setup(
                        wm->connection)).data->root,
            0, 0, 1, 1,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            0, NULL);
    wm->ewmh_support_win = support;

    xcb_ewmh_set_wm_name(wm->ewmh, support,
            sizeof(ICOWM_EWMH_NAME) - 1u, ICOWM_EWMH_NAME);
    xcb_change_property(wm->connection, XCB_PROP_MODE_REPLACE,
            support, wm->ewmh->_NET_SUPPORTING_WM_CHECK,
            XCB_ATOM_WINDOW, 32, 1, &support);

    supported_atoms[n_supported++] = wm->ewmh->_NET_SUPPORTED;
    supported_atoms[n_supported++] = wm->ewmh->_NET_SUPPORTING_WM_CHECK;
    supported_atoms[n_supported++] = wm->ewmh->_NET_CLIENT_LIST;
    supported_atoms[n_supported++] = wm->ewmh->_NET_CLIENT_LIST_STACKING;
    supported_atoms[n_supported++] = wm->ewmh->_NET_NUMBER_OF_DESKTOPS;
    supported_atoms[n_supported++] = wm->ewmh->_NET_CURRENT_DESKTOP;
    supported_atoms[n_supported++] = wm->ewmh->_NET_DESKTOP_GEOMETRY;
    supported_atoms[n_supported++] = wm->ewmh->_NET_DESKTOP_VIEWPORT;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WORKAREA;
    supported_atoms[n_supported++] = wm->ewmh->_NET_ACTIVE_WINDOW;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_NAME;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ICON_NAME;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_DESKTOP;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_HIDDEN;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_FULLSCREEN;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_MAXIMIZED_VERT;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_MAXIMIZED_HORZ;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_ABOVE;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_BELOW;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_STICKY;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_SHADED;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION;
    supported_atoms[n_supported++] = wm->ewmh->_NET_CLOSE_WINDOW;

    for (snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);
        if (surface == NULL || surface->screen == NULL) {
            continue;
        }
        xcb_ewmh_set_supporting_wm_check(wm->ewmh,
                surface->screen->root, support);
        xcb_ewmh_set_supported(wm->ewmh, (int) surface->id,
                n_supported, supported_atoms);
    }

    xcb_flush(wm->connection);
    return 0;
}


/* Synchronize EWMH root properties for all managed surfaces */
void wm_ewmh_sync(void)
{
    list_item_td *snode;

    if (wm == NULL || wm->surfaces == NULL || wm->ewmh == NULL) {
        return;
    }

    for (snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);
        desktop_td *current;
        xcb_window_t active = XCB_NONE;
        xcb_ewmh_coordinates_t *viewport;

        if (surface == NULL) {
            continue;
        }

        xcb_ewmh_set_number_of_desktops(surface->ewmh,
                (int) surface->id, surface->desktop_count);
        xcb_ewmh_set_current_desktop(surface->ewmh,
                (int) surface->id, surface->desktop_cur);
        xcb_ewmh_set_desktop_geometry(surface->ewmh, (int) surface->id,
                surface->properties.dim.w, surface->properties.dim.h);
        viewport = calloc(surface->desktop_count,
                sizeof(xcb_ewmh_coordinates_t));
        if (viewport != NULL) {
            xcb_ewmh_set_desktop_viewport(surface->ewmh,
                    (int) surface->id, surface->desktop_count,
                    viewport);
            free(viewport);
        }

        current = surface_desktop_get(surface, surface->desktop_cur);
        if (current != NULL && current->client_active_id != XCB_NONE) {
            client_td *active_client = lookup_find_client(wm->surfaces,
                    current->client_active_id, NULL, NULL);
            if (active_client != NULL) {
                active = active_client->window;
            }
        }

        xcb_ewmh_set_active_window(surface->ewmh, (int) surface->id,
                active);
        s_wm_sync_workarea(surface);
        s_wm_sync_client_lists(surface);
    }

    xcb_flush(wm->connection);
}


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

    /* Zero-initialize all pointer fields so s_wm_cleanup can check
     * each one safely during any subsequent error path. */
    wm->connection = NULL;
    wm->ewmh = NULL;
    wm->ewmh_support_win = XCB_NONE;
    wm->config = NULL;
    wm->surfaces = NULL;

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
        wm->connection = NULL;  /* 'xcb_disconnect' not needed on error */
        s_wm_cleanup();
        return 2;
    }

    LOGGER_TRACE("Allocating memory for EWMH connection", L_NARG);
    wm->ewmh = malloc(sizeof(xcb_ewmh_connection_t));
    if (wm->ewmh == NULL) {
        LOGGER_FATAL("Error allocating memory for EWMH connection",
                L_NARG);
        s_wm_cleanup();
        return 1;
    }

    if (!xcb_ewmh_init_atoms_replies(wm->ewmh,
                xcb_ewmh_init_atoms(wm->connection, wm->ewmh),
                NULL)) {
        LOGGER_ERROR("Error initializating EWMH atoms", L_NARG);
    }

    wm->config = config_init();
    if (wm->config == NULL) {
        s_wm_cleanup();
        return 3;
    }

    LOGGER_TRACE("Loading configuration into window manager", L_NARG);
    wm->config_dir_prefix = config_dir_prefix;
    config_load(wm->config, wm->config_dir_prefix);

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

    LOGGER_TRACE("Allocating memory for surface structures", L_NARG);
    wm->surfaces = list_init((void(*)(void *)) surface_destroy);
    if (wm->surfaces == NULL) {
        LOGGER_FATAL("Failed to allocate memory for surfaces array",
                L_NARG);
        s_wm_cleanup();
        return 6;
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

    if (wm_ewmh_init() != 0) {
        LOGGER_WARNING("Failed to initialize EWMH root metadata",
                L_NARG);
    }
    wm_ewmh_sync();

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
    if (wm->ewmh_support_win != XCB_NONE) {
        xcb_destroy_window(wm->connection, wm->ewmh_support_win);
        wm->ewmh_support_win = XCB_NONE;
    }
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


/* Return the managed surface with the given identifier */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    list_item_td *snode;
    if (wm == NULL || wm->surfaces == NULL) {
        return NULL;
    }
    for (snode = list_head(wm->surfaces);
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
    }
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
    }
}
