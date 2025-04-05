/**
 * @file surface.c
 *
 * @brief Screen handling implementation
 */

/* System includes */
#include <stdbool.h>
#include <stdlib.h>     /* NULL, free, malloc */

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Screen */

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <surface.h>


/* Update surface properties by asking X */
static void s_update_properties(surface_td *surface, Screen *xsurface)
{
    int xx, yy;

    /* Update surface dimensions */
    xx = XWidthOfScreen(xsurface);
    yy = XWidthOfScreen(xsurface);
    surface->properties.dim.w = (xx > 0) ? (unsigned int) xx : 0;
    surface->properties.dim.w = (yy > 0) ? (unsigned int) yy : 0;

    xx = XHeightOfScreen(xsurface);
    yy = XHeightOfScreen(xsurface);
    surface->properties.dim.h = (xx > 0) ? (unsigned int) xx : 0;
    surface->properties.dim.h = (yy > 0) ? (unsigned int) yy : 0;

    /* Calculate DPI; dpi = px / (mm/25.4);  1 in ~= 25.4 mm */
    xx = XWidthMMOfScreen(xsurface);
    yy = XHeightMMOfScreen(xsurface);
    surface->properties.dim_mm.w = (xx > 0) ? (unsigned int) xx : 0;
    surface->properties.dim_mm.h = (yy > 0) ? (unsigned int) yy : 0;

    /* Calculate DPI for x-axis */
    if (surface->properties.dim_mm.w> 0) {
        surface->properties.dpi.x =
            (unsigned int) ((float) surface->properties.dim.w /
                    ((float) surface->properties.dim_mm.w / 25.4f));
    } else {
        /* Division by zero:  DPI in 'x' set to 0 */
        /** @todo Handle division by zero in 'dpi.x' */
        surface->properties.dpi.x = 0;
    }

    /* Calculate DPI for y-axis */
    if (surface->properties.dim_mm.h > 0) {
        surface->properties.dpi.y =
            (unsigned int) ((float) surface->properties.dim.h /
                    ((float) surface->properties.dim_mm.h / 25.4f));
    } else {
        /* Division by zero:  DPI in 'y' set to 0 */
        /** @todo Handle division by zero in 'dpi.y' */
        surface->properties.dpi.y = 0;
    }

    /* Set visual properties */
    surface->properties.visual_info.properties.depth =
        DefaultDepth(surface->display, surface->id);
    surface->properties.visual_info.properties.colormaps =
        DefaultColormap(surface->display, surface->id);
    surface->properties.visual_info.visual =
        DefaultVisual(surface->display, surface->id);
    surface->root = RootWindow(surface->display, surface->id);
}


/* Initialize a new surface */
surface_td *surface_init(Display *display, const XID surface_id,
        unsigned int desktop_count, config_td *config)
{
    surface_td *surface;
    Screen *xsurface;

    LOGGER_DEBUG("Initializing surface %lu", surface_id);
    surface = malloc(sizeof(surface_td));
    if (surface == NULL) {
        LOGGER_FATAL("Failed to allocate memory for surface %lu",
                surface_id);
        return NULL;
    }

    LOGGER_TRACE("Retrieving surface information from X server", L_NARG);
    xsurface = ScreenOfDisplay(display, surface_id);
    if (xsurface == NULL) {
        LOGGER_FATAL("Failed to retrieve information for surface %lu",
                surface_id);
        return NULL;
    }

    surface->id = surface_id;
    surface->display = display;
    surface->config = config;
    surface->xsurface = xsurface;

    /* Update surface properties */
    s_update_properties(surface, xsurface);

    /* Handle desktops */
    LOGGER_TRACE("Setting up all %d desktops", desktop_count);

    LOGGER_TRACE("Initializing desktop list structure for surface %lu",
            surface_id);
    surface->desktops = cdlist_init((void(*)(void *)) desktop_destroy);
    if (surface->desktops == NULL) {
        LOGGER_FATAL("Failed to allocate memory for desktops on" \
                " surface %lu", surface_id);
        free(surface);
        return NULL;
    }

    /* Initialize desktops */
    surface->desktop_count = desktop_count;
    for (unsigned int i = 0; i < desktop_count; ++i) {
        desktop_td *desktop = desktop_init(surface_id, i,
                &(surface->config->base), &(surface->config->theme));
        if (surface == NULL) {
            LOGGER_FATAL("Failed to initialize desktop %lu on" \
                    " surface %lu", i, surface_id);
            cdlist_destroy(surface->desktops);
            return NULL;
        }

        LOGGER_TRACE("Inserting desktop %lu ('%s') of " \
                "surface %lu into desktop list",
                i, desktop->name, surface_id);
        if (cdlist_ins_next(surface->desktops,
                    cdlist_tail(surface->desktops),
                    (const void *) desktop) != 0) {
            LOGGER_FATAL("Failed to insert desktop %lu ('%s') on" \
                    " surface %lu into desktop list",
                    i, desktop->name, surface_id);
            desktop_destroy(desktop);
            cdlist_destroy(surface->desktops);
            return NULL;
        }
    }

    surface->is_outdated = true;

    return surface;
}

/* Free allocated memory for a surface */
void surface_destroy(surface_td *surface)
{
    if (surface == NULL) {
        return;
    }

    LOGGER_DEBUG("Deallocating structure for surface %lu", surface->id);

    LOGGER_TRACE("Deallocating desktops on surface %lu", surface->id);
    cdlist_destroy(surface->desktops);

    LOGGER_TRACE("Destroying surface %lu", surface->id);
    free(surface);
}


/* Soft surface update */
void surface_update(surface_td *surface)
{
//    LOGGER_TRACE("Updating surface %lu", surface->id);

    /* Establish that this surface is already updated */
    surface->is_outdated = false;
}


/* Full surface update */
void surface_update_full(surface_td *surface)
{
    cdlist_item_td *desktop_node  = cdlist_head(surface->desktops);

    LOGGER_TRACE("Fully updating surface %lu", surface->id);

    /* Soft update */
    surface_update(surface);

    /* Update all desktops */
    if (desktop_node != NULL) {
        /* Reference to the initial node not to end up an infinite loop
         * in this circular list */
        cdlist_item_td *desktop_initial = desktop_node;
        do {
            desktop_td *desktop_cur =
                (desktop_td *) cdlist_data(desktop_node);
                if (desktop_cur->is_outdated) {
                    desktop_update_full(desktop_cur);
                }
                desktop_node = cdlist_prev(desktop_node);
        } while (desktop_node != desktop_initial);
    }

    LOGGER_TRACE("Updated surface %lu", surface->id);
}


/* Resize the surface */
void surface_resize(surface_td *surface,
        unsigned int width, unsigned int height)
{
    if (surface->properties.dim.w != width) {
        surface->properties.dim.w = width;
    }

    if (surface->properties.dim.h != height) {
        surface->properties.dim.h = height;
    }

    /* TODO: More logic here to update display, desktops, &c. */
}


/* Add a new desktop to the list */
int surface_desktop_add(surface_td *surface, desktop_td *desktop)
{
    if (surface == NULL|| desktop == NULL) {
        return -1;
    }

    /* Add to the tail of the circular linked list */
    if (cdlist_ins_next(surface->desktops,
                cdlist_tail(surface->desktops), desktop) != 0) {
        /* Failed to add to the list */
        return 1;
    }

    /* Update the count of desktops */
    surface->desktop_count++;

    return 0;
}


/* Remove a desktop from the list by its ID */
int surface_desktop_rem(surface_td *surface, XID desktop_id)
{
    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    /* Iterate over each desktop in the circular list */
    for (cdlist_item_td *current_item = cdlist_head(surface->desktops);
         current_item != NULL;
         current_item = cdlist_next(current_item)) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            /* Remove the desktop */
            if (cdlist_rem_next(surface->desktops,
                        current_item, NULL) != 0) {
                /* Failed to remove from list */
                return 1;
            }
            desktop_destroy(desktop);

            /* Update the count of desktops */
            surface->desktop_count--;
            return 0;
        }
    }

    /* Desktop not found */
    return 2;
}


/* Get a desktop from the list by its ID */
desktop_td *surface_desktop_get(surface_td *surface, XID desktop_id)
{
    if (surface == NULL) {
        return NULL;
    }

    /* Iterate over each desktop in the circular list */
    for (cdlist_item_td *current_item = cdlist_head(surface->desktops);
            current_item != NULL;
            current_item = cdlist_next(current_item)) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            return desktop;
        }
    }

    /* Desktop ID not found */
    return NULL;
}


/* Get the previous desktop in the list, optionally cycling */
desktop_td *surface_desktop_prev(surface_td *surface, XID desktop_id,
        bool cycle)
{
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktops == NULL ||
            surface->desktop_count == 0) {
        return NULL;
    }

    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0; i < surface->desktop_count; ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            cdlist_item_td *prev_item = cdlist_prev(current_item);
            if (prev_item == cdlist_head(surface->desktops)) {
                if (cycle) {
                    /* Circular behavior; wrap to the last desktop */
                    return (desktop_td *) cdlist_data(cdlist_tail(surface->desktops));
                }
                /* No valid previous desktop */
                return NULL;
            }
            return (desktop_td *) cdlist_data(prev_item);
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop not found */
    return NULL;
}


/* Get the next desktop in the list, optionally cycling */
desktop_td *surface_desktop_next(surface_td *surface, XID desktop_id,
        bool cycle)
{
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktops == NULL ||
            surface->desktop_count == 0) {
        return NULL;
    }

    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0; i < surface->desktop_count; ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            cdlist_item_td *next_item = cdlist_next(current_item);
            if (next_item == cdlist_head(surface->desktops)) {
                if (cycle) {
                    /* Circular behavior; wrap to the first desktop */
                    return (desktop_td *) cdlist_data(cdlist_head(surface->desktops));
                }

                /* No valid next desktop */
                return NULL;
            }
            return (desktop_td *) cdlist_data(next_item);
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop not found */
    return NULL;
}


/* Select the previous desktop, optionally cycling */
int surface_desktop_select_prev(surface_td *surface, bool cycle)
{
    desktop_td *prev_desktop;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    prev_desktop =
        surface_desktop_prev(surface, surface->desktop_cur, cycle);
    if (prev_desktop) {
        /* Update ID of new current desktop */
        surface->desktop_cur = prev_desktop->id;
        return 0;
    }

    /* No previous desktop found */
    return 1;
}


/* Select the next desktop, optionally cycling */
int surface_desktop_select_next(surface_td *surface, bool cycle)
{
    desktop_td *next_desktop;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    next_desktop =
        surface_desktop_next(surface, surface->desktop_cur, cycle);
    if (next_desktop) {
        /* Update ID of new current desktop */
        surface->desktop_cur = next_desktop->id;
        return 0;
    }

    /* No next desktop found */
    return 1;
}


/* Select a specific desktop by ID */
int surface_desktop_select(surface_td *surface, XID desktop_id)
{
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    /* Iterate through the desktops list to check if ID is valid */
    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0; i < surface->desktop_count; ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            /* Update ID of new current desktop */
            surface->desktop_cur = desktop_id;
            return 0;
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop ID not found */
    return 1;
}
