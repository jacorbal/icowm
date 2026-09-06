/**
 * @file tests/menu/notify/test_desktop.c
 *
 * @brief Test battery for the desktop-switch notification popup
 *
 * 'menu/notify/desktop.c' is a thin, single-static-instance wrapper
 * over the generic 'notify_popup_*' family already covered on its own
 * in 'tests/menu/test_notify.c'; both the generic 'menu/notify.c' and
 * 'menu/draw.c' are linked here for real, so this file's own
 * assertions can focus purely on desktop.c's own contribution: the
 * 'cfg->base.overlay' gate, delegating the label text to
 * 'surface_desktop_label', and forwarding through to the right
 * generic call with the right static state and timeout.
 * 'surface_desktop_label' itself belongs to a different module
 * ('surface/desktops.c') and is stubbed as a recording stand-in here,
 * the same as any other genuinely cross-module dependency; every raw
 * XCB entry point 'menu/notify.c' and 'menu/draw.c' call through to is
 * a link-only or recording stand-in, following the same pattern
 * 'tests/menu/test_notify.c' already established.  'LOGGER_TRACE'
 * is a macro over 'logger_msg', itself a silent, safe no-op while
 * uninitialized, so it needs no stand-in at all.
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
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>
#include <types/pair.h>

/* Default initial values */
#include <defs/desktop.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <menu/notify/desktop.h>


/** Fake, non-null XCB connection handle */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;

static uint32_t s_next_generated_id;

static int s_call_xcb_create_window;
static int s_call_xcb_map_window;
static int s_call_xcb_window_destroy;
static int s_call_surface_desktop_label;
static uint32_t s_last_label_desktop_id;
static char s_last_label_name[128];
static bool s_last_label_is_pinned;
static bool s_last_label_shows_name;

/** Text 'surface_desktop_label' writes into its out_label buffer */
static const char *s_label_reply = "Desktop 3";


xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    s_next_generated_id++;
    return s_next_generated_id;
}


xcb_void_cookie_t xcb_create_window(xcb_connection_t *connection,
        uint8_t depth, xcb_window_t wid, xcb_window_t parent,
        int16_t x, int16_t y, uint16_t width, uint16_t height,
        uint16_t border_width, uint16_t class,
        xcb_visualid_t visual, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
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
    s_call_xcb_create_window++;
    return cookie;
}


xcb_void_cookie_t xcb_map_window(xcb_connection_t *connection,
        xcb_window_t window)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) window;
    s_call_xcb_map_window++;
    return cookie;
}


void xcb_window_destroy(xcb_window_t window)
{
    (void) window;
    s_call_xcb_window_destroy++;
}


void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw_opacity)
{
    (void) connection;
    (void) window;
    (void) raw_opacity;
}


uint32_t config_theme_opacity_to_raw(uint8_t percent)
{
    return (uint32_t) percent;
}


xcb_void_cookie_t xcb_ewmh_set_wm_window_type(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t list_len, xcb_atom_t *list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) ewmh;
    (void) window;
    (void) list_len;
    (void) list;
    return cookie;
}


xcb_void_cookie_t xcb_create_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc, xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) gc;
    (void) drawable;
    (void) value_mask;
    (void) value_list;
    return cookie;
}


xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc, uint32_t rects_len,
        const xcb_rectangle_t *rects)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) drawable;
    (void) gc;
    (void) rects_len;
    (void) rects;
    return cookie;
}


xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) connection;
    (void) gc;
    return cookie;
}


void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) drawable;
    (void) gc;
    (void) pos;
    (void) text;
}


int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;
    return 0;
}


void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
}


uint16_t text_string_measure(const char *text)
{
    (void) text;
    return 40u;
}


/**
 * @brief Recording stand-in for @a surface_desktop_label
 *
 * The real formatting algorithm belongs to a different module
 * ('surface/desktops.c'), already exercised on its own elsewhere;
 * what this file checks is only that 'notify_desktop_show' calls
 * through to it with the right arguments and then forwards whatever
 * it wrote into the generic popup, via
 * 'tests/menu/test_notify.c''s own text-caching assertion mirrored
 * here on 'notify_desktop_show' instead.
 *
 * @note Complexity: @e O(1)
 */
void surface_desktop_label(const surface_td *surface, uint32_t desktop_id,
        const char *desktop_name, bool is_pinned, bool shows_name,
        char *out_label, size_t length)
{
    (void) surface;
    (void) desktop_name;
    s_call_surface_desktop_label++;
    s_last_label_desktop_id = desktop_id;
    s_last_label_is_pinned = is_pinned;
    s_last_label_shows_name = shows_name;
    (void) strncpy(s_last_label_name, desktop_name ? desktop_name : "",
            sizeof(s_last_label_name) - 1);
    s_last_label_name[sizeof(s_last_label_name) - 1] = '\0';
    (void) snprintf(out_label, length, "%s", s_label_reply);
}


/**
 * @brief Link-only stand-in for @a surface_desktop_get
 *
 * @note Complexity: @e O(1)
 */
desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    /* Only ever compared against NULL: 'notify_desktop_show' passes
     * whatever this returns straight to the viewport page lookup,
     * whose stand-in below ignores it, and 'desktop_td' is an
     * incomplete type here, so a placeholder address is all this can
     * and needs to hand back */
    static long placeholder;

    (void) surface;
    (void) desktop_id;

    return (desktop_td *) &placeholder;
}


/**
 * @brief Link-only stand-in for @a scmd_surface_viewport_desktop_page
 *
 * Reports whichever page a scenario last registered, or none at all,
 * so the four combinations of desktops and pages can each be
 * exercised without pulling in the whole of 'cmds/surface.c'.
 *
 * @note Complexity: @e O(1)
 */
static bool s_viewport_has_page;
static uint32_t s_viewport_col;
static uint32_t s_viewport_row;

bool scmd_surface_viewport_desktop_page(const surface_td *surface,
        const desktop_td *desktop, uint32_t *col_out, uint32_t *row_out)
{
    (void) surface;
    (void) desktop;

    if (!s_viewport_has_page) {
        return false;
    }
    *col_out = s_viewport_col;
    *row_out = s_viewport_row;
    return true;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 *
 * Every 'LOGGER_*' macro in the codebase, 'LOGGER_TRACE' included,
 * expands to a call through here; this file has nothing to assert
 * about logging, so it is a pure no-op only present to satisfy the
 * linker.
 *
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


static void s_reset(void)
{
    s_next_generated_id = 2000u;
    s_call_xcb_create_window = 0;
    s_viewport_has_page = false;
    s_viewport_col = 0u;
    s_viewport_row = 0u;
    s_call_xcb_map_window = 0;
    s_call_xcb_window_destroy = 0;
    s_call_surface_desktop_label = 0;
    s_last_label_desktop_id = 0u;
    memset(s_last_label_name, 0, sizeof(s_last_label_name));
    s_last_label_is_pinned = true;
    s_last_label_shows_name = false;
    s_label_reply = "Desktop 3";

    /* Force the shared static popup back to closed, regardless of
     * what an earlier scenario in this same process left it in;
     * there is no direct reset entry point, so a real close call
     * against whatever handle it may be holding is how this file
     * settles it, matching 'notify_desktop_close''s own null-window
     * guard when nothing was actually open */
    notify_desktop_close(s_fake_connection);
    s_call_xcb_window_destroy = 0;
}


static config_td s_make_config(bool show_overlay)
{
    config_td cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.theme.overlay.color.background = 0x111111u;
    cfg.theme.overlay.color.foreground = 0xeeeeeeu;
    cfg.theme.overlay.border.color = 0x222222u;
    cfg.theme.overlay.border.width = 1u;
    cfg.theme.overlay.opacity = 80u;
    cfg.base.overlay.on_desktop_switch = show_overlay;
    cfg.base.overlay.on_viewport_move = show_overlay;
    return cfg;
}


static surface_td s_make_surface(void)
{
    surface_td surface;
    static xcb_screen_t screen;

    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));
    surface.screen = &screen;
    /* More than one, or the overlay has no desktop worth naming and
     * declines to show anything at all */
    surface.desktop_count = 4u;
    surface.properties.dim.w = 1024u;
    surface.properties.dim.h = 768u;
    return surface;
}


/* notify_desktop_show does nothing at all, not even formatting a
 * label, on any null argument or a screen-less surface */
static void s_test_show_null_guards(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    surface = s_make_surface();
    cfg = s_make_config(true);

    notify_desktop_show(NULL, &surface, 2u, "Web", 
            NOTIFY_DESKTOP_CAUSE_SWITCH, &cfg);
    TAP_EQ_INT(s_call_surface_desktop_label, 0,
            "a null connection never even formats a label");

    notify_desktop_show(s_fake_connection, NULL, 2u, "Web", 
            NOTIFY_DESKTOP_CAUSE_SWITCH, &cfg);
    TAP_EQ_INT(s_call_surface_desktop_label, 0,
            "a null surface never formats a label");

    notify_desktop_show(s_fake_connection, &surface, 2u, "Web",
            NOTIFY_DESKTOP_CAUSE_SWITCH, NULL);
    TAP_EQ_INT(s_call_surface_desktop_label, 0,
            "a null config never formats a label");

    surface.screen = NULL;
    notify_desktop_show(s_fake_connection, &surface, 2u, "Web", 
            NOTIFY_DESKTOP_CAUSE_SWITCH, &cfg);
    TAP_EQ_INT(s_call_surface_desktop_label, 0,
            "a screen-less surface never formats a label");
}


/* The overlay is entirely suppressed, without formatting a label or
 * opening any window, when the active configuration turns it off */
static void s_test_show_respects_overlay_toggle(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    surface = s_make_surface();
    cfg = s_make_config(false);

    notify_desktop_show(s_fake_connection, &surface, 1u, "Mail", 
            NOTIFY_DESKTOP_CAUSE_SWITCH, &cfg);

    TAP_EQ_INT(s_call_surface_desktop_label, 0,
            "show_overlay=false skips formatting the label entirely");
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "show_overlay=false never opens a popup window");
    TAP_OK(!notify_desktop_is_open(),
            "the notification is correctly reported as not open");
}


/* A normal show call, with the overlay enabled, formats the label via
 * surface_desktop_label with the requested desktop id and name, then
 * forwards the formatted text into a real, mapped popup window */
static void s_test_show_formats_and_opens_popup(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    surface = s_make_surface();
    cfg = s_make_config(true);
    s_label_reply = "Desktop 4: Terminal";

    notify_desktop_show(s_fake_connection, &surface, 3u, "Terminal",
            
            NOTIFY_DESKTOP_CAUSE_SWITCH, &cfg);

    TAP_EQ_INT(s_call_surface_desktop_label, 1,
            "surface_desktop_label is called exactly once");
    TAP_EQ_INT((long) s_last_label_desktop_id, (long) 3u,
            "the requested desktop index is forwarded unchanged");
    TAP_EQ_STR(s_last_label_name, "Terminal",
            "the requested desktop name is forwarded unchanged");
    TAP_OK(!s_last_label_is_pinned,
            "the overlay always asks for an unpinned-style label");
    TAP_OK(!s_last_label_shows_name,
            "the overlay asks the label for the bracketed part only,"
            " prepending the desktop's own name itself");
    TAP_EQ_INT(s_call_xcb_create_window, 1,
            "exactly one popup window is created");
    TAP_EQ_INT(s_call_xcb_map_window, 1,
            "exactly one popup window is mapped");
    TAP_OK(notify_desktop_is_open(),
            "the notification now reports itself as open");
    TAP_OK(notify_desktop_window() != XCB_WINDOW_NONE,
            "notify_desktop_window answers a real window id");
}


/* A null desktop_name still reaches surface_desktop_label (which
 * itself is a different module's responsibility to null-guard), and
 * the popup still opens from whatever it wrote */
static void s_test_show_handles_null_name(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    surface = s_make_surface();
    cfg = s_make_config(true);
    s_label_reply = "Desktop 5";

    notify_desktop_show(s_fake_connection, &surface, 4u, NULL, 
            NOTIFY_DESKTOP_CAUSE_SWITCH, &cfg);

    TAP_EQ_INT(s_call_surface_desktop_label, 1,
            "a null desktop_name still reaches surface_desktop_label"
            " once");
    TAP_EQ_STR(s_last_label_name, "",
            "a null desktop_name is normalized to an empty string"
            " before being recorded");
    TAP_OK(notify_desktop_is_open(),
            "the popup still opens using whatever label text was"
            " produced");
}


/* notify_desktop_close destroys whatever window notify_desktop_show
 * left open, and afterward the notification reports itself closed */
static void s_test_close_closes_open_popup(void)
{
    surface_td surface;
    config_td cfg;

    s_reset();
    surface = s_make_surface();
    cfg = s_make_config(true);

    notify_desktop_show(s_fake_connection, &surface, 0u, "Desktop 1",
            
            NOTIFY_DESKTOP_CAUSE_SWITCH, &cfg);
    TAP_OK(notify_desktop_is_open(), "the popup opens as expected");

    notify_desktop_close(s_fake_connection);

    TAP_EQ_INT(s_call_xcb_window_destroy, 1,
            "closing destroys exactly one window");
    TAP_OK(!notify_desktop_is_open(),
            "the notification reports itself closed afterward");
    TAP_EQ_INT((long) notify_desktop_window(), (long) XCB_WINDOW_NONE,
            "notify_desktop_window answers XCB_WINDOW_NONE once"
            " closed");
}


/* notify_desktop_ms_remaining answers -1 while nothing is open, and a
 * positive value bounded by WM_DESKTOP_NOTIFY_TIMEOUT_MS right after
 * opening */
static void s_test_ms_remaining_reflects_state(void)
{
    surface_td surface;
    config_td cfg;
    int remaining;

    s_reset();
    surface = s_make_surface();
    cfg = s_make_config(true);

    TAP_EQ_INT(notify_desktop_ms_remaining(), -1,
            "with nothing open, remaining time is -1");

    notify_desktop_show(s_fake_connection, &surface, 0u, "Desktop 1",
            
            NOTIFY_DESKTOP_CAUSE_SWITCH, &cfg);
    remaining = notify_desktop_ms_remaining();
    TAP_OK(remaining > 0 && remaining <= WM_DESKTOP_NOTIFY_TIMEOUT_MS,
            "right after opening, remaining time is positive and"
            " bounded by the desktop notify timeout constant");

    notify_desktop_close(s_fake_connection);
}


/* notify_desktop_repaint on a closed notification never crashes and
 * never touches the drawing stand-ins; it is only meaningfully
 * exercised indirectly through the open popup's own paint on show,
 * already covered above */
static void s_test_repaint_closed_is_harmless(void)
{
    config_td cfg;

    s_reset();
    cfg = s_make_config(true);

    notify_desktop_repaint(s_fake_connection, &cfg);

    TAP_OK(1, "repainting a closed desktop notification never"
            " crashes");
}


/* The four rows of the rule: what there is to name decides what is
 * shown, and when there is nothing, no popup at all */
static void s_test_content_follows_what_exists(void)
{
    config_td cfg = s_make_config(true);
    surface_td surface = s_make_surface();

    /* One desktop and a viewport that cannot pan: nothing moved that
     * the user could not already see */
    s_reset();
    surface.desktop_count = 1u;
    notify_desktop_show(s_fake_connection, &surface, 0u, "Only",
            NOTIFY_DESKTOP_CAUSE_VIEWPORT, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "one desktop and a single-page viewport shows no popup"
            " at all");
    TAP_EQ_INT(s_call_surface_desktop_label, 0,
            "and never even asks for a label");

    /* One desktop but a viewport with pages: the page names itself,
     * and the desktop label is not asked for at all */
    s_reset();
    surface.desktop_count = 1u;
    s_viewport_has_page = true;
    s_viewport_col = 1u;
    notify_desktop_show(s_fake_connection, &surface, 0u, "Only",
            NOTIFY_DESKTOP_CAUSE_VIEWPORT, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 1,
            "one desktop with pages does show a popup");
    TAP_EQ_INT(s_call_surface_desktop_label, 0,
            "and names the page alone, never asking for a desktop"
            " label that would say nothing");

    /* Several desktops: the label is asked for, without the name,
     * which the overlay prepends itself */
    s_reset();
    surface.desktop_count = 4u;
    notify_desktop_show(s_fake_connection, &surface, 2u, "Web",
            NOTIFY_DESKTOP_CAUSE_SWITCH, &cfg);
    TAP_EQ_INT(s_call_surface_desktop_label, 1,
            "several desktops do ask for the bracketed label");
    TAP_OK(!s_last_label_shows_name,
            "and ask for it without the name, which goes in front");
}


/* Each trigger is gated by its own setting, so panning can be silent
 * while switching desktops is not */
static void s_test_causes_are_gated_separately(void)
{
    config_td cfg = s_make_config(true);
    surface_td surface = s_make_surface();

    cfg.base.overlay.on_viewport_move = false;

    s_reset();
    notify_desktop_show(s_fake_connection, &surface, 2u, "Web",
            NOTIFY_DESKTOP_CAUSE_VIEWPORT, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 0,
            "a viewport move is silent once its own setting is off");

    s_reset();
    notify_desktop_show(s_fake_connection, &surface, 2u, "Web",
            NOTIFY_DESKTOP_CAUSE_SWITCH, &cfg);
    TAP_EQ_INT(s_call_xcb_create_window, 1,
            "while a desktop switch still announces itself");
}


int main(void)
{
    TAP_PLAN(34);

    s_test_show_null_guards();
    s_test_show_respects_overlay_toggle();
    s_test_show_formats_and_opens_popup();
    s_test_show_handles_null_name();
    s_test_close_closes_open_popup();
    s_test_ms_remaining_reflects_state();
    s_test_repaint_closed_is_harmless();

    s_test_content_follows_what_exists();
    s_test_causes_are_gated_separately();

    return TAP_DONE();
}
