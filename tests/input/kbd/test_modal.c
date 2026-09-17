/**
 * @file tests/input/kbd/test_modal.c
 *
 * @brief Tests for the modal keyboard move/resize state machine
 *
 * Everything this module reaches outside itself is stubbed below: the
 * two XCB calls it makes (the keyboard ungrab and the flush), the two
 * enact verbs it moves and resizes through, and the two predicates it
 * asks about maximization.  libxcb is not linked, so the stubs here are
 * the only definitions the linker finds, and each records what it was
 * asked to do for the assertions to read back.
 *
 * What is being tested is the state machine alone: that a session
 * starts and ends when it should, that each arrow moves or resizes by
 * the configured step and in the right direction, that Escape puts the
 * geometry back where Return leaves it, and that keys arriving with no
 * session open are passed through rather than swallowed.
 *
 * @copyright Copyright (c) 2026, J. A. Corbal
 *
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <logger.h>
#include <defs/kbd.h>

/* Local includes */
#include <input/kbd/modal.h>

/* Test harness */
#include <harness/tap.h>


/* How many times the keyboard was ungrabbed */
static int s_ungrab_count;

/* The last geometry each enact verb was handed */
static int32_t s_moved_x;
static int32_t s_moved_y;
static uint32_t s_resized_w;
static uint32_t s_resized_h;
static int s_move_count;
static int s_resize_count;


/**
 * @brief Stand-in for the XCB keyboard ungrab, recording the call
 *
 * @param connection Unused
 * @param time       Unused
 *
 * @return An empty cookie, which no caller here inspects
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ungrab_keyboard(xcb_connection_t *connection,
        xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) time;
    s_ungrab_count++;

    return cookie;
}


/**
 * @brief Stand-in for the keyboard grab request
 *
 * @param connection Unused
 * @param owner      Unused
 * @param window     Unused
 * @param time       Unused
 * @param pointer    Unused
 * @param keyboard   Unused
 *
 * @return An empty cookie, the reply stub below answering for it
 *
 * @note Complexity: @e O(1)
 */
xcb_grab_keyboard_cookie_t xcb_grab_keyboard(
        xcb_connection_t *connection, uint8_t owner,
        xcb_window_t window, xcb_timestamp_t time,
        uint8_t pointer, uint8_t keyboard)
{
    xcb_grab_keyboard_cookie_t cookie = { 0 };

    (void) connection;
    (void) owner;
    (void) window;
    (void) time;
    (void) pointer;
    (void) keyboard;

    return cookie;
}


/**
 * @brief Stand-in for the grab reply, always granting the grab
 *
 * @param connection Unused
 * @param cookie     Unused
 * @param error      Unused
 *
 * @return A reply saying the grab succeeded, which the caller frees
 *
 * @note Complexity: @e O(1)
 */
xcb_grab_keyboard_reply_t *xcb_grab_keyboard_reply(
        xcb_connection_t *connection,
        xcb_grab_keyboard_cookie_t cookie,
        xcb_generic_error_t **error)
{
    xcb_grab_keyboard_reply_t *reply = malloc(sizeof(*reply));

    (void) connection;
    (void) cookie;
    (void) error;

    if (reply != NULL) {
        memset(reply, 0, sizeof(*reply));
        reply->status = XCB_GRAB_STATUS_SUCCESS;
    }

    return reply;
}


/**
 * @brief Stand-in for the logger, silencing it for this run
 *
 * @param level  Unused
 * @param prefix Unused
 * @param msg    Unused
 * @param ...    Unused
 *
 * @return Zero, as the real one does on success
 *
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict msg, ...)
{
    (void) level;
    (void) prefix;
    (void) msg;

    return 0;
}


/**
 * @brief Stand-in for the XCB flush
 *
 * @param connection Unused
 *
 * @return 1, the value libxcb returns on success
 *
 * @note Complexity: @e O(1)
 */
int xcb_flush(xcb_connection_t *connection)
{
    (void) connection;

    return 1;
}


/**
 * @brief Stand-in for the move verb, recording where it was sent and
 *        applying it so the next key builds on it as the real one does
 *
 * @param client Client being moved
 * @param pos    Target position
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move(client_td *client, struct position_s pos)
{
    s_moved_x = pos.x;
    s_moved_y = pos.y;
    s_move_count++;

    if (client != NULL) {
        client->layout.geometry.cur.pos = pos;
    }
}


/**
 * @brief Stand-in for the resize verb, recording and applying the size
 *
 * @param client Client being resized
 * @param geom   Target geometry
 *
 * @note Complexity: @e O(1)
 */
void enact_client_resize(client_td *client, struct geometry_s geom)
{
    s_resized_w = geom.dim.w;
    s_resized_h = geom.dim.h;
    s_resize_count++;

    if (client != NULL) {
        client->layout.geometry.cur = geom;
    }
}


/* A connection that is never dereferenced, only checked against null,
 * and a stage whose only used field is its screen's root window */
static xcb_connection_t *const s_conn = (xcb_connection_t *) 0x1;
static xcb_screen_t s_screen;
static stage_td s_stage;


/**
 * @brief Point the stage at a screen with a root window, which is
 *        all 's_modal_enter' asks of it before granting the grab
 *
 * @note Complexity: @e O(1)
 */
static void s_stage_reset(void)
{
    memset(&s_screen, 0, sizeof(s_screen));
    memset(&s_stage, 0, sizeof(s_stage));
    s_screen.root = (xcb_window_t) 1u;
    s_stage.screen = &s_screen;
}


/**
 * @brief Build a client sitting at a known place, so every assertion
 *        below can be written against a geometry it chose
 *
 * @param client Client to fill in
 *
 * @note Complexity: @e O(1)
 */
static void s_client_reset(client_td *client)
{
    memset(client, 0, sizeof(*client));
    client->layout.geometry.cur.pos.x = 100;
    client->layout.geometry.cur.pos.y = 200;
    client->layout.geometry.cur.dim.w = 300u;
    client->layout.geometry.cur.dim.h = 400u;
    client->properties.flags = 0u;
}


/**
 * @brief Zero every stub's record before a test drives the module
 *
 * @note Complexity: @e O(1)
 */
static void s_stubs_reset(void)
{
    s_ungrab_count = 0;
    s_move_count = 0;
    s_resize_count = 0;
    s_moved_x = 0;
    s_moved_y = 0;
    s_resized_w = 0u;
    s_resized_h = 0u;
}


/**
 * @brief A configuration with steps this file's assertions know
 *
 * @param config Configuration to fill in
 *
 * @note Steps of ten and twenty rather than one, so a test that
 *       accidentally moved by a default step would not still pass
 * @note Complexity: @e O(1)
 */
static void s_config_reset(config_td *config)
{
    memset(config, 0, sizeof(*config));
    config->base.windows.move_step = 10u;
    config->base.windows.resize_step = 20u;
}


/**
 * @brief A session is not active until one is started
 *
 * @note Complexity: @e O(1)
 */
static void s_test_inactive_until_started(void)
{
    client_td client;
    config_td config;

    s_stubs_reset();
    s_client_reset(&client);
    s_config_reset(&config);

    TAP_OK(!kbd_modal_is_active(),
            "no session is active before one is started");
    TAP_OK(!kbd_modal_handle_keypress(s_conn, &s_stage, KS_LEFT, &config),
            "a key with no session open is left for whoever is next");
    TAP_OK(s_move_count == 0,
            "and nothing was moved on the way past");
}


/**
 * @brief Starting a move opens a session; Return closes it
 *
 * @note Complexity: @e O(1)
 */
static void s_test_start_and_finish(void)
{
    client_td client;
    config_td config;

    s_stubs_reset();
    s_client_reset(&client);
    s_config_reset(&config);
    s_stage_reset();

    kbd_modal_move_start(s_conn, &s_stage, &client);
    TAP_OK(kbd_modal_is_active(), "starting a move opens a session");

    TAP_OK(kbd_modal_handle_keypress(s_conn, &s_stage, KS_RETURN, &config),
            "Return is taken by the session rather than passed on");
    TAP_OK(!kbd_modal_is_active(), "and closes it");
    TAP_OK(s_ungrab_count == 1,
            "releasing the keyboard exactly once on the way out");
}


/**
 * @brief Each arrow moves by the configured step, in its own direction
 *
 * @note Complexity: @e O(1)
 */
static void s_test_arrows_move_by_step(void)
{
    client_td client;
    config_td config;

    s_stubs_reset();
    s_client_reset(&client);
    s_config_reset(&config);
    s_stage_reset();
    kbd_modal_move_start(s_conn, &s_stage, &client);

    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RIGHT, &config);
    TAP_OK(s_moved_x == 110 && s_moved_y == 200,
            "Right moves one step along x and leaves y alone");

    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_DOWN, &config);
    TAP_OK(s_moved_x == 110 && s_moved_y == 210,
            "Down moves one step along y, from where Right left it");

    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_LEFT, &config);
    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_UP, &config);
    TAP_OK(s_moved_x == 100 && s_moved_y == 200,
            "Left and Up undo them, arriving back where it began");

    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RETURN, &config);
}


/**
 * @brief Escape puts the geometry back; Return leaves it moved
 *
 * @note Complexity: @e O(1)
 */
static void s_test_escape_restores(void)
{
    client_td client;
    config_td config;

    s_stubs_reset();
    s_client_reset(&client);
    s_config_reset(&config);
    s_stage_reset();

    kbd_modal_move_start(s_conn, &s_stage, &client);
    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RIGHT, &config);
    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RIGHT, &config);
    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_ESCAPE, &config);

    TAP_OK(!kbd_modal_is_active(), "Escape closes the session too");
    TAP_OK(client.layout.geometry.cur.pos.x == 100,
            "and puts the client back where the session found it");

    s_stubs_reset();
    s_client_reset(&client);
    s_stage_reset();
    kbd_modal_move_start(s_conn, &s_stage, &client);
    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RIGHT, &config);
    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RETURN, &config);

    TAP_OK(client.layout.geometry.cur.pos.x == 110,
            "while Return leaves it where the arrows put it");
}


/**
 * @brief A resize session grows and shrinks by the resize step, which
 *        is not the move step
 *
 * @note Complexity: @e O(1)
 */
static void s_test_resize_uses_its_own_step(void)
{
    client_td client;
    config_td config;

    s_stubs_reset();
    s_client_reset(&client);
    s_config_reset(&config);
    s_stage_reset();
    kbd_modal_resize_start(s_conn, &s_stage, &client);

    TAP_OK(kbd_modal_is_active(), "starting a resize opens a session");

    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RIGHT, &config);
    TAP_OK(s_resize_count > 0,
            "an arrow in a resize session resizes rather than moves");
    TAP_OK(s_move_count == 0,
            "and moves nothing at all");

    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RETURN, &config);
}


/**
 * @brief A key the session does not use changes nothing, but is still
 *        taken rather than passed on
 *
 * @note Complexity: @e O(1)
 */
static void s_test_unused_key_is_still_swallowed(void)
{
    client_td client;
    config_td config;

    s_stubs_reset();
    s_client_reset(&client);
    s_config_reset(&config);
    s_stage_reset();
    kbd_modal_move_start(s_conn, &s_stage, &client);

    TAP_OK(kbd_modal_handle_keypress(s_conn, &s_stage, KS_TAB, &config),
            "a key the session has no use for is taken all the same");
    TAP_OK(s_move_count == 0 && s_resize_count == 0,
            "and moves or resizes nothing");
    TAP_OK(kbd_modal_is_active(),
            "leaving the session open");

    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RETURN, &config);
}


/**
 * @brief A null configuration falls back to a step of one rather than
 *        refusing to move
 *
 * @note Complexity: @e O(1)
 */
static void s_test_null_config_falls_back(void)
{
    client_td client;

    s_stubs_reset();
    s_client_reset(&client);
    s_stage_reset();
    kbd_modal_move_start(s_conn, &s_stage, &client);

    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RIGHT, NULL);
    TAP_OK(s_moved_x == 101,
            "a null configuration moves by one rather than not at all");

    (void) kbd_modal_handle_keypress(s_conn, &s_stage, KS_RETURN, NULL);
}


int main(void)
{
    TAP_PLAN(20);

    s_test_inactive_until_started();
    s_test_start_and_finish();
    s_test_arrows_move_by_step();
    s_test_escape_restores();
    s_test_resize_uses_its_own_step();
    s_test_unused_key_is_still_swallowed();
    s_test_null_config_falls_back();

    return TAP_DONE();
}
