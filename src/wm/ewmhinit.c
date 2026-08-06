/**
 * @file wm/ewmhinit.c
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
#include <string.h>     /* memcpy */
#include <time.h>       /* time */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/ewmh.h>
#include <defs/wm.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>

/* Local includes */
#include <wm.h>
#include <wm/internal.h>


/**
 * @brief Compute and publish @c _NET_WORKAREA for one managed surface
 *
 * Builds an array of workarea rectangles, one per desktop on the given
 * surface, and writes it to the @c _NET_WORKAREA root property.
 *
 * @param surface Pointer to the target surface
 *
 * @note Desktops without a valid work area fall back to the full
 *       surface geometry
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
static void s_wm_sync_workarea(surface_td *surface)
{
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

    for (uint32_t did = 0; did < surface->desktop_count; ++did) {
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


/**
 * @brief Compute and publish @c _NET_DESKTOP_LAYOUT for one surface
 *
 * Builds a fixed 4-element layout descriptor (orientation, columns,
 * rows, starting corner) describing the desktops as a single horizontal
 * row, and writes it to the @c _NET_DESKTOP_LAYOUT root property.
 *
 * @param surface Pointer to the target surface
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_sync_desktop_layout(surface_td *surface)
{
    uint32_t layout[4];

    if (surface == NULL || surface->connection == NULL ||
            surface->screen == NULL || surface->ewmh == NULL) {
        return;
    }

    /* Orientation=0(horizontal), cols=n, rows=1, corner=0(top-left) */
    layout[0] = 0u;
    layout[1] = (surface->desktop_count > 0u)
        ? surface->desktop_count : 1u;
    layout[2] = 1u;
    layout[3] = 0u;

    xcb_change_property(surface->connection, XCB_PROP_MODE_REPLACE,
            surface->screen->root, surface->ewmh->_NET_DESKTOP_LAYOUT,
            XCB_ATOM_CARDINAL, 32, 4, layout);
}


/**
 * @brief Compute and publish @c _NET_CLIENT_LIST and its stacking
 *        variant
 *
 * Collects the windows of every managed client across all desktops of
 * the surface, publishing them via @c _NET_CLIENT_LIST in insertion
 * order and via @c _NET_CLIENT_LIST_STACKING in bottom-to-top stacking
 * order.  Also updates each client's @c _NET_WM_DESKTOP property, using
 * the special "all desktops" value for sticky clients.
 *
 * @param surface Pointer to the target surface
 *
 * @note Complexity: @e O(n), where @e n is the total number of managed
 *       clients across all desktops
 */
static void s_wm_sync_client_lists(surface_td *surface)
{
    size_t total_clients = 0u;
    size_t idx = 0u;
    xcb_window_t *client_list;
    xcb_window_t *stacking_list;

    if (surface == NULL || surface->ewmh == NULL) {
        return;
    }

    for (uint32_t did = 0; did < surface->desktop_count; ++did) {
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

    for (uint32_t did = 0; did < surface->desktop_count; ++did) {
        void *elem;
        uint32_t did_prop;

        desktop_td *desktop = surface_desktop_get(surface, did);
        if (desktop == NULL || desktop->clients == NULL) {
            continue;
        }

        ohtbl_foreach(desktop->clients, elem) {
            client_td *client = (client_td *) elem;

            if (client->window != XCB_NONE && idx < total_clients) {
                client_list[idx++] = client->window;

                /* Publish '_NET_WM_DESKTOP' so taskbars and pagers can
                 * associate each window with the correct desktop */
                did_prop = (client->properties.flags & CLIENT_FLAG_STICKY)
                    ? WM_DESKTOP_ID_ALL : desktop->id;
                xcb_change_property(surface->connection,
                        XCB_PROP_MODE_REPLACE,
                        client->window,
                        surface->ewmh->_NET_WM_DESKTOP,
                        XCB_ATOM_CARDINAL, 32, 1, &did_prop);
            }
        } /* ! ohtbl_foreach */
    }

    xcb_ewmh_set_client_list(surface->ewmh, (int) surface->id,
            (uint32_t) idx, client_list);

    idx = 0u;
    for (uint32_t did = 0; did < surface->desktop_count; ++did) {
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
 * Builds a null-separated UTF-8 list with every desktop name of the
 * target surface and writes it to the root-window EWMH property.
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
static void s_wm_sync_desktop_names(surface_td *surface)
{
    size_t names_len;
    size_t offset;
    char *names;

    if (surface == NULL || surface->ewmh == NULL ||
            surface->desktop_count == 0u) {
        return;
    }

    names_len = 0u;
    for (uint32_t did = 0u; did < surface->desktop_count; ++did) {
        desktop_td *desktop = surface_desktop_get(surface, did);
        if (desktop != NULL && desktop->name[0] != '\0') {
            names_len += safe_strlen(desktop->name) + 1u;
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
    for (uint32_t did = 0u; did < surface->desktop_count; ++did) {
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

        name_len = safe_strlen(name);
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
    xcb_intern_atom_reply_t *ia;
    xcb_atom_t net_wm_state_focused = XCB_ATOM_NONE;
    xcb_atom_t net_wm_win_type_notif = XCB_ATOM_NONE;
    xcb_atom_t net_wm_icon_geometry = XCB_ATOM_NONE;
    xcb_atom_t net_restack_window = XCB_ATOM_NONE;
    xcb_atom_t net_wm_fullscreen_monitors = XCB_ATOM_NONE;
    xcb_atom_t wm_icon_size_atom = XCB_ATOM_NONE;
    xcb_atom_t manager_atom = XCB_ATOM_NONE;
    uint32_t icon_size_hints[6];

    if (wm == NULL || wm->connection == NULL || wm->ewmh == NULL) {
        return 1;
    }

    /* Intern atoms not exposed directly by 'xcb_ewmh_connection_t' */
    ia = xcb_intern_atom_reply(wm->connection,
            xcb_intern_atom(wm->connection, 0, 20,
                "_NET_WM_STATE_FOCUSED"),
            NULL);
    if (ia != NULL) {
        net_wm_state_focused = ia->atom;
        free(ia);
    }

    ia = xcb_intern_atom_reply(wm->connection,
            xcb_intern_atom(wm->connection, 0,
                sizeof("_NET_WM_WINDOW_TYPE_NOTIFICATION") - 1u,
                "_NET_WM_WINDOW_TYPE_NOTIFICATION"), NULL);

    if (ia != NULL) {
        net_wm_win_type_notif = ia->atom;
        free(ia);
    }

    ia = xcb_intern_atom_reply(wm->connection,
            xcb_intern_atom(wm->connection, 0,
                sizeof("_NET_WM_ICON_GEOMETRY") - 1u,
                "_NET_WM_ICON_GEOMETRY"), NULL);

    if (ia != NULL) {
        net_wm_icon_geometry = ia->atom;
        free(ia);
    }

    ia = xcb_intern_atom_reply(wm->connection,
            xcb_intern_atom(wm->connection, 0,
                sizeof("WM_ICON_SIZE") - 1u, "WM_ICON_SIZE"), NULL);

    if (ia != NULL) {
        wm_icon_size_atom = ia->atom;
        free(ia);
    }

    ia = xcb_intern_atom_reply(wm->connection,
            xcb_intern_atom(wm->connection, 0,
                sizeof("_NET_RESTACK_WINDOW") - 1u,
                "_NET_RESTACK_WINDOW"), NULL);

    if (ia != NULL) {
        net_restack_window = ia->atom;
        free(ia);
    }

    ia = xcb_intern_atom_reply(wm->connection,
            xcb_intern_atom(wm->connection, 0,
                sizeof("_NET_WM_FULLSCREEN_MONITORS") - 1u,
                "_NET_WM_FULLSCREEN_MONITORS"), NULL);

    if (ia != NULL) {
        net_wm_fullscreen_monitors = ia->atom;
        free(ia);
    }

    ia = xcb_intern_atom_reply(wm->connection,
            xcb_intern_atom(wm->connection, 0,
                sizeof("MANAGER") - 1u, "MANAGER"), NULL);
    if (ia != NULL) {
        manager_atom = ia->atom;
        free(ia);
    }

    /* ICCCM §4.1.3: announce the fixed icon dimensions to clients */
    icon_size_hints[0] = WM_ICON_SQUARE_SIZE;   /* min_width */
    icon_size_hints[1] = WM_ICON_SQUARE_SIZE;   /* min_height */
    icon_size_hints[2] = WM_ICON_SQUARE_SIZE;   /* max_width */
    icon_size_hints[3] = WM_ICON_SQUARE_SIZE;   /* max_height */
    icon_size_hints[4] = 1u;                    /* width_inc */
    icon_size_hints[5] = 1u;                    /* height_inc */

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
    supported_atoms[n_supported++] =
        wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_SKIP_TASKBAR;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_SKIP_PAGER;
    supported_atoms[n_supported++] = wm->ewmh->_NET_CLOSE_WINDOW;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_WINDOW_TYPE;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_WINDOW_TYPE_DOCK;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_WINDOW_TYPE_NORMAL;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_WINDOW_TYPE_DIALOG;
    supported_atoms[n_supported++] = wm->ewmh->_NET_MOVERESIZE_WINDOW;
    supported_atoms[n_supported++] = wm->ewmh->_NET_FRAME_EXTENTS;
    supported_atoms[n_supported++] = wm->ewmh->_NET_REQUEST_FRAME_EXTENTS;
    supported_atoms[n_supported++] = net_restack_window;
    supported_atoms[n_supported++] = net_wm_fullscreen_monitors;
    supported_atoms[n_supported++] = wm->ewmh->_NET_DESKTOP_LAYOUT;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_STATE_MODAL;
    supported_atoms[n_supported++] = net_wm_state_focused;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ALLOWED_ACTIONS;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_MOVE;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_RESIZE;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_MINIMIZE;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_SHADE;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_STICK;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_MAXIMIZE_HORZ;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_MAXIMIZE_VERT;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_FULLSCREEN;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_CHANGE_DESKTOP;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_CLOSE;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_ABOVE;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_ACTION_BELOW;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_PING;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_USER_TIME;
    if (wm->sync_available) {
        supported_atoms[n_supported++] = wm->ewmh->_NET_WM_SYNC_REQUEST;
        supported_atoms[n_supported++] =
            wm->ewmh->_NET_WM_SYNC_REQUEST_COUNTER;
    }
    supported_atoms[n_supported++] = wm->ewmh->_NET_SHOWING_DESKTOP;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_WINDOW_TYPE_DESKTOP;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_WINDOW_TYPE_MENU;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_WINDOW_TYPE_UTILITY;
    supported_atoms[n_supported++] = wm->ewmh->_NET_WM_WINDOW_TYPE_SPLASH;
    supported_atoms[n_supported++] = net_wm_win_type_notif;
    supported_atoms[n_supported++] = net_wm_icon_geometry;

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

        if (manager_atom != XCB_ATOM_NONE) {
            char selection_name[16];
            xcb_atom_t selection_atom = XCB_ATOM_NONE;
            xcb_get_selection_owner_reply_t *owner_reply;
            xcb_client_message_event_t manager_event;

            snprintf(selection_name, sizeof(selection_name),
                    "WM_S%u", surface->id);
            ia = xcb_intern_atom_reply(wm->connection,
                    xcb_intern_atom(wm->connection, 0,
                        (uint16_t) safe_strlen(selection_name),
                        selection_name), NULL);
            if (ia != NULL) {
                selection_atom = ia->atom;
                free(ia);
            }

            if (selection_atom != XCB_ATOM_NONE) {
                xcb_set_selection_owner(wm->connection, support,
                        selection_atom, XCB_CURRENT_TIME);
                owner_reply = xcb_get_selection_owner_reply(wm->connection,
                        xcb_get_selection_owner(wm->connection,
                            selection_atom), NULL);
                if (owner_reply != NULL &&
                        owner_reply->owner == support) {
                    memset(&manager_event, 0, sizeof(manager_event));
                    manager_event.response_type = XCB_CLIENT_MESSAGE;
                    manager_event.format = 32;
                    manager_event.window = surface->screen->root;
                    manager_event.type = manager_atom;
                    manager_event.data.data32[0] = XCB_CURRENT_TIME;
                    manager_event.data.data32[1] = selection_atom;
                    manager_event.data.data32[2] = support;
                    manager_event.data.data32[3] = 0u;
                    manager_event.data.data32[4] = 0u;
                    xcb_send_event(wm->connection, 0,
                            surface->screen->root,
                            XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                            (const char *) &manager_event);
                }
                if (owner_reply != NULL) {
                    free(owner_reply);
                }
            }
        }

        /* ICCCM §4.1.3: announce fixed icon dimensions on the root
         * window */
        if (wm_icon_size_atom != XCB_ATOM_NONE) {
            xcb_change_property(wm->connection, XCB_PROP_MODE_REPLACE,
                    surface->screen->root, wm_icon_size_atom,
                    wm_icon_size_atom, 32, 6, icon_size_hints);
        }
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
        xcb_ewmh_set_showing_desktop(surface->ewmh, (int) surface->id,
                (surface->showing_desktop) ? 1u : 0u);
        s_wm_sync_desktop_names(surface);
        s_wm_sync_desktop_layout(surface);
        s_wm_sync_workarea(surface);
        s_wm_sync_client_lists(surface);
    }

    xcb_flush(wm->connection);
}


/**
 * @brief Send an @c _NET_WM_PING probe to a client and update its state
 *
 * Checks whether a previously sent ping has timed out without a reply,
 * marking the client as unresponsive if so. Otherwise, sends a new
 * @c _NET_WM_PING client message if enough time has elapsed since the
 * last ping, and records the timestamp of the sent probe.
 *
 * @param client  Pointer to the client to ping
 * @param ewmh    EWMH connection used to build the ping message
 * @param now     Current timestamp
 * @param timeout Maximum allowed time without a ping reply before the
 *                client is considered unresponsive
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_ping_client(client_td *client,
        xcb_ewmh_connection_t *ewmh, uint32_t now, uint32_t timeout)
{
    xcb_client_message_event_t ev;

    if (client == NULL || client->connection == NULL || ewmh == NULL ||
            client->window == XCB_NONE || !client->has_net_wm_ping) {
        return;
    }

    if (client->last_ping_sent != 0u &&
            client->last_ping_reply != client->last_ping_sent &&
            now >= client->last_ping_sent &&
            now - client->last_ping_sent >= timeout) {
        client_set_unresponsive(client);
    }

    if (client->last_ping_sent != 0u &&
            now >= client->last_ping_sent &&
            now - client->last_ping_sent < (uint32_t) WM_EWMH_PING_INTERVAL) {
        return;
    }

    memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.format = 32;
    ev.window = client->window;
    ev.type = ewmh->WM_PROTOCOLS;
    ev.data.data32[0] = (uint32_t) ewmh->_NET_WM_PING;
    ev.data.data32[1] = now;
    ev.data.data32[2] = (uint32_t) client->window;

    xcb_send_event(client->connection, 0, client->window,
            XCB_EVENT_MASK_NO_EVENT, (const char *) &ev);
    client->last_ping_sent = now;
}


/* Perform periodic EWMH maintenance: ping and timeout handling */
void wm_ewmh_tick(void)
{
    uint32_t now;
    uint32_t timeout;

    if (wm == NULL || wm->connection == NULL || wm->ewmh == NULL ||
            wm->surfaces == NULL) {
        return;
    }

    now = (uint32_t) time(NULL);
    timeout = (uint32_t) WM_EWMH_PING_TIMEOUT;
    for (list_item_td *snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);
        if (surface == NULL) {
            continue;
        }

        for (uint32_t did = 0u; did < surface->desktop_count; ++did) {
            desktop_td *desktop = surface_desktop_get(surface, did);
            void *elem;

            if (desktop == NULL || desktop->clients == NULL) {
                continue;
            }

            ohtbl_foreach(desktop->clients, elem) {
                client_td *client = (client_td *) elem;
                s_wm_ping_client(client, wm->ewmh, now, timeout);
            }
        }
    }

    xcb_flush(wm->connection);
}
