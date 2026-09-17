/**
 * @file tests/policy/test_placement.c
 *
 * @brief Test battery for window placement policy
 *
 * Every XCB function this module reaches (xcb_configure_window,
 * xcb_query_pointer/_reply, xcb_get_geometry/_reply) is stubbed
 * below as a controllable, call-recording stand-in: libxcb itself is
 * deliberately never linked here, so the linker resolves every one
 * of those symbols against this file's own definitions instead.
 * stage_desktop_get, stage_monitor_primary,
 * stage_monitor_for_point, and lookup_find_client (all real
 * modules with XCB-dependent implementations elsewhere) are stubbed
 * the same way.  s_score_window_pos, s_place_apply_gravity, and
 * s_clip_to_monitor are static to placement.c itself and only
 * reachable indirectly, through place_smart/place_apply/
 * place_apply_cascade's own observable results (the final
 * coordinates captured by the xcb_configure_window stub, or
 * place_smart's own out_x/out_y).
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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Local includes */
#include <client.h>
#include <desktop.h>
#include <stage.h>
#include <config.h>
#include <harness/tap.h>
#include <policy/placement/window.h>
#include <wm/internal.h>
#include <utils/xcb/connection.h>


/**
 * @brief Stands in for a live connection
 *
 * Its address is all that is wanted: every XCB call this file reaches
 * is answered by a stub below, so nothing ever looks inside it.
 */
static int s_placement_conn;


/* ===== XCB stand-ins (libxcb itself is never linked here) ===== */

static xcb_window_t s_configured_window;
static uint16_t s_configured_mask;
static int32_t s_configured_x;
static int32_t s_configured_y;
static int s_configure_calls;

xcb_void_cookie_t xcb_configure_window(xcb_connection_t *c,
        xcb_window_t window, uint16_t value_mask,
        const void *value_list)
{
    const uint32_t *values = (const uint32_t *) value_list;
    xcb_void_cookie_t cookie = {0};

    (void) c;
    s_configure_calls++;
    s_configured_window = window;
    s_configured_mask = value_mask;
    s_configured_x = (int32_t) values[0];
    s_configured_y = (int32_t) values[1];
    return cookie;
}

/** Controllable stand-in for the pointer query used by both
 *  s_reference_monitor (CONFIG_PLACEMENT_MONITOR_POINTER) and
 *  place_apply's own CONFIG_PLACEMENT_POLICY_UNDER_MOUSE: NULL means
 *  "query failed", matching the real xcb_query_pointer_reply's own
 *  documented failure mode */
static xcb_query_pointer_reply_t *s_pointer_reply = NULL;

xcb_query_pointer_cookie_t xcb_query_pointer(xcb_connection_t *c,
        xcb_window_t window)
{
    xcb_query_pointer_cookie_t cookie = {0};

    (void) c;
    (void) window;
    return cookie;
}

xcb_query_pointer_reply_t *xcb_query_pointer_reply(xcb_connection_t *c,
        xcb_query_pointer_cookie_t cookie, xcb_generic_error_t **e)
{
    xcb_query_pointer_reply_t *copy;

    (void) c;
    (void) cookie;
    (void) e;

    if (s_pointer_reply == NULL) {
        return NULL;
    }
    /* The real reply is malloc'd and free()'d by the caller; this
     * stand-in returns a fresh heap copy each time for the same
     * reason, so a caller's own free() is always valid */
    copy = malloc(sizeof(xcb_query_pointer_reply_t));
    *copy = *s_pointer_reply;
    return copy;
}

/** Controllable stand-in for the transient-parent geometry fallback
 *  in s_place_transient_centered; NULL means "query failed" */
static xcb_get_geometry_reply_t *s_geometry_reply = NULL;

xcb_get_geometry_cookie_t xcb_get_geometry(xcb_connection_t *c,
        xcb_drawable_t drawable)
{
    xcb_get_geometry_cookie_t cookie = {0};

    (void) c;
    (void) drawable;
    return cookie;
}

xcb_get_geometry_reply_t *xcb_get_geometry_reply(xcb_connection_t *c,
        xcb_get_geometry_cookie_t cookie, xcb_generic_error_t **e)
{
    xcb_get_geometry_reply_t *copy;

    (void) c;
    (void) cookie;
    (void) e;

    if (s_geometry_reply == NULL) {
        return NULL;
    }
    copy = malloc(sizeof(xcb_get_geometry_reply_t));
    *copy = *s_geometry_reply;
    return copy;
}


/* ===== Project stand-ins ===== */

/** Link-only stand-in for client_group_transient_anchor (cmds/
 *  client/transient.c): every client this file builds names its own
 *  transient parent directly when it has one, so the group lookup
 *  never has an answer to give */
/** Link-only stand-in for atom_set_window_bypass_compositor
 *  (utils/xcb/atom.c): the drag outline sets that property on the
 *  strips it creates, and no test here has a server to set it on */
void atom_set_window_bypass_compositor(xcb_connection_t *connection,
        xcb_window_t window)
{
    (void) connection;
    (void) window;
}


client_td *client_group_transient_anchor(const client_td *client)
{
    (void) client;
    return NULL;
}


/** Link-only stand-in for systray_get_geometry (systray.c): no test
 *  here docks a systray, so placement always runs with the whole
 *  workarea free of one */
bool systray_get_geometry(const stage_td *stage,
        struct geometry_s *restrict out_tray)
{
    (void) stage;
    (void) out_tray;
    return false;
}


static desktop_td *s_desktops_by_id[4];

desktop_td *stage_desktop_get(stage_td *stage, uint32_t desktop_id)
{
    (void) stage;
    if (desktop_id >= 4u) {
        return NULL;
    }
    return s_desktops_by_id[desktop_id];
}


static monitor_td s_primary_monitor;

monitor_td stage_monitor_primary(const stage_td *stage)
{
    (void) stage;
    return s_primary_monitor;
}


static monitor_td s_monitor_for_point;

monitor_td stage_monitor_for_point(const stage_td *stage,
        struct position_s pos)
{
    (void) stage;
    (void) pos;
    return s_monitor_for_point;
}


static client_td *s_found_client = NULL;

client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **out_stage, desktop_td **out_desktop)
{
    (void) stages;
    (void) window;
    if (out_stage != NULL) {
        *out_stage = NULL;
    }
    if (out_desktop != NULL) {
        *out_desktop = NULL;
    }
    return s_found_client;
}


static void s_reset_stubs(void)
{
    s_configure_calls = 0;
    s_configured_window = 0;
    s_configured_mask = 0;
    s_configured_x = 0;
    s_configured_y = 0;
    s_pointer_reply = NULL;
    s_geometry_reply = NULL;
    memset(&s_primary_monitor, 0, sizeof(s_primary_monitor));
    memset(&s_monitor_for_point, 0, sizeof(s_monitor_for_point));
    s_found_client = NULL;
    memset(s_desktops_by_id, 0, sizeof(s_desktops_by_id));
}


/**
 * @brief Build a minimal, single-monitor stage (monitor_count <= 1
 *        so s_clip_to_monitor's own clipping never engages) with one
 *        desktop at index 0
 */
static xcb_screen_t s_dummy_screen;

static void s_make_stage(stage_td *stage, desktop_td *desktop,
        uint32_t screen_w, uint32_t screen_h)
{
    memset(stage, 0, sizeof(*stage));
    memset(desktop, 0, sizeof(*desktop));
    memset(&s_dummy_screen, 0, sizeof(s_dummy_screen));
    stage->screen = &s_dummy_screen;
    stage->properties.dim.w = screen_w;
    stage->properties.dim.h = screen_h;
    stage->desktop_count = 1u;
    stage->desktop_cur = 0u;
    stage->monitor_count = 1u;
    s_desktops_by_id[0] = desktop;
}


static wm_td *s_make_wm(wm_td *wm, config_td *config)
{
    memset(wm, 0, sizeof(*wm));
    memset(config, 0, sizeof(*config));
    wm->config = config;
    /* Never dereferenced for real: every XCB call it reaches is
     * stubbed above, and place_smart's own guard only checks it
     * against NULL */
    wm->connection = (xcb_connection_t *) 0x1;
    config->base.windows.monitor_policy = CONFIG_PLACEMENT_MONITOR_PRIMARY;
    return wm;
}


/* place_window_smart's own behavior (guard clauses, empty-desktop
 * centering, avoiding occupied/iconified positions, respecting the
 * workarea and monitor bounds, and the no-current-desktop failure
 * case) is not covered here: it is static to window.c
 * (s_place_window_smart), unreachable from this file.  What
 * place_window_apply below does reach is exercised: each of the four
 * decisions taken ahead of the policy (splash, an honored requested
 * position, the junk transient origin that one declines, and the
 * sibling grouping) and each policy that resolves a position of its
 * own (CENTERED, CASCADE, UNDER_MOUSE, SMART, MANUAL and none). */



/* place_apply_cascade advances its own shared sequence counter by
 * exactly one cascade_step (24px) per call, regardless of whatever
 * value that counter already held coming in from other tests or
 * calls */
static void s_test_cascade_advances_by_one_step(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    bool wrapped;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    /* Large workarea: max_steps stays comfortably above 1, so two
     * consecutive calls essentially never wrap in practice */
    s_make_stage(&stage, &desktop, 2000u, 1600u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    place_window_apply_cascade(&wm, &stage, &client);
    x1 = s_configured_x;
    y1 = s_configured_y;

    place_window_apply_cascade(&wm, &stage, &client);
    x2 = s_configured_x;
    y2 = s_configured_y;

    wrapped = (x2 < x1);
    TAP_OK(wrapped || (x2 == x1 + 24 && y2 == y1 + 24),
            "each call advances the shared cascade position by" \
            " exactly one 24px step (or wraps back to the start)");
}


/* place_apply_cascade never hands out a position that leaves the
 * workarea, and never hands out the same position twice running: each
 * axis wraps back to the origin on its own once that axis stops
 * fitting, rather than walking off the edge */
static void s_test_cascade_stays_inside_workarea(void)
{
    const int32_t screen_w = 248;
    const int32_t screen_h = 148;
    const int32_t win_w = 200;
    const int32_t win_h = 100;
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;
    int32_t prev_x = 0;
    int32_t prev_y = 0;
    bool is_inside = true;
    bool has_repeat = false;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_stage(&stage, &desktop, (uint32_t) screen_w,
            (uint32_t) screen_h);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = (uint32_t) win_w;
    client.layout.geometry.cur.dim.h = (uint32_t) win_h;

    /* More calls than either axis has room for, so both wrap at least
     * twice whatever position the shared state carried in */
    for (int i = 0; i < 12; ++i) {
        place_window_apply_cascade(&wm, &stage, &client);

        if (s_configured_x < 0 || s_configured_y < 0 ||
                s_configured_x + win_w > screen_w ||
                s_configured_y + win_h > screen_h) {
            is_inside = false;
        }
        if (i > 0 && s_configured_x == prev_x &&
                s_configured_y == prev_y) {
            has_repeat = true;
        }
        prev_x = s_configured_x;
        prev_y = s_configured_y;
    }

    TAP_OK(is_inside,
            "every cascade position stays wholly inside the" \
            " workarea");
    TAP_OK(!has_repeat,
            "no two consecutive cascade positions are the same");
}


/* Wrapping the vertical axis opens a new column rather than returning
 * to where the run began: the horizontal axis carries on across the
 * wrap, so each column starts further right than the one before it.
 * Sharing one step count between the axes, as taking the smaller of
 * the two did, tied x to y and started every column at the same x */
static void s_test_cascade_starts_new_column(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;
    int32_t prev_y;
    int32_t x_top_first = 0;
    int32_t y_top_first = 0;
    int32_t x_top_second = 0;
    int32_t y_top_second = 0;
    int tops = 0;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    /* Wide and short: the vertical axis has room for 3 steps
     * ((148 - 100) / 24) and the horizontal one for 25, so a vertical
     * wrap has somewhere else to go and cannot take x with it */
    s_make_stage(&stage, &desktop, 800u, 148u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    /* The cascade position is shared and carries over from whatever
     * ran before this, so both column tops are found by watching for
     * the vertical wrap rather than assumed to fall on any given
     * call */
    place_window_apply_cascade(&wm, &stage, &client);
    prev_y = s_configured_y;
    for (int i = 0; i < 16 && tops < 2; ++i) {
        place_window_apply_cascade(&wm, &stage, &client);

        if (s_configured_y < prev_y) {
            if (tops == 0) {
                x_top_first = s_configured_x;
                y_top_first = s_configured_y;
            } else {
                x_top_second = s_configured_x;
                y_top_second = s_configured_y;
            }
            tops++;
        }
        prev_y = s_configured_y;
    }

    TAP_EQ_INT(y_top_second, y_top_first,
            "every column starts at the same height, the vertical" \
            " axis wrapping rather than running off the bottom");
    TAP_OK(tops == 2 && x_top_second > x_top_first,
            "each column starts further right than the one before" \
            " it, rather than back where the run began");
}


/* place_apply_cascade's own guard clauses: any missing required
 * argument is a safe no-op */
static void s_test_cascade_guards(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;

    place_window_apply_cascade(NULL, &stage, &client);
    place_window_apply_cascade(&wm, NULL, &client);
    place_window_apply_cascade(&wm, &stage, NULL);

    TAP_EQ_INT(s_configure_calls, 0,
            "missing required arguments never reach xcb_configure_window");
}


/* place_apply's own guard clauses: any missing required argument is
 * a safe no-op */
static void s_test_apply_guards(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;

    place_window_apply(NULL, &stage, &client);
    place_window_apply(&wm, NULL, &client);
    place_window_apply(&wm, &stage, NULL);

    TAP_EQ_INT(s_configure_calls, 0,
            "missing required arguments never reach xcb_configure_window");
}


/* CONFIG_PLACEMENT_POLICY_CENTERED centers the client on the
 * workarea, clamped so it never starts left of or above it */
static void s_test_apply_centered(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_CENTERED;
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    place_window_apply(&wm, &stage, &client);

    /* (1000-200)/2 = 400; (800-100)/2 = 350 */
    TAP_EQ_INT(s_configured_x, 400, "centered horizontally on the workarea");
    TAP_EQ_INT(s_configured_y, 350, "centered vertically on the workarea");
}


/* A splash screen is centered on the workarea ahead of every policy
 * and ahead of the honored-position branch, and without gravity being
 * applied to the result: the middle worked out here is already where
 * the window goes */
static void s_test_apply_splash_centers_on_workarea(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        CONFIG_PLACEMENT_POLICY_CASCADE;
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_CENTER;
    client.properties.type = (uint16_t) CLIENT_TYPE_SPLASH;
    /* Asking for a corner as well, which a splash routinely does:
     * the splash branch has to win over the honored-position one */
    client.hints_icccm.size.has_position = true;
    client.hints_icccm.size.req_pos.x = 700;
    client.hints_icccm.size.req_pos.y = 600;

    place_window_apply(&wm, &stage, &client);

    /* (1000-200)/2 = 400; (800-100)/2 = 350, with center gravity
     * deliberately not taking half the width off again */
    TAP_EQ_INT(s_configured_x, 400,
            "a splash is centered horizontally whatever it asked for");
    TAP_EQ_INT(s_configured_y, 350,
            "a splash is centered vertically whatever it asked for");
}


/* A client-requested position (ICCCM 4.1.2.3) takes priority over the
 * configured policy */
static void s_test_apply_honors_requested_position(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        CONFIG_PLACEMENT_POLICY_CENTERED;
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;
    client.hints_icccm.size.has_position = true;
    client.hints_icccm.size.req_pos.x = 123;
    client.hints_icccm.size.req_pos.y = 77;

    place_window_apply(&wm, &stage, &client);

    TAP_EQ_INT(s_configured_x, 123,
            "a requested x wins over the centered policy");
    TAP_EQ_INT(s_configured_y, 77,
            "a requested y wins over the centered policy");
}


/* A transient asking for exactly (0, 0) is toolkit boilerplate rather
 * than a deliberate choice, so that one request is not honored */
static void s_test_apply_ignores_junk_transient_origin(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        CONFIG_PLACEMENT_POLICY_CENTERED;
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;
    client.transient_for = 99u;
    client.hints_icccm.size.has_position = true;
    client.hints_icccm.size.req_pos.x = 0;
    client.hints_icccm.size.req_pos.y = 0;

    place_window_apply(&wm, &stage, &client);

    /* No parent is findable here (both stand-ins answer nothing), so
     * the transient centering below declines too and the configured
     * policy has the last word.  What matters is only that (0, 0) was
     * not taken at its word. */
    TAP_OK(s_configured_x != 0 || s_configured_y != 0,
            "a transient asking for (0, 0) is not pinned to the" \
            " screen corner");
}


/* CONFIG_PLACEMENT_POLICY_SMART reaches the smart search and lands
 * somewhere wholly inside the workarea */
static void s_test_apply_smart_stays_inside_workarea(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_SMART;
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    place_window_apply(&wm, &stage, &client);

    TAP_EQ_INT(s_configure_calls, 1,
            "the smart policy places the window exactly once");
    TAP_OK(s_configured_x >= 0 && s_configured_y >= 0 &&
            s_configured_x + 200 <= 1000 &&
            s_configured_y + 100 <= 800,
            "the smart policy lands wholly inside the workarea");
}


/* CONFIG_PLACEMENT_POLICY_MANUAL settles the window where the smart
 * policy would have put it, that being both where the outline starts
 * and where the window stays if nobody answers */
static void s_test_apply_manual_matches_smart(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;
    int32_t smart_x;
    int32_t smart_y;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    config.base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_SMART;
    place_window_apply(&wm, &stage, &client);
    smart_x = s_configured_x;
    smart_y = s_configured_y;

    /* The stage is rebuilt as well as the stubs: 's_reset_stubs'
     * clears the desktop registry 'stage_desktop_get' answers from,
     * and a placement running without a desktop falls back on
     * different workarea bounds, which would compare two different
     * questions rather than two policies */
    s_reset_stubs();
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;
    config.base.windows.placement_policy =
        CONFIG_PLACEMENT_POLICY_MANUAL;
    place_window_apply(&wm, &stage, &client);

    TAP_EQ_INT(s_configured_x, smart_x,
            "the manual policy starts from the smart position in x");
    TAP_EQ_INT(s_configured_y, smart_y,
            "the manual policy starts from the smart position in y");
}


/* An unrecognized/"none" policy leaves an already-valid position
 * alone entirely, never even reaching xcb_configure_window */
static void s_test_apply_none_leaves_valid_position(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        (enum config_placement_policy_e) 999; /* unrecognized */
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.pos.x = 50;
    client.layout.geometry.cur.pos.y = 60;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    place_window_apply(&wm, &stage, &client);

    TAP_EQ_INT(s_configure_calls, 0,
            "an already on-screen position is left untouched entirely");
}


/* An unrecognized/"none" policy still clamps a position that would
 * otherwise start above or left of the screen/workarea */
static void s_test_apply_none_clamps_offscreen_position(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        (enum config_placement_policy_e) 999;
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.pos.x = -30;
    client.layout.geometry.cur.pos.y = -10;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    place_window_apply(&wm, &stage, &client);

    TAP_EQ_INT(s_configure_calls, 1,
            "an off-screen starting position is corrected");
    TAP_EQ_INT(s_configured_x, 0, "x is clamped to never be negative");
    TAP_EQ_INT(s_configured_y, 0,
            "y is clamped to the workarea top (0 here, the full" \
            " screen fallback)");
}


/* A transient client is centered over its already-managed parent,
 * short-circuiting the rest of place_apply's own policy logic
 * entirely */
static void s_test_apply_transient_centers_over_parent(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;
    client_td parent;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_CASCADE;
    s_make_stage(&stage, &desktop, 1000u, 800u);

    memset(&parent, 0, sizeof(parent));
    parent.layout.geometry.cur.pos.x = 100;
    parent.layout.geometry.cur.pos.y = 100;
    parent.layout.geometry.cur.dim.w = 600u;
    parent.layout.geometry.cur.dim.h = 400u;
    s_found_client = &parent;

    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.transient_for = 42u; /* any non-zero window id */
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    place_window_apply(&wm, &stage, &client);

    /* parent center: x=100+(600-200)/2=300, y=100+(400-100)/2=250 */
    TAP_EQ_INT(s_configured_x, 300, "centered horizontally over the parent");
    TAP_EQ_INT(s_configured_y, 250, "centered vertically over the parent");
}


/* CONFIG_PLACEMENT_POLICY_CASCADE delegates entirely to
 * place_apply_cascade and returns without running any of place_
 * apply's own other policy branches */
static void s_test_apply_cascade_delegates(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_CASCADE;
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    place_window_apply(&wm, &stage, &client);

    TAP_EQ_INT(s_configure_calls, 1,
            "cascade delegation still results in exactly one" \
            " configure call");
}


/* CONFIG_PLACEMENT_POLICY_UNDER_MOUSE centers the client on the
 * current pointer position */
static void s_test_apply_under_mouse(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;
    xcb_query_pointer_reply_t reply;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        CONFIG_PLACEMENT_POLICY_UNDER_MOUSE;
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    memset(&reply, 0, sizeof(reply));
    reply.root_x = 500;
    reply.root_y = 400;
    s_pointer_reply = &reply;

    place_window_apply(&wm, &stage, &client);

    /* 500 - 200/2 = 400; 400 - 100/2 = 350 */
    TAP_EQ_INT(s_configured_x, 400, "centered horizontally on the pointer");
    TAP_EQ_INT(s_configured_y, 350, "centered vertically on the pointer");
}


/* CONFIG_PLACEMENT_POLICY_UNDER_MOUSE with a failed pointer query
 * leaves the window entirely alone, rather than placing it at some
 * arbitrary fallback position */
static void s_test_apply_under_mouse_query_fails(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        CONFIG_PLACEMENT_POLICY_UNDER_MOUSE;
    s_make_stage(&stage, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    s_pointer_reply = NULL; /* query fails */

    place_window_apply(&wm, &stage, &client);

    TAP_EQ_INT(s_configure_calls, 0,
            "a failed pointer query leaves the window untouched");
}


/* When windows.group-related is on, a new client whose own
 * WM_CLIENT_LEADER matches an already-mapped sibling on the same
 * desktop is placed offset from that sibling instead of running the
 * configured placement policy at all */
static size_t s_ptr_hash1(const void *key)
{
    return (size_t) (uintptr_t) key;
}

static size_t s_ptr_hash2(const void *key)
{
    (void) key;
    return 1u;
}

static bool s_ptr_match(const void *key1, const void *key2)
{
    return key1 == key2;
}

static void s_test_apply_groups_with_sibling(void)
{
    wm_td wm;
    config_td config;
    stage_td stage;
    desktop_td desktop;
    client_td client;
    client_td sibling;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_CENTERED;
    config.base.windows.group_related = true;
    s_make_stage(&stage, &desktop, 1000u, 800u);

    memset(&sibling, 0, sizeof(sibling));
    sibling.hints_icccm.hints.client_leader = 42u;
    sibling.properties.state = (uint16_t) CLIENT_STATE_NORMAL;
    sibling.layout.geometry.cur.pos.x = 300;
    sibling.layout.geometry.cur.pos.y = 200;

    desktop.clients = ohtbl_init(8, 8, s_ptr_hash1, s_ptr_hash2,
            s_ptr_match, NULL);
    ohtbl_insert(desktop.clients, &sibling);

    memset(&client, 0, sizeof(client));
    /* A real window ID: placement now reaches the server through
     * 'utils/xcb/window.h', which does nothing for 'XCB_WINDOW_NONE',
     * so a client left at zero would never reach the stub below */
    client.window = 1u;
    /* same group as 'sibling' */
    client.hints_icccm.hints.client_leader = 42u;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    place_window_apply(&wm, &stage, &client);

    /* One visible sibling: offset by exactly one cascade_step (24) */
    TAP_EQ_INT(s_configured_x, 324,
            "placed offset from its own group's sibling, not" \
            " centered by the configured policy");
    TAP_EQ_INT(s_configured_y, 224,
            "offset in both x and y from the sibling's own position");

    ohtbl_destroy(desktop.clients);
}


int main(void)
{
    /* Placement reaches the server through 'utils/xcb/window.h', which
     * does nothing at all without a connection.  The stubs below are
     * what actually answer, so any non-null pointer will do: it is
     * never dereferenced, only checked for being there. */
    xcb_connection_set((xcb_connection_t *) &s_placement_conn);

    TAP_PLAN(30);

    s_test_cascade_advances_by_one_step();
    s_test_cascade_stays_inside_workarea();
    s_test_cascade_starts_new_column();
    s_test_cascade_guards();
    s_test_apply_guards();
    s_test_apply_centered();
    s_test_apply_splash_centers_on_workarea();
    s_test_apply_honors_requested_position();
    s_test_apply_ignores_junk_transient_origin();
    s_test_apply_smart_stays_inside_workarea();
    s_test_apply_manual_matches_smart();
    s_test_apply_none_leaves_valid_position();
    s_test_apply_none_clamps_offscreen_position();
    s_test_apply_transient_centers_over_parent();
    s_test_apply_cascade_delegates();
    s_test_apply_under_mouse();
    s_test_apply_under_mouse_query_fails();
    s_test_apply_groups_with_sibling();

    return TAP_DONE();
}
