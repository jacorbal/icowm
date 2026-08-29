/**
 * @file wm/startup/selection.c
 *
 * @brief Acquiring the ICCCM manager selection for every managed
 *        screen
 *
 * The window this creates is later reused by @a wm_ewmh_init
 * (@c wm/ewmh.c) as the @c _NET_SUPPORTING_WM_CHECK window, rather than
 * that function creating one of its own: a single window owning both
 * roles is valid per ICCCM §2.8, and avoids this running twice, once
 * here and once there.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* free */
#include <string.h>     /* memset */
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* EWMH-related constants */
#include <defs/ewmh.h>

/* Project includes */
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Utils includes */
#include <utils/time/clock.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/reply.h>
#include <utils/xcb/wait.h>

/* Local includes */
#include <wm/startup/selection.h>
#include <utils/xcb/window.h>


/**
 * @brief Wait, bounded by @c WM_SN_REPLACE_TIMEOUT_MS, for a
 *        @c DestroyNotify on @p previous_owner
 *
 * Any other event that arrives on @p connection while waiting is
 * discarded outright: this runs before the main event loop starts,
 * so nothing else is watching the connection yet to hand such an
 * event off to.
 *
 * @param connection     XCB connection
 * @param previous_owner Window to wait for a @c DestroyNotify on
 *
 * @return @c true once @p previous_owner is destroyed, @c false once
 *         @c WM_SN_REPLACE_TIMEOUT_MS elapses first
 *
 * @note Complexity: @e O(n), where @e n is the number of X events
 *       delivered on @p connection before either outcome
 */
static bool s_wait_for_relinquish(xcb_connection_t *connection,
        xcb_window_t previous_owner)
{
    struct timespec deadline;

    clock_gettime(CLOCK_MONOTONIC, &deadline);
    clock_add_ms(&deadline, (unsigned int) WM_SN_REPLACE_TIMEOUT_MS);

    while (clock_ms_until(&deadline) > 0) {
        xcb_generic_event_t *event;

        if (!xcb_wait_readable(connection,
                    (int) clock_ms_until(&deadline))) {
            break;
        }

        while ((event = xcb_poll_for_event(connection)) != NULL) {
            bool is_match = false;

            if ((event->response_type & ~0x80u) ==
                    XCB_DESTROY_NOTIFY) {
                const xcb_destroy_notify_event_t *const dn =
                    (xcb_destroy_notify_event_t *) event;

                is_match = (dn->window == previous_owner);
            }
            free(event);
            if (is_match) {
                return true;
            }
        }
    }

    return false;
}


/**
 * @brief Acquire @p surface's own @c WM_S<n> selection with
 *        @p support, replacing a previous owner if asked to
 *
 * @param connection        XCB connection
 * @param support           Window to make the new selection owner
 * @param surface            Surface whose own selection is acquired
 * @param replace_requested Whether to wait out and replace a previous
 *                          owner instead of refusing outright
 * @param manager_atom      Interned @c MANAGER atom, or
 *                          @c XCB_ATOM_NONE if interning it failed
 *                          (the announcement is then skipped, this
 *                          function's own success is unaffected)
 *
 * @return 0 on success, -1 if @p surface has no owned selection to
 *         acquire, is already owned and @p replace_requested is
 *         @c false, or the previous owner does not relinquish it in
 *         time
 *
 * @note Complexity: @e O(1), aside from @a s_wait_for_relinquish's
 *       own cost when a previous owner must be waited out
 */
static int s_acquire_one_screen(xcb_connection_t *connection,
        xcb_window_t support, const surface_td *surface,
        bool replace_requested, xcb_atom_t manager_atom)
{
    char selection_name[16];
    xcb_atom_t selection_atom;
    xcb_get_selection_owner_reply_t *owner_reply;
    xcb_generic_error_t *owner_error = NULL;
    xcb_window_t previous_owner;

    (void) snprintf(selection_name, sizeof(selection_name),
            "WM_S%u", surface->id);
    selection_atom = atom_intern(connection, selection_name, false);
    if (selection_atom == XCB_ATOM_NONE) {
        return -1;
    }

    owner_reply = xcb_get_selection_owner_reply(connection,
            xcb_get_selection_owner(connection, selection_atom),
            &owner_error);
    xcb_reply_log_error(owner_error,
            "the current owner of a manager selection");
    previous_owner = (owner_reply != NULL)
        ? owner_reply->owner : (xcb_window_t) XCB_NONE;
    if (owner_reply != NULL) {
        free(owner_reply);
    }

    if (previous_owner != (xcb_window_t) XCB_NONE) {
        uint32_t values[1];

        if (!replace_requested) {
            LOGGER_FATAL("Screen %u's own '%s' selection is already" \
                    " owned by another window manager; pass '-r' to" \
                    " replace it", surface->id, selection_name);
            return -1;
        }

        LOGGER_NOTICE("Screen %u's own '%s' selection is already" \
                " owned; '-r' given, waiting for the previous window" \
                " manager to relinquish it", surface->id,
                selection_name);

        values[0] = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
        xcb_change_window_attributes(connection, previous_owner,
                XCB_CW_EVENT_MASK, values);
    }

    xcb_set_selection_owner(connection, support, selection_atom,
            XCB_CURRENT_TIME);
    xcb_flush(connection);

    if (previous_owner != (xcb_window_t) XCB_NONE) {
        if (!s_wait_for_relinquish(connection, previous_owner)) {
            LOGGER_FATAL("Screen %u's previous window manager did" \
                    " not relinquish '%s' within %u ms",
                    surface->id, selection_name,
                    (unsigned int) WM_SN_REPLACE_TIMEOUT_MS);
            return -1;
        }

        LOGGER_INFO("Screen %u's previous window manager relinquished" \
                " '%s'; taking over", surface->id, selection_name);
    }

    if (manager_atom != XCB_ATOM_NONE) {
        xcb_client_message_event_t manager_event;

        memset(&manager_event, 0, sizeof(manager_event));
        manager_event.response_type = XCB_CLIENT_MESSAGE;
        manager_event.format = 32;
        manager_event.window = surface->screen->root;
        manager_event.type = manager_atom;
        manager_event.data.data32[0] = XCB_CURRENT_TIME;
        manager_event.data.data32[1] = selection_atom;
        manager_event.data.data32[2] = support;
        manager_event.data.data32[3] = 0u;
        manager_event.data.data32[4] = 0u;
        xcb_send_event(connection, 0, surface->screen->root,
                XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                (const char *) &manager_event);
    }

    return 0;
}


/* Acquire the 'WM_Sn' manager selection on every managed screen,
 * taking over an already-running window manager's own ownership when
 * asked to */
int wm_startup_acquire_selection(wm_td *wm, bool replace_requested)
{
    xcb_connection_t *connection = wm_connection(wm);
    list_td *surfaces = wm_surfaces(wm);
    xcb_atom_t manager_atom;
    xcb_window_t support;

    if (wm == NULL || connection == NULL || surfaces == NULL) {
        return -1;
    }

    manager_atom = atom_intern(connection, "MANAGER", false);

    support = xcb_generate_id(connection);
    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            support,
            xcb_setup_roots_iterator(xcb_get_setup(
                        connection)).data->root,
            0, 0, 1, 1,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            0, NULL);

    for (list_item_td *node = list_head(surfaces);
            node != NULL; node = list_next(node)) {
        const surface_td *const surface =
            (const surface_td *) list_data(node);

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        if (s_acquire_one_screen(connection, support, surface,
                    replace_requested, manager_atom) != 0) {
            xcb_window_destroy(support);
            xcb_flush(connection);
            return -1;
        }
    }

    wm_set_ewmh_support_win(wm, support);
    xcb_flush(connection);
    return 0;
}
