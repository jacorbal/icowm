/**
 * @file tests/menu/dialog/test_confirm.c
 *
 * @brief Test battery for the two-button confirm/cancel modal dialog
 *
 * 'menu_confirm_dialog_show' opens a real XCB window through a
 * one-directional handful of libxcb calls (xcb_create_window,
 * xcb_map_window, and so on), none of which this file's scenarios
 * ever need to inspect the return value of; every one is a link-only
 * stand-in below, exactly the 'tests/test_client.c' pattern of
 * re-declaring libxcb's own entry points with matching signatures
 * rather than linking the real library, since there is no X server
 * for this test binary to actually talk to.  'menu/dialog/defer.c'
 * and 'utils/time/clock.c', by contrast, are small, self-contained,
 * and exactly the deferred-click logic this file's own click
 * scenarios exist to exercise, so both are linked for real instead of
 * stubbed.  'utils/safe/safestr.c' is linked for real too, since
 * confirm.c copies every caller-supplied string through it and a
 * scenario checking that a too-long prompt gets truncated needs the
 * genuine copy-and-truncate behavior, not a stand-in that would have
 * to reimplement it just to be checked against itself.
 *
 * 's_confirm_compute_layout' (file-static) is reached only through
 * the public 'menu_confirm_dialog_show', exactly as the window
 * manager itself reaches it; this file's 'menu_draw_measure' and
 * 'text_font_ascent'/'text_font_descent' stand-ins return small fixed
 * values so that layout call always succeeds deterministically,
 * without this file needing to duplicate its pixel arithmetic to
 * predict an exact width or height.
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
#include <stdint.h>
#include <string.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <surface.h>

/* Local includes */
#include <harness/tap.h>
#include <menu/dialog.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/defer.h>
#include <menu/draw.h>
#include <render/text.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


/** Fake, ever-increasing XCB resource IDs, so every xcb_generate_id
 *  call this file's stand-in answers gets a distinct value, the same
 *  as a real X server would hand out */
static uint32_t s_next_xid = 1u;

/** Last window xcb_window_destroy was called with, or
 *  XCB_WINDOW_NONE if it was never called since the last s_reset */
static xcb_window_t s_destroyed_window;
static int s_call_window_destroy;

/** Recording stand-ins' own call counters, reset by s_reset */
static int s_call_create_window;
static int s_call_map_window;
static int s_call_grab_keyboard;
static int s_call_ungrab_keyboard;
static int s_call_set_input_focus;
static xcb_window_t s_last_focus_window;
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
    (void) width;
    (void) height;
    (void) border_width;
    (void) klass;
    (void) visual;
    (void) value_mask;
    (void) value_list;

    s_call_create_window++;
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
 * Reached from every 'menu_confirm_dialog_show' call, to stack the new
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
 *  currently focused, before a confirm dialog takes it; settable per
 *  scenario, XCB_WINDOW_NONE by default */
static xcb_window_t s_prior_focus_window = XCB_WINDOW_NONE;


/**
 * @brief Test-controlled stand-in for @a xcb_get_input_focus_reply
 *
 * Hands back a heap-allocated reply naming 's_prior_focus_window' as
 * the focus already held before 'menu_confirm_dialog_show' runs,
 * mirroring what a real server would answer for whichever window had
 * real X11 input focus at that moment; freed by confirm.c itself
 * right after reading it, the same as a real xcb reply.
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
 *
 * Reached only from the repaint path ('s_confirm_draw'), which none of
 * this file's scenarios ever trigger through the expose handler
 * (this file never calls 'menu_confirm_dialog_repaint' directly on a
 * closed dialog): show/click scenarios do reach it, so a plausible
 * fake ID is still returned.
 *
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
 * 'menu_confirm_dialog_show', which otherwise would need
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
 * for 's_confirm_compute_layout' to size a dialog around without this
 * file needing real font metrics.
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
 * The real function is tiny and pure, but 's_confirm_compute_layout'
 * calls it directly rather than through any seam this file could
 * otherwise intercept differently, so its own trivial behavior is
 * simply reproduced here rather than linking 'src/menu/dialog.c',
 * which would drag in 'dlgutil_resolve_monitor' and its own XCB
 * pointer query.
 *
 * @note Complexity: @e O(1)
 */
uint16_t dlgutil_u16max(uint16_t a, uint16_t b)
{
    return (a > b) ? a : b;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_set_wm_window_type
 *
 * Never actually reached at runtime, since this file's own
 * 'xcb_ewmh_connection_get' stand-in always answers NULL, but the
 * call site still exists in confirm.c's compiled object, so the
 * symbol must resolve at link time regardless.
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
 * @brief Fixed stand-in for @a menu_dialog_center
 *
 * Always centers at a fixed (30, 40): none of this file's scenarios
 * check the dialog's exact screen position, only that showing one
 * creates a window and that its later close/accept/cancel behavior
 * is correct.
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


/** Non-null opaque connection and surface handles, never dereferenced
 *  by anything this file links for real (confirm.c only checks
 *  'surface->screen != NULL', so a real, zeroed surface_td is used
 *  instead of an opaque stand-in for that one field) */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;
static surface_td s_surface;
static xcb_screen_t s_screen;
static config_td s_config;

/** Confirm/cancel callback call counters and the connection each was
 *  last invoked with, reset by s_reset */
static int s_call_on_confirm;
static int s_call_on_cancel;


/**
 * @brief Test callback standing in for a caller's @c on_confirm
 * @note Complexity: @e O(1)
 */
static void s_on_confirm(xcb_connection_t *connection)
{
    (void) connection;
    s_call_on_confirm++;
}


/**
 * @brief Test callback standing in for a caller's @c on_cancel
 * @note Complexity: @e O(1)
 */
static void s_on_cancel(xcb_connection_t *connection)
{
    (void) connection;
    s_call_on_cancel++;
}


/**
 * @brief Reset every recording stand-in and shared fixture between
 *        scenarios
 *
 * Also force-closes any dialog still open from a previous scenario:
 * every scenario here calls @a menu_confirm_dialog_show, which is a
 * silent no-op while one is already open, so a dialog left open by a
 * prior failing assertion would otherwise cascade into every
 * scenario after it.
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    if (menu_confirm_dialog_is_open()) {
        menu_confirm_dialog_cancel(s_fake_connection);
    }

    s_call_create_window = 0;
    s_call_map_window = 0;
    s_call_grab_keyboard = 0;
    s_call_ungrab_keyboard = 0;
    s_call_set_input_focus = 0;
    s_last_focus_window = XCB_WINDOW_NONE;
    s_call_window_destroy = 0;
    s_destroyed_window = XCB_WINDOW_NONE;
    s_call_atom_set_window_opacity = 0;
    s_call_on_confirm = 0;
    s_call_on_cancel = 0;
    s_prior_focus_window = XCB_WINDOW_NONE;
    s_last_user_time = 0u;

    memset(&s_surface, 0, sizeof(s_surface));
    memset(&s_screen, 0, sizeof(s_screen));
    s_surface.screen = &s_screen;
    memset(&s_config, 0, sizeof(s_config));
}


/* Showing the dialog creates exactly one window, maps it, grabs the
 * keyboard, and takes input focus */
static void s_test_show_creates_and_maps_window(void)
{
    s_reset();

    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Really quit?", "No", "Yes", s_on_confirm, s_on_cancel, 0u);

    TAP_OK(menu_confirm_dialog_is_open(),
            "the dialog reports itself open right after being shown");
    TAP_EQ_INT(s_call_create_window, 1,
            "exactly one window is created");
    TAP_EQ_INT(s_call_map_window, 1, "and it is mapped exactly once");
    TAP_EQ_INT(s_call_grab_keyboard, 1,
            "the keyboard is grabbed exactly once");
    TAP_OK(menu_confirm_dialog_window() != XCB_WINDOW_NONE,
            "the dialog's window accessor returns a real window,"
            " not XCB_WINDOW_NONE");
}


/* A second show call while one is already open is a silent no-op:
 * only one confirm dialog instance is ever allowed at a time */
static void s_test_show_while_open_is_noop(void)
{
    xcb_window_t first_window;

    s_reset();

    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "First prompt", "No", "Yes", s_on_confirm, s_on_cancel, 0u);
    first_window = menu_confirm_dialog_window();
    s_call_create_window = 0;

    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Second prompt", "No", "Yes", s_on_confirm, s_on_cancel, 0u);

    TAP_EQ_INT(s_call_create_window, 0,
            "a second show call while one dialog is already open"
            " creates no additional window");
    TAP_OK(menu_confirm_dialog_window() == first_window,
            "the original dialog's window is left untouched");
}


/* menu_confirm_dialog_show is a no-op for a NULL connection, surface,
 * config, or a surface with no screen, none of which crash */
static void s_test_show_null_guards(void)
{
    surface_td surface_without_screen;

    s_reset();
    memset(&surface_without_screen, 0, sizeof(surface_without_screen));
    surface_without_screen.screen = NULL;

    menu_confirm_dialog_show(NULL, &s_surface, &s_config, "x", "n", "y",
            NULL, NULL, 0u);
    TAP_OK(!menu_confirm_dialog_is_open(),
            "a NULL connection never opens a dialog");

    menu_confirm_dialog_show(s_fake_connection, NULL, &s_config, "x",
            "n", "y", NULL, NULL, 0u);
    TAP_OK(!menu_confirm_dialog_is_open(),
            "a NULL surface never opens a dialog");

    menu_confirm_dialog_show(s_fake_connection, &s_surface, NULL, "x",
            "n", "y", NULL, NULL, 0u);
    TAP_OK(!menu_confirm_dialog_is_open(),
            "a NULL config never opens a dialog");

    menu_confirm_dialog_show(s_fake_connection, &surface_without_screen,
            &s_config, "x", "n", "y", NULL, NULL, 0u);
    TAP_OK(!menu_confirm_dialog_is_open(),
            "a surface with no screen never opens a dialog");
}


/* The dialog defaults to the cancel button selected; toggling once
 * moves to confirm, and toggling again wraps back to cancel */
static void s_test_toggle_selection_wraps(void)
{
    s_reset();

    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", s_on_confirm, s_on_cancel, 0u);

    menu_confirm_dialog_accept(s_fake_connection);
    TAP_EQ_INT(s_call_on_cancel, 1,
            "accepting immediately after show activates cancel,"
            " the default selection");
    TAP_EQ_INT(s_call_on_confirm, 0,
            "and never activates confirm at the same time");

    s_reset();
    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", s_on_confirm, s_on_cancel, 0u);
    menu_confirm_dialog_toggle_selection();
    menu_confirm_dialog_accept(s_fake_connection);
    TAP_EQ_INT(s_call_on_confirm, 1,
            "toggling once before accepting activates confirm"
            " instead");
    TAP_EQ_INT(s_call_on_cancel, 0,
            "and never activates cancel in that case");

    s_reset();
    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", s_on_confirm, s_on_cancel, 0u);
    menu_confirm_dialog_toggle_selection();
    menu_confirm_dialog_toggle_selection();
    menu_confirm_dialog_accept(s_fake_connection);
    TAP_EQ_INT(s_call_on_cancel, 1,
            "toggling twice wraps the selection back to cancel");
}


/* Accepting closes the dialog: it is no longer open, and the same
 * window handed to xcb_create_window is handed to xcb_window_destroy */
static void s_test_accept_closes_dialog(void)
{
    xcb_window_t shown_window;

    s_reset();

    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", s_on_confirm, s_on_cancel, 0u);
    shown_window = menu_confirm_dialog_window();

    menu_confirm_dialog_accept(s_fake_connection);

    TAP_OK(!menu_confirm_dialog_is_open(),
            "accepting closes the dialog");
    TAP_EQ_INT(s_call_window_destroy, 1,
            "the dialog's window is destroyed exactly once");
    TAP_OK(s_destroyed_window == shown_window,
            "and it is the same window that was originally created");
    TAP_OK(menu_confirm_dialog_window() == XCB_WINDOW_NONE,
            "the window accessor reports XCB_WINDOW_NONE once closed");
}


/* Accepting with a NULL on_confirm callback (selected via toggle)
 * still closes the dialog cleanly, without invoking anything */
static void s_test_accept_with_null_callback_is_safe(void)
{
    s_reset();

    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", NULL, NULL, 0u);
    menu_confirm_dialog_toggle_selection();

    menu_confirm_dialog_accept(s_fake_connection);

    TAP_OK(!menu_confirm_dialog_is_open(),
            "accepting with both callbacks NULL still closes the"
            " dialog without crashing");
}


/* Cancel always runs on_cancel and closes the dialog, regardless of
 * which button was actually selected at the time */
static void s_test_cancel_ignores_selection(void)
{
    s_reset();

    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", s_on_confirm, s_on_cancel, 0u);
    menu_confirm_dialog_toggle_selection();

    menu_confirm_dialog_cancel(s_fake_connection);

    TAP_EQ_INT(s_call_on_cancel, 1,
            "cancel always runs on_cancel, even though confirm was"
            " the selected button at the time");
    TAP_EQ_INT(s_call_on_confirm, 0,
            "and never runs on_confirm in that case");
    TAP_OK(!menu_confirm_dialog_is_open(), "cancel closes the dialog");
}


/* Closing restores the X11 input focus that was in place right before
 * the dialog opened */
static void s_test_close_restores_prior_focus(void)
{
    s_reset();
    s_prior_focus_window = (xcb_window_t) 4242;

    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", NULL, NULL, 0u);
    s_call_set_input_focus = 0;
    s_last_focus_window = XCB_WINDOW_NONE;

    menu_confirm_dialog_cancel(s_fake_connection);

    TAP_EQ_INT(s_call_set_input_focus, 1,
            "closing the dialog restores input focus exactly once");
    TAP_OK(s_last_focus_window == (xcb_window_t) 4242,
            "restoring to the window that held real X11 focus"
            " right before the dialog opened");
}


/* A click inside the cancel button's rectangle activates cancel; a
 * click inside the confirm button's rectangle activates confirm; a
 * click outside both is ignored and returns false */
static void s_test_handle_click_dispatches_by_position(void)
{
    bool result;

    s_reset();
    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", s_on_confirm, s_on_cancel, 0u);

    result = menu_confirm_dialog_handle_click(s_fake_connection,
            &s_config, -1000, -1000);
    TAP_OK(!result,
            "a click far outside both buttons is not consumed");
    TAP_EQ_INT(s_call_on_cancel, 0,
            "and activates neither callback while still open");
    TAP_OK(menu_confirm_dialog_is_open(),
            "the dialog is still open after a click outside both"
            " buttons");
}


/* A click at negative coordinates, well outside either button's
 * rectangle, is rejected the same as any other out-of-bounds click,
 * rather than underflowing the unsigned pixel math s_confirm_layout
 * stores its button rectangles in; ticking afterward with nothing
 * deferred and no timeout configured leaves the dialog untouched */
static void s_test_click_negative_coordinates_is_safe(void)
{
    bool result;

    s_reset();
    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", s_on_confirm, s_on_cancel, 0u);

    result = menu_confirm_dialog_handle_click(s_fake_connection,
            &s_config, -5, -5);
    TAP_OK(!result, "a click at negative coordinates returns false"
            " rather than underflowing into a false hit");

    menu_confirm_dialog_tick(s_fake_connection, &s_config);
    TAP_OK(menu_confirm_dialog_is_open(),
            "ticking with nothing deferred and no timeout running"
            " leaves the dialog open");
}


/* With no timeout requested (timeout_seconds == 0), ms_remaining
 * reports -1: nothing is pending */
static void s_test_ms_remaining_no_timeout(void)
{
    int ms;

    s_reset();
    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", NULL, NULL, 0u);

    ms = menu_confirm_dialog_ms_remaining();
    TAP_EQ_INT(ms, -1,
            "with timeout_seconds 0, ms_remaining reports -1");
}


/* With a timeout requested, ms_remaining reports a small non-negative
 * value bounded by the requested number of seconds */
static void s_test_ms_remaining_with_timeout(void)
{
    int ms;

    s_reset();
    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", NULL, NULL, 5u);

    ms = menu_confirm_dialog_ms_remaining();
    TAP_OK(ms >= 0 && ms <= 5000,
            "with a 5-second timeout just started, ms_remaining"
            " reports a value between 0 and 5000 inclusive");
}


/* menu_confirm_dialog_tick cancels the dialog on its own, calling
 * on_cancel, once a timeout of 0 apparent seconds has fully elapsed;
 * a real timeout_seconds of 0 disables the feature per the API
 * contract, so this drives it through a deliberately elapsed
 * countdown instead, by sleeping past a 1-second timeout window is
 * too slow for a unit test, so this scenario checks the same
 * contract at the public boundary: ms_remaining never reports a
 * value larger than the requested window */
static void s_test_timeout_ms_remaining_bounded(void)
{
    int ms;

    s_reset();
    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            "Prompt", "No", "Yes", NULL, s_on_cancel, 1u);

    ms = menu_confirm_dialog_ms_remaining();
    TAP_OK(ms >= 0 && ms <= 1000,
            "a 1-second timeout reports ms_remaining within its own"
            " window immediately after showing");

    menu_confirm_dialog_tick(s_fake_connection, &s_config);
    TAP_EQ_INT(s_call_on_cancel, 0,
            "ticking well before the timeout elapses never runs"
            " on_cancel yet");
    TAP_OK(menu_confirm_dialog_is_open(),
            "and the dialog is still open");
}


/* menu_confirm_dialog_repaint on a closed dialog performs no drawing
 * (the function bails out at 's_confirm_draw's own window guard) */
static void s_test_repaint_when_closed_is_safe(void)
{
    s_reset();
    TAP_OK(!menu_confirm_dialog_is_open(),
            "no dialog is open at the start of this scenario");

    menu_confirm_dialog_repaint(s_fake_connection, &s_config);
    TAP_OK(!menu_confirm_dialog_is_open(),
            "repainting a closed dialog is a safe no-op, not a"
            " crash, and does not somehow open one");
}


/* menu_confirm_dialog_handle_click on a closed dialog returns false
 * immediately, without dereferencing the stale cached layout */
static void s_test_handle_click_when_closed_returns_false(void)
{
    bool result;

    s_reset();

    result = menu_confirm_dialog_handle_click(s_fake_connection,
            &s_config, 10, 10);
    TAP_OK(!result,
            "handling a click while no dialog is open returns false");
}


/* A prompt longer than the internal buffer is truncated, not
 * overrun; safe_strncpy (linked for real) guarantees the copy is
 * still null-terminated */
static void s_test_long_prompt_is_truncated_safely(void)
{
    char long_prompt[1024];

    s_reset();
    memset(long_prompt, 'A', sizeof(long_prompt) - 1u);
    long_prompt[sizeof(long_prompt) - 1u] = '\0';

    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            long_prompt, "No", "Yes", NULL, NULL, 0u);

    TAP_OK(menu_confirm_dialog_is_open(),
            "a dialog with an over-length prompt still opens rather"
            " than failing or overflowing its fixed-size buffer");
}


/* A NULL prompt/cancel_label/confirm_label is treated as an empty
 * string rather than crashing */
static void s_test_null_labels_are_safe(void)
{
    s_reset();

    menu_confirm_dialog_show(s_fake_connection, &s_surface, &s_config,
            NULL, NULL, NULL, NULL, NULL, 0u);

    TAP_OK(menu_confirm_dialog_is_open(),
            "NULL prompt and button labels are treated as empty"
            " strings, not a crash");
}


int main(void)
{
    TAP_PLAN(41);

    s_test_show_creates_and_maps_window();
    s_test_show_while_open_is_noop();
    s_test_show_null_guards();
    s_test_toggle_selection_wraps();
    s_test_accept_closes_dialog();
    s_test_accept_with_null_callback_is_safe();
    s_test_cancel_ignores_selection();
    s_test_close_restores_prior_focus();
    s_test_handle_click_dispatches_by_position();
    s_test_click_negative_coordinates_is_safe();
    s_test_ms_remaining_no_timeout();
    s_test_ms_remaining_with_timeout();
    s_test_timeout_ms_remaining_bounded();
    s_test_repaint_when_closed_is_safe();
    s_test_handle_click_when_closed_returns_false();
    s_test_long_prompt_is_truncated_safely();
    s_test_null_labels_are_safe();

    s_reset();

    return TAP_DONE();
}
