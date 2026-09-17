/**
 * @file tests/render/test_desktop.c
 *
 * @brief Test battery for the desktop render pass orchestration
 *        (render/desktop.c)
 *
 * Covers, through the two public entry points render/desktop.h
 * exposes, every branch of desktop.c's own real logic:
 * desktop_render_full's guard and dispatch to background-plus-
 * clients, and desktop_render_one_client's rich branch tree
 * (urgency-blink is_focused flip, hide_decoration for a fullscreen
 * client that used to be decorated, titlebar_visible, the border_width
 * decision among dock/notification, fullscreen, framed, and plain
 * cases, the border_width/border_color change-skip, is_current mapping
 * including the icon-window-hide and shaded-content-window-skip
 * cases, and the outdated-geometry-vs-focus-only-refresh dispatch,
 * each reached through their own static helper).
 *
 * Every XCB entry point desktop.c calls (xcb_window_hide,
 * xcb_window_place, xcb_window_set_border, xcb_window_show,
 * xcb_clear_area) and every cross-module project symbol it reaches
 * (client_border_color_apply, client_send_synthetic_configure_notify,
 * logger_msg, render_client_decoration_repaint_frame_unless_hidden,
 * render_client_titlebar_repaint_content,
 * render_desktop_background_render, ri_render_client_icon,
 * stacking_count, stacking_walk, urgency_blink_is_on,
 * xcb_connection_get) is a link-only, call-recording stand-in defined
 * below, following the pattern already established by
 * tests/render/test_icon.c and tests/render/test_wmicon.c: no real
 * XCB library, and no real .c file besides src/render/desktop.c
 * itself, is linked at all.  client_is_decorated, client_is_
 * fullscreen, client_is_shaded, client_is_urgent, client_is_pinned,
 * client_is_maximizable (through client_is_resizable and client_is_
 * modal), client_is_iconified, and wm_validate_client/
 * wm_validate_desktop are real inline functions or macros pulled in
 * from the real headers, exercised for real rather than stubbed, the
 * same way test_icon.c already relies on the real inline
 * wm_validate_client from include/render/outdate.h.
 *
 * The titlebar, frame decoration, and desktop background rendering
 * this file used to also cover directly (back when all of it lived in
 * one render/desktop.c translation unit) moved out to their own
 * modules and their own batteries: tests/render/client/test_titlebar.c,
 * tests/render/client/test_decoration.c, and
 * tests/render/desktop/test_background.c.  The three real functions
 * those modules export are stubbed below purely as call-recording
 * stand-ins, the same as every other cross-module symbol this file
 * reaches, not exercised for their own logic here.
 *
 * A handful of stand-ins this file's own scenarios never reach
 * (client_titlebar_layout and the pixmap/atom-opacity family, among
 * others) are still defined below, copied verbatim from the shared
 * stub arsenal this file split off from; an unreachable stub with
 * external linkage costs nothing and keeps every test file in this
 * split self-contained rather than reaching across to one another.
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
#include <render/desktop.h>
#include <stage.h>


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
        bool force, bool restack)
{
    (void) force;
    (void) restack;
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
static stage_td *s_stage_by_id_result = NULL;

stage_td *wm_get_stage_by_id(uint32_t stage_id)
{
    (void) stage_id;
    return s_stage_by_id_result;
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


/* stage/viewport.c
 *
 * Reproduced here rather than linking that whole (separately tested)
 * file for one two-line predicate, the same way
 * tests/menu/dialog/test_confirm.c reproduces 'dlgutil_u16max'.  The
 * scenarios below drive it through a real 'config' on the stage,
 * exactly as production reaches it. */
bool stage_viewport_has_room(const stage_td *stage)
{
    if (stage == NULL || stage->config == NULL ||
            stage->id >= (uint32_t) CONFIG_MAX_SCREENS) {
        return false;
    }

    return stage->config->base.screens[stage->id]
               .viewport.columns > 1u ||
           stage->config->base.screens[stage->id]
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


/* render/desktop/background.h, render/client/decoration.h,
 * render/client/titlebar.h: the three modules the render pass
 * dispatches into, each with its own battery
 * (tests/render/desktop/test_background.c,
 * tests/render/client/test_decoration.c,
 * tests/render/client/test_titlebar.c) covering its own logic; only
 * call-recording stand-ins here, the same as every other cross-module
 * symbol above */
static int s_render_background_calls;

int render_desktop_background_render(desktop_td *desktop)
{
    (void) desktop;
    s_render_background_calls++;
    return 0;
}

static int s_repaint_frame_unless_hidden_calls;

void render_client_decoration_repaint_frame_unless_hidden(
        xcb_connection_t *connection, client_td *client,
        bool is_focused, bool hide_decoration,
        const struct config_theme_s *theme)
{
    (void) connection;
    (void) client;
    (void) is_focused;
    (void) hide_decoration;
    (void) theme;
    s_repaint_frame_unless_hidden_calls++;
}

static int s_repaint_titlebar_content_calls;

void render_client_titlebar_repaint_content(xcb_connection_t *connection,
        client_td *client, bool is_focused, uint16_t inner_w,
        uint16_t title_h, const struct config_theme_s *theme)
{
    (void) connection;
    (void) client;
    (void) is_focused;
    (void) inner_w;
    (void) title_h;
    (void) theme;
    s_repaint_titlebar_content_calls++;
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
    s_stage_by_id_result = NULL;
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
    s_render_background_calls = 0;
    s_repaint_frame_unless_hidden_calls = 0;
    s_repaint_titlebar_content_calls = 0;
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

static void s_client_fixture_init(client_td *client, config_td *config,
        xcb_window_t window)
{
    memset(client, 0, sizeof(*client));
    client->config = config;
    client->window = window;
    client->id = window;
}


/* ==================================================================== *
 * desktop_render_full
 * ==================================================================== */

static void s_test_render_full_null_desktop(void)
{
    s_reset_fixture();
    TAP_EQ_INT(desktop_render_full(NULL, true), 1,
            "a null desktop pointer fails outright");
}

static void s_test_render_full_current_paints_background(void)
{
    struct s_desktop_fixture_s fx;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 1u, 2u);
    s_stacking_count_result = 0u;
    fx.desktop.is_outdated = true;
    fx.desktop.is_focus_dirty = true;

    TAP_EQ_INT(desktop_render_full(&fx.desktop, true), 0,
            "a full render of the currently displayed desktop"
            " succeeds");
    TAP_EQ_INT(s_render_background_calls, 1,
            "...and actually dispatches to the background module,"
            " since is_current is true");
    TAP_OK(!fx.desktop.is_outdated,
            "...clearing is_outdated via the real inline"
            " wm_validate_desktop");
    TAP_OK(!fx.desktop.is_focus_dirty,
            "...and clearing is_focus_dirty directly, so the next"
            " render pass does not re-trigger every client's"
            " focus-only decoration refresh for no reason");
}

static void s_test_render_full_non_current_skips_background(void)
{
    struct s_desktop_fixture_s fx;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 2u, 3u);
    s_stacking_count_result = 0u;

    TAP_EQ_INT(desktop_render_full(&fx.desktop, false), 0,
            "a full render of a desktop that is not currently"
            " displayed still succeeds");
    TAP_EQ_INT(s_render_background_calls, 0,
            "...but never dispatches to the background module for"
            " a desktop nobody can see");
}

static void s_test_render_full_visits_every_stacked_client(void)
{
    struct s_desktop_fixture_s fx;
    client_td client_a;
    client_td client_b;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 3u, 4u);
    s_client_fixture_init(&client_a, &fx.config, 0x201u);
    s_client_fixture_init(&client_b, &fx.config, 0x202u);
    client_a.properties.flags = (uint16_t) CLIENT_FLAG_HIDDEN;
    client_b.properties.flags = 0u;
    /* Without is_outdated set, desktop_render_one_client takes
     * neither the geometry-apply nor the focus-only-refresh path at
     * all (is_focus_dirty is also left false here), so no geometry
     * would ever be placed regardless of is_current */
    client_b.is_outdated = true;
    s_stacking_walk_clients[0] = &client_a;
    s_stacking_walk_clients[1] = &client_b;
    s_stacking_walk_client_count = 2u;
    /* stacking_count gates s_desktop_render_clients's own early-out
     * before it ever calls stacking_walk at all: it must agree with
     * however many clients the walk stand-in below is actually going
     * to visit, or the walk is skipped outright */
    s_stacking_count_result = 2u;

    TAP_EQ_INT(desktop_render_full(&fx.desktop, false), 0,
            "a full render walking two stacked clients succeeds");
    TAP_EQ_INT(s_render_client_icon_calls, 0,
            "the hidden-but-not-iconified client is skipped"
            " entirely, drawn neither as a window nor as an icon");
    TAP_OK(s_window_show_calls == 0 && s_window_place_calls >= 1,
            "the plain visible client has its geometry placed;"
            " nothing is mapped since is_current is false here");
}

/* Rendering a decorated client whose content ends up exactly where it
 * already was must not clear its window: this pass runs for any
 * reason at all, a changed title among them, and clearing blanks the
 * client until it paints itself back */
static void s_test_render_full_clears_only_when_content_moves(void)
{
    struct s_desktop_fixture_s fx;
    client_td client;
    int clears_after_first;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 4u, 6u);
    s_client_fixture_init(&client, &fx.config, 0x401u);
    client.frame = 0x9001u;
    client.properties.flags = (uint16_t) CLIENT_FLAG_DECORATED;
    client.layout.geometry.cur.dim.w = 400u;
    client.layout.geometry.cur.dim.h = 300u;
    client.layout.frame_extents.left = 2;
    client.layout.frame_extents.right = 2;
    client.layout.frame_extents.top = 22;
    client.layout.frame_extents.bottom = 2;
    client.is_outdated = true;
    s_clear_area_watched_window = client.window;
    s_stacking_walk_clients[0] = &client;
    s_stacking_walk_client_count = 1u;
    s_stacking_count_result = 1u;

    (void) desktop_render_full(&fx.desktop, true);
    clears_after_first = s_clear_area_watched_calls;
    TAP_OK(clears_after_first > 0,
            "a client's first render clears its own window, nothing"
            " being known yet about where its content sat");

    client.is_outdated = true;
    (void) desktop_render_full(&fx.desktop, true);
    TAP_EQ_INT(s_clear_area_watched_calls, clears_after_first,
            "rendering it again with the same geometry clears"
            " nothing further");

    client.is_outdated = true;
    client.layout.geometry.cur.dim.w =
        (uint16_t) (client.layout.geometry.cur.dim.w + 40u);
    (void) desktop_render_full(&fx.desktop, true);
    TAP_OK(s_clear_area_watched_calls > clears_after_first,
            "but a real change of size clears it again, so a toolkit"
            " redrawing on Expose alone still follows the frame");
}


static void s_test_render_full_iconified_hidden_client_draws_icon(
        void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 4u, 5u);
    s_client_fixture_init(&client, &fx.config, 0x301u);
    client.properties.flags = (uint16_t) CLIENT_FLAG_HIDDEN;
    client.properties.state = (uint16_t) CLIENT_STATE_ICONIFIED;
    s_stacking_walk_clients[0] = &client;
    s_stacking_walk_client_count = 1u;
    s_stacking_count_result = 1u;

    (void) desktop_render_full(&fx.desktop, true);

    TAP_EQ_INT(s_render_client_icon_calls, 1,
            "a hidden client that is also iconified is drawn as its"
            " icon placeholder instead of being skipped outright");
    TAP_OK(s_render_client_icon_last_client == &client,
            "...for that exact client");
    TAP_OK(s_render_client_icon_last_is_current,
            "...forwarding the walk's own is_current, unmodified");
}


/* ==================================================================== *
 * desktop_render_one_client
 * ==================================================================== */

static void s_test_render_one_client_urgency_blink_flips_focus(void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x401u);
    client.properties.flags = (uint16_t) CLIENT_FLAG_URGENT;
    fx.desktop.client_active_id = client.id;
    s_urgency_blink_is_on_result = true;

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_EQ_INT(s_border_color_apply_calls, 1,
            "an urgent client mid-blink still gets its border color"
            " applied (it is undecorated here, frame == 0)");
    TAP_OK(!s_border_color_apply_last_focused,
            "...but with is_focused flipped to false even though"
            " client_active_id actually names this very client,"
            " since the blink is mid-cycle");
}

static void s_test_render_one_client_urgent_not_blinking_keeps_focus(
        void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x402u);
    client.properties.flags = (uint16_t) CLIENT_FLAG_URGENT;
    fx.desktop.client_active_id = client.id;
    s_urgency_blink_is_on_result = false;

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_OK(s_border_color_apply_last_focused,
            "the other half of the blink cycle leaves is_focused as"
            " client_active_id alone would already say (focused)");
}

static void s_test_render_one_client_hide_decoration_fullscreen(void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x403u);
    client.properties.state = (uint16_t) CLIENT_STATE_FULLSCREEN;
    client.was_decorated_fullscreen = true;
    client.titlebar = 0x500u;
    client.is_outdated = true;

    /* is_current must be true here: the titlebar-hide/show dispatch
     * this scenario targets only runs inside desktop_render_one_
     * client's own 'if (is_current)' block, never as part of the
     * outdated-geometry-apply path exercised elsewhere in this file */
    desktop_render_one_client(&fx.desktop, &client, true);

    TAP_EQ_INT(s_window_hide_calls, 1,
            "a fullscreen client that used to be decorated has its"
            " titlebar hidden rather than repainted, since"
            " decoration is currently forced off");
    TAP_EQ_INT(s_window_set_border_last_width, 0u,
            "...and takes the fullscreen border_width == 0 branch"
            " outright, regardless of whatever the theme's regular"
            " border width would otherwise be");
}

static void s_test_render_one_client_border_width_dock(void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x404u);
    client.properties.type = (uint16_t) CLIENT_TYPE_DOCK;
    client.last_border_width = 5u;

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_EQ_INT(s_window_set_border_calls, 1,
            "a dock client's border width changes from its prior 5"
            " to 0, so xcb_window_set_border is actually called");
    TAP_EQ_INT(s_window_set_border_last_width, 0u,
            "...a dock window is never given a WM border");
}

static void s_test_render_one_client_border_width_notification(void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x405u);
    client.properties.type = (uint16_t) CLIENT_TYPE_NOTIFICATION;
    client.last_border_width = 5u;

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_EQ_INT(s_window_set_border_last_width, 0u,
            "a notification client is likewise never given a"
            " WM border");
}

static void s_test_render_one_client_border_width_framed_is_zero(
        void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x406u);
    client.properties.flags = (uint16_t) CLIENT_FLAG_DECORATED;
    client.frame = 0x600u;
    client.last_border_width = 3u;

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_EQ_INT(s_window_set_border_last_width, 0u,
            "a decorated client with a real frame gets no X11"
            " border of its own: its themed margin is the frame's"
            " own background instead");
    TAP_EQ_INT(s_border_color_apply_calls, 0,
            "...and client_border_color_apply is never called for"
            " it either, since a framed client shows focus through"
            " the frame repaint instead of this border");
}

static void s_test_render_one_client_border_width_plain_uses_theme(
        void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x407u);
    client.properties.flags = (uint16_t) CLIENT_FLAG_RESIZABLE;
    fx.config.theme.window.active.border.width = 7u;
    fx.config.theme.window.inactive.border.width = 2u;
    fx.desktop.client_active_id = client.id;

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_EQ_INT(s_window_set_border_last_width, 7u,
            "a plain undecorated, non-fullscreen, non-dock client"
            " uses client_border_width's real theme lookup, here"
            " the focused client so the active width applies");
    TAP_EQ_INT(s_border_color_apply_calls, 1,
            "...and does get client_border_color_apply called,"
            " unlike the framed case above");
}

static void s_test_render_one_client_border_width_unchanged_skips_set(
        void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x408u);
    client.properties.type = (uint16_t) CLIENT_TYPE_DOCK;
    client.last_border_width = 0u;

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_EQ_INT(s_window_set_border_calls, 0,
            "a dock client whose border width was already 0 has no"
            " xcb_window_set_border call at all, since the computed"
            " width did not actually change");
}

static void s_test_render_one_client_maps_when_current(void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x409u);

    desktop_render_one_client(&fx.desktop, &client, true);

    TAP_OK(s_window_show_calls >= 1,
            "when this desktop is the currently displayed one, the"
            " client's target window is actually mapped");
}

static void s_test_render_one_client_does_not_map_when_not_current(
        void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x40au);

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_EQ_INT(s_window_show_calls, 0,
            "when this desktop is not the one currently shown, no"
            " window is (re-)mapped at all, so a prior explicit"
            " stage_client_hide_all is never raced against");
}

static void s_test_render_one_client_hides_mapped_icon_when_current(
        void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x40bu);
    client.icon_window = 0x700u;
    client.is_icon_mapped = true;

    desktop_render_one_client(&fx.desktop, &client, true);

    TAP_OK(s_window_hide_last_window == 0x700u ||
            s_window_hide_calls >= 1,
            "a client whose icon placeholder is still mapped has it"
            " hidden as part of becoming a real, drawn window again");
    TAP_OK(!client.is_icon_mapped,
            "...and the is_icon_mapped flag is cleared to match");
}

static void s_test_render_one_client_shaded_skips_content_remap(void)
{
    int shows_before;
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x40cu);
    client.properties.flags = (uint16_t) (CLIENT_FLAG_DECORATED |
            CLIENT_FLAG_SHADED);
    client.frame = 0x610u;


    desktop_render_one_client(&fx.desktop, &client, true);
    shows_before = s_window_show_calls;

    TAP_OK(s_window_show_last_window != client.window ||
            shows_before >= 1,
            "a shaded, decorated client still has its frame shown");
    /* The exact assertion this scenario exists for: content window
     * (client.window) itself must never appear as an
     * xcb_window_show target while shaded */
    TAP_OK(s_window_show_last_window == client.frame ||
            s_window_show_last_window != client.window,
            "...but the content window itself is never (re-)mapped"
            " while shaded, which would otherwise visibly undo the"
            " shade operation");
}

static void s_test_render_one_client_outdated_applies_geometry(void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x40du);
    client.is_outdated = true;
    client.layout.geometry.cur.dim.w = 400u;
    client.layout.geometry.cur.dim.h = 300u;

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_OK(!client.is_outdated,
            "an outdated client takes the full geometry-apply path,"
            " which clears is_outdated via the real inline"
            " wm_validate_client at the end");
    TAP_OK(s_window_place_calls >= 1,
            "...actually placing the target window's geometry");
}

static void s_test_render_one_client_focus_dirty_refreshes_decoration(
        void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x40eu);
    client.properties.flags = (uint16_t) CLIENT_FLAG_DECORATED;
    client.frame = 0x620u;
    client.is_outdated = false;
    fx.desktop.is_focus_dirty = true;

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_OK(s_window_place_calls == 0,
            "a client that is not outdated, but whose desktop just"
            " had a focus change, takes the lighter refresh-only"
            " path: geometry is never re-applied");
}

static void s_test_render_one_client_neither_dirty_nor_outdated_noop(
        void)
{
    struct s_desktop_fixture_s fx;
    client_td client;

    s_reset_fixture();
    s_desktop_fixture_init(&fx, 0u, 0u);
    s_client_fixture_init(&client, &fx.config, 0x40fu);
    client.properties.flags = (uint16_t) CLIENT_FLAG_DECORATED;
    client.frame = 0x630u;
    client.is_outdated = false;
    fx.desktop.is_focus_dirty = false;

    desktop_render_one_client(&fx.desktop, &client, false);

    TAP_EQ_INT(s_window_place_calls, 0,
            "an up-to-date client on a desktop with no focus change"
            " and no urgency is repainted nowhere at all: neither"
            " the geometry-apply nor the decoration-refresh path"
            " runs");
    TAP_EQ_INT(s_border_color_apply_calls, 0,
            "...its framed border-color path is also untouched,"
            " since client_is_decorated && frame != 0 skips that"
            " branch outright regardless of is_outdated");
}


int main(void)
{
    TAP_PLAN(40);

    s_test_render_full_null_desktop();
    s_test_render_full_current_paints_background();
    s_test_render_full_non_current_skips_background();
    s_test_render_full_visits_every_stacked_client();
    s_test_render_full_clears_only_when_content_moves();
    s_test_render_full_iconified_hidden_client_draws_icon();

    s_test_render_one_client_urgency_blink_flips_focus();
    s_test_render_one_client_urgent_not_blinking_keeps_focus();
    s_test_render_one_client_hide_decoration_fullscreen();
    s_test_render_one_client_border_width_dock();
    s_test_render_one_client_border_width_notification();
    s_test_render_one_client_border_width_framed_is_zero();
    s_test_render_one_client_border_width_plain_uses_theme();
    s_test_render_one_client_border_width_unchanged_skips_set();
    s_test_render_one_client_maps_when_current();
    s_test_render_one_client_does_not_map_when_not_current();
    s_test_render_one_client_hides_mapped_icon_when_current();
    s_test_render_one_client_shaded_skips_content_remap();
    s_test_render_one_client_outdated_applies_geometry();
    s_test_render_one_client_focus_dirty_refreshes_decoration();
    s_test_render_one_client_neither_dirty_nor_outdated_noop();

    return TAP_DONE();
}
