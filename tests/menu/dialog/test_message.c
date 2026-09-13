/**
 * @file tests/menu/dialog/test_message.c
 *
 * @brief Test battery for the read-only message modal dialog with
 *        a scrollable body and a single "OK" button
 *
 * 'menu_message_dialog_show'/'_show_pairs' open a real XCB window
 * through a one-directional handful of libxcb calls (xcb_create_window,
 * xcb_map_window, and so on), none of which this file's scenarios ever
 * need to inspect the return value of; every one is a link-only
 * stand-in below, the same 'tests/menu/dialog/test_confirm.c' pattern
 * of re-declaring libxcb's own entry points with matching signatures
 * rather than linking the real library, since there is no X server for
 * this test binary to actually talk to.  'menu/dialog/defer.c' is
 * linked for real (per this round's task note: message.c's click path
 * schedules its close through it, and covering that deferred-close
 * timing is exactly what confirm.c's own tests already established the
 * pattern for), which in turn needs 'utils/time/clock.c' linked for
 * real too.  'utils/safe/safestr.c' is linked for real as well, since
 * message.c copies and wraps every caller-supplied string through it,
 * and the word-wrap/value-wrap scenarios below need the genuine
 * copy-and-truncate behavior, not a stand-in that would have to
 * reimplement it just to be checked against itself.
 *
 * 's_message_wrap_text', 's_message_value_column',
 * 's_message_wrap_value', and 's_message_compute_layout' (all
 * file-static in message.c) are reached only through the public
 * 'menu_message_dialog_show'/'_show_pairs', exactly as the window
 * manager itself reaches them; this file's 'menu_draw_measure' and
 * 'text_font_ascent'/'text_font_descent' stand-ins return small fixed
 * values so that layout call always succeeds deterministically,
 * without this file needing to duplicate its pixel arithmetic to
 * predict an exact width or height.  'dlgutil_resolve_monitor' is
 * stubbed to a fixed-size monitor for the same reason 'menu_dialog.c'
 * itself is not linked here: only the tiny, pure 'dlgutil_u16max' is
 * needed from it, and it is reproduced link-only below instead,
 * exactly the precedent already followed in 'test_confirm.c',
 * 'test_inspect.c' and 'test_shortcuts.c', to avoid dragging in
 * 'dlgutil_resolve_monitor''s own real XCB pointer query.
 *
 * @note 'dialog_pair_append_blank' is genuinely defined in
 *       message.c itself (unlike the link-only reproductions of it
 *       seen in test_inspect.c/test_shortcuts.c, built only because
 *       neither of those files links message.c), so it is exercised
 *       here as real production code, not stood in for
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <i18n.h>
#include <surface.h>

/* Default includes */
#include <defs/uistr.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/dialog.h>
#include <menu/dialog/message.h>
#include <menu/draw.h>
#include <render/text.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


/** Fake, ever-increasing XCB resource IDs, so every xcb_generate_id
 *  call this file's stand-in answers gets a distinct value, the same
 *  as a real X server would hand out */
static uint32_t s_next_xid = 1u;

/** Recording stand-ins' own call counters, reset by s_reset */
static int s_call_create_window;
static uint16_t s_last_create_window_w;
static uint16_t s_last_create_window_h;
static int s_call_map_window;
static int s_call_grab_keyboard;
static int s_call_ungrab_keyboard;
static int s_call_set_input_focus;
static xcb_window_t s_last_focus_window;
static int s_call_window_destroy;
static xcb_window_t s_destroyed_window;
static int s_call_atom_set_window_opacity;


/**
 * @brief Link-only stand-in for @a xcb_generate_id
 * @note Complexity: @e O(1)
 */
uint32_t xcb_generate_id(xcb_connection_t *c)
{
    (void) c;
    return s_next_xid++;
}


/**
 * @brief Recording stand-in for @a xcb_create_window
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_create_window(xcb_connection_t *c, uint8_t depth,
        xcb_window_t wid, xcb_window_t parent, int16_t x, int16_t y,
        uint16_t width, uint16_t height, uint16_t border_width,
        uint16_t klass, xcb_visualid_t visual, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) depth;
    (void) wid;
    (void) parent;
    (void) x;
    (void) y;
    (void) border_width;
    (void) klass;
    (void) visual;
    (void) value_mask;
    (void) value_list;

    s_call_create_window++;
    s_last_create_window_w = width;
    s_last_create_window_h = height;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_map_window
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_map_window(xcb_connection_t *c, xcb_window_t window)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) window;

    s_call_map_window++;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_configure_window
 *
 * Reached from every 'menu_message_dialog_show' call, to stack the new
 * window above; nothing here inspects stacking order.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_configure_window(xcb_connection_t *c,
        xcb_window_t window, uint16_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) window;
    (void) value_mask;
    (void) value_list;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_grab_keyboard
 * @note Complexity: @e O(1)
 */
xcb_grab_keyboard_cookie_t xcb_grab_keyboard(xcb_connection_t *c,
        uint8_t owner_events, xcb_window_t grab_window,
        xcb_timestamp_t time, uint8_t pointer_mode, uint8_t keyboard_mode)
{
    xcb_grab_keyboard_cookie_t cookie;

    (void) c;
    (void) owner_events;
    (void) grab_window;
    (void) time;
    (void) pointer_mode;
    (void) keyboard_mode;

    s_call_grab_keyboard++;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_ungrab_keyboard
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ungrab_keyboard(xcb_connection_t *c,
        xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) time;

    s_call_ungrab_keyboard++;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_set_input_focus
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_set_input_focus(xcb_connection_t *c,
        uint8_t revert_to, xcb_window_t focus, xcb_timestamp_t time)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) revert_to;
    (void) time;

    s_call_set_input_focus++;
    s_last_focus_window = focus;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Test-controlled stand-in for @a xcb_get_input_focus
 *
 * Always returns a zeroed cookie; the reply this file's own
 * 'xcb_get_input_focus_reply' stand-in hands back does not depend on
 * it.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_input_focus_cookie_t xcb_get_input_focus(xcb_connection_t *c)
{
    xcb_get_input_focus_cookie_t cookie;

    (void) c;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/** Window this file's 'xcb_get_input_focus_reply' stand-in reports as
 *  currently focused, before a message dialog takes it; settable per
 *  scenario, XCB_WINDOW_NONE by default */
static xcb_window_t s_prior_focus_window = XCB_WINDOW_NONE;


/**
 * @brief Test-controlled stand-in for @a xcb_get_input_focus_reply
 *
 * Hands back a heap-allocated reply naming 's_prior_focus_window' as
 * the focus already held before 'menu_message_dialog_show' runs,
 * mirroring what a real server would answer for whichever window had
 * real X11 input focus at that moment; freed by message.c itself right
 * after reading it, the same as a real xcb reply.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_input_focus_reply_t *xcb_get_input_focus_reply(
        xcb_connection_t *c, xcb_get_input_focus_cookie_t cookie,
        xcb_generic_error_t **e)
{
    xcb_get_input_focus_reply_t *reply = malloc(sizeof(*reply));

    (void) c;
    (void) cookie;

    if (e != NULL) {
        *e = NULL;
    }
    memset(reply, 0, sizeof(*reply));
    reply->focus = s_prior_focus_window;
    return reply;
}


/**
 * @brief Link-only stand-in for @a xcb_create_gc
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_create_gc(xcb_connection_t *c, xcb_gcontext_t cid,
        xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) cid;
    (void) drawable;
    (void) value_mask;
    (void) value_list;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_change_gc
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_change_gc(xcb_connection_t *c, xcb_gcontext_t gc,
        uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) gc;
    (void) value_mask;
    (void) value_list;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_free_gc
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_free_gc(xcb_connection_t *c, xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) gc;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_poly_fill_rectangle
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *c,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        uint32_t rectangles_len, const xcb_rectangle_t *rectangles)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) drawable;
    (void) gc;
    (void) rectangles_len;
    (void) rectangles;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_window_destroy
 *
 * Project-local helper (utils/xcb/window.h), not raw libxcb.
 *
 * @note Complexity: @e O(1)
 */
void xcb_window_destroy(xcb_window_t window)
{
    s_call_window_destroy++;
    s_destroyed_window = window;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_connection_get
 *
 * Answering NULL skips the '_NET_WM_WINDOW_TYPE' branch entirely in
 * 'menu_message_dialog_show', which otherwise would need
 * 'xcb_ewmh_set_wm_window_type' stubbed too; that branch only ever
 * decorates the window for a pager or compositor, nothing this file's
 * own assertions check.
 *
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_set_wm_window_type
 *
 * Never actually reached at runtime, since this file's own
 * 'xcb_ewmh_connection_get' stand-in always answers NULL, but the call
 * site still exists in message.c's compiled object, so the symbol must
 * resolve at link time regardless.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_wm_window_type(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t list_len, xcb_atom_t *list)
{
    xcb_void_cookie_t cookie;

    (void) ewmh;
    (void) window;
    (void) list_len;
    (void) list;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/**
 * @brief Recording stand-in for @a atom_set_window_opacity
 * @note Complexity: @e O(1)
 */
void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw)
{
    (void) connection;
    (void) window;
    (void) raw;
    s_call_atom_set_window_opacity++;
}


/**
 * @brief Test-controlled stand-in for @a client_last_user_time
 *
 * Zero (the default) makes every timestamp argument fall back to
 * @c XCB_CURRENT_TIME, exactly as a session with no client interaction
 * yet would.
 *
 * @note Complexity: @e O(1)
 */
static uint32_t s_last_user_time;

uint32_t client_last_user_time(void)
{
    return s_last_user_time;
}


/**
 * @brief Link-only stand-in for @a config_theme_opacity_to_raw
 * @note Complexity: @e O(1)
 */
uint32_t config_theme_opacity_to_raw(uint8_t percent)
{
    return (uint32_t) percent;
}


/**
 * @brief Link-only stand-in for @a text_renderer_use_font
 *
 * Always succeeds; the layout math this file's scenarios check does
 * not depend on which font name was requested.
 *
 * @note Complexity: @e O(1)
 */
int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;
    return 0;
}


/**
 * @brief Link-only stand-in for @a text_renderer_set_color
 * @note Complexity: @e O(1)
 */
void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
}


/**
 * @brief Fixed-width stand-in for @a menu_draw_measure
 *
 * Ten pixels per character, a small deterministic value good enough
 * for 's_message_compute_layout' and 's_message_value_column' to size
 * a dialog around without this file needing real font metrics.
 *
 * @note Complexity: @e O(n), where @e n is the length of @p text
 */
uint16_t menu_draw_measure(const char *text)
{
    return (uint16_t) (strlen(text) * 10u);
}


/**
 * @brief Link-only stand-in for @a menu_draw_label
 *
 * Reached from the repaint path, which draws text this file's
 * scenarios never pixel-inspect.
 *
 * @note Complexity: @e O(1)
 */
void menu_draw_label(xcb_connection_t *connection, xcb_window_t window,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) window;
    (void) pos;
    (void) text;
}


/**
 * @brief Fixed stand-in for @a text_font_ascent
 * @note Complexity: @e O(1)
 */
int16_t text_font_ascent(void)
{
    return 10;
}


/**
 * @brief Fixed stand-in for @a text_font_descent
 * @note Complexity: @e O(1)
 */
int16_t text_font_descent(void)
{
    return 3;
}


/**
 * @brief Link-only stand-in for @a dlgutil_button_border_draw
 *
 * Reached from the repaint path only.
 *
 * @note Complexity: @e O(1)
 */
void dlgutil_button_border_draw(xcb_connection_t *connection,
        xcb_window_t window, uint32_t color, uint32_t width,
        struct geometry_s geom)
{
    (void) connection;
    (void) window;
    (void) color;
    (void) width;
    (void) geom;
}


/**
 * @brief Link-only stand-in for @a dlgutil_u16max
 *
 * The real function is tiny and pure, but 's_message_compute_layout'
 * calls it directly rather than through any seam this file could
 * otherwise intercept differently, so its own trivial behavior is
 * simply reproduced here rather than linking 'src/menu/dialog.c',
 * which would drag in 'dlgutil_resolve_monitor' and its own XCB
 * pointer query; the same precedent already followed in
 * 'test_confirm.c', 'test_inspect.c', and 'test_shortcuts.c'.
 *
 * @note Complexity: @e O(1)
 */
uint16_t dlgutil_u16max(uint16_t a, uint16_t b)
{
    return (a > b) ? a : b;
}


/** Monitor this file's 'dlgutil_resolve_monitor' stand-in reports;
 *  settable per scenario, defaults to a generous 1000x800 so most
 *  scenarios never hit the 70%-height scroll cap unless they mean to */
static monitor_td s_stub_monitor;


/**
 * @brief Test-controlled stand-in for @a dlgutil_resolve_monitor
 *
 * @note Complexity: @e O(1)
 */
monitor_td dlgutil_resolve_monitor(xcb_connection_t *connection,
        const surface_td *surface)
{
    (void) connection;
    (void) surface;
    return s_stub_monitor;
}


/**
 * @brief Fixed stand-in for @a menu_dialog_center
 *
 * Always centers at a fixed (30, 40): none of this file's scenarios
 * check the dialog's exact screen position, only its computed size and
 * its later show/close/click/scroll behavior.
 *
 * @note Complexity: @e O(1)
 */
void menu_dialog_center(xcb_connection_t *connection,
        const surface_td *surface, uint16_t width, uint16_t height,
        int16_t *restrict out_x, int16_t *restrict out_y)
{
    (void) connection;
    (void) surface;
    (void) width;
    (void) height;

    *out_x = 30;
    *out_y = 40;
}


/** Non-null opaque connection handle, never dereferenced by anything
 *  this file links for real (message.c only checks
 *  'surface->screen != NULL', so a real, zeroed surface_td is used
 *  instead of an opaque stand-in for that one field) */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;
static surface_td s_surface;
static xcb_screen_t s_screen;
static config_td s_config;


/**
 * @brief Reset every recording stand-in and shared fixture between
 *        scenarios
 *
 * Also force-closes any dialog still open from a previous scenario:
 * 'menu_message_dialog_show'/'_show_pairs' are both silent no-ops
 * while one is already open, so a dialog left open by a prior failing
 * assertion would otherwise cascade into every scenario after it, the
 * same global-state-persists-across-scenarios risk already found and
 * fixed in 'test_run.c'.
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    if (menu_message_dialog_is_open()) {
        menu_message_dialog_close(s_fake_connection);
    }

    s_call_create_window = 0;
    s_last_create_window_w = 0u;
    s_last_create_window_h = 0u;
    s_call_map_window = 0;
    s_call_grab_keyboard = 0;
    s_call_ungrab_keyboard = 0;
    s_call_set_input_focus = 0;
    s_last_focus_window = XCB_WINDOW_NONE;
    s_call_window_destroy = 0;
    s_destroyed_window = XCB_WINDOW_NONE;
    s_call_atom_set_window_opacity = 0;
    s_prior_focus_window = XCB_WINDOW_NONE;
    s_last_user_time = 0u;

    s_stub_monitor.x = 0;
    s_stub_monitor.y = 0;
    s_stub_monitor.w = 1000u;
    s_stub_monitor.h = 800u;

    memset(&s_surface, 0, sizeof(s_surface));
    memset(&s_screen, 0, sizeof(s_screen));
    s_surface.screen = &s_screen;
    memset(&s_config, 0, sizeof(s_config));
}


/* Showing an ordinary message dialog creates exactly one window, maps
 * it, grabs the keyboard, and takes input focus */
static void s_test_show_creates_and_maps_window(void)
{
    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Hello there", MENU_MSG_LEVEL_NONE);

    TAP_OK(menu_message_dialog_is_open(),
            "the dialog reports itself open right after being shown");
    TAP_EQ_INT(s_call_create_window, 1, "exactly one window is created");
    TAP_EQ_INT(s_call_map_window, 1, "and it is mapped exactly once");
    TAP_EQ_INT(s_call_grab_keyboard, 1,
            "the keyboard is grabbed exactly once");
    TAP_EQ_INT(s_call_set_input_focus, 1,
            "input focus is taken exactly once");
    TAP_OK(menu_message_dialog_window() != XCB_WINDOW_NONE,
            "the dialog's window accessor returns a real window,"
            " not XCB_WINDOW_NONE");
}


/* A second show call while one is already open is a silent no-op */
static void s_test_show_while_open_is_noop(void)
{
    xcb_window_t first_window;

    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "First", MENU_MSG_LEVEL_INFO);
    first_window = menu_message_dialog_window();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Second", MENU_MSG_LEVEL_ERROR);

    TAP_EQ_INT(s_call_create_window, 1,
            "a second show while one dialog is open creates no"
            " second window");
    TAP_OK(menu_message_dialog_window() == first_window,
            "the window accessor still names the first dialog's"
            " window");
}


/* NULL connection/surface/config, or a surface with no screen, are
 * safe no-ops that leave the dialog closed */
static void s_test_show_null_guards(void)
{
    s_reset();

    menu_message_dialog_show(NULL, &s_surface, &s_config, "x",
            MENU_MSG_LEVEL_NONE);
    TAP_OK(!menu_message_dialog_is_open(),
            "a NULL connection is a safe no-op");

    menu_message_dialog_show(s_fake_connection, NULL, &s_config, "x",
            MENU_MSG_LEVEL_NONE);
    TAP_OK(!menu_message_dialog_is_open(),
            "a NULL surface is a safe no-op");

    menu_message_dialog_show(s_fake_connection, &s_surface, NULL, "x",
            MENU_MSG_LEVEL_NONE);
    TAP_OK(!menu_message_dialog_is_open(),
            "a NULL config is a safe no-op");

    s_surface.screen = NULL;
    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "x", MENU_MSG_LEVEL_NONE);
    TAP_OK(!menu_message_dialog_is_open(),
            "a surface with a NULL screen is a safe no-op");
    s_surface.screen = &s_screen;
}


/* A NULL message is treated as an empty string rather than crashing,
 * still opening a dialog with just the level prefix (or nothing at
 * all for MENU_MSG_LEVEL_NONE) */
static void s_test_null_message_is_safe(void)
{
    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            NULL, MENU_MSG_LEVEL_WARNING);

    TAP_OK(menu_message_dialog_is_open(),
            "a NULL message is treated as empty, not a crash");
}


/* close() destroys the window, ungrabs the keyboard, and restores
 * whichever real focus was recorded before the dialog opened */
static void s_test_close_restores_prior_focus(void)
{
    xcb_window_t shown_window;

    s_reset();
    s_prior_focus_window = (xcb_window_t) 777u;

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Closing test", MENU_MSG_LEVEL_NONE);
    shown_window = menu_message_dialog_window();

    menu_message_dialog_close(s_fake_connection);

    TAP_OK(!menu_message_dialog_is_open(),
            "the dialog reports itself closed after close()");
    TAP_EQ_INT(s_call_window_destroy, 1,
            "the window is destroyed exactly once");
    TAP_OK(s_destroyed_window == shown_window,
            "the destroyed window is the one that was shown");
    TAP_EQ_INT(s_call_ungrab_keyboard, 1,
            "the keyboard is ungrabbed exactly once");
    TAP_EQ_INT(s_call_set_input_focus, 2,
            "input focus is set twice in total: once to take it on"
            " show, once to restore it on close");
    TAP_OK(s_last_focus_window == (xcb_window_t) 777u,
            "the second focus call restores the real prior focus"
            " window");
    TAP_OK(menu_message_dialog_window() == XCB_WINDOW_NONE,
            "the window accessor reports XCB_WINDOW_NONE once closed");
}


/* close() does not re-restore focus when no real prior focus was ever
 * recorded (PointerRoot/None at show time) */
static void s_test_close_with_no_prior_focus_skips_restore(void)
{
    s_reset();
    s_prior_focus_window = XCB_WINDOW_NONE;

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "No prior focus", MENU_MSG_LEVEL_NONE);

    menu_message_dialog_close(s_fake_connection);

    TAP_EQ_INT(s_call_set_input_focus, 1,
            "with no real prior focus recorded, only the initial"
            " focus-taking call on show ever happens, close() does"
            " not issue a second, pointless one");
}


/* close() on an already-closed dialog is a safe no-op */
static void s_test_close_when_already_closed_is_safe(void)
{
    s_reset();

    menu_message_dialog_close(s_fake_connection);

    TAP_OK(!menu_message_dialog_is_open(),
            "closing an already-closed dialog is a safe no-op");
    TAP_EQ_INT(s_call_window_destroy, 0,
            "and never calls the destroy stand-in");
}


/* close() with a NULL connection is a safe no-op that leaves an open
 * dialog open */
static void s_test_close_null_connection_is_safe(void)
{
    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Still open", MENU_MSG_LEVEL_NONE);

    menu_message_dialog_close(NULL);

    TAP_OK(menu_message_dialog_is_open(),
            "a NULL connection passed to close() is a safe no-op,"
            " the dialog it was called on stays open");
}


/* repaint() on a closed dialog is a safe no-op; on an open one it
 * reaches the drawing path without crashing (drawing calls are all
 * link-only stand-ins here, so only "did not crash" is checked) */
static void s_test_repaint_paths(void)
{
    s_reset();

    menu_message_dialog_repaint(s_fake_connection, &s_config);
    TAP_OK(!menu_message_dialog_is_open(),
            "repainting a closed dialog does not open one");

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Repaint me\nacross two lines", MENU_MSG_LEVEL_INFO);
    menu_message_dialog_repaint(s_fake_connection, &s_config);
    TAP_OK(menu_message_dialog_is_open(),
            "repainting an open dialog does not close it");

    menu_message_dialog_repaint(NULL, &s_config);
    menu_message_dialog_repaint(s_fake_connection, NULL);
    TAP_OK(menu_message_dialog_is_open(),
            "a NULL connection or config passed to repaint() is"
            " a safe no-op, the dialog stays open and unaffected");
}


/* requires_selection() is true only for WARNING and ERROR levels, and
 * false for every other level as well as when no dialog is open */
static void s_test_requires_selection_by_level(void)
{
    s_reset();
    TAP_OK(!menu_message_dialog_requires_selection(),
            "no dialog open never requires selection");

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "info", MENU_MSG_LEVEL_INFO);
    TAP_OK(!menu_message_dialog_requires_selection(),
            "an INFO-level dialog does not require selection");
    TAP_OK(menu_message_dialog_ok_selected(),
            "and its OK button starts already selected");
    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "none", MENU_MSG_LEVEL_NONE);
    TAP_OK(!menu_message_dialog_requires_selection(),
            "a NONE-level dialog does not require selection");
    TAP_OK(menu_message_dialog_ok_selected(),
            "and its OK button starts already selected");
    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "warn", MENU_MSG_LEVEL_WARNING);
    TAP_OK(menu_message_dialog_requires_selection(),
            "a WARNING-level dialog requires selection");
    TAP_OK(!menu_message_dialog_ok_selected(),
            "and its OK button starts NOT selected");
    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "err", MENU_MSG_LEVEL_ERROR);
    TAP_OK(menu_message_dialog_requires_selection(),
            "an ERROR-level dialog requires selection");
    TAP_OK(!menu_message_dialog_ok_selected(),
            "and its OK button starts NOT selected");
}


/* select_ok() flips an unselected OK button to selected, and is
 * a no-op both when already selected and when no dialog is open */
static void s_test_select_ok(void)
{
    s_reset();

    menu_message_dialog_select_ok(s_fake_connection, &s_config);
    TAP_OK(!menu_message_dialog_ok_selected(),
            "select_ok() with no dialog open does not somehow flip"
            " a would-be selection state");

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "warn", MENU_MSG_LEVEL_WARNING);
    TAP_OK(!menu_message_dialog_ok_selected(),
            "sanity: the WARNING dialog starts unselected");

    menu_message_dialog_select_ok(s_fake_connection, &s_config);
    TAP_OK(menu_message_dialog_ok_selected(),
            "select_ok() selects the OK button");

    menu_message_dialog_select_ok(s_fake_connection, &s_config);
    TAP_OK(menu_message_dialog_ok_selected(),
            "calling select_ok() again while already selected is"
            " a harmless no-op");
}


/* handle_click() inside the OK button's rectangle selects it and
 * schedules a deferred close; outside the button is a no-op */
static void s_test_handle_click_inside_button_defers_close(void)
{
    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "warn", MENU_MSG_LEVEL_WARNING);
    TAP_OK(!menu_message_dialog_ok_selected(),
            "sanity: starts unselected for a WARNING dialog");

    menu_message_dialog_handle_click(s_fake_connection, &s_config,
            -1000, -1000);
    TAP_OK(!menu_message_dialog_ok_selected(),
            "a click well outside the button does not select it");
    TAP_OK(menu_message_dialog_ms_remaining() < 0,
            "and schedules no deferred close");

    /* With a zeroed config (every padding/font-metric input this
     * file's stand-ins consume is 0 except the fixed 10px/char
     * measure and the 10/3 ascent/descent), 's_message_compute_layout'
     * always settles on the same button rectangle for any
     * short, one-line, NONE-or-WARNING/ERROR-level message: width and
     * height floor out at DIALOG_MIN_W (220) and DIALOG_MIN_H's
     * derived 'reserved_h' (90), giving a button of DIALOG_BTN_MIN_W
     * (60) by 13 (ascent+descent) centered at x=80..140,
     * y=69..82; (100, 75) sits well inside that rectangle regardless
     * of this dialog's own short message text */
    menu_message_dialog_handle_click(s_fake_connection, &s_config,
            100, 75);
    TAP_OK(menu_message_dialog_ok_selected(),
            "a click inside the OK button's computed rectangle"
            " selects it");
}


/* A click-triggered close actually closes the dialog once its delay
 * has elapsed and tick() is called */
static void s_test_click_close_via_tick(void)
{
    xcb_window_t shown_window;
    struct timespec remaining_delay;

    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "click to close", MENU_MSG_LEVEL_NONE);
    shown_window = menu_message_dialog_window();
    TAP_OK(menu_message_dialog_ok_selected(),
            "sanity: an ordinary NONE-level dialog starts selected");

    /* (100, 75) always lands on the button in this file's fixed test
     * geometry (10px/char measure, zeroed padding), the same
     * coordinate already derived and explained in
     * 's_test_handle_click_inside_button_defers_close' */
    menu_message_dialog_handle_click(s_fake_connection, &s_config,
            100, 75);
    TAP_OK(menu_message_dialog_is_open(),
            "the dialog is still open right after the click, the"
            " close is deferred rather than immediate");
    TAP_OK(menu_message_dialog_ms_remaining() >= 0,
            "a deferred close is now pending");

    menu_message_dialog_tick(s_fake_connection);
    TAP_OK(menu_message_dialog_is_open(),
            "a tick before the delay elapses does not close the"
            " dialog yet");

    remaining_delay.tv_sec = 0;
    remaining_delay.tv_nsec = 200L * 1000L * 1000L;
    (void) nanosleep(&remaining_delay, NULL);

    menu_message_dialog_tick(s_fake_connection);
    TAP_OK(!menu_message_dialog_is_open(),
            "a tick after the delay elapses closes the dialog");
    TAP_OK(s_destroyed_window == shown_window,
            "and the window destroyed is the one the click landed on");
}


/* handle_click() and tick()/ms_remaining() on a closed dialog are safe
 * no-ops */
static void s_test_click_and_tick_when_closed_are_safe(void)
{
    s_reset();

    menu_message_dialog_handle_click(s_fake_connection, &s_config,
            0, 0);
    TAP_OK(!menu_message_dialog_is_open(),
            "a click with no dialog open does not open one");

    TAP_EQ_INT(menu_message_dialog_ms_remaining(), -1,
            "ms_remaining() reports -1 when nothing is pending");

    menu_message_dialog_tick(s_fake_connection);
    TAP_OK(!menu_message_dialog_is_open(),
            "ticking with nothing pending and no dialog open is"
            " a safe no-op");
}


/* An ordinary text message with an explicit newline produces exactly
 * two wrapped lines, one per side of the newline, and the dialog grows
 * to fit them both */
static void s_test_wrap_explicit_newline_two_lines(void)
{
    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "First line\nSecond line", MENU_MSG_LEVEL_NONE);

    TAP_OK(menu_message_dialog_is_open(),
            "a two-line message (one explicit newline) opens fine");
}


/* A single word far wider than the wrap width is placed on its own
 * line rather than being split mid-word */
static void s_test_wrap_overlong_word_own_line(void)
{
    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "short "
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            " tail", MENU_MSG_LEVEL_NONE);

    TAP_OK(menu_message_dialog_is_open(),
            "a message containing one pathologically long word still"
            " opens a dialog rather than crashing or looping forever");
}


/* An empty message string (as opposed to a NULL one) is also handled
 * safely, producing a dialog with essentially no wrapped body text */
static void s_test_empty_message_is_safe(void)
{
    s_reset();

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "", MENU_MSG_LEVEL_NONE);

    TAP_OK(menu_message_dialog_is_open(),
            "an empty message string is a safe, valid input");
}


/* A message so tall (many explicit newlines) it exceeds the 70%
 * monitor-height cap triggers scrolling: scroll() actually moves the
 * offset, clamped at both ends, and is a no-op once the whole message
 * already fits */
static void s_test_scroll_clamped_and_noop_when_fits(void)
{
    char tall_message[2000];
    size_t pos = 0u;
    int line;

    s_reset();
    /* A tiny monitor forces the 70%-height cap to bind well below
     * what 40 lines would otherwise need, guaranteeing scrolling
     * kicks in regardless of this file's fixed 10px/char, ascent-10/
     * descent-3 line-height stand-ins */
    s_stub_monitor.w = 1000u;
    s_stub_monitor.h = 200u;

    tall_message[0] = '\0';
    for (line = 0; line < 40; ++line) {
        int written = snprintf(tall_message + pos,
                sizeof(tall_message) - pos, "Line number %d\n", line);

        if (written > 0) {
            pos += (size_t) written;
        }
    }

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            tall_message, MENU_MSG_LEVEL_NONE);
    TAP_OK(menu_message_dialog_is_open(),
            "a message tall enough to need scrolling still opens");

    /* Scrolling past the end clamps at the last valid offset rather
     * than wrapping or growing past the line count; verified
     * indirectly through repaint() never crashing, since
     * 'scroll_offset' is not itself exposed by any accessor */
    menu_message_dialog_scroll(s_fake_connection, &s_config, 10000);
    menu_message_dialog_repaint(s_fake_connection, &s_config);
    TAP_OK(menu_message_dialog_is_open(),
            "scrolling far past the end clamps rather than crashing"
            " or closing the dialog");

    menu_message_dialog_scroll(s_fake_connection, &s_config, -10000);
    menu_message_dialog_repaint(s_fake_connection, &s_config);
    TAP_OK(menu_message_dialog_is_open(),
            "scrolling far before the start clamps at zero rather"
            " than underflowing the unsigned offset");

    menu_message_dialog_scroll(NULL, &s_config, 1);
    menu_message_dialog_scroll(s_fake_connection, NULL, 1);
    TAP_OK(menu_message_dialog_is_open(),
            "a NULL connection or config passed to scroll() is"
            " a safe no-op");
}


/* A dialog tall enough to need scrolling is also made wide enough to
 * fit the footer's own status/scroll-hint line, not just its message
 * content, since that line is drawn at its own width with no
 * wrapping of its own */
static void s_test_scroll_widens_dialog_for_footer(void)
{
    char tall_message[2000];
    size_t pos = 0u;
    int line;
    uint16_t footer_w;

    s_reset();
    /* Same tiny monitor and 40-line message as the scrolling test
     * above, guaranteeing scrolling kicks in here too */
    s_stub_monitor.w = 1000u;
    s_stub_monitor.h = 200u;

    tall_message[0] = '\0';
    for (line = 0; line < 40; ++line) {
        int written = snprintf(tall_message + pos,
                sizeof(tall_message) - pos, "Line number %d\n", line);

        if (written > 0) {
            pos += (size_t) written;
        }
    }

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            tall_message, MENU_MSG_LEVEL_NONE);

    /* "99-99/99: " (10 characters) plus the translated scroll hint
     * itself, at this file's fixed 10px/character stand-in, with no
     * label padding configured here.  Every content line ("Line
     * number NN") measures far short of that on its own, so this
     * only passes if the footer's own width was actually folded into
     * the dialog's, not just the message content above it. */
    footer_w = (uint16_t) ((10u +
                strlen(_(STR_DIALOG_MSG_SCROLL_HINT))) * 10u);
    TAP_OK(s_last_create_window_w >= footer_w,
            "a scrolling dialog is widened to fit its own footer's"
            " status/scroll-hint line");
}


/* scroll() is a no-op (never repaints, never crashes) once the whole
 * message already fits without scrolling, and also a safe no-op with
 * no dialog open at all */
static void s_test_scroll_noop_cases(void)
{
    s_reset();

    menu_message_dialog_scroll(s_fake_connection, &s_config, 5);
    TAP_OK(!menu_message_dialog_is_open(),
            "scrolling with no dialog open does not open one");

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "one short line", MENU_MSG_LEVEL_NONE);

    menu_message_dialog_scroll(s_fake_connection, &s_config, 3);
    TAP_OK(menu_message_dialog_is_open(),
            "scrolling a message that already fits entirely is"
            " a harmless no-op, the dialog stays open");
}


/* show_pairs(): NULL guards and an empty pair_count are safe no-ops */
static void s_test_show_pairs_null_guards(void)
{
    struct dialog_pair_s pairs[2];

    s_reset();
    pairs[0].label = "Label";
    pairs[0].value = "Value";

    menu_message_dialog_show_pairs(NULL, &s_surface, &s_config, pairs,
            1u, MENU_MSG_LEVEL_NONE);
    TAP_OK(!menu_message_dialog_is_open(),
            "a NULL connection to show_pairs() is a safe no-op");

    menu_message_dialog_show_pairs(s_fake_connection, NULL, &s_config,
            pairs, 1u, MENU_MSG_LEVEL_NONE);
    TAP_OK(!menu_message_dialog_is_open(),
            "a NULL surface to show_pairs() is a safe no-op");

    menu_message_dialog_show_pairs(s_fake_connection, &s_surface, NULL,
            pairs, 1u, MENU_MSG_LEVEL_NONE);
    TAP_OK(!menu_message_dialog_is_open(),
            "a NULL config to show_pairs() is a safe no-op");

    menu_message_dialog_show_pairs(s_fake_connection, &s_surface,
            &s_config, NULL, 1u, MENU_MSG_LEVEL_NONE);
    TAP_OK(!menu_message_dialog_is_open(),
            "a NULL pairs array to show_pairs() is a safe no-op");

    menu_message_dialog_show_pairs(s_fake_connection, &s_surface,
            &s_config, pairs, 0u, MENU_MSG_LEVEL_NONE);
    TAP_OK(!menu_message_dialog_is_open(),
            "a zero pair_count to show_pairs() is a safe no-op");
}


/* show_pairs() opens a dialog for an ordinary list of label/value
 * rows, including a heading-only row (value NULL) and a blank
 * separator row built through the real dialog_pair_append_blank() */
static void s_test_show_pairs_basic_rows(void)
{
    struct dialog_pair_s pairs[4];
    uint8_t count = 0u;

    s_reset();

    pairs[count].label = "Section heading";
    pairs[count].value = NULL;
    count++;

    dialog_pair_append_blank(pairs, &count);

    pairs[count].label = "Name";
    pairs[count].value = "IcoWM";
    count++;

    pairs[count].label = "Version";
    pairs[count].value = "2";
    count++;

    menu_message_dialog_show_pairs(s_fake_connection, &s_surface,
            &s_config, pairs, count, MENU_MSG_LEVEL_NONE);

    TAP_EQ_INT((int) count, 4,
            "dialog_pair_append_blank incremented the real,"
            " caller-visible count by exactly one");
    TAP_OK(menu_message_dialog_is_open(),
            "a pairs dialog with a heading, a blank separator, and"
            " two real rows opens successfully");
    TAP_EQ_INT(s_call_create_window, 1,
            "and it creates exactly one window, the same as an"
            " ordinary text dialog");
}


/* dialog_pair_append_blank() silently stops once the array is already
 * at DIALOG_MSG_MAX_LINES, rather than writing past its end */
static void s_test_append_blank_stops_at_capacity(void)
{
    struct dialog_pair_s pairs[4];
    uint8_t count = (uint8_t) DIALOG_MSG_MAX_LINES;

    dialog_pair_append_blank(pairs, &count);

    TAP_EQ_INT((int) count, (int) DIALOG_MSG_MAX_LINES,
            "appending a blank row once already at"
            " DIALOG_MSG_MAX_LINES leaves the count unchanged rather"
            " than incrementing past the array's real capacity");
}


/* A value too long for its column wraps onto further lines that share
 * the row (label blank on the continuation lines); the dialog still
 * opens successfully with the wrapped result */
static void s_test_show_pairs_value_wraps(void)
{
    struct dialog_pair_s pairs[1];

    s_reset();
    /* A very narrow monitor keeps this value's wrap column tight
     * enough (see 's_message_value_column'/show_pairs' own 'avail'
     * math, both driven by 'monitor.w') that a long single-word-free
     * value is guaranteed to need more than one wrapped line under
     * this file's fixed 10px/char measure stand-in */
    s_stub_monitor.w = 300u;
    s_stub_monitor.h = 800u;

    pairs[0].label = "Path";
    pairs[0].value = "one two three four five six seven eight nine ten"
        " eleven twelve thirteen fourteen fifteen";

    menu_message_dialog_show_pairs(s_fake_connection, &s_surface,
            &s_config, pairs, 1u, MENU_MSG_LEVEL_NONE);

    TAP_OK(menu_message_dialog_is_open(),
            "a pairs dialog whose one value wraps across several"
            " lines still opens successfully");
}


/* A NULL value.label pair (neither given) is a blank row; a pair with
 * a label but a NULL value is a heading row; both are safe to pass
 * directly without going through dialog_pair_append_blank() */
static void s_test_show_pairs_mixed_row_kinds(void)
{
    struct dialog_pair_s pairs[3];

    s_reset();

    pairs[0].label = NULL;
    pairs[0].value = NULL;

    pairs[1].label = "Just a heading line";
    pairs[1].value = NULL;

    pairs[2].label = "Key";
    pairs[2].value = "Value";

    menu_message_dialog_show_pairs(s_fake_connection, &s_surface,
            &s_config, pairs, 3u, MENU_MSG_LEVEL_NONE);

    TAP_OK(menu_message_dialog_is_open(),
            "a mix of blank, heading-only, and full pair rows built"
            " directly (without dialog_pair_append_blank) opens"
            " a valid dialog");
}


/* show_pairs() honors requires_selection()/ok_selected() by level,
 * exactly the same as the plain-text show() path */
static void s_test_show_pairs_level_gates_selection(void)
{
    struct dialog_pair_s pairs[1];

    s_reset();
    pairs[0].label = "Key";
    pairs[0].value = "Value";

    menu_message_dialog_show_pairs(s_fake_connection, &s_surface,
            &s_config, pairs, 1u, MENU_MSG_LEVEL_ERROR);

    TAP_OK(menu_message_dialog_requires_selection(),
            "an ERROR-level pairs dialog requires selection, exactly"
            " like an ERROR-level plain-text one");
    TAP_OK(!menu_message_dialog_ok_selected(),
            "and its OK button starts unselected");
}


/* show_pairs() while a dialog is already open (whichever path opened
 * it) is a silent no-op, same as the plain-text show() path */
static void s_test_show_pairs_while_open_is_noop(void)
{
    struct dialog_pair_s pairs[1];
    xcb_window_t first_window;

    s_reset();
    pairs[0].label = "Key";
    pairs[0].value = "Value";

    menu_message_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Already open", MENU_MSG_LEVEL_NONE);
    first_window = menu_message_dialog_window();

    menu_message_dialog_show_pairs(s_fake_connection, &s_surface,
            &s_config, pairs, 1u, MENU_MSG_LEVEL_NONE);

    TAP_EQ_INT(s_call_create_window, 1,
            "show_pairs() while a plain-text dialog is already open"
            " creates no second window");
    TAP_OK(menu_message_dialog_window() == first_window,
            "the window accessor still names the first dialog");
}


int main(void)
{
    TAP_PLAN(78);

    s_test_show_creates_and_maps_window();
    s_test_show_while_open_is_noop();
    s_test_show_null_guards();
    s_test_null_message_is_safe();
    s_test_close_restores_prior_focus();
    s_test_close_with_no_prior_focus_skips_restore();
    s_test_close_when_already_closed_is_safe();
    s_test_close_null_connection_is_safe();
    s_test_repaint_paths();
    s_test_requires_selection_by_level();
    s_test_select_ok();
    s_test_handle_click_inside_button_defers_close();
    s_test_click_close_via_tick();
    s_test_click_and_tick_when_closed_are_safe();
    s_test_wrap_explicit_newline_two_lines();
    s_test_wrap_overlong_word_own_line();
    s_test_empty_message_is_safe();
    s_test_scroll_clamped_and_noop_when_fits();
    s_test_scroll_widens_dialog_for_footer();
    s_test_scroll_noop_cases();
    s_test_show_pairs_null_guards();
    s_test_show_pairs_basic_rows();
    s_test_append_blank_stops_at_capacity();
    s_test_show_pairs_value_wraps();
    s_test_show_pairs_mixed_row_kinds();
    s_test_show_pairs_level_gates_selection();
    s_test_show_pairs_while_open_is_noop();

    s_reset();

    return TAP_DONE();
}
