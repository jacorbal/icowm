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


/* Initialize window manager instance */
wm_td *wm_init(config_td *config)
{
    static wm_td *wm = NULL;    /* Singleton instance */

    logger_msg(LOG_DEBUG, "Initializing window manager");
    if (wm == NULL) {
        unsigned int screens_detected;

        /* Allocate memory for the window manager */
        wm = malloc(sizeof(wm_td));
        if (wm == NULL) {
            logger_msg(LOG_FATAL,
                    "Failed to allocate memory for window manager");
            return NULL;
        }

        logger_msg(LOG_DEBUG, "Opening X display");
        Display *display = XOpenDisplay(NULL);
        if (display == NULL) {
            logger_msg(LOG_FATAL, "Failed to open X display");
            free(wm);
            return NULL;
        }

        /* Set and load configuration */
        logger_msg(LOG_TRACE,
                "Loading configuration into window manager");
        wm->config = config;
        config_load(wm->config);

        /* Events */
        wm->event_handler = event_handler_init();
        if (wm->event_handler == NULL) {
            logger_msg(LOG_FATAL,
                    "Failed to initialize event handler");
            free(wm);
            return NULL;
        }

        /* Handle screens */
        logger_msg(LOG_TRACE,
                "Allocating memory for screen structures");
        wm->screens = list_init((void(*)(void *)) screen_destroy);
        if (wm->screens == NULL) {
            logger_msg(LOG_FATAL,
                    "Failed to allocate memory for screens array");
            XCloseDisplay(display);
            free(wm);
            return NULL;
        }

        /* Initialize screens */
        logger_msg(LOG_TRACE, "Initializing screen structures");
        /* NOTE.  Maybe there are more screens defined in the
         *        configuration file, but only those detected will be
         *        initialized, hence the 'ScreenCount(display)' instead
         *        of taking the JSON information.  In the same way,
         *        maybe there are n screens, but only want to use m<n
         *        defined in the JSON file. */
        screens_detected = (unsigned int) ScreenCount(display);
        if (config->base.screen_count != screens_detected) {
            logger_msg(LOG_INFO,
                "Detected %u screens; %u are specified in the" \
                " configuration file",
                    screens_detected, config->base.screen_count);
            if (config->base.screen_count < screens_detected) {
                config->base.screen_count = config->base.screen_count;
            } else {
                config->base.screen_count = screens_detected;
            }
        }
    
        logger_msg(LOG_NOTICE, "Setting number of screens to: %u",
                config->base.screen_count);

        for (unsigned int i = 0; i < config->base.screen_count; ++i) {
            /* Get number of desktops for this screen */
            unsigned int desktops_count =
                config->base.screens[i].desktop_count;
            screen_td *screen =
                screen_init(display, i, desktops_count, wm->config);
            if (screen == NULL) {
                logger_msg(LOG_FATAL,
                        "Failed to initialize screen %u", i);
                list_destroy(wm->screens);
                XCloseDisplay(display);
                free(wm);
                return NULL;
            }

            logger_msg(LOG_TRACE,
                    "Inserting detected screens into screen list");
            if (list_ins_next(wm->screens,
                        list_tail(wm->screens),
                        (const void *) screen) != 0) {
                logger_msg(LOG_FATAL,
                        "Failed to insert screen %u into screen list", i);
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

            logger_msg(LOG_TRACE,
                "Setting desktop %u as the startup desktop on screen %u",
                screen->desktop_cur, i);
        }

        /* Begin! */
        logger_msg(LOG_TRACE, "Setting 'is_running' status to 'true'");
        wm->is_running = true;
    }

    return wm;
}


/* Destroy window manager instance */
void wm_destroy(wm_td *wm)
{
    logger_msg(LOG_DEBUG, "Deallocating structure for window manager");
    if (wm == NULL) {
        return;
    }

    logger_msg(LOG_TRACE, "Deallocating screens in window manager");
    list_destroy(wm->screens);
    config_destroy(wm->config);
    event_handler_destroy(wm->event_handler);

    logger_msg(LOG_TRACE, "Destroying window manager");
    free(wm);
}


/* Update window manager */
void wm_update(wm_td *wm)
{
    /* Update all screens */
    for (list_item_td *screen_node = list_head(wm->screens); 
         screen_node != NULL; 
         screen_node = list_next(screen_node)) {
        screen_td *screen_cur = (screen_td *) list_data(screen_node);
        screen_update(screen_cur);
    }
}


/* Window manager main loop */
void wm_loop(wm_td *wm)
{
    XEvent event;

    if (wm == NULL || !wm->is_running) {
        logger_msg(LOG_TRACE,
                "Window manager is not initialized" \
                "or set not to run");
        return;
    }

    logger_msg(LOG_DEBUG, "Entering main event loop");
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
                        XNextEvent(window->display, &event);
                        event_handler_generic(wm->event_handler, window,
                                &event);
                    }
                } /* ! for (windows) */
            } /* ! for (desktops) */
        } /* ! for (screens) */
    }
    logger_msg(LOG_DEBUG, "Exiting event loop");
}
