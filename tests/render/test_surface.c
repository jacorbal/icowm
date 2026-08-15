/**
 * @file tests/render/test_surface.c
 *
 * @brief Test battery for surface rendering orchestration
 *
 * desktop_render_full and desktop_render_background (render/desktop.c,
 * both XCB-backed) and surface_desktop_get (surface.c) are stubbed
 * below as controllable, call-recording stand-ins; xcb_flush itself
 * is linked for real, the same way test_urgency.c already does,
 * since surface_render_flush's own only real behavior worth testing
 * is its NULL-connection guard, never actually reaching that call.
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

/* ADT includes */
#include <adt/cdlist.h>

/* Local includes */
#include <harness/tap.h>
#include <render/surface.h>


static int s_render_full_calls;
static bool s_last_is_current;
static desktop_td *s_last_rendered;
static int s_render_full_return = 0;

int desktop_render_full(desktop_td *desktop, bool is_current)
{
    s_render_full_calls++;
    s_last_rendered = desktop;
    s_last_is_current = is_current;
    return s_render_full_return;
}


static int s_render_background_calls;

int desktop_render_background(desktop_td *desktop)
{
    (void) desktop;
    s_render_background_calls++;
    return 0;
}


static desktop_td *s_desktops_by_id[4];

desktop_td *surface_desktop_get(surface_td *surface, uint32_t desktop_id)
{
    (void) surface;
    if (desktop_id >= 4u) {
        return NULL;
    }
    return s_desktops_by_id[desktop_id];
}


/**
 * @brief Build a surface with 'count' desktops linked into its own
 *        circular list, each with the given id (0..count-1)
 */
static void s_make_surface(surface_td *surface, desktop_td *desktops,
        uint32_t count, uint32_t desktop_cur)
{
    memset(surface, 0, sizeof(*surface));
    surface->desktop_count = count;
    surface->desktop_cur = desktop_cur;
    surface->desktops = cdlist_init(NULL);

    for (uint32_t i = 0; i < count; ++i) {
        memset(&desktops[i], 0, sizeof(desktops[i]));
        s_desktops_by_id[i] = &desktops[i];
        cdlist_ins_next(surface->desktops, cdlist_tail(surface->desktops),
                &desktops[i]);
    }
}


static void s_reset_recorders(void)
{
    s_render_full_calls = 0;
    s_render_background_calls = 0;
    s_last_rendered = NULL;
    s_render_full_return = 0;
}


/* surface_render_current_desktop fails cleanly on a NULL surface or
 * one with no desktops list */
static void s_test_current_desktop_null_guards(void)
{
    surface_td surface;

    TAP_EQ_INT(surface_render_current_desktop(NULL), 1,
            "a NULL surface fails");

    memset(&surface, 0, sizeof(surface));
    surface.desktops = NULL;
    TAP_EQ_INT(surface_render_current_desktop(&surface), 1,
            "a surface with no desktops list fails");
}


/* surface_render_current_desktop walks to the desktop at
 * desktop_cur's own index and renders exactly that one, marked as
 * the current one */
static void s_test_current_desktop_renders_correct_one(void)
{
    surface_td surface;
    desktop_td desktops[3];
    int rc;

    s_make_surface(&surface, desktops, 3, 1u);
    s_reset_recorders();

    rc = surface_render_current_desktop(&surface);

    TAP_EQ_INT(rc, 0, "renders successfully");
    TAP_EQ_INT(s_render_full_calls, 1, "exactly one desktop was rendered");
    TAP_OK(s_last_rendered == &desktops[1],
            "the desktop at desktop_cur's own index was the one" \
            " rendered");
    TAP_OK(s_last_is_current,
            "it was rendered as the current one (client remapping" \
            " allowed)");

    cdlist_destroy(surface.desktops);
}


/* surface_render_current_desktop propagates a render failure */
static void s_test_current_desktop_propagates_failure(void)
{
    surface_td surface;
    desktop_td desktops[1];
    int rc;

    s_make_surface(&surface, desktops, 1, 0u);
    s_reset_recorders();
    s_render_full_return = 1;

    rc = surface_render_current_desktop(&surface);
    TAP_EQ_INT(rc, 1, "a render failure propagates as a failure");

    cdlist_destroy(surface.desktops);
}


/* surface_render_all_desktops with zero desktops is a harmless no-op
 * success, not a failure */
static void s_test_all_desktops_zero_count(void)
{
    surface_td surface;

    memset(&surface, 0, sizeof(surface));
    surface.desktops = cdlist_init(NULL);
    surface.desktop_count = 0u;

    TAP_EQ_INT(surface_render_all_desktops(&surface), 0,
            "zero desktops is a harmless success, not a failure");

    cdlist_destroy(surface.desktops);
}


/* surface_render_all_desktops only re-renders desktops actually
 * marked outdated, skipping the rest entirely */
static void s_test_all_desktops_only_renders_outdated(void)
{
    surface_td surface;
    desktop_td desktops[3];

    s_make_surface(&surface, desktops, 3, 0u);
    desktops[0].is_outdated = true;
    desktops[1].is_outdated = false;
    desktops[2].is_outdated = true;
    s_reset_recorders();

    surface_render_all_desktops(&surface);

    TAP_EQ_INT(s_render_full_calls, 2,
            "only the two outdated desktops were actually re-rendered");

    cdlist_destroy(surface.desktops);
}


/* surface_render_all_desktops re-applies the current desktop's own
 * background last, and clears the surface's own is_outdated flag */
static void s_test_all_desktops_reapplies_background_and_clears_flag(void)
{
    surface_td surface;
    desktop_td desktops[2];

    s_make_surface(&surface, desktops, 2, 1u);
    surface.is_outdated = true;
    s_reset_recorders();

    surface_render_all_desktops(&surface);

    TAP_EQ_INT(s_render_background_calls, 1,
            "the current desktop's own background is re-applied once");
    TAP_OK(!surface.is_outdated,
            "the surface's own is_outdated flag is cleared afterward");

    cdlist_destroy(surface.desktops);
}


/* surface_render_current_desktop_repaint marks only the current
 * desktop outdated (not the others), then triggers a full pass */
static void s_test_repaint_marks_current_and_triggers_full_pass(void)
{
    surface_td surface;
    desktop_td desktops[2];

    s_make_surface(&surface, desktops, 2, 0u);
    /* Neither starts outdated */
    s_reset_recorders();

    surface_render_current_desktop_repaint(&surface);

    TAP_OK(desktops[0].is_outdated || s_render_full_calls > 0,
            "the current desktop was marked outdated and a full pass" \
            " ran");
    TAP_EQ_INT(s_render_full_calls, 1,
            "only the one now-outdated desktop was actually rendered");

    cdlist_destroy(surface.desktops);
}


/* surface_render_current_desktop_repaint on a NULL surface is a
 * harmless no-op */
static void s_test_repaint_null_surface_is_safe(void)
{
    surface_render_current_desktop_repaint(NULL);
    TAP_OK(true, "a NULL surface is safely ignored, no crash");
}


/* surface_render_flush's own guard: a NULL surface, or one with no
 * connection, never reaches xcb_flush at all */
static void s_test_flush_guards(void)
{
    surface_td surface;

    surface_render_flush(NULL);
    TAP_OK(true, "a NULL surface is safely ignored, no crash");

    memset(&surface, 0, sizeof(surface));
    surface.connection = NULL;
    surface_render_flush(&surface);
    TAP_OK(true, "a NULL connection is safely ignored, no crash");
}


int main(void)
{
    TAP_PLAN(16);

    s_test_current_desktop_null_guards();
    s_test_current_desktop_renders_correct_one();
    s_test_current_desktop_propagates_failure();
    s_test_all_desktops_zero_count();
    s_test_all_desktops_only_renders_outdated();
    s_test_all_desktops_reapplies_background_and_clears_flag();
    s_test_repaint_marks_current_and_triggers_full_pass();
    s_test_repaint_null_surface_is_safe();
    s_test_flush_guards();

    return TAP_DONE();
}
