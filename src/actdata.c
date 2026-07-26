/**
 * @file actdata.c
 *
 * @brief Allocation and deallocation functions for object data structures
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* Project includes */
#include <action.h>
#include <client.h>
#include <desktop.h>
#include <surface.h>

/* Utils includes */
#include <utils/safemem.h>

/* Local includes */
#include <actdata.h>


/* Allocate client data structure */
action_data_client_td *action_data_client_init(client_td *client,
        enum action_client_e action_client)

{
    action_data_client_td *action_data_client;

    action_data_client = malloc(sizeof(action_data_client_td));
    if (action_data_client == NULL) {
        return NULL;
    }

    *action_data_client =
        (action_data_client_td) { .client = client,
                                  .action_client = action_client,
                                  .new_data = {
                                      .str = { .str0 = NULL,
                                               .str1 = NULL }
                                  }
                                };

    return action_data_client;
}


/* Deallocate client data structure */
void action_data_client_destroy(action_data_client_td *action_data_client)
{
    if (action_data_client == NULL) {
        return;
    }

    /* 'new_data' is a union: only the actions that actually populate
     * its 'str' member own heap-allocated strings that must be freed
     * here.  Actions using 'geometry' (or no extra data at all) must
     * not touch 'str.str0'/'str.str1', since those bytes overlap in
     * memory with numeric geometry fields and are not valid pointers in
     * that case; freeing them would corrupt the heap. */
    switch (action_data_client->action_client) {
        case ACTION_CLIENT_RENAME:
        case ACTION_CLIENT_RECLASS:
        case ACTION_CLIENT_REROLE:
        case ACTION_CLIENT_SET_ICON:
            safe_free((void **) &action_data_client->new_data.str.str0);
            safe_free((void **) &action_data_client->new_data.str.str1);
            break;

        case ACTION_CLIENT_CREATE:
        case ACTION_CLIENT_CLOSE:
        case ACTION_CLIENT_KILL:
        case ACTION_CLIENT_RESTORE:
        case ACTION_CLIENT_FOCUS:
        case ACTION_CLIENT_UNFOCUS:
        case ACTION_CLIENT_RESIZE:
        case ACTION_CLIENT_MOVE:
        case ACTION_CLIENT_CENTER:
        case ACTION_CLIENT_MAXIMIZE:
        case ACTION_CLIENT_MAXIMIZE_HORZ:
        case ACTION_CLIENT_MAXIMIZE_VERT:
        case ACTION_CLIENT_ICONIFY:
        case ACTION_CLIENT_HIDE:
        case ACTION_CLIENT_UNHIDE:
        case ACTION_CLIENT_SHADE:
        case ACTION_CLIENT_UNSHADE:
        case ACTION_CLIENT_TOGGLE_SHADE:
        case ACTION_CLIENT_STICKY:
        case ACTION_CLIENT_UNSTICKY:
        case ACTION_CLIENT_TOGGLE_STICKY:
        case ACTION_CLIENT_FULLSCREEN:
        case ACTION_CLIENT_UNFULLSCREEN:
        case ACTION_CLIENT_TOGGLE_FULLSCREEN:
        case ACTION_CLIENT_RAISE:
        case ACTION_CLIENT_LOWER:
        case ACTION_CLIENT_LAYER_ABOVE:
        case ACTION_CLIENT_LAYER_NORMAL:
        case ACTION_CLIENT_LAYER_BELOW:
        case ACTION_CLIENT_SET_URGENT:
        case ACTION_CLIENT_CLEAR_URGENT:
        case ACTION_CLIENT_TOGGLE_DECORATION:
        case ACTION_CLIENT_CYCLE_NEXT:
        case ACTION_CLIENT_CYCLE_PREV:
            /* No heap-allocated data to release for these actions */
            break;
    }

    free(action_data_client);
}


/* Allocate desktop data structure */
action_data_desktop_td *action_data_desktop_init(desktop_td *desktop,
        enum action_desktop_e action_desktop)
{
    action_data_desktop_td *action_data_desktop;

    action_data_desktop = malloc(sizeof(action_data_desktop_td));
    if (action_data_desktop == NULL) {
        return NULL;
    }

    action_data_desktop->desktop = desktop;
    action_data_desktop->target = NULL;
    action_data_desktop->client = NULL;
    action_data_desktop->action_desktop = action_desktop;
    action_data_desktop->new_data.str = NULL;

    return action_data_desktop;
}


/* Deallocate desktop data structure */
void action_data_desktop_destroy(action_data_desktop_td *action_data_desktop)
{
    free(action_data_desktop);
}


/* Allocate surface data structure */
action_data_surface_td *action_data_surface_init(surface_td *surface,
        enum action_surface_e action_surface)
{
    action_data_surface_td *action_data_surface;

    action_data_surface = malloc(sizeof(action_data_surface_td));
    if (action_data_surface == NULL) {
        return NULL;
    }

    action_data_surface->surface= surface;
    action_data_surface->action_surface = action_surface;

    return action_data_surface;
}


/* Deallocate surface data structure */
void action_data_surface_destroy(action_data_surface_td *action_data_surface)
{
    free(action_data_surface);
}


/* Allocate memory for the window manager data structure */
action_data_wm_td *action_data_wm_init(wm_td *wm,
        enum action_wm_e action_wm)
{
    action_data_wm_td *action_data_wm;

    action_data_wm = malloc(sizeof(action_data_wm_td));
    if (action_data_wm == NULL) {
        return NULL;
    }

    action_data_wm->wm = wm;
    action_data_wm->action_wm = action_wm;

    return action_data_wm;
}


/* Deallocate window manager data structure */
void action_data_wm_destroy(action_data_wm_td *action_data_wm)
{
    free(action_data_wm);
}
