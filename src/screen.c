/**
 * @file screen.c
 *
 * @brief Screen handling implementation
 */

/* System includes */
#include <stdbool.h>    /* bool, false, true */
#include <stdlib.h>     /* NULL, free, malloc */

/* External libraries */
#include <X11/Xlib.h>   /* Display, Screen */

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <screen.h>


/* Initialize a new screen */
screen_td *screen_init(Display *display, const unsigned int screen_id,
        unsigned int desktop_count, config_td *config)
{
    screen_td *screen;
    Screen *xscreen;

    LOGGER_DEBUG("Initializing screen %u", screen_id);
    screen = malloc(sizeof(screen_td));
    if (screen == NULL) {
        LOGGER_FATAL("Failed to allocate memory for screen %u",
                screen_id);
        return NULL;
    }

    LOGGER_TRACE("Retrieving screen information from X server", L_NARG);
    xscreen = ScreenOfDisplay(display, screen_id);
    if (xscreen == NULL) {
        LOGGER_FATAL("Failed to retrieve information for screen %u",
                screen_id);
        return NULL;
    }

    screen->id = screen_id;
    screen->display = display;
    screen->config = config;

    screen->dim.w = (unsigned int) xscreen->width;
    screen->dim.h = (unsigned int) xscreen->height;

    /* DPI = px / (mm/25.4);  1 in = 25.4 mm*/
    screen->dpi.x = (unsigned int)
        ((float) screen->dim.w /
         (((float) XDisplayWidthMM(display, (int) screen_id) / 25.4f)));
    screen->dpi.y = (unsigned int)
        ((float) screen->dim.h /
         (((float) XDisplayHeightMM(display, (int) screen_id) / 25.4f)));

    screen->depth = DefaultDepth(display, screen_id);
    screen->colormaps = DefaultColormap(display, screen_id);
    screen->visual = DefaultVisual(display, screen_id);
    screen->root = RootWindow(display, screen_id);

    /* Handle desktops */
    LOGGER_TRACE("Setting up all %d desktops", desktop_count);

    LOGGER_TRACE("Initializing desktop list structure for screen %u",
            screen_id);
    screen->desktops = cdlist_init((void(*)(void *)) desktop_destroy);
    if (screen->desktops == NULL) {
        LOGGER_FATAL("Failed to allocate memory for desktops on" \
                " screen %u", screen_id);
        free(screen);
        return NULL;
    }

    /* Initialize desktops */
    screen->desktop_count = desktop_count;
    for (unsigned int i = 0; i < desktop_count; ++i) {
        desktop_td *desktop = desktop_init(screen_id, i,
                &(screen->config->base), &(screen->config->theme));
        if (screen == NULL) {
            LOGGER_FATAL("Failed to initialize desktop %u on" \
                    " screen %u", i, screen_id);
            cdlist_destroy(screen->desktops);
            return NULL;
        }

        LOGGER_TRACE("Inserting desktop %u ('%s') of " \
                "screen %u into desktop list",
                i, desktop->name, screen_id);
        if (cdlist_ins_next(screen->desktops,
                    cdlist_tail(screen->desktops),
                    (const void *) desktop) != 0) {
            LOGGER_FATAL("Failed to insert desktop %u ('%s') on" \
                    " screen %u into desktop list",
                    i, desktop->name, screen_id);
            desktop_destroy(desktop);
            cdlist_destroy(screen->desktops);
            return NULL;
        }
    }

    screen->is_outdated = true;

    return screen;
}

/* Free allocated memory for a screen */
void screen_destroy(screen_td *screen)
{
    LOGGER_DEBUG("Deallocating structure for screen %u",
            screen->id);
    if (screen == NULL) {
        return;
    }

    LOGGER_TRACE("Deallocating desktops on screen %u", screen->id);
    cdlist_destroy(screen->desktops);

    LOGGER_TRACE("Destroying screen %u", screen->id);
    free(screen);
}


/* Soft screen update */
void screen_update(screen_td *screen)
{
//    LOGGER_TRACE("Updating screen %u", screen->id);

    /* Establish that this screen is already updated */
    screen->is_outdated = false;
}


/* Full screen update */
void screen_update_full(screen_td *screen)
{
    LOGGER_TRACE("Fully updating screen %u", screen->id);

    /* Soft update */
    screen_update(screen);

    /* Update all desktops */
    cdlist_item_td *desktop_node = cdlist_head(screen->desktops);
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

    LOGGER_TRACE("Updated screen %u", screen->id);
}


/* Resize the screen */
void screen_resize(screen_td *screen,
        unsigned int width, unsigned int height)
{
    if (screen->dim.w != width) {
        screen->dim.w = width;
    }

    if (screen->dim.h != height) {
        screen->dim.h = height;
    }

    /* TODO: More logic here to update display, desktops, &c. */
}


/* Add a new desktop to the list */
int screen_desktop_add(screen_td *screen, desktop_td *desktop)
{
    if (screen == NULL|| desktop == NULL) {
        return -1;
    }

    /* Add to the tail of the circular linked list */
    if (cdlist_ins_next(screen->desktops,
                cdlist_tail(screen->desktops), desktop) != 0) {
        /* Failed to add to the list */
        return 1;
    }

    /* Update the count of desktops */
    screen->desktop_count++;

    return 0;
}


/* Remove a desktop from the list by its ID */
int screen_desktop_rem(screen_td *screen, unsigned int desktop_id)
{
    if (screen == NULL || screen->desktop_count == 0) {
        return -1;
    }

    /* Iterate over each desktop in the circular list */
    for (cdlist_item_td *current_item = cdlist_head(screen->desktops);
         current_item != NULL;
         current_item = cdlist_next(current_item)) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            /* Remove the desktop */
            if (cdlist_rem_next(screen->desktops,
                        current_item, NULL) != 0) {
                /* Failed to remove from list */
                return 1;
            }
            desktop_destroy(desktop);

            /* Update the count of desktops */
            screen->desktop_count--;
            return 0;
        }
    }

    /* Desktop not found */
    return 2;
}


/* Get a desktop from the list by its ID */
desktop_td *screen_desktop_get(screen_td *screen,
        unsigned int desktop_id)
{
    if (screen == NULL) {
        return NULL;
    }

    /* Iterate over each desktop in the circular list */
    for (cdlist_item_td *current_item = cdlist_head(screen->desktops);
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
desktop_td *screen_desktop_prev(screen_td *screen,
        unsigned int desktop_id, bool cycle)
{
    if (screen == NULL || screen->desktops == NULL ||
            screen->desktop_count == 0) {
        return NULL;
    }

    cdlist_item_td *current_item = cdlist_head(screen->desktops);
    for (size_t i = 0; i < screen->desktop_count; ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            cdlist_item_td *prev_item = cdlist_prev(current_item);
            if (prev_item == cdlist_head(screen->desktops)) {
                if (cycle) {
                    /* Circular behavior; wrap to the last desktop */
                    return (desktop_td *) cdlist_data(cdlist_tail(screen->desktops));
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
desktop_td *screen_desktop_next(screen_td *screen,
        unsigned int desktop_id, bool cycle)
{
    if (screen == NULL || screen->desktops == NULL ||
            screen->desktop_count == 0) {
        return NULL;
    }

    cdlist_item_td *current_item = cdlist_head(screen->desktops);
    for (size_t i = 0; i < screen->desktop_count; ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            cdlist_item_td *next_item = cdlist_next(current_item);
            if (next_item == cdlist_head(screen->desktops)) {
                if (cycle) {
                    /* Circular behavior; wrap to the first desktop */
                    return (desktop_td *) cdlist_data(cdlist_head(screen->desktops));
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
int screen_desktop_select_prev(screen_td *screen, bool cycle)
{
   if (screen == NULL || screen->desktop_count == 0) {
        return -1;
    }

    desktop_td *prev_desktop =
        screen_desktop_prev(screen, screen->desktop_cur, cycle);
    if (prev_desktop) {
        /* Update ID of new current desktop */
        screen->desktop_cur = prev_desktop->id;
        return 0;
    }

    /* No previous desktop found */
    return 1;
}


/* Select the next desktop, optionally cycling */
int screen_desktop_select_next(screen_td *screen, bool cycle)
{
    if (screen == NULL || screen->desktop_count == 0) {
        return -1;
    }

    desktop_td *next_desktop =
        screen_desktop_next(screen, screen->desktop_cur, cycle);
    if (next_desktop) {
        /* Update ID of new current desktop */
        screen->desktop_cur = next_desktop->id;
        return 0;
    }

    /* No next desktop found */
    return 1;
}


/* Select a specific desktop by ID */
int screen_desktop_select(screen_td *screen, unsigned int desktop_id)
{
    if (screen == NULL || screen->desktop_count == 0) {
        return -1;
    }

    /* Iterate through the desktops list to check if ID is valid */
    cdlist_item_td *current_item = cdlist_head(screen->desktops);
    for (size_t i = 0; i < screen->desktop_count; ++i) {
        desktop_td *desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            /* Update ID of new current desktop */
            screen->desktop_cur = desktop_id;
            return 0;
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop ID not found */
    return 1;
}
