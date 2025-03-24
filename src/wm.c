/**
 * @file wm.c
 *
 * @brief Window manager implementation
 */

/* System includes */
#include <stdbool.h>    /* bool, false, true */
#include <stdlib.h>     /* NULL, free, malloc */

/* External libraries */
#include <X11/Xlib.h>   /* XOpenDisplay, XCloseDisplay */
#include <X11/keysym.h> /* XK_* */

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
#include <event.h>
#include <logger.h>
#include <screen.h>

/* Local includes */
#include <wm.h>


/* Though variable static dost often lurk near,
 * In shadows of scope, few e’er call thee their own,
 * Thy global existence, to none dost bring fear,
 * A sentinel watching, though thou art alone. */
static wm_td *wm = NULL;    /**< Window manager singleton pointer */


/**
 * @brief Soft window manager update
 *
 * @note Complexity: @e O(1)
 */
static void _wm_update(void)
{
//    LOGGER_TRACE("Updating window manager", L_NARG);
}


/**
 * @brief Full window manager update
 *
 * This function updates the window manager by updating every window on
 * every desktop of every screen.
 *
 * @note Complexity: @e O(n*m), where @e n is the number of screens and
 *       @e m is the number of desktops on the screen
 */
static void _wm_update_full(void)
{
    LOGGER_TRACE("Fully updating window manager", L_NARG);

    /* Soft update */
    _wm_update();

    /* Update all screens */
    for (list_item_td *screen_node = list_head(wm->screens);
         screen_node != NULL;
         screen_node = list_next(screen_node)) {
        screen_td *screen_cur = (screen_td *) list_data(screen_node);
        if (screen_cur->is_outdated) {
            screen_update_full(screen_cur);
        }
    }
}


/**
 * @brief Enters the main event handling loop of the window manager
 *
 * This function runs continuously while the window manager is active,
 * listening for X11 events and passing them to the event handler for
 * processing.  It uses @p XNextEvent to wait for incoming events from
 * the X server, enabling responsive behavior in window management.
 * The condition to end the loop is by setting @p is_running to @c false.
 *
 * @note The event loop will stop when the @p is_running flag is set to
 *       @c false, which should be handled in response to user actions
 *       or when the window manager is terminating
 * @note Complexity: @e O(1) for each event processed; however, the
 *       overall time complexity depends on the number of events
 *       processed, so each call to @e event_handle may have a different
 *       complexity based on the event type and operations performed
 */
static void _wm_loop(void)
{
    if (wm == NULL || !wm->is_running) {
        LOGGER_TRACE("Window manager is not initialized" \
                "or set not to run", L_NARG);
        return;
    }

    /* Update window manager before start */
    _wm_update_full();

    LOGGER_DEBUG("Entering main event loop", L_NARG);
    while (wm->is_running) {
        XEvent event;
        event_handler_process(wm->event_handler, &event);

        while (XPending(wm->display) > 0) {
            XNextEvent(wm->display, &event);

            /* Key pressed */
            if (event.type == KeyPress) {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);

                /* Quit: Ctr+Alt+Shift+Backspace */
                if (keysym == XK_BackSpace &&
                        (event.xkey.state & ControlMask) &&
                        (event.xkey.state & Mod1Mask) &&
                        (event.xkey.state & ShiftMask)) {
                    LOGGER_TRACE("Setting 'is_running' status to 'false'",
                            L_NARG);
                    wm->is_running = false;
                    break;
                }
            } /* ! if (KeyPress) */
        } /* ! while (XPending) */

        /* Update the window manager */
        _wm_update();
        XFlush(wm->display);    //<- ?
    }
    LOGGER_DEBUG("Exiting event loop", L_NARG);
}


/* Initialize a window manager instance */
int wm_start(const char *display_name)
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
        /* If 'display_name' is NULL, then it defaults to the value of
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
        config_load(wm->config);

        /* Events */
        wm->event_handler = event_handler_init();
        if (wm->event_handler == NULL) {
            LOGGER_FATAL("Failed to initialize event handler", L_NARG);
            XCloseDisplay(wm->display);
            config_destroy(wm->config);
            free(wm);
            return 4;
        }

        /* Handle screens */
        LOGGER_TRACE("Allocating memory for screen structures", L_NARG);
        wm->screens = list_init((void(*)(void *)) screen_destroy);
        if (wm->screens == NULL) {
            LOGGER_FATAL("Failed to allocate memory for screens array",
                    L_NARG);
            event_handler_destroy(wm->event_handler);
            XCloseDisplay(wm->display);
            config_destroy(wm->config);
            free(wm);
            return 5;
        }

        /* Initialize screens */
        LOGGER_TRACE("Initializing screen structures", L_NARG);
        /* NOTE.  Maybe there are more screens defined in the
         *        configuration file, but only those detected will be
         *        initialized, hence the 'ScreenCount(display)' instead
         *        of taking the JSON information.  In the same way,
         *        maybe there are 'n' screens, but only want to use
         *        the 'm < n' defined in the JSON file. */
        screens_detected = (unsigned int) ScreenCount(wm->display);
        if (wm->config->base.screen_count != screens_detected) {
            LOGGER_INFO("Detected %u screens; %u are specified in" \
                    " the configuration file",
                    screens_detected, wm->config->base.screen_count);
            if (wm->config->base.screen_count >= screens_detected) {
                wm->config->base.screen_count = screens_detected;
            }
        }

        LOGGER_NOTICE("Setting number of screens to: %u",
                wm->config->base.screen_count);

        for (unsigned int i = 0; i < wm->config->base.screen_count; ++i) {
            /* Get number of desktops for this screen */
            unsigned int desktops_count =
                wm->config->base.screens[i].desktop_count;
            screen_td *screen =
                screen_init(wm->display, i, desktops_count, wm->config);
            if (screen == NULL) {
                LOGGER_FATAL("Failed to initialize screen %u", i);
                list_destroy(wm->screens);
                XCloseDisplay(wm->display);
                event_handler_destroy(wm->event_handler);
                config_destroy(wm->config);
                free(wm);
                return 6;
            }

            LOGGER_TRACE("Inserting screen %u into screen list", i);
            if (list_ins_next(wm->screens,
                        list_tail(wm->screens),
                        (const void *) screen) != 0) {
                LOGGER_FATAL("Failed to insert screen " \
                        "%u into screen list", i);
                screen_destroy(screen);
                list_destroy(wm->screens);
                XCloseDisplay(wm->display);
                event_handler_destroy(wm->event_handler);
                config_destroy(wm->config);
                free(wm);
                return 7;
            }

            screen->desktop_count =
                wm->config->base.screens[i].desktop_count;
            screen->desktop_cur =
                wm->config->base.screens[i].desktop_inaugural;

            LOGGER_TRACE("Setting desktop %u as the startup desktop" \
                    " on screen %u", screen->desktop_cur, i);
        }

        /* Begin! */
        LOGGER_TRACE("Setting 'is_running' status to 'true'", L_NARG);
        wm->is_running = true;
        _wm_loop();

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

    LOGGER_TRACE("Deallocating screens in window manager", L_NARG);
    list_destroy(wm->screens);

    event_handler_destroy(wm->event_handler);

    LOGGER_TRACE("Closing X display", L_NARG);
    XCloseDisplay(wm->display);

    config_destroy(wm->config);

    LOGGER_TRACE("Destroying window manager", L_NARG);
    free(wm);
    wm = NULL;  /* Make sure the singleton points back to 'NULL' */

    return 0;
}


/* Window manager screen count */
size_t wm_screen_count(void)
{
    return ((wm == NULL) ? 0 : wm->screens->size);
}
