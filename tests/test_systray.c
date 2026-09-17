/**
 * @file tests/test_systray.c
 *
 * @brief Test battery for the built-in systray dock's shared state and
 *        public event dispatchers
 *
 * Exercises 'src/systray.c' linked for real, which itself defines the
 * one shared 's_tray' instance every function under test reads and
 * writes; this file only ever resets its fields between scenarios,
 * through the 'extern' declaration in 'systray/internal.h', rather
 * than declaring a second, colliding definition of its own the way
 * 'tests/systray/test_layout.c' does for 'layout.c' (which does not
 * itself define 's_tray').  Every function this file's target calls
 * out of 'systray/protocol.c', 'systray/layout.c', and 'systray/
 * text.c' is a controllable, call-recording stand-in below, since
 * those are separate modules under test elsewhere in this same round;
 * likewise every raw XCB entry point and this project's own
 * 'utils/xcb/window.h' wrappers, so no X server or extra library is
 * needed.
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <stdlib.h>     /* calloc */
#include <string.h>     /* memset */

/* ADT includes */
#include <adt/list.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <config.h>
#include <harness/tap.h>
#include <logger.h>
#include <stage.h>
#include <systray/handle.h>
#include <systray/icon.h>
#include <systray/internal.h>
#include <wm.h>


/** A non-NULL opaque handle standing in for a real wm_td, which this
 *  file never actually builds, since the type is opaque outside wm.c
 *  itself; wm_config/wm_connection/wm_stages below never dereference
 *  it, only ignore it and return their own fixtures, so a dummy
 *  address is enough, the same pattern tests/rules/test_apply.c and
 *  tests/wm/test_actions.c already use */
static int s_fake_wm_storage;
static wm_td *const s_fake_wm = (wm_td *) &s_fake_wm_storage;

/** The config_td the wm_config stand-in hands back, rebuilt fresh by
 *  s_reset_config before each scenario that needs one */
static config_td s_config;

/** A fixture stage, only ever compared by address against
 *  's_tray.stage', never dereferenced by anything under test here */
static stage_td s_fixture_stage;


/* Controllable stand-in state */

static xcb_connection_t *s_connection_stub = (xcb_connection_t *) 1;

static int s_window_hide_calls = 0;
static int s_window_show_calls = 0;
static int s_window_destroy_calls = 0;
static xcb_window_t s_window_destroy_last_window = XCB_WINDOW_NONE;

static int s_configure_window_calls = 0;
static xcb_window_t s_configure_window_last_window = XCB_WINDOW_NONE;
static uint16_t s_configure_window_last_mask = 0u;

static int s_flush_calls = 0;

static int s_get_geometry_calls = 0;
static bool s_get_geometry_reply_is_null = false;
static int16_t s_get_geometry_x = 0;
static int16_t s_get_geometry_y = 0;
static uint16_t s_get_geometry_w = 0u;
static uint16_t s_get_geometry_h = 0u;

static int s_refresh_clock_calls = 0;
static int s_refresh_battery_calls = 0;

static int s_window_ensure_calls = 0;
static bool s_window_ensure_result = true;

static int s_selection_acquire_calls = 0;
static bool s_selection_acquire_result = true;

static int s_selection_release_calls = 0;

static int s_protocol_dock_calls = 0;
static xcb_window_t s_protocol_dock_last_icon = XCB_WINDOW_NONE;

static int s_protocol_property_changed_calls = 0;
static xcb_window_t s_protocol_property_changed_last_window =
    XCB_WINDOW_NONE;
static xcb_atom_t s_protocol_property_changed_last_atom =
    XCB_ATOM_NONE;

static int s_protocol_map_request_calls = 0;
static bool s_protocol_map_request_result = true;

static int s_protocol_apply_theme_style_calls = 0;
static int s_protocol_resort_calls = 0;

static int s_layout_reflow_calls = 0;
static int s_layout_restack_calls = 0;


/* 'wm.h' stand-ins */

config_td *wm_config(const wm_td *wm)
{
    (void) wm;
    return &s_config;
}

xcb_connection_t *wm_connection(const wm_td *wm)
{
    (void) wm;
    return s_connection_stub;
}

list_td *wm_stages(const wm_td *wm)
{
    (void) wm;
    return NULL;
}


/* 'utils/xcb/connection.h' stand-in */

xcb_connection_t *xcb_connection_get(void)
{
    return s_connection_stub;
}


/* 'utils/xcb/window.h' stand-ins */

void xcb_window_hide(xcb_window_t window)
{
    (void) window;
    s_window_hide_calls++;
}

void xcb_window_show(xcb_window_t window)
{
    (void) window;
    s_window_show_calls++;
}

void xcb_window_destroy(xcb_window_t window)
{
    s_window_destroy_calls++;
    s_window_destroy_last_window = window;
}


/* Raw XCB stand-ins (not linking libxcb at all) */

xcb_void_cookie_t xcb_configure_window(xcb_connection_t *connection,
        xcb_window_t window, uint16_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0u };

    (void) connection;
    (void) value_list;
    s_configure_window_calls++;
    s_configure_window_last_window = window;
    s_configure_window_last_mask = value_mask;
    return cookie;
}

int xcb_flush(xcb_connection_t *connection)
{
    (void) connection;
    s_flush_calls++;
    return 1;
}

xcb_get_geometry_cookie_t xcb_get_geometry(xcb_connection_t *connection,
        xcb_drawable_t drawable)
{
    xcb_get_geometry_cookie_t cookie = { 0u };

    (void) connection;
    (void) drawable;
    s_get_geometry_calls++;
    return cookie;
}

xcb_get_geometry_reply_t *xcb_get_geometry_reply(
        xcb_connection_t *connection, xcb_get_geometry_cookie_t cookie,
        xcb_generic_error_t **e)
{
    xcb_get_geometry_reply_t *reply;

    (void) connection;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }

    if (s_get_geometry_reply_is_null) {
        return NULL;
    }

    reply = calloc(1, sizeof(*reply));
    reply->x = s_get_geometry_x;
    reply->y = s_get_geometry_y;
    reply->width = s_get_geometry_w;
    reply->height = s_get_geometry_h;
    return reply;
}


/* 'systray/text.c' stand-ins */

void systray_text_refresh_clock(void)
{
    s_refresh_clock_calls++;
}

void systray_text_refresh_battery(void)
{
    s_refresh_battery_calls++;
}

uint16_t systray_text_width(void)
{
    return 0u;
}

const char *systray_text_for_item(enum config_systray_text_item_e item,
        bool *out_enabled)
{
    (void) item;
    if (out_enabled != NULL) {
        *out_enabled = false;
    }
    return "";
}


/* 'systray/layout.c' stand-ins */

void systray_layout_reflow(void)
{
    s_layout_reflow_calls++;
}

void systray_layout_restack(void)
{
    s_layout_restack_calls++;
}


/* 'systray/protocol.c' stand-ins */

bool systray_protocol_window_ensure(const wm_td *wm)
{
    (void) wm;
    s_window_ensure_calls++;
    return s_window_ensure_result;
}

bool systray_protocol_selection_acquire(void)
{
    s_selection_acquire_calls++;
    return s_selection_acquire_result;
}

void systray_protocol_selection_release(void)
{
    s_selection_release_calls++;
}

void systray_protocol_dock(xcb_window_t icon)
{
    s_protocol_dock_calls++;
    s_protocol_dock_last_icon = icon;
}

void systray_protocol_property_changed(xcb_window_t window,
        xcb_atom_t atom)
{
    s_protocol_property_changed_calls++;
    s_protocol_property_changed_last_window = window;
    s_protocol_property_changed_last_atom = atom;
}

bool systray_protocol_map_request(xcb_window_t window)
{
    (void) window;
    s_protocol_map_request_calls++;
    return s_protocol_map_request_result;
}

void systray_protocol_apply_theme_style(void)
{
    s_protocol_apply_theme_style_calls++;
}

void systray_protocol_resort(void)
{
    s_protocol_resort_calls++;
}


/* Link-only stand-in for 'logger_msg', behind every 'LOGGER_*' call
 * this file's target makes; no test here asserts on log output */
int logger_msg(enum logger_level_e level, const char *prefix,
        const char *fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/* Fixture helpers */

/** Fills 's_config' with a plain, systray-enabled configuration every
 *  scenario can start from and then adjust just the fields it cares
 *  about */
static void s_reset_config(void)
{
    memset(&s_config, 0, sizeof(s_config));
    s_config.base.systray.is_enabled = true;
    s_config.base.systray.is_embedding_enabled = true;
    s_config.base.systray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    s_config.base.systray.order = CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT;
    s_config.base.systray.layer = CONFIG_SYSTRAY_LAYER_BELOW;
    s_config.theme.systray.pixmap.size = 24u;
    s_config.theme.systray.pixmap.padding = 4u;
    s_config.theme.systray.height = 28u;
}

/** Resets every stub call counter and recorded argument, and zeroes
 *  the real, module-level 's_tray' this file links for real, without
 *  touching 's_config' (each scenario builds that itself) */
static void s_reset(void)
{
    memset(&s_tray, 0, sizeof(s_tray));

    s_connection_stub = (xcb_connection_t *) 1;
    s_window_hide_calls = 0;
    s_window_show_calls = 0;
    s_window_destroy_calls = 0;
    s_window_destroy_last_window = XCB_WINDOW_NONE;
    s_configure_window_calls = 0;
    s_configure_window_last_window = XCB_WINDOW_NONE;
    s_configure_window_last_mask = 0u;
    s_flush_calls = 0;
    s_get_geometry_calls = 0;
    s_get_geometry_reply_is_null = false;
    s_get_geometry_x = 0;
    s_get_geometry_y = 0;
    s_get_geometry_w = 0u;
    s_get_geometry_h = 0u;
    s_refresh_clock_calls = 0;
    s_refresh_battery_calls = 0;
    s_window_ensure_calls = 0;
    s_window_ensure_result = true;
    s_selection_acquire_calls = 0;
    s_selection_acquire_result = true;
    s_selection_release_calls = 0;
    s_protocol_dock_calls = 0;
    s_protocol_dock_last_icon = XCB_WINDOW_NONE;
    s_protocol_property_changed_calls = 0;
    s_protocol_property_changed_last_window = XCB_WINDOW_NONE;
    s_protocol_property_changed_last_atom = XCB_ATOM_NONE;
    s_protocol_map_request_calls = 0;
    s_protocol_map_request_result = true;
    s_protocol_apply_theme_style_calls = 0;
    s_protocol_resort_calls = 0;
    s_layout_reflow_calls = 0;
    s_layout_restack_calls = 0;
}


/* 'systray_init' is a no-op for a NULL wm, a NULL config, or a config
 * with the tray disabled; none of these ever touch the window-ensure
 * or config-copy path */
static void s_test_init_guards_refuse_cleanly(void)
{
    s_reset();
    s_reset_config();

    systray_init(NULL);
    TAP_EQ_INT(s_window_ensure_calls, 0,
            "a NULL wm never reaches the window-ensure step");

    s_config.base.systray.is_enabled = false;
    systray_init(s_fake_wm);
    TAP_EQ_INT(s_window_ensure_calls, 0,
            "a disabled systray never reaches the window-ensure step"
            " either");
    TAP_EQ_INT(s_refresh_clock_calls, 0,
            "nor does it copy any configuration into s_tray");
}


/* A fully enabled init copies configuration fields, ensures the
 * window, becomes active, acquires the selection (embedding is on),
 * and reflows */
static void s_test_init_enabled_runs_the_full_sequence(void)
{
    s_reset();
    s_reset_config();
    s_config.base.systray.position =
        CONFIG_SYSTRAY_POSITION_BOTTOM_RIGHT;
    s_config.theme.systray.pixmap.size = 32u;

    systray_init(s_fake_wm);

    TAP_EQ_INT((long) s_tray.position,
            (long) CONFIG_SYSTRAY_POSITION_BOTTOM_RIGHT,
            "the configured position is copied into s_tray");
    TAP_EQ_INT((long) s_tray.pixmap_size, 32,
            "as is the configured pixmap size");
    TAP_EQ_INT(s_refresh_clock_calls, 1,
            "the clock text is refreshed exactly once during init");
    TAP_EQ_INT(s_refresh_battery_calls, 1,
            "and so is the battery text");
    TAP_EQ_INT(s_window_ensure_calls, 1,
            "the tray window is ensured exactly once");
    TAP_OK(s_tray.is_active,
            "the tray becomes active once the window is ready");
    TAP_EQ_INT(s_selection_acquire_calls, 1,
            "the selection is acquired since embedding is enabled");
    TAP_EQ_INT(s_layout_reflow_calls, 1,
            "and the layout is reflowed exactly once at the end");
}


/* When the window cannot be ensured, init stops right there: never
 * becomes active, never touches the selection, never reflows */
static void s_test_init_stops_if_window_ensure_fails(void)
{
    s_reset();
    s_reset_config();
    s_window_ensure_result = false;

    systray_init(s_fake_wm);

    TAP_OK(!s_tray.is_active,
            "the tray never becomes active when the window could not"
            " be ensured");
    TAP_EQ_INT(s_selection_acquire_calls, 0,
            "the selection is never even attempted");
    TAP_EQ_INT(s_layout_reflow_calls, 0,
            "and the layout is never reflowed");
}


/* With embedding disabled, init still ensures the window and reflows,
 * but never touches the selection at all */
static void s_test_init_skips_selection_when_embedding_disabled(void)
{
    s_reset();
    s_reset_config();
    s_config.base.systray.is_embedding_enabled = false;

    systray_init(s_fake_wm);

    TAP_OK(s_tray.is_active, "the tray still becomes active");
    TAP_EQ_INT(s_selection_acquire_calls, 0,
            "but the selection is never acquired with embedding"
            " disabled");
    TAP_EQ_INT(s_layout_reflow_calls, 1,
            "the layout is still reflowed regardless");
}


/* Shutdown with no window ready releases the selection but does
 * nothing else, and still zeroes 's_tray' at the end */
static void s_test_shutdown_with_no_window_only_releases_selection(
        void)
{
    s_reset();
    s_tray.is_window_ready = false;

    systray_shutdown(s_fake_wm);

    TAP_EQ_INT(s_selection_release_calls, 1,
            "the selection is released even with no window ready");
    TAP_EQ_INT(s_window_destroy_calls, 0,
            "but no window is destroyed since none was ready");
    TAP_EQ_INT(s_tray.window, (xcb_window_t) XCB_WINDOW_NONE,
            "s_tray is left fully zeroed afterward");
}


/* Shutdown with a ready window and a real connection destroys the
 * window and flushes, then still zeroes s_tray */
static void s_test_shutdown_with_window_destroys_it(void)
{
    s_reset();
    s_tray.is_window_ready = true;
    s_tray.window = (xcb_window_t) 555u;

    systray_shutdown(s_fake_wm);

    TAP_EQ_INT(s_window_destroy_calls, 1,
            "a ready window is destroyed exactly once");
    TAP_EQ_INT((long) s_window_destroy_last_window, 555,
            "the exact tray window is the one destroyed");
    TAP_EQ_INT(s_flush_calls, 1,
            "the connection is flushed exactly once after destroying"
            " it");
    TAP_OK(s_tray.window == XCB_WINDOW_NONE,
            "s_tray is fully zeroed after shutdown");
}


/* Shutdown never destroys a window when the connection is NULL, even
 * if is_window_ready is true */
static void s_test_shutdown_skips_destroy_with_null_connection(void)
{
    s_reset();
    s_tray.is_window_ready = true;
    s_tray.window = (xcb_window_t) 555u;
    s_connection_stub = NULL;

    systray_shutdown(s_fake_wm);

    TAP_EQ_INT(s_window_destroy_calls, 0,
            "no window is destroyed when there is no connection to"
            " destroy it on");
    s_connection_stub = (xcb_connection_t *) 1;
}


/* 'systray_owns_window' is a straightforward three-way guard */
static void s_test_owns_window_guards(void)
{
    s_reset();
    TAP_OK(!systray_owns_window((xcb_window_t) 10u),
            "not ready yet: never owns any window");

    s_tray.is_window_ready = true;
    s_tray.window = (xcb_window_t) 10u;
    TAP_OK(!systray_owns_window(XCB_WINDOW_NONE),
            "XCB_WINDOW_NONE is never owned even if 's_tray.window'"
            " happens to be nonzero");
    TAP_OK(!systray_owns_window((xcb_window_t) 11u),
            "a different window is not owned");
    TAP_OK(systray_owns_window((xcb_window_t) 10u),
            "the exact tray window, once ready, is owned");
}


/* 'systray_below_window' only ever answers with the tray window when
 * it is ready, active, and in the 'below' layer */
static void s_test_below_window_guards(void)
{
    s_reset();
    s_tray.window = (xcb_window_t) 20u;

    TAP_EQ_INT((long) systray_below_window(), (long) XCB_WINDOW_NONE,
            "not ready: XCB_WINDOW_NONE");

    s_tray.is_window_ready = true;
    TAP_EQ_INT((long) systray_below_window(), (long) XCB_WINDOW_NONE,
            "ready but not active: still XCB_WINDOW_NONE");

    s_tray.is_active = true;
    s_tray.layer = CONFIG_SYSTRAY_LAYER_ABOVE;
    TAP_EQ_INT((long) systray_below_window(), (long) XCB_WINDOW_NONE,
            "ready and active but in the 'above' layer: still"
            " XCB_WINDOW_NONE");

    s_tray.layer = CONFIG_SYSTRAY_LAYER_BELOW;
    TAP_EQ_INT((long) systray_below_window(), 20,
            "ready, active, and in the 'below' layer: the tray window"
            " itself");
}


/* 'systray_get_reserved_strut' only answers for the exact stage the
 * tray is docked on, and only once the window is ready */
static void s_test_get_reserved_strut_guards(void)
{
    const struct strut_partial_s *strut;

    s_reset();
    s_tray.stage = &s_fixture_stage;
    s_tray.reserved_strut.sides.left = 7;

    TAP_OK(systray_get_reserved_strut(NULL) == NULL,
            "a NULL stage never gets a strut back");

    TAP_OK(systray_get_reserved_strut(&s_fixture_stage) == NULL,
            "not window-ready yet: NULL even for the right stage");

    s_tray.is_window_ready = true;
    TAP_OK(systray_get_reserved_strut(&s_fixture_stage) ==
            &s_tray.reserved_strut,
            "window-ready and the right stage: the real strut"
            " pointer");

    strut = systray_get_reserved_strut(&s_fixture_stage);
    TAP_EQ_INT(strut->sides.left, 7,
            "pointing at the tray's actual current reservation");

    TAP_OK(systray_get_reserved_strut((stage_td *) 0x9999) == NULL,
            "a different stage than the one docked on gets NULL");
}


/* 'systray_get_geometry' guards on stage/output pointers and state,
 * then performs a real round trip through the stubbed XCB calls */
static void s_test_get_geometry(void)
{
    struct geometry_s out;
    bool ok;

    s_reset();
    s_tray.stage = &s_fixture_stage;
    s_tray.is_window_ready = true;
    s_tray.is_active = true;
    s_tray.window = (xcb_window_t) 30u;

    TAP_OK(!systray_get_geometry(NULL, &out),
            "a NULL stage is refused");
    TAP_OK(!systray_get_geometry(&s_fixture_stage, NULL),
            "a NULL output pointer is refused");
    TAP_OK(!systray_get_geometry((stage_td *) 0x9999, &out),
            "a stage other than the one docked on is refused");
    TAP_EQ_INT(s_get_geometry_calls, 0,
            "none of the refusals above ever reach the X server");

    s_get_geometry_x = 5;
    s_get_geometry_y = 6;
    s_get_geometry_w = 200u;
    s_get_geometry_h = 40u;
    ok = systray_get_geometry(&s_fixture_stage, &out);

    TAP_OK(ok, "a ready, active tray on the right stage succeeds");
    TAP_EQ_INT(s_get_geometry_calls, 1,
            "exactly one round trip is made");
    TAP_EQ_INT(out.pos.x, 5, "the reply's x coordinate is copied");
    TAP_EQ_INT(out.pos.y, 6, "the reply's y coordinate is copied");
    TAP_EQ_INT((long) out.dim.w, 200,
            "the reply's width is copied");
    TAP_EQ_INT((long) out.dim.h, 40,
            "the reply's height is copied");

    s_get_geometry_reply_is_null = true;
    ok = systray_get_geometry(&s_fixture_stage, &out);
    TAP_OK(!ok, "a NULL reply (failed round trip) reports failure");
}


/* 'systray_icon_size_enforce' finds a docked icon by window and
 * forces its size; a window that is not docked is refused */
static void s_test_icon_size_enforce(void)
{
    bool result;

    s_reset();
    s_tray.icon_count = 2u;
    s_tray.icons[0].window = (xcb_window_t) 41u;
    s_tray.icons[1].window = (xcb_window_t) 42u;
    s_tray.pixmap_size = 24u;

    TAP_OK(!systray_icon_size_enforce(XCB_WINDOW_NONE),
            "XCB_WINDOW_NONE is refused outright");
    TAP_OK(!systray_icon_size_enforce((xcb_window_t) 999u),
            "a window that is not docked is refused");
    TAP_EQ_INT(s_configure_window_calls, 0,
            "neither refusal above configures anything");

    result = systray_icon_size_enforce((xcb_window_t) 42u);
    TAP_OK(result, "a docked window is found and enforced");
    TAP_EQ_INT((long) s_configure_window_last_window, 42,
            "the exact docked window is the one configured");
    TAP_EQ_INT(s_configure_window_calls, 1,
            "exactly one configure request is made");
    TAP_EQ_INT(s_flush_calls, 1,
            "and the connection is flushed once, to apply it"
            " immediately");
}


/* 'systray_icon_map_request' is a pure delegate to the protocol
 * module's own function */
static void s_test_icon_map_request_delegates(void)
{
    bool result;

    s_reset();
    s_protocol_map_request_result = true;

    result = systray_icon_map_request((xcb_window_t) 77u);

    TAP_OK(result, "the protocol module's own answer is returned"
            " as-is");
    TAP_EQ_INT(s_protocol_map_request_calls, 1,
            "the protocol module is asked exactly once");
}


/* 'systray_handle_client_message' guards on a NULL event, an
 * unowned selection, the wrong window, and the wrong atom, and only
 * docks on the exact 'REQUEST_DOCK' opcode */
static void s_test_handle_client_message(void)
{
    xcb_client_message_event_t event;

    s_reset();
    s_tray.is_selection_owned = true;
    s_tray.window = (xcb_window_t) 90u;
    s_tray.opcode_atom = (xcb_atom_t) 500u;

    systray_handle_client_message(s_fake_wm, NULL);
    TAP_EQ_INT(s_protocol_dock_calls, 0,
            "a NULL event never reaches the dock dispatch");

    memset(&event, 0, sizeof(event));
    event.window = (xcb_window_t) 90u;
    event.type = (xcb_atom_t) 500u;
    event.data.data32[1] = SYSTRAY_OPCODE_REQUEST_DOCK;
    event.data.data32[2] = 123u;

    s_tray.is_selection_owned = false;
    systray_handle_client_message(s_fake_wm, &event);
    TAP_EQ_INT(s_protocol_dock_calls, 0,
            "nothing is dispatched while the selection is not owned");

    s_tray.is_selection_owned = true;
    event.window = (xcb_window_t) 999u;
    systray_handle_client_message(s_fake_wm, &event);
    TAP_EQ_INT(s_protocol_dock_calls, 0,
            "an event for a different window is ignored");

    event.window = (xcb_window_t) 90u;
    event.type = (xcb_atom_t) 999u;
    systray_handle_client_message(s_fake_wm, &event);
    TAP_EQ_INT(s_protocol_dock_calls, 0,
            "an event with the wrong opcode atom is ignored");

    event.type = (xcb_atom_t) 500u;
    systray_handle_client_message(s_fake_wm, &event);
    TAP_EQ_INT(s_protocol_dock_calls, 1,
            "a well-formed REQUEST_DOCK message is dispatched exactly"
            " once");
    TAP_EQ_INT((long) s_protocol_dock_last_icon, 123,
            "naming the icon window carried in data32[2]");
}


/* 'systray_handle_destroy' removes exactly the matching entry (or
 * entries), shifting the rest down, and always reflows afterward */
static void s_test_handle_destroy_removes_and_shifts(void)
{
    s_reset();
    s_tray.is_window_ready = true;
    s_tray.icon_count = 3u;
    s_tray.icons[0].window = (xcb_window_t) 1u;
    s_tray.icons[1].window = (xcb_window_t) 2u;
    s_tray.icons[2].window = (xcb_window_t) 3u;

    systray_handle_destroy(s_fake_wm, (xcb_window_t) 2u);

    TAP_EQ_INT((long) s_tray.icon_count, 2,
            "the icon count drops by exactly one");
    TAP_EQ_INT((long) s_tray.icons[0].window, 1,
            "the first surviving icon is unchanged");
    TAP_EQ_INT((long) s_tray.icons[1].window, 3,
            "and the one after the removed entry shifted down into"
            " its place");
    TAP_EQ_INT(s_layout_reflow_calls, 1,
            "the layout is reflowed exactly once after the removal");

    systray_handle_destroy(s_fake_wm, (xcb_window_t) 999u);
    TAP_EQ_INT((long) s_tray.icon_count, 2,
            "destroying a window that was never docked leaves the"
            " count unchanged");
    TAP_EQ_INT(s_layout_reflow_calls, 2,
            "but the reflow is still called unconditionally");
}


/* 'systray_handle_destroy' is a no-op (aside from never reflowing)
 * when the tray window itself is not ready */
static void s_test_handle_destroy_noop_when_not_ready(void)
{
    s_reset();
    s_tray.is_window_ready = false;
    s_tray.icon_count = 1u;
    s_tray.icons[0].window = (xcb_window_t) 1u;

    systray_handle_destroy(s_fake_wm, (xcb_window_t) 1u);

    TAP_EQ_INT((long) s_tray.icon_count, 1,
            "nothing is removed while the tray is not ready");
    TAP_EQ_INT(s_layout_reflow_calls, 0,
            "and no reflow happens either");
}


/* 'systray_handle_property_notify' guards on a NULL event, not-ready
 * state, and a delete notification, and otherwise delegates */
static void s_test_handle_property_notify(void)
{
    xcb_property_notify_event_t event;

    s_reset();
    s_tray.is_window_ready = true;
    memset(&event, 0, sizeof(event));
    event.window = (xcb_window_t) 50u;
    event.atom = (xcb_atom_t) 600u;
    event.state = XCB_PROPERTY_NEW_VALUE;

    systray_handle_property_notify(s_fake_wm, NULL);
    TAP_EQ_INT(s_protocol_property_changed_calls, 0,
            "a NULL event never delegates");

    s_tray.is_window_ready = false;
    systray_handle_property_notify(s_fake_wm, &event);
    TAP_EQ_INT(s_protocol_property_changed_calls, 0,
            "not window-ready: never delegates either");

    s_tray.is_window_ready = true;
    event.state = XCB_PROPERTY_DELETE;
    systray_handle_property_notify(s_fake_wm, &event);
    TAP_EQ_INT(s_protocol_property_changed_calls, 0,
            "a property deletion is ignored, not delegated");

    event.state = XCB_PROPERTY_NEW_VALUE;
    systray_handle_property_notify(s_fake_wm, &event);
    TAP_EQ_INT(s_protocol_property_changed_calls, 1,
            "a genuine change delegates exactly once");
    TAP_EQ_INT((long) s_protocol_property_changed_last_window, 50,
            "carrying the exact window from the event");
    TAP_EQ_INT((long) s_protocol_property_changed_last_atom, 600,
            "and the exact atom from the event");
}


/* 'systray_handle_stage_resize' and 'systray_restack' are pure,
 * unconditional delegates */
static void s_test_stage_resize_and_restack_delegate(void)
{
    s_reset();

    systray_handle_stage_resize(s_fake_wm);
    TAP_EQ_INT(s_layout_reflow_calls, 1,
            "a stage resize always reflows exactly once");

    systray_restack();
    TAP_EQ_INT(s_layout_restack_calls, 1,
            "restack always asks the layout module to restack exactly"
            " once");
}


/* 'systray_reload' with a NULL wm or NULL config is a pure no-op */
static void s_test_reload_guards_refuse_cleanly(void)
{
    s_reset();
    s_reset_config();

    systray_reload(NULL);
    TAP_EQ_INT(s_protocol_apply_theme_style_calls, 0,
            "a NULL wm never applies any theme style");
}


/* Was disabled, now enabled by the reload: the window is ensured, the
 * tray becomes active, the selection is acquired (embedding on), and
 * both resort and reflow run */
static void s_test_reload_disabled_to_enabled(void)
{
    s_reset();
    s_reset_config();
    s_tray.is_active = false;

    systray_reload(s_fake_wm);

    TAP_EQ_INT(s_protocol_apply_theme_style_calls, 1,
            "the theme style is re-applied exactly once, unconditional"
            " of the transition");
    TAP_EQ_INT(s_window_ensure_calls, 1,
            "the window is ensured exactly once becoming enabled");
    TAP_OK(s_tray.is_active, "the tray becomes active");
    TAP_EQ_INT(s_selection_acquire_calls, 1,
            "the selection is acquired since embedding is enabled");
    TAP_EQ_INT(s_protocol_resort_calls, 1,
            "icons are re-sorted exactly once");
    TAP_EQ_INT(s_layout_reflow_calls, 1,
            "and the layout is reflowed exactly once");
}


/* Was enabled, now disabled by the reload, with the selection owned:
 * releasing it is enough, since that itself triggers a reflow */
static void s_test_reload_enabled_to_disabled_with_selection(void)
{
    s_reset();
    s_reset_config();
    s_config.base.systray.is_enabled = false;
    s_tray.is_active = true;
    s_tray.is_selection_owned = true;

    systray_reload(s_fake_wm);

    TAP_OK(!s_tray.is_active, "the tray becomes inactive");
    TAP_EQ_INT(s_selection_release_calls, 1,
            "the selection is released exactly once");
    TAP_EQ_INT(s_layout_reflow_calls, 0,
            "no separate reflow call is made here: releasing the"
            " selection is documented to trigger its own");
}


/* Was enabled, now disabled, with the selection never owned to begin
 * with (embedding was off): the reflow must be called directly since
 * there is no selection release to trigger one */
static void s_test_reload_enabled_to_disabled_without_selection(void)
{
    s_reset();
    s_reset_config();
    s_config.base.systray.is_enabled = false;
    s_tray.is_active = true;
    s_tray.is_selection_owned = false;

    systray_reload(s_fake_wm);

    TAP_EQ_INT(s_selection_release_calls, 0,
            "the selection is never released since it was not owned");
    TAP_EQ_INT(s_layout_reflow_calls, 1,
            "the layout is reflowed directly instead, exactly once");
}


/* Still active before and after the reload: only resort and reflow
 * run, the window-ensure and selection paths are left alone */
static void s_test_reload_still_active_only_resorts_and_reflows(void)
{
    s_reset();
    s_reset_config();
    s_tray.is_active = true;

    systray_reload(s_fake_wm);

    TAP_EQ_INT(s_window_ensure_calls, 0,
            "an already-active tray never re-ensures its window");
    TAP_EQ_INT(s_selection_acquire_calls, 0,
            "nor does it re-acquire the selection");
    TAP_EQ_INT(s_protocol_resort_calls, 1,
            "icons are still re-sorted exactly once");
    TAP_EQ_INT(s_layout_reflow_calls, 1,
            "and the layout is still reflowed exactly once");
}


/* Still disabled before and after the reload: nothing beyond the
 * unconditional config copy and theme style re-apply happens */
static void s_test_reload_still_disabled_is_a_no_op(void)
{
    s_reset();
    s_reset_config();
    s_config.base.systray.is_enabled = false;
    s_tray.is_active = false;

    systray_reload(s_fake_wm);

    TAP_EQ_INT(s_window_ensure_calls, 0,
            "a still-disabled tray never ensures a window");
    TAP_EQ_INT(s_selection_acquire_calls, 0,
            "never acquires a selection");
    TAP_EQ_INT(s_selection_release_calls, 0,
            "never releases one either, since it was never owned");
    TAP_EQ_INT(s_protocol_resort_calls, 0,
            "and never re-sorts, since nothing is showing to sort");
    TAP_EQ_INT(s_layout_reflow_calls, 0,
            "nor reflows");
}


int main(void)
{
    TAP_PLAN(101);

    s_test_init_guards_refuse_cleanly();
    s_test_init_enabled_runs_the_full_sequence();
    s_test_init_stops_if_window_ensure_fails();
    s_test_init_skips_selection_when_embedding_disabled();
    s_test_shutdown_with_no_window_only_releases_selection();
    s_test_shutdown_with_window_destroys_it();
    s_test_shutdown_skips_destroy_with_null_connection();
    s_test_owns_window_guards();
    s_test_below_window_guards();
    s_test_get_reserved_strut_guards();
    s_test_get_geometry();
    s_test_icon_size_enforce();
    s_test_icon_map_request_delegates();
    s_test_handle_client_message();
    s_test_handle_destroy_removes_and_shifts();
    s_test_handle_destroy_noop_when_not_ready();
    s_test_handle_property_notify();
    s_test_stage_resize_and_restack_delegate();
    s_test_reload_guards_refuse_cleanly();
    s_test_reload_disabled_to_enabled();
    s_test_reload_enabled_to_disabled_with_selection();
    s_test_reload_enabled_to_disabled_without_selection();
    s_test_reload_still_active_only_resorts_and_reflows();
    s_test_reload_still_disabled_is_a_no_op();

    return TAP_DONE();
}
