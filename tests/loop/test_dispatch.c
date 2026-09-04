/**
 * @file tests/loop/test_dispatch.c
 *
 * @brief Test battery for the main loop's per-event-type routing table
 *        (loop/dispatch.c)
 *
 * loop_dispatch_event's own logic is exactly a null-argument guard
 * clause, an extension-event probe compared against two run-time base
 * event codes, a table lookup by masked response type, and a call
 * through whatever function pointer that lookup produced (or a log
 * line when it found none); none of that requires a live X connection,
 * only a real, syntactically valid xcb_generic_event_t whose fields
 * this file controls directly.  What it hands off to, every
 * s_loop_dispatch_* adapter's own one-line forward into handler.h,
 * loop/event.h, or input/mouse/event.h, is not dispatch.c's own logic
 * at all, so every one of those targets is a link-only stand-in below
 * that records which adapter actually reached it and with which event,
 * rather than a real handler that would need a live surfaces list, a
 * live wm singleton, or a live X server round trip to do anything
 * useful; s_loop_dispatch_extension and the dispatch table itself are
 * both static to dispatch.c, so this file can only reach them through
 * loop_dispatch_event, exactly like every other caller would.
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
#include <stddef.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/sync.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <loop/context.h>
#include <loop/dispatch.h>
#include <loop/event.h>


/** Link-only stand-in for logger_msg (logger.c): a silent no-op,
 *  matching the real logger's own behavior whenever logger_start has
 *  never run (its first check is 'logger == NULL'), the same
 *  reasoning tests/wm/test_lifecycle.c already documents for never
 *  calling logger_start at all here either */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/** Recording of the single most recent call across every stand-in
 *  below, since exactly one adapter is ever reached per
 *  loop_dispatch_event call */
static const char *s_last_called;
static loop_ctx_td *s_last_ctx;
static xcb_generic_event_t *s_last_event;
static int s_call_count;


static void s_reset(void)
{
    s_last_called = NULL;
    s_last_ctx = NULL;
    s_last_event = NULL;
    s_call_count = 0;
}


/** Link-only stand-in for xcb_connection_get (utils/xcb/connection.c):
 *  s_loop_dispatch_enter_notify is the only table adapter that calls
 *  it directly rather than merely forwarding ctx, so a harmless
 *  sentinel is enough for that one call site */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/** Link-only stand-in for handler_protocol_error (handler.c) */
void handler_protocol_error(const xcb_generic_event_t *event)
{
    s_last_called = "handler_protocol_error";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for mouse_handle_enter (input/mouse/event.c) */
void mouse_handle_enter(xcb_connection_t *connection, list_td *surfaces,
        xcb_enter_notify_event_t *event, const config_td *config)
{
    (void) connection;
    (void) surfaces;
    (void) config;
    s_last_called = "mouse_handle_enter";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_leave_notify (handler.c) */
void handler_leave_notify(const wm_td *wm, xcb_leave_notify_event_t *event)
{
    (void) wm;
    s_last_called = "handler_leave_notify";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_focus_in (handler.c) */
void handler_focus_in(xcb_connection_t *connection, list_td *surfaces,
        xcb_focus_in_event_t *event)
{
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_focus_in";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_focus_out (handler.c) */
void handler_focus_out(const wm_td *wm, xcb_focus_out_event_t *event)
{
    (void) wm;
    s_last_called = "handler_focus_out";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_configure_notify (handler.c) */
void handler_configure_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_configure_notify_event_t *event)
{
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_configure_notify";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_configure_request (handler.c) */
void handler_configure_request(xcb_connection_t *connection,
        list_td *surfaces, xcb_configure_request_event_t *event)
{
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_configure_request";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_map_request (handler.c) */
void handler_map_request(const wm_td *wm, xcb_map_request_event_t *event)
{
    (void) wm;
    s_last_called = "handler_map_request";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_map_notify (handler.c) */
void handler_map_notify(xcb_connection_t *connection, list_td *surfaces,
        xcb_map_notify_event_t *event)
{
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_map_notify";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_unmap_notify (handler.c) */
void handler_unmap_notify(xcb_connection_t *connection, list_td *surfaces,
        xcb_unmap_notify_event_t *event)
{
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_unmap_notify";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_destroy_notify (handler.c) */
void handler_destroy_notify(wm_td *wm, xcb_connection_t *connection,
        list_td *surfaces, xcb_destroy_notify_event_t *event)
{
    (void) wm;
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_destroy_notify";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_property_notify (handler.c) */
void handler_property_notify(const wm_td *wm, xcb_connection_t *connection,
        list_td *surfaces, xcb_property_notify_event_t *event)
{
    (void) wm;
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_property_notify";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_colormap_notify (handler.c) */
void handler_colormap_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_colormap_notify_event_t *event)
{
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_colormap_notify";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_expose (handler.c) */
void handler_expose(xcb_connection_t *connection, list_td *surfaces,
        xcb_expose_event_t *event, const config_td *config)
{
    (void) connection;
    (void) surfaces;
    (void) config;
    s_last_called = "handler_expose";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_client_message (handler.c) */
void handler_client_message(wm_td *wm, xcb_client_message_event_t *event)
{
    (void) wm;
    s_last_called = "handler_client_message";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_mapping_notify (handler.c) */
void handler_mapping_notify(xcb_key_symbols_t *keysyms, list_td *surfaces,
        xcb_mapping_notify_event_t *event, const config_td *config)
{
    (void) keysyms;
    (void) surfaces;
    (void) config;
    s_last_called = "handler_mapping_notify";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_gravity_notify (handler.c) */
void handler_gravity_notify(xcb_connection_t *connection, list_td *surfaces,
        xcb_gravity_notify_event_t *event)
{
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_gravity_notify";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_circulate_notify (handler.c) */
void handler_circulate_notify(xcb_connection_t *connection,
        list_td *surfaces, xcb_circulate_notify_event_t *event)
{
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_circulate_notify";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_circulate_request (handler.c) */
void handler_circulate_request(xcb_connection_t *connection,
        list_td *surfaces, xcb_circulate_request_event_t *event)
{
    (void) connection;
    (void) surfaces;
    s_last_called = "handler_circulate_request";
    s_last_event = (xcb_generic_event_t *) event;
    s_call_count++;
}


/** Link-only stand-in for handler_randr_event (handler.c) */
void handler_randr_event(wm_td *wm, xcb_generic_event_t *event)
{
    (void) wm;
    s_last_called = "handler_randr_event";
    s_last_event = event;
    s_call_count++;
}


/** Link-only stand-in for handler_sync_event (handler.c) */
void handler_sync_event(const wm_td *wm, xcb_generic_event_t *event)
{
    (void) wm;
    s_last_called = "handler_sync_event";
    s_last_event = event;
    s_call_count++;
}


/** Link-only stand-in for loop_event_key_press (loop/event/input.c) */
void loop_event_key_press(loop_ctx_td *ctx, xcb_generic_event_t **event)
{
    s_last_called = "loop_event_key_press";
    s_last_ctx = ctx;
    s_last_event = *event;
    s_call_count++;
}


/** Link-only stand-in for loop_event_key_release (loop/event/input.c) */
void loop_event_key_release(loop_ctx_td *ctx, xcb_generic_event_t **event)
{
    s_last_called = "loop_event_key_release";
    s_last_ctx = ctx;
    s_last_event = *event;
    s_call_count++;
}


/** Link-only stand-in for loop_event_button_press (loop/event/input.c) */
void loop_event_button_press(loop_ctx_td *ctx, xcb_generic_event_t **event)
{
    s_last_called = "loop_event_button_press";
    s_last_ctx = ctx;
    s_last_event = *event;
    s_call_count++;
}


/** Link-only stand-in for loop_event_button_release
 *  (loop/event/input.c) */
void loop_event_button_release(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    s_last_called = "loop_event_button_release";
    s_last_ctx = ctx;
    s_last_event = *event;
    s_call_count++;
}


/** Link-only stand-in for loop_event_motion_notify
 *  (loop/event/motion.c) */
void loop_event_motion_notify(loop_ctx_td *ctx, xcb_generic_event_t **event)
{
    s_last_called = "loop_event_motion_notify";
    s_last_ctx = ctx;
    s_last_event = *event;
    s_call_count++;
}


/**
 * @brief Build a loop context whose extension probing is disabled
 */
static loop_ctx_td s_make_ctx(void)
{
    loop_ctx_td ctx;

    memset(&ctx, 0, sizeof(ctx));
    return ctx;
}


/* A null ctx, a null event pointer, or a null *event are every one
 * refused outright, reaching not a single stand-in above */
static void s_test_null_guards(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t event;
    xcb_generic_event_t *event_ptr = &event;
    xcb_generic_event_t *null_event_ptr = NULL;

    memset(&event, 0, sizeof(event));
    event.response_type = XCB_KEY_PRESS;

    s_reset();
    loop_dispatch_event(NULL, &event_ptr);
    TAP_EQ_INT(s_call_count, 0,
            "loop_dispatch_event on a null ctx calls nothing");

    s_reset();
    loop_dispatch_event(&ctx, NULL);
    TAP_EQ_INT(s_call_count, 0,
            "loop_dispatch_event on a null event pointer calls"
            " nothing");

    s_reset();
    loop_dispatch_event(&ctx, &null_event_ptr);
    TAP_EQ_INT(s_call_count, 0,
            "loop_dispatch_event on a null *event calls nothing");
}


/* Response type 0 (a protocol error, not a real event type) is routed
 * to handler_protocol_error, the table's own explicit [0] entry */
static void s_test_protocol_error(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t event;
    xcb_generic_event_t *event_ptr = &event;

    memset(&event, 0, sizeof(event));
    event.response_type = 0u;

    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);

    TAP_EQ_STR(s_last_called, "handler_protocol_error",
            "response_type 0 is routed to handler_protocol_error");
    TAP_EQ_INT(s_call_count, 1,
            "exactly one adapter is reached per dispatched event");
}


/* Every input event type reaches its own loop_event_* adapter, with
 * both ctx and the very same event pointer passed through unchanged */
static void s_test_input_events_route_to_loop_event(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t event;
    xcb_generic_event_t *event_ptr;

    memset(&event, 0, sizeof(event));

    event.response_type = XCB_KEY_PRESS;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "loop_event_key_press",
            "KEY_PRESS is routed to loop_event_key_press");
    TAP_OK(s_last_ctx == &ctx,
            "loop_event_key_press receives the same ctx pointer");
    TAP_OK(s_last_event == &event,
            "loop_event_key_press receives the same event pointer");

    event.response_type = XCB_KEY_RELEASE;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "loop_event_key_release",
            "KEY_RELEASE is routed to loop_event_key_release");

    event.response_type = XCB_BUTTON_PRESS;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "loop_event_button_press",
            "BUTTON_PRESS is routed to loop_event_button_press");

    event.response_type = XCB_BUTTON_RELEASE;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "loop_event_button_release",
            "BUTTON_RELEASE is routed to loop_event_button_release");

    event.response_type = XCB_MOTION_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "loop_event_motion_notify",
            "MOTION_NOTIFY is routed to loop_event_motion_notify");
}


/* The high (synthetic-event) bit is masked off the response type
 * before the table lookup, so a synthetic KEY_PRESS routes exactly
 * like a genuine one */
static void s_test_synthetic_bit_is_masked(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t event;
    xcb_generic_event_t *event_ptr = &event;

    memset(&event, 0, sizeof(event));
    event.response_type = (uint8_t) (XCB_KEY_PRESS | 0x80u);

    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);

    TAP_EQ_STR(s_last_called, "loop_event_key_press",
            "a synthetic KEY_PRESS (top bit set) is routed exactly"
            " like a genuine one");
}


/* Every handler.h-forwarding table entry reaches its own adapter,
 * covering the remaining, less exercised, table rows */
static void s_test_handler_forwarding_events(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t event;
    xcb_generic_event_t *event_ptr;

    memset(&event, 0, sizeof(event));

    event.response_type = XCB_ENTER_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "mouse_handle_enter",
            "ENTER_NOTIFY is routed to mouse_handle_enter");

    event.response_type = XCB_LEAVE_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_leave_notify",
            "LEAVE_NOTIFY is routed to handler_leave_notify");

    event.response_type = XCB_FOCUS_IN;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_focus_in",
            "FOCUS_IN is routed to handler_focus_in");

    event.response_type = XCB_FOCUS_OUT;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_focus_out",
            "FOCUS_OUT is routed to handler_focus_out");

    event.response_type = XCB_EXPOSE;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_expose",
            "EXPOSE is routed to handler_expose");

    event.response_type = XCB_DESTROY_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_destroy_notify",
            "DESTROY_NOTIFY is routed to handler_destroy_notify");

    event.response_type = XCB_UNMAP_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_unmap_notify",
            "UNMAP_NOTIFY is routed to handler_unmap_notify");

    event.response_type = XCB_MAP_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_map_notify",
            "MAP_NOTIFY is routed to handler_map_notify");

    event.response_type = XCB_MAP_REQUEST;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_map_request",
            "MAP_REQUEST is routed to handler_map_request");

    event.response_type = XCB_CONFIGURE_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_configure_notify",
            "CONFIGURE_NOTIFY is routed to handler_configure_notify");

    event.response_type = XCB_CONFIGURE_REQUEST;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_configure_request",
            "CONFIGURE_REQUEST is routed to"
            " handler_configure_request");

    event.response_type = XCB_GRAVITY_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_gravity_notify",
            "GRAVITY_NOTIFY is routed to handler_gravity_notify");

    event.response_type = XCB_CIRCULATE_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_circulate_notify",
            "CIRCULATE_NOTIFY is routed to handler_circulate_notify");

    event.response_type = XCB_CIRCULATE_REQUEST;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_circulate_request",
            "CIRCULATE_REQUEST is routed to"
            " handler_circulate_request");

    event.response_type = XCB_PROPERTY_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_property_notify",
            "PROPERTY_NOTIFY is routed to handler_property_notify");

    event.response_type = XCB_COLORMAP_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_colormap_notify",
            "COLORMAP_NOTIFY is routed to handler_colormap_notify");

    event.response_type = XCB_CLIENT_MESSAGE;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_client_message",
            "CLIENT_MESSAGE is routed to handler_client_message");

    event.response_type = XCB_MAPPING_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_mapping_notify",
            "MAPPING_NOTIFY is routed to handler_mapping_notify");
}


/* CREATE_NOTIFY and REPARENT_NOTIFY are both routed to the table's own
 * deliberate no-op entry, s_loop_dispatch_ignore, reachable here only
 * indirectly: no stand-in is ever called for either, which is exactly
 * how a silent ignore is told apart from an unhandled type */
static void s_test_ignored_events_call_nothing(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t event;
    xcb_generic_event_t *event_ptr;

    memset(&event, 0, sizeof(event));

    event.response_type = XCB_CREATE_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_INT(s_call_count, 0,
            "CREATE_NOTIFY is routed to the silent ignore entry,"
            " calling no stand-in");

    event.response_type = XCB_REPARENT_NOTIFY;
    event_ptr = &event;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_INT(s_call_count, 0,
            "REPARENT_NOTIFY is routed to the silent ignore entry,"
            " calling no stand-in");
}


/* A type with no table entry at all is logged and otherwise ignored,
 * calling no stand-in either; XCB_NO_OPERATION, response type 127,
 * has no dedicated handler anywhere in the table */
static void s_test_unhandled_type_calls_nothing(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t event;
    xcb_generic_event_t *event_ptr = &event;

    memset(&event, 0, sizeof(event));
    event.response_type = 127u;

    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);

    TAP_EQ_INT(s_call_count, 0,
            "a response type with no table entry calls no stand-in");
}


/* An XRandR extension event, recognized only when is_randr_available
 * is set and the type matches randr_base_event plus the known
 * offsets, is routed to handler_randr_event ahead of the plain table
 * lookup entirely; the same event type is left to the plain table
 * (finding nothing, and so calling nothing) once randr is reported
 * unavailable */
static void s_test_randr_extension_event(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t event;
    xcb_generic_event_t *event_ptr = &event;

    memset(&event, 0, sizeof(event));
    ctx.is_randr_available = true;
    ctx.randr_base_event = 90u;
    event.response_type =
        (uint8_t) (90u + XCB_RANDR_SCREEN_CHANGE_NOTIFY);

    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_randr_event",
            "a matching RandR screen-change type, with randr"
            " available, is routed to handler_randr_event");
    TAP_EQ_INT(s_call_count, 1,
            "the RandR route is taken instead of falling through to"
            " the plain table");

    ctx.is_randr_available = false;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_INT(s_call_count, 0,
            "the very same response type, with randr unavailable, is"
            " left to the plain table, which has no entry for it"
            " either");
}


/* An XSync extension event is recognized the same way, gated on
 * is_sync_available and sync_base_event instead */
static void s_test_sync_extension_event(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t event;
    xcb_generic_event_t *event_ptr = &event;

    memset(&event, 0, sizeof(event));
    ctx.is_sync_available = true;
    ctx.sync_base_event = 64u;
    event.response_type = (uint8_t) (64u + XCB_SYNC_ALARM_NOTIFY);

    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_STR(s_last_called, "handler_sync_event",
            "a matching XSync alarm-notify type, with sync"
            " available, is routed to handler_sync_event");

    ctx.is_sync_available = false;
    s_reset();
    loop_dispatch_event(&ctx, &event_ptr);
    TAP_EQ_INT(s_call_count, 0,
            "the very same response type, with sync unavailable, is"
            " left to the plain table instead");
}


int main(void)
{
    TAP_PLAN(39);

    s_test_null_guards();
    s_test_protocol_error();
    s_test_input_events_route_to_loop_event();
    s_test_synthetic_bit_is_masked();
    s_test_handler_forwarding_events();
    s_test_ignored_events_call_nothing();
    s_test_unhandled_type_calls_nothing();
    s_test_randr_extension_event();
    s_test_sync_extension_event();

    return TAP_DONE();
}
