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
    if (action_data_client) {
        safe_free((void **) &action_data_client->new_data.str.str0);
        safe_free((void **) &action_data_client->new_data.str.str1);
        free(action_data_client);
    }
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
