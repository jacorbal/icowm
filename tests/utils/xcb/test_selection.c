/**
 * @file tests/utils/xcb/test_selection.c
 *
 * @brief Test battery for ICCCM manager-selection acquisition
 *
 * Exercises 'util_xcb_acquire_manager_selection' (utils/xcb/
 * selection.c) linked for real, together with the genuine
 * 'xcb_reply_log_error' (utils/xcb/reply.c) it calls on the failure
 * path.  Every raw XCB entry point it calls ('xcb_set_selection_owner',
 * 'xcb_get_selection_owner', 'xcb_get_selection_owner_reply',
 * 'xcb_send_event') is a controllable, call-recording stand-in below,
 * so no '-lxcb' link is required and both the "ownership granted" and
 * "another manager already owns it" branches can be driven and
 * checked without a real X server.  'logger_msg' behind
 * 'xcb_reply_log_error''s own 'LOGGER_WARNING' call is stubbed the
 * same way 'tests/systray/test_layout.c' already stubs it.
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
#include <stdlib.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <utils/xcb/selection.h>


/* Controllable stand-in state */

static int s_set_owner_calls = 0;
static xcb_window_t s_set_owner_last_window = XCB_WINDOW_NONE;
static xcb_atom_t s_set_owner_last_selection = XCB_ATOM_NONE;

static int s_get_owner_calls = 0;

static xcb_window_t s_owner_reply_owner = XCB_WINDOW_NONE;
static bool s_owner_reply_is_null = false;
static bool s_owner_reply_with_protocol_error = false;

static int s_send_event_calls = 0;
static xcb_window_t s_send_event_last_dest = XCB_WINDOW_NONE;
static uint32_t s_send_event_last_mask = 0u;
static xcb_client_message_event_t s_send_event_last_message;

static int s_logger_warning_calls = 0;


/* Raw XCB stand-ins (not linking libxcb at all) */

xcb_void_cookie_t xcb_set_selection_owner(xcb_connection_t *connection,
        xcb_window_t owner, xcb_atom_t selection, xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) time;
    s_set_owner_calls++;
    s_set_owner_last_window = owner;
    s_set_owner_last_selection = selection;
    return cookie;
}

xcb_get_selection_owner_cookie_t xcb_get_selection_owner(
        xcb_connection_t *connection, xcb_atom_t selection)
{
    xcb_get_selection_owner_cookie_t cookie = { 0u };

    (void) connection;
    (void) selection;
    s_get_owner_calls++;
    return cookie;
}

xcb_get_selection_owner_reply_t *xcb_get_selection_owner_reply(
        xcb_connection_t *connection,
        xcb_get_selection_owner_cookie_t cookie,
        xcb_generic_error_t **e)
{
    xcb_get_selection_owner_reply_t *reply;

    (void) connection;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
        if (s_owner_reply_with_protocol_error) {
            *e = calloc(1, sizeof(**e));
        }
    }

    if (s_owner_reply_is_null) {
        return NULL;
    }

    reply = malloc(sizeof(*reply));
    reply->owner = s_owner_reply_owner;
    return reply;
}

xcb_void_cookie_t xcb_send_event(xcb_connection_t *connection,
        uint8_t propagate, xcb_window_t destination, uint32_t mask,
        const char *event)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) propagate;
    s_send_event_calls++;
    s_send_event_last_dest = destination;
    s_send_event_last_mask = mask;
    s_send_event_last_message =
        *((const xcb_client_message_event_t *) (const void *) event);
    return cookie;
}


/* Link-only stand-in for 'logger_msg', behind 'xcb_reply_log_error''s
 * own 'LOGGER_WARNING' call on the "not granted" path */
int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    s_logger_warning_calls++;
    return 0;
}


static void s_reset(void)
{
    s_set_owner_calls = 0;
    s_set_owner_last_window = XCB_WINDOW_NONE;
    s_set_owner_last_selection = XCB_ATOM_NONE;
    s_get_owner_calls = 0;
    s_owner_reply_owner = XCB_WINDOW_NONE;
    s_owner_reply_is_null = false;
    s_owner_reply_with_protocol_error = false;
    s_send_event_calls = 0;
    s_send_event_last_dest = XCB_WINDOW_NONE;
    s_send_event_last_mask = 0u;
    s_logger_warning_calls = 0;
}


/* Ownership actually granted: the selection is set, the ownership is
 * verified against the same window, and the MANAGER client message is
 * broadcast on 'root' with every field the ICCCM convention expects */
static void s_test_ownership_granted_broadcasts_manager_message(void)
{
    bool result;

    s_reset();
    s_owner_reply_owner = (xcb_window_t) 77u;

    result = util_xcb_acquire_manager_selection((xcb_connection_t *) 1,
            (xcb_window_t) 77u, (xcb_atom_t) 500u, (xcb_atom_t) 600u,
            (xcb_window_t) 1u);

    TAP_OK(result, "ownership actually granted reports success");
    TAP_EQ_INT(s_set_owner_calls, 1,
            "the selection owner is set exactly once");
    TAP_EQ_INT((long) s_set_owner_last_window, 77,
            "on the window asking to own it");
    TAP_EQ_INT((long) s_set_owner_last_selection, 500,
            "for the requested selection atom");
    TAP_EQ_INT(s_get_owner_calls, 1,
            "ownership is verified with exactly one round trip");
    TAP_EQ_INT(s_send_event_calls, 1,
            "exactly one MANAGER client message is broadcast");
    TAP_EQ_INT((long) s_send_event_last_dest, 1,
            "sent to the given root window");
    TAP_EQ_INT((long) s_send_event_last_mask,
            (long) XCB_EVENT_MASK_STRUCTURE_NOTIFY,
            "with the structure-notify event mask");
    TAP_EQ_INT((long) s_send_event_last_message.response_type,
            (long) XCB_CLIENT_MESSAGE,
            "the broadcast message is a ClientMessage");
    TAP_EQ_INT((long) s_send_event_last_message.format, 32,
            "in 32-bit format");
    TAP_EQ_INT((long) s_send_event_last_message.window, 1,
            "addressed to the root window itself");
    TAP_EQ_INT((long) s_send_event_last_message.type, 600,
            "typed as the given MANAGER atom");
    TAP_EQ_INT((long) s_send_event_last_message.data.data32[1], 500,
            "naming the just-acquired selection atom in data32[1]");
    TAP_EQ_INT((long) s_send_event_last_message.data.data32[2], 77,
            "and the new owner window in data32[2]");
    TAP_EQ_INT(s_logger_warning_calls, 0,
            "nothing is logged on the success path");
}


/* Another manager already owns the selection: the server's reply
 * names a different window as owner, so ownership is refused, logged,
 * and no MANAGER message is ever broadcast */
static void s_test_ownership_refused_when_owner_differs(void)
{
    bool result;

    s_reset();
    s_owner_reply_owner = (xcb_window_t) 999u;

    result = util_xcb_acquire_manager_selection((xcb_connection_t *) 1,
            (xcb_window_t) 77u, (xcb_atom_t) 500u, (xcb_atom_t) 600u,
            (xcb_window_t) 1u);

    TAP_OK(!result,
            "ownership is refused when the reply names a different"
            " owner");
    TAP_EQ_INT(s_send_event_calls, 0,
            "no MANAGER message is broadcast when ownership was not"
            " granted");
    /* 'xcb_reply_log_error' only ever logs when handed a non-NULL
     * 'xcb_generic_error_t'; this fixture's
     * 'xcb_get_selection_owner_reply' stand-in always answers with
     * a NULL error (a mismatched owner is not itself a protocol
     * error, just an unwelcome answer), so nothing is logged here
     * either, exactly as the real function would behave given the
     * same NULL error */
    TAP_EQ_INT(s_logger_warning_calls, 0,
            "a mismatched owner with no protocol error logs nothing");
}


/* A NULL reply (the round trip itself failed) is treated exactly the
 * same as an answer naming another owner: refused, logged, no
 * broadcast */
static void s_test_null_reply_is_refused(void)
{
    bool result;

    s_reset();
    s_owner_reply_is_null = true;

    result = util_xcb_acquire_manager_selection((xcb_connection_t *) 1,
            (xcb_window_t) 77u, (xcb_atom_t) 500u, (xcb_atom_t) 600u,
            (xcb_window_t) 1u);

    TAP_OK(!result, "a NULL owner reply is treated as ownership not"
            " granted");
    TAP_EQ_INT(s_send_event_calls, 0,
            "no MANAGER message is broadcast for a NULL reply");
    TAP_EQ_INT(s_logger_warning_calls, 0,
            "a NULL reply with no protocol error logs nothing, same"
            " as an explicit owner mismatch");
}


/* A genuine protocol error accompanying the owner reply (the server
 * itself rejected the request) is logged through the real
 * 'xcb_reply_log_error', which frees it; this is the one scenario
 * where 'logger_msg' actually fires */
static void s_test_protocol_error_is_logged(void)
{
    bool result;

    s_reset();
    s_owner_reply_owner = (xcb_window_t) 999u;
    s_owner_reply_with_protocol_error = true;

    result = util_xcb_acquire_manager_selection((xcb_connection_t *) 1,
            (xcb_window_t) 77u, (xcb_atom_t) 500u, (xcb_atom_t) 600u,
            (xcb_window_t) 1u);

    TAP_OK(!result,
            "ownership is still refused when a protocol error"
            " accompanies the reply");
    TAP_EQ_INT(s_logger_warning_calls, 1,
            "a genuine protocol error is logged exactly once");
}


int main(void)
{
    TAP_PLAN(23);

    s_test_ownership_granted_broadcasts_manager_message();
    s_test_ownership_refused_when_owner_differs();
    s_test_null_reply_is_refused();
    s_test_protocol_error_is_logged();

    return TAP_DONE();
}
