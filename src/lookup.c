/**
 * @file lookup.c
 *
 * @brief Window, client, surface, and desktop lookup implementation
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
#include <stdlib.h>     /* NULL, free */
#include <string.h>     /* memset */
  
/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>

/* Local includes */
#include <lookup.h>


/**
 * @brief Test whether an X window belongs to a managed client
 *
 * Checks whether the specified window matches any of the windows
 * associated with the client: client window, frame, titlebar, icon
 * window, or client identifier.
 *
 * @param client Pointer to the client to test
 * @param window Window ID to compare against the client
 *
 * @return @c true if the window belongs to the client
 *
 * @note Complexity: @e O(1)
 */
static bool s_lookup_client_matches_window(const client_td *client,
        xcb_window_t window)
{
    if (client == NULL || window == XCB_WINDOW_NONE) {
        return false;
    }

    return client->id == window ||
           client->window == window ||
           client->frame == window ||
           client->titlebar == window ||
           client->icon_window == window;
}


/* Find the surface whose root window matches 'root' */
surface_td *lookup_surface_for_root(list_td *surfaces,
        xcb_window_t root)
{
    if (surfaces == NULL) {
        return NULL;
    }

    for (list_item_td *node = list_head(surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *const surface = (surface_td *) list_data(node);
        if (surface != NULL && surface->screen != NULL &&
                surface->screen->root == root) {
            return surface;
        }
    }

    return NULL;
}


/* Return the currently active desktop for a surface */
desktop_td *lookup_current_desktop(surface_td *surface)
{
    if (surface == NULL) {
        return NULL;
    }

    return surface_desktop_get(surface, surface->desktop_cur);
}


/* Search all surfaces and desktops for a client by window ID */
client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **out_surface, desktop_td **out_desktop)
{
    if (out_surface != NULL) {
        *out_surface = NULL;
    }
    if (out_desktop != NULL) {
        *out_desktop = NULL;
    }

    if (surfaces == NULL || window == XCB_WINDOW_NONE) {
        return NULL;
    }

    for (list_item_td *snode = list_head(surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *const surface = (surface_td *) list_data(snode);
        cdlist_item_td *dnode;
        const cdlist_item_td *dinitial;

        if (surface == NULL || surface->desktops == NULL ||
                cdlist_size(surface->desktops) == 0) {
            continue;
        }

        dnode = cdlist_head(surface->desktops);
        dinitial = dnode;
        if (dnode == NULL) {
            continue;
        }

        do {
            desktop_td *const desktop = (desktop_td *) cdlist_data(dnode);

            if (desktop != NULL && desktop->clients != NULL) {
                void *elem;

                /* Fast path: O(1) hash lookup by 'client->id'.
                 * Works for all managed clients because 'client->id'
                 * equals the X window ID for managed clients */
                client_td temp_key;
                client_td *found = NULL;

                memset(&temp_key, 0, sizeof(temp_key));
                temp_key.id = window;
                found = &temp_key;
                if (ohtbl_lookup(desktop->clients,
                            (void **) &found) == 0 &&
                        found != NULL &&
                        s_lookup_client_matches_window(found, window)) {

                    if (out_surface != NULL) {
                        *out_surface = surface;
                    }

                    if (out_desktop != NULL) {
                        *out_desktop = desktop;
                    }

                    return found;
                }

                /* Slow path: linear scan for frame, titlebar, icon
                 * window IDs that differ from 'client->id' */
                ohtbl_foreach(desktop->clients, elem) {
                    client_td *const client = (client_td *) elem;

                    if (!s_lookup_client_matches_window(client, window)) {
                        continue;
                    }

                    if (out_surface != NULL) {
                        *out_surface = surface;
                    }

                    if (out_desktop != NULL) {
                        *out_desktop = desktop;
                    }

                    return client;
                }
            }
            dnode = cdlist_next(dnode);
        } while (dnode != NULL && dnode != dinitial);
    }

    return NULL;
}
