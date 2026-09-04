/**
 * @file tests/menu/dialog/test_info.c
 *
 * @brief Test battery for the informational message dialog thin
 *        wrapper
 *
 * 'info.c' has no logic of its own beyond forwarding each call
 * straight through to its matching 'menu_message_dialog_*' entry
 * point (menu/dialog/message.h), so all six of those are link-only,
 * recording stand-ins below rather than the real message.c: this file
 * exists to check that every argument reaches its counterpart
 * unchanged and that every return value comes back unchanged, not to
 * re-exercise message.c's own dialog behavior, which has its own test
 * battery.
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

/* Project includes */
#include <config.h>
#include <surface.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/dialog/info.h>
#include <menu/dialog/message.h>


/** Fake, non-null XCB connection/surface/config handles, standing in
 *  for live ones wherever info.c merely forwards them onward without
 *  ever dereferencing them itself */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;
static surface_td s_fake_surface;
static config_td s_fake_config;

/** Recording stand-ins' own call counters and captured arguments,
 *  reset by s_reset between scenarios */
static int s_call_show;
static xcb_connection_t *s_show_connection;
static surface_td *s_show_surface;
static const config_td *s_show_config;
static const char *s_show_message;
static menu_msg_level_e s_show_level;

static int s_call_close;
static xcb_connection_t *s_close_connection;

static int s_call_repaint;
static xcb_connection_t *s_repaint_connection;
static const config_td *s_repaint_config;

static int s_call_handle_click;
static xcb_connection_t *s_click_connection;
static const config_td *s_click_config;
static int s_click_x;
static int s_click_y;

static int s_call_is_open;
static bool s_stub_is_open;

static int s_call_window;
static xcb_window_t s_stub_window;


/**
 * @brief Reset every stand-in's recorded call state between scenarios
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_call_show = 0;
    s_show_connection = NULL;
    s_show_surface = NULL;
    s_show_config = NULL;
    s_show_message = NULL;
    s_show_level = MENU_MSG_LEVEL_NONE;

    s_call_close = 0;
    s_close_connection = NULL;

    s_call_repaint = 0;
    s_repaint_connection = NULL;
    s_repaint_config = NULL;

    s_call_handle_click = 0;
    s_click_connection = NULL;
    s_click_config = NULL;
    s_click_x = 0;
    s_click_y = 0;

    s_call_is_open = 0;
    s_stub_is_open = false;

    s_call_window = 0;
    s_stub_window = XCB_WINDOW_NONE;
}


/**
 * @brief Recording stand-in for @a menu_message_dialog_show
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_show(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const char *message, menu_msg_level_e level)
{
    s_call_show++;
    s_show_connection = connection;
    s_show_surface = surface;
    s_show_config = config;
    s_show_message = message;
    s_show_level = level;
}


/**
 * @brief Recording stand-in for @a menu_message_dialog_show_pairs
 *
 * Never exercised by info.c, which never calls it, but its symbol
 * still must resolve at link time since it lives in the same header
 * as the five entry points info.c does forward to.
 *
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_show_pairs(xcb_connection_t *connection,
        surface_td *surface, const config_td *config,
        const struct dialog_pair_s *pairs, size_t count,
        menu_msg_level_e level)
{
    (void) connection;
    (void) surface;
    (void) config;
    (void) pairs;
    (void) count;
    (void) level;
}


/**
 * @brief Recording stand-in for @a menu_message_dialog_close
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_close(xcb_connection_t *connection)
{
    s_call_close++;
    s_close_connection = connection;
}


/**
 * @brief Recording stand-in for @a menu_message_dialog_repaint
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_repaint(xcb_connection_t *connection,
        const config_td *config)
{
    s_call_repaint++;
    s_repaint_connection = connection;
    s_repaint_config = config;
}


/**
 * @brief Recording stand-in for @a menu_message_dialog_handle_click
 * @note Complexity: @e O(1)
 */
void menu_message_dialog_handle_click(xcb_connection_t *connection,
        const config_td *config, int x, int y)
{
    s_call_handle_click++;
    s_click_connection = connection;
    s_click_config = config;
    s_click_x = x;
    s_click_y = y;
}


/**
 * @brief Test-controlled stand-in for @a menu_message_dialog_is_open
 * @note Complexity: @e O(1)
 */
bool menu_message_dialog_is_open(void)
{
    s_call_is_open++;
    return s_stub_is_open;
}


/**
 * @brief Test-controlled stand-in for @a menu_message_dialog_window
 * @note Complexity: @e O(1)
 */
xcb_window_t menu_message_dialog_window(void)
{
    s_call_window++;
    return s_stub_window;
}


/* dialog_info_show forwards every argument, unchanged, straight to
 * menu_message_dialog_show */
static void s_test_show_forwards_arguments(void)
{
    s_reset();
    dialog_info_show(s_fake_connection, &s_fake_surface, &s_fake_config,
            "no such file", MENU_MSG_LEVEL_WARNING);

    TAP_EQ_INT(s_call_show, 1, "info_show: forwards to show exactly once");
    TAP_OK(s_show_connection == s_fake_connection,
            "info_show: forwards the connection unchanged");
    TAP_OK(s_show_surface == &s_fake_surface,
            "info_show: forwards the surface unchanged");
    TAP_OK(s_show_config == &s_fake_config,
            "info_show: forwards the config unchanged");
    TAP_EQ_STR(s_show_message, "no such file",
            "info_show: forwards the message text unchanged");
    TAP_EQ_INT((int) s_show_level, (int) MENU_MSG_LEVEL_WARNING,
            "info_show: forwards the alert level unchanged");
}


/* dialog_info_close forwards its connection to menu_message_dialog_close */
static void s_test_close_forwards_connection(void)
{
    s_reset();
    dialog_info_close(s_fake_connection);

    TAP_EQ_INT(s_call_close, 1, "info_close: forwards to close exactly"
            " once");
    TAP_OK(s_close_connection == s_fake_connection,
            "info_close: forwards the connection unchanged");
}


/* dialog_info_repaint forwards both its arguments to
 * menu_message_dialog_repaint */
static void s_test_repaint_forwards_arguments(void)
{
    s_reset();
    dialog_info_repaint(s_fake_connection, &s_fake_config);

    TAP_EQ_INT(s_call_repaint, 1, "info_repaint: forwards to repaint"
            " exactly once");
    TAP_OK(s_repaint_connection == s_fake_connection,
            "info_repaint: forwards the connection unchanged");
    TAP_OK(s_repaint_config == &s_fake_config,
            "info_repaint: forwards the config unchanged");
}


/* dialog_info_handle_click forwards every argument, including negative
 * coordinates, unchanged */
static void s_test_handle_click_forwards_arguments(void)
{
    s_reset();
    dialog_info_handle_click(s_fake_connection, &s_fake_config, -3, 17);

    TAP_EQ_INT(s_call_handle_click, 1, "info_handle_click: forwards to"
            " handle_click exactly once");
    TAP_OK(s_click_connection == s_fake_connection,
            "info_handle_click: forwards the connection unchanged");
    TAP_OK(s_click_config == &s_fake_config,
            "info_handle_click: forwards the config unchanged");
    TAP_EQ_INT(s_click_x, -3,
            "info_handle_click: forwards a negative x unchanged");
    TAP_EQ_INT(s_click_y, 17,
            "info_handle_click: forwards y unchanged");
}


/* dialog_info_is_open forwards menu_message_dialog_is_open's answer
 * back unchanged, both when it is open and when it is not */
static void s_test_is_open_forwards_result(void)
{
    s_reset();
    s_stub_is_open = true;
    TAP_OK(dialog_info_is_open() == true,
            "info_is_open: forwards a true answer unchanged");
    TAP_EQ_INT(s_call_is_open, 1,
            "info_is_open: forwards to is_open exactly once");

    s_reset();
    s_stub_is_open = false;
    TAP_OK(dialog_info_is_open() == false,
            "info_is_open: forwards a false answer unchanged");
}


/* dialog_info_window forwards menu_message_dialog_window's answer back
 * unchanged */
static void s_test_window_forwards_result(void)
{
    s_reset();
    s_stub_window = (xcb_window_t) 777u;
    TAP_EQ_INT((int) dialog_info_window(), 777,
            "info_window: forwards the window identifier unchanged");
    TAP_EQ_INT(s_call_window, 1,
            "info_window: forwards to window exactly once");
}


int main(void)
{
    TAP_PLAN(21);

    s_test_show_forwards_arguments();
    s_test_close_forwards_connection();
    s_test_repaint_forwards_arguments();
    s_test_handle_click_forwards_arguments();
    s_test_is_open_forwards_result();
    s_test_window_forwards_result();

    return TAP_DONE();
}
