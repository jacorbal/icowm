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
        /* Allocate memory for the window manager */
        wm = malloc(sizeof(wm_td));
        if (wm == NULL) {
            logger_msg(LOG_FATAL,
                    "Cannot allocate memory for window manager");
            return NULL;
        }

        logger_msg(LOG_DEBUG, "Opening X display");
        Display *display = XOpenDisplay(NULL);
        if (display == NULL) {
            logger_msg(LOG_FATAL, "Unable to open X display");
            free(wm);
            return NULL;
        }

        /* Handle screens */
        logger_msg(LOG_TRACE, "Allocating memory for screens");
        wm->screens = list_init((void(*)(void *)) screen_destroy);
        if (wm->screens == NULL) {
            logger_msg(LOG_FATAL,
                    "Failed to allocate memory for screens array");
            XCloseDisplay(display);
            free(wm);
            return NULL;
        }

        /* Initialize screens */
        logger_msg(LOG_TRACE, "Initializing screens");
        for (unsigned int i = 0;
                i < (unsigned int) ScreenCount(display); ++i) {
            screen_td *screen = screen_init(display, i);
            if (screen == NULL) {
                logger_msg(LOG_FATAL, "Failed to initialize screen %#x", i);
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
                        "Failed to insert screen %#x into screen list", i);
                screen_destroy(screen);
                list_destroy(wm->screens);
                XCloseDisplay(display);
                free(wm);
                return NULL;
            }
        }

        /* Set and load configuration */
        logger_msg(LOG_TRACE,
                "Loading configuration into window manager");
        wm->config = config;
        config_load(wm->config);

        /* Events */
        wm->event_handler = NULL;

        /* Begin! */
        logger_msg(LOG_TRACE, "Setting 'is_running' to \"true\"");
        wm->is_running = true;
    }

    return wm;
}


/* Destroy window manager instance */
void wm_destroy(wm_td *wm)
{
    logger_msg(LOG_DEBUG, "Deallocating window manager structure");
    if (wm == NULL) {
        return;
    }

    logger_msg(LOG_TRACE, "Deallocating screens for window manager");
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
        return;
    }

    logger_msg(LOG_TRACE, "Catching events...");
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
