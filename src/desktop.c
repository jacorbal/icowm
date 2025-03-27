/**
 * @file desktop.c
 *
 * @brief Desktop handling implementation
 */

/* System includes */
#include <stdbool.h>    /* bool, false, true */
#include <stdint.h>     /* uint32_t */
#include <stdlib.h>     /* NULL, free, malloc, rand */
#include <time.h>       /* time */
#include <unistd.h>     /* getpid */

/* External libraries */
//#include <X11/Xlib.h>

/* ADT includes */
#include <adt/ohtbl.h>  /* Open-addressed hash table (closed hashing) */

/* Utils includes */
#include <utils/murmurhash.h>
#include <utils/safestr.h>

/* Project includes */
#include <logger.h>
#include <window.h>

/* Local includes */
#include <desktop.h>


/* Define a hash function with a random seed */
static size_t s_h1(const void *data)
{
    uint32_t seed;
    const window_td *window = (const window_td *) data;

    seed = (uint32_t) (time(NULL) ^ getpid() ^ rand());
    return (size_t) murmurhash3_32(window, sizeof(window_td), seed);
}


/* Define an auxiliary hash function with a random seed */
static size_t s_h2(const void *data)
{
    uint32_t seed;
    const window_td *window = (const window_td *) data;

    seed = (uint32_t) (time(NULL) ^ (getpid() << 16) ^ rand());
    return (size_t) murmurhash3_32(window, sizeof(window_td), seed);
}


/* Members of the hash table (windows) match if they have equal key
 * value (identifier) */
static bool s_window_match(const void *key1, const void *key2)
{
    const window_td *window1 = (const window_td *) key1;
    const window_td *window2 = (const window_td *) key2;

    return window1->id == window2->id;
}


/* Initialize a new desktop */
desktop_td *desktop_init(unsigned int screen_id,
        unsigned int desktop_id,
        struct config_base_s *config_base,
        struct config_theme_s *config_theme)
{
    desktop_td *desktop;

    LOGGER_DEBUG("Initializing desktop %u on screen %u",
            desktop_id, screen_id);
    desktop = malloc(sizeof(desktop_td));
    if (desktop == NULL) {
        LOGGER_ERROR("Failed to allocate memory for" \
                "desktop %u on screen %u", desktop_id, screen_id);
        return NULL;
    }

    /* Stablish the basics */
    desktop->screen_id = screen_id;
    desktop->id = desktop_id;
    desktop->window_active = NULL;

    /* Get the configuration */
    desktop->config_base = config_base;
    desktop->config_theme = config_theme;

    /* Set desktop name */
    safe_strncpy(desktop->name,
            config_base->screens[screen_id].desktops[desktop_id].name,
            DESKTOP_MAX_LENGTH_NAME - 1);
        desktop->name[DESKTOP_MAX_LENGTH_NAME - 1] = '\0';

    /* Set background color */
    desktop->background.bg.color =
        config_base->screens[screen_id].desktops[desktop_id].settings.background.color;

    LOGGER_TRACE("Initializing window list structure for" \
            " desktop %u ('%s') on screen %u",
            desktop_id, desktop->name, screen_id);
    desktop->windows =
        ohtbl_init(DESKTOP_INITIAL_CAPACITY, s_h1, s_h2, s_window_match,
                (void(*)(void *)) window_destroy);
    if (desktop->windows == NULL) {
        LOGGER_ERROR("Failed to allocate memory for window hash table" \
                " on desktop %u ('%s') on screen %u",
                desktop_id, desktop->name, screen_id);
        free(desktop);
        return NULL;
    }

    /* Needs too be updated */
    desktop->is_outdated = true;

    return desktop;
}


/* Free memory for allocated desktop */
void desktop_destroy(desktop_td *desktop)
{
    LOGGER_DEBUG("Deallocating structure for desktop %u ('%s')",
            desktop->id, desktop->name);
    if (desktop == NULL) {
        return;
    }

    LOGGER_TRACE("Deallocating windows on desktop %u ('%s')",
            desktop->id, desktop->name);
    ohtbl_destroy(desktop->windows);
    LOGGER_TRACE("Destroying desktop %u ('%s')",
            desktop->id, desktop->name);
    free(desktop);
}


/* Soft desktop update */
void desktop_update(desktop_td *desktop)
{
//    LOGGER_TRACE("Updating desktop %u ('%s')",
//            desktop->id, desktop->name);

    /* Establish that this desktop is already updated */
    desktop->is_outdated = false;
}


/* Full desktop update */
void desktop_update_full(desktop_td *desktop)
{
    LOGGER_TRACE("Fully updating desktop %u ('%s')",
            desktop->id, desktop->name);

    /* Soft update */
    desktop_update(desktop);

    /* Update all windows on the hash table */
    for (size_t i = 0; i < desktop->windows->positions; ++i) {
        /* Check if the position has an element */
        if (desktop->windows->table[i] != NULL &&
            desktop->windows->table[i] != desktop->windows->vacated) {
            /* Get the window and update it */
            window_td *window =
                (window_td *) desktop->windows->table[i];
            window_update(window);
        }
    }

    LOGGER_TRACE("Updated desktop %u ('%s')",
            desktop->id, desktop->name);
}


/* Clear a desktop by removing all its windows */
void desktop_clear(desktop_td *desktop)
{
    LOGGER_DEBUG("Preparing to clear desktop %u ('%s')",
            desktop->id, desktop->name);
    if (desktop != NULL && desktop->windows != NULL) {
        ohtbl_destroy(desktop->windows);
    }
}


/* Add a previously allocated window in the desktop */
int desktop_action_window_add(desktop_td *desktop, window_td *window)
{
    LOGGER_DEBUG("Preparing to add window %#lx ('%s') to" \
            " desktop %u ('%s')",
            window->id, window->name, desktop->id, desktop->name);

    if (desktop == NULL || window == NULL) {
        return -1;
    }

    /* Add window */
    if (ohtbl_insert(desktop->windows, (void *) window) != 0) {
        LOGGER_ALERT("Failed to allocate memory for window" \
                " %#lx on desktop %u ('%s') on screen %u",
                window->id,
                desktop->id, desktop->name, desktop->screen_id);
        return -1;
    }

    LOGGER_TRACE("Added window %#lx ('%s') to desktop %u ('%s')",
            window->id, window->name, desktop->id, desktop->name);
    return 0;
}


/* Remove a window from the desktop */
int desktop_action_window_rem(desktop_td *desktop, window_td *window)
{
    LOGGER_DEBUG("Preparing to remove window %#lx ('%s') from" \
            " desktop %u ('%s')",
            window->id, window->name, desktop->id, desktop->name);

    if (desktop == NULL || window == NULL) {
        return -1;
    }

    /* Search window in list of windows */
    void *removed_window = NULL;
    if (ohtbl_remove(desktop->windows,
                (void **) &removed_window) == 0) {
        window_td *removed_window_td = (window_td *) removed_window;
        if (removed_window_td != NULL &&
            removed_window_td->id == window->id) {
            /* Remove window */
            window_destroy(removed_window_td);
            LOGGER_TRACE("Removing window %#lx ('%s') from" \
                    " desktop %u ('%s')",
                    window->id, window->name,
                    desktop->id, desktop->name);
            return 0;
        }
    }

    /* Window not found */
    LOGGER_TRACE("Window %#lx ('%s') not found on desktop %u ('%s')",
            window->id, window->name, desktop->id, desktop->name);
    return -1;
}
