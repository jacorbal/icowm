/**
 * @file wm/ewmh.c
 *
 * @brief Window manager EWMH initialization and synchronization
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
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* NULL, calloc, free, malloc */
#include <string.h>     /* memcpy, strlen */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Default initial values */
#include <defs/ewmh.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <wm.h>
#include <wm/internal.h>


/* Compute and publish '_NET_WORKAREA' for one managed surface */
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


/**
 * @brief Compute and publish @c _NET_DESKTOP_NAMES for one surface
 *
 * Builds a NUL-separated UTF-8 list with every desktop name of the
 * target surface and writes it to the root-window EWMH property.
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
static void s_wm_sync_desktop_names(surface_td *surface)
{
    uint32_t did;
    size_t names_len;
    size_t offset;
    char *names;

    if (surface == NULL || surface->ewmh == NULL ||
            surface->desktop_count == 0u) {
        return;
    }

    names_len = 0u;
    for (did = 0u; did < surface->desktop_count; ++did) {
        desktop_td *desktop = surface_desktop_get(surface, did);
        if (desktop != NULL && desktop->name[0] != '\0') {
            names_len += strlen(desktop->name) + 1u;
        } else {
            char fallback_name[32];
            int written = snprintf(fallback_name, sizeof(fallback_name),
                    "Desktop %u", did + 1u);
            if (written <= 0) {
                continue;
            }
            names_len += (size_t) written + 1u;
        }
    }

    if (names_len == 0u || names_len > UINT32_MAX) {
        return;
    }

    names = calloc(names_len, sizeof(char));
    if (names == NULL) {
        return;
    }

    offset = 0u;
    for (did = 0u; did < surface->desktop_count; ++did) {
        const char *name;
        size_t name_len;
        desktop_td *desktop = surface_desktop_get(surface, did);
        char fallback_name[32];

        if (desktop != NULL && desktop->name[0] != '\0') {
            name = desktop->name;
        } else {
            int written = snprintf(fallback_name, sizeof(fallback_name),
                    "Desktop %u", did + 1u);
            if (written <= 0) {
                continue;
            }
            fallback_name[sizeof(fallback_name) - 1u] = '\0';
            name = fallback_name;
        }

        name_len = strlen(name);
        if (offset + name_len + 1u > names_len) {
            break;
        }

        memcpy(names + offset, name, name_len + 1u);
        offset += name_len + 1u;
    }

    if (offset > 0u) {
        xcb_ewmh_set_desktop_names(surface->ewmh, (int) surface->id,
                (uint32_t) offset, names);
    }
    free(names);
}


/* Create and publish root EWMH metadata required by compliant clients */
int wm_ewmh_init(void)
{
    xcb_atom_t supported_atoms[WM_EWMH_SUPPORTED_COUNT];
    uint32_t n_supported = 0u;
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
            sizeof(WM_EWMH_NAME) - 1u, WM_EWMH_NAME);
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
    supported_atoms[n_supported++] = wm->ewmh->_NET_DESKTOP_NAMES;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STRUT_PARTIAL;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STRUT;
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

    for (list_item_td *snode = list_head(wm->surfaces);
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
    if (wm == NULL || wm->surfaces == NULL || wm->ewmh == NULL) {
        return;
    }

    for (list_item_td *snode = list_head(wm->surfaces);
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
        s_wm_sync_desktop_names(surface);
        s_wm_sync_workarea(surface);
        s_wm_sync_client_lists(surface);
    }

    xcb_flush(wm->connection);
}
