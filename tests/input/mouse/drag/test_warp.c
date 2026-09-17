/**
 * @file tests/input/mouse/drag/test_warp.c
 *
 * @brief Test battery for the screen-edge desktop warp during a drag
 *
 * warp.c reaches deep into the running window manager: the stage
 * and desktop it warps across (@a wm_get_stage_by_id,
 * @a wm_get_client_desktop, @a stage_desktop_get), the transient
 * family it drags along (@c cmds/client/transient.c), the actual X
 * requests that move the pointer and windows
 * (@c xcb_warp_pointer, @c xcb_configure_window, @a enact_client_move),
 * and the desktop-switch chrome (@a notify_desktop_show,
 * @a stage_client_hide_all, @a stage_client_show_all).  None of that is
 * needed to exercise this file's real target: the edge-detection and
 * countdown bookkeeping in @a drag_warp_edge_check and
 * @a drag_warp_ms_remaining, and the early-exit guards at the top of
 * @a drag_warp_tick, so every one of those heavier calls is a
 * link-only or recording stand-in below instead.  @a clock_gettime and
 * @a clock_ms_until/@a clock_add_ms (utils/time/clock.c) are linked
 * real, since the countdown math itself is exactly what is under
 * test.  Storage for @c s_drag lives in drag.c, which this file never
 * links, so it is defined here instead, the same way drag.c itself
 * would define it.
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

/* Project includes */
#include <client.h>
#include <cmds/client/transient.h>
#include <desktop.h>
#include <enact.h>
#include <input/mouse/drag/icon.h>
#include <input/mouse/drag/outline.h>
#include <input/mouse/drag/overlay.h>
#include <menu/notify/desktop.h>
#include <policy/focus.h>
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/warp.h>
#include <utils/time/clock.h>


/** Singleton drag state; storage normally lives in drag.c, which this
 *  file never links, so it is defined here instead */
drag_state_td s_drag;

/** Stage @a wm_get_stage_by_id hands back, @c NULL to make the
 *  lookup itself fail */
static stage_td *s_stub_stage;

/** Count of every heavy call this file only records rather than acts
 *  on, reset by @a s_reset before each scenario */
static int s_call_enact_client_move;

/** Last position @a enact_client_move was asked to move to, for
 *  scenarios that check a locked move axis is really left untouched */
static struct position_s s_enact_client_move_last_pos;
static int s_call_stage_clients_hide;
static int s_call_stage_clients_show;
static int s_call_notify_desktop_show;
static int s_call_drag_outline_move;
static int s_call_drag_overlay_show;
static int s_call_desktop_action_client_add;
static int s_call_desktop_action_client_rem;

/** Value @a scmd_stage_viewport_pan_available hands back, reset to
 *  @c false (the sensible "no room to pan" default) by @a s_reset
 *  before each scenario */
static bool s_stub_pan_available;

/** Count of calls to the @a drag_pan_edge_check stand-in below, reset
 *  to @c 0 by @a s_reset before each scenario */
static int s_call_drag_pan_edge_check;

/** Desktop @a stage_desktop_east and @a stage_desktop_north hand
 *  back, @c NULL to make the lookup itself fail; the scenarios
 *  exercising a full @a drag_warp_tick switch point this at a real
 *  desktop instead */
static desktop_td *s_stub_desktop_target;

/** Count of calls to the @a xcb_warp_pointer stand-in below, and the
 *  last X/Y it was asked to warp the pointer to, reset by @a s_reset
 *  before each scenario */
static int s_call_xcb_warp_pointer;
static int16_t s_warp_pointer_last_x;
static int16_t s_warp_pointer_last_y;

/** Last window, mask, and X/Y values handed to the
 *  @a xcb_configure_window stand-in below, for scenarios that check an
 *  icon drag really gets repositioned to the expected spot */
static int s_call_configure_window;
static xcb_window_t s_configure_window_last_window;
static uint16_t s_configure_window_last_mask;
static int32_t s_configure_window_last_x;
static int32_t s_configure_window_last_y;


/**
 * @brief Link-only stand-in for @a wm_get_stage_by_id
 *
 * @note Complexity: @e O(1)
 */
stage_td *wm_get_stage_by_id(uint32_t stage_id)
{
    (void) stage_id;
    return s_stub_stage;
}


/**
 * @brief Link-only stand-in for @a wm_get_client_desktop, never
 *        exercised by any scenario below
 *
 * @note Complexity: @e O(1)
 */
desktop_td *wm_get_client_desktop(const client_td *client)
{
    (void) client;
    return NULL;
}


/** Desktop every desktop-returning stand-in in this file answers
 *  with, @c NULL until a scenario supplies one */
static desktop_td *s_current_desktop;


/**
 * @brief Link-only stand-in for @a stage_desktop_get
 *
 * @note Complexity: @e O(1)
 */
desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;

    /* The desktop being left, which the warp reads the page it was
     * travelling along from; @c NULL until a scenario supplies one */
    return s_current_desktop;
}


/**
 * @brief Link-only stand-in for @a stage_desktop_south
 *
 * @note Complexity: @e O(1)
 */
desktop_td *stage_desktop_south(stage_td *stage,
        uint32_t desktop_id, bool cycle)
{
    (void) stage;
    (void) desktop_id;
    (void) cycle;
    return NULL;
}


/**
 * @brief Controllable stand-in for @a stage_desktop_east
 *
 * @c NULL by default (the same as every other direction below), but
 * the scenarios exercising a full @a drag_warp_tick switch point
 * @a s_stub_desktop_target at a real desktop instead.
 *
 * @note Complexity: @e O(1)
 */
desktop_td *stage_desktop_east(stage_td *stage,
        uint32_t desktop_id, bool cycle)
{
    (void) stage;
    (void) desktop_id;
    (void) cycle;
    return s_stub_desktop_target;
}


/**
 * @brief Controllable stand-in for @a stage_desktop_north
 *
 * @c NULL by default (the same as every other direction below), but
 * the scenario exercising a full north @a drag_warp_tick switch points
 * @a s_stub_desktop_target at a real desktop instead, exactly like
 * @a stage_desktop_east above.
 *
 * @note Complexity: @e O(1)
 */
desktop_td *stage_desktop_north(stage_td *stage,
        uint32_t desktop_id, bool cycle)
{
    (void) stage;
    (void) desktop_id;
    (void) cycle;
    return s_stub_desktop_target;
}


/**
 * @brief Link-only stand-in for @a stage_desktop_west
 *
 * @note Complexity: @e O(1)
 */
desktop_td *stage_desktop_west(stage_td *stage,
        uint32_t desktop_id, bool cycle)
{
    (void) stage;
    (void) desktop_id;
    (void) cycle;
    return NULL;
}


/**
 * @brief Recording stand-in for @a stage_client_hide_all
 *
 * @note Complexity: @e O(1)
 */
void stage_client_hide_all(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
    s_call_stage_clients_hide++;
}


/**
 * @brief Recording stand-in for @a stage_client_show_all
 *
 * @note Complexity: @e O(1)
 */
void stage_client_show_all(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    (void) desktop_id;
    s_call_stage_clients_show++;
}


/**
 * @brief Recording stand-in for @a desktop_action_client_add
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_add(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    s_call_desktop_action_client_add++;
    return 0;
}


/**
 * @brief Recording stand-in for @a desktop_action_client_rem
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_rem(desktop_td *desktop, client_td *client)
{
    (void) desktop;
    (void) client;
    s_call_desktop_action_client_rem++;
    return 0;
}


/**
 * @brief Link-only stand-in for @a desktop_action_client_move, never
 *        exercised by any scenario below
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_move(desktop_td *from, desktop_td *to,
        client_td *client)
{
    (void) from;
    (void) to;
    (void) client;
    return 0;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_transient_top_parent
 *
 * @note Complexity: @e O(1)
 */
client_td *ccmd_client_transient_top_parent(client_td *client)
{
    return client;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_transient_family_snapshot,
 *        never exercised by any scenario below
 *
 * @note Complexity: @e O(1)
 */
client_td **ccmd_client_transient_family_snapshot(const desktop_td *desktop,
        client_td *top, size_t *out_count)
{
    (void) desktop;
    (void) top;
    *out_count = 0;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a focus_order_to_top, never exercised
 *        by any scenario below
 *
 * @note Complexity: @e O(1)
 */
void focus_order_to_top(client_td *client)
{
    (void) client;
}


/**
 * @brief Recording stand-in for @a notify_desktop_show
 *
 * @note Complexity: @e O(1)
 */
/** Test-controlled stand-in for @a stage_viewport_dims
 * @note Complexity: @e O(1) */
static uint32_t s_viewport_columns = 1u;
static uint32_t s_viewport_rows = 1u;

void stage_viewport_dims(const stage_td *stage,
        uint32_t *columns_out, uint32_t *rows_out)
{
    (void) stage;

    *columns_out = s_viewport_columns;
    *rows_out = s_viewport_rows;
}


/** Link-only stand-in for @a lookup_current_desktop, answering the
 *  same desktop every other stand-in here hands back
 * @note Complexity: @e O(1) */
desktop_td *lookup_current_desktop(const stage_td *stage)
{
    (void) stage;

    return s_current_desktop;
}


/** Recording stand-in for @a scmd_stage_viewport_set: this file has
 *  no viewport, so the warp's own "enter by the opposite edge" step
 *  is observed rather than performed
 * @note Complexity: @e O(1) */
static int s_call_viewport_set;
static int32_t s_last_viewport_x;
static int32_t s_last_viewport_y;

void scmd_stage_viewport_set(stage_td *stage, int32_t x,
        int32_t y)
{
    (void) stage;

    s_call_viewport_set++;
    s_last_viewport_x = x;
    s_last_viewport_y = y;
}


/** Test-controlled stand-in for
 *  @a scmd_stage_viewport_desktop_page
 * @note Complexity: @e O(1) */
static bool s_page_known;
static uint32_t s_page_col;
static uint32_t s_page_row;

bool scmd_stage_viewport_desktop_page(const stage_td *stage,
        const desktop_td *desktop, uint32_t *col_out, uint32_t *row_out)
{
    (void) stage;
    (void) desktop;

    if (!s_page_known) {
        return false;
    }
    *col_out = s_page_col;
    *row_out = s_page_row;
    return true;
}


void notify_desktop_show(xcb_connection_t *connection, stage_td *stage,
        uint32_t desktop_idx, const char *desktop_name,
        enum notify_desktop_cause_e cause, const config_td *config)
{
    (void) connection;
    (void) stage;
    (void) desktop_idx;
    (void) desktop_name;
    (void) cause;
    (void) config;
    s_call_notify_desktop_show++;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get, never exercised
 *        by any scenario below
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/**
 * @brief Recording stand-in for @a enact_client_move
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move(client_td *client, struct position_s pos)
{
    (void) client;
    s_enact_client_move_last_pos = pos;
    s_call_enact_client_move++;
}


/**
 * @brief Recording stand-in for the raw @a xcb_warp_pointer request
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_warp_pointer(xcb_connection_t *c,
        xcb_window_t src_window, xcb_window_t dst_window,
        int16_t src_x, int16_t src_y, uint16_t src_width,
        uint16_t src_height, int16_t dst_x, int16_t dst_y)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) src_window;
    (void) dst_window;
    (void) src_x;
    (void) src_y;
    (void) src_width;
    (void) src_height;

    memset(&cookie, 0, sizeof(cookie));
    s_call_xcb_warp_pointer++;
    s_warp_pointer_last_x = dst_x;
    s_warp_pointer_last_y = dst_y;

    return cookie;
}


/**
 * @brief Recording stand-in for the raw @a xcb_configure_window
 *        request
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_configure_window(xcb_connection_t *c,
        xcb_window_t window, uint16_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie;
    const uint32_t *vals;

    (void) c;

    memset(&cookie, 0, sizeof(cookie));
    s_call_configure_window++;
    s_configure_window_last_window = window;
    s_configure_window_last_mask = value_mask;

    vals = value_list;
    s_configure_window_last_x = (int32_t) vals[0];
    s_configure_window_last_y = (int32_t) vals[1];

    return cookie;
}


/**
 * @brief Recording stand-in for @a drag_outline_move
 *
 * @note Complexity: @e O(1)
 */
void drag_outline_move(xcb_connection_t *connection, struct geometry_s geom)
{
    (void) connection;
    (void) geom;
    s_call_drag_outline_move++;
}


/**
 * @brief Recording stand-in for @a drag_overlay_show
 *
 * @note Complexity: @e O(1)
 */
void drag_overlay_show(xcb_connection_t *connection, bool is_icon,
        struct geometry_s target, const char *text)
{
    (void) connection;
    (void) is_icon;
    (void) target;
    (void) text;
    s_call_drag_overlay_show++;
}


/**
 * @brief Link-only stand-in for @a drag_icon_height, never exercised
 *        by any scenario below
 *
 * @note Complexity: @e O(1)
 */
uint16_t drag_icon_height(const client_td *client)
{
    (void) client;
    return 0u;
}


/**
 * @brief Controllable stand-in for @a scmd_stage_viewport_pan_available
 *
 * @note Complexity: @e O(1)
 */
bool scmd_stage_viewport_pan_available(stage_td *stage,
        enum compass_direction_e direction)
{
    (void) stage;
    (void) direction;
    return s_stub_pan_available;
}


/**
 * @brief Recording stand-in for @a drag_pan_edge_check
 *
 * @note Complexity: @e O(1)
 */
void drag_pan_edge_check(int16_t root_x, int16_t root_y)
{
    (void) root_x;
    (void) root_y;
    s_call_drag_pan_edge_check++;
}


static void s_reset(void)
{
    static client_td dragged;
    static stage_td stage;
    static config_td config;
    static xcb_screen_t screen;

    memset(&s_drag, 0, sizeof(s_drag));
    memset(&dragged, 0, sizeof(dragged));
    memset(&stage, 0, sizeof(stage));
    memset(&config, 0, sizeof(config));
    memset(&screen, 0, sizeof(screen));

    dragged.id = 1u;
    dragged.screen_id = 0u;
    dragged.icon_window = 42u;

    config.desktops.warp_on_edge_drag = true;

    stage.config = &config;
    stage.desktop_count = 4u;
    stage.desktop_cur = 0u;
    stage.screen = &screen;

    s_drag.client = &dragged;
    s_drag.screen_w = 1920u;
    s_drag.screen_h = 1080u;

    s_stub_stage = &stage;
    s_stub_desktop_target = NULL;

    s_call_enact_client_move = 0;
    s_enact_client_move_last_pos.x = 0;
    s_enact_client_move_last_pos.y = 0;
    s_call_stage_clients_hide = 0;
    s_call_stage_clients_show = 0;
    s_call_notify_desktop_show = 0;
    s_call_drag_outline_move = 0;
    s_call_drag_overlay_show = 0;
    s_call_desktop_action_client_add = 0;
    s_call_desktop_action_client_rem = 0;
    s_stub_pan_available = false;
    s_call_drag_pan_edge_check = 0;
    s_call_xcb_warp_pointer = 0;
    s_warp_pointer_last_x = 0;
    s_warp_pointer_last_y = 0;
    s_call_configure_window = 0;
    s_configure_window_last_window = XCB_WINDOW_NONE;
    s_configure_window_last_mask = 0u;
    s_configure_window_last_x = 0;
    s_configure_window_last_y = 0;
}


/**
 * @brief Shared setup for a scenario that runs a full
 *        @a drag_warp_tick east switch, to exercise
 *        @a s_warp_move_dragged
 *
 * @note Complexity: @e O(1)
 */
static void s_reset_for_full_east_switch(void)
{
    static desktop_td desktop_east;

    s_reset();
    memset(&desktop_east, 0, sizeof(desktop_east));
    desktop_east.id = 1u;
    s_stub_desktop_target = &desktop_east;

    s_drag.is_warp_pending = true;
    s_drag.warp_direction = COMPASS_EAST;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.drag_window = XCB_WINDOW_NONE;
    s_drag.is_solid_drag = true;
    s_drag.last_root_x = 1919;
    s_drag.last_root_y = 500;
    s_drag.client_start.pos.x = 100;
    s_drag.client_start.pos.y = 200;
    s_drag.client_cur.pos.x = 110;
    s_drag.client_cur.pos.y = 210;
}


/* No client under drag at all: the edge check clears any pending warp
 * and does nothing else */
static void s_test_edge_check_no_client_clears_pending(void)
{
    s_reset();
    s_drag.client = NULL;
    s_drag.is_warp_pending = true;

    drag_warp_edge_check(0, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "no dragged client: pending warp is cleared");
}


/* wm_get_stage_by_id fails to resolve a stage: no-op */
static void s_test_edge_check_no_stage_is_noop(void)
{
    s_reset();
    s_stub_stage = NULL;

    drag_warp_edge_check(0, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "stage lookup failing clears any pending warp");
}


/* warp_on_edge_drag is off in configuration: no-op even at an edge */
static void s_test_edge_check_disabled_in_config_is_noop(void)
{
    s_reset();
    s_stub_stage->config->desktops.warp_on_edge_drag = false;

    drag_warp_edge_check(0, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "warp_on_edge_drag false: edge touch never arms a warp");
}


/* Only one desktop on the stage: no-op even at an edge, there being
 * nowhere else to warp to */
static void s_test_edge_check_single_desktop_is_noop(void)
{
    s_reset();
    s_stub_stage->desktop_count = 1u;

    drag_warp_edge_check(0, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "a single-desktop stage never arms a warp");
}


/* Pointer well inside the screen on both axes: no-op */
static void s_test_edge_check_middle_of_screen_is_noop(void)
{
    s_reset();

    drag_warp_edge_check(960, 540);

    TAP_OK(!s_drag.is_warp_pending,
            "pointer away from every edge never arms a warp");
}


/* Pointer at the exact left edge (x == 0) arms a west warp */
static void s_test_edge_check_left_edge_arms_west(void)
{
    s_reset();

    drag_warp_edge_check(0, 500);

    TAP_OK(s_drag.is_warp_pending, "x=0 arms a pending warp");
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_WEST,
            "direction is west");
}


/* Pointer at the left edge, but the viewport still has room to pan
 * that same edge with 'pan_on_edge_drag' enabled: the warp defers to
 * the pan instead of arming */
static void s_test_edge_check_pan_available_defers_to_pan(void)
{
    s_reset();
    s_stub_stage->config->base.viewport.pan_on_edge_drag = true;
    s_stub_pan_available = true;

    drag_warp_edge_check(0, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "viewport pan still available: no warp armed");
}


/* Pointer one pixel inside the left edge (x == 1) does not arm a
 * warp: the boundary is inclusive of 0, not 1 */
static void s_test_edge_check_one_past_left_edge_is_noop(void)
{
    s_reset();

    drag_warp_edge_check(1, 500);

    TAP_OK(!s_drag.is_warp_pending,
            "x=1 is one pixel short of the left edge: no warp armed");
}


/* Pointer at the exact right edge (x == screen_w - 1) arms an east
 * warp */
static void s_test_edge_check_right_edge_arms_east(void)
{
    s_reset();

    drag_warp_edge_check(1919, 500);

    TAP_OK(s_drag.is_warp_pending, "x=screen_w-1 arms a pending warp");
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_EAST,
            "direction is east");
}


/* Pointer at the exact top edge (y == 0) arms a north warp */
static void s_test_edge_check_top_edge_arms_north(void)
{
    s_reset();

    drag_warp_edge_check(960, 0);

    TAP_OK(s_drag.is_warp_pending, "y=0 arms a pending warp");
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_NORTH,
            "direction is north");
}


/* Pointer at the exact bottom edge (y == screen_h - 1) arms a south
 * warp */
static void s_test_edge_check_bottom_edge_arms_south(void)
{
    s_reset();

    drag_warp_edge_check(960, 1079);

    TAP_OK(s_drag.is_warp_pending, "y=screen_h-1 arms a pending warp");
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_SOUTH,
            "direction is south");
}


/* A corner holds two edges at once: the horizontal direction always
 * wins over the vertical one */
static void s_test_edge_check_corner_prefers_horizontal(void)
{
    s_reset();

    drag_warp_edge_check(0, 0);

    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_WEST,
            "top-left corner: west (horizontal) wins over north");

    s_reset();

    drag_warp_edge_check(1919, 1079);

    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_EAST,
            "bottom-right corner: east (horizontal) wins over south");
}


/* Holding the same edge across two calls keeps the original countdown
 * running rather than restarting it */
static void s_test_edge_check_same_edge_keeps_countdown(void)
{
    struct timespec first_due;

    s_reset();

    drag_warp_edge_check(0, 500);
    first_due = s_drag.warp_due;

    drag_warp_edge_check(0, 501);

    TAP_OK(s_drag.warp_due.tv_sec == first_due.tv_sec &&
            s_drag.warp_due.tv_nsec == first_due.tv_nsec,
            "the same edge held again does not restart the countdown");
}


/* Switching to a different edge mid-hold re-arms a fresh countdown
 * for the new direction */
static void s_test_edge_check_different_edge_rearms(void)
{
    s_reset();

    drag_warp_edge_check(0, 500);
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_WEST,
            "first touch arms west");

    drag_warp_edge_check(960, 0);
    TAP_EQ_INT((long) s_drag.warp_direction, (long) COMPASS_NORTH,
            "moving to the top edge re-arms as north instead");
}


/* Leaving every edge after being armed clears the pending warp again */
static void s_test_edge_check_leaving_edge_clears_pending(void)
{
    s_reset();

    drag_warp_edge_check(0, 500);
    TAP_OK(s_drag.is_warp_pending, "armed at the left edge");

    drag_warp_edge_check(960, 540);
    TAP_OK(!s_drag.is_warp_pending,
            "moving back to the middle of the screen clears it");
}


/* drag_warp_ms_remaining returns -1 whenever nothing is pending */
static void s_test_ms_remaining_returns_negative_when_idle(void)
{
    s_reset();
    s_drag.is_warp_pending = false;

    TAP_EQ_INT(drag_warp_ms_remaining(), -1,
            "no pending warp: -1, never a stale countdown");
}


/* drag_warp_ms_remaining reflects a countdown armed by the edge check
 * itself: strictly positive and no larger than the configured delay */
static void s_test_ms_remaining_positive_right_after_arming(void)
{
    int remaining;

    s_reset();

    drag_warp_edge_check(0, 500);
    remaining = drag_warp_ms_remaining();

    TAP_OK(remaining > 0 && remaining <= WM_DESKTOP_WARP_DELAY_MS,
            "freshly armed countdown is within (0, configured delay]");
}


/* drag_warp_ms_remaining reads 0, never negative, once due has
 * already passed */
static void s_test_ms_remaining_zero_once_due_has_passed(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 10;

    TAP_EQ_INT(drag_warp_ms_remaining(), 0,
            "a due time already 10 seconds in the past reads back as"
            " exactly 0, never negative");
}


/* drag_warp_tick: a NULL connection is a no-op, touching nothing */
static void s_test_tick_null_connection_is_noop(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;

    drag_warp_tick(NULL);

    TAP_OK(s_drag.is_warp_pending,
            "a NULL connection leaves the pending flag untouched");
    TAP_EQ_INT(s_call_stage_clients_hide, 0,
            "and never reaches the actual warp machinery");
}


/* drag_warp_tick: nothing pending is a no-op */
static void s_test_tick_nothing_pending_is_noop(void)
{
    s_reset();
    s_drag.is_warp_pending = false;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_stage_clients_hide, 0,
            "nothing pending: the tick never touches the desktop"
            " switch machinery");
}


/* drag_warp_tick: countdown still running (ms_remaining > 0) is a
 * no-op, and leaves the pending flag set for the next tick */
static void s_test_tick_countdown_not_due_is_noop(void)
{
    s_reset();

    drag_warp_edge_check(0, 500);
    TAP_OK(s_drag.is_warp_pending, "armed by the edge check");

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_OK(s_drag.is_warp_pending,
            "countdown not yet due: still pending after the tick");
    TAP_EQ_INT(s_call_stage_clients_hide, 0,
            "and the desktop switch itself never ran");
}


/* drag_warp_tick: due, but no client under drag any more: clears
 * pending and stops there */
static void s_test_tick_due_no_client_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.client = NULL;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_OK(!s_drag.is_warp_pending,
            "due tick with no client: pending flag is cleared anyway");
    TAP_EQ_INT(s_call_stage_clients_hide, 0,
            "but the desktop switch itself never ran");
}


/* drag_warp_tick: due, but the operation is a resize, not a move:
 * stops before touching the desktop switch machinery */
static void s_test_tick_due_wrong_operation_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_RESIZING;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_stage_clients_hide, 0,
            "a resize in progress never triggers the desktop switch");
}


/* drag_warp_tick: due, moving, but drag_window names neither
 * XCB_WINDOW_NONE nor the dragged client's own icon window: stale
 * state, stops before the switch */
static void s_test_tick_due_stale_drag_window_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.client->icon_window = 42u;
    s_drag.drag_window = 99u;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_stage_clients_hide, 0,
            "drag_window matching neither XCB_WINDOW_NONE nor the"
            " client's own icon window never triggers the switch");
}


/* drag_warp_tick: due, moving, drag_window matches the client's own
 * icon window (an icon drag): allowed past that guard, only stopped
 * later by the stage having a single desktop */
static void s_test_tick_icon_drag_window_matches_is_allowed(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_drag.client->icon_window = 42u;
    s_drag.drag_window = 42u;
    s_stub_stage->desktop_count = 1u;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_stage_clients_hide, 0,
            "a matching icon window clears the drag_window guard, but"
            " the single-desktop stage still stops it right after");
}


/* drag_warp_tick: due, moving, but the resolved stage has only one
 * desktop: stops before touching the desktop switch machinery */
static void s_test_tick_due_single_desktop_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_stub_stage->desktop_count = 1u;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_stage_clients_hide, 0,
            "a single-desktop stage stops the tick before the"
            " desktop switch itself");
}


/* drag_warp_tick: due, moving, but warp_on_edge_drag was disabled
 * after the countdown was already armed: stops before the switch */
static void s_test_tick_due_config_disabled_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_stub_stage->config->desktops.warp_on_edge_drag = false;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_stage_clients_hide, 0,
            "warp_on_edge_drag turned off since arming: the switch"
            " itself never runs");
}


/* drag_warp_tick: due, moving, stage itself no longer resolvable:
 * stops before the switch */
static void s_test_tick_due_no_stage_stops_early(void)
{
    s_reset();
    s_drag.is_warp_pending = true;
    (void) clock_gettime(CLOCK_MONOTONIC, &s_drag.warp_due);
    s_drag.warp_due.tv_sec -= 1;
    s_drag.operation = CLIENT_OPERATION_MOVING;
    s_stub_stage = NULL;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_stage_clients_hide, 0,
            "the stage lookup failing stops the tick before the"
            " switch itself");
}


/* drag_warp_tick: a full east switch on an ordinary (unlocked) move
 * drag shifts the dragged client's position by the pointer's own
 * warp delta, exactly like an ordinary move would */
static void s_test_tick_full_switch_unlocked_axis_moves(void)
{
    s_reset_for_full_east_switch();

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_xcb_warp_pointer, 1,
            "an east warp that actually switches desktops warps the"
            " pointer exactly once");
    TAP_EQ_INT(s_warp_pointer_last_x, 1,
            "the pointer lands one pixel in from the opposite (west)"
            " edge");
    TAP_EQ_INT(s_drag.client_cur.pos.x, -1808,
            "client_cur.pos.x shifts by the same delta the pointer"
            " itself just jumped (110 + (1 - 1919) = -1808)");
    TAP_EQ_INT(s_drag.client_cur.pos.y, 210,
            "client_cur.pos.y is untouched by a purely horizontal"
            " warp, whose Y pointer coordinate passes through"
            " unchanged");
    TAP_EQ_INT(s_call_enact_client_move, 1,
            "a solid, unlocked move drag repositions the real window"
            " exactly once");
    TAP_EQ_INT(s_enact_client_move_last_pos.x, -1808,
            "moved to the same shifted X the state tracking now"
            " reflects, since neither move axis is locked");
}


/* drag_warp_tick: a warp only happens once the viewport has no room
 * left that way, so it continues the movement rather than restarting
 * it: the desktop entered is put on the page opposite the edge just
 * left, keeping the other axis, instead of wherever it was last */
static void s_test_tick_full_switch_enters_opposite_page(void)
{
    desktop_td desktop;

    s_reset_for_full_east_switch();
    memset(&desktop, 0, sizeof(desktop));
    desktop.geometry.dim.w = 1920u;
    desktop.geometry.dim.h = 1080u;
    s_current_desktop = &desktop;
    s_viewport_columns = 3u;
    s_viewport_rows = 2u;
    s_page_known = true;
    s_page_col = 2u;    /* the east edge, which is why it warped */
    s_page_row = 1u;
    s_call_viewport_set = 0;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_viewport_set, 1,
            "an east warp puts the desktop entered on a page of its"
            " own exactly once");
    TAP_EQ_INT(s_last_viewport_x, 0,
            "leaving by the east edge arrives at column 0, the west"
            " edge of the new desktop");
    TAP_EQ_INT(s_last_viewport_y, 1080,
            "and keeps the row it was travelling along, row 1");
}


/* The same warp on a single-page viewport has no edge to arrive by,
 * so the desktop entered is left exactly where it was */
static void s_test_tick_full_switch_single_page_untouched(void)
{
    desktop_td desktop;

    s_reset_for_full_east_switch();
    memset(&desktop, 0, sizeof(desktop));
    desktop.geometry.dim.w = 1920u;
    desktop.geometry.dim.h = 1080u;
    s_current_desktop = &desktop;
    s_viewport_columns = 1u;
    s_viewport_rows = 1u;
    s_call_viewport_set = 0;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_viewport_set, 0,
            "a single-page viewport is never repositioned by a warp");
}


/* drag_warp_tick: a full east switch on a horizontally-maximized move
 * drag (X locked) must leave the locked X axis exactly where it was,
 * the same invariant 's_drag_update_move' already enforces on every
 * ordinary motion notify; regression test for the desync where
 * 's_warp_move_dragged' ignored 'is_move_x_locked'/'is_move_y_locked'
 * entirely */
static void s_test_tick_full_switch_locked_x_axis_stays_put(void)
{
    s_reset_for_full_east_switch();
    s_drag.is_move_x_locked = true;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_xcb_warp_pointer, 1,
            "the pointer itself still warps across even though the"
            " client's X is locked: only the client's own position is"
            " pinned, never the pointer");
    TAP_EQ_INT(s_drag.client_cur.pos.x, 110,
            "client_cur.pos.x stays exactly at its pre-warp value: a"
            " locked move axis must never shift just because a warp"
            " fired instead of an ordinary motion update");
    TAP_EQ_INT(s_drag.client_cur.pos.y, 210,
            "the unlocked Y axis is untouched by a purely horizontal"
            " warp either way, locked or not");
    TAP_EQ_INT(s_enact_client_move_last_pos.x, 110,
            "the real window is likewise moved back to, never past,"
            " its locked X");
}


/* The same locked-axis regression, on the orthogonal (Y) axis, using a
 * north warp instead of east, so the locked axis is the one the warp
 * itself is actually along */
static void s_test_tick_full_switch_locked_y_axis_stays_put(void)
{
    s_reset_for_full_east_switch();
    s_drag.warp_direction = COMPASS_NORTH;
    s_drag.last_root_x = 500;
    s_drag.last_root_y = 0;
    s_drag.is_move_y_locked = true;

    drag_warp_tick((xcb_connection_t *) (void *) 1);

    TAP_EQ_INT(s_call_xcb_warp_pointer, 1,
            "a north warp still moves the pointer across regardless"
            " of the client's own Y lock");
    TAP_EQ_INT(s_drag.client_cur.pos.y, 210,
            "client_cur.pos.y stays exactly at its pre-warp value when"
            " the warp itself is along the locked axis");
    TAP_EQ_INT(s_drag.client_cur.pos.x, 110,
            "the orthogonal, unlocked X axis is untouched by a north"
            " warp either way, locked or not");
}


int main(void)
{
    TAP_PLAN(56);

    s_test_edge_check_no_client_clears_pending();
    s_test_edge_check_no_stage_is_noop();
    s_test_edge_check_disabled_in_config_is_noop();
    s_test_edge_check_single_desktop_is_noop();
    s_test_edge_check_middle_of_screen_is_noop();
    s_test_edge_check_left_edge_arms_west();
    s_test_edge_check_pan_available_defers_to_pan();
    s_test_edge_check_one_past_left_edge_is_noop();
    s_test_edge_check_right_edge_arms_east();
    s_test_edge_check_top_edge_arms_north();
    s_test_edge_check_bottom_edge_arms_south();
    s_test_edge_check_corner_prefers_horizontal();
    s_test_edge_check_same_edge_keeps_countdown();
    s_test_edge_check_different_edge_rearms();
    s_test_edge_check_leaving_edge_clears_pending();
    s_test_ms_remaining_returns_negative_when_idle();
    s_test_ms_remaining_positive_right_after_arming();
    s_test_ms_remaining_zero_once_due_has_passed();
    s_test_tick_null_connection_is_noop();
    s_test_tick_nothing_pending_is_noop();
    s_test_tick_countdown_not_due_is_noop();
    s_test_tick_due_no_client_stops_early();
    s_test_tick_due_wrong_operation_stops_early();
    s_test_tick_due_stale_drag_window_stops_early();
    s_test_tick_icon_drag_window_matches_is_allowed();
    s_test_tick_due_single_desktop_stops_early();
    s_test_tick_due_config_disabled_stops_early();
    s_test_tick_due_no_stage_stops_early();
    s_test_tick_full_switch_unlocked_axis_moves();
    s_test_tick_full_switch_enters_opposite_page();
    s_test_tick_full_switch_single_page_untouched();
    s_test_tick_full_switch_locked_x_axis_stays_put();
    s_test_tick_full_switch_locked_y_axis_stays_put();

    return TAP_DONE();
}
