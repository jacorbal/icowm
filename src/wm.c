/**
 * @file wm.c
 *
 * @brief Window manager implementation
 */

/* System includes */
#include <stdbool.h>    /* true */
#include <stdlib.h>     /* NULL, free, malloc */

/* External libraries */
#include <X11/Xlib.h>   /* XOpenDisplay, XCloseDisplay */

/* ADT */
#include <adt/cdlist.h> /* Doubly linked circular list */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
#include <event.h>
#include <logger.h>
#include <screen.h>

/* Local includes */
#include <wm.h>


/* Initialize a window manager instance */
wm_td *wm_init(config_td *config)
{
    static wm_td *wm = NULL;    /* Singleton instance */

    LOGGER_DEBUG("Initializing window manager", L_NARG);
    if (wm == NULL) {
        unsigned int screens_detected;

        /* Allocate memory for the window manager */
        wm = malloc(sizeof(wm_td));
        if (wm == NULL) {
            LOGGER_FATAL("Failed to allocate memory for window manager",
                    L_NARG);
            return NULL;
        }

        LOGGER_DEBUG("Opening X display", L_NARG);
        Display *display = XOpenDisplay(NULL);
        if (display == NULL) {
            LOGGER_FATAL("Failed to open X display", L_NARG);
            free(wm);
            return NULL;
        }

        /* Set and load configuration */
        LOGGER_TRACE("Loading configuration into window manager",
                L_NARG);
        wm->config = config;
        config_load(wm->config);

        /* Events */
        wm->event_handler = event_handler_init();
        if (wm->event_handler == NULL) {
            LOGGER_FATAL("Failed to initialize event handler", L_NARG);
            free(wm);
            return NULL;
        }

        /* Handle screens */
        LOGGER_TRACE("Allocating memory for screen structures", L_NARG);
        wm->screens = list_init((void(*)(void *)) screen_destroy);
        if (wm->screens == NULL) {
            LOGGER_FATAL("Failed to allocate memory for screens array",
                    L_NARG);
            XCloseDisplay(display);
            free(wm);
            return NULL;
        }

        /* Initialize screens */
        LOGGER_TRACE("Initializing screen structures", L_NARG);
        /* NOTE.  Maybe there are more screens defined in the
         *        configuration file, but only those detected will be
         *        initialized, hence the 'ScreenCount(display)' instead
         *        of taking the JSON information.  In the same way,
         *        maybe there are n screens, but only want to use m<n
         *        defined in the JSON file. */
        screens_detected = (unsigned int) ScreenCount(display);
        if (config->base.screen_count != screens_detected) {
            LOGGER_INFO("Detected %u screens; %u are specified in" \
                    " the configuration file",
                    screens_detected, config->base.screen_count);
            if (config->base.screen_count < screens_detected) {
                config->base.screen_count = config->base.screen_count;
            } else {
                config->base.screen_count = screens_detected;
            }
        }

        LOGGER_NOTICE("Setting number of screens to: %u",
                config->base.screen_count);

        for (unsigned int i = 0; i < config->base.screen_count; ++i) {
            /* Get number of desktops for this screen */
            unsigned int desktops_count =
                config->base.screens[i].desktop_count;
            screen_td *screen =
                screen_init(display, i, desktops_count, wm->config);
            if (screen == NULL) {
                LOGGER_FATAL("Failed to initialize screen %u", i);
                list_destroy(wm->screens);
                XCloseDisplay(display);
                free(wm);
                return NULL;
            }

            LOGGER_TRACE("Inserting detected screens into screen list",
                    NULL);
            if (list_ins_next(wm->screens,
                        list_tail(wm->screens),
                        (const void *) screen) != 0) {
                LOGGER_FATAL("Failed to insert screen " \
                        "%u into screen list", i);
                screen_destroy(screen);
                list_destroy(wm->screens);
                XCloseDisplay(display);
                free(wm);
                return NULL;
            }

            screen->desktop_count =
                config->base.screens[i].desktop_count;
            screen->desktop_cur =
                config->base.screens[i].desktop_inaugural;

            LOGGER_TRACE("Setting desktop" \
                    "%u as the startup desktop on screen %u",
                    screen->desktop_cur, i);
        }

        /* Begin! */
        wm->is_running = true;
        LOGGER_TRACE("Setting 'is_running' status to '%d'",
                wm->is_running);
    }

    return wm;
}


/* Destroy window manager instance */
void wm_destroy(wm_td *wm)
{
    LOGGER_DEBUG("Deallocating structure for window manager", L_NARG);
    if (wm == NULL) {
        return;
    }

    LOGGER_TRACE("Deallocating screens in window manager", L_NARG);
    list_destroy(wm->screens);
    config_destroy(wm->config);
    event_handler_destroy(wm->event_handler);

    LOGGER_TRACE("Destroying window manager", L_NARG);
    free(wm);
}


/* Soft window manager update */
void wm_update(wm_td *wm)
{
    LOGGER_TRACE("Updating window manager", L_NARG);
}


/* Full window manager update */
void wm_update_full(wm_td *wm)
{
    LOGGER_TRACE("Force updating window manager", L_NARG);

    /* Soft update */
    wm_update(wm);

    /* Update all screens */
    for (list_item_td *screen_node = list_head(wm->screens);
         screen_node != NULL;
         screen_node = list_next(screen_node)) {
        screen_td *screen_cur = (screen_td *) list_data(screen_node);
        screen_update_full(screen_cur);
    }
}


/* Window manager main loop */
void wm_loop(wm_td *wm)
{
    XEvent event;

    if (wm == NULL || !wm->is_running) {
        LOGGER_TRACE("Window manager is not initialized" \
                "or set not to run", L_NARG);
        return;
    }

    LOGGER_DEBUG("Entering main event loop", L_NARG);
    while (wm->is_running) {
        /* For each screen ('list_td *') */
        for (list_item_td *screen_item = list_head(wm->screens);
             screen_item != NULL;
             screen_item = list_next(screen_item)) {
            screen_td *screen = (screen_td *) list_data(screen_item);

            /* For each desktop ('cdlist_td *') */
            for (cdlist_item_td *desktop_item =
                    cdlist_head(screen->desktops);
                 desktop_item != NULL;
                 desktop_item = cdlist_next(desktop_item)) {
                desktop_td *desktop =
                    (desktop_td *) cdlist_data(desktop_item);

                /* Update only the current desktop (the visible one) on
                 * this screen only if needed */
                if (screen->desktop_cur == desktop->id &&
                        desktop->is_outdated) {
                    /* For each window ('ohtbl_td *') */
                    for (size_t i = 0;
                         i < ohtbl_size(desktop->windows);
                         ++i) {
                        void *window_data = NULL;

                        /* Lookup the window in the hash table */
                        int result = ohtbl_lookup(desktop->windows,
                                &window_data);
                        if (result == 0 && window_data != NULL) {
                            window_td *window = (window_td *) window_data;

                            /* Wait for an event on the current screen */
                            while (XPending(window->display) > 0) {
                                XNextEvent(window->display, &event);
                                event_handler_process(wm->event_handler,
                                        window, &event);
                            } /* ! while (XPending) */
                        } /* ! if (window */
                    } /* ! for (windows) */
                } /* ! if (desktop_cur && desktop->is_outdated) */
            } /* ! for (desktops) */
        } /* ! for (screens) */

        /* Update the window manager */
        wm_update(wm);
    }
    LOGGER_DEBUG("Exiting event loop", L_NARG);
}
