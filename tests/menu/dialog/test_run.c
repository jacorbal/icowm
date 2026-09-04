/**
 * @file tests/menu/dialog/test_run.c
 *
 * @brief Test battery for the built-in run-box (menu/dialog/run.c)
 *
 * 's_run' (declared file-static inside run.c) is reached only through
 * the public API this file exercises directly: run_init, run_is_open,
 * run_owns_window, run_handle_keypress and run_draw.  Every raw XCB
 * entry point run.c calls, every project-level XCB wrapper, the text
 * renderer, the menu drawing primitives, and the launch/info helpers it
 * reaches are all stubbed below as controllable, call-recording
 * stand-ins, so every branch runs without a real X server, a real
 * window manager, or a real font, following the same pattern as
 * tests/menu/test_search.c for the sibling fuzzy window-search widget.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <config.h>
#include <defs/run.h>
#include <desktop.h>
#include <harness/tap.h>
#include <menu/dialog/message.h>
#include <menu/dialog/run.h>
#include <surface.h>


/* Controllable stand-in state */

static xcb_connection_t *s_connection_stub = (xcb_connection_t *) 1;
static xcb_ewmh_connection_t *s_ewmh_stub = NULL;

static uint32_t s_next_id = 1000u;
static xcb_window_t s_created_window = XCB_WINDOW_NONE;
static xcb_window_t s_created_parent = XCB_WINDOW_NONE;
static int s_create_window_calls = 0;
static int s_map_window_calls = 0;
static int s_grab_keyboard_calls = 0;
static xcb_window_t s_grab_keyboard_window = XCB_WINDOW_NONE;
static int s_set_input_focus_calls = 0;
static xcb_window_t s_set_input_focus_window = XCB_WINDOW_NONE;
static int s_ungrab_keyboard_calls = 0;
static int s_window_destroy_calls = 0;
static xcb_window_t s_window_destroyed = XCB_WINDOW_NONE;

/** What 'xcb_get_input_focus_reply' hands back to 'run_init'; a test
 *  sets this before calling run_init to control 's_run.prev_focus' */
static xcb_window_t s_focus_reply_focus = XCB_WINDOW_NONE;
static bool s_focus_reply_is_null = false;

static uint32_t s_last_user_time_stub = 0u;

static int s_dialog_info_show_calls = 0;
static menu_msg_level_e s_dialog_info_show_level = MENU_MSG_LEVEL_NONE;
static char s_dialog_info_show_message[512];

/** Test-controlled outcome of 'desktop_action_process_launch': the
 *  command it was asked to run, how many times, and what it returns */
static int s_launch_calls = 0;
static char s_launch_last_command[WM_RUN_COMMAND_MAX_LENGTH];
static int s_launch_result = 0;

/** Desktop 'surface_desktop_get' answers with, or NULL if none was
 *  registered */
static desktop_td *s_desktop_stub = NULL;

static int s_menu_draw_label_calls = 0;
static int s_text_use_font_calls = 0;
static int s_text_set_color_calls = 0;

static xcb_screen_t s_screen_stub;
static surface_td s_surface;
static config_td s_config;


/* Raw XCB stand-ins */

uint32_t xcb_generate_id(xcb_connection_t *c)
{
    (void) c;
    return s_next_id++;
}

xcb_void_cookie_t xcb_create_window(xcb_connection_t *c, uint8_t depth,
        xcb_window_t wid, xcb_window_t parent, int16_t x, int16_t y,
        uint16_t width, uint16_t height, uint16_t border_width,
        uint16_t class, xcb_visualid_t visual, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) depth;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    (void) border_width;
    (void) class;
    (void) visual;
    (void) value_mask;
    (void) value_list;

    s_create_window_calls++;
    s_created_window = wid;
    s_created_parent = parent;
    return cookie;
}

xcb_void_cookie_t xcb_map_window(xcb_connection_t *c, xcb_window_t window)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) window;
    s_map_window_calls++;
    return cookie;
}

xcb_grab_keyboard_cookie_t xcb_grab_keyboard(xcb_connection_t *c,
        uint8_t owner_events, xcb_window_t grab_window,
        xcb_timestamp_t time, uint8_t pointer_mode, uint8_t keyboard_mode)
{
    xcb_grab_keyboard_cookie_t cookie = { 0u };

    (void) c;
    (void) owner_events;
    (void) time;
    (void) pointer_mode;
    (void) keyboard_mode;

    s_grab_keyboard_calls++;
    s_grab_keyboard_window = grab_window;
    return cookie;
}

xcb_void_cookie_t xcb_set_input_focus(xcb_connection_t *c,
        uint8_t revert_to, xcb_window_t focus, xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) revert_to;
    (void) time;

    s_set_input_focus_calls++;
    s_set_input_focus_window = focus;
    return cookie;
}

xcb_get_input_focus_cookie_t xcb_get_input_focus(xcb_connection_t *c)
{
    xcb_get_input_focus_cookie_t cookie = { 0u };

    (void) c;
    return cookie;
}

xcb_get_input_focus_reply_t *xcb_get_input_focus_reply(
        xcb_connection_t *c, xcb_get_input_focus_cookie_t cookie,
        xcb_generic_error_t **e)
{
    xcb_get_input_focus_reply_t *reply;

    (void) c;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }

    if (s_focus_reply_is_null) {
        return NULL;
    }

    reply = calloc(1, sizeof(*reply));
    reply->focus = s_focus_reply_focus;
    return reply;
}

xcb_void_cookie_t xcb_ungrab_keyboard(xcb_connection_t *c,
        xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) time;
    s_ungrab_keyboard_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_ewmh_set_wm_window_type(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t list_len, xcb_atom_t *list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) ewmh;
    (void) window;
    (void) list_len;
    (void) list;
    return cookie;
}

xcb_gcontext_t s_last_gc = 0u;

xcb_void_cookie_t xcb_create_gc(xcb_connection_t *c, xcb_gcontext_t cid,
        xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) drawable;
    (void) value_mask;
    (void) value_list;
    s_last_gc = cid;
    return cookie;
}

xcb_void_cookie_t xcb_free_gc(xcb_connection_t *c, xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) gc;
    return cookie;
}

xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *c,
        xcb_drawable_t drawable, xcb_gcontext_t gc, uint32_t rects_len,
        const xcb_rectangle_t *rects)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) c;
    (void) drawable;
    (void) gc;
    (void) rects_len;
    (void) rects;
    return cookie;
}


/* 'utils/xcb/connection.h' stand-ins */

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection_stub;
}

xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return s_ewmh_stub;
}


/* 'utils/xcb/window.h' stand-in */

void xcb_window_destroy(xcb_window_t window)
{
    s_window_destroy_calls++;
    s_window_destroyed = window;
}


/* 'client.h' stand-in */

uint32_t client_last_user_time(void)
{
    return s_last_user_time_stub;
}


/* 'menu/dialog/info.h' stand-in; info.c is separately tested
 * (tests/menu/dialog/test_info.c), only its forwarding matters here */

void dialog_info_show(xcb_connection_t *connection, surface_td *surface,
        const config_td *config, const char *message,
        menu_msg_level_e level)
{
    (void) connection;
    (void) surface;
    (void) config;

    s_dialog_info_show_calls++;
    s_dialog_info_show_level = level;
    (void) strncpy(s_dialog_info_show_message, message,
            sizeof(s_dialog_info_show_message) - 1u);
}


/* 'desktop.h' stand-ins */

desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
    return s_desktop_stub;
}

int desktop_action_process_launch(desktop_td *desktop,
        const char *executable_path)
{
    (void) desktop;

    s_launch_calls++;
    (void) strncpy(s_launch_last_command, executable_path,
            sizeof(s_launch_last_command) - 1u);
    return s_launch_result;
}


/* 'menu/draw.h' stand-in */

void menu_draw_label(xcb_connection_t *connection, xcb_window_t window,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) window;
    (void) pos;
    (void) text;
    s_menu_draw_label_calls++;
}


/* 'render/text.h' stand-ins; a fixed one-pixel-per-character metric is
 * enough for every geometry decision run.c makes (view-follow, marks,
 * cursor placement), the same convention test_search.c's own
 * 'menu_draw_measure' stand-in already uses */

int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;
    s_text_use_font_calls++;
    return 0;
}

void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
    s_text_set_color_calls++;
}

int16_t text_font_ascent(void)
{
    return 10;
}

int16_t text_font_descent(void)
{
    return 3;
}

uint16_t text_string_measure(const char *text)
{
    return (uint16_t) ((text != NULL) ? strlen(text) : 0u);
}


/**
 * @brief Reset every captured/recorded value and the config/surface
 *        fixtures to a clean, known-default state ahead of one scenario
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    /* s_run is file-static inside run.c and outlives every scenario;
     * a box left open by the previous scenario is closed here first,
     * via the same public Escape path a real close would use, so each
     * scenario's own run_init call below is genuinely the first
     * window this run of the box creates rather than an implicit
     * re-open. */
    if (run_is_open()) {
        run_handle_keypress(s_connection_stub, &s_surface, 0xff1bu, 0u,
                &s_config);
    }

    s_create_window_calls = 0;
    s_created_window = XCB_WINDOW_NONE;
    s_created_parent = XCB_WINDOW_NONE;
    s_map_window_calls = 0;
    s_grab_keyboard_calls = 0;
    s_grab_keyboard_window = XCB_WINDOW_NONE;
    s_set_input_focus_calls = 0;
    s_set_input_focus_window = XCB_WINDOW_NONE;
    s_ungrab_keyboard_calls = 0;
    s_window_destroy_calls = 0;
    s_window_destroyed = XCB_WINDOW_NONE;
    s_focus_reply_focus = XCB_WINDOW_NONE;
    s_focus_reply_is_null = false;
    s_last_user_time_stub = 0u;
    s_dialog_info_show_calls = 0;
    s_dialog_info_show_level = MENU_MSG_LEVEL_NONE;
    memset(s_dialog_info_show_message, 0, sizeof(s_dialog_info_show_message));
    s_launch_calls = 0;
    memset(s_launch_last_command, 0, sizeof(s_launch_last_command));
    s_launch_result = 0;
    s_menu_draw_label_calls = 0;
    s_text_use_font_calls = 0;
    s_text_set_color_calls = 0;

    memset(&s_screen_stub, 0, sizeof(s_screen_stub));
    s_screen_stub.root = (xcb_window_t) 1u;

    memset(&s_surface, 0, sizeof(s_surface));
    s_surface.screen = &s_screen_stub;
    s_surface.properties.dim.w = 1920u;
    s_surface.properties.dim.h = 1080u;
    s_surface.desktop_cur = 0u;

    memset(&s_config, 0, sizeof(s_config));
    (void) strncpy(s_config.theme.prompt.label.font, "sans-10",
            sizeof(s_config.theme.prompt.label.font) - 1u);
    (void) strncpy(s_config.theme.prompt.input.font, "mono-10",
            sizeof(s_config.theme.prompt.input.font) - 1u);
    s_config.theme.prompt.label.color.background = 0x111111u;
    s_config.theme.prompt.label.color.foreground = 0xeeeeeeu;
    s_config.theme.prompt.input.color.background = 0x222222u;
    s_config.theme.prompt.input.color.foreground = 0xffffffu;
    s_config.theme.prompt.border.color = 0x333333u;
    s_config.theme.prompt.border.width = 1u;

    s_desktop_stub = NULL;
}


/**
 * @brief NULL connection, surface or config all leave the box unopened
 *
 * @note Complexity: @e O(1)
 */
static void s_test_init_null_guards(void)
{
    s_reset();
    run_init(NULL, &s_surface, &s_config);
    TAP_OK(!run_is_open(), "run_init: NULL connection is a no-op");

    s_reset();
    run_init(s_connection_stub, NULL, &s_config);
    TAP_OK(!run_is_open(), "run_init: NULL surface is a no-op");

    s_reset();
    run_init(s_connection_stub, &s_surface, NULL);
    TAP_OK(!run_is_open(), "run_init: NULL config is a no-op");
}


/**
 * @brief A valid call creates exactly one window as a child of the
 *        surface's own screen root, maps it, grabs the keyboard on the
 *        screen root, focuses the box, and paints immediately
 *
 * @note Complexity: @e O(1)
 */
static void s_test_init_opens_and_draws(void)
{
    s_reset();
    run_init(s_connection_stub, &s_surface, &s_config);

    TAP_OK(run_is_open(), "run_init: the box is open afterward");
    TAP_EQ_INT(s_create_window_calls, 1,
            "run_init: creates exactly one window");
    TAP_EQ_INT((int) s_created_parent, (int) s_screen_stub.root,
            "run_init: the window is a child of the surface's screen"
            " root");
    TAP_EQ_INT(s_map_window_calls, 1, "run_init: maps the window once");
    TAP_EQ_INT(s_grab_keyboard_calls, 1,
            "run_init: grabs the keyboard exactly once");
    TAP_EQ_INT((int) s_grab_keyboard_window, (int) s_screen_stub.root,
            "run_init: the keyboard grab targets the screen root, not"
            " the box window");
    TAP_EQ_INT(s_set_input_focus_calls, 1,
            "run_init: focuses the box exactly once");
    TAP_EQ_INT((int) s_set_input_focus_window, (int) s_created_window,
            "run_init: focus goes to the newly created box window");
    TAP_OK(s_menu_draw_label_calls > 0,
            "run_init: paints immediately rather than waiting for a"
            " keystroke");
    TAP_OK(run_owns_window(s_created_window),
            "run_init: run_owns_window recognizes the box's own"
            " window");
    TAP_OK(!run_owns_window((xcb_window_t) 0xdeadu),
            "run_init: run_owns_window rejects an unrelated window");
}


/**
 * @brief Re-opening an already-open box destroys the previous window
 *        first and allocates a fresh identifier
 *
 * @note Complexity: @e O(1)
 */
static void s_test_init_reopen_destroys_previous(void)
{
    xcb_window_t first_window;

    s_reset();
    run_init(s_connection_stub, &s_surface, &s_config);
    first_window = s_created_window;

    run_init(s_connection_stub, &s_surface, &s_config);
    TAP_EQ_INT(s_window_destroy_calls, 1,
            "run_init: re-opening destroys the previously open window");
    TAP_EQ_INT((int) s_window_destroyed, (int) first_window,
            "run_init: the window destroyed is the previous one");
    TAP_OK(s_created_window != first_window,
            "run_init: re-opening allocates a fresh window identifier");
}


/**
 * @brief A real previous focus is restored via 'XCB_INPUT_FOCUS_PARENT'
 *        on close; 'PointerRoot' and 'None' are never restored, and
 *        with no previous focus recorded, close issues no second focus
 *        call at all
 *
 * @note Complexity: @e O(1)
 */
static void s_test_escape_restores_focus(void)
{
    s_reset();
    s_focus_reply_focus = (xcb_window_t) 777u;
    run_init(s_connection_stub, &s_surface, &s_config);
    s_set_input_focus_calls = 0;

    run_handle_keypress(s_connection_stub, &s_surface, 0xff1bu, 0u,
            &s_config);
    TAP_OK(!run_is_open(), "run_handle_keypress: Escape closes the box");
    TAP_EQ_INT(s_ungrab_keyboard_calls, 1,
            "run_handle_keypress: Escape ungrabs the keyboard");
    TAP_EQ_INT(s_set_input_focus_calls, 1,
            "run_handle_keypress: Escape restores a real previous"
            " focus");
    TAP_EQ_INT((int) s_set_input_focus_window, 777,
            "run_handle_keypress: focus is restored to the real"
            " previous window");

    s_reset();
    s_focus_reply_is_null = true;
    run_init(s_connection_stub, &s_surface, &s_config);
    s_set_input_focus_calls = 0;
    run_handle_keypress(s_connection_stub, &s_surface, 0xff1bu, 0u,
            &s_config);
    TAP_EQ_INT(s_set_input_focus_calls, 0,
            "run_handle_keypress: with no previous focus recorded,"
            " closing issues no focus call at all");
}


/**
 * @brief Escape, Return and every editing keysym are all no-ops while
 *        the box is not open
 *
 * @note Complexity: @e O(1)
 */
static void s_test_keypress_noop_when_closed(void)
{
    s_reset();
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_INT(s_launch_calls, 0,
            "run_handle_keypress: Return is a no-op while the box is"
            " not open");
}


/**
 * @brief Typing a command and pressing Return launches it via
 *        'desktop_action_process_launch' and closes the box either way
 *
 * @note Complexity: @e O(1)
 */
static void s_test_return_launches_and_closes(void)
{
    const char *cmd = "xterm";
    size_t i;

    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    s_launch_result = 0;
    run_init(s_connection_stub, &s_surface, &s_config);
    for (i = 0u; cmd[i] != '\0'; i++) {
        run_handle_keypress(s_connection_stub, &s_surface,
                (xcb_keysym_t) (unsigned char) cmd[i], 0u, &s_config);
    }
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);

    TAP_EQ_INT(s_launch_calls, 1,
            "run_handle_keypress: Return launches the typed command"
            " exactly once");
    TAP_EQ_STR(s_launch_last_command, cmd,
            "run_handle_keypress: the launched command matches exactly"
            " what was typed");
    TAP_OK(!run_is_open(),
            "run_handle_keypress: the box closes once the command"
            " launches successfully");
    TAP_EQ_INT(s_dialog_info_show_calls, 0,
            "run_handle_keypress: no \"not found\" dialog on a"
            " successful launch");
}


/**
 * @brief KP_Enter behaves exactly like Return
 *
 * @note Complexity: @e O(1)
 */
static void s_test_kp_enter_launches(void)
{
    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    s_launch_result = 0;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff8du, 0u,
            &s_config);
    TAP_EQ_INT(s_launch_calls, 1,
            "run_handle_keypress: KP_Enter launches the typed command"
            " exactly like Return");
}


/**
 * @brief An empty command does nothing on Return: no launch attempt,
 *        and the box stays open
 *
 * @note Complexity: @e O(1)
 */
static void s_test_return_empty_command(void)
{
    s_reset();
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_INT(s_launch_calls, 0,
            "run_handle_keypress: Return with an empty command attempts"
            " no launch");
    TAP_OK(run_is_open(),
            "run_handle_keypress: the box stays open after an empty"
            " Return");
}


/**
 * @brief A failed launch shows the \"not found\" informational dialog,
 *        never a blocking warning or error, with the box already
 *        closed
 *
 * @note Complexity: @e O(1)
 */
static void s_test_return_launch_failure_shows_info(void)
{
    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    s_launch_result = -1;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'z', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);

    TAP_EQ_INT(s_dialog_info_show_calls, 1,
            "run_handle_keypress: a failed launch shows exactly one"
            " informational dialog");
    TAP_EQ_INT((int) s_dialog_info_show_level,
            (int) MENU_MSG_LEVEL_INFO,
            "run_handle_keypress: the failure dialog is informational,"
            " never a warning or error");
    TAP_OK(strstr(s_dialog_info_show_message, "z") != NULL,
            "run_handle_keypress: the failure message names the"
            " command that could not be found");
}


/**
 * @brief With no desktop resolvable for the surface, Return attempts
 *        no launch at all and closes the box quietly
 *
 * @note Complexity: @e O(1)
 */
static void s_test_return_no_desktop(void)
{
    s_reset();
    s_desktop_stub = NULL;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_INT(s_launch_calls, 0,
            "run_handle_keypress: with no resolvable desktop, no launch"
            " is attempted");
    TAP_EQ_INT(s_dialog_info_show_calls, 0,
            "run_handle_keypress: with no resolvable desktop, no"
            " failure dialog is shown either");
}


/**
 * @brief Backspace removes the character behind the cursor and moves
 *        it back one; Delete removes the character under it and leaves
 *        the cursor where it was; both are no-ops at their respective
 *        edge
 *
 * @note Complexity: @e O(1)
 */
static void s_test_backspace_and_delete(void)
{
    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'b', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'c', 0u, &s_config);
    /* Cursor after the 'c', command is "abc" */
    run_handle_keypress(s_connection_stub, &s_surface, 0xff08u, 0u,
            &s_config);      /* Backspace removes 'c' */
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);      /* Return launches whatever remains */
    TAP_EQ_STR(s_launch_last_command, "ab",
            "run_handle_keypress: Backspace removes the character"
            " immediately behind the cursor");

    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff08u, 0u,
            &s_config);      /* Backspace on an empty box: a no-op */
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'x', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "x",
            "run_handle_keypress: Backspace at the left edge is a"
            " no-op");

    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'b', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff50u, 0u,
            &s_config);      /* Home: cursor back to 0 */
    run_handle_keypress(s_connection_stub, &s_surface, 0xffffu, 0u,
            &s_config);      /* Delete removes 'a', cursor still 0 */
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "b",
            "run_handle_keypress: Delete removes the character under"
            " the cursor without moving it");

    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xffffu, 0u,
            &s_config);      /* Delete at the right edge: a no-op */
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "a",
            "run_handle_keypress: Delete at the right edge is a"
            " no-op");
}


/**
 * @brief Left and Right move the cursor a character at a time, and
 *        stop at either edge; an insertion made after moving the
 *        cursor lands exactly where the cursor now is, not at the end
 *
 * @note Complexity: @e O(1)
 */
static void s_test_left_right_and_mid_insert(void)
{
    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'c', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff51u, 0u,
            &s_config);      /* Left: cursor between 'a' and 'c' */
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'b', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "abc",
            "run_handle_keypress: Left moves the cursor so a following"
            " insertion lands in the middle");

    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff51u, 0u,
            &s_config);      /* Left at the left edge: a no-op */
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'z', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "z",
            "run_handle_keypress: Left at the left edge is a no-op");

    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff53u, 0u,
            &s_config);      /* Right at the right edge: a no-op */
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'b', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "ab",
            "run_handle_keypress: Right at the right edge is a no-op,"
            " so the next insertion still lands at the end");
}


/**
 * @brief Home and Ctrl+A both take the cursor to the start; End and
 *        Ctrl+E both take it to the end
 *
 * @note Complexity: @e O(1)
 */
static void s_test_home_end_and_ctrl_variants(void)
{
    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'b', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'c', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff50u, 0u,
            &s_config);      /* Home */
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "abc",
            "run_handle_keypress: Home moves the cursor to the start");

    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'b', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'c', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 0x61u, XCB_MOD_MASK_CONTROL,
            &s_config);      /* Ctrl+A: same as Home */
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "abc",
            "run_handle_keypress: Ctrl+A moves the cursor to the start,"
            " same as Home");

    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'b', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff50u, 0u,
            &s_config);      /* Home */
    run_handle_keypress(s_connection_stub, &s_surface, 0xff57u, 0u,
            &s_config);      /* End */
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'c', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "abc",
            "run_handle_keypress: End moves the cursor back to the end"
            " of the command");

    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'b', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff50u, 0u,
            &s_config);      /* Home */
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 0x65u, XCB_MOD_MASK_CONTROL,
            &s_config);      /* Ctrl+E: same as End */
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'c', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "abc",
            "run_handle_keypress: Ctrl+E moves the cursor to the end,"
            " same as End");
}


/**
 * @brief A non-printable keysym, and a printable one held with Ctrl,
 *        are both ignored rather than inserted
 *
 * @note Complexity: @e O(1)
 */
static void s_test_unhandled_keys_ignored(void)
{
    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    run_init(s_connection_stub, &s_surface, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'a', 0u, &s_config);
    run_handle_keypress(s_connection_stub, &s_surface, 0xffbeu, 0u,
            &s_config);      /* F1: outside the printable ASCII range */
    run_handle_keypress(s_connection_stub, &s_surface,
            (xcb_keysym_t) 'z', XCB_MOD_MASK_CONTROL,
            &s_config);      /* Ctrl+z: not one of the two Ctrl cases */
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_STR(s_launch_last_command, "a",
            "run_handle_keypress: a non-printable keysym and an"
            " unrelated Ctrl combination are both ignored");
}


/**
 * @brief The command buffer stops accepting insertions once full,
 *        rather than overflowing
 *
 * @note Complexity: @e O(n), where @e n is the buffer's capacity
 */
static void s_test_buffer_full_guard(void)
{
    unsigned int i;

    s_reset();
    s_desktop_stub = (desktop_td *) 0x1234;
    s_launch_result = 0;
    run_init(s_connection_stub, &s_surface, &s_config);
    for (i = 0u; i < (unsigned int) (WM_RUN_COMMAND_MAX_LENGTH + 8); i++) {
        run_handle_keypress(s_connection_stub, &s_surface,
                (xcb_keysym_t) 'x', 0u, &s_config);
    }
    run_handle_keypress(s_connection_stub, &s_surface, 0xff0du, 0u,
            &s_config);
    TAP_EQ_INT((int) strlen(s_launch_last_command),
            WM_RUN_COMMAND_MAX_LENGTH - 1,
            "run_handle_keypress: insertion stops exactly at the"
            " buffer's last usable slot rather than overflowing");
}


/**
 * @brief run_draw is a safe no-op while the box is not open, or with a
 *        NULL config; while open, it exercises the label paint, the
 *        input paint, and the edge marks once the command overflows
 *        the visible width
 *
 * @note Complexity: @e O(1)
 */
static void s_test_draw_paints_and_guards(void)
{
    int calls_before;
    unsigned int i;

    s_reset();
    run_draw(s_connection_stub, &s_config);
    TAP_EQ_INT(s_menu_draw_label_calls, 0,
            "run_draw: a no-op while the box is not open");

    s_reset();
    run_init(s_connection_stub, &s_surface, &s_config);
    run_draw(s_connection_stub, NULL);
    calls_before = s_menu_draw_label_calls;
    TAP_OK(calls_before > 0,
            "run_draw: init already painted, establishing a baseline"
            " before the NULL-config call below");
    run_draw(s_connection_stub, NULL);
    TAP_EQ_INT(s_menu_draw_label_calls, calls_before,
            "run_draw: a NULL config paints nothing further");

    s_reset();
    run_init(s_connection_stub, &s_surface, &s_config);
    calls_before = s_menu_draw_label_calls;
    for (i = 0u; i < 60u; i++) {
        run_handle_keypress(s_connection_stub, &s_surface,
                (xcb_keysym_t) 'x', 0u, &s_config);
    }
    TAP_OK(s_menu_draw_label_calls > calls_before,
            "run_draw: a long command that overflows the visible width"
            " still repaints without crashing, exercising the edge"
            " marks and the view-follow-cursor logic");
}


int main(void)
{
    TAP_PLAN(52);

    s_test_init_null_guards();
    s_test_init_opens_and_draws();
    s_test_init_reopen_destroys_previous();
    s_test_escape_restores_focus();
    s_test_keypress_noop_when_closed();
    s_test_return_launches_and_closes();
    s_test_kp_enter_launches();
    s_test_return_empty_command();
    s_test_return_launch_failure_shows_info();
    s_test_return_no_desktop();
    s_test_backspace_and_delete();
    s_test_left_right_and_mid_insert();
    s_test_home_end_and_ctrl_variants();
    s_test_unhandled_keys_ignored();
    s_test_buffer_full_guard();
    s_test_draw_paints_and_guards();

    return TAP_DONE();
}
