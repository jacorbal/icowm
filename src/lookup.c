/**
 * @file lookup.c
 *
 * @brief Window, client, stage, and desktop lookup implementation
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

/* Stage includes */
#include <stage/desktop.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <stage.h>

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


/* Find the stage whose root window matches 'root' */
stage_td *lookup_stage_for_root(list_td *stages,
        xcb_window_t root)
{
    if (stages == NULL) {
        return NULL;
    }

    for (list_item_td *node = list_head(stages);
            node != NULL; node = list_next(node)) {
        stage_td *const stage = (stage_td *) list_data(node);
        if (stage != NULL && stage->screen != NULL &&
                stage->screen->root == root) {
            return stage;
        }
    }

    return NULL;
}


/* Return the currently active desktop for a stage */
desktop_td *lookup_current_desktop(stage_td *stage)
{
    if (stage == NULL) {
        return NULL;
    }

    return stage_desktop_get(stage, stage->desktop_cur);
}


/* Search all stages and desktops for a client by window ID */
client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **out_stage, desktop_td **out_desktop)
{
    if (out_stage != NULL) {
        *out_stage = NULL;
    }
    if (out_desktop != NULL) {
        *out_desktop = NULL;
    }

    if (stages == NULL || window == XCB_WINDOW_NONE) {
        return NULL;
    }

    for (list_item_td *snode = list_head(stages);
            snode != NULL; snode = list_next(snode)) {
        stage_td *const stage = (stage_td *) list_data(snode);
        cdlist_item_td *dnode;
        const cdlist_item_td *dinitial;

        if (stage == NULL || stage->desktops == NULL ||
                cdlist_size(stage->desktops) == 0) {
            continue;
        }

        dnode = cdlist_head(stage->desktops);
        dinitial = dnode;
        if (dnode == NULL) {
            continue;
        }

        do {
            desktop_td *const desktop =
                (desktop_td *) cdlist_data(dnode);

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

                    if (out_stage != NULL) {
                        *out_stage = stage;
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

                    if (out_stage != NULL) {
                        *out_stage = stage;
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
