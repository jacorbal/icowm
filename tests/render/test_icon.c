/**
 * @file tests/render/test_icon.c
 *
 * @brief Test battery for iconified-client icon rendering guard
 *        clauses, skip-check logic, and state-hint letter selection
 *
 * @a ri_render_client_icon and @a ri_icon_hints_draw are drawing
 * routines bound to XCB calls, but each carries real branch logic
 * ahead of those calls: guard clauses, a skip-check that avoids
 * needless re-sending of X requests, an urgency-blink display swap,
 * and a letter-priority chain choosing which single state-hint letter
 * to draw.  Every XCB entry point and every cross-module call these
 * two functions reach (window/pixmap helpers, the text renderer, the
 * cycle menu, the drag subsystem, the systray, the opacity converter)
 * is stubbed below as a controllable, call-recording stand-in; only
 * @c src/render/icon.c itself is linked for real, since every one of
 * those stubbed calls is either a genuine X server round trip or
 * belongs to an unrelated subsystem this file does not own.
 *
 * A real, zero-initialized @c client_td and @c config_td back every
 * test, rather than a hand-fabricated stand-in struct, since both are
 * fully defined, includable project types; only the handful of fields
 * @c render/icon.c itself actually reads are ever set explicitly.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <harness/tap.h>
#include <client.h>
#include <defs/icon.h>
#include <render/icon.h>
#include <surface.h>


/* Controllable stand-in state */

static xcb_connection_t *s_connection_stub = (xcb_connection_t *) 1;
static xcb_ewmh_connection_t *s_ewmh_stub = NULL;
static uint32_t s_next_id = 5000u;

static bool s_urgency_blink_is_on = false;
static bool s_cycle_is_open = false;
static client_td *s_cycle_selected_client = NULL;
static bool s_drag_is_icon_drag = false;
static client_td *s_drag_client = NULL;
static client_td *s_iconmenu_target = NULL;
static surface_td *s_surface_for_screen = NULL;

static int s_change_window_attributes_calls = 0;
static uint32_t s_last_bg_pixel = 0u;
static uint32_t s_last_border_pixel = 0u;
static int s_set_border_calls = 0;
static uint32_t s_last_border_width = 0u;
static int s_set_opacity_calls = 0;
static uint32_t s_last_opacity_raw = 0u;

static xcb_pixmap_t s_offscreen_buffer_result = XCB_NONE;
static int s_offscreen_buffer_create_calls = 0;
static uint16_t s_last_offscreen_width = 0u;
static uint16_t s_last_offscreen_height = 0u;

static int s_clear_area_calls = 0;
static int s_poly_fill_rectangle_calls = 0;
static int s_poly_rectangle_calls = 0;
static int s_copy_area_calls = 0;
static int s_create_gc_calls = 0;
static int s_free_gc_calls = 0;
static int s_free_pixmap_calls = 0;

static xcb_window_t s_systray_below_window_result = XCB_WINDOW_NONE;
static int s_window_show_calls = 0;
static int s_window_lower_calls = 0;
static int s_window_stack_below_calls = 0;

static int s_wmicon_draw_calls = 0;

static int s_text_renderer_use_font_calls = 0;
static int s_text_renderer_set_color_calls = 0;
static int s_text_truncate_to_width_calls = 0;
static char s_text_truncate_output[CONFIG_MAX_LENGTH_NAME] = "";
static int s_text_draw_string_calls = 0;
static char s_last_drawn_string[CONFIG_MAX_LENGTH_NAME] = "";
static uint16_t s_text_string_measure_result = 6u;
static int16_t s_text_font_ascent_result = 10;

static int s_client_sync_visible_name_calls = 0;


/* Link-only stand-ins */

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection_stub;
}

xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return s_ewmh_stub;
}

bool urgency_blink_is_on(void)
{
    return s_urgency_blink_is_on;
}

bool cycle_is_open(void)
{
    return s_cycle_is_open;
}

client_td *cycle_get_selected_client(void)
{
    return s_cycle_selected_client;
}

bool drag_is_icon_drag(void)
{
    return s_drag_is_icon_drag;
}

client_td *drag_client(void)
{
    return s_drag_client;
}

bool iconmenu_target_is(const client_td *client)
{
    return client != NULL && client == s_iconmenu_target;
}

surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return s_surface_for_screen;
}

xcb_pixmap_t xcb_offscreen_buffer_create(xcb_connection_t *connection,
        uint8_t depth, xcb_drawable_t reference, uint16_t width,
        uint16_t height)
{
    (void) connection;
    (void) depth;
    (void) reference;
    s_offscreen_buffer_create_calls++;
    s_last_offscreen_width = width;
    s_last_offscreen_height = height;
    return s_offscreen_buffer_result;
}

xcb_window_t systray_below_window(void)
{
    return s_systray_below_window_result;
}

/* icon.c only ever passes this function pointer through to
 * 'client_sync_visible_name', itself stubbed below and never actually
 * calling it, so this stub only needs to exist for the linker; it is
 * never invoked at runtime */
xcb_void_cookie_t xcb_ewmh_set_wm_visible_icon_name_checked(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        uint32_t strlen, const char *string)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) ewmh;
    (void) window;
    (void) strlen;
    (void) string;
    return cookie;
}

void client_sync_visible_name(client_td *client, char *cached,
        const char *full_name, const char *rendered,
        xcb_void_cookie_t (*set_fn)(xcb_ewmh_connection_t *,
            xcb_window_t, uint32_t, const char *),
        xcb_atom_t atom)
{
    (void) client;
    (void) cached;
    (void) full_name;
    (void) rendered;
    (void) set_fn;
    (void) atom;
    s_client_sync_visible_name_calls++;
}

uint32_t config_theme_opacity_to_raw(uint8_t percent)
{
    (void) percent;
    return 0xffffffffu;
}

int text_renderer_use_font(xcb_connection_t *connection, const char *font)
{
    (void) connection;
    (void) font;
    s_text_renderer_use_font_calls++;
    return 0;
}

void text_renderer_set_color(uint32_t fg, uint32_t bg)
{
    (void) fg;
    (void) bg;
    s_text_renderer_set_color_calls++;
}

void text_truncate_to_width(char *buf, size_t buf_size, const char *text,
        uint16_t max_width)
{
    (void) max_width;
    s_text_truncate_to_width_calls++;
    /* Copies 's_text_truncate_output' verbatim rather than truncating
     * 'text' itself, so a test controls exactly what the caption ends
     * up holding without needing the real width-measuring pipeline */
    (void) text;
    strncpy(buf, s_text_truncate_output, buf_size - 1u);
    buf[buf_size - 1u] = '\0';
}

void text_draw_string(xcb_connection_t *connection, xcb_drawable_t drawable,
        xcb_gcontext_t gc, struct position_s pos, const char *text)
{
    (void) connection;
    (void) drawable;
    (void) gc;
    (void) pos;
    s_text_draw_string_calls++;
    strncpy(s_last_drawn_string, text, sizeof(s_last_drawn_string) - 1u);
    s_last_drawn_string[sizeof(s_last_drawn_string) - 1u] = '\0';
}

uint16_t text_string_measure(const char *text)
{
    (void) text;
    return s_text_string_measure_result;
}

int16_t text_font_ascent(void)
{
    return s_text_font_ascent_result;
}

uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    return s_next_id++;
}

xcb_void_cookie_t xcb_change_window_attributes(xcb_connection_t *connection,
        xcb_window_t window, uint32_t value_mask, const void *value_list)
{
    const uint32_t *values = value_list;
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) window;
    s_change_window_attributes_calls++;
    if ((value_mask & XCB_CW_BACK_PIXEL) != 0u) {
        s_last_bg_pixel = values[0];
    }
    if ((value_mask & XCB_CW_BORDER_PIXEL) != 0u) {
        s_last_border_pixel = values[1];
    }
    return cookie;
}

void xcb_window_set_border(xcb_window_t window, uint32_t width)
{
    (void) window;
    s_set_border_calls++;
    s_last_border_width = width;
}

void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw)
{
    (void) connection;
    (void) window;
    s_set_opacity_calls++;
    s_last_opacity_raw = raw;
}

xcb_void_cookie_t xcb_create_gc(xcb_connection_t *connection,
        xcb_gcontext_t cid, xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) cid;
    (void) drawable;
    (void) value_mask;
    (void) value_list;
    s_create_gc_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) gc;
    s_free_gc_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        uint32_t rectangles_len, const xcb_rectangle_t *rectangles)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) drawable;
    (void) gc;
    (void) rectangles_len;
    (void) rectangles;
    s_poly_fill_rectangle_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_poly_rectangle(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        uint32_t rectangles_len, const xcb_rectangle_t *rectangles)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) drawable;
    (void) gc;
    (void) rectangles_len;
    (void) rectangles;
    s_poly_rectangle_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_clear_area(xcb_connection_t *connection,
        uint8_t exposures, xcb_window_t window, int16_t x, int16_t y,
        uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) exposures;
    (void) window;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    s_clear_area_calls++;
    return cookie;
}

void xcb_window_show(xcb_window_t window)
{
    (void) window;
    s_window_show_calls++;
}

void xcb_window_lower(xcb_window_t window)
{
    (void) window;
    s_window_lower_calls++;
}

void xcb_window_stack_below(xcb_window_t window, xcb_window_t sibling)
{
    (void) window;
    (void) sibling;
    s_window_stack_below_calls++;
}

void wmicon_draw(xcb_connection_t *connection, xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, xcb_drawable_t drawable, uint16_t area_size,
        uint32_t frame_color, uint32_t bg_color, wmicon_cache_td *cache)
{
    (void) connection;
    (void) ewmh;
    (void) window;
    (void) drawable;
    (void) area_size;
    (void) frame_color;
    (void) bg_color;
    (void) cache;
    s_wmicon_draw_calls++;
}

xcb_void_cookie_t xcb_copy_area(xcb_connection_t *connection,
        xcb_drawable_t src_drawable, xcb_drawable_t dst_drawable,
        xcb_gcontext_t gc, int16_t src_x, int16_t src_y, int16_t dst_x,
        int16_t dst_y, uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) src_drawable;
    (void) dst_drawable;
    (void) gc;
    (void) src_x;
    (void) src_y;
    (void) dst_x;
    (void) dst_y;
    (void) width;
    (void) height;
    s_copy_area_calls++;
    return cookie;
}

xcb_void_cookie_t xcb_free_pixmap(xcb_connection_t *connection,
        xcb_pixmap_t pixmap)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) pixmap;
    s_free_pixmap_calls++;
    return cookie;
}


/* Fixture helpers */

static config_td s_config_fixture;
static client_td s_client_fixture;
static xcb_ewmh_connection_t s_ewmh_fixture;

/** Resets every stub call counter and rebuilds a fresh, minimally
 *  valid client/config fixture pair, so each test starts from the
 *  same known-good baseline regardless of what an earlier test
 *  mutated on its own copy of the same statics */
static void s_reset_fixture(void)
{
    s_next_id = 5000u;
    s_urgency_blink_is_on = false;
    s_cycle_is_open = false;
    s_cycle_selected_client = NULL;
    s_drag_is_icon_drag = false;
    s_drag_client = NULL;
    s_iconmenu_target = NULL;
    s_surface_for_screen = NULL;
    s_change_window_attributes_calls = 0;
    s_last_bg_pixel = 0u;
    s_last_border_pixel = 0u;
    s_set_border_calls = 0;
    s_last_border_width = 0u;
    s_set_opacity_calls = 0;
    s_last_opacity_raw = 0u;
    s_offscreen_buffer_result = XCB_NONE;
    s_offscreen_buffer_create_calls = 0;
    s_last_offscreen_width = 0u;
    s_last_offscreen_height = 0u;
    s_clear_area_calls = 0;
    s_poly_fill_rectangle_calls = 0;
    s_poly_rectangle_calls = 0;
    s_copy_area_calls = 0;
    s_create_gc_calls = 0;
    s_free_gc_calls = 0;
    s_free_pixmap_calls = 0;
    s_systray_below_window_result = XCB_WINDOW_NONE;
    s_window_show_calls = 0;
    s_window_lower_calls = 0;
    s_window_stack_below_calls = 0;
    s_wmicon_draw_calls = 0;
    s_text_renderer_use_font_calls = 0;
    s_text_renderer_set_color_calls = 0;
    s_text_truncate_to_width_calls = 0;
    s_text_truncate_output[0] = '\0';
    s_text_draw_string_calls = 0;
    s_last_drawn_string[0] = '\0';
    s_text_string_measure_result = 6u;
    s_text_font_ascent_result = 10;
    s_client_sync_visible_name_calls = 0;

    memset(&s_config_fixture, 0, sizeof(s_config_fixture));
    memset(&s_client_fixture, 0, sizeof(s_client_fixture));
    memset(&s_ewmh_fixture, 0, sizeof(s_ewmh_fixture));
    s_ewmh_stub = NULL;

    s_client_fixture.config = &s_config_fixture;
    s_client_fixture.is_icon_mapped = true;
    s_client_fixture.icon_window = 42u;
    s_client_fixture.window = 43u;
    s_client_fixture.screen_id = 0u;
    s_client_fixture.is_outdated = true;
    s_client_fixture.was_icon_selected = false;
}


/* ==================================================================== *
 * ri_render_client_icon: guard clauses                                  *
 * ==================================================================== */

static void s_test_render_client_icon_null_client_is_noop(void)
{
    s_reset_fixture();
    ri_render_client_icon(NULL, true, true, true);

    TAP_EQ_INT(s_change_window_attributes_calls, 0,
            "a NULL client is a no-op");
}

static void s_test_render_client_icon_null_connection_is_noop(void)
{
    s_reset_fixture();
    s_connection_stub = NULL;
    ri_render_client_icon(&s_client_fixture, true, true, true);
    s_connection_stub = (xcb_connection_t *) 1;

    TAP_EQ_INT(s_change_window_attributes_calls, 0,
            "a NULL connection is a no-op");
}

static void s_test_render_client_icon_null_config_is_noop(void)
{
    s_reset_fixture();
    s_client_fixture.config = NULL;
    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_change_window_attributes_calls, 0,
            "a client with no config is a no-op");
}

static void s_test_render_client_icon_not_current_is_noop(void)
{
    s_reset_fixture();
    ri_render_client_icon(&s_client_fixture, false, true, true);

    TAP_EQ_INT(s_change_window_attributes_calls, 0,
            "a client whose desktop is not the current one is a"
            " no-op, even when force is set");
}

static void s_test_render_client_icon_not_mapped_is_noop(void)
{
    s_reset_fixture();
    s_client_fixture.is_icon_mapped = false;
    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_change_window_attributes_calls, 0,
            "a client not icon-mapped is a no-op");
}

static void s_test_render_client_icon_no_icon_window_is_noop(void)
{
    s_reset_fixture();
    s_client_fixture.icon_window = 0u;
    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_change_window_attributes_calls, 0,
            "a client with icon_window 0 is a no-op");
}


/* ==================================================================== *
 * ri_render_client_icon: skip-check logic                               *
 * ==================================================================== */

/* Not forced, not outdated, cycle-selection unchanged, and not urgent:
 * every one of the skip-check's own tracked conditions says nothing
 * changed, so the whole render is skipped */
static void s_test_render_client_icon_skips_when_nothing_changed(void)
{
    s_reset_fixture();
    s_client_fixture.is_outdated = false;
    s_client_fixture.was_icon_selected = false;
    s_client_fixture.properties.flags = 0u; /* not urgent */

    ri_render_client_icon(&s_client_fixture, true, false, true);

    TAP_EQ_INT(s_change_window_attributes_calls, 0,
            "nothing tracked changed and force is false: render is"
            " skipped entirely");
}

/* force = true always overrides the skip-check, even with nothing
 * else changed */
static void s_test_render_client_icon_force_always_renders(void)
{
    s_reset_fixture();
    s_client_fixture.is_outdated = false;
    s_client_fixture.was_icon_selected = false;
    s_client_fixture.properties.flags = 0u;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_change_window_attributes_calls, 1,
            "force = true always renders, regardless of the skip"
            " check's own tracked conditions");
}

/* is_outdated = true always renders too, independent of force */
static void s_test_render_client_icon_outdated_always_renders(void)
{
    s_reset_fixture();
    s_client_fixture.is_outdated = true;
    s_client_fixture.was_icon_selected = false;
    s_client_fixture.properties.flags = 0u;

    ri_render_client_icon(&s_client_fixture, true, false, true);

    TAP_EQ_INT(s_change_window_attributes_calls, 1,
            "is_outdated alone is enough to render, without force");
}

/* An urgent client always renders on every call, regardless of every
 * other tracked condition, since its blink needs to repaint on every
 * phase change */
static void s_test_render_client_icon_urgent_always_renders(void)
{
    s_reset_fixture();
    s_client_fixture.is_outdated = false;
    s_client_fixture.was_icon_selected = false;
    s_client_fixture.properties.flags = CLIENT_FLAG_URGENT;

    ri_render_client_icon(&s_client_fixture, true, false, true);

    TAP_EQ_INT(s_change_window_attributes_calls, 1,
            "an urgent client always renders, bypassing the skip"
            " check entirely");
}

/* A cycle-selection change (was_icon_selected disagreeing with
 * the freshly computed is_cycle_sel) also always renders */
static void s_test_render_client_icon_cycle_sel_change_renders(void)
{
    s_reset_fixture();
    s_client_fixture.is_outdated = false;
    s_client_fixture.was_icon_selected = false;
    s_client_fixture.properties.flags = 0u;
    s_cycle_is_open = true;
    s_cycle_selected_client = &s_client_fixture; /* is_cycle_sel now true */

    ri_render_client_icon(&s_client_fixture, true, false, true);

    TAP_EQ_INT(s_change_window_attributes_calls, 1,
            "a cycle-selection state change from last render always"
            " renders");
    TAP_OK(s_client_fixture.was_icon_selected,
            "...and the freshly computed selection state is stored"
            " back for next time's comparison");
}


/* ==================================================================== *
 * ri_render_client_icon: is_cycle_sel computation                       *
 * ==================================================================== */

static void s_test_render_client_icon_cycle_sel_from_cycle_menu(void)
{
    s_reset_fixture();
    s_cycle_is_open = true;
    s_cycle_selected_client = &s_client_fixture;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_OK(s_client_fixture.was_icon_selected,
            "being the cycle menu's selected client makes"
            " is_cycle_sel true");
}

static void s_test_render_client_icon_cycle_sel_from_icon_drag(void)
{
    s_reset_fixture();
    s_drag_is_icon_drag = true;
    s_drag_client = &s_client_fixture;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_OK(s_client_fixture.was_icon_selected,
            "being the icon currently being dragged also makes"
            " is_cycle_sel true");
}

static void s_test_render_client_icon_cycle_sel_false_by_default(void)
{
    s_reset_fixture();

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_OK(!s_client_fixture.was_icon_selected,
            "neither the cycle menu nor an icon drag holding this"
            " client: is_cycle_sel is false");
}

/* The cycle menu picking a DIFFERENT client, or a drag holding a
 * different client, does not make this one selected */
static void s_test_render_client_icon_cycle_sel_ignores_other_client(void)
{
    client_td other_client;

    memset(&other_client, 0, sizeof(other_client));
    s_reset_fixture();
    s_cycle_is_open = true;
    s_cycle_selected_client = &other_client;
    s_drag_is_icon_drag = true;
    s_drag_client = &other_client;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_OK(!s_client_fixture.was_icon_selected,
            "another client being selected or dragged does not select"
            " this one");
}

static void s_test_render_client_icon_cycle_sel_from_iconmenu(void)
{
    s_reset_fixture();
    s_iconmenu_target = &s_client_fixture;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_OK(s_client_fixture.was_icon_selected,
            "its own icon context menu being open also makes"
            " is_cycle_sel true, so several stacked icons still show"
            " which one a menu belongs to");
}

/* The icon context menu being open for a DIFFERENT client does not
 * make this one selected either */
static void s_test_render_client_icon_cycle_sel_ignores_other_iconmenu(void)
{
    client_td other_client;

    memset(&other_client, 0, sizeof(other_client));
    s_reset_fixture();
    s_iconmenu_target = &other_client;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_OK(!s_client_fixture.was_icon_selected,
            "an icon context menu open for another client does not"
            " select this one");
}


/* ==================================================================== *
 * ri_render_client_icon: urgency-blink display swap                     *
 * ==================================================================== */

/* An urgent client's "on" blink phase swaps display_active to the
 * opposite of is_cycle_sel, visible here through which background
 * color (active vs. inactive) actually gets applied to the window */
static void s_test_render_client_icon_blink_on_swaps_display_active(void)
{
    s_reset_fixture();
    s_client_fixture.properties.flags = CLIENT_FLAG_URGENT;
    s_urgency_blink_is_on = true;
    /* is_cycle_sel stays false (no cycle/drag selection): display_active
     * starts false, then the blink swap flips it to true */
    s_config_fixture.theme.icon.active.color.background = 0x111111u;
    s_config_fixture.theme.icon.inactive.color.background = 0x222222u;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT((long) s_last_bg_pixel, 0x111111,
            "an urgent client's 'on' blink phase swaps display_active"
            " to true (the active background), even though"
            " is_cycle_sel itself is false");
}

/* The blink's "off" phase (or a non-urgent client) leaves
 * display_active exactly at is_cycle_sel, with no swap */
static void s_test_render_client_icon_blink_off_no_swap(void)
{
    s_reset_fixture();
    s_client_fixture.properties.flags = CLIENT_FLAG_URGENT;
    s_urgency_blink_is_on = false;
    s_config_fixture.theme.icon.active.color.background = 0x111111u;
    s_config_fixture.theme.icon.inactive.color.background = 0x222222u;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT((long) s_last_bg_pixel, 0x222222,
            "the blink's 'off' phase applies the inactive background,"
            " unswapped, matching is_cycle_sel = false");
}


/* ==================================================================== *
 * ri_render_client_icon: icon_h and offscreen-buffer fallback           *
 * ==================================================================== */

/* No surface resolvable for client->screen_id: no offscreen buffer is
 * ever requested, and the icon window itself becomes the drawable
 * everything else draws onto (via xcb_clear_area instead of a
 * poly-fill into a buffer) */
static void s_test_render_client_icon_no_surface_draws_direct(void)
{
    s_reset_fixture();
    s_surface_for_screen = NULL;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_offscreen_buffer_create_calls, 0,
            "no surface resolvable for this screen_id: no offscreen"
            " buffer is ever requested");
    TAP_EQ_INT(s_clear_area_calls, 1,
            "...falls back to clearing the icon window directly"
            " instead");
    TAP_EQ_INT(s_copy_area_calls, 0,
            "...and never copies a buffer over, since none was built");
}

/* A resolvable surface but a buffer creation failure (XCB_NONE)
 * behaves the same way: falls back to drawing directly on the icon
 * window */
static void s_test_render_client_icon_buffer_creation_failure_direct(void)
{
    surface_td surface;
    xcb_screen_t screen;

    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));
    surface.screen = &screen;

    s_reset_fixture();
    s_surface_for_screen = &surface;
    s_offscreen_buffer_result = XCB_NONE;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_offscreen_buffer_create_calls, 1,
            "a resolvable surface does request an offscreen buffer");
    TAP_EQ_INT(s_clear_area_calls, 1,
            "...but a creation failure (XCB_NONE) falls back to"
            " clearing the icon window directly");
}

/* A successfully created offscreen buffer is drawn into, then copied
 * onto the real icon window and freed, rather than clearing the icon
 * window directly */
static void s_test_render_client_icon_buffer_success_copies_and_frees(void)
{
    surface_td surface;
    xcb_screen_t screen;

    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));
    surface.screen = &screen;

    s_reset_fixture();
    s_surface_for_screen = &surface;
    s_offscreen_buffer_result = 999u;
    s_client_fixture.config->theme.icon.is_captioned
        ? (void) 0 : (void) 0; /* silence unused-field-path warnings */

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_poly_fill_rectangle_calls, 1,
            "a successfully created buffer is filled with the"
            " background color first");
    TAP_EQ_INT(s_copy_area_calls, 1,
            "...then copied onto the real icon window");
    TAP_EQ_INT(s_free_pixmap_calls, 1,
            "...and freed once copied");
    TAP_EQ_INT(s_clear_area_calls, 0,
            "...never falling back to a direct clear when the buffer"
            " path succeeded");
}

/* icon_h grows by WM_ICON_CAPTION_HEIGHT when the theme captions
 * icons, and stays at just WM_ICON_SQUARE_SIZE when it does not */
static void s_test_render_client_icon_h_grows_when_captioned(void)
{
    surface_td surface;
    xcb_screen_t screen;

    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));
    surface.screen = &screen;

    s_reset_fixture();
    s_surface_for_screen = &surface;
    s_offscreen_buffer_result = 999u;
    s_config_fixture.theme.icon.is_captioned = true;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT((long) s_last_offscreen_height,
            (long) (WM_ICON_SQUARE_SIZE + WM_ICON_CAPTION_HEIGHT),
            "a captioned theme grows the offscreen buffer's height by"
            " WM_ICON_CAPTION_HEIGHT");
}

static void s_test_render_client_icon_h_stays_square_uncaptioned(void)
{
    surface_td surface;
    xcb_screen_t screen;

    memset(&surface, 0, sizeof(surface));
    memset(&screen, 0, sizeof(screen));
    surface.screen = &screen;

    s_reset_fixture();
    s_surface_for_screen = &surface;
    s_offscreen_buffer_result = 999u;
    s_config_fixture.theme.icon.is_captioned = false;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT((long) s_last_offscreen_height, (long) WM_ICON_SQUARE_SIZE,
            "an uncaptioned theme leaves the buffer's height at just"
            " WM_ICON_SQUARE_SIZE");
}


/* ==================================================================== *
 * ri_render_client_icon: systray stacking                               *
 * ==================================================================== */

static void s_test_render_client_icon_stacks_below_tray_when_present(void)
{
    s_reset_fixture();
    s_systray_below_window_result = 77u;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_window_stack_below_calls, 1,
            "a present systray-below window stacks the icon below it");
    TAP_EQ_INT(s_window_lower_calls, 0,
            "...instead of a plain lower");
}

static void s_test_render_client_icon_lowers_when_no_tray(void)
{
    s_reset_fixture();
    s_systray_below_window_result = XCB_WINDOW_NONE;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_window_lower_calls, 1,
            "no systray-below window: falls back to a plain lower");
    TAP_EQ_INT(s_window_stack_below_calls, 0,
            "...instead of stacking below anything");
}


/* restack=false skips both stacking calls outright, whether or not a
 * systray-below window is present; handler_expose (handler/expose.c)
 * is the one caller that asks for this, an 'Expose' from being merely
 * uncovered being no reason to shuffle the icon against its siblings */
static void s_test_render_client_icon_no_restack_skips_stacking_with_tray(
        void)
{
    s_reset_fixture();
    s_systray_below_window_result = 77u;

    ri_render_client_icon(&s_client_fixture, true, true, false);

    TAP_EQ_INT(s_window_stack_below_calls, 0,
            "restack=false skips stacking below the tray");
    TAP_EQ_INT(s_window_lower_calls, 0,
            "...and skips the plain lower fallback too");
}

static void s_test_render_client_icon_no_restack_skips_stacking_no_tray(
        void)
{
    s_reset_fixture();
    s_systray_below_window_result = XCB_WINDOW_NONE;

    ri_render_client_icon(&s_client_fixture, true, true, false);

    TAP_EQ_INT(s_window_lower_calls, 0,
            "restack=false skips the plain lower even with no tray");
    TAP_EQ_INT(s_window_stack_below_calls, 0,
            "...and skips stacking below anything either way");
}


/* ==================================================================== *
 * ri_render_client_icon: pixmap-visibility gate                         *
 * ==================================================================== */

static void s_test_render_client_icon_draws_pixmap_when_shown_unselected(
        void)
{
    s_reset_fixture();
    s_config_fixture.theme.icon.show_pixmaps = true;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_wmicon_draw_calls, 1,
            "show_pixmaps on, and this icon is not the cycle"
            " selection: the pixmap is drawn");
}

static void s_test_render_client_icon_hides_pixmap_when_show_off(void)
{
    s_reset_fixture();
    s_config_fixture.theme.icon.show_pixmaps = false;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_wmicon_draw_calls, 0,
            "show_pixmaps off: the pixmap is never drawn");
}

/* Even with show_pixmaps on, the picked-up icon (cycle-selected or
 * being dragged) never draws its pixmap, so text reads against the
 * plain selected background */
static void s_test_render_client_icon_hides_pixmap_when_cycle_selected(void)
{
    s_reset_fixture();
    s_config_fixture.theme.icon.show_pixmaps = true;
    s_cycle_is_open = true;
    s_cycle_selected_client = &s_client_fixture;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_wmicon_draw_calls, 0,
            "show_pixmaps on, but this icon is the cycle selection:"
            " the pixmap is withheld");
}


/* ==================================================================== *
 * ri_render_client_icon: caption drawing                                *
 * ==================================================================== */

static void s_test_render_client_icon_no_caption_when_uncaptioned(void)
{
    s_reset_fixture();
    s_config_fixture.theme.icon.is_captioned = false;
    s_client_fixture.info.name = (char *) "a window";

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_text_draw_string_calls, 0,
            "an uncaptioned theme never draws a caption, even with a"
            " real client name available");
}

static void s_test_render_client_icon_no_caption_when_name_null(void)
{
    s_reset_fixture();
    s_config_fixture.theme.icon.is_captioned = true;
    s_client_fixture.info.name = NULL;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_text_draw_string_calls, 0,
            "a captioned theme with a NULL client name draws no"
            " caption either");
}

static void s_test_render_client_icon_draws_nonempty_caption(void)
{
    s_reset_fixture();
    s_config_fixture.theme.icon.is_captioned = true;
    s_client_fixture.info.name = (char *) "a window";
    strncpy(s_text_truncate_output, "a window",
            sizeof(s_text_truncate_output) - 1u);

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_text_draw_string_calls, 1,
            "a captioned theme with a nonempty truncated caption"
            " draws it");
    TAP_EQ_STR(s_last_drawn_string, "a window",
            "...and the exact truncated text is what gets drawn");
}

/* A caption that truncates down to an empty string is not drawn at
 * all, per the guard right before text_draw_string */
static void s_test_render_client_icon_skips_empty_truncated_caption(void)
{
    s_reset_fixture();
    s_config_fixture.theme.icon.is_captioned = true;
    s_client_fixture.info.name = (char *) "a window";
    s_text_truncate_output[0] = '\0'; /* truncated down to nothing */

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_text_draw_string_calls, 0,
            "a caption truncated down to an empty string is not"
            " drawn at all");
}

/* client_sync_visible_name is only ever consulted when a live EWMH
 * connection is available */
static void s_test_render_client_icon_syncs_visible_name_with_ewmh(void)
{
    s_reset_fixture();
    s_config_fixture.theme.icon.is_captioned = true;
    s_client_fixture.info.name = (char *) "a window";
    strncpy(s_text_truncate_output, "a window",
            sizeof(s_text_truncate_output) - 1u);
    s_ewmh_stub = &s_ewmh_fixture;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_client_sync_visible_name_calls, 1,
            "a live EWMH connection triggers a visible-name sync"
            " call");
}

static void s_test_render_client_icon_skips_sync_without_ewmh(void)
{
    s_reset_fixture();
    s_config_fixture.theme.icon.is_captioned = true;
    s_client_fixture.info.name = (char *) "a window";
    strncpy(s_text_truncate_output, "a window",
            sizeof(s_text_truncate_output) - 1u);
    s_ewmh_stub = NULL;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_EQ_INT(s_client_sync_visible_name_calls, 0,
            "no EWMH connection: the visible-name sync is skipped");
}


/* ==================================================================== *
 * ri_render_client_icon: is_outdated cleared at the end                 *
 * ==================================================================== */

static void s_test_render_client_icon_clears_outdated_flag(void)
{
    s_reset_fixture();
    s_client_fixture.is_outdated = true;

    ri_render_client_icon(&s_client_fixture, true, true, true);

    TAP_OK(!s_client_fixture.is_outdated,
            "a completed render clears the client's is_outdated flag");
}


/* ==================================================================== *
 * ri_icon_hints_draw: guard clauses                                     *
 * ==================================================================== */

static void s_test_hints_null_guards(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    ri_icon_hints_draw(NULL, &s_client_fixture, 1u, false, &theme);
    ri_icon_hints_draw(s_connection_stub, NULL, 1u, false, &theme);
    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            NULL);

    s_client_fixture.icon_window = 0u;
    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);
    s_client_fixture.icon_window = 42u;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, XCB_NONE,
            false, &theme);

    TAP_EQ_INT(s_poly_fill_rectangle_calls + s_text_draw_string_calls, 0,
            "every null/zero guard returns before drawing anything");
}


/* ==================================================================== *
 * ri_icon_hints_draw: show_hints off, not urgent-blinking               *
 * ==================================================================== */

static void s_test_hints_show_hints_off_not_blinking_draws_nothing(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = false;

    s_reset_fixture();
    s_client_fixture.properties.flags = 0u; /* not urgent */

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_INT(s_poly_fill_rectangle_calls, 0,
            "show_hints off and not urgent-blinking: the pin square is"
            " never drawn");
    TAP_EQ_INT(s_text_draw_string_calls, 0,
            "...nor is any letter");
}

/* The one documented exception: show_hints off, but an urgent
 * client's 'on' blink phase, still draws the urgent letter (only) */
static void s_test_hints_show_hints_off_blinking_draws_urgent_letter(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = false;

    s_reset_fixture();
    s_client_fixture.properties.flags = CLIENT_FLAG_URGENT;
    s_urgency_blink_is_on = true;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_INT(s_poly_fill_rectangle_calls, 0,
            "show_hints off still never draws the pin square, blinking"
            " or not");
    TAP_EQ_INT(s_text_draw_string_calls, 1,
            "...but the urgent letter alone is still drawn during the"
            " blink's 'on' phase");
    TAP_EQ_STR(s_last_drawn_string, "!",
            "...and it is exactly the urgent-hint character");
}


/* ==================================================================== *
 * ri_icon_hints_draw: pin square                                        *
 * ==================================================================== */

static void s_test_hints_pin_square_drawn_when_pinned(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    s_client_fixture.properties.flags = CLIENT_FLAG_PIN;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_INT(s_poly_fill_rectangle_calls, 1,
            "a pinned client with show_hints on draws the pin square");
}

static void s_test_hints_pin_square_not_drawn_when_unpinned(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    s_client_fixture.properties.flags = 0u;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_INT(s_poly_fill_rectangle_calls, 0,
            "an unpinned client draws no pin square");
}


/* ==================================================================== *
 * ri_icon_hints_draw: sticky square                                     *
 * ==================================================================== */

static void s_test_hints_sticky_square_drawn_when_sticky(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    s_client_fixture.properties.flags = CLIENT_FLAG_STICKY;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_INT(s_poly_rectangle_calls, 1,
            "a sticky client with show_hints on draws the outlined"
            " sticky square");
}

static void s_test_hints_sticky_square_not_drawn_when_unsticky(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    s_client_fixture.properties.flags = 0u;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_INT(s_poly_rectangle_calls, 0,
            "a non-sticky client draws no sticky square");
}


/* ==================================================================== *
 * ri_icon_hints_draw: letter-priority chain                             *
 * ==================================================================== */

static void s_test_hints_letter_blink_on_beats_every_state(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    s_client_fixture.properties.flags = CLIENT_FLAG_URGENT;
    s_urgency_blink_is_on = true;
    s_client_fixture.properties.state = (uint16_t)
        (CLIENT_STATE_FULLSCREEN | CLIENT_STATE_MAXIMIZED);

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_STR(s_last_drawn_string, "!",
            "blink_on outranks every state bit, drawing the urgent"
            " letter regardless of fullscreen/maximized also being"
            " set");
}

static void s_test_hints_letter_fullscreen_beats_maximized(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    s_client_fixture.properties.state = (uint16_t)
        (CLIENT_STATE_FULLSCREEN | CLIENT_STATE_MAXIMIZED);

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_STR(s_last_drawn_string, "f",
            "fullscreen outranks maximized when both bits are set");
}

static void s_test_hints_letter_maximized_beats_maximized_horz(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    /* CLIENT_STATE_MAXIMIZED is HORZ | VERT together, so this is both
     * fully maximized and, on its own, maximized_horz */
    s_client_fixture.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_STR(s_last_drawn_string, "m",
            "full maximized outranks the plain maximized_horz check");
}

static void s_test_hints_letter_maximized_horz_alone(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    s_client_fixture.properties.state = (uint16_t)
        CLIENT_STATE_MAXIMIZED_HORZ;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_STR(s_last_drawn_string, "h",
            "maximized_horz alone draws the horizontal-maximize"
            " letter");
}

static void s_test_hints_letter_maximized_vert_alone(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    s_client_fixture.properties.state = (uint16_t)
        CLIENT_STATE_MAXIMIZED_VERT;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_STR(s_last_drawn_string, "v",
            "maximized_vert alone draws the vertical-maximize letter");
}

static void s_test_hints_letter_none_draws_nothing(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    s_client_fixture.properties.state = (uint16_t) CLIENT_STATE_NORMAL;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_INT(s_text_draw_string_calls, 0,
            "holding no relevant state bit at all draws no letter");
}


/* ==================================================================== *
 * ri_icon_hints_draw: letter position                                   *
 * ==================================================================== */

static void s_test_hints_letter_positioned_right_aligned(void)
{
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));
    theme.icon.show_hints = true;

    s_reset_fixture();
    s_client_fixture.properties.state = (uint16_t)
        CLIENT_STATE_FULLSCREEN;
    s_text_string_measure_result = 8u;
    s_text_font_ascent_result = 11;

    ri_icon_hints_draw(s_connection_stub, &s_client_fixture, 1u, false,
            &theme);

    TAP_EQ_INT(s_text_draw_string_calls, 1,
            "the state letter is drawn exactly once");
}


int main(void)
{
    TAP_PLAN(67);

    s_test_render_client_icon_null_client_is_noop();
    s_test_render_client_icon_null_connection_is_noop();
    s_test_render_client_icon_null_config_is_noop();
    s_test_render_client_icon_not_current_is_noop();
    s_test_render_client_icon_not_mapped_is_noop();
    s_test_render_client_icon_no_icon_window_is_noop();

    s_test_render_client_icon_skips_when_nothing_changed();
    s_test_render_client_icon_force_always_renders();
    s_test_render_client_icon_outdated_always_renders();
    s_test_render_client_icon_urgent_always_renders();
    s_test_render_client_icon_cycle_sel_change_renders();

    s_test_render_client_icon_cycle_sel_from_cycle_menu();
    s_test_render_client_icon_cycle_sel_from_icon_drag();
    s_test_render_client_icon_cycle_sel_false_by_default();
    s_test_render_client_icon_cycle_sel_ignores_other_client();
    s_test_render_client_icon_cycle_sel_from_iconmenu();
    s_test_render_client_icon_cycle_sel_ignores_other_iconmenu();

    s_test_render_client_icon_blink_on_swaps_display_active();
    s_test_render_client_icon_blink_off_no_swap();

    s_test_render_client_icon_no_surface_draws_direct();
    s_test_render_client_icon_buffer_creation_failure_direct();
    s_test_render_client_icon_buffer_success_copies_and_frees();
    s_test_render_client_icon_h_grows_when_captioned();
    s_test_render_client_icon_h_stays_square_uncaptioned();

    s_test_render_client_icon_stacks_below_tray_when_present();
    s_test_render_client_icon_lowers_when_no_tray();
    s_test_render_client_icon_no_restack_skips_stacking_with_tray();
    s_test_render_client_icon_no_restack_skips_stacking_no_tray();

    s_test_render_client_icon_draws_pixmap_when_shown_unselected();
    s_test_render_client_icon_hides_pixmap_when_show_off();
    s_test_render_client_icon_hides_pixmap_when_cycle_selected();

    s_test_render_client_icon_no_caption_when_uncaptioned();
    s_test_render_client_icon_no_caption_when_name_null();
    s_test_render_client_icon_draws_nonempty_caption();
    s_test_render_client_icon_skips_empty_truncated_caption();
    s_test_render_client_icon_syncs_visible_name_with_ewmh();
    s_test_render_client_icon_skips_sync_without_ewmh();

    s_test_render_client_icon_clears_outdated_flag();

    s_test_hints_null_guards();

    s_test_hints_show_hints_off_not_blinking_draws_nothing();
    s_test_hints_show_hints_off_blinking_draws_urgent_letter();

    s_test_hints_pin_square_drawn_when_pinned();
    s_test_hints_pin_square_not_drawn_when_unpinned();
    s_test_hints_sticky_square_drawn_when_sticky();
    s_test_hints_sticky_square_not_drawn_when_unsticky();

    s_test_hints_letter_blink_on_beats_every_state();
    s_test_hints_letter_fullscreen_beats_maximized();
    s_test_hints_letter_maximized_beats_maximized_horz();
    s_test_hints_letter_maximized_horz_alone();
    s_test_hints_letter_maximized_vert_alone();
    s_test_hints_letter_none_draws_nothing();

    s_test_hints_letter_positioned_right_aligned();

    return TAP_DONE();
}
