/**
 * @file tests/input/mouse/drag/test_drag.c
 *
 * @brief Test battery for the mouse drag-operation public state
 *        machine (input/mouse/drag.c)
 *
 * drag_start/drag_start_directed/drag_start_resize_axis_locked/
 * drag_update/drag_end/drag_cancel/drag_is_active/drag_client all
 * read and write the module's own singleton drag state, s_drag,
 * whose storage is defined right here in drag.c itself; this file
 * links the real drag.c, so it never redefines s_drag the way its
 * sibling files under this same directory do.  Every sibling
 * drag subdirectory header collaborator (icon.h, outline.h,
 * overlay.h, resist.h, snap.h, warp.h) is a cross-module command
 * whose own math/behavior
 * is already fully covered by its own dedicated test file
 * (test_icon.c, test_outline.c, test_overlay.c, test_resist.c,
 * test_snap.c, test_warp.c all under this same directory), so each is
 * a plain recording stand-in here rather than exercised again.
 * enact_client_move/enact_client_resize/enact_client_resize_force/
 * enact_client_restore, focus_apply, systray_get_geometry/
 * systray_restack, place_icon_avoid_systray_overlap,
 * client_size_constrain, im_bounds_resize, mouse_cursor_move/
 * mouse_resize_cursor_for_axes and wm_request_client_redraw are each
 * a whole separate module's own public entry point, so they are
 * recording or controllable stand-ins too.  The raw XCB pointer-grab
 * and window-move requests are stubbed directly, the same convention
 * test_icon.c and tests/enact/test_send_to_desktop.c already use,
 * since no live X connection is used.
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

/* Types includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <cmds/stage.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <policy/focus.h>
#include <policy/placement/icon.h>
#include <stage.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/bounds.h>
#include <input/mouse/cursor.h>
#include <input/mouse/drag.h>
#include <input/mouse/drag/icon.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/outline.h>
#include <input/mouse/drag/overlay.h>
#include <input/mouse/drag/resist.h>
#include <input/mouse/drag/snap.h>
#include <input/mouse/drag/warp.h>


/** Recorded calls to every collaborator stand-in this file defines */
static int s_grab_pointer_calls;
static int s_grab_pointer_reply_calls;
static uint8_t s_stub_grab_status;
static bool s_stub_grab_reply_null;
static int s_change_active_grab_calls;
static int s_ungrab_pointer_calls;
static int s_flush_calls;
static int s_clear_area_calls;
static int s_window_move_calls;
static xcb_window_t s_window_move_last_window;
static int32_t s_window_move_last_x;
static int32_t s_window_move_last_y;

static int s_overlay_hide_calls;
static int s_overlay_show_calls;
static int s_outline_start_calls;
static int s_outline_move_calls;
static int s_outline_end_calls;
static int s_resist_update_calls;
static int s_resist_finalize_calls;
static bool s_resist_finalize_last_arg;
static int s_snap_move_calls;
static int s_snap_resize_calls;
static int s_warp_edge_check_calls;
static int s_pan_edge_check_calls;
static int s_icon_height_calls;

static int s_enact_move_calls;
static int s_enact_resize_calls;
static int s_enact_resize_force_calls;
static int s_enact_restore_calls;
static struct geometry_s s_enact_resize_last_geom;
static struct geometry_s s_enact_resize_force_last_geom;
static struct position_s s_enact_move_last_pos;

static int s_focus_apply_calls;
static int s_systray_get_geometry_calls;
static bool s_stub_systray_has_geometry;
static struct geometry_s s_stub_systray_geometry;
static int s_systray_restack_calls;
static int s_place_icon_avoid_calls;
static bool s_stub_place_icon_pushed;
static int s_size_constrain_calls;
static int s_bounds_resize_calls;
static im_resize_bounds_td s_stub_bounds_resize;
static int s_redraw_calls;
static int s_logger_msg_calls;
static int s_viewport_drag_exclude_calls;
static client_td *s_viewport_drag_exclude_last_client;


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


/**
 * @brief Recording stand-in for the raw
 *        @a xcb_change_active_pointer_grab request
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_change_active_pointer_grab(xcb_connection_t *c,
        xcb_cursor_t cursor, xcb_timestamp_t time, uint16_t event_mask)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) cursor;
    (void) time;
    (void) event_mask;

    memset(&cookie, 0, sizeof(cookie));
    s_change_active_grab_calls++;

    return cookie;
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
 * @brief Recording stand-in for the raw @a xcb_flush request
 * @note Complexity: @e O(1)
 */
int xcb_flush(xcb_connection_t *c)
{
    (void) c;

    s_flush_calls++;

    return 1;
}


/**
 * @brief Recording stand-in for the raw @a xcb_clear_area request
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_clear_area(xcb_connection_t *c, uint8_t exposures,
        xcb_window_t window, int16_t x, int16_t y, uint16_t width,
        uint16_t height)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) exposures;
    (void) window;
    (void) x;
    (void) y;
    (void) width;
    (void) height;

    memset(&cookie, 0, sizeof(cookie));
    s_clear_area_calls++;

    return cookie;
}


/**
 * @brief Recording stand-in for the project's @a xcb_window_move
 *        wrapper
 * @note Complexity: @e O(1)
 */
void xcb_window_move(xcb_window_t window, int32_t x, int32_t y)
{
    s_window_move_calls++;
    s_window_move_last_window = window;
    s_window_move_last_x = x;
    s_window_move_last_y = y;
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
 * @brief Recording stand-in for @a drag_overlay_show
 * @note Complexity: @e O(1)
 */
void drag_overlay_show(xcb_connection_t *connection, bool is_icon,
        struct geometry_s target, const char *text)
{
    (void) connection;
    (void) is_icon;
    (void) target;
    (void) text;

    s_overlay_show_calls++;
}


/**
 * @brief Recording stand-in for @a drag_outline_start
 * @note Complexity: @e O(1)
 */
void drag_outline_start(xcb_connection_t *connection,
        struct geometry_s geom)
{
    (void) connection;
    (void) geom;

    s_outline_start_calls++;
}


/**
 * @brief Recording stand-in for @a drag_outline_move
 * @note Complexity: @e O(1)
 */
void drag_outline_move(xcb_connection_t *connection,
        struct geometry_s geom)
{
    (void) connection;
    (void) geom;

    s_outline_move_calls++;
}


/**
 * @brief Recording stand-in for @a drag_outline_end
 * @note Complexity: @e O(1)
 */
void drag_outline_end(xcb_connection_t *connection)
{
    (void) connection;

    s_outline_end_calls++;
}


/**
 * @brief Recording stand-in for @a drag_resist_axis_update
 * @note Complexity: @e O(1)
 */
void drag_resist_axis_update(uint32_t drag_dist_w, uint32_t drag_dist_h,
        uint32_t resistance)
{
    (void) drag_dist_w;
    (void) drag_dist_h;
    (void) resistance;

    s_resist_update_calls++;
}


/**
 * @brief Recording stand-in for @a drag_resist_axis_finalize
 * @note Complexity: @e O(1)
 */
void drag_resist_axis_finalize(bool finalize_resize)
{
    s_resist_finalize_calls++;
    s_resist_finalize_last_arg = finalize_resize;
}


/**
 * @brief Recording stand-in for @a drag_snap_move
 * @note Complexity: @e O(1)
 */
void drag_snap_move(int32_t *restrict x, int32_t *restrict y,
        uint32_t width, uint32_t height)
{
    (void) x;
    (void) y;
    (void) width;
    (void) height;

    s_snap_move_calls++;
}


/**
 * @brief Recording stand-in for @a drag_snap_resize
 * @note Complexity: @e O(1)
 */
void drag_snap_resize(int32_t *restrict x, int32_t *restrict y,
        uint32_t *restrict width, uint32_t *restrict height)
{
    (void) x;
    (void) y;
    (void) width;
    (void) height;

    s_snap_resize_calls++;
}


/**
 * @brief Recording stand-in for @a drag_warp_edge_check
 * @note Complexity: @e O(1)
 */
void drag_warp_edge_check(int16_t root_x, int16_t root_y)
{
    (void) root_x;
    (void) root_y;

    s_warp_edge_check_calls++;
}


/**
 * @brief Recording stand-in for @a drag_pan_edge_check
 * @note Complexity: @e O(1)
 */
void drag_pan_edge_check(int16_t root_x, int16_t root_y)
{
    (void) root_x;
    (void) root_y;

    s_pan_edge_check_calls++;
}


/**
 * @brief Controllable stand-in for @a drag_icon_height
 * @note Complexity: @e O(1)
 */
uint16_t drag_icon_height(const client_td *client)
{
    (void) client;

    s_icon_height_calls++;

    return WM_ICON_SQUARE_SIZE;
}


/**
 * @brief Recording stand-in for @a enact_client_move
 * @note Complexity: @e O(1)
 */
void enact_client_move(client_td *client, struct position_s pos)
{
    (void) client;

    s_enact_move_calls++;
    s_enact_move_last_pos = pos;
}


/**
 * @brief Recording stand-in for @a scmd_stage_viewport_drag_exclude
 * @note Complexity: @e O(1)
 */
void scmd_stage_viewport_drag_exclude(client_td *client)
{
    s_viewport_drag_exclude_calls++;
    s_viewport_drag_exclude_last_client = client;
}


/**
 * @brief Recording stand-in for @a enact_client_resize
 * @note Complexity: @e O(1)
 */
void enact_client_resize(client_td *client, struct geometry_s geom)
{
    (void) client;

    s_enact_resize_calls++;
    s_enact_resize_last_geom = geom;
}


/**
 * @brief Recording stand-in for @a enact_client_resize_force
 * @note Complexity: @e O(1)
 */
void enact_client_resize_force(client_td *client, struct geometry_s geom)
{
    (void) client;

    s_enact_resize_force_calls++;
    s_enact_resize_force_last_geom = geom;
}


/**
 * @brief Recording stand-in for @a enact_client_restore
 * @note Complexity: @e O(1)
 */
void enact_client_restore(client_td *client)
{
    (void) client;

    s_enact_restore_calls++;
}


/**
 * @brief Recording stand-in for @a focus_apply
 * @note Complexity: @e O(1)
 */
void focus_apply(list_td *stages, stage_td *stage,
        desktop_td *desktop, client_td *client, bool raise,
        const config_td *cfg)
{
    (void) stages;
    (void) stage;
    (void) desktop;
    (void) client;
    (void) raise;
    (void) cfg;

    s_focus_apply_calls++;
}


/**
 * @brief Controllable stand-in for @a systray_get_geometry
 * @note Complexity: @e O(1)
 */
bool systray_get_geometry(const stage_td *stage,
        struct geometry_s *restrict out_tray)
{
    (void) stage;

    s_systray_get_geometry_calls++;

    if (s_stub_systray_has_geometry && out_tray != NULL) {
        *out_tray = s_stub_systray_geometry;
    }

    return s_stub_systray_has_geometry;
}


/**
 * @brief Recording stand-in for @a systray_restack
 * @note Complexity: @e O(1)
 */
void systray_restack(void)
{
    s_systray_restack_calls++;
}


/**
 * @brief Controllable stand-in for @a place_icon_avoid_systray_overlap
 * @note Complexity: @e O(1)
 */
bool place_icon_avoid_systray_overlap(
        const int16_t *restrict io_x, int16_t *restrict io_y,
        struct dimensions_s icon_dim, struct geometry_s tray,
        const struct geometry_s *workarea)
{
    (void) io_x;
    (void) io_y;
    (void) icon_dim;
    (void) tray;
    (void) workarea;

    s_place_icon_avoid_calls++;

    return s_stub_place_icon_pushed;
}


/**
 * @brief Recording stand-in for @a client_size_constrain
 * @note Complexity: @e O(1)
 */
void client_size_constrain(const client_td *client,
        uint32_t *restrict width, uint32_t *restrict height)
{
    (void) client;
    (void) width;
    (void) height;

    s_size_constrain_calls++;
}


/**
 * @brief Controllable stand-in for @a im_bounds_resize
 * @note Complexity: @e O(1)
 */
im_resize_bounds_td im_bounds_resize(const client_td *client)
{
    (void) client;

    s_bounds_resize_calls++;

    return s_stub_bounds_resize;
}


/**
 * @brief Recording stand-in for @a wm_request_client_redraw
 * @note Complexity: @e O(1)
 */
void wm_request_client_redraw(client_td *client)
{
    (void) client;

    s_redraw_calls++;
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
 * @brief Controllable stand-in for @a mouse_resize_cursor_for_axes
 * @note Complexity: @e O(1)
 */
xcb_cursor_t mouse_resize_cursor_for_axes(bool resize_w, bool resize_h,
        bool anchor_right, bool anchor_bottom)
{
    (void) resize_w;
    (void) resize_h;
    (void) anchor_right;
    (void) anchor_bottom;

    return 2u;
}


/**
 * @brief Recording stand-in for @a logger_msg
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


static void s_reset(void)
{
    memset(&s_drag, 0, sizeof(s_drag));
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.overlay_window = XCB_WINDOW_NONE;
    s_drag.root = XCB_WINDOW_NONE;

    s_grab_pointer_calls = 0;
    s_grab_pointer_reply_calls = 0;
    s_stub_grab_status = XCB_GRAB_STATUS_SUCCESS;
    s_stub_grab_reply_null = false;
    s_change_active_grab_calls = 0;
    s_ungrab_pointer_calls = 0;
    s_flush_calls = 0;
    s_clear_area_calls = 0;
    s_window_move_calls = 0;
    s_window_move_last_window = XCB_WINDOW_NONE;
    s_window_move_last_x = 0;
    s_window_move_last_y = 0;

    s_overlay_hide_calls = 0;
    s_overlay_show_calls = 0;
    s_outline_start_calls = 0;
    s_outline_move_calls = 0;
    s_outline_end_calls = 0;
    s_resist_update_calls = 0;
    s_resist_finalize_calls = 0;
    s_resist_finalize_last_arg = false;
    s_snap_move_calls = 0;
    s_snap_resize_calls = 0;
    s_warp_edge_check_calls = 0;
    s_pan_edge_check_calls = 0;
    s_icon_height_calls = 0;

    s_enact_move_calls = 0;
    s_enact_resize_calls = 0;
    s_enact_resize_force_calls = 0;
    s_enact_restore_calls = 0;
    memset(&s_enact_resize_last_geom, 0, sizeof(s_enact_resize_last_geom));
    memset(&s_enact_resize_force_last_geom, 0,
            sizeof(s_enact_resize_force_last_geom));
    memset(&s_enact_move_last_pos, 0, sizeof(s_enact_move_last_pos));

    s_focus_apply_calls = 0;
    s_systray_get_geometry_calls = 0;
    s_stub_systray_has_geometry = false;
    memset(&s_stub_systray_geometry, 0, sizeof(s_stub_systray_geometry));
    s_systray_restack_calls = 0;
    s_place_icon_avoid_calls = 0;
    s_stub_place_icon_pushed = false;
    s_size_constrain_calls = 0;
    s_bounds_resize_calls = 0;
    memset(&s_stub_bounds_resize, 0, sizeof(s_stub_bounds_resize));
    s_redraw_calls = 0;
    s_logger_msg_calls = 0;
    s_viewport_drag_exclude_calls = 0;
    s_viewport_drag_exclude_last_client = NULL;
}


static void s_make_client(client_td *client)
{
    memset(client, 0, sizeof(*client));
    client->window = 10u;
    client->frame = 11u;
    client->icon_window = 12u;
    client->layout.geometry.cur.pos.x = 100;
    client->layout.geometry.cur.pos.y = 200;
    client->layout.geometry.cur.dim.w = 300u;
    client->layout.geometry.cur.dim.h = 150u;
}


/* drag_start with a null connection or a null client is a no-op:
 * s_drag.is_active stays false and no pointer grab is even
 * attempted */
static void s_test_start_null_guards_are_noop(void)
{
    client_td client;
    struct position_s root_pos = { 150, 250 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);

    drag_start(NULL, 1u, &client, NULL, CLIENT_OPERATION_MOVING, 1,
            root_pos, screen_dim);
    TAP_OK(!s_drag.is_active, "null connection: never activates");
    TAP_EQ_INT(s_grab_pointer_calls, 0,
            "and no pointer grab is even attempted");

    s_reset();
    drag_start((xcb_connection_t *) 1, 1u, NULL, NULL,
            CLIENT_OPERATION_MOVING, 1, root_pos, screen_dim);
    TAP_OK(!s_drag.is_active, "null client: never activates either");
}


/* A successful move-drag start populates the drag state's geometry
 * snapshot from the client's own current geometry, grabs the
 * pointer, and flushes the connection once */
static void s_test_start_move_success_populates_state(void)
{
    client_td client;
    struct position_s root_pos = { 150, 250 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);
    s_stub_grab_status = XCB_GRAB_STATUS_SUCCESS;

    drag_start((xcb_connection_t *) 1, 55u, &client, NULL,
            CLIENT_OPERATION_MOVING, 1234, root_pos, screen_dim);

    TAP_OK(s_drag.is_active, "successful grab: drag becomes active");
    TAP_OK(s_drag.client == &client, "client pointer recorded");
    TAP_OK(s_drag.operation == CLIENT_OPERATION_MOVING,
            "operation recorded as moving");
    TAP_EQ_INT(s_drag.pointer_start_x, 150, "pointer start X recorded");
    TAP_EQ_INT(s_drag.pointer_start_y, 250, "pointer start Y recorded");
    TAP_EQ_INT(s_drag.client_start.pos.x, 100,
            "client_start position X taken from the client's own"
            " current geometry");
    TAP_EQ_INT((int) s_drag.client_start.dim.w, 300,
            "client_start width likewise");
    TAP_OK(s_drag.root == 55u, "root window recorded");
    TAP_OK(!s_drag.is_anchor_right && !s_drag.is_anchor_bottom &&
            !s_drag.is_resize_w && !s_drag.is_resize_h,
            "a move drag never sets any of the resize-only anchor/"
            "axis fields");
    TAP_OK((uint16_t) client.properties.operation ==
            (uint16_t) CLIENT_OPERATION_MOVING,
            "the client's own operation field mirrors the drag's");
    TAP_OK(!client.has_rule_position_locked,
            "a user-initiated move clears any rule-locked position");
    TAP_EQ_INT(s_grab_pointer_calls, 1, "exactly one grab is attempted");
    TAP_EQ_INT(s_flush_calls, 1,
            "the connection is flushed once after a successful grab");
    TAP_EQ_INT(s_outline_start_calls, 0,
            "a solid drag (the default, no config attached) never"
            " starts an outline");
}


/* A resize-drag start near the top-left corner locks in a top-left
 * anchor and resizes both axes */
static void s_test_start_resize_infers_top_left_anchor(void)
{
    client_td client;
    struct position_s root_pos = { 105, 205 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);
    s_stub_bounds_resize.left = 100;
    s_stub_bounds_resize.top = 200;
    s_stub_bounds_resize.right = 400;
    s_stub_bounds_resize.bottom = 350;
    s_stub_bounds_resize.margin_left = 10;
    s_stub_bounds_resize.margin_top = 10;
    s_stub_bounds_resize.margin_right = 10;
    s_stub_bounds_resize.margin_bottom = 10;

    drag_start((xcb_connection_t *) 1, 55u, &client, NULL,
            CLIENT_OPERATION_RESIZING, 1, root_pos, screen_dim);

    TAP_OK(s_drag.is_active, "resize drag near a corner: still active");
    TAP_OK(s_drag.is_anchor_right,
            "grab point near the left margin: right edge is the fixed"
            " anchor");
    TAP_OK(s_drag.is_anchor_bottom,
            "grab point near the top margin: bottom edge is the fixed"
            " anchor");
    TAP_OK(s_drag.is_resize_w && s_drag.is_resize_h,
            "both axes are actively resized from a corner grab");
    TAP_EQ_INT(s_bounds_resize_calls, 1,
            "im_bounds_resize is consulted exactly once to infer the"
            " anchor");
}


/* A resize-drag start with the grab point squarely in the middle,
 * away from every margin, still resizes both axes (the fallback for
 * a grab that hit neither edge zone) */
static void s_test_start_resize_middle_grab_resizes_both_axes(void)
{
    client_td client;
    struct position_s root_pos = { 250, 275 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);
    s_stub_bounds_resize.left = 100;
    s_stub_bounds_resize.top = 200;
    s_stub_bounds_resize.right = 400;
    s_stub_bounds_resize.bottom = 350;
    s_stub_bounds_resize.margin_left = 10;
    s_stub_bounds_resize.margin_top = 10;
    s_stub_bounds_resize.margin_right = 10;
    s_stub_bounds_resize.margin_bottom = 10;

    drag_start((xcb_connection_t *) 1, 55u, &client, NULL,
            CLIENT_OPERATION_RESIZING, 1, root_pos, screen_dim);

    TAP_OK(s_drag.is_resize_w && s_drag.is_resize_h,
            "neither margin zone hit: both axes still end up active"
            " (the center fallback)");
}


/* A move-drag start on a client already maximized horizontally locks
 * the X axis for the whole drag; likewise for vertical/Y */
static void s_test_start_move_locks_maximized_axis(void)
{
    client_td client;
    struct position_s root_pos = { 150, 250 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);
    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;

    drag_start((xcb_connection_t *) 1, 55u, &client, NULL,
            CLIENT_OPERATION_MOVING, 1, root_pos, screen_dim);

    TAP_OK(s_drag.is_move_x_locked,
            "horizontally-maximized client: the X axis is locked for"
            " this move drag");
    TAP_OK(!s_drag.is_move_y_locked,
            "but the Y axis, not maximized, is not");
}


/* A failed grab (bad status) rolls the drag state back to inactive
 * and logs a warning, without leaking the reply */
static void s_test_start_failed_grab_status_rolls_back(void)
{
    client_td client;
    struct position_s root_pos = { 150, 250 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);
    s_stub_grab_status = XCB_GRAB_STATUS_ALREADY_GRABBED;

    drag_start((xcb_connection_t *) 1, 55u, &client, NULL,
            CLIENT_OPERATION_MOVING, 1, root_pos, screen_dim);

    TAP_OK(!s_drag.is_active,
            "grab reports a non-success status: rolled back to"
            " inactive");
    TAP_OK(s_logger_msg_calls > 0,
            "a warning is logged for the failed grab");
    TAP_OK(s_drag.client == NULL,
            "client pointer cleared too, not left dangling");
    TAP_OK(s_viewport_drag_exclude_last_client == NULL,
            "viewport-pan exclusion cleared, not left stuck on a"
            " client no drag is actually holding");
}


/* A null grab reply is handled the same way as a bad status */
static void s_test_start_null_grab_reply_rolls_back(void)
{
    client_td client;
    struct position_s root_pos = { 150, 250 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);
    s_stub_grab_reply_null = true;

    drag_start((xcb_connection_t *) 1, 55u, &client, NULL,
            CLIENT_OPERATION_MOVING, 1, root_pos, screen_dim);

    TAP_OK(!s_drag.is_active,
            "null grab reply: rolled back to inactive, not left"
            " active with a dangling status read");
    TAP_OK(s_drag.client == NULL,
            "client pointer cleared too, not left dangling");
    TAP_OK(s_viewport_drag_exclude_last_client == NULL,
            "viewport-pan exclusion cleared, not left stuck on a"
            " client no drag is actually holding");
}


/* drag_start_directed overrides drag_start's inferred anchor/axes
 * with the caller's explicit choice, and updates the live grab's
 * cursor to match */
static void s_test_start_directed_overrides_anchor(void)
{
    client_td client;
    struct position_s root_pos = { 250, 275 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);

    drag_start_directed((xcb_connection_t *) 1, 55u, &client, NULL, 1,
            root_pos, screen_dim, true, false, true, false);

    TAP_OK(s_drag.is_active, "directed start: drag is active");
    TAP_OK(s_drag.is_anchor_right,
            "explicit anchor_right overrides whatever drag_start"
            " itself would have inferred");
    TAP_OK(!s_drag.is_anchor_bottom, "as does anchor_bottom");
    TAP_OK(s_drag.is_resize_w && !s_drag.is_resize_h,
            "as do the explicit resize_w/resize_h axis flags");
    TAP_EQ_INT(s_change_active_grab_calls, 1,
            "the already-active grab's cursor is updated to match the"
            " overridden direction");
}


/* drag_start_directed does nothing further when the underlying
 * drag_start itself failed to activate */
static void s_test_start_directed_noop_when_start_fails(void)
{
    client_td client;
    struct position_s root_pos = { 250, 275 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);
    s_stub_grab_reply_null = true;

    drag_start_directed((xcb_connection_t *) 1, 55u, &client, NULL, 1,
            root_pos, screen_dim, true, false, true, false);

    TAP_OK(!s_drag.is_active,
            "the underlying grab failed: still inactive");
    TAP_EQ_INT(s_change_active_grab_calls, 0,
            "and the cursor is never updated for a drag that never"
            " started");
}


/* drag_start_resize_axis_locked locks out the requested axis (axes),
 * leaving the other one alone */
static void s_test_start_axis_locked_locks_one_axis(void)
{
    client_td client;
    struct position_s root_pos = { 105, 205 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);
    s_stub_bounds_resize.left = 100;
    s_stub_bounds_resize.top = 200;
    s_stub_bounds_resize.right = 400;
    s_stub_bounds_resize.bottom = 350;
    s_stub_bounds_resize.margin_left = 10;
    s_stub_bounds_resize.margin_top = 10;
    s_stub_bounds_resize.margin_right = 10;
    s_stub_bounds_resize.margin_bottom = 10;

    drag_start_resize_axis_locked((xcb_connection_t *) 1, 55u, &client,
            NULL, 1, root_pos, screen_dim, true, false);

    TAP_OK(s_drag.is_active,
            "one axis locked, the other still free: drag stays"
            " active");
    TAP_OK(s_drag.is_resist_axis_w,
            "the locked width axis is flagged resistant");
    TAP_OK(!s_drag.is_resize_w,
            "and is_resize_w itself starts false for it");
    TAP_OK(s_drag.is_resize_h,
            "the untouched height axis keeps whatever drag_start"
            " itself inferred (still active here, near the top"
            " margin)");
}


/* drag_start_resize_axis_locked's own dead-drag cancel branch
 * (neither axis resizable nor resistant) is unreachable in practice
 * given drag_start's own guarantee that at least one axis always
 * starts active (its fallback forces both true whenever neither
 * margin zone was hit; see drag_start's own comment on that exact
 * fallback) and that is_resist_axis_w/h here are set to whichever of
 * axis_w_locked/axis_h_locked the caller passed in directly: setting
 * a lock argument true is precisely what would be needed to force
 * the corresponding is_resize_* false, yet doing so also forces the
 * matching is_resist_axis_* true, and leaving both lock arguments
 * false leaves whichever axis drag_start already activated
 * untouched.  No combination of the two boolean parameters can
 * reach all four flags false at once.  This still exercises every
 * other branch of the same function with a genuinely single locked
 * axis, immediately below. */
static void s_test_start_axis_locked_single_axis_never_dead(void)
{
    client_td client;
    struct position_s root_pos = { 105, 275 };
    struct dimensions_s screen_dim = { 1920u, 1080u };

    s_reset();
    s_make_client(&client);
    s_stub_bounds_resize.left = 100;
    s_stub_bounds_resize.top = 200;
    s_stub_bounds_resize.right = 400;
    s_stub_bounds_resize.bottom = 350;
    s_stub_bounds_resize.margin_left = 10;
    s_stub_bounds_resize.margin_top = 10;
    s_stub_bounds_resize.margin_right = 10;
    s_stub_bounds_resize.margin_bottom = 10;

    /* Grab near the left margin alone: drag_start infers is_resize_w
     * true (the only axis near any edge) and is_resize_h false (the Y
     * coordinate is nowhere near the top or bottom margin).  Locking
     * out just the W axis leaves is_resize_w false but is_resist_axis_w
     * true, which alone keeps this drag out of the dead-cancel
     * branch. */
    drag_start_resize_axis_locked((xcb_connection_t *) 1, 55u, &client,
            NULL, 1, root_pos, screen_dim, true, false);

    TAP_OK(s_drag.is_active,
            "the locked axis being resistant, not merely inactive,"
            " keeps this drag alive rather than cancelling it");
    TAP_OK(!s_drag.is_resize_w && s_drag.is_resist_axis_w,
            "the locked W axis itself is inactive but flagged"
            " resistant");
    TAP_OK(!s_drag.is_resize_h && !s_drag.is_resist_axis_h,
            "the untouched H axis was never active to begin with and"
            " is not resistant either");
}


/* drag_update with a null connection, no active drag, or no client
 * attached is a pure no-op */
static void s_test_update_null_guards_are_noop(void)
{
    client_td client;
    struct position_s root_pos = { 160, 260 };

    s_reset();
    s_make_client(&client);
    s_drag.is_active = true;
    s_drag.client = &client;

    drag_update(NULL, root_pos);
    TAP_EQ_INT(s_warp_edge_check_calls, 0,
            "null connection: nothing runs at all");
    TAP_EQ_INT(s_pan_edge_check_calls, 0,
            "null connection: the pan edge check does not run either");

    s_reset();
    s_drag.is_active = false;
    drag_update((xcb_connection_t *) 1, root_pos);
    TAP_EQ_INT(s_warp_edge_check_calls, 0,
            "no active drag: nothing runs at all");
    TAP_EQ_INT(s_pan_edge_check_calls, 0,
            "no active drag: the pan edge check does not run either");

    s_reset();
    s_drag.is_active = true;
    s_drag.client = NULL;
    drag_update((xcb_connection_t *) 1, root_pos);
    TAP_EQ_INT(s_warp_edge_check_calls, 0,
            "active drag but no client attached: nothing runs at"
            " all");
    TAP_EQ_INT(s_pan_edge_check_calls, 0,
            "active drag but no client attached: the pan edge check"
            " does not run either");
}


/* drag_update skips a duplicate MotionNotify reporting the exact same
 * root position already acted on */
static void s_test_update_skips_duplicate_position(void)
{
    client_td client;
    struct position_s root_pos = { 160, 260 };

    s_reset();
    s_make_client(&client);
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.has_last_pos = true;
    s_drag.last_root_x = 160;
    s_drag.last_root_y = 260;

    drag_update((xcb_connection_t *) 1, root_pos);

    TAP_EQ_INT(s_enact_move_calls, 0,
            "exact same position as last time: the whole update is"
            " skipped");
}


/* drag_update dispatches to the move path for a plain window move,
 * applying the pointer displacement on top of the drag's starting
 * geometry and calling enact_client_move for a solid drag */
static void s_test_update_move_dispatches_to_move_path(void)
{
    client_td client;
    struct position_s root_pos = { 160, 260 };

    s_reset();
    s_make_client(&client);
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.is_solid_drag = true;
    s_drag.pointer_start_x = 150;
    s_drag.pointer_start_y = 250;
    s_drag.client_start.pos.x = 100;
    s_drag.client_start.pos.y = 200;
    s_drag.client_start.dim.w = 300u;
    s_drag.client_start.dim.h = 150u;

    drag_update((xcb_connection_t *) 1, root_pos);

    TAP_EQ_INT(s_enact_move_calls, 1,
            "solid move drag: enact_client_move is called once");
    TAP_EQ_INT(s_enact_move_last_pos.x, 110,
            "moved position X is the start position plus the pointer"
            " displacement (10 here)");
    TAP_EQ_INT(s_enact_move_last_pos.y, 210,
            "moved position Y likewise (10 here)");
    TAP_EQ_INT(s_outline_move_calls, 0,
            "a solid drag never goes through the outline path");
    TAP_EQ_INT(s_warp_edge_check_calls, 1,
            "the pointer-warp edge is still checked exactly once");
    TAP_EQ_INT(s_pan_edge_check_calls, 1,
            "the viewport-pan edge is also checked exactly once");
}


/* drag_update dispatches to the outline path instead of moving the
 * real window live when the drag is not a solid one, and moves the
 * real window off screen exactly once the first time it actually
 * moves */
static void s_test_update_move_dispatches_to_outline_path(void)
{
    client_td client;
    struct position_s root_pos = { 160, 260 };

    s_reset();
    s_make_client(&client);
    client.properties.flags = (uint16_t) CLIENT_FLAG_DECORATED;
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.is_solid_drag = false;
    s_drag.is_outline_offscreened = false;
    s_drag.pointer_start_x = 150;
    s_drag.pointer_start_y = 250;
    s_drag.client_start.pos.x = 100;
    s_drag.client_start.pos.y = 200;
    s_drag.client_start.dim.w = 300u;
    s_drag.client_start.dim.h = 150u;

    drag_update((xcb_connection_t *) 1, root_pos);

    TAP_EQ_INT(s_enact_move_calls, 0,
            "an outline drag never calls enact_client_move directly");
    TAP_EQ_INT(s_outline_move_calls, 1,
            "the outline stand-in is moved once instead");
    TAP_OK(s_drag.is_outline_offscreened,
            "is_outline_offscreened flips true on the first real"
            " motion");
    TAP_EQ_INT(s_window_move_calls, 1,
            "the real (decorated) window is parked off screen exactly"
            " once");
    TAP_OK(s_window_move_last_window == client.frame,
            "the frame, not the plain client window, is what gets"
            " parked, since this client is decorated");

    drag_update((xcb_connection_t *) 1,
            (struct position_s) { 161, 260 });
    TAP_EQ_INT(s_window_move_calls, 1,
            "a second motion after the first: the real window is not"
            " parked off screen all over again");
}


/* drag_update dispatches to the icon path when the drag window
 * matches the client's own icon window, moving that icon window
 * directly rather than going through enact_client_move at all */
static void s_test_update_icon_dispatches_to_icon_path(void)
{
    client_td client;
    struct position_s root_pos = { 160, 260 };

    s_reset();
    s_make_client(&client);
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.drag_window = client.icon_window;
    s_drag.is_solid_drag = true;
    s_drag.pointer_start_x = 150;
    s_drag.pointer_start_y = 250;
    s_drag.client_start.pos.x = 20;
    s_drag.client_start.pos.y = 30;

    drag_update((xcb_connection_t *) 1, root_pos);

    TAP_EQ_INT(s_enact_move_calls, 0,
            "an icon drag never calls enact_client_move");
    TAP_EQ_INT(s_window_move_calls, 1,
            "instead the icon window itself is moved directly");
    TAP_OK(s_window_move_last_window == client.icon_window,
            "specifically the client's own icon window");
    TAP_EQ_INT(s_window_move_last_x, 30,
            "moved to the icon's start position plus the pointer"
            " displacement on X (10 here)");
    TAP_EQ_INT(s_window_move_last_y, 40,
            "and likewise on Y (10 here)");
}


/* drag_update dispatches to the resize path for a resize operation,
 * calling enact_client_resize for a solid drag with the geometry the
 * drag has reached */
static void s_test_update_resize_dispatches_to_resize_path(void)
{
    client_td client;
    struct position_s root_pos = { 400, 260 };

    s_reset();
    s_make_client(&client);
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_RESIZING;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.is_solid_drag = true;
    s_drag.is_resize_w = true;
    s_drag.is_resize_h = false;
    s_drag.is_anchor_right = false;
    s_drag.pointer_start_x = 150;
    s_drag.pointer_start_y = 250;
    s_drag.client_start.pos.x = 100;
    s_drag.client_start.pos.y = 200;
    s_drag.client_start.dim.w = 300u;
    s_drag.client_start.dim.h = 150u;

    drag_update((xcb_connection_t *) 1, root_pos);

    TAP_EQ_INT(s_enact_resize_calls, 1,
            "solid resize drag: enact_client_resize is called once");
    TAP_EQ_INT(s_resist_update_calls, 1,
            "the maximize-resistance axes are recomputed exactly"
            " once per update");
    TAP_EQ_INT(s_snap_resize_calls, 1,
            "edge snapping for the resize is consulted exactly once");
    TAP_EQ_INT(s_size_constrain_calls, 1,
            "the client's own size hints are applied exactly once");
}


/* drag_end with no active drag is a pure no-op */
static void s_test_end_no_active_drag_is_noop(void)
{
    struct position_s root_pos = { 160, 260 };

    s_reset();
    s_drag.is_active = false;

    drag_end((xcb_connection_t *) 1, NULL, NULL, root_pos);

    TAP_EQ_INT(s_overlay_hide_calls, 0,
            "no active drag: not even the overlay is touched");
}


/* drag_end for a plain solid move finalizes nothing further (already
 * applied live by drag_update along the way), hides the overlay,
 * ungrabs the pointer and resets every drag field back to idle */
static void s_test_end_move_resets_state(void)
{
    client_td client;
    struct position_s root_pos = { 160, 260 };

    s_reset();
    s_make_client(&client);
    client.properties.operation = (uint16_t) CLIENT_OPERATION_MOVING;
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.desktop = NULL;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.is_solid_drag = true;

    drag_end((xcb_connection_t *) 1, NULL, NULL, root_pos);

    TAP_OK(!s_drag.is_active, "drag ends: is_active reset to false");
    TAP_OK(s_drag.operation == CLIENT_OPERATION_IDLE,
            "operation reset to idle");
    TAP_OK(s_drag.client == NULL, "client pointer cleared");
    TAP_OK((uint16_t) client.properties.operation ==
            (uint16_t) CLIENT_OPERATION_IDLE,
            "the client's own operation field is reset to idle too");
    TAP_EQ_INT(s_enact_resize_calls, 0,
            "a move, not a resize: no finalizing resize call happens");
    TAP_EQ_INT(s_overlay_hide_calls, 1,
            "the feedback overlay is hidden exactly once");
    TAP_EQ_INT(s_ungrab_pointer_calls, 1,
            "the pointer grab is released exactly once");
    TAP_EQ_INT(s_flush_calls, 1, "the connection is flushed once");
}


/* drag_end for a solid resize issues one final enact_client_resize
 * to settle the last live update fully onto the client's own size
 * hints */
static void s_test_end_resize_finalizes_once_more(void)
{
    client_td client;
    struct position_s root_pos = { 160, 260 };

    s_reset();
    s_make_client(&client);
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_RESIZING;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.is_solid_drag = true;

    drag_end((xcb_connection_t *) 1, NULL, NULL, root_pos);

    TAP_EQ_INT(s_enact_resize_calls, 1,
            "solid resize: exactly one finalizing enact_client_resize"
            " call happens");
    TAP_EQ_INT(s_resist_finalize_calls, 1,
            "the resistance state is finalized exactly once");
    TAP_OK(s_resist_finalize_last_arg,
            "finalize is told this was indeed a resize");
}


/* drag_end for a non-solid (outline) drag applies the final geometry
 * in one call and destroys the outline windows, rather than calling
 * enact_client_move/resize live the way a solid drag already did all
 * along */
static void s_test_end_outline_move_applies_final_position(void)
{
    client_td client;
    struct position_s root_pos = { 160, 260 };

    s_reset();
    s_make_client(&client);
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.is_solid_drag = false;
    s_drag.client_cur.pos.x = 123;
    s_drag.client_cur.pos.y = 456;

    drag_end((xcb_connection_t *) 1, NULL, NULL, root_pos);

    TAP_EQ_INT(s_enact_move_calls, 1,
            "outline move: exactly one final enact_client_move"
            " settles the real window's position");
    TAP_EQ_INT(s_enact_move_last_pos.x, 123,
            "at the outline's own last tracked position X");
    TAP_EQ_INT(s_enact_move_last_pos.y, 456,
            "and Y likewise");
    TAP_EQ_INT(s_outline_end_calls, 1,
            "the outline strip windows are destroyed exactly once");
}


/* drag_end for a non-solid resize uses the forced resize entry point
 * instead of the normal one, since the real window was parked off
 * screen for the whole drag */
static void s_test_end_outline_resize_uses_forced_resize(void)
{
    client_td client;
    struct position_s root_pos = { 160, 260 };

    s_reset();
    s_make_client(&client);
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_RESIZING;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.is_solid_drag = false;
    s_drag.client_cur.dim.w = 500u;
    s_drag.client_cur.dim.h = 400u;

    drag_end((xcb_connection_t *) 1, NULL, NULL, root_pos);

    TAP_EQ_INT(s_enact_resize_force_calls, 1,
            "outline resize: the forced resize entry point is used"
            " exactly once");
    TAP_EQ_INT(s_enact_resize_calls, 0,
            "not the normal one, which could otherwise queue behind"
            " an unrelated in-flight exchange");
    TAP_EQ_INT(s_outline_end_calls, 1,
            "the outline strip windows are still destroyed exactly"
            " once");
}


/* drag_end for an icon drag that never moved past the click threshold
 * treats it as a plain click: restores the client and focuses it,
 * without ever touching icon_pos at all */
static void s_test_end_icon_click_restores_and_focuses(void)
{
    struct position_s root_pos;
    client_td client;
    stage_td stage;
    desktop_td desktop;

    s_reset();
    s_make_client(&client);
    memset(&stage, 0, sizeof(stage));
    memset(&desktop, 0, sizeof(desktop));
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.drag_window = client.icon_window;
    s_drag.is_solid_drag = true;
    s_drag.pointer_start_x = 150;
    s_drag.pointer_start_y = 250;
    root_pos.x = 150;
    root_pos.y = 250;

    drag_end((xcb_connection_t *) 1, &stage, &desktop, root_pos);

    TAP_EQ_INT(s_enact_restore_calls, 1,
            "pointer never moved past the click threshold: the icon"
            " is restored exactly once, as a click would be");
    TAP_EQ_INT(s_focus_apply_calls, 1,
            "and the now-restored client is focused exactly once");
    TAP_EQ_INT(s_place_icon_avoid_calls, 0,
            "the icon-drop tray-avoidance logic never runs for a"
            " plain click");
}


/* drag_end for an icon drag that did move past the click threshold
 * settles the icon's final position instead, consulting the systray
 * tray-avoidance logic along the way, and restores whatever mapped
 * state the icon had before the drag began */
static void s_test_end_icon_drag_settles_final_position(void)
{
    struct position_s root_pos;
    client_td client;
    stage_td stage;

    s_reset();
    s_make_client(&client);
    memset(&stage, 0, sizeof(stage));
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.drag_window = client.icon_window;
    s_drag.is_solid_drag = true;
    s_drag.was_icon_mapped = true;
    s_drag.pointer_start_x = 0;
    s_drag.pointer_start_y = 0;
    s_drag.client_start.pos.x = 20;
    s_drag.client_start.pos.y = 30;
    root_pos.x = 100;
    root_pos.y = 100;
    s_stub_systray_has_geometry = false;

    drag_end((xcb_connection_t *) 1, &stage, NULL, root_pos);

    TAP_EQ_INT(s_enact_restore_calls, 0,
            "the drag moved well past the click threshold: it is not"
            " treated as a plain click");
    TAP_OK(client.icon_pos.x == (int16_t) (20 + 100) &&
            client.icon_pos.y == (int16_t) (30 + 100),
            "the icon's final position is settled at its start"
            " position plus the full pointer displacement");
    TAP_EQ_INT(s_systray_get_geometry_calls, 1,
            "the systray's own geometry is consulted exactly once to"
            " decide whether the icon needs to be pushed off it");
    TAP_EQ_INT(s_systray_restack_calls, 1,
            "a restack is forced exactly once so the icon never"
            " visibly sits over the tray, even briefly");
    TAP_OK(client.is_icon_mapped,
            "the icon's mapped state is restored to whatever it was"
            " right before the drag began (was true here)");
}


/* drag_cancel with no active drag, or for a client other than the
 * one currently being dragged, is a pure no-op */
static void s_test_cancel_guards_are_noop(void)
{
    client_td client_a;
    client_td client_b;

    s_reset();
    s_make_client(&client_a);
    s_make_client(&client_b);
    s_drag.is_active = false;

    drag_cancel((xcb_connection_t *) 1, &client_a);
    TAP_EQ_INT(s_overlay_hide_calls, 0,
            "no active drag: nothing at all runs");

    s_reset();
    s_make_client(&client_a);
    s_make_client(&client_b);
    s_drag.is_active = true;
    s_drag.client = &client_a;

    drag_cancel((xcb_connection_t *) 1, &client_b);
    TAP_EQ_INT(s_overlay_hide_calls, 0,
            "a different client than the one being dragged: nothing"
            " runs, the active drag is left untouched");
    TAP_OK(s_drag.is_active,
            "the unrelated drag itself is still active afterward");
}


/* drag_cancel for the client actually being dragged tears the drag
 * down: hides the overlay, ends any outline, and resets every field,
 * including the surviving client's own operation flag */
static void s_test_cancel_active_client_tears_down(void)
{
    client_td client;

    s_reset();
    s_make_client(&client);
    client.properties.operation = (uint16_t) CLIENT_OPERATION_MOVING;
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.is_solid_drag = true;

    drag_cancel((xcb_connection_t *) 1, &client);

    TAP_OK(!s_drag.is_active, "the dragged client: torn down");
    TAP_OK(s_drag.client == NULL, "client pointer cleared");
    TAP_OK((uint16_t) client.properties.operation ==
            (uint16_t) CLIENT_OPERATION_IDLE,
            "the surviving client's own operation flag is reset to"
            " idle, not left stuck at moving forever");
    TAP_EQ_INT(s_overlay_hide_calls, 1,
            "the feedback overlay is hidden exactly once");
    TAP_EQ_INT(s_outline_end_calls, 1,
            "the outline windows are always cleaned up, whether or"
            " not this particular drag actually used one");
    TAP_EQ_INT(s_ungrab_pointer_calls, 1,
            "the pointer grab is released exactly once");
}


/* drag_cancel moves an outline-offscreened client back to its
 * genuine, never-actually-changed logical position before tearing
 * down, so it never stays stuck off screen with nothing left to move
 * it back */
static void s_test_cancel_restores_offscreened_client(void)
{
    client_td client;

    s_reset();
    s_make_client(&client);
    client.properties.flags = (uint16_t) CLIENT_FLAG_DECORATED;
    s_drag.is_active = true;
    s_drag.client = &client;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.is_solid_drag = false;
    s_drag.is_outline_offscreened = true;

    drag_cancel((xcb_connection_t *) 1, &client);

    TAP_EQ_INT(s_window_move_calls, 1,
            "the offscreened real window is moved back exactly once");
    TAP_OK(s_window_move_last_window == client.frame,
            "specifically the decorated frame, not the bare client"
            " window");
    TAP_EQ_INT(s_window_move_last_x, client.layout.geometry.cur.pos.x,
            "moved back to its own genuine, never-actually-changed"
            " logical position X");
    TAP_EQ_INT(s_window_move_last_y, client.layout.geometry.cur.pos.y,
            "and Y likewise");
}


/* drag_is_active mirrors the module's own is_active field exactly */
static void s_test_is_active_mirrors_state(void)
{
    s_reset();
    s_drag.is_active = false;
    TAP_OK(!drag_is_active(), "inactive drag state: reports inactive");

    s_drag.is_active = true;
    TAP_OK(drag_is_active(), "active drag state: reports active");
}


/* drag_client returns exactly the client currently attached to the
 * drag, or null when none is */
static void s_test_client_returns_attached_client(void)
{
    client_td client;

    s_reset();
    s_make_client(&client);
    s_drag.client = NULL;
    TAP_NULL(drag_client(), "no client attached: returns null");

    s_drag.client = &client;
    TAP_OK(drag_client() == &client,
            "a client attached: returns that exact pointer");
}


int main(void)
{
    TAP_PLAN(118);

    s_test_start_null_guards_are_noop();
    s_test_start_move_success_populates_state();
    s_test_start_resize_infers_top_left_anchor();
    s_test_start_resize_middle_grab_resizes_both_axes();
    s_test_start_move_locks_maximized_axis();
    s_test_start_failed_grab_status_rolls_back();
    s_test_start_null_grab_reply_rolls_back();
    s_test_start_directed_overrides_anchor();
    s_test_start_directed_noop_when_start_fails();
    s_test_start_axis_locked_locks_one_axis();
    s_test_start_axis_locked_single_axis_never_dead();
    s_test_update_null_guards_are_noop();
    s_test_update_skips_duplicate_position();
    s_test_update_move_dispatches_to_move_path();
    s_test_update_move_dispatches_to_outline_path();
    s_test_update_icon_dispatches_to_icon_path();
    s_test_update_resize_dispatches_to_resize_path();
    s_test_end_no_active_drag_is_noop();
    s_test_end_move_resets_state();
    s_test_end_resize_finalizes_once_more();
    s_test_end_outline_move_applies_final_position();
    s_test_end_outline_resize_uses_forced_resize();
    s_test_end_icon_click_restores_and_focuses();
    s_test_end_icon_drag_settles_final_position();
    s_test_cancel_guards_are_noop();
    s_test_cancel_active_client_tears_down();
    s_test_cancel_restores_offscreened_client();
    s_test_is_active_mirrors_state();
    s_test_client_returns_attached_client();

    return TAP_DONE();
}
