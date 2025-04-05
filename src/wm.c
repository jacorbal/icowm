/**
 * @file wm.c
 *
 * @brief Window manager implementation
 */

/* System includes */
#include <stdbool.h>
#include <stdlib.h>     /* NULL, free, malloc */

/* X11 includes */
#include <X11/Xlib.h>   /* XOpenDisplay, XCloseDisplay */
#include <X11/keysym.h> /* XK_* */

/* ADT includes */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
#include <eventq.h>
#include <logger.h>
#include <surface.h>

/* Local includes */
#include <wm.h>


/* Though variable static dost often lurk near,
 * In shadows of scope, few e’er call thee their own,
 * Thy global existence, to none dost bring fear,
 * A sentinel watching, though thou art alone. */
static wm_td *wm = NULL;    /**< Pointer to the singleton instance of
                                 the window manager */


/**
 * @brief Soft window manager update
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_update(void)
{
//    LOGGER_TRACE("Updating window manager", L_NARG);
}


/**
 * @brief Full window manager update
 *
 * Updates the window manager by updating every window on every desktop
 * of every surface.
 *
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       and @e m is the number of desktops on the surface
 */
static void s_wm_update_full(void)
{
    LOGGER_TRACE("Fully updating window manager", L_NARG);

    /* Soft update */
    s_wm_update();

    /* Update all surfaces */
    for (list_item_td *surface_node = list_head(wm->surfaces);
         surface_node != NULL;
         surface_node = list_next(surface_node)) {
        surface_td *surface_cur = (surface_td *) list_data(surface_node);
        if (surface_cur->is_outdated) {
            surface_update_full(surface_cur);
        }
    }
}


/**
 * @brief Enters the main event handling loop of the window manager
 *
 * Runs continuously while the window manager is active, listening for
 * X11 events and passing them to the event handler for processing.  It
 * uses @p XNextEvent to wait for incoming events from the X server,
 * enabling responsive behavior in window management.  The condition to
 * end the loop is by setting @p is_running to @c false.
 *
 * @note The event loop will stop when the @p is_running flag is set to
 *       @c false, which should be handled in response to user actions
 *       or when the window manager is terminating
 * @note Complexity: @e O(1) for each event processed; however, the
 *       overall time complexity depends on the number of events
 *       processed, so each call to @a event_handle may have a different
 *       complexity based on the event type and operations performed
 */
static void s_wm_loop(void)
{
    if (wm == NULL || !wm->is_running) {
        LOGGER_TRACE("Window manager is not initialized" \
                "or set to not run", L_NARG);
        return;
    }

    /* Update window manager before start */
    s_wm_update_full();

    LOGGER_DEBUG("Entering main event loop", L_NARG);
    while (wm->is_running) {
        XEvent xevent;
        //event_handler_process(wm->event_handler, &event);

        /* Process window manager events from event priority queue */
        eventq_process();

        /* Process X events */
        while (XPending(wm->display) > 0) {
            XNextEvent(wm->display, &xevent);

            /* Quit: Ctr+Alt+Shift+Backspace */
            switch (xevent.type) {
                case KeyPress:
                {
                    KeySym keysym = XLookupKeysym(&xevent.xkey, 0);

                    if (keysym == XK_BackSpace &&
                            (xevent.xkey.state & ControlMask) &&
                            (xevent.xkey.state & Mod1Mask) &&
                            (xevent.xkey.state & ShiftMask)) {
                        LOGGER_TRACE("Setting 'is_running' status" \
                                " flag to 'false'", L_NARG);
                        wm->is_running = false;
                    } else {
                        //event_handler_handle_process(&event);
                    }
                    break;
                }

                case ConfigureNotify:
                    /* Handle window resize or move events */
                    //event_handler_handle_configure(&event);
                    break;

                case MapNotify:
                    /* Handle window mapping events (when a window
                     * is shown) */
                    //event_handler_handle_map(&event);
                    break;

                case UnmapNotify:
                    /* Handle window unmapping events (when a window
                     * is hidden) */
                    // event_handler_handle_unmap(&event);
                    break;

                    /* Add cases for other event types as required */
                default:
                    /* Optional: handle unknown events if needed */
                    break;
            } /* switch (event.type) */
        } /* ! while (XPending) */

        /* Update the window manager */
        s_wm_update();
    } /* ! while (is_running) */
    LOGGER_DEBUG("Exiting event loop", L_NARG);
}


/* Initialize a window manager instance */
int wm_start(const char *display_name, const char *config_dir_prefix)
{
    LOGGER_DEBUG("Initializing window manager", L_NARG);
    if (wm == NULL) {
        unsigned int screens_detected;

        /* Allocate memory for the window manager */
        wm = malloc(sizeof(wm_td));
        if (wm == NULL) {
            LOGGER_FATAL("Failed to allocate memory for window manager",
                    L_NARG);
            return 1;
        }

        LOGGER_DEBUG("Opening X display", L_NARG);
        /* If 'display_name' is 'NULL', then it defaults to the value of
         * the 'DISPLAY' environment variable */
        wm->display = XOpenDisplay(display_name);
        if (wm->display == NULL) {
            if (XDisplayName(NULL)[0] == '\0') {
                LOGGER_FATAL("Failed to open X display", L_NARG);
            } else {
                LOGGER_FATAL("Failed to open X display: '%s'", \
                        XDisplayName(NULL));
            }
            free(wm);
            return 2;
        }

        /* Set and load configuration */
        wm->config = config_init();
        if (wm->config == NULL) {
            return 3;
        }

        LOGGER_TRACE("Loading configuration into window manager",
                L_NARG);
        config_load(wm->config, config_dir_prefix);

        /* Events: priority queue as min-heap (bottom-heavy heap) */
        if (eventq_start() != 0) {
            LOGGER_FATAL("Failed to initialize event priority queue",
                    L_NARG);
            config_destroy(wm->config);
            XCloseDisplay(wm->display);
            free(wm);
            return 4;
        }

        /* Handle surfaces */
        LOGGER_TRACE("Allocating memory for surface structures", L_NARG);
        wm->surfaces = list_init((void(*)(void *)) surface_destroy);
        if (wm->surfaces == NULL) {
            LOGGER_FATAL("Failed to allocate memory for surfaces array",
                    L_NARG);
            eventq_stop();
            config_destroy(wm->config);
            XCloseDisplay(wm->display);
            free(wm);
            return 5;
        }

        /* Initialize surfaces */
        LOGGER_TRACE("Initializing surface structures", L_NARG);
        /* NOTE.  Maybe there are more surfaces defined in the
         *        configuration file, but only those detected will be
         *        initialized, hence the 'ScreenCount(display)' instead
         *        of taking the JSON information.  In the same way,
         *        maybe there are 'n' surfaces, but only want to use
         *        the 'm < n' defined in the JSON file. */
        screens_detected = (unsigned int) ScreenCount(wm->display);
        if (wm->config->base.screen_count != screens_detected) {
            LOGGER_NOTICE("Detected %u surfaces; %u are specified in" \
                    " the configuration file",
                    screens_detected, wm->config->base.screen_count);
            if (wm->config->base.screen_count >= screens_detected) {
                wm->config->base.screen_count = screens_detected;
            }
            LOGGER_INFO("Setting number of surfaces to: %u",
                    wm->config->base.screen_count);
        } else {
            LOGGER_DEBUG("Setting number of surfaces to: %u",
                    wm->config->base.screen_count);
        }

        for (size_t i = 0; i < wm->config->base.screen_count; ++i) {
            /* Get number of desktops for this surface */
            unsigned int desktops_count =
                wm->config->base.screens[i].desktop_count;
            surface_td *surface =
                surface_init(wm->display, i, desktops_count, wm->config);
            if (surface == NULL) {
                LOGGER_FATAL("Failed to initialize surface %lu", i);
                list_destroy(wm->surfaces);
                eventq_stop();
                config_destroy(wm->config);
                XCloseDisplay(wm->display);
                free(wm);
                return 6;
            }

            LOGGER_TRACE("Inserting surface %lu into surface list", i);
            if (list_ins_next(wm->surfaces,
                        list_tail(wm->surfaces),
                        (const void *) surface) != 0) {
                LOGGER_FATAL("Failed to insert surface " \
                        "%lu into surface list", i);
                surface_destroy(surface);
                list_destroy(wm->surfaces);
                eventq_stop();
                config_destroy(wm->config);
                XCloseDisplay(wm->display);
                free(wm);
                return 7;
            }

            surface->desktop_count =
                wm->config->base.screens[i].desktop_count;
            surface->desktop_cur =
                wm->config->base.screens[i].desktop_inaugural;

            LOGGER_TRACE("Setting desktop %lu as the startup desktop" \
                    " on surface %lu", surface->desktop_cur, i);
        }

        /* Begin! */
        LOGGER_TRACE("Setting 'is_running' status flag to 'true'",
                L_NARG);
        wm->is_running = true;
        s_wm_loop();

        return 0;
    }

    return -1;
}


/* Destroy window manager instance */
int wm_stop(void)
{
    LOGGER_DEBUG("Deallocating structure for window manager", L_NARG);
    if (wm == NULL) {
        return 1;
    }

    /* Deallocate every surface */
    LOGGER_TRACE("Deallocating surfaces in window manager", L_NARG);
    list_destroy(wm->surfaces);

    /* Stop event priority queue */
    eventq_stop();

    /* Destroy configuration structure */
    config_destroy(wm->config);

    /* Close the display */
    LOGGER_TRACE("Closing X display", L_NARG);
    XCloseDisplay(wm->display);

    LOGGER_TRACE("Destroying window manager", L_NARG);
    free(wm);
    wm = NULL;  /* Reset the singleton instance pointer to 'NULL' */

    LOGGER_DEBUG("Window manager has been destroyed", L_NARG);

    return 0;
}
