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

/* Policy includes */
#include <policy/focus.h>
#include <policy/urgency.h>

/* IPC includes */
#include <ipc.h>

/* Input includes */
#include <input/kbd/bind.h>
#include <input/kbd/event.h>
#include <input/mouse/bind.h>
#include <input/mouse/event.h>
#include <input/mouse/hover.h>
#include <input/mouse/drag.h>
#include <input/mouse/drag/warp.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/message.h>
#include <menu/notify/desktop.h>
#include <menu/popup.h>
#include <menu/search.h>

/* Default initial values */
#include <defs/ipc.h>
#include <defs/loop.h>

/* Project includes */
#include <handler.h>
#include <cctl/adopt.h>
#include <lookup.h>
#include <logger.h>
#include <memguard.h>
#include <cctl/sn.h>
#include <wm/startup/handle.h>
#include <wm/startup/install.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>
#include <cctl/kill.h>
#include <wm/shutdown.h>

/* Local includes */
#include <loop.h>


/**
 * @brief Perform a partial (outdated-only) surface update
 *
 * Re-renders only the surfaces that have been marked as outdated.
 * Called on every iteration of the main event loop.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
static void s_loop_update(const wm_td *wm)
{
    list_td *surfaces = wm_surfaces(wm);

    if (wm == NULL || surfaces == NULL) {
        return;
    }

    for (list_item_td *node = list_head(surfaces);
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
 * @brief Handle pointer-leave notifications for focus-sloppy
 *
 * @param wm    Window-manager singleton
 * @param event Leave-notify event to process
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected by @a lookup_find_client
 */
static void s_loop_handle_leave_notify(const wm_td *wm,
        xcb_leave_notify_event_t *event)
{
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    const config_td *config = wm_config(wm);
    xcb_connection_t *connection = wm_connection(wm);

    if (wm == NULL || event == NULL) {
        return;
    }

    /* Independent of focus-follows-mouse below: a resize-cursor poll
     * target (see 'mouse_hover_poll_tick' in input/mouse/hover.h)
     * tracked for this window must stop being polled once the pointer
     * has actually left it, regardless of whether hover also affects
     * focus. */
    mouse_hover_poll_clear(event->event);

    if (focus_is_sloppy(config) &&
            event->mode == XCB_NOTIFY_MODE_NORMAL &&
            event->detail != XCB_NOTIFY_DETAIL_INFERIOR &&
            lookup_find_client(wm_surfaces(wm), event->event,
                    &surface, &desktop) != NULL) {
        /* Pointer left a managed window; release focus so the cursor
         * resting on the root background leaves all clients visually
         * unfocused */
        xcb_set_input_focus(connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                event->time);
        if (desktop != NULL) {
            desktop->client_active_id = 0;
            desktop->focus_dirty = true;
            desktop->is_outdated = true;
        }
        if (surface != NULL) {
            surface->is_outdated = true;
        }
        xcb_flush(connection);
    }
}


/**
 * @brief Mark a client dirty after losing X input focus
 *
 * @param wm    Window-manager singleton
 * @param event Focus-out event to process
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected by @a lookup_find_client
 */
static void s_loop_handle_focus_out(const wm_td *wm,
        xcb_focus_out_event_t *event)
{
    surface_td *surface = NULL;

    if (wm == NULL || event == NULL) {
        return;
    }

    if ((event->mode == XCB_NOTIFY_MODE_NORMAL ||
                event->mode == XCB_NOTIFY_MODE_WHILE_GRABBED) &&
            lookup_find_client(wm_surfaces(wm), event->event,
                    &surface, NULL) != NULL &&
            surface != NULL) {
        surface->is_outdated = true;
    }
}


/**
 * @brief Tighten a poll timeout to a candidate deadline, if sooner
 *
 * Shared by every "shorten the poll timeout so some pending countdown
 * (a popup's auto-close, a startup-notification busy cursor's expiry,
 * a hover-triggered cursor re-check, ...) fires promptly" check in
 * @a loop_run below, which otherwise each repeated the same "is this
 * candidate both valid and sooner than what we already have" test.
 *
 * @param poll_timeout_ms Current timeout, in milliseconds; lowered in
 *                        place when @p candidate_ms is sooner
 * @param candidate_ms    A countdown's own remaining time, or
 *                        a negative value when that countdown is not
 *                        currently active at all
 *
 * @note Complexity: @e O(1)
 */
static void s_loop_tighten_poll_timeout(int *poll_timeout_ms,
        int candidate_ms)
{
    if (candidate_ms >= 0 && candidate_ms < *poll_timeout_ms) {
        *poll_timeout_ms = candidate_ms;
    }
}


/**
 * @brief Close a single-instance overlay dialog and repaint whichever
 *        surface is first in the surface list
 *
 * Shared by @a loop_run's own timed auto-close for the info popup and
 * the desktop-switch notification below: both close a dialog that,
 * unlike a per-client one, is not tied to any one particular surface,
 * so any surface's own current-desktop repaint is enough to clear its
 * remnants from the screen.
 *
 * @param connection XCB connection
 * @param surfaces   Surface list to find a repaint target in
 * @param close_fn   The dialog's own @c X_close function
 *
 * @note Complexity: @e O(1), since only the first surface is needed
 */
static void s_loop_close_and_repaint_first_surface(
        xcb_connection_t *connection, list_td *surfaces,
        void (*close_fn)(xcb_connection_t *))
{
    surface_td *found = NULL;

    for (list_item_td *node = list_head(surfaces); node != NULL;
            node = list_next(node)) {
        surface_td *const s = (surface_td *) list_data(node);
        if (s != NULL) {
            found = s;
            break;
        }
    }

    close_fn(connection);
    if (found != NULL) {
        surface_render_current_desktop_repaint(found);
    }
}


/**
 * @brief Human-readable description for an @c xcb_connection_has_error
 *        return value
 *
 * Distinguishes an ordinary per-window protocol error, which XCB
 * delivers as a regular event and never causes this, from an actual
 * connection failure: the socket to the X server itself is gone,
 * something no window manager can recover from, since the window
 * manager is just another client of that same server.  Logging which
 * one occurred is the most this function's caller can do about it; an
 * @c XCB_CONN_ERROR here in particular, especially right after a client
 * (e.g., a game attempting hardware-accelerated rendering) was seen
 * doing something unusual, is worth checking the system's own logs
 * (Xorg's own log file, @c dmesg for a GPU driver crash) for, outside
 * of icowm entirely.
 *
 * @param error_code Value returned by @a xcb_connection_has_error
 *
 * @return A short, constant description; never @c NULL
 *
 * @note Complexity: @e O(1)
 */
static const char *s_loop_connection_error_string(int error_code)
{
    switch (error_code) {
        case XCB_CONN_ERROR:
            return "XCB_CONN_ERROR (socket, pipe, or other stream" \
                " error; most likely the X server itself is gone)";
        case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
            return "XCB_CONN_CLOSED_EXT_NOTSUPPORTED" \
                " (a required X extension is not supported)";
        case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
            return "XCB_CONN_CLOSED_MEM_INSUFFICIENT" \
                " (out of memory)";
        case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
            return "XCB_CONN_CLOSED_REQ_LEN_EXCEED" \
                " (a request exceeded the server's maximum length)";
        case XCB_CONN_CLOSED_PARSE_ERR:
            return "XCB_CONN_CLOSED_PARSE_ERR" \
                " (error parsing the display name)";
        case XCB_CONN_CLOSED_INVALID_SCREEN:
            return "XCB_CONN_CLOSED_INVALID_SCREEN" \
                " (the server has no screen matching the display)";
        case XCB_CONN_CLOSED_FDPASSING_FAILED:
            return "XCB_CONN_CLOSED_FDPASSING_FAILED" \
                " (file descriptor passing failed)";
        default:
            return "unknown XCB connection error code";
    }
}


/**
 * @brief Refresh the client behind a genuine input event's own
 *        @c user_time, for @c _NET_ACTIVE_WINDOW focus-stealing
 *        prevention to compare against later
 *
 * @param wm            Window manager state, for @p surfaces
 * @param window        Window a real @c KeyPress or @c ButtonPress
 *                      named as its own @c event field, i.e., the one
 *                      that actually received it
 * @param response_type The raw, unmasked @c response_type off the event
 *                      itself, top bit included
 * @param time          X server timestamp of the event
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
static void s_loop_note_real_input(const wm_td *wm, xcb_window_t window,
        uint8_t response_type, uint32_t time)
{
    client_td *client;

    if ((response_type & 0x80u) != 0u) {
        return;
    }

    client = lookup_find_client(wm_surfaces(wm), window, NULL, NULL);
    client_update_user_time(client, time);
}


/**
 * @brief Force a full re-render of all surfaces
 *
 * Marks every surface as outdated and then delegates to @a loop_update.
 * Called once before entering the event loop so pre-existing windows
 * are drawn from scratch.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       and @e m is the number of desktops
 */
static void s_loop_update_full(const wm_td *wm)
{
    list_td *surfaces = wm_surfaces(wm);

    if (wm == NULL || surfaces == NULL) {
        return;
    }

    LOGGER_TRACE("Fully updating window manager", L_NARG);

    for (list_item_td *node = list_head(surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *const surface = (surface_td *) list_data(node);
        if (surface != NULL) {
            surface->is_outdated = true;
        }
    }

    s_loop_update(wm);
}


/**
 * @brief Log an X protocol error (event type @c 0) at the right
 *        severity
 *
 * X has no named constant for a protocol error as an event type;
 * @c 0 never collides with a real one, since those start at @c 1.
 * Not inherently fatal on its own, unlike an actual connection failure.
 *
 * X11's request/reply model is asynchronous: a window this window
 * manager just sent a routine request against (reconfigure it, change
 * or delete one of its properties, query its geometry, give it input
 * focus, ...) can legitimately have been destroyed by whichever client
 * owned it before the server gets around to processing that request,
 * and nothing on this side can prevent that race without a synchronous
 * round trip before every single such request, far too costly to do
 * routinely.  @c BadWindow / @c BadDrawable (the resource itself is
 * simply gone by then) and @c BadMatch (ICCCM's defined failure for,
 * e.g., @c SetInputFocus on a target that is no longer viewable)
 * against one of the request codes in @a s_routine_target_ops below are
 * exactly that expected race, not a sign of anything actually wrong on
 * this window manager's end, so they are logged at DEBUG instead, e.g.:
 *
 * +-------+-----------------+-----------+--------+
 * | major | request         | error     | result |
 * +-------+-----------------+-----------+--------+
 * | 12    | ConfigureWindow | BadWindow | DEBUG  |
 * | 18    | ChangeProperty  | BadWindow | DEBUG  |
 * | 19    | DeleteProperty  | BadWindow | DEBUG  |
 * | 42    | SetInputFocus   | BadMatch  | DEBUG  |
 * +-------+-----------------+-----------+--------+
 *
 * Any other error (@c BadValue, @c BadAlloc, @c BadAccess, or even
 * @c BadWindow / @c BadMatch on a request outside that "routine" set)
 * still gets WARNING (rather than @a loop_run's switch's usual
 * TRACE-level default), so it is not lost among routine unhandled-event
 * traffic, since it is far more likely to be a genuine bug worth
 * noticing.
 *
 * @param event The raw event @a loop_run received with response type
 *              @c 0, reinterpreted here as the protocol error it
 *              actually is
 *
 * @note Complexity: @e O(1)
 *
 * @ see @a s_loop_connection_error_string's call site in @a loop_run
 */
static void s_loop_handle_protocol_error(const xcb_generic_event_t *event)
{
    static const uint8_t s_routine_target_ops[] = {
        2u,  /* X_ChangeWindowAttributes */
        3u,  /* X_GetWindowAttributes */
        4u,  /* X_DestroyWindow */
        8u,  /* X_MapWindow */
        10u, /* X_UnmapWindow */
        12u, /* X_ConfigureWindow */
        14u, /* X_GetGeometry */
        15u, /* X_QueryTree */
        18u, /* X_ChangeProperty */
        19u, /* X_DeleteProperty */
        20u, /* X_GetProperty */
        42u  /* X_SetInputFocus */
    };
    const xcb_generic_error_t *proto_error;
    bool is_routine_target_op = false;
    bool is_vanished_resource_error;

    proto_error = (const xcb_generic_error_t *) event;

    for (size_t oi = 0u; oi < sizeof(s_routine_target_ops) /
            sizeof(s_routine_target_ops[0]); ++oi) {
        if (proto_error->major_code == s_routine_target_ops[oi]) {
            is_routine_target_op = true;
            break;
        }
    }

    is_vanished_resource_error = is_routine_target_op &&
        (proto_error->error_code == 3u  /* BadWindow */ ||
         proto_error->error_code == 9u  /* BadDrawable */ ||
         proto_error->error_code == 8u  /* BadMatch */);

    if (is_vanished_resource_error) {
        LOGGER_DEBUG("X protocol error (code=%u," \
                " resource=0x%x, major=%u, minor=%u," \
                " sequence=%u)",
                proto_error->error_code,
                proto_error->resource_id,
                proto_error->major_code,
                proto_error->minor_code,
                proto_error->sequence);
    } else {
        LOGGER_WARNING("X protocol error (code=%u," \
                " resource=0x%x, major=%u, minor=%u," \
                " sequence=%u)",
                proto_error->error_code,
                proto_error->resource_id,
                proto_error->major_code,
                proto_error->minor_code,
                proto_error->sequence);
    }
}


/* Run the main event loop until the window manager is stopped */
void loop_run(wm_td *wm)
{
    xcb_key_symbols_t *keysyms;
    xcb_generic_event_t *pending_event = NULL; /**< One-event lookahead
                                                    used to coalesce
                                                    a run of consecutive
                                                    @c MotionNotify
                                                    events (see the
                                                    comment at the
                                                    @c XCB_MOTION_NOTIFY
                                                    case below) */
    struct pollfd pfd[1 + IPC_MAX_CLIENTS + 1];
    bool any_outdated;
    xcb_connection_t *connection;
    list_td *surfaces;
    const config_td *config;
    uint32_t restricted_memory_mib;
    bool randr_available;
    uint8_t randr_base_event;
    bool sync_available;
    uint8_t sync_base_event;

    if (wm == NULL || !wm_is_running(wm)) {
        LOGGER_TRACE("Window manager is not initialized or" \
                " set to not run", L_NARG);
        return;
    }

    connection = wm_connection(wm);
    surfaces = wm_surfaces(wm);
    config = wm_config(wm);
    restricted_memory_mib = wm_restricted_memory_mib(wm);
    randr_available = wm_randr_available(wm);
    randr_base_event = wm_randr_base_event(wm);
    sync_available = wm_sync_available(wm);
    sync_base_event = wm_sync_base_event(wm);

    if (wm_startup_install_signals() != 0) {
        LOGGER_WARNING("Continuing without termination signal handling",
                L_NARG);
    }

    if (wm_startup_install_crash_handlers() != 0) {
        LOGGER_WARNING("Continuing without fatal-signal diagnostics",
                L_NARG);
    }

    keysyms = xcb_key_symbols_alloc(connection);
    if (keysyms == NULL) {
        LOGGER_ERROR("Failed to allocate key symbols table", L_NARG);
        return;
    }
    wm_set_keysyms(wm, keysyms);

    keyboard_load(surfaces, keysyms, config);
    mouse_load(surfaces, config);

    cctl_adopt_scan(wm);
    s_loop_update_full(wm);

    /* Synchronize EWMH root properties after the initial scan so that
     * taskbars reading '_NET_CLIENT_LIST' see the windows that were
     * adopted by 'cctl_adopt_scan'.  The earlier 'wm_ewmh_sync' call in
     * 'wm_init' ran before any clients were managed, leaving the list
     * empty; 'loop_update_full' then cleared 'is_outdated', so the
     * first main-loop iteration would never trigger a sync on its
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
            keyboard_load(surfaces, keysyms, config);
            mouse_load(surfaces, config);
        }

        if (wm_startup_requested_child_reap()) {
            session_reap_children();
        }

        conn_error = xcb_connection_has_error(connection);
        if (conn_error != 0) {
            LOGGER_ERROR("X connection error detected (%s);" \
                    " requesting shutdown",
                    s_loop_connection_error_string(conn_error));
            wm_request_stop();
            break;
        }

        pfd[0].fd = xcb_get_file_descriptor(connection);
        pfd[0].events = POLLIN;
        pfd[0].revents = 0;
        nfds = 1;

        for (int i = 0; i < ipc_count; ++i) {
            pfd[nfds].fd = ipc_fds[i];
            pfd[nfds].events = POLLIN;
            pfd[nfds].revents = 0;
            ++nfds;
        }

        /* Use a shorter poll timeout when the info popup is visible so
         * it closes promptly at the configured expiry time. */
        poll_timeout_ms = WM_EVENT_POLL_TIMEOUT_MS;
        if (popup_is_open()) {
            s_loop_tighten_poll_timeout(&poll_timeout_ms,
                    popup_ms_remaining());
        }

        if (notify_desktop_is_open()) {
            s_loop_tighten_poll_timeout(&poll_timeout_ms,
                    notify_desktop_ms_remaining());
        }

        s_loop_tighten_poll_timeout(&poll_timeout_ms,
                systray_clock_ms_remaining());

        s_loop_tighten_poll_timeout(&poll_timeout_ms,
                urgency_blink_ms_remaining(config));

        s_loop_tighten_poll_timeout(&poll_timeout_ms,
                cctl_sn_ms_remaining());

        /* Shorter still while a resize-cursor poll target is being
         * tracked (see 'mouse_hover_poll_tick' in
         * 'input/mouse/hover.h'), so an undecorated client's cursor
         * gets re-evaluated promptly as the pointer moves within it. */
        s_loop_tighten_poll_timeout(&poll_timeout_ms,
                mouse_hover_poll_ms_remaining());

        /* Shorter still while a confirm dialog has a timer of its own
         * running (see 'menu_confirm_dialog_tick' in
         * 'menu/dialog/confirm.h').  A pending click-triggered
         * close/accept, or a countdown timeout that needs its visible
         * number to advance once a second and, once it fully elapses,
         * to act. */
        s_loop_tighten_poll_timeout(&poll_timeout_ms,
                menu_confirm_dialog_ms_remaining());

        /* Shorter still while the message dialog has a
         * click-triggered close of its own pending (see
         * 'menu_message_dialog_tick' in 'menu/dialog/message.'h). */
        s_loop_tighten_poll_timeout(&poll_timeout_ms,
                menu_message_dialog_ms_remaining());

        /* Shorter still while a window drag is holding the pointer
         * against a warp-eligible screen edge (see 'drag_warp_tick' in
         * 'input/mouse/drag.h'), so it still switches desktops once its
         * own countdown elapses even with no further 'MotionNotify'
         * arriving to drive it. */
        s_loop_tighten_poll_timeout(&poll_timeout_ms,
                drag_warp_ms_remaining());

        /* Shorter still while a coordinated shutdown (see
         * 'wm_shutdown_tick' in 'wm/shutdown.h') is waiting on managed
         * clients to close on their own, so the timeout that forces the
         * rest closed elapses promptly instead of waiting for the next
         * unrelated event to wake the loop up. */
        s_loop_tighten_poll_timeout(&poll_timeout_ms,
                wm_shutdown_ms_remaining());

        /* Shorter still while a process kill is pending escalation
         * to 'SIGKILL' (see 'cctl_kill_tick' in 'wm/kill.h'), for the
         * same reason. */
        s_loop_tighten_poll_timeout(&poll_timeout_ms,
                cctl_kill_ms_remaining());

        poll_status = poll(pfd, (nfds_t) nfds, poll_timeout_ms);
        if (poll_status < 0 && errno != EINTR) {
            LOGGER_ERROR("Failed waiting on X connection: %s",
                    strerror(errno));
            break;
        }

        /* Every non-X11 descriptor 'poll' reported ready belongs to IPC
         * (index 0 is always the X connection, handled below via
         * 'xcb_poll_for_event' instead of this array at all).
         * 'ipc_handle_readable' itself tells the listening socket apart
         * from an already-connected client, so nothing here needs
         * to. */
        if (poll_status > 0) {
            for (int i = 1; i < nfds; ++i) {
                if (pfd[i].revents & POLLIN) {
                    ipc_handle_readable(wm, pfd[i].fd);
                }
            }
        }

        systray_clock_tick();
        urgency_blink_tick(surfaces, config);
        cctl_sn_tick(connection, surfaces);
        mouse_hover_poll_tick(connection, surfaces);
        menu_confirm_dialog_tick(connection, config);
        menu_message_dialog_tick(connection);
        drag_warp_tick(connection);
        wm_shutdown_tick(wm);
        cctl_kill_tick();
        if (restricted_memory_mib > 0u &&
                surfaces != NULL && !list_is_empty(surfaces)) {
            memguard_tick(connection,
                    (surface_td *) list_data(list_head(surfaces)),
                    config);
        }

        while ((event = (pending_event != NULL)
                    ? pending_event
                    : xcb_poll_for_event(connection)) != NULL) {
            xcb_motion_notify_event_t *me;
            uint8_t event_type;

            pending_event = NULL;
            event_type = (uint8_t) (event->response_type & ~0x80u);

            if (randr_available &&
                    (event_type == (uint8_t) (randr_base_event +
                            XCB_RANDR_SCREEN_CHANGE_NOTIFY) ||
                     event_type == (uint8_t) (randr_base_event +
                            XCB_RANDR_NOTIFY))) {
                handler_randr_event(wm, event);
                free(event);
                continue;
            }

            if (sync_available &&
                    event_type == (uint8_t) (sync_base_event +
                            XCB_SYNC_ALARM_NOTIFY)) {
                handler_sync_event(wm, event);
                free(event);
                continue;
            }

            switch (event->response_type & ~0x80u) {
                case XCB_KEY_PRESS: {
                    xcb_key_press_event_t *const kp =
                        (xcb_key_press_event_t *) event;

                    s_loop_note_real_input(wm, kp->event,
                            event->response_type, kp->time);
                    keyboard_handle_press(wm, keysyms, kp,
                            surfaces, config);
                    break;
                }

                case XCB_KEY_RELEASE:
                    keyboard_handle_release(keysyms,
                            (xcb_key_release_event_t *) event,
                            surfaces, config);
                    break;

                case XCB_BUTTON_PRESS: {
                    xcb_button_press_event_t *const bp =
                        (xcb_button_press_event_t *) event;

                    s_loop_note_real_input(wm, bp->event,
                            event->response_type, bp->time);
                    mouse_handle_press(wm, connection, surfaces,
                            bp, config);
                    break;
                }

                case XCB_BUTTON_RELEASE:
                    mouse_handle_release(connection, surfaces,
                            (xcb_button_release_event_t *) event,
                            config);
                    break;

                case XCB_MOTION_NOTIFY:
                    me = (xcb_motion_notify_event_t *) event;

                    /* Coalesce a run of consecutive pending
                     * 'MotionNotify' events into just the latest one.
                     * The X server can queue many of these faster than
                     * one round of window-move (or resize) plus
                     * 'xcb_flush' can be processed, especially for
                     * a large or decorated window whose move is more
                     * expensive per event (the frame itself repaints,
                     * and reparented-child bookkeeping adds further
                     * server-side cost on top of a plain top-level
                     * window's move).  Reacting to every stale
                     * intermediate position instead of jumping straight
                     * to the newest one is what makes a drag visibly
                     * lag behind the pointer, worse the more expensive
                     * that per-event work is.  A non-motion event found
                     * while peeking ahead is kept in 'pending_event'
                     * rather than dropped, so it is still handled, on
                     * the very next iteration of this same loop. */
                    while ((pending_event =
                                xcb_poll_for_event(connection)) !=
                            NULL) {
                        if ((uint8_t) (pending_event->response_type &
                                    ~0x80u) != XCB_MOTION_NOTIFY) {
                            break;
                        }
                        free(event);
                        event = pending_event;
                        me = (xcb_motion_notify_event_t *) event;
                    }

                    drag_update(connection,
                            (struct position_s) {
                                me->root_x, me->root_y
                            });
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
                        mouse_handle_motion_hover(connection,
                                surfaces, me);
                    }
                    break;

                case XCB_ENTER_NOTIFY:
                    mouse_handle_enter(connection, surfaces,
                            (xcb_enter_notify_event_t *) event,
                            config);
                    break;

                case XCB_CONFIGURE_NOTIFY:
                    handler_configure_notify(connection,
                            surfaces,
                            (xcb_configure_notify_event_t *) event);
                    break;

                case XCB_UNMAP_NOTIFY:
                    handler_unmap_notify(connection,
                            surfaces,
                            (xcb_unmap_notify_event_t *) event);
                    break;

                case XCB_DESTROY_NOTIFY:
                    systray_handle_destroy(wm,
                            ((xcb_destroy_notify_event_t *)
                                event)->window);
                    handler_destroy_notify(connection,
                            surfaces,
                            (xcb_destroy_notify_event_t *) event);
                    break;

                case XCB_PROPERTY_NOTIFY:
                    handler_property_notify(wm, connection,
                            surfaces,
                            (xcb_property_notify_event_t *) event);
                    break;

                case XCB_FOCUS_IN:
                    handler_focus_in(connection, surfaces,
                            (xcb_focus_in_event_t *) event);
                    break;

                case XCB_COLORMAP_NOTIFY:
                    handler_colormap_notify(connection, surfaces,
                            (xcb_colormap_notify_event_t *) event);
                    break;

                case XCB_EXPOSE:
                    handler_expose(connection, surfaces,
                            (xcb_expose_event_t *) event, config);
                    break;

                case XCB_CONFIGURE_REQUEST:
                    handler_configure_request(connection,
                            surfaces,
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
                    handler_mapping_notify(keysyms, surfaces,
                            (xcb_mapping_notify_event_t *) event,
                            config);
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
                    handler_map_notify(connection,
                            surfaces,
                            (xcb_map_notify_event_t *) event);
                    break;

                case XCB_GRAVITY_NOTIFY:
                    handler_gravity_notify(connection,
                            surfaces,
                            (xcb_gravity_notify_event_t *) event);
                    break;

                case XCB_CIRCULATE_NOTIFY:
                    handler_circulate_notify(connection,
                            surfaces,
                            (xcb_circulate_notify_event_t *) event);
                    break;

                case XCB_CIRCULATE_REQUEST:
                    handler_circulate_request(connection,
                            surfaces,
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
                    s_loop_handle_protocol_error(event);
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
            s_loop_close_and_repaint_first_surface(connection,
                    surfaces, popup_close);
        }

        /* Auto-close the desktop notify when its timeout has elapsed */
        if (notify_desktop_is_open() &&
                notify_desktop_ms_remaining() == 0) {
            s_loop_close_and_repaint_first_surface(connection,
                    surfaces, notify_desktop_close);
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
        for (list_item_td *sync_node = list_head(surfaces);
                sync_node != NULL; sync_node = list_next(sync_node)) {
            const surface_td *s = (surface_td *) list_data(sync_node);
            if (s != NULL && s->is_outdated) {
                any_outdated = true;
                break;
            }
        }

        s_loop_update(wm);
        if (any_outdated) {
            wm_ewmh_sync(wm);
        }
    }

    LOGGER_DEBUG("Exiting event loop", L_NARG);
    xcb_key_symbols_free(keysyms);
    wm_set_keysyms(wm, NULL);
}
