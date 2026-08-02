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

#define _POSIX_C_SOURCE 200112L /* poll, strerror */


/* System includes */
#include <errno.h>      /* EINTR */
#include <poll.h>       /* poll */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* free */
#include <string.h>     /* strerror */
#include <sys/wait.h>   /* waitpid */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Render includes */
#include <render/surface.h>

/* Policy includes */
#include <policy/focus.h>

/* Input includes */
#include <input/drag.h>
#include <input/kbpress.h>
#include <input/keyboard.h>
#include <input/mouse.h>

/* Menu includes */
#include <menu/popup.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <eventq.h>
#include <handler.h>
#include <lifecycle.h>
#include <lookup.h>
#include <logger.h>
#include <startup.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <loop.h>


/**
 * @brief Handle pointer-leave notifications for focus-follow-mouse
 *
 * @param wm    Window-manager singleton
 * @param event Leave-notify event to process
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected by @c lookup_find_client
 */
static void s_loop_handle_leave_notify(wm_td *wm,
        xcb_leave_notify_event_t *event)
{
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;

    if (wm == NULL || event == NULL) {
        return;
    }

    if (focus_is_follow_mouse(wm->config) &&
            event->mode == XCB_NOTIFY_MODE_NORMAL &&
            event->detail != XCB_NOTIFY_DETAIL_INFERIOR &&
            lookup_find_client(wm->surfaces, event->event,
                    &surface, &desktop) != NULL) {
        /* Pointer left a managed window; release focus so the cursor
         * resting on the root background leaves all clients visually
         * unfocused */
        xcb_set_input_focus(wm->connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                event->time);
        if (desktop != NULL) {
            desktop->client_active_id = 0;
            desktop->is_outdated = true;
        }
        if (surface != NULL) {
            surface->is_outdated = true;
        }
        xcb_flush(wm->connection);
    }
}


/**
 * @brief Mark a client dirty after losing X input focus
 *
 * @param wm    Window-manager singleton
 * @param event Focus-out event to process
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected by @c lookup_find_client
 */
static void s_loop_handle_focus_out(wm_td *wm,
        xcb_focus_out_event_t *event)
{
    surface_td *surface = NULL;

    if (wm == NULL || event == NULL) {
        return;
    }

    if ((event->mode == XCB_NOTIFY_MODE_NORMAL ||
                event->mode == XCB_NOTIFY_MODE_WHILE_GRABBED) &&
            lookup_find_client(wm->surfaces, event->event,
                    &surface, NULL) != NULL &&
            surface != NULL) {
        surface->is_outdated = true;
    }
}


/* Run the main event loop until the window manager is stopped */
void loop_run(wm_td *wm)
{
    xcb_key_symbols_t *keysyms;
    xcb_generic_event_t *event;
    struct pollfd pfd;
    int poll_status;
    int poll_timeout_ms;
    bool any_outdated;

    if (wm == NULL || !wm->is_running) {
        LOGGER_TRACE("Window manager is not initialized or" \
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

    /* Synchronize EWMH root properties after the initial scan so that
     * taskbars reading '_NET_CLIENT_LIST' see the windows that were
     * adopted by 'lifecycle_scan_existing'.  The earlier 'wm_ewmh_sync'
     * call in 'wm_init' ran before any clients were managed, leaving
     * the list empty; 'loop_update_full' then cleared 'is_outdated', so
     * the first main-loop iteration would never trigger a sync on its
     * own. */
    wm_ewmh_sync();

    LOGGER_DEBUG("Entering main event loop", L_NARG);

    while (wm->is_running) {
        if (startup_stop_requested()) {
            LOGGER_INFO("Termination signal received;" \
                    " requesting shutdown", L_NARG);
            wm_request_stop();
            break;
        }

        if (startup_reload_requested()) {
            LOGGER_INFO("'SIGHUP' received; reloading configuration",
                    L_NARG);
            (void) wm_action_config_reload();
        }

        if (startup_resume_requested()) {
            LOGGER_INFO("'SIGCONT' received; re-establishing" \
                    " input grabs", L_NARG);
            keyboard_load(wm->surfaces, keysyms, wm->config);
            mouse_load(wm->surfaces, wm->config);
        }

        if (startup_child_reap_requested()) {
            while (waitpid(-1, NULL, WNOHANG) > 0) {
            }
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

        /* Use a shorter poll timeout when the info popup is visible so
         * it closes promptly at the configured expiry time */
        poll_timeout_ms = WM_EVENT_POLL_TIMEOUT_MS;
        if (popup_is_open()) {
            int ms = popup_ms_remaining();
            if (ms >= 0 && ms < poll_timeout_ms) {
                poll_timeout_ms = ms;
            }
        }

        poll_status = poll(&pfd, 1, poll_timeout_ms);
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

                case XCB_CLIENT_MESSAGE:
                    handler_client_message(wm,
                            (xcb_client_message_event_t *) event);
                    break;

                case XCB_MAPPING_NOTIFY:
                    handler_mapping_notify(keysyms, wm->surfaces,
                            (xcb_mapping_notify_event_t *) event,
                            wm->config);
                    break;

                case XCB_LEAVE_NOTIFY:
                    s_loop_handle_leave_notify(wm,
                            (xcb_leave_notify_event_t *) event);
                    break;

                case XCB_FOCUS_OUT:
                    s_loop_handle_focus_out(wm,
                            (xcb_focus_out_event_t *) event);
                    break;

                case XCB_MAP_NOTIFY:
                    handler_map_notify(wm->connection,
                            wm->surfaces,
                            (xcb_map_notify_event_t *) event);
                    break;

                case XCB_GRAVITY_NOTIFY:
                    handler_gravity_notify(wm->connection,
                            wm->surfaces,
                            (xcb_gravity_notify_event_t *) event);
                    break;

                case XCB_CIRCULATE_NOTIFY:
                    handler_circulate_notify(wm->connection,
                            wm->surfaces,
                            (xcb_circulate_notify_event_t *) event);
                    break;

                case XCB_CIRCULATE_REQUEST:
                    handler_circulate_request(wm->connection,
                            wm->surfaces,
                            (xcb_circulate_request_event_t *) event);
                    break;

                case XCB_REPARENT_NOTIFY:
                case XCB_CREATE_NOTIFY:
                    /* - 'XCB_REPARENT_NOTIFY': icowm does its own
                     *   reparenting and absorbs the spurious
                     *   'UnmapNotify' via 'ignore_unmap'; no action
                     *   needed here.
                     *
                     * - 'XCB_CREATE_NOTIFY': windows are adopted on
                     *   'MAP_REQUEST', not on creation; a created
                     *   window may never be mapped. */
                    break;

                default:
                    LOGGER_TRACE("Unhandled X event type: %d",
                            event->response_type & ~0x80u);
                    break;
            }

            free(event);
        }

        eventq_process();

        /* Auto-close the info popup when its display timeout has
         * elapsed.  Close before the 'loop_update' call so any visual
         * update triggered by the close is handled in the same
         * iteration. */
        if (popup_is_open() && popup_ms_remaining() == 0) {
            surface_td *popup_surface = NULL;

            for (list_item_td *ps_node = list_head(wm->surfaces);
                    ps_node != NULL; ps_node = list_next(ps_node)) {
                surface_td *s = (surface_td *) list_data(ps_node);
                if (s != NULL) {
                    popup_surface = s;
                    break;
                }
            }

            popup_close(wm->connection);
            if (popup_surface != NULL) {
                surface_render_current_desktop_repaint(popup_surface);
            }
        }

        /* Only sync EWMH root properties when state actually changed.
         * Calling 'wm_ewmh_sync' unconditionally writes root window
         * properties every iteration; the X server then sends
         * 'PropertyNotify' events back (root has 'PROPERTY_CHANGE'
         * selected), keeping 'poll' permanently readable and spinning
         * the CPU more than it should.  Checking 'is_outdated' before
         * 'loop_update' (which clears the flag) gates the sync to
         * iterations where real work happened. */
        any_outdated = false;
        for (list_item_td *sync_node = list_head(wm->surfaces);
                sync_node != NULL; sync_node = list_next(sync_node)) {
            surface_td *s = (surface_td *) list_data(sync_node);
            if (s != NULL && s->is_outdated) {
                any_outdated = true;
                break;
            }
        }

        loop_update(wm);
        if (any_outdated) {
            wm_ewmh_sync();
        }
    }

    LOGGER_DEBUG("Exiting event loop", L_NARG);
    xcb_key_symbols_free(keysyms);
}


/* Perform a partial (outdated-only) surface update */
void loop_update(wm_td *wm)
{
    if (wm == NULL || wm->surfaces == NULL) {
        return;
    }

    for (list_item_td *node = list_head(wm->surfaces);
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


/* Force a full re-render of all surfaces */
void loop_update_full(wm_td *wm)
{
    if (wm == NULL || wm->surfaces == NULL) {
        return;
    }

    LOGGER_TRACE("Fully updating window manager", L_NARG);

    for (list_item_td *node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        if (surface != NULL) {
            surface->is_outdated = true;
        }
    }

    loop_update(wm);
}
