/**
 * @file tests/input/mouse/drag/test_background.c
 *
 * @brief Test battery for the background-pan drag module
 *        (input/mouse/drag/background.c)
 *
 * 'drag_background_start'/'_update'/'_end'/'_is_active' read and
 * write the module's own singleton state, whose storage is defined
 * right here in background.c itself; this file links the real
 * background.c, so it never redefines that state.
 * 'lookup_current_desktop' and 'stage_desktop_get' are
 * test-controlled stand-ins, answering whichever desktop pointer (or
 * 'NULL') the currently running scenario registered beforehand, so
 * every branch runs without a live desktop list ever needing to
 * exist.  'scmd_stage_viewport_set' (cmds/stage.c) is a recording
 * stand-in: its own clamping and client-translation logic is already
 * fully covered by tests/cmds/test_stage_viewport_pan.c, so this
 * file only asserts on the exact 'x'/'y' background.c hands it,
 * confirming the pan-follows-pointer sign convention.
 * 'lookup_find_client' and 'enact_client_unfocus' are recording
 * stand-ins for the same reason.  The raw XCB pointer-grab requests,
 * 'mouse_cursor_move', and 'mouse_plain_cursor' are stubbed directly,
 * the same convention tests/input/mouse/drag/test_drag.c already
 * uses, since no live X connection is used.  'scmd_stage_viewport_
 * has_room' is a controllable stand-in deciding which of those two
 * cursors 'xcb_grab_pointer' is expected to have recorded.
 * 'logger_msg' is a link-only stand-in, reached only on a grab
 * failure this file asserts nothing about beyond the resulting
 * state.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <cmds/stage.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <stage.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/cursor.h>
#include <input/mouse/drag/background.h>


/** Controllable stand-in result for the next xcb_grab_pointer_reply
 *  call */
static bool s_stub_grab_reply_null;
static uint8_t s_stub_grab_status;

/** Recorded calls to every collaborator stand-in this file defines */
static int s_grab_pointer_calls;
static int s_ungrab_pointer_calls;

/** Recorded cursor argument from the last xcb_grab_pointer call */
static xcb_cursor_t s_grab_last_cursor;

/** Controllable stand-in result for the next
 *  stage_viewport_has_room call */
static bool s_stub_viewport_has_room;

/** Controllable stand-in result for the next lookup_current_desktop
 *  call */
static desktop_td *s_stub_current_desktop;

/** Controllable stand-in result for the next stage_desktop_get
 *  call */
static desktop_td *s_stub_desktop_get_result;

/** Recorded arguments from the last scmd_stage_viewport_set call */
static int s_viewport_set_calls;
static int32_t s_viewport_set_last_x;
static int32_t s_viewport_set_last_y;

/** Controllable stand-in result for the next lookup_find_client call */
static int s_lookup_find_client_calls;
static xcb_window_t s_lookup_find_client_last_window;
static client_td *s_stub_lookup_find_client_result;

/** Recorded calls to enact_client_unfocus */
static int s_unfocus_calls;
static client_td *s_unfocus_last_client;


/**
 * @brief Controllable stand-in for the raw @a xcb_grab_pointer request
 * @note Complexity: @e O(1)
 */
xcb_grab_pointer_cookie_t xcb_grab_pointer(xcb_connection_t *c,
        uint8_t owner_events, xcb_window_t grab_window,
        uint16_t event_mask, uint8_t pointer_mode,
        uint8_t keyboard_mode, xcb_window_t confine_to,
        xcb_cursor_t cursor, xcb_timestamp_t time)
{
    xcb_grab_pointer_cookie_t cookie;

    (void) c;
    (void) owner_events;
    (void) grab_window;
    (void) event_mask;
    (void) pointer_mode;
    (void) keyboard_mode;
    (void) confine_to;
    (void) time;

    memset(&cookie, 0, sizeof(cookie));
    s_grab_pointer_calls++;
    s_grab_last_cursor = cursor;

    return cookie;
}


/**
 * @brief Controllable stand-in for the raw @a xcb_grab_pointer_reply
 *        request
 * @note Complexity: @e O(1)
 */
xcb_grab_pointer_reply_t *xcb_grab_pointer_reply(xcb_connection_t *c,
        xcb_grab_pointer_cookie_t cookie, xcb_generic_error_t **e)
{
    xcb_grab_pointer_reply_t *reply;

    (void) c;
    (void) cookie;

    if (e != NULL) {
        *e = NULL;
    }

    if (s_stub_grab_reply_null) {
        return NULL;
    }

    reply = malloc(sizeof(*reply));
    if (reply == NULL) {
        return NULL;
    }
    memset(reply, 0, sizeof(*reply));
    reply->status = s_stub_grab_status;

    return reply;
}


/**
 * @brief Recording stand-in for the raw @a xcb_ungrab_pointer request
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ungrab_pointer(xcb_connection_t *c,
        xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) time;

    memset(&cookie, 0, sizeof(cookie));
    s_ungrab_pointer_calls++;

    return cookie;
}


/**
 * @brief Controllable stand-in for @a mouse_cursor_move
 * @note Complexity: @e O(1)
 */
xcb_cursor_t mouse_cursor_move(void)
{
    return 1u;
}


/**
 * @brief Controllable stand-in for @a mouse_plain_cursor
 * @note Complexity: @e O(1)
 */
xcb_cursor_t mouse_plain_cursor(void)
{
    return 2u;
}


/**
 * @brief Controllable stand-in for @a stage_viewport_has_room
 * @note Complexity: @e O(1)
 */
bool stage_viewport_has_room(const stage_td *stage)
{
    (void) stage;

    return s_stub_viewport_has_room;
}


/**
 * @brief Controllable stand-in for @a lookup_current_desktop
 * @note Complexity: @e O(1)
 */
desktop_td *lookup_current_desktop(stage_td *stage)
{
    (void) stage;

    return s_stub_current_desktop;
}


/**
 * @brief Controllable stand-in for @a stage_desktop_get
 * @note Complexity: @e O(1)
 */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;

    return s_stub_desktop_get_result;
}


/**
 * @brief Recording stand-in for @a scmd_stage_viewport_set
 * @note Complexity: @e O(1)
 */
void scmd_stage_viewport_set(stage_td *stage, int32_t x, int32_t y)
{
    (void) stage;

    s_viewport_set_calls++;
    s_viewport_set_last_x = x;
    s_viewport_set_last_y = y;
}


/**
 * @brief Controllable/recording stand-in for @a lookup_find_client
 * @note Complexity: @e O(1)
 */
client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **out_stage, desktop_td **out_desktop)
{
    (void) stages;

    s_lookup_find_client_calls++;
    s_lookup_find_client_last_window = window;

    if (out_stage != NULL) {
        *out_stage = NULL;
    }
    if (out_desktop != NULL) {
        *out_desktop = NULL;
    }

    return s_stub_lookup_find_client_result;
}


/**
 * @brief Recording stand-in for @a enact_client_unfocus
 * @note Complexity: @e O(1)
 */
void enact_client_unfocus(client_td *client)
{
    s_unfocus_calls++;
    s_unfocus_last_client = client;
}


/**
 * @brief Link-only stand-in for @a logger_msg, reached only on a
 *        grab failure this file asserts nothing about beyond state
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;

    return 0;
}


static void s_reset(void)
{
    s_stub_grab_reply_null = false;
    s_stub_grab_status = XCB_GRAB_STATUS_SUCCESS;
    s_grab_pointer_calls = 0;
    s_ungrab_pointer_calls = 0;
    s_grab_last_cursor = 0u;
    s_stub_viewport_has_room = true;
    s_stub_current_desktop = NULL;
    s_stub_desktop_get_result = NULL;
    s_viewport_set_calls = 0;
    s_viewport_set_last_x = 0;
    s_viewport_set_last_y = 0;
    s_lookup_find_client_calls = 0;
    s_lookup_find_client_last_window = 0;
    s_stub_lookup_find_client_result = NULL;
    s_unfocus_calls = 0;
    s_unfocus_last_client = NULL;
}


/* A null connection is a no-op: no grab is attempted and the module
 * never becomes active */
static void s_test_start_null_connection_is_noop(void)
{
    desktop_td desktop;
    stage_td stage;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    s_stub_current_desktop = &desktop;

    drag_background_start(NULL, &stage, 1u, 0,
            (struct position_s) { 0, 0 });

    TAP_EQ_INT(s_grab_pointer_calls, 0,
            "a null connection: no pointer grab is attempted");
    TAP_OK(!drag_background_is_active(),
            "a null connection: the module never becomes active");
}


/* A null stage is a no-op, the same way */
static void s_test_start_null_stage_is_noop(void)
{
    s_reset();

    drag_background_start((xcb_connection_t *) 1, NULL, 1u, 0,
            (struct position_s) { 0, 0 });

    TAP_EQ_INT(s_grab_pointer_calls, 0,
            "a null stage: no pointer grab is attempted");
    TAP_OK(!drag_background_is_active(),
            "a null stage: the module never becomes active");
}


/* A stage with no resolvable current desktop is a no-op, before
 * any pointer grab is even attempted */
static void s_test_start_no_desktop_is_noop(void)
{
    stage_td stage;

    s_reset();
    memset(&stage, 0, sizeof(stage));
    s_stub_current_desktop = NULL;

    drag_background_start((xcb_connection_t *) 1, &stage, 1u, 0,
            (struct position_s) { 0, 0 });

    TAP_EQ_INT(s_grab_pointer_calls, 0,
            "no current desktop: no pointer grab is attempted");
    TAP_OK(!drag_background_is_active(),
            "no current desktop: the module never becomes active");
}


/* A failed pointer grab (null reply) leaves the module inactive */
static void s_test_start_grab_reply_null_is_noop(void)
{
    desktop_td desktop;
    stage_td stage;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    s_stub_current_desktop = &desktop;
    s_stub_grab_reply_null = true;

    drag_background_start((xcb_connection_t *) 1, &stage, 1u, 0,
            (struct position_s) { 0, 0 });

    TAP_OK(!drag_background_is_active(),
            "a null grab reply: the module never becomes active");
}


/* A failed pointer grab (non-success status) leaves the module
 * inactive too */
static void s_test_start_grab_status_failure_is_noop(void)
{
    desktop_td desktop;
    stage_td stage;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    s_stub_current_desktop = &desktop;
    s_stub_grab_status = XCB_GRAB_STATUS_ALREADY_GRABBED;

    drag_background_start((xcb_connection_t *) 1, &stage, 1u, 0,
            (struct position_s) { 0, 0 });

    TAP_OK(!drag_background_is_active(),
            "a failed grab status: the module never becomes active");
}


/* A successful grab activates the module and captures the starting
 * viewport origin, later used unchanged by drag_background_update */
static void s_test_start_success_activates_module(void)
{
    desktop_td desktop;
    stage_td stage;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    desktop.viewport_origin.x = 200;
    desktop.viewport_origin.y = 100;
    s_stub_current_desktop = &desktop;

    drag_background_start((xcb_connection_t *) 1, &stage, 1u, 0,
            (struct position_s) { 50, 60 });

    TAP_EQ_INT(s_grab_pointer_calls, 1,
            "a resolvable desktop: exactly one pointer grab attempt");
    TAP_OK(drag_background_is_active(),
            "a successful grab: the module becomes active");

    /* No pointer movement yet: the viewport origin the drag started
     * at is handed back unchanged */
    drag_background_update((xcb_connection_t *) 1,
            (struct position_s) { 50, 60 });

    TAP_EQ_INT(s_viewport_set_last_x, 200,
            "the captured origin.x survives with no pointer movement");
    TAP_EQ_INT(s_viewport_set_last_y, 100,
            "the captured origin.y survives with no pointer movement");

    /* Left active otherwise, leaking into whichever test runs next */
    drag_background_end((xcb_connection_t *) 1, NULL,
            (struct position_s) { 50, 60 });
}


/* A viewport with room to pan grabs the pointer with the move
 * cursor, promising a drag that can actually go somewhere */
static void s_test_start_with_room_grabs_move_cursor(void)
{
    desktop_td desktop;
    stage_td stage;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    s_stub_current_desktop = &desktop;
    s_stub_viewport_has_room = true;

    drag_background_start((xcb_connection_t *) 1, &stage, 1u, 0,
            (struct position_s) { 0, 0 });

    TAP_EQ_INT((int) s_grab_last_cursor, (int) mouse_cursor_move(),
            "a viewport with room: the pointer grab uses the move"
            " cursor");

    drag_background_end((xcb_connection_t *) 1, NULL,
            (struct position_s) { 0, 0 });
}


/* A plain {1,1} desktop, with no room to pan at all, grabs the
 * pointer with the plain cursor instead, so the drag never promises
 * a pan it could never actually deliver */
static void s_test_start_without_room_grabs_plain_cursor(void)
{
    desktop_td desktop;
    stage_td stage;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    s_stub_current_desktop = &desktop;
    s_stub_viewport_has_room = false;

    drag_background_start((xcb_connection_t *) 1, &stage, 1u, 0,
            (struct position_s) { 0, 0 });

    TAP_EQ_INT((int) s_grab_last_cursor, (int) mouse_plain_cursor(),
            "a plain {1,1} desktop: the pointer grab uses the plain"
            " cursor");
    TAP_OK(drag_background_is_active(),
            "a plain {1,1} desktop: the module still activates, so a"
            " release still unfocuses like a plain click");

    drag_background_end((xcb_connection_t *) 1, NULL,
            (struct position_s) { 0, 0 });
}


/* drag_background_update is a no-op while no background-pan drag is
 * active */
static void s_test_update_inactive_is_noop(void)
{
    s_reset();

    drag_background_update((xcb_connection_t *) 1,
            (struct position_s) { 10, 10 });

    TAP_EQ_INT(s_viewport_set_calls, 0,
            "no active drag: scmd_stage_viewport_set is never"
            " called");
}


/* The desktop's content visually follows the pointer 1:1: the new
 * origin is the starting origin minus the pointer's own delta */
static void s_test_update_pans_with_inverted_delta(void)
{
    desktop_td desktop;
    stage_td stage;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    desktop.viewport_origin.x = 300;
    desktop.viewport_origin.y = 150;
    s_stub_current_desktop = &desktop;

    drag_background_start((xcb_connection_t *) 1, &stage, 1u, 0,
            (struct position_s) { 500, 400 });

    /* Pointer moved +40 right, -25 up since the press */
    drag_background_update((xcb_connection_t *) 1,
            (struct position_s) { 540, 375 });

    TAP_EQ_INT(s_viewport_set_calls, 1,
            "a moved pointer: scmd_stage_viewport_set is called"
            " once");
    TAP_EQ_INT(s_viewport_set_last_x, 260,
            "origin.x moves opposite the pointer's own delta"
            " (300 - 40)");
    TAP_EQ_INT(s_viewport_set_last_y, 175,
            "origin.y moves opposite the pointer's own delta"
            " (150 - (-25))");

    /* Left active otherwise, leaking into whichever test runs next */
    drag_background_end((xcb_connection_t *) 1, NULL,
            (struct position_s) { 540, 375 });
}


/* drag_background_end is a no-op while no background-pan drag is
 * active: no pointer ungrab, no unfocus */
static void s_test_end_inactive_is_noop(void)
{
    s_reset();

    drag_background_end((xcb_connection_t *) 1, NULL,
            (struct position_s) { 0, 0 });

    TAP_EQ_INT(s_ungrab_pointer_calls, 0,
            "no active drag: xcb_ungrab_pointer is never called");
    TAP_EQ_INT(s_unfocus_calls, 0,
            "no active drag: enact_client_unfocus is never called");
}


/* A release with no real movement past the click-vs-drag threshold
 * is treated as the plain background click it always was: the active
 * client is unfocused and the desktop's focus bookkeeping is cleared */
static void s_test_end_below_threshold_unfocuses_active_client(void)
{
    desktop_td desktop;
    stage_td stage;
    client_td active_client;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    memset(&active_client, 0, sizeof(active_client));
    desktop.client_active_id = 42u;
    s_stub_current_desktop = &desktop;
    s_stub_desktop_get_result = &desktop;
    s_stub_lookup_find_client_result = &active_client;

    drag_background_start((xcb_connection_t *) 1, &stage, 1u, 0,
            (struct position_s) { 100, 100 });

    /* Released one pixel away: well under the 4px x 4px threshold */
    drag_background_end((xcb_connection_t *) 1, NULL,
            (struct position_s) { 101, 100 });

    TAP_EQ_INT(s_lookup_find_client_calls, 1,
            "a below-threshold release: the active client is looked"
            " up");
    TAP_EQ_INT((int) s_lookup_find_client_last_window, 42,
            "looked up by the desktop's own client_active_id");
    TAP_EQ_INT(s_unfocus_calls, 1,
            "a below-threshold release: the active client is"
            " unfocused once");
    TAP_OK(s_unfocus_last_client == &active_client,
            "the exact resolved client reaches enact_client_unfocus");
    TAP_EQ_INT((int) desktop.client_active_id, 0,
            "the desktop's client_active_id is cleared");
    TAP_OK(desktop.is_focus_dirty,
            "the desktop's focus bookkeeping is marked dirty");
    TAP_OK(desktop.is_outdated,
            "the desktop itself is marked outdated");
    TAP_OK(stage.is_outdated,
            "the stage itself is marked outdated");
    TAP_EQ_INT(s_ungrab_pointer_calls, 1,
            "the pointer grab is released exactly once");
    TAP_OK(!drag_background_is_active(),
            "the module is inactive again once the drag ends");
}


/* A below-threshold release with no active client to unfocus still
 * ends the drag cleanly, without ever calling lookup_find_client */
static void s_test_end_below_threshold_no_active_client(void)
{
    desktop_td desktop;
    stage_td stage;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    desktop.client_active_id = 0;
    s_stub_current_desktop = &desktop;

    drag_background_start((xcb_connection_t *) 1, &stage, 1u, 0,
            (struct position_s) { 100, 100 });

    drag_background_end((xcb_connection_t *) 1, NULL,
            (struct position_s) { 100, 100 });

    TAP_EQ_INT(s_lookup_find_client_calls, 0,
            "no active client id: lookup_find_client is skipped");
    TAP_EQ_INT(s_unfocus_calls, 0,
            "no active client id: enact_client_unfocus is skipped");
    TAP_OK(!drag_background_is_active(),
            "the module still ends cleanly");
}


/* A release that moved past the click-vs-drag threshold is a real
 * pan, not a click: the active client is left focused */
static void s_test_end_above_threshold_skips_unfocus(void)
{
    desktop_td desktop;
    stage_td stage;
    client_td active_client;

    s_reset();
    memset(&desktop, 0, sizeof(desktop));
    memset(&stage, 0, sizeof(stage));
    memset(&active_client, 0, sizeof(active_client));
    desktop.client_active_id = 7u;
    s_stub_current_desktop = &desktop;
    s_stub_desktop_get_result = &desktop;
    s_stub_lookup_find_client_result = &active_client;

    drag_background_start((xcb_connection_t *) 1, &stage, 1u, 0,
            (struct position_s) { 0, 0 });

    /* Released 20px away on each axis: well past the threshold */
    drag_background_end((xcb_connection_t *) 1, NULL,
            (struct position_s) { 20, 20 });

    TAP_EQ_INT(s_unfocus_calls, 0,
            "an above-threshold release: enact_client_unfocus is"
            " never called");
    TAP_EQ_INT((int) desktop.client_active_id, 7,
            "the desktop's client_active_id is left untouched");
    TAP_EQ_INT(s_ungrab_pointer_calls, 1,
            "the pointer grab is still released exactly once");
    TAP_OK(!drag_background_is_active(),
            "the module is still inactive again once the drag ends");
}


int main(void)
{
    TAP_PLAN(38);

    s_test_start_null_connection_is_noop();
    s_test_start_null_stage_is_noop();
    s_test_start_no_desktop_is_noop();
    s_test_start_grab_reply_null_is_noop();
    s_test_start_grab_status_failure_is_noop();
    s_test_start_success_activates_module();
    s_test_start_with_room_grabs_move_cursor();
    s_test_start_without_room_grabs_plain_cursor();
    s_test_update_inactive_is_noop();
    s_test_update_pans_with_inverted_delta();
    s_test_end_inactive_is_noop();
    s_test_end_below_threshold_unfocuses_active_client();
    s_test_end_below_threshold_no_active_client();
    s_test_end_above_threshold_skips_unfocus();

    return TAP_DONE();
}
