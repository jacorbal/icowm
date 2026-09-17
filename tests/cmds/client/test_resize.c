/**
 * @file tests/cmds/client/test_resize.c
 *
 * @brief Test battery for the client resize command and its
 *        @c _NET_WM_SYNC_REQUEST throttling pipeline
 *        (cmds/client/resize.c)
 *
 * 'ccmd_client_resize', 'ccmd_client_resize_force', and 'ccmd_client_
 * resize_flush_pending' are exercised through the real source file.
 * 'ccmd_client_apply_geometry' itself, the single shared XCB
 * configure point every one of the functions under test funnels
 * through, is a recording stand-in rather than the real one from
 * 'cmds/client/move.c': linking that whole file in would also pull
 * in its own unrelated dependencies ('ccmd_client_monitor',
 * 'stage_monitor_direction', 'ccmd_client_resolve_workarea',
 * 'logger_msg', none of which anything under test here ever calls),
 * so this file instead just records the mask it was asked to apply.
 * The client's own 'layout.geometry.cur' still ends up updated by
 * 'cmds/client/resize.c' itself right after that call returns, which
 * is real code under test, so every assertion below can still
 * inspect the client's own geometry fields directly, exactly as it
 * would against the real 'ccmd_client_apply_geometry'.
 * 'client_size_constrain' is a test-controlled stand-in answering
 * whatever a test registers (by default a no-op passthrough), so a
 * resize's content-space clamping can be exercised in isolation
 * from any real @c WM_NORMAL_HINTS parsing.  'wm_sync_is_available'
 * is a
 * test-controlled stand-in switching the whole throttling pipeline
 * on or off.  'xcb_ewmh_connection_get' answers a real, fully-
 * initialized 'xcb_ewmh_connection_t' (not opaque, a plain struct
 * this project's own header already exposes) so the sync-request
 * 'ClientMessage' build can run for real and be inspected through
 * the recording 'xcb_send_event' stand-in.  'ccmd_client_unshade',
 * 'client_decoration_layout_sync', 'wm_request_client_redraw',
 * 'client_send_synthetic_configure_notify', 'ccmd_target_win', and
 * 'xcb_clear_area' are link-only stand-ins: side effects this file's
 * assertions do not need to observe directly, the geometry and state
 * changes already being visible on 'client_td' itself.
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
#include <stdint.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <client.h>
#include <cmds/client/move.h>
#include <cmds/client/resize.h>
#include <desktop.h>
#include <harness/tap.h>
#include <stage.h>
#include <types/pair.h>


/**
 * @brief Link-only stand-in for @a ccmd_target_win
 * @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client)
{
    (void) client;
    return (xcb_window_t) 1;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * Answers a fixed non-null, never-dereferenced value: nothing in
 * this file ever reads through the pointer itself, since
 * @a xcb_clear_area and @a xcb_send_event are both stand-ins rather
 * than real libxcb calls, and @a ccmd_client_apply_geometry itself
 * is a stand-in too, never reaching the real
 * @a xcb_configure_window at all
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return (xcb_connection_t *) (uintptr_t) 1;
}


/** Recorded arguments from the last @a ccmd_client_apply_geometry
 *  call */
static uint16_t s_cw_mask;
static int s_cw_count;


/**
 * @brief Recording stand-in for @a ccmd_client_apply_geometry
 *
 * See this file's own doc comment for why this stands in for the
 * real 'cmds/client/move.c' implementation: records only the mask
 * this call was asked to apply, since 'cmds/client/resize.c' itself,
 * real code under test, is the one that updates
 * 'client->layout.geometry.cur' right after calling this
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_apply_geometry(client_td *client,
        xcb_window_t target, uint16_t mask,
        int32_t x, int32_t y, uint32_t w, uint32_t h,
        uint32_t border_width)
{
    (void) client;
    (void) target;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    (void) border_width;

    s_cw_mask = mask;
    s_cw_count++;
}


/** Count of @a xcb_clear_area calls, the only thing any test here
 *  needs to know about it */
static int s_clear_area_count;


/**
 * @brief Recording stand-in for @a xcb_clear_area
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_clear_area(xcb_connection_t *connection,
        uint8_t exposures, xcb_window_t window,
        int16_t x, int16_t y, uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) connection;
    (void) exposures;
    (void) window;
    (void) x;
    (void) y;
    (void) width;
    (void) height;

    s_clear_area_count++;

    return cookie;
}


/** Recorded arguments from the last @a xcb_send_event call, so a
 *  test can inspect the sync-request 'ClientMessage' this file's
 *  own 's_ccmd_resize_send_sync_request' builds */
static xcb_client_message_event_t s_sent_event;
static int s_send_event_count;


/**
 * @brief Recording stand-in for @a xcb_send_event
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_send_event(xcb_connection_t *connection,
        uint8_t propagate, xcb_window_t destination,
        uint32_t event_mask, const char *event)
{
    xcb_void_cookie_t cookie = {.sequence = 0u};

    (void) connection;
    (void) propagate;
    (void) destination;
    (void) event_mask;

    memcpy(&s_sent_event, event, sizeof(s_sent_event));
    s_send_event_count++;

    return cookie;
}


/** Real, fully-initialized EWMH connection this file's own
 *  @a xcb_ewmh_connection_get stand-in answers, so the sync-request
 *  build runs against real atom values rather than all zeroes */
static xcb_ewmh_connection_t s_ewmh;
static bool s_ewmh_present;


/**
 * @brief Test-controlled stand-in for @a xcb_ewmh_connection_get
 *
 * Answers @c NULL when @a s_ewmh_present is false, exercising every
 * function under test's own null-EWMH-connection tolerance; answers
 * the address of a real, filled-in struct otherwise
 *
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return (s_ewmh_present) ? &s_ewmh : NULL;
}


static void s_set_ewmh_present(bool present)
{
    memset(&s_ewmh, 0, sizeof(s_ewmh));
    s_ewmh.WM_PROTOCOLS = (xcb_atom_t) 111u;
    s_ewmh._NET_WM_SYNC_REQUEST = (xcb_atom_t) 222u;
    s_ewmh_present = present;
}


/** Whether @a wm_sync_is_available answers true, registered by
 *  @a s_set_sync_available */
static bool s_sync_available;


/**
 * @brief Test-controlled stand-in for @a wm_sync_is_available
 * @note Complexity: @e O(1)
 */
bool wm_sync_is_available(void)
{
    return s_sync_available;
}


/**
 * @brief Test-controlled stand-in for @a client_size_constrain
 *
 * A no-op passthrough by default, leaving width/height exactly as
 * requested: what every test not specifically exercising the
 * ICCCM size-hint clamp itself needs, so the resize math under test
 * is picked apart from the hint parsing 'client_size_constrain'
 * itself would otherwise perform
 *
 * @note Complexity: @e O(1)
 */
void client_size_constrain(const client_td *client,
        uint32_t *restrict width, uint32_t *restrict height)
{
    (void) client;
    (void) width;
    (void) height;
}


/**
 * @brief Link-only stand-in for @a client_decoration_layout_sync
 * @note Complexity: @e O(1)
 */
void client_decoration_layout_sync(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a wm_request_client_redraw
 * @note Complexity: @e O(1)
 */
void wm_request_client_redraw(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a client_send_synthetic_configure_
 *        notify
 * @note Complexity: @e O(1)
 */
void client_send_synthetic_configure_notify(xcb_connection_t *connection,
        const client_td *client)
{
    (void) connection;
    (void) client;
}


/** Whether @a ccmd_client_unshade was called at all */
static int s_unshade_count;


/**
 * @brief Recording stand-in for @a ccmd_client_unshade
 * @note Complexity: @e O(1)
 */
void ccmd_client_unshade(client_td *client)
{
    if (client != NULL) {
        client->properties.flags &= ~(uint16_t) CLIENT_FLAG_SHADED;
    }
    s_unshade_count++;
}


static void s_reset(void)
{
    s_cw_mask = 0u;
    s_cw_count = 0;
    s_clear_area_count = 0;
    memset(&s_sent_event, 0, sizeof(s_sent_event));
    s_send_event_count = 0;
    s_ewmh_present = false;
    memset(&s_ewmh, 0, sizeof(s_ewmh));
    s_sync_available = false;
    s_unshade_count = 0;
}


static struct geometry_s s_geom(int32_t x, int32_t y,
        uint32_t w, uint32_t h)
{
    struct geometry_s g;

    g.pos.x = x;
    g.pos.y = y;
    g.dim.w = w;
    g.dim.h = h;

    return g;
}


/* A null client is a silent no-op for every entry point in this
 * file, never a crash */
static void s_test_null_client_is_noop(void)
{
    struct geometry_s geom = s_geom(0, 0, 100u, 100u);

    s_reset();

    ccmd_client_resize(NULL, geom);
    ccmd_client_resize_force(NULL, geom);
    ccmd_client_resize_flush_pending(NULL);

    TAP_OK(true, "every entry point tolerates a null client without"
            " crashing");
}


/* A fullscreen client refuses the resize outright */
static void s_test_resize_fullscreen_refused(void)
{
    client_td client;
    struct geometry_s geom = s_geom(0, 0, 500u, 500u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_FULLSCREEN;
    client.layout.geometry.cur.dim.w = 111u;
    client.layout.geometry.cur.dim.h = 222u;

    ccmd_client_resize(&client, geom);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 111,
            "a fullscreen client's width is left untouched");
    TAP_EQ_INT(client.layout.geometry.cur.dim.h, 222,
            "a fullscreen client's height is left untouched");
}


/* A fully maximized client (both axes) refuses the resize outright
 * too */
static void s_test_resize_maximized_both_refused(void)
{
    client_td client;
    struct geometry_s geom = s_geom(0, 0, 500u, 500u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;
    client.layout.geometry.cur.dim.w = 333u;

    ccmd_client_resize(&client, geom);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 333,
            "a fully maximized client's width is left untouched");
}


/* A client maximized on only one axis is deliberately let through:
 * its free axis stays genuinely resizable, per the function's own
 * doc comment */
static void s_test_resize_single_axis_maximized_allowed(void)
{
    client_td client;
    struct geometry_s geom = s_geom(0, 0, 640u, 480u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;

    ccmd_client_resize(&client, geom);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 640,
            "a client maximized on only one axis is still"
            " resizable");
    TAP_EQ_INT(client.layout.geometry.cur.dim.h, 480,
            "and both dimensions from the request are applied");
}


/* A shaded client is unshaded as a precondition, then resized
 * normally rather than refused outright */
static void s_test_resize_shaded_unshaded_first(void)
{
    client_td client;
    struct geometry_s geom = s_geom(10, 20, 300u, 200u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.flags |= (uint16_t) CLIENT_FLAG_SHADED;

    ccmd_client_resize(&client, geom);

    TAP_OK(!(client.properties.flags & (uint16_t) CLIENT_FLAG_SHADED),
            "a shaded client is unshaded before being resized");
    TAP_EQ_INT(s_unshade_count, 1,
            "ccmd_client_unshade is called exactly once");
    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 300,
            "and the resize itself still goes through afterward");
}


/* An unsynchronized client (no _NET_WM_SYNC_REQUEST support) applies
 * the geometry immediately, with no sync request sent at all */
static void s_test_resize_unsynced_applies_immediately(void)
{
    client_td client;
    struct geometry_s geom = s_geom(5, 5, 800u, 600u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.hints_ewmh.sync.is_supported = false;

    ccmd_client_resize(&client, geom);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 800,
            "an unsynchronized client's width is applied right away");
    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 5,
            "and its position is applied right away too");
    TAP_EQ_INT(s_send_event_count, 0,
            "no sync request is ever sent for an unsynchronized"
            " client");
}


/* A client that supports sync but with no live sync subsystem
 * available also applies immediately, unsynchronized */
static void s_test_resize_sync_supported_but_unavailable(void)
{
    client_td client;
    struct geometry_s geom = s_geom(0, 0, 400u, 300u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.hints_ewmh.sync.is_supported = true;
    s_sync_available = false;

    ccmd_client_resize(&client, geom);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 400,
            "geometry is still applied when the sync subsystem"
            " itself is unavailable");
    TAP_EQ_INT(s_send_event_count, 0,
            "no sync request is sent without a live sync subsystem");
}


/* A synchronized client not already waiting on an acknowledgement
 * dispatches immediately: the sync request is sent and the geometry
 * applied in the same call */
static void s_test_resize_synced_dispatches_immediately(void)
{
    client_td client;
    struct geometry_s geom = s_geom(0, 0, 640u, 480u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.hints_ewmh.sync.is_supported = true;
    s_sync_available = true;
    s_set_ewmh_present(true);

    ccmd_client_resize(&client, geom);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 640,
            "a synchronized client's geometry is applied right"
            " away too, not deferred");
    TAP_EQ_INT(s_send_event_count, 1,
            "exactly one sync request is sent for the first resize"
            " in a session");
    TAP_OK(client.hints_ewmh.sync.is_waiting,
            "the client is now marked as waiting for the"
            " acknowledgement");
    TAP_EQ_INT((long) client.hints_ewmh.sync.value, 1,
            "the local shadow sync counter is incremented once");
}


/* The sync request 'ClientMessage' itself carries the right window,
 * protocol atoms, and counter value */
static void s_test_resize_sync_message_contents(void)
{
    client_td client;
    struct geometry_s geom = s_geom(0, 0, 200u, 200u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.window = (xcb_window_t) 77u;
    client.hints_ewmh.sync.is_supported = true;
    s_sync_available = true;
    s_set_ewmh_present(true);

    ccmd_client_resize(&client, geom);

    TAP_EQ_INT((long) s_sent_event.response_type, XCB_CLIENT_MESSAGE,
            "the sent event is a genuine ClientMessage");
    TAP_EQ_INT((long) s_sent_event.window, 77,
            "the ClientMessage targets the client's own window");
    TAP_EQ_INT((long) s_sent_event.type, 111,
            "the ClientMessage type is WM_PROTOCOLS");
    TAP_EQ_INT((long) s_sent_event.data.data32[0], 222,
            "data32[0] names _NET_WM_SYNC_REQUEST");
    TAP_EQ_INT((long) s_sent_event.data.data32[2], 1,
            "data32[2] carries the new counter value");
}


/* A synchronized client already waiting queues the new geometry
 * instead of dispatching a second, overlapping request */
static void s_test_resize_synced_already_waiting_queues(void)
{
    client_td client;
    struct geometry_s first = s_geom(0, 0, 100u, 100u);
    struct geometry_s second = s_geom(0, 0, 200u, 200u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.hints_ewmh.sync.is_supported = true;
    s_sync_available = true;
    s_set_ewmh_present(true);

    ccmd_client_resize(&client, first);
    ccmd_client_resize(&client, second);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 100,
            "the already-applied geometry from the first call is"
            " left in place, not overwritten by the queued one");
    TAP_OK(client.hints_ewmh.sync.has_pending,
            "the second geometry is queued as pending instead");
    TAP_EQ_INT(client.hints_ewmh.sync.pending_geom.dim.w, 200,
            "the queued geometry is exactly the second request");
    TAP_EQ_INT(s_send_event_count, 1,
            "only the first call's sync request was ever sent");
}


/* An unresponsive client, past WM_SYNC_MAX_WAIT_TICKS worth of
 * queued attempts, gives up waiting and force-dispatches instead */
static void s_test_resize_gives_up_after_max_wait_ticks(void)
{
    client_td client;
    struct geometry_s g1 = s_geom(0, 0, 10u, 10u);
    struct geometry_s g2 = s_geom(0, 0, 20u, 20u);
    struct geometry_s g3 = s_geom(0, 0, 30u, 30u);
    struct geometry_s g4 = s_geom(0, 0, 40u, 40u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.hints_ewmh.sync.is_supported = true;
    s_sync_available = true;
    s_set_ewmh_present(true);

    /* First call dispatches and starts waiting; every call after
     * that queues instead, bumping 'wait_ticks', until it exceeds
     * WM_SYNC_MAX_WAIT_TICKS (2), at which point the next call gives
     * up and dispatches directly again */
    ccmd_client_resize(&client, g1);
    ccmd_client_resize(&client, g2);
    ccmd_client_resize(&client, g3);
    ccmd_client_resize(&client, g4);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 40,
            "once wait_ticks exceeds the maximum, the newest"
            " geometry is applied directly instead of staying"
            " queued forever");
    TAP_OK(!client.hints_ewmh.sync.has_pending,
            "the pending flag is cleared once forced through");
}


/* ccmd_client_resize_force bypasses any in-flight wait state and
 * applies right away, discarding whatever was queued */
static void s_test_resize_force_bypasses_pending(void)
{
    client_td client;
    struct geometry_s first = s_geom(0, 0, 50u, 50u);
    struct geometry_s queued = s_geom(0, 0, 60u, 60u);
    struct geometry_s forced = s_geom(0, 0, 999u, 999u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.hints_ewmh.sync.is_supported = true;
    s_sync_available = true;
    s_set_ewmh_present(true);

    ccmd_client_resize(&client, first);
    ccmd_client_resize(&client, queued);
    TAP_OK(client.hints_ewmh.sync.has_pending,
            "a geometry is indeed queued before the forced call");

    ccmd_client_resize_force(&client, forced);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 999,
            "the forced geometry is applied immediately, bypassing"
            " the queue");
    TAP_OK(!client.hints_ewmh.sync.has_pending,
            "the previously queued geometry is discarded, not"
            " left to resurface later");
}


/* ccmd_client_resize_force on a fully maximized client still refuses
 * outright, the same precondition the throttled entry point shares */
static void s_test_resize_force_maximized_refused(void)
{
    client_td client;
    struct geometry_s geom = s_geom(0, 0, 500u, 500u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;
    client.layout.geometry.cur.dim.w = 42u;

    ccmd_client_resize_force(&client, geom);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 42,
            "a fully maximized client refuses a forced resize too");
}


/* ccmd_client_resize_flush_pending on a client not currently waiting
 * is a no-op */
static void s_test_flush_pending_noop_when_not_waiting(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.hints_ewmh.sync.is_waiting = false;
    client.layout.geometry.cur.dim.w = 15u;

    ccmd_client_resize_flush_pending(&client);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 15,
            "flushing when nothing is pending leaves geometry"
            " untouched");
}


/* ccmd_client_resize_flush_pending applies a queued geometry once
 * the wait clears, dispatching the next sync request for it */
static void s_test_flush_pending_applies_queued_geometry(void)
{
    client_td client;
    struct geometry_s first = s_geom(0, 0, 10u, 10u);
    struct geometry_s queued = s_geom(0, 0, 321u, 321u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.hints_ewmh.sync.is_supported = true;
    s_sync_available = true;
    s_set_ewmh_present(true);

    ccmd_client_resize(&client, first);
    ccmd_client_resize(&client, queued);
    TAP_OK(client.hints_ewmh.sync.has_pending,
            "a geometry is queued before the alarm fires");

    ccmd_client_resize_flush_pending(&client);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 321,
            "the queued geometry is applied once the alarm fires");
    TAP_OK(!client.hints_ewmh.sync.has_pending,
            "the pending flag is cleared once applied");
    TAP_OK(client.hints_ewmh.sync.is_waiting,
            "is_waiting ends up true again, since applying the"
            " queued geometry dispatches a fresh sync request of"
            " its own, unconditionally marking the client as"
            " waiting once more");
    TAP_EQ_INT(s_send_event_count, 2,
            "flushing a pending geometry sends a fresh sync"
            " request for it, on top of the original dispatch");
}


/* A resize whose requested position differs from the client's
 * current one asks for the full X/Y/WIDTH/HEIGHT mask; one that only
 * changes size asks for WIDTH/HEIGHT alone */
static void s_test_resize_mask_depends_on_position_change(void)
{
    client_td client;
    struct geometry_s same_pos = s_geom(0, 0, 500u, 400u);
    struct geometry_s moved = s_geom(1, 0, 500u, 400u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 0;
    client.layout.geometry.cur.pos.y = 0;

    ccmd_client_resize(&client, same_pos);
    TAP_EQ_INT(s_cw_mask,
            (long) ((uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                (uint16_t) XCB_CONFIG_WINDOW_HEIGHT),
            "an unchanged position asks only for the WIDTH/HEIGHT"
            " mask");

    ccmd_client_resize(&client, moved);
    TAP_EQ_INT(s_cw_mask,
            (long) ((uint16_t) XCB_CONFIG_WINDOW_X |
                (uint16_t) XCB_CONFIG_WINDOW_Y |
                (uint16_t) XCB_CONFIG_WINDOW_WIDTH |
                (uint16_t) XCB_CONFIG_WINDOW_HEIGHT),
            "a changed position asks for the full X/Y/WIDTH/HEIGHT"
            " mask");
}


/* The frame dimensions actually configured are clamped through
 * geom_dim_clamp against the minimum window dimension, even when
 * the requested content size (after subtracting frame extents)
 * would fall under it */
static void s_test_resize_clamps_to_minimum_dimension(void)
{
    client_td client;
    struct geometry_s tiny = s_geom(0, 0, 1u, 1u);

    s_reset();
    memset(&client, 0, sizeof(client));

    ccmd_client_resize(&client, tiny);

    TAP_OK(client.layout.geometry.cur.dim.w >= WM_MIN_WINDOW_DIMENSION,
            "width is clamped up to at least the minimum window"
            " dimension");
    TAP_OK(client.layout.geometry.cur.dim.h >= WM_MIN_WINDOW_DIMENSION,
            "height is clamped up to at least the minimum window"
            " dimension too");
}


/* Frame extents are subtracted before content-space clamping and
 * added back afterward, so a decorated client's final frame size
 * still accounts for its own decoration */
static void s_test_resize_accounts_for_frame_extents(void)
{
    client_td client;
    struct geometry_s geom = s_geom(0, 0, 500u, 400u);

    s_reset();
    memset(&client, 0, sizeof(client));
    client.layout.frame_extents.left = 2;
    client.layout.frame_extents.right = 3;
    client.layout.frame_extents.top = 20;
    client.layout.frame_extents.bottom = 5;

    ccmd_client_resize(&client, geom);

    TAP_EQ_INT(client.layout.geometry.cur.dim.w, 500,
            "the frame width round-trips through content space"
            " unchanged when client_size_constrain is a"
            " passthrough");
    TAP_EQ_INT(client.layout.geometry.cur.dim.h, 400,
            "the frame height round-trips through content space"
            " unchanged the same way");
}


int main(void)
{
    TAP_PLAN(45);

    s_test_null_client_is_noop();
    s_test_resize_fullscreen_refused();
    s_test_resize_maximized_both_refused();
    s_test_resize_single_axis_maximized_allowed();
    s_test_resize_shaded_unshaded_first();
    s_test_resize_unsynced_applies_immediately();
    s_test_resize_sync_supported_but_unavailable();
    s_test_resize_synced_dispatches_immediately();
    s_test_resize_sync_message_contents();
    s_test_resize_synced_already_waiting_queues();
    s_test_resize_gives_up_after_max_wait_ticks();
    s_test_resize_force_bypasses_pending();
    s_test_resize_force_maximized_refused();
    s_test_flush_pending_noop_when_not_waiting();
    s_test_flush_pending_applies_queued_geometry();
    s_test_resize_mask_depends_on_position_change();
    s_test_resize_clamps_to_minimum_dimension();
    s_test_resize_accounts_for_frame_extents();

    return TAP_DONE();
}
