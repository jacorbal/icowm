/**
 * @file tests/systray/test_layout.c
 *
 * @brief Test battery for systray positioning, stacking, and drawing
 *
 * s_tray (systray/internal.h) is the shared, module-level state this
 * whole file operates on; this test file owns the one real instance,
 * the same way test_text.c does.  Every XCB entry point layout.c
 * calls (both the raw xcb_* requests and this project's own thin
 * wrappers around them), plus text.c's renderer and the handful of
 * wm.h/surface.h lookups it makes, are stubbed below as controllable,
 * call-recording stand-ins, so every branch can be driven and checked
 * without a real X server or a real window manager around it.
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

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Local includes */
#include <client.h>
#include <desktop.h>
#include <harness/tap.h>
#include <logger.h>
#include <surface.h>
#include <systray/internal.h>


struct systray_state_s s_tray;


/** Shared open-addressed-hash-table callbacks for every fixture
 *  desktop's 'clients' table below, keying purely off 'client_td.id',
 *  matching the pattern already established by
 *  'tests/menu/context/test_winlist.c' */
static size_t s_id_hash1(const void *key)
{
    return (size_t) ((const client_td *) key)->id;
}

static size_t s_id_hash2(const void *key)
{
    (void) key;
    return 1u;
}

static bool s_id_match(const void *key1, const void *key2)
{
    return ((const client_td *) key1)->id ==
        ((const client_td *) key2)->id;
}


/* Controllable stand-in state */

static xcb_connection_t *s_connection_stub = (xcb_connection_t *) 1;
static xcb_ewmh_connection_t *s_ewmh_stub = NULL;

static int s_lower_calls = 0;
static int s_raise_calls = 0;
static int s_stack_below_calls = 0;
static xcb_window_t s_stack_below_window = XCB_WINDOW_NONE;
static xcb_window_t s_stack_below_sibling = XCB_WINDOW_NONE;
static int s_place_calls = 0;
static int32_t s_place_x = 0;
static int32_t s_place_y = 0;
static uint32_t s_place_w = 0u;
static uint32_t s_place_h = 0u;
static int s_show_calls = 0;
static int s_hide_calls = 0;
static int s_move_calls = 0;

static int s_strut_partial_calls = 0;
static int s_strut_calls = 0;
static xcb_ewmh_wm_strut_partial_t s_last_strut_partial;
static uint32_t s_last_strut_left = 0u;
static uint32_t s_last_strut_right = 0u;
static uint32_t s_last_strut_top = 0u;
static uint32_t s_last_strut_bottom = 0u;

static list_td *s_surfaces_stub = NULL;

static monitor_td s_primary_monitor_stub = { 0, 0, 0u, 0u };
static int s_refresh_workareas_calls = 0;

static int s_text_draw_calls = 0;
static int s_offscreen_buffer_calls = 0;
static xcb_pixmap_t s_offscreen_buffer_return = XCB_NONE;


/* 'utils/xcb/connection.h' stand-ins */

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection_stub;
}

xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return s_ewmh_stub;
}


/* 'utils/xcb/window.h' stand-ins */

void xcb_window_lower(xcb_window_t window)
{
    (void) window;
    s_lower_calls++;
}

void xcb_window_raise(xcb_window_t window)
{
    (void) window;
    s_raise_calls++;
}

void xcb_window_stack_below(xcb_window_t window, xcb_window_t sibling)
{
    s_stack_below_calls++;
    s_stack_below_window = window;
    s_stack_below_sibling = sibling;
}

void xcb_window_place(xcb_window_t window, int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    (void) window;
    s_place_calls++;
    s_place_x = x;
    s_place_y = y;
    s_place_w = width;
    s_place_h = height;
}

void xcb_window_move(xcb_window_t window, int32_t x, int32_t y)
{
    (void) window;
    (void) x;
    (void) y;
    s_move_calls++;
}

void xcb_window_show(xcb_window_t window)
{
    (void) window;
    s_show_calls++;
}

void xcb_window_hide(xcb_window_t window)
{
    (void) window;
    s_hide_calls++;
}


/* 'utils/xcb/pixmap.h' stand-in */

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
    return s_offscreen_buffer_return;
}


/* Raw XCB stand-ins (not linking libxcb at all) */

uint32_t xcb_generate_id(xcb_connection_t *connection)
{
    (void) connection;
    return 1u;
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
    return cookie;
}

xcb_void_cookie_t xcb_free_gc(xcb_connection_t *connection,
        xcb_gcontext_t gc)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) gc;
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
    return cookie;
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
    return cookie;
}

xcb_void_cookie_t xcb_free_pixmap(xcb_connection_t *connection,
        xcb_pixmap_t pixmap)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) pixmap;
    return cookie;
}

xcb_void_cookie_t xcb_ewmh_set_wm_strut_partial(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        xcb_ewmh_wm_strut_partial_t strut)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) ewmh;
    (void) window;
    s_strut_partial_calls++;
    s_last_strut_partial = strut;
    return cookie;
}

xcb_void_cookie_t xcb_ewmh_set_wm_strut(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t left, uint32_t right,
        uint32_t top, uint32_t bottom)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) ewmh;
    (void) window;
    s_strut_calls++;
    s_last_strut_left = left;
    s_last_strut_right = right;
    s_last_strut_top = top;
    s_last_strut_bottom = bottom;
    return cookie;
}


/* 'render/text.h' stand-ins */

int text_renderer_use_font(xcb_connection_t *connection,
        const char *font)
{
    (void) connection;
    (void) font;
    return 0;
}

void text_renderer_set_color(uint32_t foreground, uint32_t background)
{
    (void) foreground;
    (void) background;
}

int16_t text_font_ascent(void)
{
    return 10;
}

int16_t text_font_descent(void)
{
    return 2;
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
    s_text_draw_calls++;
}

/** Controllable stand-in: a fixed width per character, so the pen
 *  position math in tests exercising more than one text item stays
 *  simple and exact to hand-compute */
uint16_t text_string_measure(const char *text)
{
    return (uint16_t) (strlen(text) * 10u);
}


/* 'systray/text.c' stand-ins
 *
 * Deliberately simple, deterministic stand-ins independent of
 * whatever 'tests/systray/test_text.c' verifies about the real
 * implementation elsewhere: only 's_tray.clock_text' /
 * 's_tray.clock_enabled' and 's_tray.battery_text' /
 * 's_tray.battery_enabled' feed these, exactly like the real
 * functions, so a fixture only has to set those fields directly. */

uint16_t systray_text_width(void)
{
    uint16_t total = 0u;
    bool any = false;

    for (uint8_t i = 0u; i < s_tray.text_order_count; ++i) {
        bool enabled = false;
        const char *text = systray_text_for_item(s_tray.text_order[i],
                &enabled);

        if (!enabled || text[0] == '\0') {
            continue;
        }
        if (any) {
            total = (uint16_t) (total + s_tray.text_gap);
        }
        total = (uint16_t) (total + text_string_measure(text));
        any = true;
    }
    if (!any) {
        return 0u;
    }
    return (uint16_t) (total + 2u * s_tray.pixmap_pad);
}

const char *systray_text_for_item(enum config_systray_text_item_e item,
        bool *out_enabled)
{
    if (item == CONFIG_SYSTRAY_TEXT_BATTERY) {
        *out_enabled = s_tray.battery_enabled;
        return s_tray.battery_text;
    }
    *out_enabled = s_tray.clock_enabled;
    return s_tray.clock_text;
}


/* 'surface.h' / 'wm.h' stand-ins */

static desktop_td *s_desktop_get_stub = NULL;

desktop_td *surface_desktop_get(surface_td *surface,
        uint32_t desktop_id)
{
    (void) surface;
    (void) desktop_id;
    return s_desktop_get_stub;
}

monitor_td surface_monitor_primary(const surface_td *surface)
{
    (void) surface;
    return s_primary_monitor_stub;
}

void surface_workarea_refresh_all(surface_td *surface)
{
    (void) surface;
    s_refresh_workareas_calls++;
}

list_td *wm_get_surfaces(void)
{
    return s_surfaces_stub;
}

/** Stand-in for 'logger_msg', behind the 'LOGGER_WARNING' macro
 *  'layout.c' calls when an out-of-range monitor index falls back to
 *  monitor 0; only its call count is observable here */
static int s_logger_warning_calls = 0;

int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    s_logger_warning_calls++;
    return 0;
}


/* Fixture helpers */

/** Resets every stub call counter and recorded argument, without
 *  touching 's_tray' or any fixture object a test built itself */
static void s_reset_stub_state(void)
{
    s_connection_stub = (xcb_connection_t *) 1;
    s_ewmh_stub = NULL;
    s_lower_calls = 0;
    s_raise_calls = 0;
    s_stack_below_calls = 0;
    s_stack_below_window = XCB_WINDOW_NONE;
    s_stack_below_sibling = XCB_WINDOW_NONE;
    s_place_calls = 0;
    s_place_x = 0;
    s_place_y = 0;
    s_place_w = 0u;
    s_place_h = 0u;
    s_show_calls = 0;
    s_hide_calls = 0;
    s_move_calls = 0;
    s_strut_partial_calls = 0;
    s_strut_calls = 0;
    memset(&s_last_strut_partial, 0, sizeof(s_last_strut_partial));
    s_last_strut_left = 0u;
    s_last_strut_right = 0u;
    s_last_strut_top = 0u;
    s_last_strut_bottom = 0u;
    s_surfaces_stub = NULL;
    s_primary_monitor_stub = (monitor_td) { 0, 0, 0u, 0u };
    s_refresh_workareas_calls = 0;
    s_text_draw_calls = 0;
    s_offscreen_buffer_calls = 0;
    s_offscreen_buffer_return = XCB_NONE;
    s_desktop_get_stub = NULL;
    s_logger_warning_calls = 0;
}

/** A ready, active, empty tray sitting on a 1000x800 surface, docked
 *  top-left by default, with no theme (skips every text/pixmap path
 *  that reads 's_tray.theme'); callers fill in whatever else their
 *  own scenario needs on top of this */
static void s_reset_tray_minimal(surface_td *surface)
{
    memset(&s_tray, 0, sizeof(s_tray));
    s_tray.is_window_ready = true;
    s_tray.is_active = true;
    s_tray.surface = surface;
    s_tray.window = 500u;
    s_tray.height = 24u;
    s_tray.pixmap_size = 16u;
    s_tray.pixmap_pad = 4u;
}

/** One fake screen every fixture surface points to, needed only for
 *  its 'root_depth' field ('systray_layout_reflow''s offscreen
 *  buffer path reads 'surface->screen->root_depth' before even
 *  checking whether a buffer could be created) */
static xcb_screen_t s_fake_screen;

/** A bare surface_td fixture of the given combined dimensions, with
 *  no monitors and no desktops, sufficient for every reflow/restack
 *  scenario that does not itself need to walk desktops or monitors */
static surface_td s_make_surface(uint32_t w, uint32_t h)
{
    surface_td surface;

    memset(&s_fake_screen, 0, sizeof(s_fake_screen));
    memset(&surface, 0, sizeof(surface));
    surface.properties.dim.w = w;
    surface.properties.dim.h = h;
    surface.screen = &s_fake_screen;
    return surface;
}


/* ==================================================================== *
 * systray_layout_restack                                                *
 * ==================================================================== */

/* Neither ready nor connected: no stub is ever touched */
static void s_test_restack_not_ready(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.is_window_ready = false;

    systray_layout_restack();

    TAP_EQ_INT(s_lower_calls + s_raise_calls + s_stack_below_calls, 0,
            "not window-ready: restack touches no stacking stub");
}

static void s_test_restack_no_connection(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_connection_stub = NULL;

    systray_layout_restack();

    TAP_EQ_INT(s_lower_calls + s_raise_calls + s_stack_below_calls, 0,
            "no live connection: restack touches no stacking stub");
}

/* BELOW: lowers the window, clears 'stacked_against', and pushes
 * every mapped icon below the tray */
static void s_test_restack_below_layer(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.layer = CONFIG_SYSTRAY_LAYER_BELOW;
    s_surfaces_stub = NULL; /* s_systray_icon_push_below_all: no surfaces */

    systray_layout_restack();

    TAP_EQ_INT(s_lower_calls, 1, "BELOW layer lowers the tray window");
    TAP_EQ_INT((long) s_tray.stacked_against, (long) XCB_WINDOW_NONE,
            "BELOW layer records stacked_against as NONE");
    TAP_EQ_INT(s_tray.stacked_layer, CONFIG_SYSTRAY_LAYER_BELOW,
            "BELOW layer records its own layer as stacked_layer");
    TAP_OK(s_tray.is_stacking_known,
            "BELOW layer marks the stacking as now known");
}

/* BELOW layer with a real client/desktop/surface graph behind
 * 'wm_get_surfaces': every mapped icon window across every desktop of
 * every surface is pushed below the tray, and an unmapped icon (or a
 * client with no icon window at all) is left alone */
static void s_test_restack_below_pushes_mapped_icons(void)
{
    surface_td surface = s_make_surface(1000u, 800u);
    surface_td *other_surface;
    desktop_td desktop;
    client_td mapped_icon;
    client_td unmapped_icon;
    client_td no_icon_window;
    ohtbl_td *clients;
    cdlist_td *desktops;
    list_td *surfaces;

    memset(&desktop, 0, sizeof(desktop));
    memset(&mapped_icon, 0, sizeof(mapped_icon));
    memset(&unmapped_icon, 0, sizeof(unmapped_icon));
    memset(&no_icon_window, 0, sizeof(no_icon_window));

    mapped_icon.id = 10u;
    mapped_icon.is_icon_mapped = true;
    mapped_icon.icon_window = 910u;

    unmapped_icon.id = 11u;
    unmapped_icon.is_icon_mapped = false;
    unmapped_icon.icon_window = 911u;

    no_icon_window.id = 12u;
    no_icon_window.is_icon_mapped = true;
    no_icon_window.icon_window = 0u;

    clients = ohtbl_init(8u, 8u, s_id_hash1, s_id_hash2, s_id_match,
            NULL);
    ohtbl_insert(clients, &mapped_icon);
    ohtbl_insert(clients, &unmapped_icon);
    ohtbl_insert(clients, &no_icon_window);
    desktop.clients = clients;
    desktop.id = 0u;

    desktops = cdlist_init(NULL);
    cdlist_ins_next(desktops, NULL, &desktop);

    other_surface = calloc(1, sizeof(*other_surface));
    other_surface->desktops = desktops;

    surfaces = list_init(NULL);
    list_ins_next(surfaces, NULL, other_surface);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.layer = CONFIG_SYSTRAY_LAYER_BELOW;
    s_surfaces_stub = surfaces;

    systray_layout_restack();

    TAP_EQ_INT(s_stack_below_calls, 1,
            "exactly one mapped icon with a real icon window is"
            " pushed below the tray");
    TAP_EQ_INT((long) s_stack_below_window, (long) mapped_icon.icon_window,
            "...specifically the mapped icon's own icon window");
    TAP_EQ_INT((long) s_stack_below_sibling, (long) s_tray.window,
            "...stacked directly below the tray window itself");

    list_destroy(surfaces);
    cdlist_destroy(desktops);
    ohtbl_destroy(clients);
    free(other_surface);
}

/* BELOW, already stacked there: skipped entirely, no further lower */
static void s_test_restack_below_already_stacked(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.layer = CONFIG_SYSTRAY_LAYER_BELOW;
    s_tray.is_stacking_known = true;
    s_tray.stacked_layer = CONFIG_SYSTRAY_LAYER_BELOW;

    systray_layout_restack();

    TAP_EQ_INT(s_lower_calls, 0,
            "BELOW layer already settled: no redundant lower");
}

/* ABOVE, no fullscreen client anywhere: raises to the very top */
static void s_test_restack_above_no_fullscreen(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.layer = CONFIG_SYSTRAY_LAYER_ABOVE;
    /* surface->desktops == NULL: s_systray_fullscreen_target_find's
     * surface_desktop_get lookup below always yields no desktop */

    systray_layout_restack();

    TAP_EQ_INT(s_raise_calls, 1,
            "ABOVE with no fullscreen target raises the tray");
    TAP_EQ_INT((long) s_tray.stacked_against, (long) XCB_WINDOW_NONE,
            "ABOVE with no fullscreen target records NONE");
}

/* ABOVE, with a fullscreen client on the tray's current desktop:
 * stacks directly below that client's frame instead of raising */
static void s_test_restack_above_with_fullscreen(void)
{
    surface_td surface = s_make_surface(1000u, 800u);
    desktop_td desktop;
    client_td client;
    cdlist_td *desktops;
    ohtbl_td *clients;

    memset(&desktop, 0, sizeof(desktop));
    memset(&client, 0, sizeof(client));
    client.id = 1u;
    client.properties.state = CLIENT_STATE_FULLSCREEN;
    client.frame = 777u;
    client.window = 778u;

    clients = ohtbl_init(4u, 4u, s_id_hash1, s_id_hash2, s_id_match,
            NULL);
    ohtbl_insert(clients, &client);
    desktop.clients = clients;
    desktop.id = 0u;

    desktops = cdlist_init(NULL);
    cdlist_ins_next(desktops, NULL, &desktop);

    s_reset_stub_state();
    s_desktop_get_stub = &desktop;
    s_reset_tray_minimal(&surface);
    s_tray.surface->desktops = desktops;
    s_tray.surface->desktop_count = 1u;
    s_tray.surface->desktop_cur = 0u;
    s_tray.layer = CONFIG_SYSTRAY_LAYER_ABOVE;

    systray_layout_restack();

    TAP_EQ_INT(s_stack_below_calls, 1,
            "ABOVE with a fullscreen client stacks below it instead"
            " of raising");
    TAP_EQ_INT((long) s_stack_below_sibling, (long) client.frame,
            "...specifically below that client's own frame window");
    TAP_EQ_INT(s_raise_calls, 0,
            "...and never raises in that same call");

    cdlist_destroy(desktops);
    ohtbl_destroy(clients);
}

/* OVERLAY always raises unconditionally, fullscreen client or not,
 * since its fullscreen_target is never even looked up */
static void s_test_restack_overlay_always_raises(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.layer = CONFIG_SYSTRAY_LAYER_OVERLAY;

    systray_layout_restack();

    TAP_EQ_INT(s_raise_calls, 1,
            "OVERLAY layer always raises the tray to the top");
    TAP_EQ_INT(s_stack_below_calls, 0,
            "...never stacking below anything, fullscreen or not");
}

/* ABOVE/OVERLAY, already settled against the same target: skipped */
static void s_test_restack_above_already_settled(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.layer = CONFIG_SYSTRAY_LAYER_ABOVE;
    s_tray.is_stacking_known = true;
    s_tray.stacked_layer = CONFIG_SYSTRAY_LAYER_ABOVE;
    s_tray.stacked_against = XCB_WINDOW_NONE;

    systray_layout_restack();

    TAP_EQ_INT(s_raise_calls + s_stack_below_calls, 0,
            "ABOVE already settled against the same target: no-op");
}


/* Monitor anchor CONFIG_SYSTRAY_MONITOR_PRIMARY: the tray docks
 * against whatever 'surface_monitor_primary' reports, not the whole
 * combined surface */
static void s_test_reflow_anchor_primary_monitor(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 2u;
    s_tray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    s_tray.monitor.anchor = CONFIG_SYSTRAY_MONITOR_PRIMARY;
    s_primary_monitor_stub = (monitor_td) { 200, 50, 640u, 480u };

    systray_layout_reflow();

    TAP_EQ_INT(s_place_x, 200,
            "PRIMARY anchor: x comes from the primary monitor's own x");
    TAP_EQ_INT(s_place_y, 50,
            "PRIMARY anchor: y comes from the primary monitor's own y");
}

/* Monitor anchor CONFIG_SYSTRAY_MONITOR_INDEX, in range: docks
 * against that specific monitor's own rectangle */
static void s_test_reflow_anchor_index_in_range(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 2u;
    s_tray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    s_tray.monitor.anchor = CONFIG_SYSTRAY_MONITOR_INDEX;
    s_tray.monitor.index = 1u;
    s_tray.surface->monitor_count = 2u;
    s_tray.surface->monitors[0] = (monitor_td) { 0, 0, 500u, 800u };
    s_tray.surface->monitors[1] = (monitor_td) { 500, 0, 500u, 800u };

    systray_layout_reflow();

    TAP_EQ_INT(s_place_x, 500,
            "INDEX anchor in range: x comes from that exact monitor");
    TAP_EQ_INT(s_logger_warning_calls, 0,
            "...and logs no warning, since the index is valid");
}

/* Monitor anchor CONFIG_SYSTRAY_MONITOR_INDEX, out of range: falls
 * back to monitor 0 and logs a warning, rather than crashing or
 * silently docking nowhere */
static void s_test_reflow_anchor_index_out_of_range(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 2u;
    s_tray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    s_tray.monitor.anchor = CONFIG_SYSTRAY_MONITOR_INDEX;
    s_tray.monitor.index = 5u; /* only 2 monitors exist: out of range */
    s_tray.surface->monitor_count = 2u;
    s_tray.surface->monitors[0] = (monitor_td) { 10, 20, 500u, 800u };
    s_tray.surface->monitors[1] = (monitor_td) { 510, 0, 500u, 800u };

    systray_layout_reflow();

    TAP_EQ_INT(s_place_x, 10,
            "INDEX anchor out of range: falls back to monitor 0's x");
    TAP_EQ_INT(s_logger_warning_calls, 1,
            "...and logs exactly one warning about the bad index");
}

/* Monitor anchor CONFIG_SYSTRAY_MONITOR_INDEX with zero monitors at
 * all: falls back to the whole combined surface rectangle instead of
 * indexing into an empty array */
static void s_test_reflow_anchor_index_no_monitors(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 2u;
    s_tray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    s_tray.monitor.anchor = CONFIG_SYSTRAY_MONITOR_INDEX;
    s_tray.monitor.index = 0u;
    s_tray.surface->monitor_count = 0u;

    systray_layout_reflow();

    TAP_EQ_INT(s_place_x, 0,
            "INDEX anchor with no monitors: falls back to the whole"
            " surface's own x");
    TAP_EQ_INT(s_logger_warning_calls, 0,
            "...without logging a warning for this particular case");
}


/* ==================================================================== *
 * systray_layout_reflow                                                 *
 * ==================================================================== */

static void s_test_reflow_not_ready(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.is_window_ready = false;

    systray_layout_reflow();

    TAP_EQ_INT(s_hide_calls + s_place_calls, 0,
            "not window-ready: reflow does nothing at all");
}

static void s_test_reflow_no_surface(void)
{
    s_reset_stub_state();
    s_reset_tray_minimal(NULL);

    systray_layout_reflow();

    TAP_EQ_INT(s_hide_calls + s_place_calls, 0,
            "no surface assigned yet: reflow does nothing at all");
}

/* Inactive: hidden and its strut cleared, without even reaching the
 * width computation */
static void s_test_reflow_inactive_hides_and_clears_strut(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.is_active = false;
    s_tray.icon_count = 3u; /* would otherwise have nonzero width */

    systray_layout_reflow();

    TAP_EQ_INT(s_hide_calls, 1, "an inactive tray is hidden");
    TAP_EQ_INT(s_place_calls, 0,
            "...and never repositioned");
    TAP_EQ_INT((long) s_tray.reserved_strut.sides.top, 0,
            "...its reserved strut is cleared to all zero");
}

/* Active but empty (no icons, neither text item enabled): also
 * hidden, same as being inactive */
static void s_test_reflow_empty_hides(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 0u;
    s_tray.clock_enabled = false;
    s_tray.battery_enabled = false;

    systray_layout_reflow();

    TAP_EQ_INT(s_hide_calls, 1,
            "active but nothing to show: still hidden");
    TAP_EQ_INT(s_place_calls, 0, "...and never repositioned");
}

/* content_width() == 0 by way of a single disabled, empty text item
 * (rather than icon_count == 0) hits the exact same early hide path,
 * distinct from the is_active/empty-everything check above it */
static void s_test_reflow_zero_width_hides(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 0u;
    s_tray.clock_enabled = true;   /* enabled, but text stays empty */
    s_tray.clock_text[0] = '\0';

    systray_layout_reflow();

    TAP_EQ_INT(s_hide_calls, 1,
            "enabled but empty text still yields zero width: hidden");
}

/* TOP_LEFT: anchored flush to the surface's own top-left corner */
static void s_test_reflow_position_top_left(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 2u;
    s_tray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;

    systray_layout_reflow();

    TAP_EQ_INT(s_place_calls, 1, "a non-empty active tray is placed");
    TAP_EQ_INT(s_place_x, 0, "TOP_LEFT: x is the surface's own left");
    TAP_EQ_INT(s_place_y, 0, "TOP_LEFT: y is the surface's own top");
}

/* TOP_RIGHT: flush right, still against the top */
static void s_test_reflow_position_top_right(void)
{
    surface_td surface = s_make_surface(1000u, 800u);
    uint16_t expected_w;

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 2u;
    s_tray.position = CONFIG_SYSTRAY_POSITION_TOP_RIGHT;

    systray_layout_reflow();

    /* content width: pad + count*(size+pad) = 4 + 2*(16+4) = 44 */
    expected_w = 44u;
    TAP_EQ_INT(s_place_x, (long) (1000u - expected_w),
            "TOP_RIGHT: x sits content width in from the right edge");
    TAP_EQ_INT(s_place_y, 0, "TOP_RIGHT: y is still the surface's top");
}

/* BOTTOM_LEFT: flush left, against the bottom */
static void s_test_reflow_position_bottom_left(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 2u;
    s_tray.position = CONFIG_SYSTRAY_POSITION_BOTTOM_LEFT;

    systray_layout_reflow();

    TAP_EQ_INT(s_place_x, 0, "BOTTOM_LEFT: x is the surface's own left");
    TAP_EQ_INT(s_place_y, (long) (800u - s_tray.height),
            "BOTTOM_LEFT: y sits the tray's height up from the bottom");
}

/* BOTTOM_RIGHT, with a themed border: both edges pull in by the
 * total border thickness on top of the plain content/height span */
static void s_test_reflow_position_bottom_right_with_border(void)
{
    surface_td surface = s_make_surface(1000u, 800u);
    struct config_theme_s theme;
    uint16_t expected_w = 44u; /* 4 + 2*(16+4) */

    memset(&theme, 0, sizeof(theme));
    theme.systray.style.border.width = 3u;

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 2u;
    s_tray.position = CONFIG_SYSTRAY_POSITION_BOTTOM_RIGHT;
    s_tray.theme = &theme;

    systray_layout_reflow();

    TAP_EQ_INT(s_place_x, (long) (1000u - expected_w - 6u),
            "BOTTOM_RIGHT: x pulls in by content width plus 2x border");
    TAP_EQ_INT(s_place_y, (long) (800u - s_tray.height - 6u),
            "BOTTOM_RIGHT: y pulls up by height plus 2x border");
}

/* Icon positions: laid out left to right in a single row, each
 * exactly one pixmap-plus-padding stride apart, vertically centered
 * within the tray's own height */
static void s_test_reflow_icon_positions(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 3u;
    s_tray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;

    systray_layout_reflow();

    TAP_EQ_INT(s_move_calls, 3,
            "every docked icon is moved into position exactly once");
    TAP_EQ_INT(s_show_calls, 1, "a non-empty active tray is shown");
}

/* The strut is (re)published on every reflow that leaves the tray
 * visible, and 'surface_workarea_refresh_all' runs only when that strut
 * actually changed from what it was before */
static void s_test_reflow_strut_change_triggers_workarea_refresh(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 1u;
    s_tray.reserve_space = true;
    s_tray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;

    systray_layout_reflow();

    TAP_EQ_INT(s_strut_partial_calls, 0,
            "no ewmh connection available: strut is never published");
    TAP_EQ_INT(s_refresh_workareas_calls, 1,
            "the strut changed from all-zero: workareas are refreshed");

    /* Reflowing again with nothing changed: same strut, no refresh */
    s_refresh_workareas_calls = 0;
    systray_layout_reflow();
    TAP_EQ_INT(s_refresh_workareas_calls, 0,
            "reflowing again with an unchanged strut: no refresh");
}

/* With 'reserve_space' on, an ewmh connection, and a TOP_LEFT dock,
 * the published strut's top edge equals the tray's own bottom edge */
static void s_test_reflow_strut_values_top_left(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 1u;
    s_tray.reserve_space = true;
    s_tray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    s_ewmh_stub = (xcb_ewmh_connection_t *) 1;

    systray_layout_reflow();

    TAP_EQ_INT(s_strut_partial_calls, 1,
            "an ewmh connection is available: the strut is published");
    TAP_EQ_INT((long) s_last_strut_top, (long) s_tray.height,
            "TOP_LEFT reserves exactly the tray's own height at top");
    TAP_EQ_INT((long) s_last_strut_bottom, 0,
            "...and nothing at all on the opposite edge");
}

/* With the text block enabled and drawn straight to the window (no
 * offscreen buffer), every enabled/non-empty text item is drawn
 * exactly once, and a disabled or empty one is skipped */
static void s_test_reflow_draws_enabled_text_only(void)
{
    surface_td surface = s_make_surface(1000u, 800u);
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 0u;
    s_tray.theme = &theme;
    s_tray.text_order[0] = CONFIG_SYSTRAY_TEXT_CLOCK;
    s_tray.text_order[1] = CONFIG_SYSTRAY_TEXT_BATTERY;
    s_tray.text_order_count = 2u;
    strcpy(s_tray.clock_text, "10:30");
    s_tray.clock_enabled = true;
    s_tray.battery_enabled = false; /* skipped: disabled */
    s_offscreen_buffer_return = XCB_NONE; /* draws straight to window */

    systray_layout_reflow();

    TAP_EQ_INT(s_text_draw_calls, 1,
            "only the one enabled, non-empty text item is drawn");
}

/* When the offscreen buffer path succeeds, the drawn block is copied
 * onto the tray window via 'xcb_copy_area' (a second gc alloc), and
 * the buffer is freed afterward; not directly observable through a
 * dedicated counter, so this exercises the path for crash/leak/ASan
 * safety and checks the text is still drawn exactly once */
static void s_test_reflow_draws_text_via_offscreen_buffer(void)
{
    surface_td surface = s_make_surface(1000u, 800u);
    struct config_theme_s theme;

    memset(&theme, 0, sizeof(theme));

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 0u;
    s_tray.theme = &theme;
    s_tray.text_order[0] = CONFIG_SYSTRAY_TEXT_CLOCK;
    s_tray.text_order_count = 1u;
    strcpy(s_tray.clock_text, "12:00");
    s_tray.clock_enabled = true;
    s_offscreen_buffer_return = 42u; /* a real-looking pixmap id */

    systray_layout_reflow();

    TAP_EQ_INT(s_offscreen_buffer_calls, 1,
            "the offscreen buffer path is attempted when it can be");
    TAP_EQ_INT(s_text_draw_calls, 1,
            "the text is still drawn exactly once via that buffer");
}

/* 'systray_layout_reflow' restacks at the end of every call that gets
 * past the early-hide checks, whatever the layer */
static void s_test_reflow_restacks_at_the_end(void)
{
    surface_td surface = s_make_surface(1000u, 800u);

    s_reset_stub_state();
    s_reset_tray_minimal(&surface);
    s_tray.icon_count = 1u;
    s_tray.layer = CONFIG_SYSTRAY_LAYER_BELOW;

    systray_layout_reflow();

    TAP_EQ_INT(s_lower_calls, 1,
            "reflow ends by restacking per the configured layer");
}


int main(void)
{
    TAP_PLAN(55);

    s_test_restack_not_ready();
    s_test_restack_no_connection();
    s_test_restack_below_layer();
    s_test_restack_below_pushes_mapped_icons();
    s_test_restack_below_already_stacked();
    s_test_restack_above_no_fullscreen();
    s_test_restack_above_with_fullscreen();
    s_test_restack_overlay_always_raises();
    s_test_restack_above_already_settled();

    s_test_reflow_not_ready();
    s_test_reflow_no_surface();
    s_test_reflow_inactive_hides_and_clears_strut();
    s_test_reflow_empty_hides();
    s_test_reflow_zero_width_hides();
    s_test_reflow_position_top_left();
    s_test_reflow_position_top_right();
    s_test_reflow_position_bottom_left();
    s_test_reflow_position_bottom_right_with_border();
    s_test_reflow_icon_positions();
    s_test_reflow_anchor_primary_monitor();
    s_test_reflow_anchor_index_in_range();
    s_test_reflow_anchor_index_out_of_range();
    s_test_reflow_anchor_index_no_monitors();
    s_test_reflow_strut_change_triggers_workarea_refresh();
    s_test_reflow_strut_values_top_left();
    s_test_reflow_draws_enabled_text_only();
    s_test_reflow_draws_text_via_offscreen_buffer();
    s_test_reflow_restacks_at_the_end();

    return TAP_DONE();
}
