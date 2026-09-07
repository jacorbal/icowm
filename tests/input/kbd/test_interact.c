/**
 * @file tests/input/kbd/test_interact.c
 *
 * @brief Test battery for direct keyboard interaction with the
 *        focused client (input/kbd/interact.c)
 *
 * 'ik_get_active_client', 'ik_handle_launch', 'ik_handle_move', and
 * 'ik_handle_resize' are exercised against the real, linked source
 * file, including its own file-static 's_kb_resize_axis_target' and
 * 's_kbd_resize_apply' helpers reached only through 'ik_handle_
 * resize' itself.  'lookup_current_desktop' and 'lookup_find_client'
 * are test-controlled stand-ins answering a fixed 'desktop_td'/
 * 'client_td' pair a scenario registers first, the same pattern
 * 'tests/menu/context/test_winlist.c' uses for the identical lookup
 * pair, letting every scenario below exercise 'ik_get_active_client'
 * s own null-checks and desktop-to-client resolution without a real
 * surface/desktop/client tree.  'enact_client_move', 'cctl_launch_
 * dispatch', and 'run_init' are recording stand-ins: each is a heavy
 * side-effecting primitive belonging to its own subsystem (real
 * client movement, process launching, a drawn dialog), so this file
 * only asserts on how 'interact.c' itself decides to call them, not
 * on what they do past that call, mirroring 'tests/cmds/client/
 * test_move.c's own treatment of 'wm_request_client_redraw'.
 * 'ccmd_client_resolve_workarea', 'client_decoration_layout_sync',
 * 'client_send_synthetic_configure_notify', 'client_aspect_ratio_
 * clamp', 'ccmd_client_apply_geometry', 'ccmd_client_unshade', and
 * 'wm_request_client_redraw' are likewise recording or controlled
 * stand-ins for the same reason 'test_move.c' already stubs 'ccmd_
 * client_resolve_workarea' itself: none of their real X-facing or
 * cross-module behavior is 'interact.c's own logic to verify.
 * 'xcb_connection_get' answers a fixed non-null, never-dereferenced
 * pointer, and 'xcb_clear_area' is link-only, exactly as 'test_move.c'
 * already treats the analogous 'xcb_configure_window' call: no test
 * here opens a live X connection for either to write a real request
 * to.  Every 'client_td' fixture below leaves 'config' NULL on
 * purpose, so 'client_border_width' (client.h, a 'static inline'
 * function, not a stand-in candidate at all) takes its documented
 * NULL-config fast path and returns 0 without needing a full theme.
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
#include <time.h>       /* nanosleep, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Command includes */
#include <cmds/client/move.h>

/* Project includes */
#include <cctl/launch.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Menu includes */
#include <menu/dialog/run.h>

/* Local includes */
#include <defs/kbd.h>
#include <harness/tap.h>
#include <input/kbd/internal.h>


/**
 * @brief Sleep comfortably past @c KBD_LAUNCH_MIN_INTERVAL_MS
 *
 * @a ik_handle_launch paces every dispatching call (that is, every
 * one that does not take the @c IK_LAUNCH_LAUNCHER early return for
 * an enabled run-box) against a single file-static timestamp inside
 * 'interact.c' itself, shared across every call this whole test
 * binary makes, not reset by @a s_reset above.  Any scenario after
 * this file's first dispatching launch needs to wait out that
 * window first, or it would otherwise be paced out itself and
 * silently see no dispatch at all, unrelated to whatever that
 * scenario is actually meant to check.
 *
 * @note Complexity: @e O(1)
 */
static void s_sleep_past_launch_pacing(void)
{
    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = (long) (KBD_LAUNCH_MIN_INTERVAL_MS + 20u) *
            1000L * 1000L;
    (void) nanosleep(&delay, NULL);
}


/** Fixed desktop/client pair @a lookup_current_desktop and
 *  @a lookup_find_client answer with, registered by @a s_set_lookup;
 *  'is_ok' controls whether either lookup succeeds at all */
static desktop_td s_desktop;
static client_td s_client;
static bool s_lookup_desktop_ok;
static bool s_lookup_client_ok;


/**
 * @brief Test-controlled stand-in for @a lookup_current_desktop
 * @note Complexity: @e O(1)
 */
desktop_td *lookup_current_desktop(surface_td *surface)
{
    (void) surface;
    return (s_lookup_desktop_ok) ? &s_desktop : NULL;
}


/**
 * @brief Test-controlled stand-in for @a lookup_find_client
 * @note Complexity: @e O(1)
 */
client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **out_surface, desktop_td **out_desktop)
{
    (void) surfaces;
    (void) window;

    if (out_surface != NULL) {
        *out_surface = NULL;
    }
    if (out_desktop != NULL) {
        *out_desktop = (s_lookup_client_ok) ? &s_desktop : NULL;
    }

    return (s_lookup_client_ok) ? &s_client : NULL;
}


/**
 * @brief Register the desktop/client pair the lookup stand-ins above
 *        answer with
 *
 * @note Complexity: @e O(1)
 */
static void s_set_lookup(bool desktop_ok, bool client_ok,
        xcb_window_t active_id)
{
    s_lookup_desktop_ok = desktop_ok;
    s_lookup_client_ok = client_ok;
    s_desktop.client_active_id = active_id;
}


/** Call counters and last-seen arguments for every recording
 *  stand-in, reset by s_reset before each scenario */
static int s_call_enact_client_move;
static struct position_s s_last_move_pos;
static int s_call_cctl_launch_dispatch;
static const char *s_last_launch_prog;
static int s_call_run_init;
static int s_call_client_decoration_layout_sync;
static int s_call_client_send_synthetic_configure_notify;
static int s_call_client_aspect_ratio_clamp;
static int s_call_ccmd_client_apply_geometry;
static struct geometry_s s_last_apply_geometry;
static int s_call_ccmd_client_unshade;
static int s_call_wm_request_client_redraw;

/** Fixed rectangle @a ccmd_client_resolve_workarea answers with,
 *  registered by @a s_set_workarea; when 'is_ok' is false it refuses
 *  outright, the same "no workarea available" contract 'test_move.c'
 *  already exercises for the real @a ccmd_client_move callers */
static int32_t s_wa_x;
static int32_t s_wa_y;
static uint16_t s_wa_w;
static uint16_t s_wa_h;
static bool s_wa_ok;


static void s_set_workarea(int32_t x, int32_t y, uint16_t w, uint16_t h)
{
    s_wa_x = x;
    s_wa_y = y;
    s_wa_w = w;
    s_wa_h = h;
    s_wa_ok = true;
}


static void s_reset(void)
{
    s_lookup_desktop_ok = false;
    s_lookup_client_ok = false;
    memset(&s_desktop, 0, sizeof(s_desktop));
    memset(&s_client, 0, sizeof(s_client));

    s_call_enact_client_move = 0;
    memset(&s_last_move_pos, 0, sizeof(s_last_move_pos));
    s_call_cctl_launch_dispatch = 0;
    s_last_launch_prog = NULL;
    s_call_run_init = 0;
    s_call_client_decoration_layout_sync = 0;
    s_call_client_send_synthetic_configure_notify = 0;
    s_call_client_aspect_ratio_clamp = 0;
    s_call_ccmd_client_apply_geometry = 0;
    memset(&s_last_apply_geometry, 0, sizeof(s_last_apply_geometry));
    s_call_ccmd_client_unshade = 0;
    s_call_wm_request_client_redraw = 0;

    s_wa_x = 0;
    s_wa_y = 0;
    s_wa_w = 0;
    s_wa_h = 0;
    s_wa_ok = false;
}


/* Recording stand-ins */

void enact_client_move(client_td *client, struct position_s pos)
{
    (void) client;
    s_call_enact_client_move++;
    s_last_move_pos = pos;
}


void cctl_launch_dispatch(surface_td *surface, const char *restrict prog,
        const char *restrict class_name)
{
    (void) surface;
    (void) class_name;
    s_call_cctl_launch_dispatch++;
    s_last_launch_prog = prog;
}


void run_init(xcb_connection_t *connection, surface_td *surface,
        const config_td *cfg)
{
    (void) connection;
    (void) surface;
    (void) cfg;
    s_call_run_init++;
}


bool ccmd_client_resolve_workarea(client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y,
        uint16_t *restrict out_w, uint16_t *restrict out_h)
{
    (void) client;

    if (!s_wa_ok) {
        return false;
    }

    if (out_x != NULL) {
        *out_x = s_wa_x;
    }
    if (out_y != NULL) {
        *out_y = s_wa_y;
    }
    *out_w = s_wa_w;
    *out_h = s_wa_h;

    return true;
}


void client_decoration_layout_sync(client_td *client)
{
    (void) client;
    s_call_client_decoration_layout_sync++;
}


void client_send_synthetic_configure_notify(xcb_connection_t *connection,
        const client_td *client)
{
    (void) connection;
    (void) client;
    s_call_client_send_synthetic_configure_notify++;
}


void client_aspect_ratio_clamp(const client_td *client, uint32_t width,
        uint32_t *restrict out_height)
{
    (void) client;
    (void) width;

    /* No aspect constraint applied: the height stays exactly what
     * 'ik_handle_resize' already computed for the axis under test,
     * letting every resize scenario below assert on a fully
     * predictable value */
    s_call_client_aspect_ratio_clamp++;
    (void) out_height;
}


void ccmd_client_apply_geometry(client_td *client,
        xcb_window_t win, uint16_t mask, int32_t x, int32_t y,
        uint32_t w, uint32_t h, uint32_t border_width)
{
    (void) client;
    (void) win;
    (void) mask;
    (void) border_width;
    s_call_ccmd_client_apply_geometry++;
    s_last_apply_geometry.pos.x = x;
    s_last_apply_geometry.pos.y = y;
    s_last_apply_geometry.dim.w = w;
    s_last_apply_geometry.dim.h = h;
}


void ccmd_client_unshade(client_td *client)
{
    (void) client;
    s_call_ccmd_client_unshade++;
}


void wm_request_client_redraw(client_td *client)
{
    (void) client;
    s_call_wm_request_client_redraw++;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * Answers a fixed non-null, never-dereferenced pointer, the same
 * pattern 'tests/cmds/client/test_move.c' already uses for it: no
 * scenario below opens a live X connection, since every function
 * that would write through it ('client_send_synthetic_configure_
 * notify', 'xcb_clear_area') is itself a stand-in here too
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return (xcb_connection_t *) (uintptr_t) 1;
}


/**
 * @brief Link-only stand-in for @a xcb_clear_area
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_clear_area(xcb_connection_t *connection,
        uint8_t exposures, xcb_window_t window, int16_t x, int16_t y,
        uint16_t width, uint16_t height)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) exposures;
    (void) window;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    memset(&cookie, 0, sizeof(cookie));
    return cookie;
}


/* ik_get_active_client */

static void s_test_get_active_client_null_surface(void)
{
    client_td *result;
    surface_td *cs_out = (surface_td *) 1;
    desktop_td *cd_out = (desktop_td *) 1;

    s_reset();
    result = ik_get_active_client(NULL, NULL, &cs_out, &cd_out);

    TAP_NULL(result, "a null surface yields no active client");
    TAP_NULL(cs_out, "a null surface clears the surface out-param");
    TAP_NULL(cd_out, "a null surface clears the desktop out-param");
}


static void s_test_get_active_client_no_desktop(void)
{
    client_td *result;
    surface_td fake_surface;

    s_reset();
    memset(&fake_surface, 0, sizeof(fake_surface));
    result = ik_get_active_client(&fake_surface, NULL, NULL, NULL);

    TAP_NULL(result,
            "no current desktop for the surface yields no active" \
            " client");
}


static void s_test_get_active_client_no_active_window(void)
{
    client_td *result;
    surface_td fake_surface;

    s_reset();
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 0);
    result = ik_get_active_client(&fake_surface, NULL, NULL, NULL);

    TAP_NULL(result,
            "a desktop with no active window (id 0) yields no" \
            " active client");
}


static void s_test_get_active_client_found(void)
{
    client_td *result;
    surface_td fake_surface;

    s_reset();
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 42);
    result = ik_get_active_client(&fake_surface, NULL, NULL, NULL);

    TAP_OK(result == &s_client,
            "a desktop with a nonzero active window resolves the" \
            " expected client");
}


/* ik_handle_launch */

static void s_test_handle_launch_terminal(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    strcpy(cfg.base.programs.terminal, "xterm");

    ik_handle_launch(IK_LAUNCH_TERMINAL, &fake_surface, &cfg);

    TAP_EQ_INT(s_call_cctl_launch_dispatch, 1,
            "IK_LAUNCH_TERMINAL dispatches exactly once");
    TAP_EQ_STR(s_last_launch_prog, "xterm",
            "IK_LAUNCH_TERMINAL dispatches config.base.programs" \
            ".terminal");
}


static void s_test_handle_launch_launcher_prompt_disabled(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    s_sleep_past_launch_pacing();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    cfg.base.prompt.is_enabled = false;
    strcpy(cfg.base.programs.launcher, "dmenu_run");

    ik_handle_launch(IK_LAUNCH_LAUNCHER, &fake_surface, &cfg);

    TAP_EQ_INT(s_call_run_init, 0,
            "IK_LAUNCH_LAUNCHER never opens the run-box when" \
            " prompt.is_enabled is false");
    TAP_EQ_STR(s_last_launch_prog, "dmenu_run",
            "IK_LAUNCH_LAUNCHER dispatches config.base.programs" \
            ".launcher when prompt.is_enabled is false");
}


static void s_test_handle_launch_launcher_prompt_enabled(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    cfg.base.prompt.is_enabled = true;

    ik_handle_launch(IK_LAUNCH_LAUNCHER, &fake_surface, &cfg);

    TAP_EQ_INT(s_call_run_init, 1,
            "IK_LAUNCH_LAUNCHER opens the run-box exactly once when" \
            " prompt.is_enabled is true");
    TAP_EQ_INT(s_call_cctl_launch_dispatch, 0,
            "IK_LAUNCH_LAUNCHER never spawns programs.launcher when" \
            " prompt.is_enabled is true");
}


static void s_test_handle_launch_pacing_blocks_rapid_repeat(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    s_sleep_past_launch_pacing();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    strcpy(cfg.base.programs.editor, "vi");

    ik_handle_launch(IK_LAUNCH_EDITOR, &fake_surface, &cfg);
    TAP_EQ_INT(s_call_cctl_launch_dispatch, 1,
            "the first IK_LAUNCH_EDITOR call within pacing dispatches");

    ik_handle_launch(IK_LAUNCH_EDITOR, &fake_surface, &cfg);
    TAP_EQ_INT(s_call_cctl_launch_dispatch, 1,
            "an immediate second call is paced out by" \
            " KBD_LAUNCH_MIN_INTERVAL_MS and does not dispatch again");
}


/* ik_handle_move */

static void s_test_handle_move_no_active_client_is_noop(void)
{
    surface_td fake_surface;

    s_reset();
    memset(&fake_surface, 0, sizeof(fake_surface));

    ik_handle_move(IK_MOVE_LEFT, &fake_surface, NULL, NULL);

    TAP_EQ_INT(s_call_enact_client_move, 0,
            "with no active client ik_handle_move never calls" \
            " enact_client_move");
}


static void s_test_handle_move_maximized_is_noop(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 7);
    s_client.properties.state = (uint16_t) CLIENT_STATE_MAXIMIZED;

    ik_handle_move(IK_MOVE_LEFT, &fake_surface, NULL, &cfg);

    TAP_EQ_INT(s_call_enact_client_move, 0,
            "a fully maximized client is never moved by keyboard");
}


static void s_test_handle_move_step(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 7);
    cfg.base.windows.move_step = 10;
    s_client.layout.geometry.cur.pos.x = 100;
    s_client.layout.geometry.cur.pos.y = 100;

    ik_handle_move(IK_MOVE_RIGHT, &fake_surface, NULL, &cfg);

    TAP_EQ_INT(s_call_enact_client_move, 1,
            "IK_MOVE_RIGHT calls enact_client_move exactly once");
    TAP_EQ_INT(s_last_move_pos.x, 110,
            "IK_MOVE_RIGHT advances x by config.base.windows" \
            ".move_step");
    TAP_EQ_INT(s_last_move_pos.y, 100,
            "IK_MOVE_RIGHT leaves y unchanged");
}


static void s_test_handle_move_corner_with_workarea(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 7);
    cfg.base.windows.move_step = 10;
    s_client.layout.geometry.cur.dim.w = 200;
    s_client.layout.geometry.cur.dim.h = 100;
    s_set_workarea(0, 0, 800, 600);

    ik_handle_move(IK_MOVE_BOTTOM_RIGHT, &fake_surface, NULL, &cfg);

    TAP_EQ_INT(s_last_move_pos.x, 800 - 200,
            "IK_MOVE_BOTTOM_RIGHT lands x at the workarea's right" \
            " edge minus the client's own width");
    TAP_EQ_INT(s_last_move_pos.y, 600 - 100,
            "IK_MOVE_BOTTOM_RIGHT lands y at the workarea's bottom" \
            " edge minus the client's own height");
}


/* ik_handle_resize */

static void s_test_handle_resize_no_active_client_is_noop(void)
{
    surface_td fake_surface;

    s_reset();
    memset(&fake_surface, 0, sizeof(fake_surface));

    ik_handle_resize(IK_RESIZE_RIGHT, &fake_surface, NULL, NULL);

    TAP_EQ_INT(s_call_ccmd_client_apply_geometry, 0,
            "with no active client ik_handle_resize never applies" \
            " geometry");
}


static void s_test_handle_resize_non_resizable_is_noop(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 7);
    s_client.properties.flags = 0;

    ik_handle_resize(IK_RESIZE_RIGHT, &fake_surface, NULL, &cfg);

    TAP_EQ_INT(s_call_ccmd_client_apply_geometry, 0,
            "a non-resizable client is never resized by keyboard");
}


static void s_test_handle_resize_fullscreen_is_noop(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 7);
    s_client.properties.flags = CLIENT_FLAG_RESIZABLE;
    s_client.properties.state = (uint16_t) CLIENT_STATE_FULLSCREEN;

    ik_handle_resize(IK_RESIZE_RIGHT, &fake_surface, NULL, &cfg);

    TAP_EQ_INT(s_call_ccmd_client_apply_geometry, 0,
            "a fullscreen client is never resized by keyboard");
}


static void s_test_handle_resize_maximized_axis_locked(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 7);
    s_client.properties.flags = CLIENT_FLAG_RESIZABLE;
    s_client.properties.state =
            (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;

    ik_handle_resize(IK_RESIZE_RIGHT, &fake_surface, NULL, &cfg);
    TAP_EQ_INT(s_call_ccmd_client_apply_geometry, 0,
            "resizing the horizontally-maximized axis is refused");

    s_reset();
    s_set_lookup(true, true, 7);
    s_client.properties.flags = CLIENT_FLAG_RESIZABLE;
    s_client.properties.state =
            (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ;
    s_client.layout.geometry.cur.dim.h = 100;

    ik_handle_resize(IK_RESIZE_DOWN, &fake_surface, NULL, &cfg);
    TAP_EQ_INT(s_call_ccmd_client_apply_geometry, 1,
            "the still-free vertical axis resizes even while the" \
            " horizontal one is maximized");
}


static void s_test_handle_resize_right_grows_by_step(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 7);
    s_client.properties.flags = CLIENT_FLAG_RESIZABLE;
    cfg.base.windows.resize_step = 20;
    s_client.layout.geometry.cur.dim.w = 300;
    s_client.layout.geometry.cur.dim.h = 200;
    s_client.layout.geometry.cur.pos.x = 50;
    s_client.layout.geometry.cur.pos.y = 50;

    ik_handle_resize(IK_RESIZE_RIGHT, &fake_surface, NULL, &cfg);

    TAP_EQ_INT(s_call_ccmd_client_apply_geometry, 1,
            "IK_RESIZE_RIGHT applies geometry exactly once");
    TAP_EQ_INT(s_last_apply_geometry.dim.w, 320,
            "IK_RESIZE_RIGHT grows the frame width by" \
            " config.base.windows.resize_step");
    TAP_EQ_INT(s_last_apply_geometry.pos.x, 50,
            "IK_RESIZE_RIGHT leaves the left edge (x) in place");
}


static void s_test_handle_resize_left_shrinks_and_shifts_x(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 7);
    s_client.properties.flags = CLIENT_FLAG_RESIZABLE;
    cfg.base.windows.resize_step = 20;
    s_client.layout.geometry.cur.dim.w = 300;
    s_client.layout.geometry.cur.dim.h = 200;
    s_client.layout.geometry.cur.pos.x = 50;
    s_client.layout.geometry.cur.pos.y = 50;

    /* 'IK_RESIZE_LEFT' moves the left edge itself, which shrinks the
     * frame ('grow' is false in 's_kb_resize_axis_target's own call
     * for this case) while sliding x right by the same amount so the
     * right edge stays fixed in place; it is 'IK_RESIZE_RIGHT' above
     * that grows the frame outward from a fixed left edge instead */
    ik_handle_resize(IK_RESIZE_LEFT, &fake_surface, NULL, &cfg);

    TAP_EQ_INT(s_last_apply_geometry.dim.w, 280,
            "IK_RESIZE_LEFT shrinks the frame width by" \
            " config.base.windows.resize_step");
    TAP_EQ_INT(s_last_apply_geometry.pos.x, 50 + 20,
            "IK_RESIZE_LEFT shifts x right by the same amount the" \
            " width shrank, keeping the right edge fixed");
}


static void s_test_handle_resize_shaded_client_unshades_first(void)
{
    config_td cfg;
    surface_td fake_surface;

    s_reset();
    memset(&cfg, 0, sizeof(cfg));
    memset(&fake_surface, 0, sizeof(fake_surface));
    s_set_lookup(true, true, 7);
    s_client.properties.flags = (uint16_t)
            (CLIENT_FLAG_RESIZABLE | CLIENT_FLAG_SHADED);
    cfg.base.windows.resize_step = 10;
    s_client.layout.geometry.old.dim.h = 200;
    s_client.layout.geometry.cur.dim.w = 300;

    ik_handle_resize(IK_RESIZE_DOWN, &fake_surface, NULL, &cfg);

    TAP_EQ_INT(s_call_ccmd_client_unshade, 1,
            "resizing a shaded client along the vertical axis" \
            " unshades it first");
}


int main(void)
{
    TAP_PLAN(32);

    s_test_get_active_client_null_surface();
    s_test_get_active_client_no_desktop();
    s_test_get_active_client_no_active_window();
    s_test_get_active_client_found();

    s_test_handle_launch_terminal();
    s_test_handle_launch_launcher_prompt_disabled();
    s_test_handle_launch_launcher_prompt_enabled();
    s_test_handle_launch_pacing_blocks_rapid_repeat();

    s_test_handle_move_no_active_client_is_noop();
    s_test_handle_move_maximized_is_noop();
    s_test_handle_move_step();
    s_test_handle_move_corner_with_workarea();

    s_test_handle_resize_no_active_client_is_noop();
    s_test_handle_resize_non_resizable_is_noop();
    s_test_handle_resize_fullscreen_is_noop();
    s_test_handle_resize_maximized_axis_locked();
    s_test_handle_resize_right_grows_by_step();
    s_test_handle_resize_left_shrinks_and_shifts_x();
    s_test_handle_resize_shaded_client_unshades_first();

    return TAP_DONE();
}
