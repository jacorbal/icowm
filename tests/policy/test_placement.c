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
 * surface_desktop_get, surface_primary_monitor,
 * surface_monitor_for_point, and lookup_find_client (all real
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
#include <harness/tap.h>
#include <policy/placement.h>
#include <wm/internal.h>


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
    (void) c;
    (void) cookie;
    (void) e;

    if (s_pointer_reply == NULL) {
        return NULL;
    }
    /* The real reply is malloc'd and free()'d by the caller; this
     * stand-in returns a fresh heap copy each time for the same
     * reason, so a caller's own free() is always valid */
    xcb_query_pointer_reply_t *copy =
        malloc(sizeof(xcb_query_pointer_reply_t));
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
    (void) c;
    (void) cookie;
    (void) e;

    if (s_geometry_reply == NULL) {
        return NULL;
    }
    xcb_get_geometry_reply_t *copy =
        malloc(sizeof(xcb_get_geometry_reply_t));
    *copy = *s_geometry_reply;
    return copy;
}


/* ===== Project stand-ins ===== */

static desktop_td *s_desktops_by_id[4];

desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    if (desktop_id >= 4u) {
        return NULL;
    }
    return s_desktops_by_id[desktop_id];
}


static monitor_td s_primary_monitor;

monitor_td surface_primary_monitor(const surface_td *surface)
{
    (void) surface;
    return s_primary_monitor;
}


static monitor_td s_monitor_for_point;

monitor_td surface_monitor_for_point(const surface_td *surface,
        int32_t x, int32_t y)
{
    (void) surface;
    (void) x;
    (void) y;
    return s_monitor_for_point;
}


static client_td *s_found_client = NULL;

client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **out_surface, desktop_td **out_desktop)
{
    (void) surfaces;
    (void) window;
    if (out_surface != NULL) {
        *out_surface = NULL;
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
 * @brief Build a minimal, single-monitor surface (monitor_count <= 1
 *        so s_clip_to_monitor's own clipping never engages) with one
 *        desktop at index 0
 */
static xcb_screen_t s_dummy_screen;

static void s_make_surface(surface_td *surface, desktop_td *desktop,
        uint32_t screen_w, uint32_t screen_h)
{
    memset(surface, 0, sizeof(*surface));
    memset(desktop, 0, sizeof(*desktop));
    memset(&s_dummy_screen, 0, sizeof(s_dummy_screen));
    surface->screen = &s_dummy_screen;
    surface->properties.dim.w = screen_w;
    surface->properties.dim.h = screen_h;
    surface->desktop_count = 1u;
    surface->desktop_cur = 0u;
    surface->monitor_count = 1u;
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


/* place_smart's own guard clauses: any missing required argument
 * fails cleanly */
static void s_test_smart_guards(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;
    int32_t out_x = 0;
    int32_t out_y = 0;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_surface(&surface, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));

    TAP_OK(!place_smart(NULL, &surface, &client, &out_x, &out_y),
            "a NULL wm fails");
    TAP_OK(!place_smart(&wm, NULL, &client, &out_x, &out_y),
            "a NULL surface fails");
    TAP_OK(!place_smart(&wm, &surface, NULL, &out_x, &out_y),
            "a NULL client fails");
    TAP_OK(!place_smart(&wm, &surface, &client, NULL, &out_y),
            "a NULL out_x fails");
}


/* On an empty desktop, place_smart seeds and keeps the centered
 * candidate: zero overlap cost there ends the search immediately */
static void s_test_smart_empty_desktop_centers(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;
    int32_t out_x = -1;
    int32_t out_y = -1;
    bool rc;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_surface(&surface, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    rc = place_smart(&wm, &surface, &client, &out_x, &out_y);

    /* center_x = 500, center_y = 400; centered candidate:
     * cx = 500 - 100 = 400, cy = 400 - 50 = 350 */
    TAP_OK(rc, "place_smart succeeds on an empty desktop");
    TAP_EQ_INT(out_x, 400, "centers horizontally on an empty desktop");
    TAP_EQ_INT(out_y, 350, "centers vertically on an empty desktop");
}


/* With the centered spot already occupied by another visible
 * client, place_smart finds a genuinely different, non-overlapping
 * position instead of settling for the occupied center */
static void s_test_smart_avoids_occupying_client(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;
    client_td occupant;
    int32_t out_x = -1;
    int32_t out_y = -1;
    bool overlaps;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_surface(&surface, &desktop, 1000u, 800u);

    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    /* Occupies exactly the spot the centered candidate would use */
    memset(&occupant, 0, sizeof(occupant));
    occupant.layout.geometry.cur.pos.x = 400;
    occupant.layout.geometry.cur.pos.y = 350;
    occupant.layout.geometry.cur.dim.w = 200u;
    occupant.layout.geometry.cur.dim.h = 100u;

    desktop.stacking = cdlist_init(NULL);
    cdlist_ins_next(desktop.stacking, NULL, &occupant);

    place_smart(&wm, &surface, &client, &out_x, &out_y);

    overlaps = (out_x < 400 + 200) && (400 < out_x + 200) &&
        (out_y < 350 + 100) && (350 < out_y + 100);
    TAP_OK(!overlaps,
            "a non-overlapping position was found instead of the" \
            " occupied center");

    cdlist_destroy(desktop.stacking);
}


/* An iconified client with a mapped icon is scored as a (lighter)
 * obstacle too, not ignored entirely: place_smart steers clear of
 * its icon position as well when a same-cost-or-better alternative
 * exists */
static void s_test_smart_considers_mapped_icons(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;
    client_td iconified;
    int32_t out_x = -1;
    int32_t out_y = -1;
    bool centered_on_icon;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_surface(&surface, &desktop, 1000u, 800u);

    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    memset(&iconified, 0, sizeof(iconified));
    iconified.properties.state = (uint16_t) CLIENT_STATE_ICONIFIED;
    iconified.icon_window = 123u;
    iconified.is_icon_mapped = true;
    iconified.icon_x = 400;
    iconified.icon_y = 350;

    desktop.stacking = cdlist_init(NULL);
    cdlist_ins_next(desktop.stacking, NULL, &iconified);

    place_smart(&wm, &surface, &client, &out_x, &out_y);

    /* The exact centered candidate (400,350) now has a nonzero cost
     * (the icon overlap penalty), so the search should not settle
     * there when scanning finds a genuinely zero-cost alternative
     * elsewhere on an otherwise-empty 1000x800 desktop */
    centered_on_icon = (out_x == 400 && out_y == 350);
    TAP_OK(!centered_on_icon,
            "an icon's own position is not treated as perfectly free");

    cdlist_destroy(desktop.stacking);
}


/* place_smart respects the desktop's own workarea (panel struts)
 * instead of the full screen when one is set */
static void s_test_smart_respects_workarea(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;
    int32_t out_x = -1;
    int32_t out_y = -1;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_surface(&surface, &desktop, 1000u, 800u);
    /* A panel reserves the top 100px: workarea starts at y=100 */
    desktop.workarea.pos.x = 0;
    desktop.workarea.pos.y = 100;
    desktop.workarea.dim.w = 1000u;
    desktop.workarea.dim.h = 700u;

    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    place_smart(&wm, &surface, &client, &out_x, &out_y);

    /* workarea center_y = 100 + 700/2 = 450; centered cy = 450-50 = 400 */
    TAP_EQ_INT(out_y, 400,
            "the workarea's own top offset is respected, not the" \
            " full screen's");
}


/* place_smart clips its candidate range to the reference monitor on
 * a multi-monitor surface, never placing outside it */
static void s_test_smart_clips_to_monitor(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;
    int32_t out_x = -1;
    int32_t out_y = -1;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_surface(&surface, &desktop, 2000u, 800u);
    surface.monitor_count = 2u;
    /* Second monitor: x in [1000, 2000) */
    s_primary_monitor.x = 1000;
    s_primary_monitor.y = 0;
    s_primary_monitor.w = 1000u;
    s_primary_monitor.h = 800u;

    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    place_smart(&wm, &surface, &client, &out_x, &out_y);

    TAP_OK(out_x >= 1000 && (uint32_t) out_x + 200u <= 2000u,
            "placement stays within the second monitor's own bounds," \
            " not the whole combined screen");
}


/* place_smart fails cleanly when the surface has no current desktop
 * to place onto at all */
static void s_test_smart_no_desktop_fails(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;
    int32_t out_x = -1;
    int32_t out_y = -1;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_surface(&surface, &desktop, 1000u, 800u);
    s_desktops_by_id[0] = NULL;  /* surface_desktop_get now finds nothing */
    memset(&client, 0, sizeof(client));

    TAP_OK(!place_smart(&wm, &surface, &client, &out_x, &out_y),
            "no current desktop to place onto: fails cleanly");
}


/* place_apply_cascade advances its own shared sequence counter by
 * exactly one cascade_step (24px) per call, regardless of whatever
 * value that counter already held coming in from other tests or
 * calls */
static void s_test_cascade_advances_by_one_step(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
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
    s_make_surface(&surface, &desktop, 2000u, 1600u);
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    place_apply_cascade(&wm, &surface, &client);
    x1 = s_configured_x;
    y1 = s_configured_y;

    place_apply_cascade(&wm, &surface, &client);
    x2 = s_configured_x;
    y2 = s_configured_y;

    wrapped = (x2 < x1);
    TAP_OK(wrapped || (x2 == x1 + 24 && y2 == y1 + 24),
            "each call advances the shared cascade position by" \
            " exactly one 24px step (or wraps back to the start)");
}


/* Over a full cycle of the sequence (determined by how many steps
 * fit the workarea), the cascade position returns to exactly where
 * it started: the sequence wraps rather than running off the edge
 * of the workarea */
static void s_test_cascade_wraps_around(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;
    int32_t x_before;
    int32_t y_before;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    /* max_steps = min((248-200)/24, (148-100)/24) = min(2, 2) = 2 */
    s_make_surface(&surface, &desktop, 248u, 148u);
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    place_apply_cascade(&wm, &surface, &client);
    x_before = s_configured_x;
    y_before = s_configured_y;

    /* Exactly one full cycle later (max_steps == 2 calls), the
     * position must be identical again */
    place_apply_cascade(&wm, &surface, &client);
    place_apply_cascade(&wm, &surface, &client);

    TAP_EQ_INT(s_configured_x, x_before,
            "after one full cycle, the cascade x position repeats");
    TAP_EQ_INT(s_configured_y, y_before,
            "after one full cycle, the cascade y position repeats");
}


/* place_apply_cascade's own guard clauses: any missing required
 * argument is a safe no-op */
static void s_test_cascade_guards(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_surface(&surface, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));

    place_apply_cascade(NULL, &surface, &client);
    place_apply_cascade(&wm, NULL, &client);
    place_apply_cascade(&wm, &surface, NULL);

    TAP_EQ_INT(s_configure_calls, 0,
            "missing required arguments never reach xcb_configure_window");
}


/* place_apply's own guard clauses: any missing required argument is
 * a safe no-op */
static void s_test_apply_guards(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    s_make_surface(&surface, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));

    place_apply(NULL, &surface, &client);
    place_apply(&wm, NULL, &client);
    place_apply(&wm, &surface, NULL);

    TAP_EQ_INT(s_configure_calls, 0,
            "missing required arguments never reach xcb_configure_window");
}


/* CONFIG_PLACEMENT_POLICY_CENTERED centers the client on the
 * workarea, clamped so it never starts left of or above it */
static void s_test_apply_centered(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_CENTERED;
    s_make_surface(&surface, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    place_apply(&wm, &surface, &client);

    /* (1000-200)/2 = 400; (800-100)/2 = 350 */
    TAP_EQ_INT(s_configured_x, 400, "centered horizontally on the workarea");
    TAP_EQ_INT(s_configured_y, 350, "centered vertically on the workarea");
}


/* An unrecognized/"none" policy leaves an already-valid position
 * alone entirely, never even reaching xcb_configure_window */
static void s_test_apply_none_leaves_valid_position(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        (enum config_placement_policy_e) 999; /* unrecognized */
    s_make_surface(&surface, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = 50;
    client.layout.geometry.cur.pos.y = 60;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    place_apply(&wm, &surface, &client);

    TAP_EQ_INT(s_configure_calls, 0,
            "an already on-screen position is left untouched entirely");
}


/* An unrecognized/"none" policy still clamps a position that would
 * otherwise start above or left of the screen/workarea */
static void s_test_apply_none_clamps_offscreen_position(void)
{
    wm_td wm;
    config_td config;
    surface_td surface;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        (enum config_placement_policy_e) 999;
    s_make_surface(&surface, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.pos.x = -30;
    client.layout.geometry.cur.pos.y = -10;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    place_apply(&wm, &surface, &client);

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
    surface_td surface;
    desktop_td desktop;
    client_td client;
    client_td parent;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_CASCADE;
    s_make_surface(&surface, &desktop, 1000u, 800u);

    memset(&parent, 0, sizeof(parent));
    parent.layout.geometry.cur.pos.x = 100;
    parent.layout.geometry.cur.pos.y = 100;
    parent.layout.geometry.cur.dim.w = 600u;
    parent.layout.geometry.cur.dim.h = 400u;
    s_found_client = &parent;

    memset(&client, 0, sizeof(client));
    client.transient_for = 42u; /* any non-zero window id */
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    place_apply(&wm, &surface, &client);

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
    surface_td surface;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_CASCADE;
    s_make_surface(&surface, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;

    place_apply(&wm, &surface, &client);

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
    surface_td surface;
    desktop_td desktop;
    client_td client;
    xcb_query_pointer_reply_t reply;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        CONFIG_PLACEMENT_POLICY_UNDER_MOUSE;
    s_make_surface(&surface, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    memset(&reply, 0, sizeof(reply));
    reply.root_x = 500;
    reply.root_y = 400;
    s_pointer_reply = &reply;

    place_apply(&wm, &surface, &client);

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
    surface_td surface;
    desktop_td desktop;
    client_td client;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy =
        CONFIG_PLACEMENT_POLICY_UNDER_MOUSE;
    s_make_surface(&surface, &desktop, 1000u, 800u);
    memset(&client, 0, sizeof(client));
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    s_pointer_reply = NULL; /* query fails */

    place_apply(&wm, &surface, &client);

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
    surface_td surface;
    desktop_td desktop;
    client_td client;
    client_td sibling;

    s_reset_stubs();
    s_make_wm(&wm, &config);
    config.base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_CENTERED;
    config.base.windows.group_related = true;
    s_make_surface(&surface, &desktop, 1000u, 800u);

    memset(&sibling, 0, sizeof(sibling));
    sibling.client_leader = 42u;
    sibling.properties.state = (uint16_t) CLIENT_STATE_NORMAL;
    sibling.layout.geometry.cur.pos.x = 300;
    sibling.layout.geometry.cur.pos.y = 200;

    desktop.clients = ohtbl_init(8, 8, s_ptr_hash1, s_ptr_hash2,
            s_ptr_match, NULL);
    ohtbl_insert(desktop.clients, &sibling);

    memset(&client, 0, sizeof(client));
    client.client_leader = 42u;  /* same group as 'sibling' */
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 100u;
    client.layout.gravity = (uint16_t) CLIENT_GRAVITY_NORTH_WEST;

    place_apply(&wm, &surface, &client);

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
    TAP_PLAN(31);

    s_test_smart_guards();
    s_test_smart_empty_desktop_centers();
    s_test_smart_avoids_occupying_client();
    s_test_smart_considers_mapped_icons();
    s_test_smart_respects_workarea();
    s_test_smart_clips_to_monitor();
    s_test_smart_no_desktop_fails();
    s_test_cascade_advances_by_one_step();
    s_test_cascade_wraps_around();
    s_test_cascade_guards();
    s_test_apply_guards();
    s_test_apply_centered();
    s_test_apply_none_leaves_valid_position();
    s_test_apply_none_clamps_offscreen_position();
    s_test_apply_transient_centers_over_parent();
    s_test_apply_cascade_delegates();
    s_test_apply_under_mouse();
    s_test_apply_under_mouse_query_fails();
    s_test_apply_groups_with_sibling();

    return TAP_DONE();
}
