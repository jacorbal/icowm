/**
 * @file tests/input/mouse/event/test_titlebar.c
 *
 * @brief Test battery for titlebar button presses (input/mouse/event/
 *        titlebar.c)
 *
 * s_mouse_hit_titlebar_buttons/im_press_titlebar dispatch button
 * clicks, double-click shade toggling, middle/right-click drag-area
 * gestures and scroll-wheel shade/unshade against a titlebar's button
 * layout.  client_titlebar_layout (client/geom.c) is the single source
 * of truth both the render pass and this file compute button positions
 * from, but its own placement arithmetic belongs to that module's own
 * tests, not this one, so it is a controllable stand-in here: what is
 * under test is only whether a click coordinate that falls on one of
 * the entries it reports dispatches the right enact_client_* call.
 * Every enact_client_*, wincmenu_show and drag_start call is a
 * recording stand-in, since each is a full subsystem covered
 * elsewhere.
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

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Policy includes */
#include <policy/focus.h>

/* Command includes */
#include <cmds/client/state.h>
#include <cmds/client/visibility.h>

/* Menu includes */
#include <menu/context/wincmenu.h>

/* Project includes */
#include <client.h>
#include <client/state.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <render/outdate.h>
#include <surface.h>
#include <wm.h>

/* Default initial values */
#include <defs/client.h>
#include <defs/input.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag.h>
#include <input/mouse/event.h>
#include <input/mouse/internal.h>


/** Controllable layout the next client_titlebar_layout call reports */
static struct titlebar_button_layout_s s_stub_left[CONFIG_MAX_TITLEBAR_BUTTONS];
static uint8_t s_stub_left_n;
static struct titlebar_button_layout_s s_stub_right[CONFIG_MAX_TITLEBAR_BUTTONS];
static uint8_t s_stub_right_n;
static int16_t s_stub_btn_y;
static int s_titlebar_layout_calls;

/** Recorded calls to each enact_client_* entry point this file
 *  exercises */
static int s_toggle_pin_calls;
static int s_toggle_stick_calls;
static int s_cycle_layer_calls;
static int s_iconify_calls;
static int s_hide_calls;
static int s_toggle_shade_calls;
static int s_maximize_calls;
static int s_maximize_vert_calls;
static int s_maximize_horz_calls;
static int s_toggle_fullscreen_calls;
static int s_close_calls;
static int s_shade_calls;
static int s_unshade_calls;
static int s_lower_calls;

/** Recorded calls to wincmenu_show and drag_start */
static int s_wincmenu_show_calls;
static int s_drag_start_calls;
static enum window_operation_e s_drag_start_operation;


/**
 * @brief Controllable stand-in for @a client_titlebar_layout
 * @note Complexity: @e O(1)
 */
void client_titlebar_layout(const struct config_theme_s *theme,
        uint16_t frame_w, uint16_t title_h, bool hide_pin,
        bool hide_sticky,
        struct titlebar_button_layout_s *restrict out_left,
        uint8_t *restrict out_left_n,
        struct titlebar_button_layout_s *restrict out_right,
        uint8_t *restrict out_right_n,
        int16_t *restrict out_title_x, uint16_t *restrict out_title_w,
        int16_t *restrict out_btn_y)
{
    uint8_t i;

    (void) theme;
    (void) frame_w;
    (void) title_h;
    (void) hide_pin;
    (void) hide_sticky;

    s_titlebar_layout_calls++;

    for (i = 0u; i < s_stub_left_n; ++i) {
        out_left[i] = s_stub_left[i];
    }
    *out_left_n = s_stub_left_n;

    for (i = 0u; i < s_stub_right_n; ++i) {
        out_right[i] = s_stub_right[i];
    }
    *out_right_n = s_stub_right_n;

    *out_title_x = 0;
    *out_title_w = 0u;
    *out_btn_y = s_stub_btn_y;
}


/**
 * @brief Recording stand-in for @a enact_client_toggle_pin
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_pin(client_td *client)
{
    (void) client;

    s_toggle_pin_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_toggle_stick
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_stick(client_td *client)
{
    (void) client;

    s_toggle_stick_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_cycle_layer
 * @note Complexity: @e O(1)
 */
void enact_client_cycle_layer(client_td *client)
{
    (void) client;

    s_cycle_layer_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_iconify
 * @note Complexity: @e O(1)
 */
void enact_client_iconify(client_td *client)
{
    (void) client;

    s_iconify_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_hide
 * @note Complexity: @e O(1)
 */
void enact_client_hide(client_td *client)
{
    (void) client;

    s_hide_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_toggle_shade
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_shade(client_td *client)
{
    (void) client;

    s_toggle_shade_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_maximize
 * @note Complexity: @e O(1)
 */
void enact_client_maximize(client_td *client)
{
    (void) client;

    s_maximize_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_maximize_vert
 * @note Complexity: @e O(1)
 */
void enact_client_maximize_vert(client_td *client)
{
    (void) client;

    s_maximize_vert_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_maximize_horz
 * @note Complexity: @e O(1)
 */
void enact_client_maximize_horz(client_td *client)
{
    (void) client;

    s_maximize_horz_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_toggle_fullscreen
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_fullscreen(client_td *client)
{
    (void) client;

    s_toggle_fullscreen_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_close
 * @note Complexity: @e O(1)
 */
void enact_client_close(client_td *client)
{
    (void) client;

    s_close_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_shade
 * @note Complexity: @e O(1)
 */
void enact_client_shade(client_td *client)
{
    (void) client;

    s_shade_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_unshade
 * @note Complexity: @e O(1)
 */
void enact_client_unshade(client_td *client)
{
    (void) client;

    s_unshade_calls++;
}


/**
 * @brief Recording stand-in for @a enact_client_lower
 * @note Complexity: @e O(1)
 */
void enact_client_lower(client_td *client)
{
    (void) client;

    s_lower_calls++;
}


/**
 * @brief Recording stand-in for @a wincmenu_show
 * @note Complexity: @e O(1)
 */
void wincmenu_show(xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, client_td *client, struct position_s pos,
        const config_td *config)
{
    (void) connection;
    (void) surface;
    (void) desktop;
    (void) client;
    (void) pos;
    (void) config;

    s_wincmenu_show_calls++;
}


/**
 * @brief Recording stand-in for @a drag_start
 * @note Complexity: @e O(1)
 */
void drag_start(xcb_connection_t *connection, xcb_window_t root,
        client_td *client, desktop_td *desktop,
        enum window_operation_e operation, xcb_timestamp_t event_time,
        struct position_s pointer_start, struct dimensions_s screen_dim)
{
    (void) connection;
    (void) root;
    (void) client;
    (void) desktop;
    (void) event_time;
    (void) pointer_start;
    (void) screen_dim;

    s_drag_start_calls++;
    s_drag_start_operation = operation;
}


static void s_reset(void)
{
    memset(s_stub_left, 0, sizeof(s_stub_left));
    s_stub_left_n = 0u;
    memset(s_stub_right, 0, sizeof(s_stub_right));
    s_stub_right_n = 0u;
    s_stub_btn_y = 0;
    s_titlebar_layout_calls = 0;

    s_toggle_pin_calls = 0;
    s_toggle_stick_calls = 0;
    s_cycle_layer_calls = 0;
    s_iconify_calls = 0;
    s_hide_calls = 0;
    s_toggle_shade_calls = 0;
    s_maximize_calls = 0;
    s_maximize_vert_calls = 0;
    s_maximize_horz_calls = 0;
    s_toggle_fullscreen_calls = 0;
    s_close_calls = 0;
    s_shade_calls = 0;
    s_unshade_calls = 0;
    s_lower_calls = 0;

    s_wincmenu_show_calls = 0;
    s_drag_start_calls = 0;
    s_drag_start_operation = CLIENT_OPERATION_IDLE;
}


/** Fixture config every test client points at; client_titlebar_layout
 *  is a controllable stand-in here that ignores its theme argument
 *  entirely, so only a non-null pointer matters, not its contents */
static config_td s_fixture_config;


static void s_make_client(client_td *client)
{
    memset(client, 0, sizeof(*client));
    memset(&s_fixture_config, 0, sizeof(s_fixture_config));
    client->layout.frame_extents.left = 2u;
    client->layout.frame_extents.right = 2u;
    client->layout.frame_extents.top = 24u;
    client->layout.geometry.cur.dim.w = 200u;
    client->title_height = 24u;
    client->titlebar = 55u;
    client->config = &s_fixture_config;
    client->properties.flags |= CLIENT_FLAG_RESIZABLE;
}


static xcb_button_press_event_t s_make_event(xcb_window_t root,
        uint8_t detail, int16_t event_x, int16_t event_y,
        xcb_timestamp_t time)
{
    xcb_button_press_event_t event;

    memset(&event, 0, sizeof(event));
    event.root = root;
    event.detail = detail;
    event.event_x = event_x;
    event.event_y = event_y;
    event.time = time;

    return event;
}


/* A click landing on the close button's X range dispatches
 * enact_client_close and marks the client outdated */
static void s_test_click_on_close_button_dispatches_close(void)
{
    client_td client;
    /* ex -= left_extent (2), ey -= title_y (0 here, since
     * top_extent (24) > title_h (24) is false, so title_y = 0):
     * button at frame-relative x=10 needs event_x = 10 + 2 = 12; btn_y
     * of 0 needs event_y within [0, WM_DECOR_BTN_SIZE) */
    xcb_button_press_event_t event = s_make_event(1, 1, 12, 5, 1000);

    s_reset();
    s_make_client(&client);
    s_stub_left[0].button = CONFIG_TITLEBAR_BUTTON_CLOSE;
    s_stub_left[0].x = 10;
    s_stub_left_n = 1u;
    s_stub_btn_y = 0;

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_close_calls, 1,
            "click within the close button's range: close dispatched");
    TAP_OK(client.is_outdated,
            "the client is marked outdated after a button dispatch");
}


/* Sticky button, button 1 click: sticky toggled, same as pin's own
 * click already dispatches enact_client_toggle_pin */
static void s_test_click_on_sticky_button_dispatches_toggle_stick(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 1, 12, 5, 1000);

    s_reset();
    s_make_client(&client);
    s_stub_left[0].button = CONFIG_TITLEBAR_BUTTON_STICKY;
    s_stub_left[0].x = 10;
    s_stub_left_n = 1u;
    s_stub_btn_y = 0;

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_toggle_stick_calls, 1,
            "click within the sticky button's range: toggle_stick"
            " dispatched");
}


/* A click just outside a button's WM_DECOR_BTN_SIZE-wide range misses
 * it entirely; since no other button matches and it is not button 1,
 * nothing at all is dispatched */
static void s_test_click_outside_button_range_misses(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 1,
            10 + 2 + (int16_t) WM_DECOR_BTN_SIZE, 5, 1000);

    s_reset();
    s_make_client(&client);
    s_stub_left[0].button = CONFIG_TITLEBAR_BUTTON_CLOSE;
    s_stub_left[0].x = 10;
    s_stub_left_n = 1u;
    s_stub_btn_y = 0;

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_close_calls, 0,
            "click one pixel past the button's range: not dispatched");
}


/* Maximize button, button 1 click: full maximize */
static void s_test_maximize_button_1_dispatches_full_maximize(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 1, 12, 5, 1000);

    s_reset();
    s_make_client(&client);
    s_stub_left[0].button = CONFIG_TITLEBAR_BUTTON_MAXIMIZE;
    s_stub_left[0].x = 10;
    s_stub_left_n = 1u;

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_maximize_calls, 1,
            "maximize button, button 1: full maximize dispatched");
    TAP_EQ_INT(s_maximize_vert_calls, 0, "not the vertical variant");
    TAP_EQ_INT(s_maximize_horz_calls, 0, "nor the horizontal one");
}


/* Maximize button, button 2 click: vertical-only maximize */
static void s_test_maximize_button_2_dispatches_vertical(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 2, 12, 5, 1000);

    s_reset();
    s_make_client(&client);
    s_stub_left[0].button = CONFIG_TITLEBAR_BUTTON_MAXIMIZE;
    s_stub_left[0].x = 10;
    s_stub_left_n = 1u;

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_maximize_vert_calls, 1,
            "maximize button, button 2: vertical maximize dispatched");
}


/* Maximize button, button 3 click: horizontal-only maximize */
static void s_test_maximize_button_3_dispatches_horizontal(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 3, 12, 5, 1000);

    s_reset();
    s_make_client(&client);
    s_stub_left[0].button = CONFIG_TITLEBAR_BUTTON_MAXIMIZE;
    s_stub_left[0].x = 10;
    s_stub_left_n = 1u;

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_maximize_horz_calls, 1,
            "maximize button, button 3: horizontal maximize dispatched");
}


/* client->config == NULL: s_mouse_hit_titlebar_buttons bails out
 * before calling client_titlebar_layout at all, and no button is ever
 * dispatched even where a click coordinate would otherwise match */
static void s_test_null_config_skips_button_hit_test(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 1, 12, 5, 1000);

    s_reset();
    s_make_client(&client);
    client.config = NULL;
    s_stub_left[0].button = CONFIG_TITLEBAR_BUTTON_CLOSE;
    s_stub_left[0].x = 10;
    s_stub_left_n = 1u;

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_titlebar_layout_calls, 0,
            "client->config is null: client_titlebar_layout never"
            " called");
    TAP_EQ_INT(s_close_calls, 0, "and no button is dispatched");
}


/* Scroll-wheel button 4 on the titlebar body (no button hit): shades
 * an unshaded client */
static void s_test_scroll_up_shades_unshaded_client(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 4, 100, 5, 1000);

    s_reset();
    s_make_client(&client);

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_shade_calls, 1,
            "scroll button 4 on titlebar body: shade dispatched");
}


/* Scroll-wheel button 5 on the titlebar body: unshades a shaded
 * client */
static void s_test_scroll_down_unshades_shaded_client(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 5, 100, 5, 1000);

    s_reset();
    s_make_client(&client);
    client.properties.flags |= CLIENT_FLAG_SHADED;

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_unshade_calls, 1,
            "scroll button 5 on titlebar body: unshade dispatched");
}


/* Single left-click on the titlebar drag area (no button hit, no
 * prior press) starts a move drag rather than toggling shade */
static void s_test_single_left_click_starts_move_drag(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 1, 100, 5, 5000);
    bool started;

    s_reset();
    s_make_client(&client);
    /* No configured buttons at all: click lands on plain drag area */

    started = im_press_titlebar((xcb_connection_t *) 1, NULL, &event,
            &client, NULL, NULL, NULL);

    TAP_EQ_INT(s_drag_start_calls, 1,
            "single left-click on drag area: drag_start dispatched"
            " once");
    TAP_OK(s_drag_start_operation == CLIENT_OPERATION_MOVING,
            "the drag started is a move");
    TAP_EQ_INT(s_toggle_shade_calls, 0, "not a shade toggle");
    TAP_OK(started,
            "reports that a move drag started, so the caller returns"
            " immediately instead of racing the fresh grab");
}


/* Two left-clicks in quick succession on the same titlebar toggle
 * shade instead of starting a second drag */
static void s_test_double_click_toggles_shade(void)
{
    client_td client;
    xcb_button_press_event_t first = s_make_event(1, 1, 100, 5, 1000);
    xcb_button_press_event_t second = s_make_event(1, 1, 100, 5, 1100);
    config_td config;
    bool first_started;
    bool second_started;

    s_reset();
    s_make_client(&client);
    memset(&config, 0, sizeof(config));
    config.a11y.interaction.double_click_ms = 400u;

    first_started = im_press_titlebar((xcb_connection_t *) 1, NULL,
            &first, &client, NULL, NULL, &config);
    second_started = im_press_titlebar((xcb_connection_t *) 1, NULL,
            &second, &client, NULL, NULL, &config);

    TAP_EQ_INT(s_drag_start_calls, 1,
            "first click starts a drag, second (double-click) does not");
    TAP_EQ_INT(s_toggle_shade_calls, 1,
            "second click within the double-click window toggles"
            " shade");
    TAP_OK(first_started, "first click reports a drag started");
    TAP_OK(!second_started,
            "second (double-click) reports no drag started");
}


/* A left click on an already-maximized client's drag area does not
 * start a move drag */
static void s_test_left_click_on_maximized_skips_drag(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 1, 100, 5, 9000);
    bool started;

    s_reset();
    s_make_client(&client);
    client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED;

    started = im_press_titlebar((xcb_connection_t *) 1, NULL, &event,
            &client, NULL, NULL, NULL);

    TAP_EQ_INT(s_drag_start_calls, 0,
            "maximized client, drag area click: no move drag started");
    TAP_OK(!started, "maximized client: reports no drag started");
}


/* A left click on a fullscreen client's drag area does not start a
 * move drag either */
static void s_test_left_click_on_fullscreen_skips_drag(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 1, 100, 5, 9500);
    bool started;

    s_reset();
    s_make_client(&client);
    client.properties.state = (uint16_t) CLIENT_STATE_FULLSCREEN;

    started = im_press_titlebar((xcb_connection_t *) 1, NULL, &event,
            &client, NULL, NULL, NULL);

    TAP_EQ_INT(s_drag_start_calls, 0,
            "fullscreen client, drag area click: no move drag started");
    TAP_OK(!started, "fullscreen client: reports no drag started");
}


/* Middle-click on the drag area lowers the client */
static void s_test_middle_click_lowers_client(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 2, 100, 5, 1000);

    s_reset();
    s_make_client(&client);

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_lower_calls, 1,
            "middle-click on drag area: lower dispatched");
}


/* Right-click on the drag area (no button hit) opens the window
 * context menu, only when both surface and desktop are available */
static void s_test_right_click_opens_wincmenu(void)
{
    client_td client;
    desktop_td desktop;
    surface_td surface;
    xcb_button_press_event_t event = s_make_event(1, 3, 100, 5, 1000);

    s_reset();
    s_make_client(&client);
    memset(&desktop, 0, sizeof(desktop));
    memset(&surface, 0, sizeof(surface));

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            &desktop, &surface, NULL);

    TAP_EQ_INT(s_wincmenu_show_calls, 1,
            "right-click on drag area, surface and desktop present:"
            " window menu shown");
}


/* Right-click on the drag area with no surface or desktop resolved:
 * the window menu never shows */
static void s_test_right_click_without_surface_skips_wincmenu(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 3, 100, 5, 1000);

    s_reset();
    s_make_client(&client);

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_wincmenu_show_calls, 0,
            "right-click, no surface/desktop: window menu not shown");
}


/* A button hit on the middle-click path takes priority: the client is
 * not also lowered when the click landed on a configured button */
static void s_test_button_hit_suppresses_middle_click_lower(void)
{
    client_td client;
    xcb_button_press_event_t event = s_make_event(1, 2, 12, 5, 1000);

    s_reset();
    s_make_client(&client);
    s_stub_left[0].button = CONFIG_TITLEBAR_BUTTON_HIDE;
    s_stub_left[0].x = 10;
    s_stub_left_n = 1u;
    s_stub_btn_y = 0;

    im_press_titlebar((xcb_connection_t *) 1, NULL, &event, &client,
            NULL, NULL, NULL);

    TAP_EQ_INT(s_hide_calls, 1, "button 2 on a configured button: hide"
            " dispatched (button wins the dispatch, not maximize"
            " variant selection, since this button is HIDE)");
    TAP_EQ_INT(s_lower_calls, 0,
            "and the middle-click lower gesture is suppressed");
}


int main(void)
{
    TAP_PLAN(25);

    s_test_click_on_close_button_dispatches_close();
    s_test_click_on_sticky_button_dispatches_toggle_stick();
    s_test_click_outside_button_range_misses();
    s_test_maximize_button_1_dispatches_full_maximize();
    s_test_maximize_button_2_dispatches_vertical();
    s_test_maximize_button_3_dispatches_horizontal();
    s_test_null_config_skips_button_hit_test();
    s_test_scroll_up_shades_unshaded_client();
    s_test_scroll_down_unshades_shaded_client();
    s_test_single_left_click_starts_move_drag();
    s_test_double_click_toggles_shade();
    s_test_left_click_on_maximized_skips_drag();
    s_test_left_click_on_fullscreen_skips_drag();
    s_test_middle_click_lowers_client();
    s_test_right_click_opens_wincmenu();
    s_test_right_click_without_surface_skips_wincmenu();
    s_test_button_hit_suppresses_middle_click_lower();

    return TAP_DONE();
}
