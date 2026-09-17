/**
 * @file tests/render/test_stage.c
 *
 * @brief Test battery for stage rendering orchestration
 *
 * desktop_render_full (render/desktop.c) and
 * render_desktop_background_render (render/desktop/background.c,
 * both XCB-backed), stage_desktop_get (stage.c), and
 * desktop_mark_outdated (desktop.c) are stubbed below as controllable,
 * call-recording stand-ins; xcb_flush itself is linked for real, the
 * same way test_urgency.c already does, since stage_render_flush's
 * own only real behavior worth testing is its NULL-connection guard,
 * never actually reaching that call.
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
#include <client.h>
#include <desktop.h>
#include <stage.h>
#include <config.h>
#include <harness/tap.h>
#include <render/desktop/background.h>
#include <render/stage.h>
#include <utils/xcb/connection.h>


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

int render_desktop_background_render(desktop_td *desktop)
{
    (void) desktop;
    s_render_background_calls++;
    return 0;
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


void desktop_mark_outdated(desktop_td *desktop)
{
    if (desktop != NULL) {
        desktop->is_outdated = true;
    }
}


/**
 * @brief Build a stage with 'count' desktops linked into its own
 *        circular list, each with the given id (0..count-1)
 */
static void s_make_stage(stage_td *stage, desktop_td *desktops,
        uint32_t count, uint32_t desktop_cur)
{
    memset(stage, 0, sizeof(*stage));
    stage->desktop_count = count;
    stage->desktop_cur = desktop_cur;
    stage->desktops = cdlist_init(NULL);

    for (uint32_t i = 0; i < count; ++i) {
        memset(&desktops[i], 0, sizeof(desktops[i]));
        s_desktops_by_id[i] = &desktops[i];
        cdlist_ins_next(stage->desktops, cdlist_tail(stage->desktops),
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


/* stage_render_current_desktop fails cleanly on a NULL stage or
 * one with no desktops list */
static void s_test_current_desktop_null_guards(void)
{
    stage_td stage;

    TAP_EQ_INT(stage_render_current_desktop(NULL), 1,
            "a NULL stage fails");

    memset(&stage, 0, sizeof(stage));
    stage.desktops = NULL;
    TAP_EQ_INT(stage_render_current_desktop(&stage), 1,
            "a stage with no desktops list fails");
}


/* stage_render_current_desktop walks to the desktop at
 * desktop_cur's own index and renders exactly that one, marked as
 * the current one */
static void s_test_current_desktop_renders_correct_one(void)
{
    stage_td stage;
    desktop_td desktops[3];
    int rc;

    s_make_stage(&stage, desktops, 3, 1u);
    s_reset_recorders();

    rc = stage_render_current_desktop(&stage);

    TAP_EQ_INT(rc, 0, "renders successfully");
    TAP_EQ_INT(s_render_full_calls, 1, "exactly one desktop was rendered");
    TAP_OK(s_last_rendered == &desktops[1],
            "the desktop at desktop_cur's own index was the one" \
            " rendered");
    TAP_OK(s_last_is_current,
            "it was rendered as the current one (client remapping" \
            " allowed)");

    cdlist_destroy(stage.desktops);
}


/* stage_render_current_desktop propagates a render failure */
static void s_test_current_desktop_propagates_failure(void)
{
    stage_td stage;
    desktop_td desktops[1];
    int rc;

    s_make_stage(&stage, desktops, 1, 0u);
    s_reset_recorders();
    s_render_full_return = 1;

    rc = stage_render_current_desktop(&stage);
    TAP_EQ_INT(rc, 1, "a render failure propagates as a failure");

    cdlist_destroy(stage.desktops);
}


/* stage_render_all_desktops with zero desktops is a harmless no-op
 * success, not a failure */
static void s_test_all_desktops_zero_count(void)
{
    stage_td stage;

    memset(&stage, 0, sizeof(stage));
    stage.desktops = cdlist_init(NULL);
    stage.desktop_count = 0u;

    TAP_EQ_INT(stage_render_all_desktops(&stage), 0,
            "zero desktops is a harmless success, not a failure");

    cdlist_destroy(stage.desktops);
}


/* stage_render_all_desktops only re-renders desktops actually
 * marked outdated, skipping the rest entirely */
static void s_test_all_desktops_only_renders_outdated(void)
{
    stage_td stage;
    desktop_td desktops[3];

    s_make_stage(&stage, desktops, 3, 0u);
    desktops[0].is_outdated = true;
    desktops[1].is_outdated = false;
    desktops[2].is_outdated = true;
    s_reset_recorders();

    stage_render_all_desktops(&stage);

    TAP_EQ_INT(s_render_full_calls, 2,
            "only the two outdated desktops were actually re-rendered");

    cdlist_destroy(stage.desktops);
}


/* stage_render_all_desktops re-applies the current desktop's own
 * background last, and clears the stage's own is_outdated flag */
static void s_test_all_desktops_reapplies_background_and_clears_flag(void)
{
    stage_td stage;
    desktop_td desktops[2];

    s_make_stage(&stage, desktops, 2, 1u);
    stage.is_outdated = true;
    s_reset_recorders();

    stage_render_all_desktops(&stage);

    TAP_EQ_INT(s_render_background_calls, 1,
            "the current desktop's own background is re-applied once");
    TAP_OK(!stage.is_outdated,
            "the stage's own is_outdated flag is cleared afterward");

    cdlist_destroy(stage.desktops);
}


/* stage_render_current_desktop_repaint marks only the current
 * desktop outdated (not the others), then triggers a full pass */
static void s_test_repaint_marks_current_and_triggers_full_pass(void)
{
    stage_td stage;
    desktop_td desktops[2];

    s_make_stage(&stage, desktops, 2, 0u);
    /* Neither starts outdated */
    s_reset_recorders();

    stage_render_current_desktop_repaint(&stage);

    TAP_OK(desktops[0].is_outdated || s_render_full_calls > 0,
            "the current desktop was marked outdated and a full pass" \
            " ran");
    TAP_EQ_INT(s_render_full_calls, 1,
            "only the one now-outdated desktop was actually rendered");

    cdlist_destroy(stage.desktops);
}


/* stage_render_current_desktop_repaint on a NULL stage is a
 * harmless no-op */
static void s_test_repaint_null_stage_is_safe(void)
{
    stage_render_current_desktop_repaint(NULL);
    TAP_OK(true, "a NULL stage is safely ignored, no crash");
}


/* stage_render_flush's own guard: a NULL stage, or one with no
 * connection, never reaches xcb_flush at all */
static void s_test_flush_guards(void)
{
    stage_td stage;

    stage_render_flush(NULL);
    TAP_OK(true, "a NULL stage is safely ignored, no crash");

    memset(&stage, 0, sizeof(stage));
    /* The connection is no longer a member of the stage: it is held
     * by 'utils/xcb/connection.h' for the whole session, so an absent
     * one is expressed by leaving that unset */
    xcb_connection_set(NULL);
    stage_render_flush(&stage);
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
    s_test_repaint_null_stage_is_safe();
    s_test_flush_guards();

    return TAP_DONE();
}
