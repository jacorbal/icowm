/**
 * @file tests/input/mouse/drag/test_icon.c
 *
 * @brief Test battery for icon-window drag start and its two pure
 *        query helpers (input/mouse/drag/icon.c)
 *
 * drag_icon_start, drag_is_icon_drag and drag_icon_height read and
 * write only the global drag state, s_drag (input/mouse/drag/
 * internal.h).  Storage for s_drag lives in drag.c, which this file
 * never links, so it is defined once here instead, the same way
 * test_resist.c and test_outline.c already do for this same
 * subsystem.  drag_overlay_hide and ri_render_client_icon are
 * recording stand-ins for their own whole subsystems (the drag
 * feedback overlay and icon rendering, respectively); raw
 * xcb_grab_pointer/xcb_grab_pointer_reply are controllable stand-ins
 * so a failed pointer grab can be exercised without a live X
 * connection.
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
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <cmds/surface.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag/icon.h>
#include <input/mouse/drag/internal.h>


/** Singleton drag state; storage normally lives in drag.c, which this
 *  file never links, so it is defined here instead */
drag_state_td s_drag;

/** Recorded calls to drag_overlay_hide and ri_render_client_icon */
static int s_overlay_hide_calls;
static int s_logger_msg_calls;
static int s_render_icon_calls;
static int s_viewport_drag_exclude_calls;
static client_td *s_viewport_drag_exclude_last_client;
static bool s_render_icon_last_is_current;
static bool s_render_icon_last_force;

/** Controllable outcome for the next xcb_grab_pointer_reply call */
static uint8_t s_stub_grab_status = XCB_GRAB_STATUS_SUCCESS;
static bool s_stub_grab_reply_null;
static int s_grab_pointer_calls;
static int s_grab_pointer_reply_calls;


/**
 * @brief Recording stand-in for @a logger_msg, backing the
 *        LOGGER_WARNING macro the failed-grab path calls
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;

    s_logger_msg_calls++;

    return 0;
}


/**
 * @brief Recording stand-in for @a drag_overlay_hide
 * @note Complexity: @e O(1)
 */
void drag_overlay_hide(xcb_connection_t *connection)
{
    (void) connection;

    s_overlay_hide_calls++;
}


/**
 * @brief Recording stand-in for @a scmd_surface_viewport_drag_exclude
 * @note Complexity: @e O(1)
 */
void scmd_surface_viewport_drag_exclude(client_td *client)
{
    s_viewport_drag_exclude_calls++;
    s_viewport_drag_exclude_last_client = client;
}


/**
 * @brief Recording stand-in for @a ri_render_client_icon
 * @note Complexity: @e O(1)
 */
void ri_render_client_icon(client_td *client, bool is_current,
        bool force)
{
    (void) client;

    s_render_icon_calls++;
    s_render_icon_last_is_current = is_current;
    s_render_icon_last_force = force;
}


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
    (void) cursor;
    (void) time;

    memset(&cookie, 0, sizeof(cookie));
    s_grab_pointer_calls++;

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

    s_grab_pointer_reply_calls++;

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


static void s_reset(void)
{
    memset(&s_drag, 0, sizeof(s_drag));
    s_overlay_hide_calls = 0;
    s_logger_msg_calls = 0;
    s_render_icon_calls = 0;
    s_render_icon_last_is_current = false;
    s_render_icon_last_force = false;
    s_stub_grab_status = XCB_GRAB_STATUS_SUCCESS;
    s_stub_grab_reply_null = false;
    s_grab_pointer_calls = 0;
    s_grab_pointer_reply_calls = 0;
    s_viewport_drag_exclude_calls = 0;
    s_viewport_drag_exclude_last_client = NULL;
}


/* Null connection is a no-op: nothing in s_drag is touched, no grab
 * is even attempted */
static void s_test_null_connection_is_noop(void)
{
    client_td client;
    desktop_td desktop;
    struct position_s icon_pos = { 10, 20 };
    struct position_s root_pos = { 100, 200 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));

    drag_icon_start(NULL, 1, &client, &desktop, icon_pos, 1000,
            root_pos, screen_dim);

    TAP_OK(!s_drag.is_active,
            "null connection: drag state is never activated");
    TAP_EQ_INT(s_grab_pointer_calls, 0,
            "and no pointer grab is even attempted");
}


/* Null client is likewise a no-op */
static void s_test_null_client_is_noop(void)
{
    desktop_td desktop;
    struct position_s icon_pos = { 10, 20 };
    struct position_s root_pos = { 100, 200 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    memset(&desktop, 0, sizeof(desktop));

    drag_icon_start((xcb_connection_t *) 1, 1, NULL, &desktop, icon_pos,
            1000, root_pos, screen_dim);

    TAP_OK(!s_drag.is_active, "null client: drag never activates");
}


/* A successful grab populates the whole drag state for an icon drag,
 * marks the client's icon mapped, and forces an immediate repaint as
 * the now-selected icon */
static void s_test_successful_grab_populates_drag_state(void)
{
    client_td client;
    desktop_td desktop;
    struct position_s icon_pos = { 10, 20 };
    struct position_s root_pos = { 100, 200 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));
    client.icon_window = 77u;
    client.is_icon_mapped = false;
    s_stub_grab_status = XCB_GRAB_STATUS_SUCCESS;

    drag_icon_start((xcb_connection_t *) 1, 55u, &client, &desktop,
            icon_pos, 1234, root_pos, screen_dim);

    TAP_OK(s_drag.is_active, "successful grab: drag state is active");
    TAP_OK(s_drag.client == &client, "the client pointer is recorded");
    TAP_OK(s_drag.desktop == &desktop, "as is the desktop");
    TAP_OK(s_drag.drag_window == client.icon_window,
            "drag_window is the client's icon window, not its frame");
    TAP_OK(s_drag.operation == CLIENT_OPERATION_MOVING,
            "an icon drag is always a move");
    TAP_EQ_INT(s_drag.pointer_start_x, 100, "pointer start X recorded");
    TAP_EQ_INT(s_drag.pointer_start_y, 200, "pointer start Y recorded");
    TAP_EQ_INT(s_drag.client_start.pos.x, 10,
            "starting geometry position X is the icon position, not"
            " the pointer position");
    TAP_EQ_INT(s_drag.client_start.pos.y, 20,
            "starting geometry position Y likewise");
    TAP_EQ_INT((int) s_drag.client_start.dim.w, 0,
            "icon drags never carry a starting width");
    TAP_EQ_INT((int) s_drag.client_start.dim.h, 0,
            "nor a starting height");
    TAP_OK(s_drag.client_cur.pos.x == icon_pos.x &&
            s_drag.client_cur.pos.y == icon_pos.y,
            "the current geometry position starts equal to the icon"
            " position too");
    TAP_OK(s_drag.is_solid_drag,
            "icon drags are always solid regardless of any config");
    TAP_EQ_INT((int) s_drag.screen_w, 1920, "screen width recorded");
    TAP_EQ_INT((int) s_drag.screen_h, 1080, "screen height recorded");
    TAP_OK(!s_drag.was_icon_mapped,
            "the icon's mapped state from before the drag is"
            " snapshotted (was false here)");
    TAP_OK(!s_drag.is_anchor_right && !s_drag.is_anchor_bottom &&
            !s_drag.is_resize_w && !s_drag.is_resize_h,
            "none of the resize-only fields are ever set for an icon"
            " drag");
    TAP_OK(!s_drag.has_last_pos,
            "has_last_pos starts false so the first drag_update"
            " always runs");
    TAP_OK(!s_drag.is_warp_pending, "no warp is pending yet");
    TAP_OK(client.properties.operation == CLIENT_OPERATION_MOVING,
            "the client's own operation field mirrors the drag state");
    TAP_OK(client.is_icon_mapped,
            "the icon is force-mapped for the duration of the drag");
    TAP_EQ_INT(s_render_icon_calls, 1,
            "an immediate icon repaint is triggered");
    TAP_OK(s_render_icon_last_is_current,
            "painted as the now-current (selected) icon");
    TAP_OK(s_render_icon_last_force, "and forced rather than deferred");
    TAP_EQ_INT(s_overlay_hide_calls, 1,
            "any leftover move/resize feedback overlay is hidden"
            " first");
}


/* A failed grab (bad status) leaves is_active false again after
 * having briefly set it, and does not crash freeing the reply */
static void s_test_failed_grab_status_deactivates(void)
{
    client_td client;
    desktop_td desktop;
    struct position_s icon_pos = { 0, 0 };
    struct position_s root_pos = { 0, 0 };
    struct dimensions_s screen_dim = { 800u, 600u };

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));
    s_stub_grab_status = XCB_GRAB_STATUS_ALREADY_GRABBED;

    drag_icon_start((xcb_connection_t *) 1, 1u, &client, &desktop,
            icon_pos, 1, root_pos, screen_dim);

    TAP_OK(!s_drag.is_active,
            "grab reports a non-success status: drag is deactivated"
            " again");
    TAP_OK(s_drag.client == NULL,
            "client pointer cleared too, not left dangling");
    TAP_OK(s_viewport_drag_exclude_last_client == NULL,
            "viewport-pan exclusion cleared, not left stuck on a"
            " client no drag is actually holding");
}


/* A null reply from xcb_grab_pointer_reply (allocation or protocol
 * failure) is handled the same way as a bad status, without a crash */
static void s_test_null_grab_reply_deactivates(void)
{
    client_td client;
    desktop_td desktop;
    struct position_s icon_pos = { 0, 0 };
    struct position_s root_pos = { 0, 0 };
    struct dimensions_s screen_dim = { 800u, 600u };

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&desktop, 0, sizeof(desktop));
    s_stub_grab_reply_null = true;

    drag_icon_start((xcb_connection_t *) 1, 1u, &client, &desktop,
            icon_pos, 1, root_pos, screen_dim);

    TAP_OK(!s_drag.is_active,
            "null grab reply: drag is deactivated, not left active"
            " with a dangling status read");
    TAP_OK(s_drag.client == NULL,
            "client pointer cleared too, not left dangling");
    TAP_OK(s_viewport_drag_exclude_last_client == NULL,
            "viewport-pan exclusion cleared, not left stuck on a"
            " client no drag is actually holding");
}


/* drag_is_icon_drag is false while no drag is active at all */
static void s_test_is_icon_drag_false_when_inactive(void)
{
    s_reset();
    s_drag.is_active = false;

    TAP_OK(!drag_is_icon_drag(),
            "no active drag: never reported as an icon drag");
}


/* drag_is_icon_drag is false for an active drag whose window is the
 * client's frame, not its icon window (an ordinary move/resize) */
static void s_test_is_icon_drag_false_for_frame_drag(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.icon_window = 42u;
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.drag_window = 99u;

    TAP_OK(!drag_is_icon_drag(),
            "active drag on the frame, not the icon window: not an"
            " icon drag");
}


/* drag_is_icon_drag is true only when the drag window matches the
 * client's own icon window */
static void s_test_is_icon_drag_true_when_matching(void)
{
    client_td client;

    s_reset();
    memset(&client, 0, sizeof(client));
    client.icon_window = 42u;
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.drag_window = 42u;

    TAP_OK(drag_is_icon_drag(),
            "active drag whose window is the client's icon window:"
            " reported as an icon drag");
}


/* drag_icon_height returns the plain square size for a null client */
static void s_test_icon_height_null_client_returns_plain_size(void)
{
    TAP_EQ_INT((int) drag_icon_height(NULL),
            (int) WM_ICON_SQUARE_SIZE,
            "null client: plain square size, no caption added");
}


/* drag_icon_height returns the plain square size for a client with no
 * config either */
static void s_test_icon_height_null_config_returns_plain_size(void)
{
    client_td client;

    memset(&client, 0, sizeof(client));
    client.config = NULL;

    TAP_EQ_INT((int) drag_icon_height(&client),
            (int) WM_ICON_SQUARE_SIZE,
            "client with no config: plain square size");
}


/* drag_icon_height adds the caption height when the theme requests
 * captioned icons */
static void s_test_icon_height_captioned_adds_caption(void)
{
    client_td client;
    config_td config;

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    config.theme.icon.is_captioned = true;
    client.config = &config;

    TAP_EQ_INT((int) drag_icon_height(&client),
            (int) (WM_ICON_SQUARE_SIZE + WM_ICON_CAPTION_HEIGHT),
            "captioned theme: square size plus caption height");
}


/* drag_icon_height omits the caption height when the theme does not
 * request captioned icons */
static void s_test_icon_height_uncaptioned_omits_caption(void)
{
    client_td client;
    config_td config;

    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    config.theme.icon.is_captioned = false;
    client.config = &config;

    TAP_EQ_INT((int) drag_icon_height(&client),
            (int) WM_ICON_SQUARE_SIZE,
            "uncaptioned theme: plain square size only");
}


int main(void)
{
    TAP_PLAN(41);

    s_test_null_connection_is_noop();
    s_test_null_client_is_noop();
    s_test_successful_grab_populates_drag_state();
    s_test_failed_grab_status_deactivates();
    s_test_null_grab_reply_deactivates();
    s_test_is_icon_drag_false_when_inactive();
    s_test_is_icon_drag_false_for_frame_drag();
    s_test_is_icon_drag_true_when_matching();
    s_test_icon_height_null_client_returns_plain_size();
    s_test_icon_height_null_config_returns_plain_size();
    s_test_icon_height_captioned_adds_caption();
    s_test_icon_height_uncaptioned_omits_caption();

    return TAP_DONE();
}
