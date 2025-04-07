/**
 * @file actdata.c
 *
 * @brief Allocation and deallocation functions for object data structures
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
        (action_data_client_td) {.client = client,
                                 .action_client = action_client,
                                 .new_data = {0}};

    return action_data_client;
}


/* Deallocate client data structure */
void action_data_client_destroy(action_data_client_td *action_data_client)
{
    if (action_data_client) {
        safe_free((void **) &action_data_client->new_data.name);
        safe_free((void **) &action_data_client->new_data.class_name);
        safe_free((void **) &action_data_client->new_data.icon_name);

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
    action_data_desktop->action_desktop = action_desktop;

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
