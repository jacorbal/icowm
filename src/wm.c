/**
 * @file wm.c
 *
 * @brief Window manager implementation
 */
/*
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* NULL, free, malloc */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <config.h>
#include <eventq.h>
#include <logger.h>
#include <render/surface.h>
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
            /* Render all desktops on this surface */
            if (surface_render_all_desktops(surface_cur) != 0) {
                LOGGER_ERROR("Failed to render surface %u",
                        surface_cur->id);
            }
        } else {
            /* Just update the current desktop */
            if (surface_render_current_desktop(surface_cur) != 0) {
                LOGGER_ERROR("Failed to render current desktop on" \
                        " surface %u", surface_cur->id);
            }
        }
    } /* ! for (surface_node) */
}


/**
 * @brief Enters the main event handling loop of the window manager
 *
 * Runs continuously while the window manager is active, listening for
 * XCB events and passing them to the event handler for processing.  It
 * uses @a xcb_poll_for_event to wait for incoming events from the
 * X server, enabling responsive behavior in window management.  The
 * condition to end the loop is by setting @p is_running to @c false.
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
    xcb_key_symbols_t *keysyms;

    if (wm == NULL || !wm->is_running) {
        LOGGER_TRACE("Window manager is not initialized" \
                " or set to not run", L_NARG);
        return;
    }

    /* Update window manager before start */
    s_wm_update_full();

#ifdef DEBUG
    /* TEST: Create dummy windows to test rendering */
    if (wm->surfaces != NULL) {
        list_item_td *surface_node = list_head(wm->surfaces);
        if (surface_node != NULL) {
            surface_td *surface = (surface_td *) list_data(surface_node);

            if (surface != NULL && surface->desktops != NULL) {
                cdlist_item_td *desktop_node = cdlist_head(surface->desktops);
                /* Iterate to the current desktop */
                for (uint32_t i = 0; i < surface->desktop_cur; ++i) {
                    desktop_node = cdlist_next(desktop_node);
                    if (desktop_node == NULL) {
                        break;
                    }
                }

                if (desktop_node != NULL) {
                    desktop_td *desktop =
                        (desktop_td *) cdlist_data(desktop_node);
                    if (desktop != NULL) {
                        /* Create a test window */
                        client_td *test_client = client_init(
                                wm->connection,
                                wm->ewmh,
                                surface->screen->root,  /* Parent window */
                                100, 100,   /* width, height */
                                50, 50,     /* x, y position */
                                &(wm->config->theme));

                        if (test_client != NULL) {
                            /* Remove 'HIDDEN' flag */
                            safeflg_unset(&(test_client)->properties.flags,
                                    CLIENT_FLAG_HIDDEN, CLIENT_FLAG_MAX);

                            desktop_action_client_add(desktop, test_client);
                            desktop->is_outdated = true;
                            LOGGER_DEBUG("Created test client for rendering",
                                    L_NARG);
                        } /* ! if (test_client) */
                    } /* ! if (desktop) */
                } /* if (desktop_node) */
            } /* ! if (surface) */
        } /* ! if (surface_node) */
    }
#endif  /* ! DEBUG */

    keysyms = xcb_key_symbols_alloc(wm->connection);

    LOGGER_DEBUG("Entering main event loop", L_NARG);
    while (wm->is_running) {
        xcb_generic_event_t *event;
        //event_handler_process(wm->event_handler, &event);

        /* Process window manager events from event priority queue */
        eventq_process();

        /* Process X events */
        while ((event = xcb_poll_for_event(wm->connection))) {
            xcb_keysym_t keysym;
            xcb_key_press_event_t *key_event;

            switch (event->response_type & ~0x80) { /* Ignore error bits */
                case XCB_KEY_PRESS:
                    key_event = (xcb_key_press_event_t *) event;
                    keysym = xcb_key_symbols_get_keysym(keysyms,
                            key_event->detail, 0);

                    /* Ctrl+Mod1+Shift+Backspace */
                    if (keysym == 0x0078 &&
                            (key_event->state & XCB_MOD_MASK_CONTROL) &&
                            (key_event->state & XCB_MOD_MASK_1) &&
                            (key_event->state & XCB_MOD_MASK_SHIFT)) {
                        LOGGER_TRACE("Setting 'is_running' status" \
                                " flag to 'false'", L_NARG);
                        wm->is_running = false;
                    } else {
                        /* event_handler_handle_process(&event); */
                    }
                    break;

                case XCB_CONFIGURE_NOTIFY:
                    /* Handle window resize or move events */
                    //event_handler_handle_configure(&event);
                    break;

                case XCB_MAP_NOTIFY:
                    /* Handle window mapping events (when a window
                     * is shown) */
                    //event_handler_handle_map(&event);
                    break;

                case XCB_UNMAP_NOTIFY:
                    /* Handle window unmapping events (when a window
                     * is hidden) */
                    // event_handler_handle_unmap(&event);
                    break;

                    /* Add cases for other event types as required */
                default:
                    /* Optional: handle unknown events if needed */
                    break;
            } /* switch (event->response_type) */
            free(event);
        } /* ! while (event) */

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
        uint32_t screens_detected;
        xcb_screen_iterator_t it;

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
        wm->connection = xcb_connect(display_name,
                (int *) &(wm->screenp));

        if (xcb_connection_has_error(wm->connection)) {
            if (display_name == NULL) {
                LOGGER_FATAL("Failed to open X display", L_NARG);
            } else {
                LOGGER_FATAL("Failed to open X display '%s'", display_name);
            }
            free(wm);
            return 2;
        }

        /* Establish EWMH connection */
        LOGGER_TRACE("Allocating memory for EWMH connection", L_NARG);
        wm->ewmh = malloc(sizeof(xcb_ewmh_connection_t));
        if (wm->ewmh == NULL) {
            LOGGER_ERROR("Error allocating memory for EWMH connection",
                    L_NARG);
        }

        /* Initializate atoms */
        if (!xcb_ewmh_init_atoms_replies(wm->ewmh,
                    xcb_ewmh_init_atoms(wm->connection, wm->ewmh),
                    NULL)) {
               LOGGER_ERROR("Error initializating EWMH atoms", L_NARG);
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
            xcb_disconnect(wm->connection);
            free(wm);
            return 4;
        }

        /* Count the number of screens.
         * NOTE. Yes,... I can use 'xcb_setup_roots_length', but this
         *       it's better for *my* purposes, "bIjatlh 'e' yImev!" */
        it = xcb_setup_roots_iterator(xcb_get_setup(wm->connection));
        screens_detected = 0;
        for (; it.rem > 0; xcb_screen_next(&it)) {
            screens_detected++;
        }

        /* Check 'screens_detected' for it could be zero on some weird error */
        if (screens_detected == 0) {
            LOGGER_FATAL("No screens detected", L_NARG);
            config_destroy(wm->config);
            xcb_disconnect(wm->connection);
            free(wm);
            return 5;
        } else {
            LOGGER_INFO("Detected preferred screen ID: %u",
                    wm->screenp);
        }

        /* Handle surfaces */
        LOGGER_TRACE("Allocating memory for surface structures", L_NARG);
        wm->surfaces = list_init((void(*)(void *)) surface_destroy);
        if (wm->surfaces == NULL) {
            LOGGER_FATAL("Failed to allocate memory for surfaces array",
                    L_NARG);
            eventq_stop();
            config_destroy(wm->config);
            xcb_disconnect(wm->connection);
            free(wm);
            return 5;
        }

        /* Initialize surfaces */
        LOGGER_TRACE("Initializing surface structures", L_NARG);
        /* Maybe there are more surfaces defined in the configuration
         * file, but only those detected will be initialized, hence the
         * 'screen_count' instead of taking the JSON information.  In
         * the same way, maybe there are 'n' surfaces, but only want to
         * use the 'm < n' defined in the JSON file. */
        if (wm->config->base.screen_count != screens_detected) {
            LOGGER_NOTICE("Detected %u screens; %u are specified in" \
                    " the configuration file",
                    screens_detected, wm->config->base.screen_count);
            if (wm->config->base.screen_count >= screens_detected ||
                wm->config->base.screen_count == 0) {
                wm->config->base.screen_count = screens_detected;
            }
            LOGGER_INFO("Setting number of screens to %u",
                    wm->config->base.screen_count);
        } else {
            LOGGER_DEBUG("Setting number of screens to %u",
                    wm->config->base.screen_count);
        }

        /* Not interested in using XCB iterator because it's needed to
         * transverse the screens from 0 to go in the same order as in
         * the configuration file. */
        for (unsigned int i = 0; i < screens_detected; ++i) {
            /* Get number of desktops for this surface */
            uint32_t desktops_count =
                wm->config->base.screens[it.rem].desktop_count;
            surface_td *surface =
                surface_init(wm->connection,
                        wm->ewmh, (uint32_t) it.rem,
                        desktops_count, wm->config);
            if (surface == NULL) {
                LOGGER_FATAL("Failed to initialize surface %u",
                        it.rem);
                list_destroy(wm->surfaces);
                eventq_stop();
                config_destroy(wm->config);
                xcb_disconnect(wm->connection);
                free(wm);
                return 6;
            }

            LOGGER_TRACE("Inserting surface %u into surface list",
                    it.rem);
            if (list_ins_next(wm->surfaces,
                        list_tail(wm->surfaces),
                        (const void *) surface) != 0) {
                LOGGER_FATAL("Failed to insert surface " \
                        "%u into surface list", it.rem);
                surface_destroy(surface);
                list_destroy(wm->surfaces);
                eventq_stop();
                config_destroy(wm->config);
                xcb_disconnect(wm->connection);
                free(wm);
                return 7;
            }

            surface->desktop_count =
                wm->config->base.screens[it.rem].desktop_count;
            surface->desktop_cur =
                wm->config->base.screens[it.rem].desktop_inaugural;

            LOGGER_TRACE("Setting desktop %u as the startup desktop" \
                    " on surface %u", surface->desktop_cur, it.rem);
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

    /* Deallocating EWMH structure */
    LOGGER_TRACE("Deallocating EWMH structure", L_NARG);
    free(wm->ewmh);

    /* Stop event priority queue */
    eventq_stop();

    /* Destroy configuration structure */
    config_destroy(wm->config);

    /* Close the display */
    LOGGER_TRACE("Closing X display", L_NARG);
    xcb_disconnect(wm->connection);

    LOGGER_TRACE("Destroying window manager", L_NARG);
    free(wm);
    wm = NULL;  /* Reset the singleton instance pointer to 'NULL' */

    LOGGER_DEBUG("Window manager has been destroyed", L_NARG);

    return 0;
}
