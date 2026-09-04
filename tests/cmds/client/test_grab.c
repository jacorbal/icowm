/**
 * @file tests/cmds/client/test_grab.c
 *
 * @brief Test battery for a client's passive mouse-button grabs
 *
 * Exercises 'ccmd_client_grab_buttons' and 'ccmd_client_ungrab_buttons'
 * (cmds/client/grab.c) linked against nothing else from the project:
 * the file under test calls only 'xcb_connection_get' (a project
 * wrapper, stubbed here as test-controlled) and the two real libxcb
 * request builders 'xcb_grab_button'/'xcb_ungrab_button', which this
 * file replaces with its own recording stand-ins rather than linking
 * '-lxcb', so a call reaching either one is observed directly instead
 * of actually going out over a socket that has no live X server on
 * the other end.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <client.h>
#include <cmds/client/grab.h>
#include <harness/tap.h>


/**
 * @brief Test-controlled stand-in for @a xcb_connection_get
 *
 * Answers @c NULL by default, matching a window manager with no live
 * X connection; a test opts into the "connected" path by pointing
 * this at any non-null address, never actually dereferenced by either
 * function under test.
 *
 * @note Complexity: @e O(1)
 */
static xcb_connection_t *s_connection;

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection;
}


/** Every button 'xcb_grab_button' was asked to grab, in call order,
 *  cleared by @a s_reset */
#define GRAB_LOG_CAP (16)
static uint8_t s_grab_button_log[GRAB_LOG_CAP];
static xcb_window_t s_grab_window_log[GRAB_LOG_CAP];
static int s_grab_log_used;


/**
 * @brief Recording stand-in for @a xcb_grab_button
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_grab_button(xcb_connection_t *c,
        uint8_t owner_events, xcb_window_t grab_window,
        uint16_t event_mask, uint8_t pointer_mode,
        uint8_t keyboard_mode, xcb_window_t confine_to,
        xcb_cursor_t cursor, uint8_t button, uint16_t modifiers)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) owner_events;
    (void) event_mask;
    (void) pointer_mode;
    (void) keyboard_mode;
    (void) confine_to;
    (void) cursor;
    (void) modifiers;

    if (s_grab_log_used < GRAB_LOG_CAP) {
        s_grab_button_log[s_grab_log_used] = button;
        s_grab_window_log[s_grab_log_used] = grab_window;
        s_grab_log_used++;
    }

    cookie.sequence = 0u;
    return cookie;
}


/** Every window 'xcb_ungrab_button' was asked to release, in call
 *  order, cleared by @a s_reset */
#define UNGRAB_LOG_CAP (16)
static xcb_window_t s_ungrab_window_log[UNGRAB_LOG_CAP];
static uint8_t s_ungrab_button_log[UNGRAB_LOG_CAP];
static uint16_t s_ungrab_modifiers_log[UNGRAB_LOG_CAP];
static int s_ungrab_log_used;


/**
 * @brief Recording stand-in for @a xcb_ungrab_button
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ungrab_button(xcb_connection_t *c, uint8_t button,
        xcb_window_t grab_window, uint16_t modifiers)
{
    xcb_void_cookie_t cookie;

    (void) c;

    if (s_ungrab_log_used < UNGRAB_LOG_CAP) {
        s_ungrab_button_log[s_ungrab_log_used] = button;
        s_ungrab_window_log[s_ungrab_log_used] = grab_window;
        s_ungrab_modifiers_log[s_ungrab_log_used] = modifiers;
        s_ungrab_log_used++;
    }

    cookie.sequence = 0u;
    return cookie;
}


/** Every client this file calloc's, freed in one place by
 *  @a s_teardown rather than at each test's own end */
#define MAX_TEST_CLIENTS (8)
static client_td *s_owned_clients[MAX_TEST_CLIENTS];
static int s_owned_clients_used;


static client_td *s_make_client(uint32_t id)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    client->window = (xcb_window_t) id;
    s_owned_clients[s_owned_clients_used] = client;
    s_owned_clients_used++;

    return client;
}


static void s_reset(void)
{
    /* Any non-null value serves as the "connected" stand-in; never
     * dereferenced by either function under test, only compared
     * against 'NULL' */
    static int s_fake_connection_storage;

    s_connection = (xcb_connection_t *) &s_fake_connection_storage;
    s_grab_log_used = 0;
    s_ungrab_log_used = 0;
}


static void s_teardown(void)
{
    for (int i = 0; i < s_owned_clients_used; ++i) {
        free(s_owned_clients[i]);
    }
    s_owned_clients_used = 0;
}


/* A null client is a silent no-op on both functions: no grab or
 * ungrab request is ever built */
static void s_test_null_client_is_a_no_op(void)
{
    s_reset();

    ccmd_client_grab_buttons(NULL);
    ccmd_client_ungrab_buttons(NULL);

    TAP_EQ_INT(s_grab_log_used, 0,
            "a null client never reaches a grab-button request");
    TAP_EQ_INT(s_ungrab_log_used, 0,
            "and never reaches an ungrab-button request either");

    s_teardown();
}


/* With no live X connection, a real client is still a no-op: the
 * guard fires before either window or connection is ever touched */
static void s_test_no_connection_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    s_connection = NULL;
    client = s_make_client(1u);

    ccmd_client_grab_buttons(client);
    ccmd_client_ungrab_buttons(client);

    TAP_EQ_INT(s_grab_log_used, 0,
            "no grab request is built without a live connection");
    TAP_EQ_INT(s_ungrab_log_used, 0,
            "no ungrab request is built without a live connection either");

    s_teardown();
}


/* A client whose own window is 'XCB_WINDOW_NONE' is refused too, even
 * with a live connection: there is nothing real to grab buttons on */
static void s_test_none_window_is_a_no_op(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(0u);
    client->window = XCB_WINDOW_NONE;

    ccmd_client_grab_buttons(client);
    ccmd_client_ungrab_buttons(client);

    TAP_EQ_INT(s_grab_log_used, 0,
            "a 'XCB_WINDOW_NONE' client never reaches a grab request");
    TAP_EQ_INT(s_ungrab_log_used, 0,
            "nor an ungrab request");

    s_teardown();
}


/* Grabbing buttons on a genuine client issues exactly five requests
 * (three ordinary buttons plus the two explicit non-wheel indices 6
 * and 7), every one of them on that client's own window */
static void s_test_grab_issues_five_button_requests(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(42u);

    ccmd_client_grab_buttons(client);

    TAP_EQ_INT(s_grab_log_used, 5,
            "exactly five passive grabs are installed");
    TAP_EQ_INT((long) s_grab_button_log[0], (long) XCB_BUTTON_INDEX_1,
            "the first grab covers button 1");
    TAP_EQ_INT((long) s_grab_button_log[1], (long) XCB_BUTTON_INDEX_2,
            "the second grab covers button 2");
    TAP_EQ_INT((long) s_grab_button_log[2], (long) XCB_BUTTON_INDEX_3,
            "the third grab covers button 3");
    TAP_EQ_INT((long) s_grab_button_log[3], 6,
            "the fourth grab covers the extra button 6");
    TAP_EQ_INT((long) s_grab_button_log[4], 7,
            "the fifth grab covers the extra button 7");
    TAP_EQ_INT((long) s_grab_window_log[0], (long) client->window,
            "every grab targets the client's own window");
    TAP_EQ_INT((long) s_grab_window_log[4], (long) client->window,
            "including the very last one of the five");

    s_teardown();
}


/* The scroll-wheel buttons 4 and 5 are deliberately never grabbed,
 * so scroll events reach the application directly */
static void s_test_grab_never_covers_scroll_wheel(void)
{
    client_td *client;
    bool saw_button_4 = false;
    bool saw_button_5 = false;

    s_reset();
    client = s_make_client(43u);

    ccmd_client_grab_buttons(client);

    for (int i = 0; i < s_grab_log_used; ++i) {
        if (s_grab_button_log[i] == 4u) {
            saw_button_4 = true;
        }
        if (s_grab_button_log[i] == 5u) {
            saw_button_5 = true;
        }
    }

    TAP_OK(!saw_button_4, "button 4 (scroll up) is never grabbed");
    TAP_OK(!saw_button_5, "button 5 (scroll down) is never grabbed");

    s_teardown();
}


/* Ungrabbing buttons on a genuine client issues exactly one request,
 * naming 'XCB_BUTTON_INDEX_ANY' and 'XCB_MOD_MASK_ANY' so every
 * grab installed above is released in one call */
static void s_test_ungrab_issues_one_any_request(void)
{
    client_td *client;

    s_reset();
    client = s_make_client(44u);

    ccmd_client_ungrab_buttons(client);

    TAP_EQ_INT(s_ungrab_log_used, 1,
            "exactly one ungrab-button request is issued");
    TAP_EQ_INT((long) s_ungrab_window_log[0], (long) client->window,
            "targeting the client's own window");
    TAP_EQ_INT((long) s_ungrab_button_log[0],
            (long) (uint8_t) XCB_BUTTON_INDEX_ANY,
            "naming every button via 'XCB_BUTTON_INDEX_ANY'");
    TAP_EQ_INT((long) s_ungrab_modifiers_log[0],
            (long) (uint16_t) XCB_MOD_MASK_ANY,
            "and every modifier combination via 'XCB_MOD_MASK_ANY'");

    s_teardown();
}


/* Two different clients never share a logged window: each grab call
 * is independent of any other client that may have been grabbed
 * before it */
static void s_test_grab_is_independent_per_client(void)
{
    client_td *first;
    client_td *second;

    s_reset();
    first = s_make_client(50u);
    second = s_make_client(51u);

    ccmd_client_grab_buttons(first);
    ccmd_client_grab_buttons(second);

    TAP_EQ_INT(s_grab_log_used, 10,
            "two clients each contribute their own five grabs");
    TAP_EQ_INT((long) s_grab_window_log[0], (long) first->window,
            "the first client's grabs are logged first");
    TAP_EQ_INT((long) s_grab_window_log[5], (long) second->window,
            "the second client's grabs follow, on its own window");

    s_teardown();
}


int main(void)
{
    TAP_PLAN(23);

    s_test_null_client_is_a_no_op();
    s_test_no_connection_is_a_no_op();
    s_test_none_window_is_a_no_op();
    s_test_grab_issues_five_button_requests();
    s_test_grab_never_covers_scroll_wheel();
    s_test_ungrab_issues_one_any_request();
    s_test_grab_is_independent_per_client();

    return TAP_DONE();
}
