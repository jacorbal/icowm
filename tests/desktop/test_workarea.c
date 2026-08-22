/**
 * @file tests/desktop/test_workarea.c
 *
 * @brief Test battery for per-desktop work-area recomputation
 *
 * desktop_update_workarea (desktop.c) folds three independent
 * reservation sources into one rectangle: each stacked client's own
 * strut, the systray's own strut, and configured margins, with
 * margins applied only on whichever of a region's own edges actually
 * coincides with the surface's own edge.  desktop_init and the XCB
 * calls it alone makes (xcb_get_setup, xcb_screen_next, xcb_setup_
 * roots_iterator) are link-only here, never exercised: every desktop
 * below is built directly, not through desktop_init.
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

/* ADT includes */
#include <adt/cdlist.h>

/* Local includes */
#include <harness/tap.h>
#include <desktop.h>
#include <surface.h>


/** Link-only stand-ins: desktop_init's own XCB screen/setup calls,
 *  never reached since every desktop here is built directly */
const xcb_setup_t *xcb_get_setup(xcb_connection_t *c)
{
    (void) c;
    return NULL;
}

xcb_screen_iterator_t xcb_setup_roots_iterator(const xcb_setup_t *R)
{
    xcb_screen_iterator_t it;

    (void) R;
    memset(&it, 0, sizeof(it));
    return it;
}

void xcb_screen_next(xcb_screen_iterator_t *i)
{
    (void) i;
}


/** Link-only stand-ins: desktop_init's own error-path cleanup calls
 *  memguard_max_clients and client_destroy, never reached since every
 *  desktop here is built directly, not through desktop_init */
uint32_t memguard_max_clients(void)
{
    return 0u;
}

void client_destroy(client_td *client)
{
    (void) client;
}


/* Build a surface with a single monitor spanning the whole screen,
 * and one desktop with an empty (but real) stacking list */
static void s_make_surface(surface_td *surface, desktop_td *desktop,
        uint32_t screen_w, uint32_t screen_h)
{
    memset(surface, 0, sizeof(*surface));
    memset(desktop, 0, sizeof(*desktop));
    surface->properties.dim.w = screen_w;
    surface->properties.dim.h = screen_h;
    surface->monitor_count = 1u;
    surface->monitors[0].x = 0;
    surface->monitors[0].y = 0;
    surface->monitors[0].w = screen_w;
    surface->monitors[0].h = screen_h;
    desktop->stacking = cdlist_init(NULL);
}


static void s_destroy_surface(surface_td *surface, desktop_td *desktop)
{
    cdlist_destroy(desktop->stacking);
    (void) surface;
}


/* An empty desktop, no config, no systray: the workarea is simply
 * the full screen, unchanged */
static void s_test_empty_no_reservations(void)
{
    surface_td surface;
    desktop_td desktop;

    s_make_surface(&surface, &desktop, 800u, 600u);

    desktop_update_workarea(&desktop, &surface, NULL, NULL, false);

    TAP_EQ_INT(desktop.workarea.pos.x, 0, "no reservations: x is 0");
    TAP_EQ_INT(desktop.workarea.pos.y, 0, "no reservations: y is 0");
    TAP_EQ_INT((long) desktop.workarea.dim.w, 800,
            "no reservations: width is the full screen");
    TAP_EQ_INT((long) desktop.workarea.dim.h, 600,
            "no reservations: height is the full screen");

    s_destroy_surface(&surface, &desktop);
}


/* Configured margins, with every edge coinciding with the surface
 * (a single full-screen monitor), reserve space on all four sides */
static void s_test_margins_only(void)
{
    surface_td surface;
    desktop_td desktop;
    struct config_desktop_s cfg;

    s_make_surface(&surface, &desktop, 800u, 600u);
    memset(&cfg, 0, sizeof(cfg));
    cfg.margins.left = 10u;
    cfg.margins.right = 20u;
    cfg.margins.top = 5u;
    cfg.margins.bottom = 15u;

    desktop_update_workarea(&desktop, &surface, &cfg, NULL, false);

    TAP_EQ_INT(desktop.workarea.pos.x, 10, "margins: x shifts by left");
    TAP_EQ_INT(desktop.workarea.pos.y, 5, "margins: y shifts by top");
    TAP_EQ_INT((long) desktop.workarea.dim.w, 770,
            "margins: width shrinks by left+right (800-10-20)");
    TAP_EQ_INT((long) desktop.workarea.dim.h, 580,
            "margins: height shrinks by top+bottom (600-5-15)");

    s_destroy_surface(&surface, &desktop);
}


/* A single stacked client's own top strut reserves space at the top,
 * independent of any configured margin */
static void s_test_single_client_strut(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td client;

    s_make_surface(&surface, &desktop, 800u, 600u);
    memset(&client, 0, sizeof(client));
    client.layout.strut_partial.sides.top = 30u;
    /* Legacy strut, start==end==0: unbounded, always applies */
    cdlist_ins_next(desktop.stacking, NULL, &client);

    desktop_update_workarea(&desktop, &surface, NULL, NULL, false);

    TAP_EQ_INT(desktop.workarea.pos.y, 30,
            "a client's own top strut shifts the workarea down");
    TAP_EQ_INT((long) desktop.workarea.dim.h, 570,
            "and shrinks the height by that same amount");

    s_destroy_surface(&surface, &desktop);
}


/* Two clients both reserving the top edge: the larger of the two
 * wins, struts are not summed together */
static void s_test_multiple_struts_take_the_max(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td small_strut;
    client_td large_strut;

    s_make_surface(&surface, &desktop, 800u, 600u);
    memset(&small_strut, 0, sizeof(small_strut));
    memset(&large_strut, 0, sizeof(large_strut));
    small_strut.layout.strut_partial.sides.top = 20u;
    large_strut.layout.strut_partial.sides.top = 50u;
    cdlist_ins_next(desktop.stacking, NULL, &small_strut);
    cdlist_ins_next(desktop.stacking, NULL, &large_strut);

    desktop_update_workarea(&desktop, &surface, NULL, NULL, false);

    TAP_EQ_INT(desktop.workarea.pos.y, 50,
            "the larger of the two top struts wins, not their sum");

    s_destroy_surface(&surface, &desktop);
}


/* A configured margin and a client's own strut on the same edge both
 * apply, added together, rather than only the larger of the two */
static void s_test_margin_and_strut_are_additive(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td client;
    struct config_desktop_s cfg;

    s_make_surface(&surface, &desktop, 800u, 600u);
    memset(&client, 0, sizeof(client));
    client.layout.strut_partial.sides.top = 30u;
    cdlist_ins_next(desktop.stacking, NULL, &client);
    memset(&cfg, 0, sizeof(cfg));
    cfg.margins.top = 10u;

    desktop_update_workarea(&desktop, &surface, &cfg, NULL, false);

    TAP_EQ_INT(desktop.workarea.pos.y, 40,
            "a margin and a client strut on the same edge add" \
            " together (30+10), neither overrides the other");

    s_destroy_surface(&surface, &desktop);
}


/* ignore_struts skips every client and systray strut, but configured
 * margins still apply: the two are independent reservation sources */
static void s_test_ignore_struts_still_applies_margins(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td client;
    struct config_desktop_s cfg;

    s_make_surface(&surface, &desktop, 800u, 600u);
    memset(&client, 0, sizeof(client));
    client.layout.strut_partial.sides.top = 30u;
    cdlist_ins_next(desktop.stacking, NULL, &client);
    memset(&cfg, 0, sizeof(cfg));
    cfg.margins.top = 10u;

    desktop_update_workarea(&desktop, &surface, &cfg, NULL, true);

    TAP_EQ_INT(desktop.workarea.pos.y, 10,
            "ignore_struts skips the client's own strut (30), but" \
            " the configured margin (10) still applies");

    s_destroy_surface(&surface, &desktop);
}


/* A strut whose own start/end span does not overlap the region at
 * all is not folded in */
static void s_test_strut_out_of_range_is_ignored(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td client;

    s_make_surface(&surface, &desktop, 800u, 600u);
    memset(&client, 0, sizeof(client));
    client.layout.strut_partial.sides.top = 30u;
    /* Top strut's own horizontal span (x-range) is 900-1000, entirely
     * to the right of an 800-wide screen: never overlaps */
    client.layout.strut_partial.start.top = 900;
    client.layout.strut_partial.end.top = 1000;
    cdlist_ins_next(desktop.stacking, NULL, &client);

    desktop_update_workarea(&desktop, &surface, NULL, NULL, false);

    TAP_EQ_INT(desktop.workarea.pos.y, 0,
            "a strut whose own span falls entirely outside the" \
            " region is not folded in at all");

    s_destroy_surface(&surface, &desktop);
}


/* The systray's own strut folds in exactly the same way a client's
 * would, even though the systray is never a stacked client itself */
static void s_test_systray_strut_folds_in(void)
{
    surface_td surface;
    desktop_td desktop;
    struct strut_partial_s systray;

    s_make_surface(&surface, &desktop, 800u, 600u);
    memset(&systray, 0, sizeof(systray));
    systray.sides.right = 40u;

    desktop_update_workarea(&desktop, &surface, NULL, &systray, false);

    TAP_EQ_INT((long) desktop.workarea.dim.w, 760,
            "the systray's own right-edge strut shrinks the" \
            " workarea width (800-40)");

    s_destroy_surface(&surface, &desktop);
}


/* Reservations larger than the region itself clamp the resulting
 * dimension to 0, never negative */
static void s_test_oversized_strut_clamps_to_zero(void)
{
    surface_td surface;
    desktop_td desktop;
    client_td client;

    s_make_surface(&surface, &desktop, 800u, 600u);
    memset(&client, 0, sizeof(client));
    client.layout.strut_partial.sides.top = 700u;
    cdlist_ins_next(desktop.stacking, NULL, &client);

    desktop_update_workarea(&desktop, &surface, NULL, NULL, false);

    TAP_EQ_INT((long) desktop.workarea.dim.h, 0,
            "a strut taller than the screen clamps height to 0," \
            " not a negative value");

    s_destroy_surface(&surface, &desktop);
}


/* Per-monitor: a margin only applies on a monitor's own edge that
 * actually coincides with the surface's own edge, not on an internal
 * boundary between two side-by-side monitors */
static void s_test_monitor_margin_only_on_screen_edge(void)
{
    surface_td surface;
    desktop_td desktop;
    struct config_desktop_s cfg;

    memset(&surface, 0, sizeof(surface));
    memset(&desktop, 0, sizeof(desktop));
    desktop.stacking = cdlist_init(NULL);
    surface.properties.dim.w = 1600u;
    surface.properties.dim.h = 600u;
    surface.monitor_count = 2u;
    /* Left monitor: x=0 (at the screen's own left edge) */
    surface.monitors[0].x = 0;
    surface.monitors[0].y = 0;
    surface.monitors[0].w = 800u;
    surface.monitors[0].h = 600u;
    /* Right monitor: x=800 (its own left edge is an internal
     * boundary between the two monitors, not the screen's own) */
    surface.monitors[1].x = 800;
    surface.monitors[1].y = 0;
    surface.monitors[1].w = 800u;
    surface.monitors[1].h = 600u;

    memset(&cfg, 0, sizeof(cfg));
    cfg.margins.left = 20u;

    desktop_update_workarea(&desktop, &surface, &cfg, NULL, false);

    TAP_EQ_INT(desktop.monitor_workareas[0].pos.x, 20,
            "the left monitor's own left edge is the screen edge:" \
            " the left margin applies there");
    TAP_EQ_INT(desktop.monitor_workareas[1].pos.x, 800,
            "the right monitor's own left edge is an internal" \
            " boundary between monitors: the left margin does" \
            " not apply there");

    cdlist_destroy(desktop.stacking);
}


/* A NULL desktop or surface is a safe no-op */
static void s_test_null_guards(void)
{
    surface_td surface;
    desktop_td desktop;

    s_make_surface(&surface, &desktop, 800u, 600u);

    desktop_update_workarea(NULL, &surface, NULL, NULL, false);
    TAP_OK(true, "a NULL desktop is a safe no-op, no crash");

    desktop_update_workarea(&desktop, NULL, NULL, NULL, false);
    TAP_OK(true, "a NULL surface is a safe no-op, no crash");

    s_destroy_surface(&surface, &desktop);
}


int main(void)
{
    TAP_PLAN(20);

    s_test_empty_no_reservations();
    s_test_margins_only();
    s_test_single_client_strut();
    s_test_multiple_struts_take_the_max();
    s_test_margin_and_strut_are_additive();
    s_test_ignore_struts_still_applies_margins();
    s_test_strut_out_of_range_is_ignored();
    s_test_systray_strut_folds_in();
    s_test_oversized_strut_clamps_to_zero();
    s_test_monitor_margin_only_on_screen_edge();
    s_test_null_guards();

    return TAP_DONE();
}
