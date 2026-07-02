/**
 * @file wm.c
 *
 * @brief Window manager implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
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
 * @brief Handle KEY_PRESS events from the X server
 *
 * Processes keyboard input events by converting XCB keycodes to
 * keysyms and performing appropriate window manager actions based
 * on configured key bindings.
 *
 * @param keysyms  Pointer to XCB key symbols structure
 * @param event    Pointer to the key press event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_key_press(xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event)
{
    xcb_keysym_t keysym;

    if (keysyms == NULL || event == NULL) {
        LOGGER_ERROR("Received NULL pointer in key press handler",
                L_NARG);
        return;
    }

    /*
     * Translate keycode to keysym using the key symbols table
     */
    keysym = xcb_key_symbols_get_keysym(keysyms, event->detail, 0);

    LOGGER_TRACE("Key press event: keysym=0x%x, state=0x%x",
            keysym, event->state);

    /*
     * Check for exit key combination: Ctrl+Mod1+Shift+BackSpace
     * This is the hardcoded emergency exit key
     */
    if (keysym == 0xff08 &&     // == XK_BackSpace &&
            (event->state & XCB_MOD_MASK_CONTROL) &&
            (event->state & XCB_MOD_MASK_1) &&
            (event->state & XCB_MOD_MASK_SHIFT)) {
        LOGGER_TRACE("Exit key combination detected," \
                " setting is_running to false",
                L_NARG);
        wm->is_running = false;
        return;
    }

    /*
     * TODO: Implement key binding lookup and action dispatch
     * This would involve:
     * 1. Looking up the keysym in the configuration bindings
     * 2. Determining the appropriate action
     * 3. Creating an event and adding it to the event queue
     */
}


/**
 * @brief Handle CONFIGURE_NOTIFY events from the X server
 *
 * Processes window configuration change notifications. These events
 * indicate that a window's geometry or stacking order has changed.
 *
 * @param event Pointer to the configure notify event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_configure_notify(
        xcb_configure_notify_event_t *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received NULL pointer in configure handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Configure notify event: window=0x%x," \
            " geom=%ux%u+%d+%d",
            event->window, event->width, event->height,
            event->x, event->y);

    /*
     * TODO: Handle window geometry changes
     * This may involve updating internal client state if necessary
     */
}


/**
 * @brief Handle MAP_REQUEST events from the X server
 *
 * Processes requests to map (display) windows. This event is sent
 * when a window wants to become visible on the screen.
 *
 * @param event Pointer to the map request event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_map_request(
        xcb_map_request_event_t *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received NULL pointer in map request handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Map request event: window=0x%x, parent=0x%x",
            event->window, event->parent);

    /*
     * TODO: Implement client addition to the window manager
     * This would involve:
     * 1. Creating a new client structure for the window
     * 2. Determining which desktop it belongs to
     * 3. Adding it to the appropriate desktop
     * 4. Marking the surface as outdated for rendering
     */
}


/**
 * @brief Handle UNMAP_NOTIFY events from the X server
 *
 * Processes notifications that a window has been unmapped (hidden).
 * This typically means the window is no longer visible on screen.
 *
 * @param event Pointer to the unmap notify event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_unmap_notify(
        xcb_unmap_notify_event_t *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received NULL pointer in unmap handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Unmap notify event: window=0x%x", event->window);

    /*
     * TODO: Handle window unmapping
     * This would involve:
     * 1. Finding the client associated with the window
     * 2. Removing it from the desktop client list
     * 3. Marking the surface as outdated for rendering
     */
}


/**
 * @brief Handle DESTROY_NOTIFY events from the X server
 *
 * Processes notifications that a window has been destroyed. When
 * this event is received, the window is no longer valid and any
 * references to it should be cleaned up.
 *
 * @param event Pointer to the destroy notify event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_destroy_notify(
        xcb_destroy_notify_event_t *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received NULL pointer in destroy handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Destroy notify event: window=0x%x", event->window);

    /*
     * TODO: Handle window destruction
     * This would involve:
     * 1. Finding the client associated with the window
     * 2. Completely removing it from all data structures
     * 3. Freeing associated resources
     * 4. Marking the surface as outdated for rendering
     */
}


/**
 * @brief Handle PROPERTY_NOTIFY events from the X server
 *
 * Processes notifications that window properties have changed.
 * Properties may include WM_NAME, WM_CLASS, WM_HINTS, and others
 * which affect how the window manager displays or manages the window.
 *
 * @param event Pointer to the property notify event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_property_notify(
        xcb_property_notify_event_t *event)
{
    if (event == NULL) {
        LOGGER_ERROR("Received NULL pointer in property handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Property notify event: window=0x%x, atom=%u",
            event->window, event->atom);

    /*
     * TODO: Handle property changes
     * This would involve:
     * 1. Determining which property changed
     * 2. Updating the client information accordingly
     * 3. Marking the surface as outdated if visual changes needed
     */
}


/**
 * @brief Soft window manager update
 *
 * Performs a minimal update of the window manager state. This is
 * called frequently to maintain responsiveness without doing heavy
 * rendering operations.
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_update(void)
{
    /*
     * Light update operations would go here
     * For now this is a no-op to avoid excessive logging
     */
}


/**
 * @brief Full window manager update
 *
 * Updates the window manager by rendering every window on every desktop
 * of every surface.  This is called when major changes occur that
 * require a complete visual refrelsh.
 *
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       and @e m is the number of desktops on the surface
 */
static void s_wm_update_full(void)
{
    LOGGER_TRACE("Fully updating window manager", L_NARG);

    /* Soft update first */
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
 * XCB events and passing them to appropriate handlers. Uses
 * @a xcb_poll_for_event to achieve non-blocking event processing for
 * responsive behavior.
 *
 * @note The event loop will stop when the @p is_running flag is set
 *       to @c false, typically in response to user actions or during
 *       window manager termination
 * @note Complexity: @e O(1) for each event; overall complexity
 *       depends on the number of events processed
 */
static void s_wm_loop(void)
{
    xcb_key_symbols_t *keysyms;
    xcb_generic_event_t *event;

    if (wm == NULL || !wm->is_running) {
        LOGGER_TRACE("Window manager is not initialized" \
                " or set to not run", L_NARG);
        return;
    }

    /* Update window manager before starting the event loop */
    s_wm_update_full();

    /* Allocate key symbols table for keyboard event processing */
    keysyms = xcb_key_symbols_alloc(wm->connection);
    if (keysyms == NULL) {
        LOGGER_ERROR("Failed to allocate key symbols table", L_NARG);
        return;
    }

#ifdef DEBUG
    /* TEST: Create dummy windows to test rendering */
    if (wm->surfaces != NULL) {
        list_item_td *surface_node = list_head(wm->surfaces);
        if (surface_node != NULL) {
            surface_td *surface = (surface_td *) list_data(surface_node);

            if (surface != NULL && surface->desktops != NULL) {
                cdlist_item_td *desktop_node =
                    cdlist_head(surface->desktops);
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
                        /* Create a test window for rendering */
                        client_td *test_client = client_init(
                                wm->connection,
                                wm->ewmh,
                                surface->screen->root,  /* Parent window */
                                100, 100,   /* width, height */
                                50, 50,     /* x, y position */
                                &(wm->config->theme));

                        if (test_client != NULL) {
                            /* Remove 'HIDDEN' flag to show the window */
                            safeflg_unset(&(test_client)->properties.flags,
                                    CLIENT_FLAG_HIDDEN, CLIENT_FLAG_MAX);

                            desktop_action_client_add(desktop, test_client);
                            desktop->is_outdated = true;
                            LOGGER_TRACE("Created test client for rendering",
                                    L_NARG);
                        } /* ! if (test_client) */
                    } /* ! if (desktop) */
                } /* if (desktop_node) */
            } /* ! if (surface) */
        } /* ! if (surface_node) */
    }
#endif  /* ! DEBUG */

    LOGGER_DEBUG("Entering main event loop", L_NARG);
    while (wm->is_running) {
        /* Process window manager events from event priority queue */
        eventq_process();

        /* Process X events.
         * 'xcb_poll_for_event' is non-blocking and returns NULL when no
         * events are available */
        while ((event = xcb_poll_for_event(wm->connection))) {
            /* Dispatch event to appropriate handler based on type.
             * The & ~0x80 mask clears the synthetic event bit */
            switch (event->response_type & ~0x80) { /* Ignore error bits */
                case XCB_KEY_PRESS:
                    s_wm_handle_key_press(
                            keysyms,
                            (xcb_key_press_event_t *) event);
                    break;

                case XCB_CONFIGURE_NOTIFY:
                    s_wm_handle_configure_notify(
                            (xcb_configure_notify_event_t *) event);
                    break;

                case XCB_MAP_REQUEST:
                    s_wm_handle_map_request(
                            (xcb_map_request_event_t *) event);
                    break;

                case XCB_UNMAP_NOTIFY:
                    s_wm_handle_unmap_notify(
                            (xcb_unmap_notify_event_t *) event);
                    break;

                case XCB_DESTROY_NOTIFY:
                    s_wm_handle_destroy_notify(
                            (xcb_destroy_notify_event_t *) event);
                    break;

                case XCB_PROPERTY_NOTIFY:
                    s_wm_handle_property_notify(
                            (xcb_property_notify_event_t *) event);
                    break;

                default:
                    LOGGER_TRACE(
                            "Unhandled X event type: %d",
                            event->response_type & ~0x80);
                    break;
            }

            /*
             * Free the event structure after processing
             */
            free(event);
        }

        /*
         * Update the window manager after processing all events
         */
        s_wm_update();
    }

    LOGGER_DEBUG("Exiting event loop", L_NARG);

    /*
     * Free key symbols table before exiting
     */
    xcb_key_symbols_free(keysyms);
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
            wm = NULL;
            return 2;
        }

        /* Establish EWMH connection */
        LOGGER_TRACE("Allocating memory for EWMH connection", L_NARG);
        wm->ewmh = malloc(sizeof(xcb_ewmh_connection_t));
        if (wm->ewmh == NULL) {
            LOGGER_ERROR("Error allocating memory for EWMH connection",
                    L_NARG);
        }

        /* Initializate EWMH atoms */
        if (!xcb_ewmh_init_atoms_replies(wm->ewmh,
                    xcb_ewmh_init_atoms(wm->connection, wm->ewmh),
                    NULL)) {
               LOGGER_ERROR("Error initializating EWMH atoms", L_NARG);
        }

        /* Set and load configuration */
        wm->config = config_init();
        if (wm->config == NULL) {
            xcb_disconnect(wm->connection);
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 3;
        }

        LOGGER_TRACE("Loading configuration into window manager", L_NARG);
        config_load(wm->config, config_dir_prefix);

        /* Events: priority queue as min-heap (bottom-heavy heap) */
        if (eventq_start() != 0) {
            LOGGER_FATAL("Failed to initialize event priority queue",
                    L_NARG);
            config_destroy(wm->config);
            xcb_disconnect(wm->connection);
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 4;
        }

        /* Count the number of screens detected by the X server.
         * NOTE. Yes,... I can use 'xcb_setup_roots_length', but this
         *       it's better for *my* purposes, "bIjatlh 'e' yImev!" */
        it = xcb_setup_roots_iterator(xcb_get_setup(wm->connection));
        screens_detected = 0;
        for (; it.rem > 0; xcb_screen_next(&it)) {
            screens_detected++;
        }

        /* Verify 'screens_detected' for it could be zero on some
         * strange error */
        if (screens_detected == 0) {
            LOGGER_FATAL("No screens detected", L_NARG);
            config_destroy(wm->config);
            eventq_stop();
            xcb_disconnect(wm->connection);
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 5;
        } else {
            LOGGER_INFO("Detected screen %u as preferred", wm->screenp);
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
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 5;
        }

        /* Initialize surfaces */
        LOGGER_TRACE("Initializing surface structures", L_NARG);
        /* Maybe there are more surfaces defined in the configuration
         * file, but only those detected will be initialized, hence the
         * 'screen_count' instead of taking the JSON information as
         * true.  In the same way, maybe there are 'n' surfaces, but
         * only want to use the 'm < n' defined in the JSON file. */
        if (wm->config->base.screen_count != screens_detected) {
            LOGGER_NOTICE("Detected %u screen(s); %u" \
                    " specified in the configuration file",
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
                wm->config->base.screens[i].desktop_count;
            surface_td *surface =
                surface_init(wm->connection,
                        wm->ewmh, (uint32_t) i,
                        desktops_count, wm->config);
            if (surface == NULL) {
                LOGGER_FATAL("Failed to initialize surface %u", i);
                list_destroy(wm->surfaces);
                eventq_stop();
                config_destroy(wm->config);
                xcb_disconnect(wm->connection);
                free(wm->ewmh);
                free(wm);
                wm = NULL;
                return 6;
            }

            LOGGER_TRACE("Inserting surface %u into surface list", i);
            if (list_ins_next(wm->surfaces,
                        list_tail(wm->surfaces),
                        (const void *) surface) != 0) {
                LOGGER_FATAL("Failed to insert surface " \
                        "%u into surface list", i);
                surface_destroy(surface);
                list_destroy(wm->surfaces);
                eventq_stop();
                config_destroy(wm->config);
                xcb_disconnect(wm->connection);
                free(wm->ewmh);
                free(wm);
                wm = NULL;
                return 7;
            }

            surface->desktop_count =
                wm->config->base.screens[i].desktop_count;
            surface->desktop_cur =
                wm->config->base.screens[i].desktop_inaugural;

            LOGGER_TRACE("Setting desktop %u as the startup desktop" \
                    " on surface %u", surface->desktop_cur, i);
        }

        /* Begin! */
        LOGGER_TRACE("Setting 'is_running' status flag to 'true'", L_NARG);
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

    /* Deallocate every surface and its contents */
    LOGGER_TRACE("Deallocating surfaces in window manager", L_NARG);
    list_destroy(wm->surfaces);

    /* Deallocate EWMH structure */
    LOGGER_TRACE("Deallocating EWMH structure", L_NARG);
    free(wm->ewmh);

    /* Stop event priority queue */
    eventq_stop();

    /* Destroy configuration structure */
    config_destroy(wm->config);

    /* Close the X display connection */
    LOGGER_TRACE("Closing X display", L_NARG);
    xcb_disconnect(wm->connection);

    /* Free the WM structure itself */
    LOGGER_TRACE("Destroying window manager", L_NARG);
    free(wm);
    wm = NULL;

    LOGGER_DEBUG("Window manager has been destroyed", L_NARG);

    return 0;
}
