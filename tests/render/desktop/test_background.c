/**
 * @file tests/render/desktop/test_background.c
 *
 * @brief Test battery for desktop root window background rendering
 *        (render/desktop/background.c)
 *
 * Covers, through the three public entry points
 * render/desktop/background.h exposes, every branch of
 * background.c's own real logic: render_desktop_background_render
 * (null desktop, out-of-range screen_id, null screen,
 * external-pixmap-detected, preserve-previous-external,
 * color-unchanged skip, and the actual paint path), and the atom-set
 * resolution and once-cached lookup
 * render_desktop_background_property_is_pixmap and
 * render_desktop_background_cache_invalidate share with it
 * (s_resolve_bg_atoms, s_get_root_background_pixmap, the per-screen
 * caches).
 *
 * Every XCB entry point background.c calls (xcb_get_property,
 * xcb_get_property_reply, xcb_get_property_value, xcb_change_window_
 * attributes, xcb_clear_area) and every cross-module project symbol
 * it reaches (atom_intern, logger_msg, viewport_mesh_is_visible,
 * viewport_mesh_render, viewport_mesh_cache_invalidate,
 * viewport_mesh_cache_release_retired, xcb_connection_get) is
 * a link-only, call-recording stand-in defined below, following the
 * pattern already established by tests/render/test_icon.c and
 * tests/render/test_wmicon.c: no real XCB library, and no real .c
 * file besides src/render/desktop/background.c itself, is linked at
 * all.
 *
 * A handful of stand-ins this file's own scenarios never reach
 * (client_titlebar_layout and the titlebar/decoration/window/atom-
 * opacity family, among others) are still defined below, copied
 * verbatim from the shared stub arsenal this file split off from
 * (tests/render/test_desktop.c); an unreachable stub with external
 * linkage costs nothing and keeps every test file in this split
 * self-contained rather than reaching across to one another.
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <client.h>
#include <client/predicates.h>
#include <config.h>
#include <defs/client.h>
#include <defs/config.h>
#include <desktop.h>
#include <harness/tap.h>
#include <logger.h>
#include <policy/stacking.h>
#include <render/desktop/background.h>
#include <surface.h>


/* ==================================================================== *
 * Link-only stand-ins for every external/cross-module/X-server call
 * desktop.c's translation unit reaches.  Each records what it was
 * asked to do and returns whatever a scenario configured beforehand,
 * following tests/wm/test_lifecycle.c's own precedent for the same
 * shape of problem.
 * ==================================================================== */

/* logger.h's LOGGER macro family */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict msg, ...)
{
    (void) level;
    (void) prefix;
    (void) msg;
    return 0;
}


/* utils/xcb/connection.h */
static xcb_connection_t *s_connection_stub = (xcb_connection_t *) 0x1;
static xcb_ewmh_connection_t *s_ewmh_stub = NULL;

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection_stub;
}

xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return s_ewmh_stub;
}


/* utils/xcb/atom.h */
static int s_atom_intern_calls;
static xcb_atom_t s_atom_intern_result = 500u;

xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) name;
    (void) only_if_exists;
    s_atom_intern_calls++;
    return s_atom_intern_result;
}

static int s_atom_set_opacity_calls;
static uint32_t s_atom_set_opacity_last_raw;

void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw)
{
    (void) connection;
    (void) window;
    s_atom_set_opacity_calls++;
    s_atom_set_opacity_last_raw = raw;
}


/* utils/xcb/pixmap.h */
static bool s_offscreen_buffer_should_fail = false;
static int s_offscreen_buffer_calls;
static xcb_pixmap_t s_next_buffer_id = 700u;

xcb_pixmap_t xcb_offscreen_buffer_create(xcb_connection_t *connection,
        uint8_t depth, xcb_drawable_t reference, uint16_t width,
        uint16_t height)
{
    (void) connection;
    (void) depth;
    (void) reference;
    (void) width;
    (void) height;
    s_offscreen_buffer_calls++;
    return (s_offscreen_buffer_should_fail) ? XCB_NONE : s_next_buffer_id;
}


/* utils/xcb/window.h */
static int s_window_place_calls;
static xcb_window_t s_window_place_last_window;

void xcb_window_place(xcb_window_t window, int32_t x, int32_t y,
        uint32_t w, uint32_t h)
{
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    s_window_place_calls++;
    s_window_place_last_window = window;
}

static int s_window_set_border_calls;
static uint32_t s_window_set_border_last_width;

void xcb_window_set_border(xcb_window_t window, uint32_t width)
{
    (void) window;
    s_window_set_border_calls++;
    s_window_set_border_last_width = width;
}

static int s_window_show_calls;
static xcb_window_t s_window_show_last_window;

void xcb_window_show(xcb_window_t window)
{
    s_window_show_calls++;
    s_window_show_last_window = window;
}

static int s_window_hide_calls;
static xcb_window_t s_window_hide_last_window;

void xcb_window_hide(xcb_window_t window)
{
    s_window_hide_calls++;
    s_window_hide_last_window = window;
}


/* client.h: cross-module (client.c / client/geom.c) */
static int s_border_color_apply_calls;
static bool s_border_color_apply_last_focused;

void client_border_color_apply(client_td *client, bool is_focused)
{
    (void) client;
    s_border_color_apply_calls++;
    s_border_color_apply_last_focused = is_focused;
}

static int s_synthetic_configure_calls;

void client_send_synthetic_configure_notify(
        xcb_connection_t *connection, const client_td *client)
{
    (void) connection;
    (void) client;
    s_synthetic_configure_calls++;
}

static int s_sync_visible_name_calls;

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
    s_sync_visible_name_calls++;
}

/* Never actually invoked at runtime (client_sync_visible_name only
 * ever receives this as a function-pointer argument in the real
 * client.c, which this file stubs out above), but the linker still
 * needs the symbol resolved wherever s_titlebar_draw_title takes its
 * address to pass along, the same gotcha already recorded for
 * tests/render/test_icon.c and xcb_ewmh_set_wm_visible_icon_name_
 * checked there */
xcb_void_cookie_t xcb_ewmh_set_wm_visible_name_checked(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        uint32_t strings_len, const char *strings)
{
    xcb_void_cookie_t cookie = {0};
    (void) ewmh;
    (void) window;
    (void) strings_len;
    (void) strings;
    return cookie;
}

static int s_titlebar_layout_calls;
static struct titlebar_button_layout_s s_titlebar_layout_left[
    CONFIG_MAX_TITLEBAR_BUTTONS];
static uint8_t s_titlebar_layout_left_n = 0u;
static struct titlebar_button_layout_s s_titlebar_layout_right[
    CONFIG_MAX_TITLEBAR_BUTTONS];
static uint8_t s_titlebar_layout_right_n = 0u;
static int16_t s_titlebar_layout_title_x = 4;
static uint16_t s_titlebar_layout_title_w = 80u;
static int16_t s_titlebar_layout_btn_y = 2;
static bool s_titlebar_layout_last_hide_pin;
static bool s_titlebar_layout_last_hide_sticky;

/* client_titlebar_layout (client/geom.c) is a cross-module dependency
 * genuinely external to desktop.c: stubbed as a fully controllable
 * stand-in rather than linked for real, matching every other stub in
 * this file, so a scenario can steer exactly which buttons land where
 * without also pulling in client/geom.c's own theme-parsing logic */
/** Real implementations, small enough to carry rather than stand in
 *  for: the drawing derives the button side and its stroke from the
 *  titlebar height through these, and a stand-in would only restate
 *  the same arithmetic
 *  @note Complexity: @e O(1) */
uint16_t client_titlebar_button_size(const struct config_theme_s *theme,
        uint16_t title_h)
{
    uint32_t side;
    uint32_t ceiling;

    if (theme == NULL) {
        return (uint16_t) WM_DECOR_BTN_SIZE_DEFAULT;
    }

    side = (uint32_t) theme->window.titlebar.buttons.size;
    if (side < (uint32_t) WM_DECOR_BTN_SIZE_MIN) {
        side = (uint32_t) WM_DECOR_BTN_SIZE_MIN;
    }

    ceiling = (title_h > 2u) ? (uint32_t) title_h - 2u
        : (uint32_t) WM_DECOR_BTN_SIZE_MIN;
    if (side > ceiling) {
        side = ceiling;
    }
    if (side < (uint32_t) WM_DECOR_BTN_SIZE_MIN) {
        side = (uint32_t) WM_DECOR_BTN_SIZE_MIN;
    }

    return (uint16_t) (side - (side % 2u));
}


uint16_t client_titlebar_button_shape_unit(uint16_t btn_size)
{
    uint16_t unit = (uint16_t) (btn_size / WM_DECOR_BTN_SHAPE_DIV);

    return (unit < (uint16_t) WM_DECOR_BTN_SHAPE_MIN)
        ? (uint16_t) WM_DECOR_BTN_SHAPE_MIN : unit;
}


void client_titlebar_layout(const struct config_theme_s *theme,
        uint16_t titlebar_w, uint16_t title_h, bool hide_pin,
        bool hide_sticky,
        struct titlebar_button_layout_s *restrict out_left,
        uint8_t *restrict out_left_n,
        struct titlebar_button_layout_s *restrict out_right,
        uint8_t *restrict out_right_n,
        int16_t *restrict out_title_x, uint16_t *restrict out_title_w,
        int16_t *restrict out_btn_y)
{
    uint8_t i;

    (void) theme;
    (void) titlebar_w;
    (void) title_h;
    s_titlebar_layout_calls++;
    s_titlebar_layout_last_hide_pin = hide_pin;
    s_titlebar_layout_last_hide_sticky = hide_sticky;

    for (i = 0u; i < s_titlebar_layout_left_n; ++i) {
        out_left[i] = s_titlebar_layout_left[i];
    }
    *out_left_n = s_titlebar_layout_left_n;
    for (i = 0u; i < s_titlebar_layout_right_n; ++i) {
        out_right[i] = s_titlebar_layout_right[i];
    }
    *out_right_n = s_titlebar_layout_right_n;
    *out_title_x = s_titlebar_layout_title_x;
    *out_title_w = s_titlebar_layout_title_w;
    *out_btn_y = s_titlebar_layout_btn_y;
}


/* config.h */
uint32_t config_theme_opacity_to_raw(uint8_t percent)
{
    /* A faithful-enough stand-in for the real formula (opacity as a
     * fraction of UINT32_MAX): not desktop.c's own logic to test, but
     * a deterministic, order-preserving mapping is worth keeping
     * anyway so a test asserting "the override percent was used, not
     * the theme default" can tell the two apart by their raw value */
    return (uint32_t) percent * 0x01010101u;
}


/* policy/stacking.h */
static uint32_t s_stacking_count_result = 0u;
static client_td *s_stacking_walk_clients[8];
static uint8_t s_stacking_walk_client_count = 0u;

uint32_t stacking_count(const desktop_td *desktop)
{
    (void) desktop;
    return s_stacking_count_result;
}

void stacking_walk(const desktop_td *desktop,
        stacking_visitor_fn visit, void *data)
{
    uint8_t i;

    (void) desktop;
    if (visit == NULL) {
        return;
    }
    for (i = 0u; i < s_stacking_walk_client_count; ++i) {
        visit(s_stacking_walk_clients[i], data);
    }
}


/* policy/urgency.h */
static bool s_urgency_blink_is_on_result = false;

bool urgency_blink_is_on(void)
{
    return s_urgency_blink_is_on_result;
}


/* render/icon.h */
static int s_render_client_icon_calls;
static client_td *s_render_client_icon_last_client;
static bool s_render_client_icon_last_is_current;

void ri_render_client_icon(client_td *client, bool is_current,
        bool force)
{
    (void) force;
    s_render_client_icon_calls++;
    s_render_client_icon_last_client = client;
    s_render_client_icon_last_is_current = is_current;
}


/* render/text.h */
static int s_text_use_font_calls;
static int s_text_set_color_calls;
static int s_text_draw_string_calls;
static char s_text_draw_string_last[256];
static int s_text_measure_result = 10;
static int16_t s_text_ascent_result = 12;
static int16_t s_text_descent_result = 3;

int text_renderer_use_font(xcb_connection_t *connection,
        const char *font_name)
{
    (void) connection;
    (void) font_name;
    s_text_use_font_calls++;
    return 0;
}

void text_renderer_set_color(uint32_t foreground, uint32_t background)
{
    (void) foreground;
    (void) background;
    s_text_set_color_calls++;
}

void text_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc,
        struct position_s pos, const char *text)
{
    (void) connection;
    (void) drawable;
    (void) gc;
    (void) pos;
    s_text_draw_string_calls++;
    s_text_draw_string_last[0] = '\0';
    if (text != NULL) {
        strncpy(s_text_draw_string_last, text,
                sizeof(s_text_draw_string_last) - 1u);
    }
}

uint16_t text_string_measure(const char *text)
{
    (void) text;
    return (uint16_t) s_text_measure_result;
}

void text_truncate_to_width(char *buf, size_t buf_sz, const char *text,
        uint16_t max_width)
{
    (void) max_width;
    if (buf == NULL || buf_sz == 0u) {
        return;
    }
    if (text == NULL) {
        buf[0] = '\0';
        return;
    }
    strncpy(buf, text, buf_sz - 1u);
    buf[buf_sz - 1u] = '\0';
}

int16_t text_font_ascent(void)
{
    return s_text_ascent_result;
}

int16_t text_font_descent(void)
{
    return s_text_descent_result;
}


/* wm.h */
static surface_td *s_surface_by_id_result = NULL;

surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    (void) surface_id;
    return s_surface_by_id_result;
}


/* render/viewport/mesh.c
 *
 * Link-only: the scenarios below all leave 'viewport.mesh.is-enabled'
 * false in their configuration, so the visibility stand-in reports
 * false and the two painting entries are never reached.  The mesh has
 * its own battery, tests/render/viewport/test_mesh.c. */
bool viewport_mesh_is_visible(const desktop_td *desktop)
{
    (void) desktop;

    return false;
}


int viewport_mesh_render(xcb_connection_t *connection,
        const desktop_td *desktop)
{
    (void) connection;
    (void) desktop;

    return 0;
}


void viewport_mesh_cache_invalidate(void)
{
}


/** Recording stand-in for @a viewport_mesh_cache_release_retired: the
 *  root has just been pointed away from whatever tile was cached, so
 *  this is where the real one frees it
 *  @note Complexity: @e O(1) */
static int s_call_mesh_release_retired;

void viewport_mesh_cache_release_retired(xcb_connection_t *connection)
{
    (void) connection;

    s_call_mesh_release_retired++;
}


/* surface/viewport.c
 *
 * Reproduced here rather than linking that whole (separately tested)
 * file for one two-line predicate, the same way
 * tests/menu/dialog/test_confirm.c reproduces 'dlgutil_u16max'.  The
 * scenarios below drive it through a real 'config' on the surface,
 * exactly as production reaches it. */
bool surface_viewport_has_room(const surface_td *surface)
{
    if (surface == NULL || surface->config == NULL ||
            surface->id >= (uint32_t) CONFIG_MAX_SCREENS) {
        return false;
    }

    return surface->config->base.screens[surface->id]
               .viewport.columns > 1u ||
           surface->config->base.screens[surface->id]
               .viewport.rows > 1u;
}


/* Raw XCB calls */
static int s_get_property_calls;
static bool s_get_property_reply_should_fail = true;
static xcb_pixmap_t s_get_property_pixmap_value = XCB_NONE;
static uint8_t s_get_property_reply_format = 32u;
static uint32_t s_get_property_reply_value_len = 1u;

xcb_get_property_cookie_t xcb_get_property(xcb_connection_t *connection,
        uint8_t _delete, xcb_window_t window, xcb_atom_t property,
        xcb_atom_t type, uint32_t long_offset, uint32_t long_length)
{
    xcb_get_property_cookie_t cookie = {0};
    (void) connection;
    (void) _delete;
    (void) window;
    (void) property;
    (void) type;
    (void) long_offset;
    (void) long_length;
    s_get_property_calls++;
    return cookie;
}

/* Heap-allocated, matching the real libxcb contract that
 * xcb_get_property_reply's caller owns and frees the pointer (desktop.c
 * itself calls 'free(reply)' after reading it), with the pixmap value
 * placed immediately after the fixed header the way xcb_get_property_
 * value's real pointer arithmetic (header size plus format-dependent
 * offset) expects */
xcb_get_property_reply_t *xcb_get_property_reply(
        xcb_connection_t *connection, xcb_get_property_cookie_t cookie,
        xcb_generic_error_t **error)
{
    xcb_get_property_reply_t *reply;

    (void) connection;
    (void) cookie;
    if (error != NULL) {
        *error = NULL;
    }
    if (s_get_property_reply_should_fail) {
        return NULL;
    }

    reply = malloc(sizeof(*reply) + sizeof(xcb_pixmap_t));
    memset(reply, 0, sizeof(*reply));
    reply->format = s_get_property_reply_format;
    reply->value_len = s_get_property_reply_value_len;
    memcpy((char *) reply + sizeof(*reply), &s_get_property_pixmap_value,
            sizeof(xcb_pixmap_t));
    return reply;
}

void *xcb_get_property_value(const xcb_get_property_reply_t *reply)
{
    return (void *) ((const char *) reply + sizeof(*reply));
}

static int s_change_window_attributes_calls;

xcb_void_cookie_t xcb_change_window_attributes(
        xcb_connection_t *connection, xcb_window_t window,
        uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = {0};
    (void) connection;
    (void) window;
    (void) value_mask;
    (void) value_list;
    s_change_window_attributes_calls++;
    return cookie;
}

static int s_clear_area_calls;

/** The titlebar and the frame are cleared on every pass by design, so
 *  a scenario watching the content window counts only its own */
static xcb_window_t s_clear_area_last_window;
static xcb_window_t s_clear_area_watched_window;
static int s_clear_area_watched_calls;

xcb_void_cookie_t xcb_clear_area(xcb_connection_t *connection,
        uint8_t exposures, xcb_window_t window, int16_t x, int16_t y,
        uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie = {0};
    (void) connection;
    (void) exposures;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    s_clear_area_calls++;
    s_clear_area_last_window = window;
    if (window == s_clear_area_watched_window) {
        s_clear_area_watched_calls++;
    }
    return cookie;
}

static uint32_t s_next_generated_id = 900u;

uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    return s_next_generated_id++;
}

static int s_create_gc_calls;

xcb_void_cookie_t xcb_create_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc, xcb_drawable_t drawable, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = {0};
    (void) connection;
    (void) gc;
    (void) drawable;
    (void) value_mask;
    (void) value_list;
    s_create_gc_calls++;
    return cookie;
}

static int s_poly_fill_rectangle_calls;

/** Shapes the button drawing emits, counted so a scenario can tell
 *  one button's outline from another's without a display */
static int s_poly_segment_calls;
static int s_poly_rectangle_calls;
static int s_change_gc_calls;

xcb_void_cookie_t xcb_poly_segment(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc, uint32_t segs_len,
        const xcb_segment_t *segs)
{
    xcb_void_cookie_t cookie = {0};
    (void) connection;
    (void) drawable;
    (void) gc;
    (void) segs_len;
    (void) segs;
    s_poly_segment_calls++;
    return cookie;
}


xcb_void_cookie_t xcb_poly_rectangle(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc, uint32_t rects_len,
        const xcb_rectangle_t *rects)
{
    xcb_void_cookie_t cookie = {0};
    (void) connection;
    (void) drawable;
    (void) gc;
    (void) rects_len;
    (void) rects;
    s_poly_rectangle_calls++;
    return cookie;
}


xcb_void_cookie_t xcb_change_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = {0};
    (void) connection;
    (void) gc;
    (void) value_mask;
    (void) value_list;
    s_change_gc_calls++;
    return cookie;
}


xcb_void_cookie_t xcb_poly_fill_rectangle(xcb_connection_t *connection,
        xcb_drawable_t drawable, xcb_gcontext_t gc, uint32_t rects_len,
        const xcb_rectangle_t *rects)
{
    xcb_void_cookie_t cookie = {0};
    (void) connection;
    (void) drawable;
    (void) gc;
    (void) rects_len;
    (void) rects;
    s_poly_fill_rectangle_calls++;
    return cookie;
}

static int s_free_gc_calls;

xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = {0};
    (void) connection;
    (void) gc;
    s_free_gc_calls++;
    return cookie;
}

static int s_copy_area_calls;

xcb_void_cookie_t xcb_copy_area(xcb_connection_t *connection,
        xcb_drawable_t src_drawable, xcb_drawable_t dst_drawable,
        xcb_gcontext_t gc, int16_t src_x, int16_t src_y, int16_t dst_x,
        int16_t dst_y, uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie = {0};
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

static int s_free_pixmap_calls;

xcb_void_cookie_t xcb_free_pixmap(xcb_connection_t *connection,
        xcb_pixmap_t pixmap)
{
    xcb_void_cookie_t cookie = {0};
    (void) connection;
    (void) pixmap;
    s_free_pixmap_calls++;
    return cookie;
}


/* ==================================================================== *
 * Fixture helpers
 * ==================================================================== */

/** Resets every stub call counter/state to a known baseline; does NOT
 *  reset the module-level static caches inside desktop.c itself
 *  (s_bg_pixmap_resolved/s_bg_pixmap_cache/s_root_bg_applied_once/
 *  s_root_bg_color_applied/s_bg_atoms), which desktop.c exposes no way
 *  to reset directly.  desktop_background_pixmap_cache_invalidate
 *  resets the pixmap half of that itself, and is called at the top of
 *  every scenario below that depends on a clean pixmap-cache state;
 *  the root-color-applied half is instead worked around by giving each
 *  such scenario its own distinct screen_id, so no scenario can ever
 *  observe another's leftover cached color */
static void s_reset_fixture(void)
{
    s_atom_intern_calls = 0;
    s_atom_set_opacity_calls = 0;
    s_atom_set_opacity_last_raw = 0u;
    s_offscreen_buffer_should_fail = false;
    s_offscreen_buffer_calls = 0;
    s_next_buffer_id = 700u;
    s_window_place_calls = 0;
    s_window_set_border_calls = 0;
    s_window_set_border_last_width = 0u;
    s_window_show_calls = 0;
    s_window_hide_calls = 0;
    s_border_color_apply_calls = 0;
    s_synthetic_configure_calls = 0;
    s_sync_visible_name_calls = 0;
    s_titlebar_layout_calls = 0;
    s_titlebar_layout_left_n = 0u;
    s_titlebar_layout_right_n = 0u;
    s_titlebar_layout_title_x = 4;
    s_titlebar_layout_title_w = 80u;
    s_titlebar_layout_btn_y = 2;
    s_stacking_count_result = 0u;
    s_stacking_walk_client_count = 0u;
    s_urgency_blink_is_on_result = false;
    s_render_client_icon_calls = 0;
    s_render_client_icon_last_client = NULL;
    s_text_use_font_calls = 0;
    s_text_set_color_calls = 0;
    s_text_draw_string_calls = 0;
    s_text_draw_string_last[0] = '\0';
    s_text_measure_result = 10;
    s_text_ascent_result = 12;
    s_text_descent_result = 3;
    s_surface_by_id_result = NULL;
    s_get_property_calls = 0;
    s_get_property_reply_should_fail = true;
    s_get_property_pixmap_value = XCB_NONE;
    s_get_property_reply_format = 32u;
    s_get_property_reply_value_len = 1u;
    s_change_window_attributes_calls = 0;
    s_clear_area_calls = 0;
    s_clear_area_last_window = 0u;
    s_clear_area_watched_window = 0u;
    s_clear_area_watched_calls = 0;
    s_create_gc_calls = 0;
    s_poly_fill_rectangle_calls = 0;
    s_poly_segment_calls = 0;
    s_poly_rectangle_calls = 0;
    s_change_gc_calls = 0;
    s_free_gc_calls = 0;
    s_copy_area_calls = 0;
    s_free_pixmap_calls = 0;
    s_ewmh_stub = NULL;
}


/** Builds a real, zero-initialized desktop_td/client_td/config_td
 *  triplet, wired together the way desktop.c expects (desktop->config,
 *  client->config), with a real xcb_screen_t behind desktop->screen so
 *  desktop_render_background's dereference is never a fabrication */
struct s_desktop_fixture_s {
    desktop_td desktop;
    xcb_screen_t screen;
    config_td config;
};

static void s_desktop_fixture_init(struct s_desktop_fixture_s *fx,
        uint32_t screen_id, uint32_t desktop_id)
{
    memset(fx, 0, sizeof(*fx));
    fx->screen.root = 0x10u + screen_id;
    fx->screen.root_depth = 24u;
    fx->screen.width_in_pixels = 1920u;
    fx->screen.height_in_pixels = 1080u;
    fx->desktop.screen = &fx->screen;
    fx->desktop.config = &fx->config;
    fx->desktop.screen_id = screen_id;
    fx->desktop.id = desktop_id;
    strncpy(fx->desktop.name, "test", sizeof(fx->desktop.name) - 1u);
}


/* ==================================================================== *
 * render_desktop_background_cache_invalidate /
 * render_desktop_background_property_is_pixmap
 * ==================================================================== */

static void s_test_property_is_bg_pixmap_none_atom_is_false(void)
{
    s_reset_fixture();

    TAP_OK(!render_desktop_background_property_is_pixmap(s_connection_stub,
                XCB_ATOM_NONE),
            "XCB_ATOM_NONE is never recognized as a background"
            " pixmap property, without even trying to resolve the"
            " candidate atom set first");
    TAP_EQ_INT(s_atom_intern_calls, 0,
            "...short-circuiting before ever calling atom_intern");
}

static void s_test_property_is_bg_pixmap_resolves_once(void)
{
    s_reset_fixture();
    s_atom_intern_result = 777u;

    (void) render_desktop_background_property_is_pixmap(s_connection_stub,
            123u);
    TAP_EQ_INT(s_atom_intern_calls, 3,
            "the first call with a real atom resolves all three"
            " candidate property names in one pass");

    (void) render_desktop_background_property_is_pixmap(s_connection_stub,
            123u);
    TAP_EQ_INT(s_atom_intern_calls, 3,
            "...and a second call reuses the cached atoms rather"
            " than re-resolving them, since none of the three were"
            " left unresolved");
}

/* Exercises the "resolved atom matches" and "different atom does not
 * match" branches using the exact same 777u every candidate name
 * resolved to just above: 's_bg_atoms' is a module-static cache that,
 * once resolved, is permanent for the rest of this test binary's
 * process lifetime (matching the real s_resolve_bg_atoms's own
 * documented contract that a successful resolution never expires), so
 * a later scenario cannot force a fresh atom_intern_result of its own
 * to take effect here; asserting against 777u directly, immediately
 * after the scenario that resolved it, keeps this test from silently
 * depending on run order for its expected value */
static void s_test_property_is_bg_pixmap_matches_resolved_atom(void)
{
    TAP_OK(render_desktop_background_property_is_pixmap(s_connection_stub,
                777u),
            "an atom equal to whatever every candidate name"
            " resolved to (all three, here, since atom_intern"
            " returns the same stub value for each) is recognized"
            " as a background pixmap property");
    TAP_OK(!render_desktop_background_property_is_pixmap(s_connection_stub,
                778u),
            "a different atom is not");
}

static void s_test_cache_invalidate_forces_pixmap_reresolution(void)
{
    int calls_before;
    struct s_desktop_fixture_s fx;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_get_property_reply_should_fail = true;

    (void) render_desktop_background_render(&fx.desktop);
    TAP_OK(s_get_property_calls > 0,
            "before any cache exists, rendering a background queries"
            " the root window's candidate pixmap properties");

    calls_before = s_get_property_calls;

    (void) render_desktop_background_render(&fx.desktop);
    TAP_EQ_INT(s_get_property_calls, calls_before,
            "a second render on the same screen hits the resolved-"
            "pixmap cache and issues no further property queries"
            " at all");

    render_desktop_background_cache_invalidate();
    (void) render_desktop_background_render(&fx.desktop);
    TAP_OK(s_get_property_calls > calls_before,
            "invalidating the cache makes the next render query the"
            " candidate properties again instead of trusting the"
            " stale cached answer");
}


/* ==================================================================== *
 * render_desktop_background_render
 * ==================================================================== */

static void s_test_render_background_null_desktop(void)
{
    s_reset_fixture();
    TAP_EQ_INT(render_desktop_background_render(NULL), 1,
            "a null desktop pointer fails outright rather than"
            " dereferencing it");
}

static void s_test_render_background_screen_id_out_of_range(void)
{
    struct s_desktop_fixture_s fx;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    fx.desktop.screen_id = (uint32_t) CONFIG_MAX_SCREENS;

    TAP_EQ_INT(render_desktop_background_render(&fx.desktop), 1,
            "a screen_id at or beyond CONFIG_MAX_SCREENS fails"
            " rather than indexing the per-screen caches"
            " out of bounds");
}

static void s_test_render_background_null_screen(void)
{
    struct s_desktop_fixture_s fx;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 1u, 0u);
    fx.desktop.screen = NULL;

    TAP_EQ_INT(render_desktop_background_render(&fx.desktop), 1,
            "a desktop whose cached screen pointer is null fails"
            " rather than dereferencing it for root/width/height");
}

static void s_test_render_background_external_pixmap_detected(void)
{
    struct s_desktop_fixture_s fx;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 2u, 0u);
    s_get_property_reply_should_fail = false;
    s_get_property_pixmap_value = 999u;
    fx.desktop.background.use_root_pixmap = false;

    TAP_EQ_INT(render_desktop_background_render(&fx.desktop), 0,
            "an external wallpaper tool's pixmap property being"
            " found succeeds without ever touching"
            " xcb_change_window_attributes");
    TAP_OK(fx.desktop.background.use_root_pixmap,
            "...and records that the root window's background is"
            " now externally owned");
    TAP_EQ_INT(s_change_window_attributes_calls, 0,
            "...never overwriting the external wallpaper with the"
            " desktop's own configured color");
}

static void s_test_render_background_preserves_previous_external(
        void)
{
    struct s_desktop_fixture_s fx;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 3u, 0u);
    s_get_property_reply_should_fail = true;
    fx.desktop.background.use_root_pixmap = true;

    TAP_EQ_INT(render_desktop_background_render(&fx.desktop), 0,
            "no external pixmap property found this time, but one"
            " was previously in effect: succeeds while leaving the"
            " root window untouched");
    TAP_EQ_INT(s_change_window_attributes_calls, 0,
            "...still never repainting over what the external tool"
            " last drew");
    TAP_OK(fx.desktop.background.use_root_pixmap,
            "...and the external-ownership flag stays set, since"
            " nothing here has any reason to believe the WM has"
            " taken the background back");
}

static void s_test_render_background_paints_color_first_time(void)
{
    struct s_desktop_fixture_s fx;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 4u, 0u);
    s_get_property_reply_should_fail = true;
    fx.desktop.background.use_root_pixmap = false;
    fx.desktop.background.bg.color = 0x112233u;

    TAP_EQ_INT(render_desktop_background_render(&fx.desktop), 0,
            "with no external pixmap and no prior applied color for"
            " this screen, the desktop's configured color is"
            " painted onto the root window");
    TAP_EQ_INT(s_change_window_attributes_calls, 1,
            "...via exactly one xcb_change_window_attributes call");
    TAP_EQ_INT(s_clear_area_calls, 1,
            "...followed by exactly one xcb_clear_area covering the"
            " whole root window");
}

static void s_test_render_background_skips_unchanged_color(void)
{
    int clears_before;
    int changes_before;
    struct s_desktop_fixture_s fx;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 5u, 0u);
    s_get_property_reply_should_fail = true;
    fx.desktop.background.bg.color = 0xaabbccu;

    (void) render_desktop_background_render(&fx.desktop);
    changes_before = s_change_window_attributes_calls;
    clears_before = s_clear_area_calls;

    TAP_EQ_INT(render_desktop_background_render(&fx.desktop), 0,
            "rendering again with the exact same configured color"
            " still succeeds");
    TAP_EQ_INT(s_change_window_attributes_calls, changes_before,
            "...but repaints nothing further, since this screen's"
            " root window already shows that same color");
    TAP_EQ_INT(s_clear_area_calls, clears_before,
            "...not even the clear-area call");
}

static void s_test_render_background_repaints_on_color_change(void)
{
    int changes_before;
    struct s_desktop_fixture_s fx;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 1u);
    render_desktop_background_cache_invalidate();
    s_get_property_reply_should_fail = true;
    fx.desktop.background.bg.color = 0x111111u;
    (void) render_desktop_background_render(&fx.desktop);

    fx.desktop.background.bg.color = 0x222222u;
    changes_before = s_change_window_attributes_calls;

    TAP_EQ_INT(render_desktop_background_render(&fx.desktop), 0,
            "a genuinely different configured color succeeds");
    TAP_OK(s_change_window_attributes_calls > changes_before,
            "...and actually repaints this time, since the color"
            " showing on screen no longer matches what is now"
            " configured");
}


int main(void)
{
    TAP_PLAN(26);

    s_test_property_is_bg_pixmap_none_atom_is_false();
    s_test_property_is_bg_pixmap_resolves_once();
    s_test_property_is_bg_pixmap_matches_resolved_atom();
    s_test_cache_invalidate_forces_pixmap_reresolution();

    s_test_render_background_null_desktop();
    s_test_render_background_screen_id_out_of_range();
    s_test_render_background_null_screen();
    s_test_render_background_external_pixmap_detected();
    s_test_render_background_preserves_previous_external();
    s_test_render_background_paints_color_first_time();
    s_test_render_background_skips_unchanged_color();
    s_test_render_background_repaints_on_color_change();

    return TAP_DONE();
}
