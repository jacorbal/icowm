/**
 * @file loop.c
 *
 * @brief Main event loop
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
#include <stdlib.h>     /* free, NULL */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/randr.h>
#include <xcb/sync.h>

/* Type includes */
#include <types/pair.h>

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
#include <menu/search.h>

/* Project includes */
#include <handler.h>
#include <cctl/adopt.h>
#include <client.h>
#include <lookup.h>
#include <logger.h>
#include <wm/startup/install.h>
#include <wm.h>

/* Local includes */
#include <loop.h>
#include <loop/context.h>
#include <loop/pollset.h>
#include <loop/refresh.h>
#include <loop/signals.h>
#include <loop/timers.h>


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


/* Run the main event loop until the window manager is stopped */
void loop_run(wm_td *wm)
{
    loop_ctx_td ctx;

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
    loop_refresh_full(&ctx);

    /* Synchronize EWMH root properties after the initial scan so that
     * taskbars reading '_NET_CLIENT_LIST' see the windows that were
     * adopted by 'cctl_adopt_scan'.  The earlier 'wm_ewmh_sync'
     * call in 'wm_init' ran before any clients were managed, leaving
     * the list empty; 'loop_refresh_full' then cleared 'is_outdated',
     * so the first main-loop iteration would never trigger a sync on
     * its own. */
    wm_ewmh_sync(wm);

    LOGGER_DEBUG("Entering main event loop", L_NARG);

    while (wm_is_running(wm)) {
        xcb_generic_event_t *event;

        if (!loop_signals_process(&ctx)) {
            break;
        }

        if (!loop_pollset_wait(&ctx, loop_timers_timeout(&ctx))) {
            break;
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

        loop_refresh(&ctx);
    }

    LOGGER_DEBUG("Exiting event loop", L_NARG);
    xcb_key_symbols_free(ctx.keysyms);
    wm_set_keysyms(wm, NULL);
}
