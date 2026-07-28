/**
 * @file loop.c
 *
 * @brief Main event loop, partial update, and full update
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L  /* poll, strerror */


/* System includes */
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* free */
#include <string.h>     /* strerror */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Render includes */
#include <render/surface.h>

/* Input includes */
#include <input/drag.h>
#include <input/kbpress.h>
#include <input/keyboard.h>
#include <input/mouse.h>

/* Project includes */
#include <handler.h>
#include <lifecycle.h>
#include <logger.h>
#include <startup.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <loop.h>


/* Run the main event loop until the window manageris stopped */
void loop_update(wm_td *wm)
{
    list_item_td *node;

    if (wm == NULL || wm->surfaces == NULL) {
        return;
    }

    for (node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        if (surface == NULL) {
            continue;
        }
        if (surface->is_outdated) {
            if (surface_render_all_desktops(surface) != 0) {
                LOGGER_ERROR("Failed to render surface %u",
                        surface->id);
            }
        }
    }
}


/* Perform a partial (outdated-only) surface update */
void loop_update_full(wm_td *wm)
{
    list_item_td *node;

    if (wm == NULL || wm->surfaces == NULL) {
        return;
    }

    LOGGER_TRACE("Fully updating window manager", L_NARG);

    for (node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        if (surface != NULL) {
            surface->is_outdated = true;
        }
    }

    loop_update(wm);
}


/* Force a full re-render of all surfaces */
void loop_run(wm_td *wm)
{
    xcb_key_symbols_t *keysyms;
    xcb_generic_event_t *event;
    struct pollfd pfd;
    int poll_status;

    if (wm == NULL || !wm->is_running) {
        LOGGER_TRACE("Window manager is not initialised or" \
                " set to not run", L_NARG);
        return;
    }

    if (startup_install_signals() != 0) {
        LOGGER_WARNING("Continuing without termination signal handling",
                L_NARG);
    }

    keysyms = xcb_key_symbols_alloc(wm->connection);
    if (keysyms == NULL) {
        LOGGER_ERROR("Failed to allocate key symbols table", L_NARG);
        return;
    }

    keyboard_load(wm->surfaces, keysyms, wm->config);
    mouse_load(wm->surfaces, wm->config);

    lifecycle_scan_existing(wm);
    loop_update_full(wm);

    LOGGER_DEBUG("Entering main event loop", L_NARG);

    while (wm->is_running) {
        if (startup_stop_requested()) {
            LOGGER_INFO("Termination signal received;" \
                    " requesting shutdown", L_NARG);
            wm_request_stop();
            break;
        }

        if (xcb_connection_has_error(wm->connection) != 0) {
            LOGGER_ERROR("X connection error detected;" \
                    " requesting shutdown", L_NARG);
            wm_request_stop();
            break;
        }

        pfd.fd = xcb_get_file_descriptor(wm->connection);
        pfd.events = POLLIN;
        pfd.revents = 0;

        poll_status = poll(&pfd, 1, -1);
        if (poll_status < 0 && errno != EINTR) {
            LOGGER_ERROR("Failed waiting on X connection: %s",
                    strerror(errno));
            break;
        }

        while ((event = xcb_poll_for_event(wm->connection)) != NULL) {
            switch (event->response_type & ~0x80u) {
                case XCB_KEY_PRESS:
                    keyboard_handle_press(keysyms,
                            (xcb_key_press_event_t *) event,
                            wm->surfaces, wm->config);
                    break;

                case XCB_KEY_RELEASE:
                    keyboard_handle_release(keysyms,
                            (xcb_key_release_event_t *) event,
                            wm->surfaces, wm->config);
                    break;

                case XCB_BUTTON_PRESS:
                    mouse_handle_press(wm->connection, wm->surfaces,
                            (xcb_button_press_event_t *) event,
                            wm->config);
                    break;

                case XCB_BUTTON_RELEASE:
                    mouse_handle_release(wm->connection, wm->surfaces,
                            (xcb_button_release_event_t *) event,
                            wm->config);
                    break;

                case XCB_MOTION_NOTIFY:
                    drag_update(wm->connection,
                            ((xcb_motion_notify_event_t *) event)->root_x,
                            ((xcb_motion_notify_event_t *) event)->root_y);
                    break;

                case XCB_ENTER_NOTIFY:
                    mouse_handle_enter(wm->connection, wm->surfaces,
                            (xcb_enter_notify_event_t *) event,
                            wm->config);
                    break;

                case XCB_CONFIGURE_NOTIFY:
                    handler_configure_notify(wm->connection,
                            wm->surfaces,
                            (xcb_configure_notify_event_t *) event);
                    break;

                case XCB_UNMAP_NOTIFY:
                    handler_unmap_notify(wm->connection,
                            wm->surfaces,
                            (xcb_unmap_notify_event_t *) event);
                    break;

                case XCB_DESTROY_NOTIFY:
                    handler_destroy_notify(wm->connection,
                            wm->surfaces,
                            (xcb_destroy_notify_event_t *) event);
                    break;

                case XCB_PROPERTY_NOTIFY:
                    handler_property_notify(wm->connection,
                            wm->surfaces,
                            (xcb_property_notify_event_t *) event);
                    break;

                case XCB_FOCUS_IN:
                    handler_focus_in(wm->connection, wm->surfaces,
                            (xcb_focus_in_event_t *) event);
                    break;

                case XCB_EXPOSE:
                    handler_expose(wm->connection, wm->surfaces,
                            (xcb_expose_event_t *) event, wm->config);
                    break;

                case XCB_CONFIGURE_REQUEST:
                    handler_configure_request(wm->connection,
                            wm->surfaces,
                            (xcb_configure_request_event_t *) event);
                    break;

                case XCB_MAP_REQUEST:
                    handler_map_request(wm,
                            (xcb_map_request_event_t *) event);
                    break;

                case XCB_MAPPING_NOTIFY:
                    handler_mapping_notify(keysyms, wm->surfaces,
                            (xcb_mapping_notify_event_t *) event,
                            wm->config);
                    break;

                default:
                    LOGGER_TRACE("Unhandled X event type: %d",
                            event->response_type & ~0x80u);
                    break;
            }

            free(event);
        }

        loop_update(wm);
    }

    LOGGER_DEBUG("Exiting event loop", L_NARG);
    xcb_key_symbols_free(keysyms);
}
