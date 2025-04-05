/**
 * @file actdata.c
 *
 * @brief Allocation and deallocation functions for object data structures
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* Project includes */
#include <action.h>
#include <desktop.h>
#include <surface.h>
#include <window.h>

/* Utils includes */
#include <utils/safemem.h>

/* Local includes */
#include <actdata.h>


/* Allocate window data structure */
action_data_window_td *action_data_window_init(window_td *window,
        enum action_window_e action_window)

{
    action_data_window_td *action_data_window;

    action_data_window = malloc(sizeof(action_data_window_td));
    if (action_data_window == NULL) {
        return NULL;
    }

    *action_data_window =
        (action_data_window_td) {.window = window,
                                 .action_window = action_window,
                                 .new_data = {0}};

    return action_data_window;
}


/* Deallocate window data structure */
void action_data_window_destroy(action_data_window_td *action_data_window)
{
    if (action_data_window) {
        safe_free((void **) &action_data_window->new_data.name);
        safe_free((void **) &action_data_window->new_data.class_name);
        safe_free((void **) &action_data_window->new_data.icon_name);

        free(action_data_window);
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
