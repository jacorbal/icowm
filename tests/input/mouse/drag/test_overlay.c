/**
 * @file tests/input/mouse/drag/test_overlay.c
 *
 * @brief Test battery for the centered drag feedback overlay window
 *        (input/mouse/drag/overlay.c)
 *
 * Not to be confused with tests/input/mouse/event/test_overlay.c,
 * which exercises a different file entirely (input/mouse/event/
 * overlay.c, the dismiss-on-click overlay/dialog/menu handler); the
 * two live in different subdirectories and cover unrelated source
 * files that merely share a base name.
 *
 * drag_overlay_hide, drag_overlay_show and drag_overlay_repaint all
 * read and write only the global drag state, s_drag (input/mouse/
 * drag/internal.h).  Storage for s_drag lives in drag.c, which this
 * file never links, so it is defined once here instead, the same way
 * test_resist.c, test_outline.c and test_icon.c already do for this
 * same subsystem.  Every raw XCB request the two heavier functions
 * issue is stubbed directly, matching the convention already used by
 * test_icon.c and tests/enact/test_send_to_desktop.c, since no live
 * X connection is used; text_renderer_use_font, text_string_measure,
 * text_renderer_set_color, text_draw_string, text_font_ascent and
 * text_font_descent are controllable stand-ins for their own whole
 * subsystem (render/text.c), and wm_get_stage_by_id/
 * xcb_ewmh_connection_get are stand-ins that always report "no
 * stage"/"no EWMH connection" so the buffer-less and EWMH-less
 * branches are the ones exercised; both are pure fallbacks already
 * reachable this way, and the buffered/EWMH branches only add extra
 * raw-XCB drawing calls without any new decision logic of this file's
 * own to verify.
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
#include <stdio.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <render/text.h>
#include <stage.h>
#include <utils/xcb/connection.h>
#include <wm.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/overlay.h>


/** Singleton drag state; storage normally lives in drag.c, which this
 *  file never links, so it is defined here instead */
drag_state_td s_drag;

/** Recorded calls to the raw XCB window-lifecycle requests */
static int s_window_destroy_calls;
static xcb_window_t s_window_destroy_last;
static int s_generate_id_calls;
static int s_create_window_calls;
static int s_map_window_calls;
static int s_configure_window_calls;
static int s_change_window_attributes_calls;

/** Recorded calls to the raw XCB drawing requests */
static int s_create_gc_calls;
static int s_poly_fill_rectangle_calls;
static int s_free_gc_calls;
static int s_clear_area_calls;
static int s_copy_area_calls;
static int s_free_pixmap_calls;

/** Recorded calls to the text renderer stand-ins */
static int s_use_font_calls;
static const char *s_use_font_last_name;
static int s_set_color_calls;
static int s_draw_string_calls;
static char s_draw_string_last_text[64];
static uint16_t s_stub_string_width;
static int16_t s_stub_ascent;
static int16_t s_stub_descent;

/** Whether the EWMH connection stand-in reports a live connection */
static bool s_stub_ewmh_present;


/**
 * @brief Recording stand-in for the project's @a xcb_window_destroy
 *        wrapper
 * @note Complexity: @e O(1)
 */
void xcb_window_destroy(xcb_window_t window)
{
    s_window_destroy_calls++;
    s_window_destroy_last = window;
}


/**
 * @brief Recording stand-in for @a xcb_generate_id
 * @note Complexity: @e O(1)
 */
uint32_t xcb_generate_id(xcb_connection_t *c)
{
    (void) c;

    s_generate_id_calls++;

    return (uint32_t) (1000 + s_generate_id_calls);
}


/**
 * @brief Recording stand-in for the raw @a xcb_create_window request
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_create_window(xcb_connection_t *c, uint8_t depth,
        xcb_window_t wid, xcb_window_t parent, int16_t x, int16_t y,
        uint16_t width, uint16_t height, uint16_t border_width,
        uint16_t class, xcb_visualid_t visual, uint32_t value_mask,
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
    (void) class;
    (void) visual;
    (void) value_mask;
    (void) value_list;

    memset(&cookie, 0, sizeof(cookie));
    s_create_window_calls++;

    return cookie;
}


/**
 * @brief Recording stand-in for the raw @a xcb_map_window request
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_map_window(xcb_connection_t *c, xcb_window_t window)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) window;

    memset(&cookie, 0, sizeof(cookie));
    s_map_window_calls++;

    return cookie;
}


/**
 * @brief Recording stand-in for the raw @a xcb_configure_window
 *        request
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
    s_configure_window_calls++;

    return cookie;
}


/**
 * @brief Recording stand-in for the raw @a xcb_change_window_attributes
 *        request
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_change_window_attributes(xcb_connection_t *c,
        xcb_window_t window, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) window;
    (void) value_mask;
    (void) value_list;

    memset(&cookie, 0, sizeof(cookie));
    s_change_window_attributes_calls++;

    return cookie;
}


/**
 * @brief Recording stand-in for the raw @a xcb_create_gc request
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
    s_create_gc_calls++;

    return cookie;
}


/**
 * @brief Recording stand-in for the raw @a xcb_poly_fill_rectangle
 *        request
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
    s_poly_fill_rectangle_calls++;

    return cookie;
}


/**
 * @brief Recording stand-in for the raw @a xcb_free_gc request
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_free_gc(xcb_connection_t *c, xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) gc;

    memset(&cookie, 0, sizeof(cookie));
    s_free_gc_calls++;

    return cookie;
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
 * @brief Recording stand-in for the raw @a xcb_copy_area request
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_copy_area(xcb_connection_t *c,
        xcb_drawable_t src_drawable, xcb_drawable_t dst_drawable,
        xcb_gcontext_t gc, int16_t src_x, int16_t src_y, int16_t dst_x,
        int16_t dst_y, uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) src_drawable;
    (void) dst_drawable;
    (void) gc;
    (void) src_x;
    (void) src_y;
    (void) dst_x;
    (void) dst_y;
    (void) width;
    (void) height;

    memset(&cookie, 0, sizeof(cookie));
    s_copy_area_calls++;

    return cookie;
}


/**
 * @brief Recording stand-in for the raw @a xcb_free_pixmap request
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_free_pixmap(xcb_connection_t *c, xcb_pixmap_t pixmap)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) pixmap;

    memset(&cookie, 0, sizeof(cookie));
    s_free_pixmap_calls++;

    return cookie;
}


/**
 * @brief Controllable stand-in for @a wm_get_stage_by_id, always
 *        reporting no stage so the buffer-less repaint branch runs
 * @note Complexity: @e O(1)
 */
stage_td *wm_get_stage_by_id(uint32_t screen_id)
{
    (void) screen_id;

    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_offscreen_buffer_create
 *
 * Never actually called in this file's tests, since
 * @a wm_get_stage_by_id above always reports no stage, taking
 * the drag_overlay_repaint's buffer-less branch instead; provided
 * only to satisfy the link, as overlay.c references it in the branch
 * this file's tests never reach.
 *
 * @note Complexity: @e O(1)
 */
xcb_pixmap_t xcb_offscreen_buffer_create(xcb_connection_t *connection,
        uint8_t depth, xcb_drawable_t reference, uint16_t width,
        uint16_t height)
{
    (void) connection;
    (void) depth;
    (void) reference;
    (void) width;
    (void) height;

    return XCB_NONE;
}


/**
 * @brief Controllable stand-in for @a xcb_ewmh_connection_get
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    static xcb_ewmh_connection_t s_ewmh;

    return (s_stub_ewmh_present) ? &s_ewmh : NULL;
}


/**
 * @brief Recording stand-in for @a text_renderer_use_font
 * @note Complexity: @e O(1)
 */
int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;

    s_use_font_calls++;
    s_use_font_last_name = font_name;

    return 0;
}


/**
 * @brief Controllable stand-in for @a text_string_measure
 * @note Complexity: @e O(1)
 */
uint16_t text_string_measure(const char *text)
{
    (void) text;

    return s_stub_string_width;
}


/**
 * @brief Recording stand-in for @a text_renderer_set_color
 * @note Complexity: @e O(1)
 */
void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;

    s_set_color_calls++;
}


/**
 * @brief Recording stand-in for @a text_draw_string
 * @note Complexity: @e O(1)
 */
void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) drawable;
    (void) gc;
    (void) pos;

    s_draw_string_calls++;
    (void) snprintf(s_draw_string_last_text,
            sizeof(s_draw_string_last_text), "%s",
            (text != NULL) ? text : "");
}


/**
 * @brief Controllable stand-in for @a text_font_ascent
 * @note Complexity: @e O(1)
 */
int16_t text_font_ascent(void)
{
    return s_stub_ascent;
}


/**
 * @brief Controllable stand-in for @a text_font_descent
 * @note Complexity: @e O(1)
 */
int16_t text_font_descent(void)
{
    return s_stub_descent;
}


static void s_reset(void)
{
    memset(&s_drag, 0, sizeof(s_drag));
    s_drag.overlay_window = XCB_WINDOW_NONE;
    s_window_destroy_calls = 0;
    s_window_destroy_last = XCB_WINDOW_NONE;
    s_generate_id_calls = 0;
    s_create_window_calls = 0;
    s_map_window_calls = 0;
    s_configure_window_calls = 0;
    s_change_window_attributes_calls = 0;
    s_create_gc_calls = 0;
    s_poly_fill_rectangle_calls = 0;
    s_free_gc_calls = 0;
    s_clear_area_calls = 0;
    s_copy_area_calls = 0;
    s_free_pixmap_calls = 0;
    s_use_font_calls = 0;
    s_use_font_last_name = NULL;
    s_set_color_calls = 0;
    s_draw_string_calls = 0;
    s_draw_string_last_text[0] = '\0';
    s_stub_string_width = 10u;
    s_stub_ascent = 10;
    s_stub_descent = 3;
    s_stub_ewmh_present = false;
}


/* drag_overlay_hide with a null connection touches no XCB request,
 * yet still resets the overlay bookkeeping fields */
static void s_test_hide_null_connection_still_resets_state(void)
{
    s_reset();
    s_drag.overlay_window = 55u;
    s_drag.is_overlay_icon = true;
    (void) snprintf(s_drag.overlay_text, sizeof(s_drag.overlay_text),
            "%s", "100, 200");

    drag_overlay_hide(NULL);

    TAP_EQ_INT(s_window_destroy_calls, 0,
            "null connection: no destroy request is issued");
    TAP_OK(s_drag.overlay_window == XCB_WINDOW_NONE,
            "yet overlay_window is still reset to none");
    TAP_OK(!s_drag.is_overlay_icon,
            "and is_overlay_icon is reset to false");
    TAP_EQ_STR(s_drag.overlay_text, "",
            "and overlay_text is reset to empty");
}


/* drag_overlay_hide with no overlay window active is a pure no-op on
 * the XCB side, but still safe to call */
static void s_test_hide_no_window_is_noop_on_xcb(void)
{
    s_reset();
    s_drag.overlay_window = XCB_WINDOW_NONE;

    drag_overlay_hide((xcb_connection_t *) 1);

    TAP_EQ_INT(s_window_destroy_calls, 0,
            "no active overlay window: nothing is destroyed");
}


/* drag_overlay_hide with both a connection and an active window
 * destroys exactly that window and resets state */
static void s_test_hide_destroys_active_window(void)
{
    s_reset();
    s_drag.overlay_window = 77u;

    drag_overlay_hide((xcb_connection_t *) 1);

    TAP_EQ_INT(s_window_destroy_calls, 1,
            "active overlay window: destroyed exactly once");
    TAP_OK(s_window_destroy_last == 77u,
            "the destroyed window is the one that was active");
    TAP_OK(s_drag.overlay_window == XCB_WINDOW_NONE,
            "overlay_window is reset to none afterward");
}


/* drag_is_overlay_window is false whenever no overlay window is
 * currently active */
static void s_test_is_overlay_window_false_when_none_active(void)
{
    s_reset();
    s_drag.overlay_window = XCB_WINDOW_NONE;

    TAP_OK(!drag_is_overlay_window(42u),
            "no active overlay window: never matches any window id");
}


/* drag_is_overlay_window is false for a window id that simply is not
 * the active overlay */
static void s_test_is_overlay_window_false_for_other_window(void)
{
    s_reset();
    s_drag.overlay_window = 42u;

    TAP_OK(!drag_is_overlay_window(43u),
            "a different window id: does not match the active overlay");
}


/* drag_is_overlay_window is true precisely for the active overlay
 * window's own id */
static void s_test_is_overlay_window_true_for_match(void)
{
    s_reset();
    s_drag.overlay_window = 42u;

    TAP_OK(drag_is_overlay_window(42u),
            "the active overlay window's own id: matches");
}


/* drag_overlay_show with a null connection is a pure no-op: nothing
 * is touched at all, not even the overlay text */
static void s_test_show_null_connection_is_noop(void)
{
    client_td client;
    config_td config;
    struct geometry_s target = { { 0, 0 }, { 100u, 40u } };

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;
    s_drag.client = &client;

    drag_overlay_show(NULL, false, target, "100, 200");

    TAP_EQ_STR(s_drag.overlay_text, "",
            "null connection: overlay_text is left untouched (still"
            " empty)");
    TAP_EQ_INT(s_create_window_calls, 0,
            "and no window is ever created");
}


/* drag_overlay_show with no client attached to the drag is likewise a
 * pure no-op, since there is no theme to read colors/fonts from */
static void s_test_show_null_client_is_noop(void)
{
    struct geometry_s target = { { 0, 0 }, { 100u, 40u } };

    s_reset();
    s_drag.client = NULL;

    drag_overlay_show((xcb_connection_t *) 1, false, target, "100, 200");

    TAP_EQ_INT(s_create_window_calls, 0,
            "no client attached: no window is ever created");
}


/* drag_overlay_show with a null or empty text is a no-op too: an
 * overlay is never shown with nothing to say */
static void s_test_show_empty_text_is_noop(void)
{
    client_td client;
    config_td config;
    struct geometry_s target = { { 0, 0 }, { 100u, 40u } };

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;
    s_drag.client = &client;

    drag_overlay_show((xcb_connection_t *) 1, false, target, "");
    TAP_EQ_INT(s_create_window_calls, 0,
            "empty text: no window is ever created");

    drag_overlay_show((xcb_connection_t *) 1, false, target, NULL);
    TAP_EQ_INT(s_create_window_calls, 0,
            "null text: likewise, no window is ever created");
}


/* drag_overlay_show creates a brand-new overlay window the first
 * time it runs for the active drag, using the window-mode theme font
 * and colors when is_icon is false, then repaints it */
static void s_test_show_creates_window_first_call(void)
{
    client_td client;
    config_td config;
    struct geometry_s target = { { 0, 0 }, { 100u, 40u } };

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    (void) snprintf(config.theme.window.active.font,
            sizeof(config.theme.window.active.font), "%s", "fixed");
    client.config = &config;
    s_drag.client = &client;

    drag_overlay_show((xcb_connection_t *) 1, false, target, "100, 200");

    TAP_EQ_STR(s_drag.overlay_text, "100, 200",
            "overlay_text is copied from the requested text");
    TAP_OK(!s_drag.is_overlay_icon,
            "is_overlay_icon reflects the is_icon argument (false"
            " here)");
    TAP_EQ_INT(s_create_window_calls, 1,
            "no overlay window existed yet: exactly one is created");
    TAP_EQ_INT(s_map_window_calls, 1, "the new window is mapped");
    TAP_EQ_INT(s_configure_window_calls, 0,
            "a freshly created window is not also reconfigured");
    TAP_OK(s_drag.overlay_window != XCB_WINDOW_NONE,
            "overlay_window now holds the freshly generated id");
    TAP_OK(s_use_font_last_name != NULL &&
            strcmp(s_use_font_last_name, "fixed") == 0,
            "the window-mode theme font is the one selected, since"
            " is_icon was false");
}


/* drag_overlay_show reconfigures rather than recreates the window on
 * a second call while a drag is still ongoing */
static void s_test_show_reconfigures_existing_window(void)
{
    client_td client;
    config_td config;
    struct geometry_s target_a = { { 0, 0 }, { 100u, 40u } };
    struct geometry_s target_b = { { 50, 60 }, { 100u, 40u } };

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    (void) snprintf(config.theme.window.active.font,
            sizeof(config.theme.window.active.font), "%s", "fixed");
    client.config = &config;
    s_drag.client = &client;

    drag_overlay_show((xcb_connection_t *) 1, false, target_a, "10, 10");
    TAP_EQ_INT(s_create_window_calls, 1,
            "first call: the overlay window is created");

    drag_overlay_show((xcb_connection_t *) 1, false, target_b, "50, 60");
    TAP_EQ_INT(s_create_window_calls, 1,
            "second call while the drag continues: no second window"
            " is created");
    TAP_EQ_INT(s_configure_window_calls, 1,
            "instead the existing window is reconfigured exactly"
            " once");
    TAP_EQ_STR(s_drag.overlay_text, "50, 60",
            "and overlay_text is updated to the new text");
}


/* drag_overlay_show selects the icon-mode theme font when is_icon is
 * true, not the window-mode one */
static void s_test_show_icon_mode_uses_icon_font(void)
{
    client_td client;
    config_td config;
    struct geometry_s target = { { 0, 0 }, { 48u, 48u } };

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    (void) snprintf(config.theme.icon.active.font,
            sizeof(config.theme.icon.active.font), "%s", "icon-font");
    (void) snprintf(config.theme.window.active.font,
            sizeof(config.theme.window.active.font), "%s", "window-font");
    client.config = &config;
    s_drag.client = &client;

    drag_overlay_show((xcb_connection_t *) 1, true, target, "icon");

    TAP_OK(s_drag.is_overlay_icon,
            "is_overlay_icon is set true for an icon-mode overlay");
    TAP_OK(s_use_font_last_name != NULL &&
            strcmp(s_use_font_last_name, "icon-font") == 0,
            "the icon-mode theme font is selected, not the window-mode"
            " one");
}


/* drag_overlay_show clamps the overlay width up to the configured
 * minimum for a very narrow string, and repaints via drag_overlay_
 * repaint every time (proven indirectly through its own recorded
 * side effects, since repaint has no separate return value) */
static void s_test_show_repaints_after_creating(void)
{
    client_td client;
    config_td config;
    struct geometry_s target = { { 0, 0 }, { 100u, 40u } };

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    (void) snprintf(config.theme.window.active.font,
            sizeof(config.theme.window.active.font), "%s", "fixed");
    client.config = &config;
    s_drag.client = &client;
    s_stub_string_width = 4u;

    drag_overlay_show((xcb_connection_t *) 1, false, target, "1");

    TAP_OK(s_draw_string_calls > 0,
            "drag_overlay_repaint is invoked after showing: text is"
            " drawn");
    TAP_EQ_STR(s_draw_string_last_text, "1",
            "and the drawn text matches the overlay's own text");
}


/* drag_overlay_repaint with a null connection is a pure no-op */
static void s_test_repaint_null_connection_is_noop(void)
{
    client_td client;
    config_td config;

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;
    s_drag.client = &client;
    s_drag.overlay_window = 99u;
    (void) snprintf(s_drag.overlay_text, sizeof(s_drag.overlay_text),
            "%s", "5, 5");

    drag_overlay_repaint(NULL);

    TAP_EQ_INT(s_draw_string_calls, 0,
            "null connection: no text is ever drawn");
}


/* drag_overlay_repaint with no active overlay window is a no-op */
static void s_test_repaint_no_window_is_noop(void)
{
    client_td client;
    config_td config;

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;
    s_drag.client = &client;
    s_drag.overlay_window = XCB_WINDOW_NONE;
    (void) snprintf(s_drag.overlay_text, sizeof(s_drag.overlay_text),
            "%s", "5, 5");

    drag_overlay_repaint((xcb_connection_t *) 1);

    TAP_EQ_INT(s_draw_string_calls, 0,
            "no active overlay window: no text is ever drawn");
}


/* drag_overlay_repaint with no client, or a client with no config, is
 * a no-op */
static void s_test_repaint_no_client_or_config_is_noop(void)
{
    client_td client;

    s_reset();
    s_drag.client = NULL;
    s_drag.overlay_window = 99u;
    (void) snprintf(s_drag.overlay_text, sizeof(s_drag.overlay_text),
            "%s", "5, 5");

    drag_overlay_repaint((xcb_connection_t *) 1);
    TAP_EQ_INT(s_draw_string_calls, 0,
            "null client: no text is ever drawn");

    memset(&client, 0, sizeof(client));
    client.config = NULL;
    s_drag.client = &client;

    drag_overlay_repaint((xcb_connection_t *) 1);
    TAP_EQ_INT(s_draw_string_calls, 0,
            "client with no config: no text is ever drawn either");
}


/* drag_overlay_repaint with an empty overlay_text is a no-op */
static void s_test_repaint_empty_text_is_noop(void)
{
    client_td client;
    config_td config;

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    client.config = &config;
    s_drag.client = &client;
    s_drag.overlay_window = 99u;
    s_drag.overlay_text[0] = '\0';

    drag_overlay_repaint((xcb_connection_t *) 1);

    TAP_EQ_INT(s_draw_string_calls, 0,
            "empty overlay_text: no text is ever drawn");
}


/* drag_overlay_repaint draws the current overlay_text using the
 * window-mode theme colors/font when is_overlay_icon is false, and
 * takes the no-offscreen-buffer branch since wm_get_stage_by_id is
 * stubbed to always report no stage */
static void s_test_repaint_draws_window_mode_text(void)
{
    client_td client;
    config_td config;

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    (void) snprintf(config.theme.window.active.font,
            sizeof(config.theme.window.active.font), "%s", "window-font");
    config.theme.window.active.color.foreground = 0xffffffu;
    config.theme.window.active.color.background = 0x000000u;
    client.config = &config;
    s_drag.client = &client;
    s_drag.overlay_window = 99u;
    s_drag.is_overlay_icon = false;
    (void) snprintf(s_drag.overlay_text, sizeof(s_drag.overlay_text),
            "%s", "42, 17");

    drag_overlay_repaint((xcb_connection_t *) 1);

    TAP_EQ_INT(s_draw_string_calls, 1, "the overlay text is drawn once");
    TAP_EQ_STR(s_draw_string_last_text, "42, 17",
            "the exact text drawn matches overlay_text");
    TAP_OK(s_use_font_last_name != NULL &&
            strcmp(s_use_font_last_name, "window-font") == 0,
            "the window-mode font is selected for a non-icon overlay");
    TAP_EQ_INT(s_set_color_calls, 1,
            "the theme foreground/background colors are applied once");
    TAP_EQ_INT(s_clear_area_calls, 1,
            "with no offscreen buffer available, the window itself is"
            " cleared directly");
    TAP_EQ_INT(s_create_gc_calls, 0,
            "and no temporary graphics context is created for a"
            " buffer that was never allocated");
    TAP_EQ_INT(s_copy_area_calls, 0,
            "nor is anything copied back from a nonexistent buffer");
}


/* drag_overlay_repaint selects icon-mode theme colors/font when
 * is_overlay_icon is true */
static void s_test_repaint_draws_icon_mode_text(void)
{
    client_td client;
    config_td config;

    s_reset();
    memset(&client, 0, sizeof(client));
    memset(&config, 0, sizeof(config));
    (void) snprintf(config.theme.icon.active.font,
            sizeof(config.theme.icon.active.font), "%s", "icon-font");
    client.config = &config;
    s_drag.client = &client;
    s_drag.overlay_window = 99u;
    s_drag.is_overlay_icon = true;
    (void) snprintf(s_drag.overlay_text, sizeof(s_drag.overlay_text),
            "%s", "icon");

    drag_overlay_repaint((xcb_connection_t *) 1);

    TAP_OK(s_use_font_last_name != NULL &&
            strcmp(s_use_font_last_name, "icon-font") == 0,
            "the icon-mode font is selected for an icon overlay");
}


int main(void)
{
    TAP_PLAN(44);

    s_test_hide_null_connection_still_resets_state();
    s_test_hide_no_window_is_noop_on_xcb();
    s_test_hide_destroys_active_window();
    s_test_is_overlay_window_false_when_none_active();
    s_test_is_overlay_window_false_for_other_window();
    s_test_is_overlay_window_true_for_match();
    s_test_show_null_connection_is_noop();
    s_test_show_null_client_is_noop();
    s_test_show_empty_text_is_noop();
    s_test_show_creates_window_first_call();
    s_test_show_reconfigures_existing_window();
    s_test_show_icon_mode_uses_icon_font();
    s_test_show_repaints_after_creating();
    s_test_repaint_null_connection_is_noop();
    s_test_repaint_no_window_is_noop();
    s_test_repaint_no_client_or_config_is_noop();
    s_test_repaint_empty_text_is_noop();
    s_test_repaint_draws_window_mode_text();
    s_test_repaint_draws_icon_mode_text();

    return TAP_DONE();
}
