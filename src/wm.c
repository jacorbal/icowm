/**
 * @file wm.c
 *
 * @brief Window manager implementation
 */

/* System includes */
#include <stdbool.h>    /* true */
#include <stdlib.h>     /* free, malloc */
#include <stdio.h>      /* fprintf */

/* External libraries */
#include <X11/Xlib.h>   /* XOpenDisplay, XCloseDisplay */
#include <X11/keysym.h> /* XK_* */

/* ADT */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
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
            logger_msg(LOG_FATAL, "Unable to open X display");
            free(wm);
            return NULL;
        }

        /* Set and load configuration */
        logger_msg(LOG_TRACE,
                "Loading configuration into window manager");
        wm->config = config;
        config_load(wm->config);

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

        /* Events */
        wm->event_handler = NULL;

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

    logger_msg(LOG_TRACE, "Destroying window manager");
    free(wm);
}


/* Window manager main loop */
void wm_loop(wm_td *wm)
{
    logger_msg(LOG_DEBUG, "Starting main event loop");
    if (wm == NULL || !wm->is_running) {
        logger_msg(LOG_TRACE, "Exiting event loop");
        return;
    }

    logger_msg(LOG_DEBUG, "Listening for events...");
    while (wm->is_running) {
        XEvent event;

        /* Wait for an event */
//        XNextEvent(wm->screens[0].display, &event);
        // Assuming we are handling events from the first screen

        /* Call the event handler (if it is set) */
        if (wm->event_handler != NULL) {
            wm->event_handler(&event);
        }

        /* Process the event (more event processing can occur here) */
    }
}
