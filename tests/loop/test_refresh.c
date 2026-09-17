/**
 * @file tests/loop/test_refresh.c
 *
 * @brief Test battery for the main loop's end-of-iteration repaint and
 *        EWMH resync (loop/refresh.c)
 *
 * loop_refresh, loop_refresh_full, and the three file-local helpers
 * they share are all real, testable list-walking and flag-checking
 * logic against a real cdlist of real 'struct stage_s' values built
 * on the stack: which stages are outdated, whether an overlay's
 * timeout has actually elapsed, and whether an EWMH resync is called
 * at all afterward.  None of that needs a live X connection itself;
 * only stage_render_all_desktops/stage_render_current_desktop_
 * repaint (the actual per-desktop XCB drawing), popup_close/notify_
 * desktop_close (each overlay's own dismissal logic), and wm_ewmh_sync
 * (a real EWMH property write) are link-only stand-ins below, each
 * simply recording that it was reached and with which stage, rather
 * than genuinely drawing anything or writing to a real root window.
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
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <stage.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <loop/context.h>
#include <loop/refresh.h>


/** Link-only stand-in for logger_msg (logger.c): a silent no-op,
 *  matching the real logger's own behavior whenever logger_start has
 *  never run (its first check is 'logger == NULL'), the same
 *  reasoning tests/wm/test_lifecycle.c already documents for never
 *  calling logger_start at all here either */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/* Controllable results/recording for the overlay-related stand-ins */
static bool s_popup_is_open;
static int s_popup_ms_remaining;
static int s_popup_close_calls;
static bool s_notify_desktop_is_open;
static int s_notify_desktop_ms_remaining;
static int s_notify_desktop_close_calls;

/* Recording for the render/sync stand-ins */
static int s_render_all_desktops_calls;
static const stage_td *s_render_all_desktops_last_stage;
static int s_render_all_desktops_return_value;
static int s_render_current_desktop_repaint_calls;
static const stage_td *s_render_current_desktop_repaint_last_stage;
static int s_wm_ewmh_sync_calls;


static void s_reset(void)
{
    s_popup_is_open = false;
    s_popup_ms_remaining = -1;
    s_popup_close_calls = 0;
    s_notify_desktop_is_open = false;
    s_notify_desktop_ms_remaining = -1;
    s_notify_desktop_close_calls = 0;
    s_render_all_desktops_calls = 0;
    s_render_all_desktops_last_stage = NULL;
    s_render_all_desktops_return_value = 0;
    s_render_current_desktop_repaint_calls = 0;
    s_render_current_desktop_repaint_last_stage = NULL;
    s_wm_ewmh_sync_calls = 0;
}


/** Link-only stand-in for xcb_connection_get (utils/xcb/connection.c) */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/** Link-only stand-in for popup_is_open (menu/popup.c) */
bool popup_is_open(void)
{
    return s_popup_is_open;
}


/** Link-only stand-in for popup_ms_remaining (menu/popup.c) */
int popup_ms_remaining(void)
{
    return s_popup_ms_remaining;
}


/** Link-only stand-in for popup_close (menu/popup.c) */
void popup_close(xcb_connection_t *connection)
{
    (void) connection;
    s_popup_close_calls++;
}


/** Link-only stand-in for notify_desktop_is_open (menu/notify/
 *  desktop.c) */
bool notify_desktop_is_open(void)
{
    return s_notify_desktop_is_open;
}


/** Link-only stand-in for notify_desktop_ms_remaining (menu/notify/
 *  desktop.c) */
int notify_desktop_ms_remaining(void)
{
    return s_notify_desktop_ms_remaining;
}


/** Link-only stand-in for notify_desktop_close (menu/notify/
 *  desktop.c) */
void notify_desktop_close(xcb_connection_t *connection)
{
    (void) connection;
    s_notify_desktop_close_calls++;
}


/** Link-only stand-in for stage_render_all_desktops
 *  (render/stage.c) */
int stage_render_all_desktops(stage_td *stage)
{
    s_render_all_desktops_calls++;
    s_render_all_desktops_last_stage = stage;
    return s_render_all_desktops_return_value;
}


/** Link-only stand-in for stage_render_current_desktop_repaint
 *  (render/stage.c) */
void stage_render_current_desktop_repaint(stage_td *stage)
{
    s_render_current_desktop_repaint_calls++;
    s_render_current_desktop_repaint_last_stage = stage;
}


/** Link-only stand-in for wm_ewmh_sync (wm/ewmh.c) */
void wm_ewmh_sync(wm_td *wm)
{
    (void) wm;
    s_wm_ewmh_sync_calls++;
}


/**
 * @brief Build a loop context around a real, caller-owned stages
 *        list
 */
static loop_ctx_td s_make_ctx(list_td *stages)
{
    loop_ctx_td ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.stages = stages;
    return ctx;
}


/* loop_refresh on a null ctx, or a ctx with a NULL stages list, is a
 * safe no-op that reaches none of the stand-ins above */
static void s_test_refresh_null_guards(void)
{
    loop_ctx_td ctx_no_stages = s_make_ctx(NULL);

    s_reset();
    loop_refresh(NULL);
    TAP_EQ_INT(s_render_all_desktops_calls, 0,
            "loop_refresh on a null ctx renders no stage");
    TAP_EQ_INT(s_wm_ewmh_sync_calls, 0,
            "loop_refresh on a null ctx never resyncs EWMH");

    s_reset();
    loop_refresh(&ctx_no_stages);
    TAP_EQ_INT(s_render_all_desktops_calls, 0,
            "loop_refresh with a NULL stages list renders no"
            " stage");
    TAP_EQ_INT(s_wm_ewmh_sync_calls, 0,
            "loop_refresh with a NULL stages list never resyncs"
            " EWMH");
}


/* With nothing outdated and neither overlay open, loop_refresh renders
 * nothing and never resyncs EWMH */
static void s_test_refresh_nothing_outdated(void)
{
    list_td *stages = list_init(NULL);
    stage_td stage_a;
    loop_ctx_td ctx = s_make_ctx(stages);

    memset(&stage_a, 0, sizeof(stage_a));
    stage_a.is_outdated = false;
    list_ins_next(stages, NULL, &stage_a);

    s_reset();
    loop_refresh(&ctx);

    TAP_EQ_INT(s_render_all_desktops_calls, 0,
            "loop_refresh renders nothing when no stage is"
            " outdated");
    TAP_EQ_INT(s_wm_ewmh_sync_calls, 0,
            "loop_refresh never resyncs EWMH when nothing was"
            " outdated");

    list_destroy(stages);
}


/* An outdated stage is rendered, and, since something was outdated,
 * an EWMH resync follows; an up-to-date stage alongside it is left
 * alone */
static void s_test_refresh_renders_outdated_and_resyncs(void)
{
    list_td *stages = list_init(NULL);
    stage_td stage_outdated;
    stage_td stage_clean;
    loop_ctx_td ctx = s_make_ctx(stages);

    memset(&stage_outdated, 0, sizeof(stage_outdated));
    memset(&stage_clean, 0, sizeof(stage_clean));
    stage_outdated.is_outdated = true;
    stage_clean.is_outdated = false;
    list_ins_next(stages, NULL, &stage_outdated);
    list_ins_next(stages, list_tail(stages), &stage_clean);

    s_reset();
    loop_refresh(&ctx);

    TAP_EQ_INT(s_render_all_desktops_calls, 1,
            "loop_refresh renders exactly the one outdated stage");
    TAP_OK(s_render_all_desktops_last_stage == &stage_outdated,
            "loop_refresh renders the outdated stage itself, not"
            " the clean one");
    TAP_EQ_INT(s_wm_ewmh_sync_calls, 1,
            "loop_refresh resyncs EWMH exactly once when something"
            " was outdated");

    list_destroy(stages);
}


/* An open popup whose countdown has not yet reached 0 is left alone:
 * neither closed nor repainted for */
static void s_test_refresh_popup_open_not_expired(void)
{
    list_td *stages = list_init(NULL);
    stage_td stage_a;
    loop_ctx_td ctx = s_make_ctx(stages);

    memset(&stage_a, 0, sizeof(stage_a));
    list_ins_next(stages, NULL, &stage_a);

    s_reset();
    s_popup_is_open = true;
    s_popup_ms_remaining = 50;

    loop_refresh(&ctx);

    TAP_EQ_INT(s_popup_close_calls, 0,
            "an open popup with time still remaining is not closed");
    TAP_EQ_INT(s_render_current_desktop_repaint_calls, 0,
            "an open popup with time still remaining triggers no"
            " overlay repaint");

    list_destroy(stages);
}


/* An open popup whose countdown has reached exactly 0 is closed, and
 * the first stage in the list is repainted to clear its remnants */
static void s_test_refresh_popup_expired_closes_and_repaints(void)
{
    list_td *stages = list_init(NULL);
    stage_td stage_first;
    stage_td stage_second;
    loop_ctx_td ctx = s_make_ctx(stages);

    memset(&stage_first, 0, sizeof(stage_first));
    memset(&stage_second, 0, sizeof(stage_second));
    list_ins_next(stages, NULL, &stage_first);
    list_ins_next(stages, list_tail(stages), &stage_second);

    s_reset();
    s_popup_is_open = true;
    s_popup_ms_remaining = 0;

    loop_refresh(&ctx);

    TAP_EQ_INT(s_popup_close_calls, 1,
            "an expired popup is closed exactly once");
    TAP_EQ_INT(s_render_current_desktop_repaint_calls, 1,
            "closing an expired popup triggers exactly one overlay"
            " repaint");
    TAP_OK(s_render_current_desktop_repaint_last_stage ==
                &stage_first,
            "the overlay repaint targets the first stage in the"
            " list, not any other");

    list_destroy(stages);
}


/* The desktop-switch notification is closed and repainted for
 * independently of the popup, following the exact same expiry rule */
static void s_test_refresh_notify_desktop_expired(void)
{
    list_td *stages = list_init(NULL);
    stage_td stage_a;
    loop_ctx_td ctx = s_make_ctx(stages);

    memset(&stage_a, 0, sizeof(stage_a));
    list_ins_next(stages, NULL, &stage_a);

    s_reset();
    s_notify_desktop_is_open = true;
    s_notify_desktop_ms_remaining = 0;

    loop_refresh(&ctx);

    TAP_EQ_INT(s_notify_desktop_close_calls, 1,
            "an expired desktop-switch notification is closed"
            " exactly once");
    TAP_EQ_INT(s_popup_close_calls, 0,
            "closing the desktop-switch notification never closes"
            " the popup too");
    TAP_EQ_INT(s_render_current_desktop_repaint_calls, 1,
            "closing the expired notification triggers exactly one"
            " overlay repaint");

    list_destroy(stages);
}


/* loop_refresh_full on a null ctx, or a ctx with a NULL stages list,
 * is a safe no-op */
static void s_test_refresh_full_null_guards(void)
{
    loop_ctx_td ctx_no_stages = s_make_ctx(NULL);

    s_reset();
    loop_refresh_full(NULL);
    TAP_EQ_INT(s_render_all_desktops_calls, 0,
            "loop_refresh_full on a null ctx renders no stage");

    s_reset();
    loop_refresh_full(&ctx_no_stages);
    TAP_EQ_INT(s_render_all_desktops_calls, 0,
            "loop_refresh_full with a NULL stages list renders no"
            " stage");
}


/* loop_refresh_full marks every stage outdated first, then renders
 * every one of them, including one that started out already up to
 * date */
static void s_test_refresh_full_marks_and_renders_every_stage(void)
{
    list_td *stages = list_init(NULL);
    stage_td stage_a;
    stage_td stage_b;
    loop_ctx_td ctx = s_make_ctx(stages);

    memset(&stage_a, 0, sizeof(stage_a));
    memset(&stage_b, 0, sizeof(stage_b));
    stage_a.is_outdated = false;
    stage_b.is_outdated = false;
    list_ins_next(stages, NULL, &stage_a);
    list_ins_next(stages, list_tail(stages), &stage_b);

    s_reset();
    loop_refresh_full(&ctx);

    TAP_OK(stage_a.is_outdated && stage_b.is_outdated,
            "loop_refresh_full marks every stage outdated,"
            " including ones that started out clean");
    TAP_EQ_INT(s_render_all_desktops_calls, 2,
            "loop_refresh_full renders every stage exactly once");

    list_destroy(stages);
}


/* loop_refresh_full itself never resyncs EWMH, unlike loop_refresh:
 * that is loop_run's own separate, later call, not this function's
 * job */
static void s_test_refresh_full_never_resyncs_ewmh(void)
{
    list_td *stages = list_init(NULL);
    stage_td stage_a;
    loop_ctx_td ctx = s_make_ctx(stages);

    memset(&stage_a, 0, sizeof(stage_a));
    list_ins_next(stages, NULL, &stage_a);

    s_reset();
    loop_refresh_full(&ctx);

    TAP_EQ_INT(s_wm_ewmh_sync_calls, 0,
            "loop_refresh_full never calls wm_ewmh_sync itself");

    list_destroy(stages);
}


/* A NULL entry in the middle of the stages list is skipped cleanly
 * by loop_refresh_full's own marking pass, without crashing */
static void s_test_refresh_full_skips_null_entries(void)
{
    list_td *stages = list_init(NULL);
    stage_td stage_a;
    loop_ctx_td ctx = s_make_ctx(stages);

    memset(&stage_a, 0, sizeof(stage_a));
    list_ins_next(stages, NULL, NULL);
    list_ins_next(stages, list_tail(stages), &stage_a);

    s_reset();
    loop_refresh_full(&ctx);

    TAP_OK(stage_a.is_outdated,
            "loop_refresh_full still marks the real stage outdated"
            " past a NULL entry ahead of it in the list");
    TAP_EQ_INT(s_render_all_desktops_calls, 1,
            "loop_refresh_full renders exactly the one real stage,"
            " skipping the NULL entry without crashing");

    list_destroy(stages);
}


int main(void)
{
    TAP_PLAN(24);

    s_test_refresh_null_guards();
    s_test_refresh_nothing_outdated();
    s_test_refresh_renders_outdated_and_resyncs();
    s_test_refresh_popup_open_not_expired();
    s_test_refresh_popup_expired_closes_and_repaints();
    s_test_refresh_notify_desktop_expired();
    s_test_refresh_full_null_guards();
    s_test_refresh_full_marks_and_renders_every_stage();
    s_test_refresh_full_never_resyncs_ewmh();
    s_test_refresh_full_skips_null_entries();

    return TAP_DONE();
}
