/**
 * @file input/mouse/hover.c
 *
 * @brief Periodic fallback re-evaluation of the resize-border cursor
 *        for a single tracked window
 *
 * A client that selects @c PointerMotion for its own purposes (common
 * in GTK/Qt applications tracking hover for their own UI) intercepts
 * motion events before they reach @a mouse_handle_motion_hover, and an
 * undecorated client has no separate frame window for a further
 * @c EnterNotify to catch when the pointer moves from its border into
 * its interior.  This periodic poll of one tracked window is the
 * fallback for both cases.
 *
 * @see @a im_hover_track's own comment for when it starts and stops
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
#include <stdint.h>
#include <stdlib.h>     /* free, NULL */
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Local includes */
#include <input/mouse/internal.h>
#include <input/mouse/hover.h>


/**
 * @brief Undecorated client whose resize cursor
 *       @a mouse_hover_poll_tick should keep re-evaluating, or
 *       @c XCB_WINDOW_NONE for none
 *
 * Set by @a im_hover_track, cleared by @a mouse_hover_poll_clear
 * (called from the @c LeaveNotify handler in @c loop.c) or the next
 * @a im_hover_track call for a window that does not itself warrant
 * tracking.  Tracked by window id rather than a @c client_td pointer
 * kept live across calls, so a client destroyed while still hovered
 * simply stops resolving in @a lookup_find_client on the next poll
 * rather than leaving a dangling pointer to clean up.
 */
static xcb_window_t s_hover_window = XCB_WINDOW_NONE;

/** Absolute time of the next scheduled poll for @a s_hover_window */
static struct timespec s_hover_next_poll;

/** How often 's_hover_window', while set, gets re-evaluated */
#define MOUSE_HOVER_POLL_INTERVAL_MS (100)


/**
 * @brief Schedule the next poll for @a s_hover_window to run
 *        @c MOUSE_HOVER_POLL_INTERVAL_MS from now
 *
 * @note Complexity: @e O(1)
 */
static void s_hover_reschedule(void)
{
    if (clock_gettime(CLOCK_MONOTONIC, &s_hover_next_poll) != 0) {
        return;
    }

    s_hover_next_poll.tv_nsec +=
        (long) MOUSE_HOVER_POLL_INTERVAL_MS * 1000000L;
    if (s_hover_next_poll.tv_nsec >= 1000000000L) {
        s_hover_next_poll.tv_sec += 1;
        s_hover_next_poll.tv_nsec -= 1000000000L;
    }
}


/* Clear the tracked hover window if it currently matches 'window' */
void mouse_hover_poll_clear(xcb_window_t window)
{
    if (window != XCB_WINDOW_NONE && window == s_hover_window) {
        s_hover_window = XCB_WINDOW_NONE;
    }
}


/* Milliseconds until 's_hover_window' should next be polled */
int mouse_hover_poll_ms_remaining(void)
{
    if (s_hover_window == XCB_WINDOW_NONE) {
        return -1;
    }

    return (int) clock_ms_until(&s_hover_next_poll);
}


/* Re-evaluate the resize cursor for 's_hover_window', if due */
void mouse_hover_poll_tick(xcb_connection_t *connection,
        list_td *surfaces)
{
    xcb_query_pointer_reply_t *reply;

    if (connection == NULL || surfaces == NULL ||
            s_hover_window == XCB_WINDOW_NONE ||
            mouse_hover_poll_ms_remaining() > 0) {
        return;
    }

    reply = xcb_query_pointer_reply(connection,
            xcb_query_pointer(connection, s_hover_window), NULL);
    if (reply != NULL) {
        if (reply->same_screen) {
            (void) im_update_resize_cursor(connection, surfaces,
                    s_hover_window,
                    (struct position_s) { reply->root_x,
                        reply->root_y });
        }
        free(reply);
    }

    s_hover_reschedule();
}


/* Update the pointer cursor to match a window's resize border */
void mouse_handle_motion_hover(xcb_connection_t *connection,
        list_td *surfaces, xcb_motion_notify_event_t *event)
{
    if (event == NULL) {
        return;
    }

    (void) im_update_resize_cursor(connection, surfaces,
            event->event,
            (struct position_s) { event->root_x, event->root_y });
}


/* Start (or clear) hover-poll tracking of a window's resize cursor */
void im_hover_track(xcb_window_t window)
{
    s_hover_window = window;
    if (s_hover_window != XCB_WINDOW_NONE) {
        s_hover_reschedule();
    }
}
