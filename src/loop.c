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
#include <stdlib.h>     /* free, NULL */
#include <string.h>     /* strerror */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/randr.h>
#include <xcb/sync.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Session includes */
#include <session.h>

/* Render includes */
#include <render/surface.h>

/* IPC includes */
#include <ipc.h>

/* Input includes */
#include <input/kbd/bind.h>
#include <input/kbd/event.h>
#include <input/mouse/bind.h>
#include <input/mouse/event.h>
#include <input/mouse/hover.h>
#include <input/mouse/drag.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/notify/desktop.h>
#include <menu/popup.h>
#include <menu/search.h>

/* Default initial values */
#include <defs/ipc.h>

/* Project includes */
#include <handler.h>
#include <cctl/adopt.h>
#include <client.h>
#include <lookup.h>
#include <logger.h>
#include <wm/startup/handle.h>
#include <wm/startup/install.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <loop.h>
#include <loop/context.h>
#include <loop/timers.h>


/**
 * @brief Perform a partial (outdated-only) surface update
 *
 * Re-renders only the surfaces that have been marked as outdated.
 * Called on every iteration of the main event loop.
 *
 * @param ctx Main loop context
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
static void s_loop_update(const loop_ctx_td *ctx)
{
    if (ctx == NULL || ctx->surfaces == NULL) {
        return;
    }

    for (list_item_td *node = list_head(ctx->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *const surface = (surface_td *) list_data(node);
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


/**
 * @brief Close a single-instance overlay dialog and repaint whichever
 *        surface is first in the surface list
 *
 * Shared by @c loop_run's own timed auto-close for the info popup and
 * the desktop-switch notification below: both close a dialog that,
 * unlike a per-client one, is not tied to any one particular surface,
 * so any surface's own current-desktop repaint is enough to clear its
 * remnants from the screen.
 *
 * @param ctx      Main loop context
 * @param close_fn The dialog's own @c X_close function
 *
 * @note Complexity: @e O(1), since only the first surface is needed
 */
static void s_loop_close_and_repaint_first_surface(
        const loop_ctx_td *ctx,
        void (*close_fn)(xcb_connection_t *))
{
    surface_td *found = NULL;

    for (list_item_td *node = list_head(ctx->surfaces); node != NULL;
            node = list_next(node)) {
        surface_td *const s = (surface_td *) list_data(node);
        if (s != NULL) {
            found = s;
            break;
        }
    }

    close_fn(ctx->connection);
    if (found != NULL) {
        surface_render_current_desktop_repaint(found);
    }
}


/**
 * @brief Refresh the client behind a genuine input event's own
 *        @c user_time, for @c _NET_ACTIVE_WINDOW focus-stealing
 *        prevention to compare against later
 *
 * @param ctx Main loop context, for its own surface list
 * @param window Window a real @c KeyPress or @c ButtonPress named as
 *               its own @c event field, i.e., the one that actually
 *               received it
 * @param response_type The raw, unmasked @c response_type off the
 *                       event itself, top bit included
 * @param time X server timestamp of the event
 *
 * @note A no-op when the top bit of @p response_type marks the event
 *       synthetic, i.e., sent by an application itself via
 *       @c XSendEvent rather than genuinely delivered by the X
 *       server: trusting a synthetic one here would let any client
 *       fake recent activity right before requesting
 *       @c _NET_ACTIVE_WINDOW, defeating the whole point of the
 *       comparison this feeds
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e c
 *       the hash-table lookup cost per desktop, the same as
 *       @a lookup_find_client itself, which this wraps
 */
static void s_loop_note_real_input(const loop_ctx_td *ctx,
        xcb_window_t window, uint8_t response_type, uint32_t time)
{
    client_td *client;

    if ((response_type & 0x80u) != 0u) {
        return;
    }

    client = lookup_find_client(ctx->surfaces, window, NULL, NULL);
    client_update_user_time(client, time);
}


/**
 * @brief Force a full re-render of all surfaces
 *
 * Marks every surface as outdated and then delegates to
 * @a s_loop_update.  Called once before entering the event loop so
 * pre-existing windows are drawn from scratch.
 *
 * @param ctx Main loop context
 *
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       and @e m is the number of desktops
 */
static void s_loop_update_full(const loop_ctx_td *ctx)
{
    if (ctx == NULL || ctx->surfaces == NULL) {
        return;
    }

    LOGGER_TRACE("Fully updating window manager", L_NARG);

    for (list_item_td *node = list_head(ctx->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *const surface = (surface_td *) list_data(node);
        if (surface != NULL) {
            surface->is_outdated = true;
        }
    }

    s_loop_update(ctx);
}


/* Run the main event loop until the window manager is stopped */
void loop_run(wm_td *wm)
{
    loop_ctx_td ctx;
    struct pollfd pfd[1 + IPC_MAX_CLIENTS + 1];
    bool any_outdated;

    if (wm == NULL || !wm_is_running(wm)) {
        LOGGER_TRACE("Window manager is not initialized or" \
                " set to not run", L_NARG);
        return;
    }

    if (!loop_context_init(&ctx, wm)) {
        LOGGER_ERROR("Failed to resolve main loop context", L_NARG);
        return;
    }

    if (wm_startup_install_signals() != 0) {
        LOGGER_WARNING("Continuing without termination signal handling",
                L_NARG);
    }

    if (wm_startup_install_crash_handlers() != 0) {
        LOGGER_WARNING("Continuing without fatal-signal diagnostics",
                L_NARG);
    }

    ctx.keysyms = xcb_key_symbols_alloc(ctx.connection);
    if (ctx.keysyms == NULL) {
        LOGGER_ERROR("Failed to allocate key symbols table", L_NARG);
        return;
    }
    wm_set_keysyms(wm, ctx.keysyms);

    keyboard_load(ctx.surfaces, ctx.keysyms, ctx.config);
    mouse_load(ctx.surfaces, ctx.config);

    cctl_adopt_scan(wm);
    s_loop_update_full(&ctx);

    /* Synchronize EWMH root properties after the initial scan so that
     * taskbars reading '_NET_CLIENT_LIST' see the windows that were
     * adopted by 'cctl_adopt_scan'.  The earlier 'wm_ewmh_sync'
     * call in 'wm_init' ran before any clients were managed, leaving
     * the list empty; 'loop_update_full' then cleared 'is_outdated', so
     * the first main-loop iteration would never trigger a sync on its
     * own. */
    wm_ewmh_sync(wm);

    LOGGER_DEBUG("Entering main event loop", L_NARG);

    while (wm_is_running(wm)) {
        xcb_generic_event_t *event;
        int nfds;
        int poll_status;
        int poll_timeout_ms;
        int conn_error;
        int ipc_fds[IPC_MAX_CLIENTS + 1];
        int ipc_count = ipc_poll_fds(ipc_fds,
                (int) (sizeof(ipc_fds) / sizeof(ipc_fds[0])));

        if (wm_startup_requested_stop()) {
            LOGGER_INFO("Termination signal received;" \
                    " requesting shutdown", L_NARG);
            wm_request_stop();
            break;
        }

        if (wm_startup_requested_reload()) {
            LOGGER_INFO("'SIGHUP' received; reloading configuration",
                    L_NARG);
            (void) wm_action_config_reload(wm);
        }

        if (wm_startup_requested_resume()) {
            LOGGER_INFO("'SIGCONT' received; re-establishing" \
                    " input grabs", L_NARG);
            keyboard_load(ctx.surfaces, ctx.keysyms, ctx.config);
            mouse_load(ctx.surfaces, ctx.config);
        }

        if (wm_startup_requested_child_reap()) {
            session_reap_children();
        }

        conn_error = xcb_connection_has_error(ctx.connection);
        if (conn_error != 0) {
            LOGGER_ERROR("X connection error detected (%s);" \
                    " requesting shutdown",
                    handler_connection_error_string(conn_error));
            wm_request_stop();
            break;
        }

        pfd[0].fd = xcb_get_file_descriptor(ctx.connection);
        pfd[0].events = POLLIN;
        pfd[0].revents = 0;
        nfds = 1;

        for (int i = 0; i < ipc_count; ++i) {
            pfd[nfds].fd = ipc_fds[i];
            pfd[nfds].events = POLLIN;
            pfd[nfds].revents = 0;
            ++nfds;
        }

        poll_timeout_ms = loop_timers_timeout(&ctx);

        poll_status = poll(pfd, (nfds_t) nfds, poll_timeout_ms);
        if (poll_status < 0 && errno != EINTR) {
            LOGGER_ERROR("Failed waiting on X connection: %s",
                    strerror(errno));
            break;
        }

        /* Every non-X11 descriptor 'poll' reported ready belongs to
         * IPC (index 0 is always the X connection, handled below via
         * 'xcb_poll_for_event' instead of this array at all).
         * 'ipc_handle_readable' itself tells the listening socket
         * apart from an already-connected client, so nothing here
         * needs to. */
        if (poll_status > 0) {
            for (int i = 1; i < nfds; ++i) {
                if (pfd[i].revents & POLLIN) {
                    ipc_handle_readable(wm, pfd[i].fd);
                }
            }
        }

        loop_timers_tick(&ctx);

        while ((event = (ctx.pending_event != NULL)
                    ? ctx.pending_event
                    : xcb_poll_for_event(ctx.connection)) != NULL) {
            xcb_motion_notify_event_t *me;
            uint8_t event_type;

            ctx.pending_event = NULL;
            event_type = (uint8_t) (event->response_type & ~0x80u);

            if (ctx.is_randr_available &&
                    (event_type == (uint8_t) (ctx.randr_base_event +
                            XCB_RANDR_SCREEN_CHANGE_NOTIFY) ||
                     event_type == (uint8_t) (ctx.randr_base_event +
                            XCB_RANDR_NOTIFY))) {
                handler_randr_event(wm, event);
                free(event);
                continue;
            }

            if (ctx.is_sync_available &&
                    event_type == (uint8_t) (ctx.sync_base_event +
                            XCB_SYNC_ALARM_NOTIFY)) {
                handler_sync_event(wm, event);
                free(event);
                continue;
            }

            switch (event->response_type & ~0x80u) {
                case XCB_KEY_PRESS: {
                    xcb_key_press_event_t *const kp =
                        (xcb_key_press_event_t *) event;

                    s_loop_note_real_input(&ctx, kp->event,
                            event->response_type, kp->time);
                    keyboard_handle_press(wm, ctx.keysyms, kp,
                            ctx.surfaces, ctx.config);
                    break;
                }

                case XCB_KEY_RELEASE:
                    keyboard_handle_release(ctx.keysyms,
                            (xcb_key_release_event_t *) event,
                            ctx.surfaces, ctx.config);
                    break;

                case XCB_BUTTON_PRESS: {
                    xcb_button_press_event_t *const bp =
                        (xcb_button_press_event_t *) event;

                    s_loop_note_real_input(&ctx, bp->event,
                            event->response_type, bp->time);
                    mouse_handle_press(wm, ctx.connection, ctx.surfaces,
                            bp, ctx.config);
                    break;
                }

                case XCB_BUTTON_RELEASE:
                    mouse_handle_release(ctx.connection, ctx.surfaces,
                            (xcb_button_release_event_t *) event,
                            ctx.config);
                    break;

                case XCB_MOTION_NOTIFY:
                    me = (xcb_motion_notify_event_t *) event;

                    /* Coalesce a run of consecutive pending
                     * 'MotionNotify' events into just the latest one.
                     * The X server can queue many of these faster
                     * than one round of window-move (or resize) plus
                     * 'xcb_flush' can be processed, especially for a
                     * large or decorated window whose move is more
                     * expensive per event (the frame itself repaints,
                     * and reparented-child bookkeeping adds further
                     * server-side cost on top of a plain top-level
                     * window's move); reacting to every stale
                     * intermediate position instead of jumping
                     * straight to the newest one is what makes a drag
                     * visibly lag behind the pointer, worse the more
                     * expensive that per-event work is.  A non-motion
                     * event found while peeking ahead is kept in
                     * 'ctx.pending_event' rather than dropped, so it is
                     * still handled, on the very next iteration of
                     * this same loop. */
                    while ((ctx.pending_event =
                                xcb_poll_for_event(ctx.connection)) !=
                            NULL) {
                        if ((uint8_t) (ctx.pending_event
                                    ->response_type & ~0x80u) !=
                                XCB_MOTION_NOTIFY) {
                            break;
                        }
                        free(event);
                        event = ctx.pending_event;
                        me = (xcb_motion_notify_event_t *) event;
                    }

                    drag_update(ctx.connection,
                            (struct position_s) { me->root_x, me->root_y });
                    if (wincmenu_is_open()) {
                        wincmenu_handle_motion(me->event,
                                me->event_x, me->event_y);
                    } else if (rootmenu_is_open()) {
                        rootmenu_handle_motion(me->event,
                                me->event_x, me->event_y);
                    } else if (winlist_is_open()) {
                        winlist_handle_motion(me->event,
                                me->event_x, me->event_y);
                    } else if (search_is_open() &&
                            (me->event == search_window() ||
                             me->child == search_window())) {
                        search_handle_motion(me->event_x, me->event_y);
                    } else if (!drag_is_active()) {
                        mouse_handle_motion_hover(ctx.connection,
                                ctx.surfaces, me);
                    }
                    break;

                case XCB_ENTER_NOTIFY:
                    mouse_handle_enter(ctx.connection, ctx.surfaces,
                            (xcb_enter_notify_event_t *) event,
                            ctx.config);
                    break;

                case XCB_CONFIGURE_NOTIFY:
                    handler_configure_notify(ctx.connection,
                            ctx.surfaces,
                            (xcb_configure_notify_event_t *) event);
                    break;

                case XCB_UNMAP_NOTIFY:
                    handler_unmap_notify(ctx.connection,
                            ctx.surfaces,
                            (xcb_unmap_notify_event_t *) event);
                    break;

                case XCB_DESTROY_NOTIFY:
                    handler_destroy_notify(wm, ctx.connection,
                            ctx.surfaces,
                            (xcb_destroy_notify_event_t *) event);
                    break;

                case XCB_PROPERTY_NOTIFY:
                    handler_property_notify(wm, ctx.connection,
                            ctx.surfaces,
                            (xcb_property_notify_event_t *) event);
                    break;

                case XCB_FOCUS_IN:
                    handler_focus_in(ctx.connection, ctx.surfaces,
                            (xcb_focus_in_event_t *) event);
                    break;

                case XCB_COLORMAP_NOTIFY:
                    handler_colormap_notify(ctx.connection, ctx.surfaces,
                            (xcb_colormap_notify_event_t *) event);
                    break;

                case XCB_EXPOSE:
                    handler_expose(ctx.connection, ctx.surfaces,
                            (xcb_expose_event_t *) event, ctx.config);
                    break;

                case XCB_CONFIGURE_REQUEST:
                    handler_configure_request(ctx.connection,
                            ctx.surfaces,
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
                    handler_mapping_notify(ctx.keysyms, ctx.surfaces,
                            (xcb_mapping_notify_event_t *) event,
                            ctx.config);
                    break;

                case XCB_LEAVE_NOTIFY:
                    handler_leave_notify(wm,
                            (xcb_leave_notify_event_t *) event);
                    break;

                case XCB_FOCUS_OUT:
                    handler_focus_out(wm,
                            (xcb_focus_out_event_t *) event);
                    break;

                case XCB_MAP_NOTIFY:
                    handler_map_notify(ctx.connection,
                            ctx.surfaces,
                            (xcb_map_notify_event_t *) event);
                    break;

                case XCB_GRAVITY_NOTIFY:
                    handler_gravity_notify(ctx.connection,
                            ctx.surfaces,
                            (xcb_gravity_notify_event_t *) event);
                    break;

                case XCB_CIRCULATE_NOTIFY:
                    handler_circulate_notify(ctx.connection,
                            ctx.surfaces,
                            (xcb_circulate_notify_event_t *) event);
                    break;

                case XCB_CIRCULATE_REQUEST:
                    handler_circulate_request(ctx.connection,
                            ctx.surfaces,
                            (xcb_circulate_request_event_t *) event);
                    break;

                case XCB_REPARENT_NOTIFY:
                    /* The WM does its own reparenting and absorbs the
                     * spurious 'UnmapNotify' via 'ignore_unmap'; no
                     * action needed here */
                case XCB_CREATE_NOTIFY:
                    /* Windows are adopted on 'MAP_REQUEST', not on
                     * creation; a created window may never be mapped */
                    break;

                case 0:
                    handler_protocol_error(event);
                    break;

                default:
                    LOGGER_TRACE("Unhandled X event type: %d",
                            event->response_type & ~0x80u);
                    break;
            }

            free(event);
        }

        /* Auto-close the info popup when its display timeout has
         * elapsed.  Close before the 'loop_update' call so any visual
         * update triggered by the close is handled in the same
         * iteration. */
        if (popup_is_open() && popup_ms_remaining() == 0) {
            s_loop_close_and_repaint_first_surface(&ctx, popup_close);
        }

        /* Auto-close the desktop notify when its timeout has elapsed */
        if (notify_desktop_is_open() &&
                notify_desktop_ms_remaining() == 0) {
            s_loop_close_and_repaint_first_surface(&ctx,
                    notify_desktop_close);
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
        for (list_item_td *sync_node = list_head(ctx.surfaces);
                sync_node != NULL; sync_node = list_next(sync_node)) {
            const surface_td *s = (surface_td *) list_data(sync_node);
            if (s != NULL && s->is_outdated) {
                any_outdated = true;
                break;
            }
        }

        s_loop_update(&ctx);
        if (any_outdated) {
            wm_ewmh_sync(wm);
        }
    }

    LOGGER_DEBUG("Exiting event loop", L_NARG);
    xcb_key_symbols_free(ctx.keysyms);
    wm_set_keysyms(wm, NULL);
}
