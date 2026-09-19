/**
 * @file tests/handler/test_configure.c
 *
 * @brief Unit tests for @c handler/configure.c
 *
 * Covers @a handler_configure_request and @a handler_configure_notify,
 * exercised entirely through synthetic @c xcb_configure_request_event_t
 * and @c xcb_configure_notify_event_t structs built by hand, with a
 * hand-built @c client_td/stage_td/desktop_td triple on the stack;
 * no live X connection is ever needed.  @a lookup_find_client is
 * replaced by a controlled stand-in (the same pattern used in
 * tests/handler/test_focus.c and tests/handler/test_crossing.c) so each
 * scenario can hand back exactly the client (or null) it wants to
 * exercise, without needing a real ohtbl-backed desktop client table.
 *
 * Two collaborators, @a client_gravity_adjust_pos (src/client/state.c)
 * and @a clock_ms_since (src/utils/time/clock.c), are linked for real:
 * both are tiny, dependency-free leaf functions and the whole point of
 * several scenarios below (the gravity-preserving resize, and the
 * shade/fullscreen cooldown suppression) is to exercise their actual
 * arithmetic, not a stand-in's guess at it.  Every other collaborator
 * that only records that it happened, or that talks to a real X server
 * (systray_icon_size_enforce, ccmd_desktop_enforce_layers,
 * client_send_synthetic_configure_notify,
 * client_decoration_layout_sync,
 * render_client_decoration_repaint_frame, xcb_configure_window,
 * logger_msg),
 * is a link-only stand-in defined below.
 *
 * Deliberately out of scope: the @c XCB_CONFIG_WINDOW_SIBLING
 * value-list slot (this window manager's own configure calls never
 * set it, and exercising it would only restate the same mask-bit
 * plumbing already covered by X/Y/WIDTH/HEIGHT/BORDER_WIDTH), and the
 * exact pixel content of the frame-decoration repaint invoked from
 * @a handler_configure_notify (covered only as "was it called or not",
 * matching the same narrowing already used for the drawing tails of
 * tests/handler/test_expose.c and tests/handler/test_focus.c).
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* struct timespec, clock_gettime */


/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <client/state.h>
#include <config.h>
#include <defs/client.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <stage.h>
#include <systray.h>

/* Local includes */
#include <handler/configure.h>
#include <harness/tap.h>
#include <render/client/decoration.h>


/* What lookup_find_client should hand back for the next call, and to
 * which stage/desktop it should attribute that client */
static client_td *s_lookup_result = NULL;
static stage_td *s_lookup_stage_out = NULL;
static desktop_td *s_lookup_desktop_out = NULL;

/* What systray_icon_size_enforce should report for the next call */
static bool s_systray_enforce_result = false;
static unsigned int s_call_systray_enforce = 0u;

/* Call counters/recorders for every other link-only stand-in */
static unsigned int s_call_configure_window = 0u;
static xcb_window_t s_configure_window_target = XCB_WINDOW_NONE;
static uint16_t s_configure_window_mask = 0u;
static uint32_t s_configure_window_values[7];
static unsigned int s_call_enforce_layers = 0u;
static unsigned int s_call_send_synthetic = 0u;
static unsigned int s_call_layout_sync = 0u;
static unsigned int s_call_repaint_frame_decoration = 0u;
static bool s_repaint_use_active_style = false;


/** Controlled stand-in for lookup_find_client */
client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **stage, desktop_td **desktop)
{
    (void) stages;
    (void) window;

    if (stage != NULL) {
        *stage = s_lookup_stage_out;
    }
    if (desktop != NULL) {
        *desktop = s_lookup_desktop_out;
    }

    return s_lookup_result;
}


/** Controlled stand-in for systray_icon_size_enforce */
bool systray_icon_size_enforce(xcb_window_t window)
{
    (void) window;

    s_call_systray_enforce++;
    return s_systray_enforce_result;
}


/** Link-only stand-in for xcb_configure_window; records the target
 *  window, mask, and value list it was asked to apply */
xcb_void_cookie_t xcb_configure_window(xcb_connection_t *c,
        xcb_window_t window, uint16_t value_mask, const void *value_list)
{
    unsigned int i;
    unsigned int n_values = 0u;
    xcb_void_cookie_t cookie = {0};
    const uint32_t *values = (const uint32_t *) value_list;

    (void) c;

    s_call_configure_window++;
    s_configure_window_target = window;
    s_configure_window_mask = value_mask;

    for (i = 0u; i < 7u; i++) {
        if (value_mask & (uint16_t) (1u << i)) {
            n_values++;
        }
    }
    for (i = 0u; i < n_values && i < 7u; i++) {
        s_configure_window_values[i] = values[i];
    }

    return cookie;
}


/** Link-only stand-in for ccmd_desktop_enforce_layers */
void ccmd_desktop_enforce_layers(desktop_td *desktop)
{
    (void) desktop;

    s_call_enforce_layers++;
}


/** Link-only stand-in for client_send_synthetic_configure_notify */
void client_send_synthetic_configure_notify(
        xcb_connection_t *connection, const client_td *client)
{
    (void) connection;
    (void) client;

    s_call_send_synthetic++;
}


/** Link-only stand-in for client_decoration_layout_sync */
void client_decoration_layout_sync(client_td *client)
{
    (void) client;

    s_call_layout_sync++;
}


/** Link-only stand-in for render_client_decoration_repaint_frame;
 *  records whether it was asked to use the active (focused) style */
void render_client_decoration_repaint_frame(xcb_connection_t *connection,
        client_td *client, bool use_active_style,
        const struct config_theme_s *theme)
{
    (void) connection;
    (void) client;
    (void) theme;

    s_call_repaint_frame_decoration++;
    s_repaint_use_active_style = use_active_style;
}


/** Link-only stand-in for logger_msg */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    va_list args;

    (void) level;
    (void) prefix;

    va_start(args, fmt);
    va_end(args);

    return 0;
}


/**
 * @brief Reset every stand-in's recorded state and lookup fixture
 *
 * Called at the top of every scenario so each test is independent of
 * whatever the previous one did.
 */
static void s_test_reset_state(void)
{
    s_lookup_result = NULL;
    s_lookup_stage_out = NULL;
    s_lookup_desktop_out = NULL;
    s_systray_enforce_result = false;
    s_call_systray_enforce = 0u;
    s_call_configure_window = 0u;
    s_configure_window_target = XCB_WINDOW_NONE;
    s_configure_window_mask = 0u;
    memset(s_configure_window_values, 0, sizeof(s_configure_window_values));
    s_call_enforce_layers = 0u;
    s_call_send_synthetic = 0u;
    s_call_layout_sync = 0u;
    s_call_repaint_frame_decoration = 0u;
    s_repaint_use_active_style = false;
}


/**
 * @brief Build a plain, undecorated, non-transitioning client
 *
 * @param client Client struct to initialize; zeroed then filled with
 *               a window ID, a starting geometry, and no rule locks,
 *               no active operation, no fullscreen state, so every
 *               scenario starts from the same clean baseline and only
 *               changes the one field it means to exercise
 */
static void s_test_build_plain_client(client_td *client)
{
    memset(client, 0, sizeof(*client));
    client->window = 0x100;
    client->frame = 0;
    client->id = 0x100;
    client->layout.geometry.cur.pos.x = 10;
    client->layout.geometry.cur.pos.y = 20;
    client->layout.geometry.cur.dim.w = 300u;
    client->layout.geometry.cur.dim.h = 200u;
    client->layout.geometry.old.dim.w = 300u;
    client->layout.geometry.old.dim.h = 200u;
    client->layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;
    client->has_rule_position_locked = false;
    client->properties.operation = CLIENT_OPERATION_IDLE;
    client->properties.state = 0u;
    client->properties.flags = 0u;
}


/**
 * @brief Build a decorated, reparented client with frame extents
 *
 * @param client Client struct to fill; same baseline geometry as
 *               @a s_test_build_plain_client but with a frame window,
 *               the decorated flag set, and non-zero extents on all
 *               four sides so the on_inner coordinate translation
 *               actually has something to translate
 */
static void s_test_build_decorated_client(client_td *client)
{
    s_test_build_plain_client(client);
    client->frame = 0x200;
    client->properties.flags = (uint16_t) CLIENT_FLAG_DECORATED;
    client->layout.frame_extents.left = 4;
    client->layout.frame_extents.right = 4;
    client->layout.frame_extents.top = 24;
    client->layout.frame_extents.bottom = 4;
}


/**
 * @brief Build a plain request event with the given mask and geometry
 *
 * @param event  Event struct to fill
 * @param window Window the request targets
 * @param mask   Value mask bits present in the request
 * @param x      Requested X, used only when @p mask carries it
 * @param y      Requested Y, used only when @p mask carries it
 * @param w      Requested width, used only when @p mask carries it
 * @param h      Requested height, used only when @p mask carries it
 */
static void s_test_build_request_event(
        xcb_configure_request_event_t *event, xcb_window_t window,
        uint16_t mask, int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    memset(event, 0, sizeof(*event));
    event->response_type = XCB_CONFIGURE_REQUEST;
    event->window = window;
    event->value_mask = mask;
    event->x = x;
    event->y = y;
    event->width = w;
    event->height = h;
    event->border_width = 0u;
    event->stack_mode = XCB_STACK_MODE_ABOVE;
}


/* A null event never crashes the request handler */
static void s_test_request_null_event(void)
{
    list_td stages;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));

    handler_configure_request(NULL, &stages, NULL);

    TAP_OK(s_call_configure_window == 0u,
            "a null request event triggers no xcb_configure_window call");
}


/* An unmanaged window whose fixed size the systray enforces is left
 * alone entirely: no generic forward happens for it */
static void s_test_request_systray_enforced(void)
{
    xcb_configure_request_event_t event;
    list_td stages;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    s_lookup_result = NULL;
    s_systray_enforce_result = true;
    s_test_build_request_event(&event, 0x999, XCB_CONFIG_WINDOW_WIDTH,
            0, 0, 64u, 64u);

    handler_configure_request(NULL, &stages, &event);

    TAP_OK(s_call_systray_enforce == 1u,
            "an unmanaged window is checked against the systray's" \
            " enforced icon size");
    TAP_OK(s_call_configure_window == 0u,
            "a systray-enforced window's request is never forwarded");
}


/* An unmanaged, non-systray window has its request forwarded
 * unmodified */
static void s_test_request_unmanaged_forwarded(void)
{
    xcb_configure_request_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    s_lookup_result = NULL;
    s_systray_enforce_result = false;
    s_test_build_request_event(&event, 0x999,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, 15, 25, 0u, 0u);

    handler_configure_request(connection, &stages, &event);

    TAP_OK(s_call_configure_window == 1u,
            "an unmanaged window's request is forwarded once");
    TAP_EQ_INT((int) s_configure_window_target, (int) 0x999,
            "the forwarded request targets the requesting window" \
            " itself");
    TAP_EQ_INT((int) s_configure_window_values[0], 15,
            "the forwarded request carries the client's own X" \
            " untouched");
    TAP_EQ_INT((int) s_configure_window_values[1], 25,
            "the forwarded request carries the client's own Y" \
            " untouched");
}


/* A plain, undecorated managed client moving via ConfigureRequest has
 * its stored geometry updated and its stage/desktop marked
 * outdated */
static void s_test_request_plain_client_move(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_request_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_plain_client(&client);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_test_build_request_event(&event, client.window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, 50, 60, 0u, 0u);

    handler_configure_request(connection, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 50,
            "a plain client's X is updated to the requested value");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 60,
            "a plain client's Y is updated to the requested value");
    TAP_OK(stage.is_outdated,
            "moving a managed client marks its stage outdated");
    TAP_OK(desktop.is_outdated,
            "moving a managed client marks its desktop outdated");
    TAP_OK(client.is_outdated,
            "moving a managed client marks the client itself outdated");
    TAP_OK(s_call_send_synthetic == 0u,
            "an undecorated client never gets a synthetic notify");
}


/* A client the window manager is actively moving or resizing has its
 * own ConfigureRequest geometry bits ignored, answered only with an
 * acknowledgement */
static void s_test_request_wm_owns_geometry(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_request_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;
    int32_t original_x;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_decorated_client(&client);
    client.properties.operation = CLIENT_OPERATION_MOVING;
    original_x = client.layout.geometry.cur.pos.x;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_test_build_request_event(&event, client.window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, 999, 999, 0u, 0u);

    handler_configure_request(connection, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, original_x,
            "a client the window manager is moving keeps its X" \
            " untouched by its own request");
    TAP_OK(s_call_configure_window == 0u,
            "a fully-suppressed request never reaches" \
            " xcb_configure_window");
    TAP_OK(s_call_send_synthetic == 1u,
            "a reparented client with a fully-suppressed request" \
            " still gets an acknowledging synthetic notify");
}


/* A client maximized horizontally only has its own X/width request
 * ignored, since that axis is owned by the window manager, while its
 * still-free Y is honored normally */
static void s_test_request_maximized_horz_ignores_x_honors_y(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_request_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;
    int32_t original_x;
    uint32_t original_w;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_plain_client(&client);
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    original_x = client.layout.geometry.cur.pos.x;
    original_w = client.layout.geometry.cur.dim.w;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_test_build_request_event(&event, client.window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH,
            999, 555, 400u, 0u);

    handler_configure_request(connection, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, original_x,
            "a horizontally maximized client's X stays put," \
            " ignoring its own request");
    TAP_EQ_INT((long) client.layout.geometry.cur.dim.w,
            (long) original_w,
            "and so does its width");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 555,
            "but its still-free Y is honored normally");
}


/* A fully maximized client has its entire geometry request ignored,
 * the same as a fullscreen one already is */
static void s_test_request_maximized_full_ignores_everything(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_request_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;
    int32_t original_x;
    int32_t original_y;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_plain_client(&client);
    client.properties.state |= (uint16_t) CLIENT_STATE_MAXIMIZED;
    original_x = client.layout.geometry.cur.pos.x;
    original_y = client.layout.geometry.cur.pos.y;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_test_build_request_event(&event, client.window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, 999, 999,
            0u, 0u);

    handler_configure_request(connection, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, original_x,
            "a fully maximized client's X stays put too");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, original_y,
            "and so does its Y, both axes owned by the window" \
            " manager");
    TAP_OK(s_call_configure_window == 0u,
            "a fully-suppressed request never reaches" \
            " xcb_configure_window");
}


/* A request for the exact width/height a client already has is
 * a true no-op: no xcb_configure_window call, no outdating */
static void s_test_request_wh_matches_current(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_request_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_plain_client(&client);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_test_build_request_event(&event, client.window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            0, 0, (uint16_t) client.layout.geometry.cur.dim.w,
            (uint16_t) client.layout.geometry.cur.dim.h);

    handler_configure_request(connection, &stages, &event);

    TAP_OK(s_call_configure_window == 0u,
            "requesting the client's own current size triggers no" \
            " xcb_configure_window call");
    TAP_OK(!stage.is_outdated,
            "requesting the client's own current size never marks" \
            " its stage outdated");
}


/* A geometry request that lands within the shade cooldown window, and
 * whose requested dimensions match what the client had right before
 * shading, is suppressed as a stale echo */
static void s_test_request_shade_cooldown_suppresses(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_request_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_plain_client(&client);
    /* Shrink the CURRENT size so the request (matching the OLD size)
     * cannot also be suppressed by the unconditional
     * wh_matches_current check, isolating the cooldown path alone */
    client.layout.geometry.cur.dim.w = 111u;
    client.layout.geometry.cur.dim.h = 222u;
    clock_gettime(CLOCK_MONOTONIC, &client.shade_transition_time);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_test_build_request_event(&event, client.window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            0, 0, (uint16_t) client.layout.geometry.old.dim.w,
            (uint16_t) client.layout.geometry.old.dim.h);

    handler_configure_request(connection, &stages, &event);

    TAP_OK(s_call_configure_window == 0u,
            "a stale-echo resize within the shade cooldown is" \
            " suppressed");
    TAP_EQ_INT((int) client.layout.geometry.cur.dim.w, 111,
            "a suppressed shade-cooldown resize leaves the client's" \
            " current width untouched");
}


/* The same geometry request, once the shade cooldown has elapsed, is
 * honored normally */
static void s_test_request_shade_cooldown_expired_honors(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_request_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;
    struct timespec long_ago;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_plain_client(&client);
    client.layout.geometry.cur.dim.w = 111u;
    client.layout.geometry.cur.dim.h = 222u;
    clock_gettime(CLOCK_MONOTONIC, &long_ago);
    long_ago.tv_sec -= 3600;
    client.shade_transition_time = long_ago;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_test_build_request_event(&event, client.window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            0, 0, (uint16_t) client.layout.geometry.old.dim.w,
            (uint16_t) client.layout.geometry.old.dim.h);

    handler_configure_request(connection, &stages, &event);

    TAP_OK(s_call_configure_window == 1u,
            "a resize matching the old size is honored once the" \
            " shade cooldown has elapsed");
    TAP_EQ_INT((int) client.layout.geometry.cur.dim.w,
            (int) client.layout.geometry.old.dim.w,
            "an honored post-cooldown resize applies the requested" \
            " width");
}


/* A pure resize (no X/Y in the mask) on a client whose gravity pulls
 * toward the opposite corner shifts the stored position to keep that
 * corner fixed */
static void s_test_request_gravity_adjusts_position(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_request_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;
    int32_t old_x;
    int32_t old_y;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_plain_client(&client);
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_SOUTH_EAST;
    client.layout.geometry.cur.dim.w = 300u;
    client.layout.geometry.cur.dim.h = 200u;
    old_x = client.layout.geometry.cur.pos.x;
    old_y = client.layout.geometry.cur.pos.y;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_test_build_request_event(&event, client.window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            0, 0, 400u, 300u);

    handler_configure_request(connection, &stages, &event);

    TAP_OK(client.layout.geometry.cur.pos.x != old_x,
            "shrinking width under south-east gravity shifts the" \
            " stored X to keep the anchor corner fixed");
    TAP_OK(client.layout.geometry.cur.pos.y != old_y,
            "shrinking height under south-east gravity shifts the" \
            " stored Y to keep the anchor corner fixed");
    TAP_OK((s_configure_window_mask & XCB_CONFIG_WINDOW_X) != 0u,
            "the gravity-adjusted reply carries the X bit even" \
            " though the request itself never asked for X");
}


/* A stack-mode-only request on a managed client asks the desktop to
 * re-enforce its window layering */
static void s_test_request_stack_mode_enforces_layers(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_request_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_plain_client(&client);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    s_test_build_request_event(&event, client.window,
            XCB_CONFIG_WINDOW_STACK_MODE, 0, 0, 0u, 0u);
    event.stack_mode = XCB_STACK_MODE_ABOVE;

    handler_configure_request(connection, &stages, &event);

    TAP_OK(s_call_enforce_layers == 1u,
            "a stack-mode-only request enforces desktop layering" \
            " once");
}


/* A null event never crashes the notify handler */
static void s_test_notify_null_event(void)
{
    list_td stages;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));

    handler_configure_notify(NULL, &stages, NULL);

    TAP_OK(s_call_repaint_frame_decoration == 0u,
            "a null notify event triggers no decoration repaint");
}


/* An unmanaged window's ConfigureNotify is a pure no-op */
static void s_test_notify_unmanaged(void)
{
    xcb_configure_notify_event_t event;
    list_td stages;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CONFIGURE_NOTIFY;
    event.event = 0x999;
    event.window = 0x999;
    s_lookup_result = NULL;

    handler_configure_notify(NULL, &stages, &event);

    TAP_OK(s_call_layout_sync == 0u,
            "an unmanaged window's notify never syncs decoration" \
            " layout");
}


/* A SubStructureNotify-delivered ConfigureNotify for the frame
 * (event->event != event->window) is ignored entirely, leaving the
 * stored geometry untouched */
static void s_test_notify_substructure_ignored(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_notify_event_t event;
    list_td stages;
    int32_t old_x;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_plain_client(&client);
    old_x = client.layout.geometry.cur.pos.x;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CONFIGURE_NOTIFY;
    /* event != window: delivered via SubStructureNotify on root */
    event.event = 0x1;
    event.window = client.window;
    event.x = 500;
    event.y = 500;
    event.width = 999u;
    event.height = 999u;

    handler_configure_notify(NULL, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, old_x,
            "a SubStructureNotify-delivered ConfigureNotify never" \
            " updates the stored position");
    TAP_OK(!stage.is_outdated,
            "a SubStructureNotify-delivered ConfigureNotify never" \
            " outdates the stage");
}





/* A StructureNotify ConfigureNotify that changes both position and
 * size updates stored geometry, repaints the decoration, and outdates
 * the stage/desktop/client */
static void s_test_notify_structure_size_changed(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    config_td config;
    xcb_configure_notify_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    memset(&config, 0, sizeof(config));
    s_test_build_decorated_client(&client);
    desktop.config = &config;
    desktop.client_active_id = client.id;
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CONFIGURE_NOTIFY;
    event.event = client.frame;
    event.window = client.frame;
    event.x = 70;
    event.y = 80;
    event.width = 500u;
    event.height = 400u;

    handler_configure_notify(connection, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 70,
            "a StructureNotify ConfigureNotify updates the stored X");
    TAP_EQ_INT((int) client.layout.geometry.cur.dim.w, 500,
            "a StructureNotify ConfigureNotify updates the stored" \
            " width");
    TAP_OK(s_call_repaint_frame_decoration == 1u,
            "a size-changing ConfigureNotify repaints the frame" \
            " decoration once");
    TAP_OK(s_repaint_use_active_style,
            "the decoration repaint uses the active style for the" \
            " desktop's currently active client");
    TAP_OK(client.is_outdated && stage.is_outdated &&
            desktop.is_outdated,
            "a geometry-changing ConfigureNotify outdates the" \
            " client, its stage, and its desktop");
}


/* A StructureNotify ConfigureNotify that changes only position, not
 * size, updates geometry and outdates without repainting decoration */
static void s_test_notify_structure_move_only(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_notify_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_decorated_client(&client);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CONFIGURE_NOTIFY;
    event.event = client.frame;
    event.window = client.frame;
    event.x = 999;
    event.y = 999;
    event.width = (uint16_t) client.layout.geometry.cur.dim.w;
    event.height = (uint16_t) client.layout.geometry.cur.dim.h;

    handler_configure_notify(connection, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 999,
            "a move-only ConfigureNotify still updates the stored" \
            " position");
    TAP_OK(s_call_repaint_frame_decoration == 0u,
            "a move-only ConfigureNotify never repaints the frame" \
            " decoration");
    TAP_OK(stage.is_outdated,
            "a move-only ConfigureNotify still outdates the stage");
}


/* A ConfigureNotify carrying a position other than the one last asked
 * for is the server echoing an earlier request that arrived late, and
 * its position is refused: taking it would leave the stored geometry
 * a step behind, and a viewport pan, which adds its delta to whatever
 * is stored, would then carry that error forward for good */
static void s_test_notify_stale_position_refused(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_notify_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_decorated_client(&client);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    /* Where the manager has just put it, and what it asked for: the
     * two agreeing is what tells the handler its own request is the
     * last word on this client's position */
    client.layout.geometry.cur.pos.x = 400;
    client.layout.geometry.cur.pos.y = 300;
    client.layout.requested_pos.x = 400;
    client.layout.requested_pos.y = 300;
    client.layout.has_requested_pos = true;

    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CONFIGURE_NOTIFY;
    event.event = client.frame;
    event.window = client.frame;
    event.x = 100;      /* the previous step, echoed late */
    event.y = 100;
    event.width = (uint16_t) client.layout.geometry.cur.dim.w;
    event.height = (uint16_t) client.layout.geometry.cur.dim.h;

    handler_configure_notify(connection, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 400,
            "a stale echo never drags the stored X backwards");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 300,
            "nor the stored Y");

    /* The very same event, once it is what was asked for, is taken */
    client.layout.requested_pos.x = 100;
    client.layout.requested_pos.y = 100;
    client.layout.geometry.cur.pos.x = 100;
    client.layout.geometry.cur.pos.y = 100;
    handler_configure_notify(connection, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 100,
            "while an echo confirming the last request is believed");
}


/* The same for the size: two resizes sent back to back (unshading and
 * then going full screen) give two echoes, and the first, arriving
 * after the second request, must not shrink the stored size back */
static void s_test_notify_stale_size_refused(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_notify_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_decorated_client(&client);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = NULL;     /* no theme needed here */

    client.layout.geometry.cur.pos.x = 0;
    client.layout.geometry.cur.pos.y = 0;
    client.layout.geometry.cur.dim.w = 1920u;
    client.layout.geometry.cur.dim.h = 1080u;
    client.layout.requested_dim.w = 1920u;
    client.layout.requested_dim.h = 1080u;
    client.layout.has_requested_dim = true;

    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CONFIGURE_NOTIFY;
    event.event = client.frame;
    event.window = client.frame;
    event.width = 496u;     /* the unshade before, echoed late */
    event.height = 350u;

    handler_configure_notify(connection, &stages, &event);

    TAP_OK(client.layout.geometry.cur.dim.w == 1920u &&
            client.layout.geometry.cur.dim.h == 1080u,
            "a stale size echo never shrinks the stored size back");

    client.layout.requested_dim.w = 496u;
    client.layout.requested_dim.h = 350u;
    client.layout.geometry.cur.dim.w = 800u;
    client.layout.geometry.cur.dim.h = 600u;
    handler_configure_notify(connection, &stages, &event);

    TAP_OK(client.layout.geometry.cur.dim.w == 496u &&
            client.layout.geometry.cur.dim.h == 350u,
            "while a size the stored one does not match is taken");
}


/* A path that moves the window by writing the stored geometry and
 * configuring the server itself, without recording what it asked for,
 * leaves the two disagreeing; the refusal must not engage there, or
 * that path's own notify would be thrown away */
static void s_test_notify_unrecorded_move_still_taken(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_notify_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_decorated_client(&client);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;

    /* Some earlier request of the manager's own */
    client.layout.requested_pos.x = 400;
    client.layout.requested_pos.y = 300;
    client.layout.has_requested_pos = true;

    /* And a later move by one of those other paths, which wrote the
     * stored geometry without recording a request */
    client.layout.geometry.cur.pos.x = 50;
    client.layout.geometry.cur.pos.y = 60;

    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CONFIGURE_NOTIFY;
    event.event = client.frame;
    event.window = client.frame;
    event.x = 55;
    event.y = 66;
    event.width = (uint16_t) client.layout.geometry.cur.dim.w;
    event.height = (uint16_t) client.layout.geometry.cur.dim.h;

    handler_configure_notify(connection, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 55,
            "a notify for a move the manager never recorded is still"
            " believed");
    TAP_EQ_INT(client.layout.geometry.cur.pos.y, 66,
            "on both axes");
}


/* A client the manager has not placed yet has nothing to confirm
 * against, so whatever position the server reports is taken */
static void s_test_notify_unrequested_position_taken(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_notify_event_t event;
    list_td stages;
    xcb_connection_t *connection = (xcb_connection_t *) 0x1234;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_decorated_client(&client);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    client.layout.has_requested_pos = false;

    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CONFIGURE_NOTIFY;
    event.event = client.frame;
    event.window = client.frame;
    event.x = 777;
    event.y = 555;
    event.width = (uint16_t) client.layout.geometry.cur.dim.w;
    event.height = (uint16_t) client.layout.geometry.cur.dim.h;

    handler_configure_notify(connection, &stages, &event);

    TAP_EQ_INT(client.layout.geometry.cur.pos.x, 777,
            "a client with nothing requested yet takes the reported"
            " position");
}


/* A ConfigureNotify for the inner content window of a decorated,
 * non-fullscreen client, arriving at the wrong position within the
 * frame, is snapped back via a layout sync */
static void s_test_notify_inner_window_snapped_back(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_notify_event_t event;
    list_td stages;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_decorated_client(&client);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CONFIGURE_NOTIFY;
    event.event = client.window;
    event.window = client.window;
    /* Wrong position: should be (left, top) == (4, 24) */
    event.x = 0;
    event.y = 0;
    event.width = 200u;
    event.height = 100u;

    handler_configure_notify(NULL, &stages, &event);

    TAP_OK(s_call_layout_sync == 1u,
            "an inner-window ConfigureNotify at the wrong offset" \
            " triggers exactly one layout sync");
}


/* The same inner-window ConfigureNotify, already at the correct
 * offset within the frame, needs no correction at all */
static void s_test_notify_inner_window_already_correct(void)
{
    client_td client;
    stage_td stage;
    desktop_td desktop;
    xcb_configure_notify_event_t event;
    list_td stages;

    s_test_reset_state();
    memset(&stages, 0, sizeof(stages));
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_test_build_decorated_client(&client);
    s_lookup_result = &client;
    s_lookup_stage_out = &stage;
    s_lookup_desktop_out = &desktop;
    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CONFIGURE_NOTIFY;
    event.event = client.window;
    event.window = client.window;
    event.x = (int16_t) client.layout.frame_extents.left;
    event.y = (int16_t) client.layout.frame_extents.top;
    event.width = 200u;
    event.height = 100u;

    handler_configure_notify(NULL, &stages, &event);

    TAP_OK(s_call_layout_sync == 0u,
            "an inner-window ConfigureNotify already at the correct" \
            " offset triggers no layout sync");
}


int main(void)
{
    TAP_PLAN(54);

    s_test_request_null_event();
    s_test_request_systray_enforced();
    s_test_request_unmanaged_forwarded();
    s_test_request_plain_client_move();
    s_test_request_wm_owns_geometry();
    s_test_request_maximized_horz_ignores_x_honors_y();
    s_test_request_maximized_full_ignores_everything();
    s_test_request_wh_matches_current();
    s_test_request_shade_cooldown_suppresses();
    s_test_request_shade_cooldown_expired_honors();
    s_test_request_gravity_adjusts_position();
    s_test_request_stack_mode_enforces_layers();

    s_test_notify_null_event();
    s_test_notify_unmanaged();
    s_test_notify_substructure_ignored();
    s_test_notify_structure_size_changed();
    s_test_notify_structure_move_only();
    s_test_notify_stale_position_refused();
    s_test_notify_stale_size_refused();
    s_test_notify_unrequested_position_taken();
    s_test_notify_unrecorded_move_still_taken();
    s_test_notify_inner_window_snapped_back();
    s_test_notify_inner_window_already_correct();

    return TAP_DONE();
}
