/**
 * @file desktop.c
 *
 * @brief Desktop handling implementation
 */

/* System includes */
#include <stdlib.h>     /* free, malloc */

/* External libraries */
//#include <X11/Xlib.h>

/* ADT */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <logger.h>

/* Local includes */
#include <desktop.h>


/* Initialize a new desktop */
desktop_td *desktop_init(unsigned int screen_id,
        unsigned int desktop_id,
        struct config_base_s *config_base,
        struct config_theme_s *config_theme)
{
    desktop_td *desktop;

    logger_msg(LOG_DEBUG, "Initializing desktop %u on screen %u",
            desktop_id, screen_id);
    desktop = malloc(sizeof(desktop_td));
    if (desktop == NULL) {
        logger_msg(LOG_ERROR,
                "Failed to allocate memory for desktop %u on screen %u",
                desktop_id, screen_id);
        return NULL;
    }

    desktop->screen_id = screen_id;
    desktop->id = desktop_id;
    desktop->window_active = NULL;
    config_base = config_base;
    config_theme = config_theme;

    desktop->background.bg.color =
        config_base->screens[screen_id].desktops[desktop_id].settings.background.color;

    logger_msg(LOG_TRACE,
            "Initializing window list structure for" \
            " desktop %u on screen %u", desktop_id, screen_id);
    desktop->windows = list_init((void(*)(void *)) window_destroy);
    if (desktop->windows == NULL) {
        logger_msg(LOG_ERROR,
                "Failed to allocate memory for window list on desktop" \
                " %u on screen %u", desktop_id, screen_id);
        free(desktop);
        return NULL;
    }

    return desktop;
}


/* Free memory for allocated desktop */
void desktop_destroy(desktop_td *desktop)
{
    logger_msg(LOG_DEBUG,
            "Deallocating structure for desktop %u", desktop->id);
    if (desktop != NULL) {
        logger_msg(LOG_TRACE,
                "Deallocating windows on desktop %u", desktop->id);
        list_destroy(desktop->windows);
        logger_msg(LOG_TRACE, "Destroying desktop %u", desktop->id);
        free(desktop);
    }
}


/* Clear a desktop by removing all its windows */
void desktop_clear(desktop_td *desktop)
{
    logger_msg(LOG_DEBUG, "Preparing to clear desktop %u", desktop->id);
    if (desktop != NULL && desktop->windows != NULL) {
        list_destroy(desktop->windows);
    }
}


/* Add a previously allocated window in the desktop */
int desktop_window_add(desktop_td *desktop, window_td *window)
{
    logger_msg(LOG_DEBUG,
            "Preparing to add window %u to desktop %u",
            window->id, desktop->id);

    if (desktop == NULL || window == NULL) {
        return -1;
    }

    if (list_ins_next(desktop->windows,
                list_tail(desktop->windows),
                (const void *) window) != 0) {
        logger_msg(LOG_ALERT,
                "Cannot allocate memory for window" \
                " %#lx on desktop %u on screen %u",
                window->id, desktop->id, desktop->screen_id);
        return -1;
    }

    return 0;
}

/* Remove a window from the desktop */
int desktop_window_rem(desktop_td *desktop, window_td *window)
{
    logger_msg(LOG_DEBUG,
            "Preparing to remove window %#x from desktop %u",
            window->id, desktop->id);

    if (desktop == NULL || window == NULL) {
        return -1;
    }

    /* Search window in list of windows */
    logger_msg(LOG_TRACE, "Searching for window %#x on desktop %u",
            window->id, desktop->id);
    list_item_td *item = list_head(desktop->windows);
    while (item != NULL) {
        window_td *window_cur = (window_td *) list_data(item);
        if (window_cur != NULL && window_cur->id == window->id) {
            logger_msg(LOG_TRACE, "Removing window %u from desktop %u",
                    window->id, desktop->id);
            list_rem_next(desktop->windows, NULL, (void **) &window_cur);
            window_destroy(window_cur);
            return 0;
        }
        item = list_next(item);
    }

    /* Window not found */
    logger_msg(LOG_TRACE, "Window %#x not found on desktop %u",
            window->id, desktop->id);
    return -1;
}

/* Remove a window from a desktop by identifier */
int desktop_window_rem_by_id(desktop_td *desktop, unsigned int window_id)
{
    logger_msg(LOG_DEBUG,
            "Preparing to remove window by id %#x from desktop %u",
            window_id, desktop->id);

    if (desktop == NULL) {
        return -1;
    }

    /* Search window by ID */
    logger_msg(LOG_TRACE,
            "Searching for window by id %#x on desktop %u",
            window_id, desktop->id);
    list_item_td *item = list_head(desktop->windows);
    while (item != NULL) {
        window_td *window_cur = (window_td *) list_data(item);
        if (window_cur != NULL && window_cur->id == window_id) {
            logger_msg(LOG_TRACE, "Removing window %#x from desktop %u",
                    window_id, desktop->id);
            list_rem_next(desktop->windows, NULL, (void **) &window_cur);
            window_destroy(window_cur);
            return 0;
        }
        item = list_next(item);
    }

    /* Window not found */
    logger_msg(LOG_TRACE, "Window %#x not found on desktop %u",
            window_id, desktop->id);
    return -1;
}
