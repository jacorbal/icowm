/**
 * @file tests/loop/event/test_input.c
 *
 * @brief Test battery for the main loop's key/button event handlers
 *        (loop/event/input.c)
 *
 * All four public entry points share the same null-guard shape and
 * then forward, mostly unconditionally, to a keyboard_handle_* or
 * mouse_handle_* target, none of which belongs to loop/event/input.c
 * itself, so every one is a link-only, call-recording stand-in below.
 * The one piece of genuine, file-local logic worth exercising in its
 * own right is s_loop_event_note_real_input's synthetic-bit check,
 * reached only from the key-press and button-press paths, which this
 * file drives indirectly by varying response_type's top bit on real
 * xcb_key_press_event_t/xcb_button_press_event_t values built on the
 * stack, and reads back through client_note_user_time/lookup_find_
 * client/client_update_user_time's own recording stand-ins.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <loop/event.h>


/* Recording for the user-time-related stand-ins below */
static int s_client_note_user_time_calls;
static uint32_t s_client_note_user_time_last_time;
static int s_lookup_find_client_calls;
static xcb_window_t s_lookup_find_client_last_window;
static client_td *s_lookup_find_client_return_value;
static int s_client_update_user_time_calls;
static client_td *s_client_update_user_time_last_client;
static uint32_t s_client_update_user_time_last_time;

/* Recording for the keyboard/mouse forwarding stand-ins below */
static int s_keyboard_handle_press_calls;
static int s_keyboard_handle_release_calls;
static int s_mouse_handle_press_calls;
static int s_mouse_handle_release_calls;


static void s_reset(void)
{
    s_client_note_user_time_calls = 0;
    s_client_note_user_time_last_time = 0;
    s_lookup_find_client_calls = 0;
    s_lookup_find_client_last_window = 0;
    s_lookup_find_client_return_value = NULL;
    s_client_update_user_time_calls = 0;
    s_client_update_user_time_last_client = NULL;
    s_client_update_user_time_last_time = 0;
    s_keyboard_handle_press_calls = 0;
    s_keyboard_handle_release_calls = 0;
    s_mouse_handle_press_calls = 0;
    s_mouse_handle_release_calls = 0;
}


/** Link-only stand-in for client_note_user_time (client.c) */
void client_note_user_time(uint32_t time)
{
    s_client_note_user_time_calls++;
    s_client_note_user_time_last_time = time;
}


/** Link-only stand-in for lookup_find_client (lookup.c) */
client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **out_surface, desktop_td **out_desktop)
{
    (void) surfaces;
    (void) out_surface;
    (void) out_desktop;
    s_lookup_find_client_calls++;
    s_lookup_find_client_last_window = window;
    return s_lookup_find_client_return_value;
}


/** Link-only stand-in for client_update_user_time (client.c) */
void client_update_user_time(client_td *client, uint32_t time)
{
    s_client_update_user_time_calls++;
    s_client_update_user_time_last_client = client;
    s_client_update_user_time_last_time = time;
}


/** Link-only stand-in for keyboard_handle_press (input/kbd/event.c) */
void keyboard_handle_press(wm_td *wm, xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event, list_td *surfaces,
        const config_td *cfg)
{
    (void) wm;
    (void) keysyms;
    (void) event;
    (void) surfaces;
    (void) cfg;
    s_keyboard_handle_press_calls++;
}


/** Link-only stand-in for keyboard_handle_release
 *  (input/kbd/event.c) */
void keyboard_handle_release(xcb_key_symbols_t *keysyms,
        xcb_key_release_event_t *event, list_td *surfaces,
        const config_td *cfg)
{
    (void) keysyms;
    (void) event;
    (void) surfaces;
    (void) cfg;
    s_keyboard_handle_release_calls++;
}


/** Link-only stand-in for mouse_handle_press (input/mouse/event.c) */
void mouse_handle_press(wm_td *wm, xcb_connection_t *connection,
        list_td *surfaces, xcb_button_press_event_t *event,
        const config_td *config)
{
    (void) wm;
    (void) connection;
    (void) surfaces;
    (void) event;
    (void) config;
    s_mouse_handle_press_calls++;
}


/** Link-only stand-in for mouse_handle_release
 *  (input/mouse/event.c) */
void mouse_handle_release(xcb_connection_t *connection,
        list_td *surfaces, const xcb_button_release_event_t *event,
        const config_td *config)
{
    (void) connection;
    (void) surfaces;
    (void) event;
    (void) config;
    s_mouse_handle_release_calls++;
}


/** Link-only stand-in for xcb_connection_get
 *  (utils/xcb/connection.c) */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/**
 * @brief Build a loop context with every field left null/zero
 */
static loop_ctx_td s_make_ctx(void)
{
    loop_ctx_td ctx;

    memset(&ctx, 0, sizeof(ctx));
    return ctx;
}


/* Each of the four public functions is a safe no-op given a null ctx,
 * a null event pointer, or a pointer to a null event, never reaching
 * any stand-in above */
static void s_test_null_guards(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_generic_event_t *null_event = NULL;
    xcb_key_press_event_t kp;

    memset(&kp, 0, sizeof(kp));

    s_reset();
    loop_event_key_press(NULL, (xcb_generic_event_t **) &kp);
    loop_event_key_press(&ctx, NULL);
    loop_event_key_press(&ctx, &null_event);
    TAP_EQ_INT(s_keyboard_handle_press_calls, 0,
            "loop_event_key_press never forwards on a null ctx, a"
            " null event pointer, or a pointer to a null event");

    s_reset();
    loop_event_key_release(NULL, (xcb_generic_event_t **) &kp);
    loop_event_key_release(&ctx, NULL);
    loop_event_key_release(&ctx, &null_event);
    TAP_EQ_INT(s_keyboard_handle_release_calls, 0,
            "loop_event_key_release never forwards on a null ctx, a"
            " null event pointer, or a pointer to a null event");

    s_reset();
    loop_event_button_press(NULL, (xcb_generic_event_t **) &kp);
    loop_event_button_press(&ctx, NULL);
    loop_event_button_press(&ctx, &null_event);
    TAP_EQ_INT(s_mouse_handle_press_calls, 0,
            "loop_event_button_press never forwards on a null ctx, a"
            " null event pointer, or a pointer to a null event");

    s_reset();
    loop_event_button_release(NULL, (xcb_generic_event_t **) &kp);
    loop_event_button_release(&ctx, NULL);
    loop_event_button_release(&ctx, &null_event);
    TAP_EQ_INT(s_mouse_handle_release_calls, 0,
            "loop_event_button_release never forwards on a null ctx,"
            " a null event pointer, or a pointer to a null event");
}


/* A genuine (non-synthetic) key-press event notes and updates user
 * time before forwarding to keyboard_handle_press */
static void s_test_key_press_genuine_notes_user_time(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_key_press_event_t kp;
    xcb_generic_event_t *event_ptr;
    client_td *const fake_client = (client_td *) (void *) 0x1234;

    memset(&kp, 0, sizeof(kp));
    kp.response_type = XCB_KEY_PRESS;
    kp.event = 42;
    kp.time = 999;
    event_ptr = (xcb_generic_event_t *) &kp;

    s_reset();
    s_lookup_find_client_return_value = fake_client;

    loop_event_key_press(&ctx, &event_ptr);

    TAP_EQ_INT(s_client_note_user_time_calls, 1,
            "a genuine key-press event notes the user time exactly"
            " once");
    TAP_EQ_INT((int) s_client_note_user_time_last_time, 999,
            "the noted user time is exactly the event's own"
            " timestamp");
    TAP_EQ_INT(s_lookup_find_client_calls, 1,
            "a genuine key-press event looks up the client behind the"
            " event window exactly once");
    TAP_EQ_INT((int) s_lookup_find_client_last_window, 42,
            "the lookup targets exactly the event's own 'event'"
            " window field");
    TAP_EQ_INT(s_client_update_user_time_calls, 1,
            "a genuine key-press event updates the found client's"
            " user time exactly once");
    TAP_OK(s_client_update_user_time_last_client == fake_client,
            "the update targets exactly the client the lookup"
            " returned");
    TAP_EQ_INT(s_keyboard_handle_press_calls, 1,
            "a key-press event still forwards to"
            " keyboard_handle_press exactly once");
}


/* A synthetic key-press event (top bit of response_type set) skips
 * noting or updating user time entirely, but still forwards to
 * keyboard_handle_press */
static void s_test_key_press_synthetic_skips_user_time(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_key_press_event_t kp;
    xcb_generic_event_t *event_ptr;

    memset(&kp, 0, sizeof(kp));
    kp.response_type = (uint8_t) (XCB_KEY_PRESS | 0x80u);
    kp.event = 42;
    kp.time = 999;
    event_ptr = (xcb_generic_event_t *) &kp;

    s_reset();

    loop_event_key_press(&ctx, &event_ptr);

    TAP_EQ_INT(s_client_note_user_time_calls, 0,
            "a synthetic key-press event never notes the user time");
    TAP_EQ_INT(s_lookup_find_client_calls, 0,
            "a synthetic key-press event never looks up the client"
            " behind the window");
    TAP_EQ_INT(s_client_update_user_time_calls, 0,
            "a synthetic key-press event never updates any client's"
            " user time");
    TAP_EQ_INT(s_keyboard_handle_press_calls, 1,
            "a synthetic key-press event still forwards to"
            " keyboard_handle_press exactly once");
}


/* A key-release event never touches user time at all, genuine or
 * synthetic, and simply forwards */
static void s_test_key_release_forwards_only(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_key_release_event_t kr;
    xcb_generic_event_t *event_ptr;

    memset(&kr, 0, sizeof(kr));
    kr.response_type = XCB_KEY_RELEASE;
    event_ptr = (xcb_generic_event_t *) &kr;

    s_reset();

    loop_event_key_release(&ctx, &event_ptr);

    TAP_EQ_INT(s_client_note_user_time_calls, 0,
            "a key-release event never notes the user time");
    TAP_EQ_INT(s_keyboard_handle_release_calls, 1,
            "a key-release event forwards to keyboard_handle_release"
            " exactly once");
}


/* A genuine button-press event notes and updates user time exactly
 * like a genuine key press does, then forwards to
 * mouse_handle_press */
static void s_test_button_press_genuine_notes_user_time(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_button_press_event_t bp;
    xcb_generic_event_t *event_ptr;
    client_td *const fake_client = (client_td *) (void *) 0x5678;

    memset(&bp, 0, sizeof(bp));
    bp.response_type = XCB_BUTTON_PRESS;
    bp.event = 7;
    bp.time = 111;
    event_ptr = (xcb_generic_event_t *) &bp;

    s_reset();
    s_lookup_find_client_return_value = fake_client;

    loop_event_button_press(&ctx, &event_ptr);

    TAP_EQ_INT(s_client_note_user_time_calls, 1,
            "a genuine button-press event notes the user time"
            " exactly once");
    TAP_EQ_INT((int) s_lookup_find_client_last_window, 7,
            "the lookup targets exactly the button-press event's own"
            " 'event' window field");
    TAP_OK(s_client_update_user_time_last_client == fake_client,
            "the update targets exactly the client the lookup"
            " returned for the button-press event");
    TAP_EQ_INT(s_mouse_handle_press_calls, 1,
            "a button-press event still forwards to"
            " mouse_handle_press exactly once");
}


/* A synthetic button-press event skips user-time bookkeeping, but
 * still forwards to mouse_handle_press */
static void s_test_button_press_synthetic_skips_user_time(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_button_press_event_t bp;
    xcb_generic_event_t *event_ptr;

    memset(&bp, 0, sizeof(bp));
    bp.response_type = (uint8_t) (XCB_BUTTON_PRESS | 0x80u);
    event_ptr = (xcb_generic_event_t *) &bp;

    s_reset();

    loop_event_button_press(&ctx, &event_ptr);

    TAP_EQ_INT(s_client_note_user_time_calls, 0,
            "a synthetic button-press event never notes the user"
            " time");
    TAP_EQ_INT(s_mouse_handle_press_calls, 1,
            "a synthetic button-press event still forwards to"
            " mouse_handle_press exactly once");
}


/* A button-release event never touches user time and simply
 * forwards */
static void s_test_button_release_forwards_only(void)
{
    loop_ctx_td ctx = s_make_ctx();
    xcb_button_release_event_t br;
    xcb_generic_event_t *event_ptr;

    memset(&br, 0, sizeof(br));
    br.response_type = XCB_BUTTON_RELEASE;
    event_ptr = (xcb_generic_event_t *) &br;

    s_reset();

    loop_event_button_release(&ctx, &event_ptr);

    TAP_EQ_INT(s_client_note_user_time_calls, 0,
            "a button-release event never notes the user time");
    TAP_EQ_INT(s_mouse_handle_release_calls, 1,
            "a button-release event forwards to"
            " mouse_handle_release exactly once");
}


int main(void)
{
    TAP_PLAN(25);

    s_test_null_guards();
    s_test_key_press_genuine_notes_user_time();
    s_test_key_press_synthetic_skips_user_time();
    s_test_key_release_forwards_only();
    s_test_button_press_genuine_notes_user_time();
    s_test_button_press_synthetic_skips_user_time();
    s_test_button_release_forwards_only();

    return TAP_DONE();
}
