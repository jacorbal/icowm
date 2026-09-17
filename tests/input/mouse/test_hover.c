/**
 * @file tests/input/mouse/test_hover.c
 *
 * @brief Test battery for resize-cursor hover tracking, on motion and
 *        on periodic poll
 *
 * 'mouse_resize_cursor_update' (input/mouse/cursor.c) is a genuinely
 * separate topic file (resize-cursor loading and application) that
 * 'hover.c' only ever calls into, never redefines, so it is replaced
 * here by a recording link-only stand-in rather than linking the real
 * cursor.c, which would drag in XCB cursor-glyph loading of its own.
 * 'xcb_query_pointer'/'xcb_query_pointer_reply' are real XCB protocol
 * calls that need a live X server to answer for real, so they are
 * replaced by controlled stand-ins that hand back a fabricated reply
 * built by each scenario.
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
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/hover.h>


/** Non-null opaque handle standing in for a real xcb_connection_t,
 *  which this file never actually builds, since the type is opaque
 *  outside libxcb itself; every stand-in below ignores it, only ever
 *  checking it is non-NULL, so a dummy address is enough */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
        (xcb_connection_t *) &s_fake_connection_storage;

/** Non-null opaque handle standing in for a real list_td of stages */
static int s_fake_stages_storage;
static list_td *const s_fake_stages = (list_td *) &s_fake_stages_storage;

/** Call counters and last-seen arguments, reset by s_reset before each
 *  scenario */
static int s_call_resize_cursor_update;
static xcb_window_t s_last_window;
static int32_t s_last_root_x;
static int32_t s_last_root_y;

/** Whether 'xcb_query_pointer_reply' should hand back a reply at all,
 *  and, when it does, what 'same_screen'/coordinates it carries */
static bool s_reply_present;
static uint8_t s_reply_same_screen;
static int16_t s_reply_root_x;
static int16_t s_reply_root_y;
static int s_call_query_pointer;
static int s_call_query_pointer_reply;


/**
 * @brief Reset every recording stand-in's state before a scenario
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_call_resize_cursor_update = 0;
    s_last_window = XCB_WINDOW_NONE;
    s_last_root_x = 0;
    s_last_root_y = 0;
    s_reply_present = false;
    s_reply_same_screen = 0;
    s_reply_root_x = 0;
    s_reply_root_y = 0;
    s_call_query_pointer = 0;
    s_call_query_pointer_reply = 0;
}


/**
 * @brief Recording stand-in for @a mouse_resize_cursor_update
 *
 * @note Complexity: @e O(1)
 */
client_td *mouse_resize_cursor_update(xcb_connection_t *connection,
        list_td *stages, xcb_window_t window, struct position_s root_pos)
{
    (void) connection;
    (void) stages;
    s_call_resize_cursor_update++;
    s_last_window = window;
    s_last_root_x = root_pos.x;
    s_last_root_y = root_pos.y;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_query_pointer
 *
 * The returned cookie is never inspected by 'hover.c' beyond handing
 * it straight to 'xcb_query_pointer_reply', so a zeroed one is enough
 *
 * @note Complexity: @e O(1)
 */
xcb_query_pointer_cookie_t xcb_query_pointer(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_query_pointer_cookie_t cookie;

    (void) connection;
    (void) window;
    s_call_query_pointer++;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Controlled stand-in for @a xcb_query_pointer_reply
 *
 * Hands back a heap-allocated reply built from 's_reply_*' when
 * 's_reply_present' is set (so the caller's 'free' has a real
 * allocation to release), or @c NULL otherwise
 *
 * @note Complexity: @e O(1)
 */
xcb_query_pointer_reply_t
    *xcb_query_pointer_reply(xcb_connection_t *connection,
            xcb_query_pointer_cookie_t cookie,
            xcb_generic_error_t **error)
{
    xcb_query_pointer_reply_t *reply;

    (void) connection;
    (void) cookie;
    s_call_query_pointer_reply++;
    if (error != NULL) {
        *error = NULL;
    }

    if (!s_reply_present) {
        return NULL;
    }

    reply = malloc(sizeof(*reply));
    memset(reply, 0, sizeof(*reply));
    reply->same_screen = s_reply_same_screen;
    reply->root_x = s_reply_root_x;
    reply->root_y = s_reply_root_y;
    return reply;
}


/* Tracking XCB_WINDOW_NONE never schedules a poll: ms_remaining stays -1 */
static void s_test_track_none(void)
{
    s_reset();

    mouse_hover_track(XCB_WINDOW_NONE);

    TAP_EQ_INT(mouse_hover_poll_ms_remaining(), -1,
            "tracking XCB_WINDOW_NONE leaves nothing scheduled");
}


/* Tracking a real window schedules a poll roughly 100ms out */
static void s_test_track_schedules_poll(void)
{
    int remaining;

    s_reset();

    mouse_hover_track((xcb_window_t) 42);
    remaining = mouse_hover_poll_ms_remaining();

    TAP_OK(remaining >= 0 && remaining <= 100,
            "tracking a real window schedules a poll within" \
            " 0..100ms from now");

    mouse_hover_poll_clear((xcb_window_t) 42);
}


/* Clearing the tracked window with a mismatching id is a no-op */
static void s_test_clear_mismatch_is_noop(void)
{
    int before;
    int after;

    s_reset();

    mouse_hover_track((xcb_window_t) 7);
    before = mouse_hover_poll_ms_remaining();
    mouse_hover_poll_clear((xcb_window_t) 999);
    after = mouse_hover_poll_ms_remaining();

    TAP_OK(before >= 0, "window 7 is tracked before the mismatched clear");
    TAP_OK(after >= 0,
            "clearing a different window id leaves tracking untouched");

    mouse_hover_poll_clear((xcb_window_t) 7);
}


/* Clearing XCB_WINDOW_NONE is always a no-op, even while tracking */
static void s_test_clear_none_is_noop(void)
{
    int before;
    int after;

    s_reset();

    mouse_hover_track((xcb_window_t) 7);
    before = mouse_hover_poll_ms_remaining();
    mouse_hover_poll_clear(XCB_WINDOW_NONE);
    after = mouse_hover_poll_ms_remaining();

    TAP_OK(before >= 0 && after >= 0,
            "clearing XCB_WINDOW_NONE never clears the real tracked window");

    mouse_hover_poll_clear((xcb_window_t) 7);
}


/* Clearing the matching tracked window stops tracking it */
static void s_test_clear_match_stops_tracking(void)
{
    s_reset();

    mouse_hover_track((xcb_window_t) 55);
    mouse_hover_poll_clear((xcb_window_t) 55);

    TAP_EQ_INT(mouse_hover_poll_ms_remaining(), -1,
            "clearing the tracked window's own id stops tracking it");
}


/* Tick is a no-op with a NULL connection or NULL stages */
static void s_test_tick_null_args(void)
{
    s_reset();

    mouse_hover_track((xcb_window_t) 3);

    mouse_hover_poll_tick(NULL, s_fake_stages);
    TAP_EQ_INT(s_call_query_pointer, 0,
            "a NULL connection never reaches xcb_query_pointer");

    mouse_hover_poll_tick(s_fake_connection, NULL);
    TAP_EQ_INT(s_call_query_pointer, 0,
            "NULL stages never reaches xcb_query_pointer either");

    mouse_hover_poll_clear((xcb_window_t) 3);
}


/* Tick is a no-op when nothing is tracked */
static void s_test_tick_nothing_tracked(void)
{
    s_reset();

    mouse_hover_poll_tick(s_fake_connection, s_fake_stages);

    TAP_EQ_INT(s_call_query_pointer, 0,
            "ticking with nothing tracked never queries the pointer");
}


/* Tick is a no-op when the next scheduled poll is not yet due */
static void s_test_tick_not_due_yet(void)
{
    s_reset();

    mouse_hover_track((xcb_window_t) 9);
    mouse_hover_poll_tick(s_fake_connection, s_fake_stages);

    TAP_EQ_INT(s_call_query_pointer, 0,
            "ticking right after tracking starts is never due yet" \
            " (100ms has not elapsed)");

    mouse_hover_poll_clear((xcb_window_t) 9);
}


/* A due tick with same_screen false skips the cursor update entirely */
static void s_test_tick_due_other_screen(void)
{
    struct timespec until_due;

    s_reset();

    mouse_hover_track((xcb_window_t) 11);
    s_reply_present = true;
    s_reply_same_screen = 0;
    s_reply_root_x = 10;
    s_reply_root_y = 20;

    /* Sleep past the 100ms poll interval so the tick is actually due */
    until_due.tv_sec = 0;
    until_due.tv_nsec = 105L * 1000L * 1000L;
    nanosleep(&until_due, NULL);

    mouse_hover_poll_tick(s_fake_connection, s_fake_stages);

    TAP_EQ_INT(s_call_query_pointer, 1,
            "a due tick queries the pointer exactly once");
    TAP_EQ_INT(s_call_query_pointer_reply, 1,
            "a due tick fetches the reply exactly once");
    TAP_EQ_INT(s_call_resize_cursor_update, 0,
            "same_screen false skips the resize-cursor update");

    mouse_hover_poll_clear((xcb_window_t) 11);
}


/* A due tick with same_screen true updates the cursor with the
 * reply's own root coordinates, and reschedules the next poll */
static void s_test_tick_due_updates_cursor(void)
{
    struct timespec until_due;
    int remaining;

    s_reset();

    mouse_hover_track((xcb_window_t) 21);
    s_reply_present = true;
    s_reply_same_screen = 1;
    s_reply_root_x = 123;
    s_reply_root_y = 456;

    until_due.tv_sec = 0;
    until_due.tv_nsec = 105L * 1000L * 1000L;
    nanosleep(&until_due, NULL);

    mouse_hover_poll_tick(s_fake_connection, s_fake_stages);

    TAP_EQ_INT(s_call_resize_cursor_update, 1,
            "same_screen true runs the resize-cursor update once");
    TAP_EQ_INT((int) s_last_window, 21,
            "the update is applied to the tracked window itself");
    TAP_EQ_INT(s_last_root_x, 123,
            "the update receives the reply's own root_x");
    TAP_EQ_INT(s_last_root_y, 456,
            "the update receives the reply's own root_y");

    remaining = mouse_hover_poll_ms_remaining();
    TAP_OK(remaining >= 0 && remaining <= 100,
            "a due tick reschedules the next poll another ~100ms out");

    mouse_hover_poll_clear((xcb_window_t) 21);
}


/* A due tick with no reply at all (query failed) still reschedules,
 * without touching the resize cursor */
static void s_test_tick_due_no_reply(void)
{
    struct timespec until_due;
    int remaining;

    s_reset();

    mouse_hover_track((xcb_window_t) 33);
    s_reply_present = false;

    until_due.tv_sec = 0;
    until_due.tv_nsec = 105L * 1000L * 1000L;
    nanosleep(&until_due, NULL);

    mouse_hover_poll_tick(s_fake_connection, s_fake_stages);

    TAP_EQ_INT(s_call_resize_cursor_update, 0,
            "no reply at all never runs the resize-cursor update");

    remaining = mouse_hover_poll_ms_remaining();
    TAP_OK(remaining >= 0 && remaining <= 100,
            "the next poll is still rescheduled even without a reply");

    mouse_hover_poll_clear((xcb_window_t) 33);
}


/* mouse_handle_motion_hover is a no-op for a NULL event */
static void s_test_motion_null_event(void)
{
    s_reset();

    mouse_handle_motion_hover(s_fake_connection, s_fake_stages, NULL);

    TAP_EQ_INT(s_call_resize_cursor_update, 0,
            "a NULL motion event never reaches the resize-cursor update");
}


/* mouse_handle_motion_hover forwards the event's own window and root
 * coordinates straight through to the resize-cursor update */
static void s_test_motion_forwards_event(void)
{
    xcb_motion_notify_event_t event;

    s_reset();

    memset(&event, 0, sizeof(event));
    event.event = (xcb_window_t) 77;
    event.root_x = 200;
    event.root_y = 300;

    mouse_handle_motion_hover(s_fake_connection, s_fake_stages, &event);

    TAP_EQ_INT(s_call_resize_cursor_update, 1,
            "a real motion event runs the resize-cursor update exactly once");
    TAP_EQ_INT((int) s_last_window, 77,
            "the update receives the event's own window");
    TAP_EQ_INT(s_last_root_x, 200,
            "the update receives the event's own root_x");
    TAP_EQ_INT(s_last_root_y, 300,
            "the update receives the event's own root_y");
}


int main(void)
{
    TAP_PLAN(25);

    s_test_track_none();
    s_test_track_schedules_poll();
    s_test_clear_mismatch_is_noop();
    s_test_clear_none_is_noop();
    s_test_clear_match_stops_tracking();
    s_test_tick_null_args();
    s_test_tick_nothing_tracked();
    s_test_tick_not_due_yet();
    s_test_tick_due_other_screen();
    s_test_tick_due_updates_cursor();
    s_test_tick_due_no_reply();
    s_test_motion_null_event();
    s_test_motion_forwards_event();

    return TAP_DONE();
}
