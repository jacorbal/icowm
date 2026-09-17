/**
 * @file tests/loop/test_timers.c
 *
 * @brief Test battery for the main loop's countdown aggregation
 *        (loop/timers.c)
 *
 * Both loop_timers_timeout and loop_timers_tick are, past their own
 * null-ctx guard, a flat sequence of calls out to more than a dozen
 * other subsystems' own "how long until I need attention" and "act on
 * it" pair; none of the arithmetic or branching that combines their
 * answers into a single poll timeout, or that decides whether the
 * memory guard runs at all, belongs to any of those subsystems, so
 * every one of them is a link-only stand-in below whose return value
 * a scenario controls directly, and s_loop_timers_tighten (itself
 * static to timers.c, reachable only through loop_timers_timeout) is
 * exercised through every one of its call sites instead of directly.
 * memguard_tick's own guard, list_is_empty and list_head/list_data
 * against a real cdlist, is the one piece of genuinely testable
 * branching loop_timers_tick itself owns beyond forwarding, so
 * adt/list.c is linked for real below rather than stood in for.
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

/* Local includes */
#include <harness/tap.h>
#include <loop/context.h>
#include <loop/timers.h>
#include <stage.h>


/* Controllable return values for every X_ms_remaining stand-in below,
 * one static per subsystem, each defaulting to -1 (not currently
 * active) unless a scenario sets it */
static int s_popup_ms;
static int s_notify_desktop_ms;
static int s_systray_clock_ms;
static int s_urgency_blink_ms;
static int s_ping_ms;
static int s_cctl_sn_ms;
static int s_mouse_hover_poll_ms;
static int s_mouse_enter_focus_ms;
static int s_menu_confirm_dialog_ms;
static int s_menu_message_dialog_ms;
static int s_drag_warp_ms;
static int s_drag_pan_ms;
static int s_viewport_edge_ms;
static int s_wm_shutdown_ms;
static int s_cctl_kill_ms;
static int s_place_manual_ms;

/* Whether popup_is_open/notify_desktop_is_open report open, gating
 * whether loop_timers_timeout even asks their own ms_remaining at
 * all */
static bool s_popup_is_open;
static bool s_notify_desktop_is_open;

/* Call counters for loop_timers_tick's own forwarding, one per
 * subsystem tick function */
static int s_tick_calls[14];
enum {
    S_TICK_SYSTRAY = 0,
    S_TICK_URGENCY,
    S_TICK_PING,
    S_TICK_CCTL_SN,
    S_TICK_HOVER,
    S_TICK_ENTER_FOCUS,
    S_TICK_CONFIRM,
    S_TICK_MESSAGE,
    S_TICK_WARP,
    S_TICK_PAN,
    S_TICK_VIEWPORT_EDGE,
    S_TICK_SHUTDOWN,
    S_TICK_KILL,
    S_TICK_PLACE_MANUAL
};

/* Recording for memguard_tick, the one tick call that is conditional */
static int s_memguard_tick_calls;
static const stage_td *s_memguard_last_stage;


static void s_reset(void)
{
    s_popup_ms = -1;
    s_notify_desktop_ms = -1;
    s_systray_clock_ms = -1;
    s_urgency_blink_ms = -1;
    s_ping_ms = -1;
    s_cctl_sn_ms = -1;
    s_mouse_hover_poll_ms = -1;
    s_mouse_enter_focus_ms = -1;
    s_menu_confirm_dialog_ms = -1;
    s_menu_message_dialog_ms = -1;
    s_drag_warp_ms = -1;
    s_drag_pan_ms = -1;
    s_viewport_edge_ms = -1;
    s_wm_shutdown_ms = -1;
    s_cctl_kill_ms = -1;
    s_place_manual_ms = -1;
    s_popup_is_open = false;
    s_notify_desktop_is_open = false;
    memset(s_tick_calls, 0, sizeof(s_tick_calls));
    s_memguard_tick_calls = 0;
    s_memguard_last_stage = NULL;
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
    return s_popup_ms;
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
    return s_notify_desktop_ms;
}


/** Link-only stand-in for systray_clock_ms_remaining (systray.c) */
int systray_clock_ms_remaining(void)
{
    return s_systray_clock_ms;
}


/** Link-only stand-in for urgency_blink_ms_remaining (policy/
 *  urgency.c) */
int urgency_blink_ms_remaining(const config_td *config)
{
    (void) config;
    return s_urgency_blink_ms;
}


/** Link-only stand-in for ping_ms_remaining (policy/ping.c) */
int ping_ms_remaining(void)
{
    return s_ping_ms;
}


/** Link-only stand-in for cctl_sn_ms_remaining (cctl/sn.c) */
int cctl_sn_ms_remaining(void)
{
    return s_cctl_sn_ms;
}


/** Link-only stand-in for mouse_hover_poll_ms_remaining (input/mouse/
 *  hover.c) */
int mouse_hover_poll_ms_remaining(void)
{
    return s_mouse_hover_poll_ms;
}


/** Link-only stand-in for mouse_enter_focus_ms_remaining (input/mouse/
 *  event.c) */
int mouse_enter_focus_ms_remaining(void)
{
    return s_mouse_enter_focus_ms;
}


/** Link-only stand-in for menu_confirm_dialog_ms_remaining (menu/
 *  dialog/confirm.c) */
int menu_confirm_dialog_ms_remaining(void)
{
    return s_menu_confirm_dialog_ms;
}


/** Link-only stand-in for menu_message_dialog_ms_remaining (menu/
 *  dialog/message.c) */
int menu_message_dialog_ms_remaining(void)
{
    return s_menu_message_dialog_ms;
}


/** Link-only stand-in for drag_warp_ms_remaining (input/mouse/
 *  drag/warp.c) */
int drag_warp_ms_remaining(void)
{
    return s_drag_warp_ms;
}


/** Link-only stand-in for drag_pan_ms_remaining (input/mouse/
 *  drag/pan.c) */
int drag_pan_ms_remaining(void)
{
    return s_drag_pan_ms;
}


/** Link-only stand-in for mouse_viewport_edge_ms_remaining
 *  (input/mouse/viewport_edge.c) */
int mouse_viewport_edge_ms_remaining(void)
{
    return s_viewport_edge_ms;
}


/** Link-only stand-in for wm_shutdown_ms_remaining (wm/shutdown.c) */
int wm_shutdown_ms_remaining(void)
{
    return s_wm_shutdown_ms;
}


/** Link-only stand-in for cctl_kill_ms_remaining (cctl/kill.c) */
int cctl_kill_ms_remaining(void)
{
    return s_cctl_kill_ms;
}


/** Link-only stand-in for place_manual_ms_remaining (policy/
 *  placement/manual.c) */
int place_manual_ms_remaining(void)
{
    return s_place_manual_ms;
}


/** Link-only stand-in for systray_clock_tick (systray.c) */
void systray_clock_tick(void)
{
    s_tick_calls[S_TICK_SYSTRAY]++;
}


/** Link-only stand-in for urgency_blink_tick (policy/urgency.c) */
void urgency_blink_tick(list_td *stages, const config_td *config)
{
    (void) stages;
    (void) config;
    s_tick_calls[S_TICK_URGENCY]++;
}


/** Link-only stand-in for ping_tick (policy/ping.c) */
void ping_tick(list_td *stages)
{
    (void) stages;
    s_tick_calls[S_TICK_PING]++;
}


/** Link-only stand-in for cctl_sn_tick (cctl/sn.c) */
void cctl_sn_tick(xcb_connection_t *connection, list_td *stages)
{
    (void) connection;
    (void) stages;
    s_tick_calls[S_TICK_CCTL_SN]++;
}


/** Link-only stand-in for mouse_hover_poll_tick (input/mouse/hover.c) */
void mouse_hover_poll_tick(xcb_connection_t *connection, list_td *stages)
{
    (void) connection;
    (void) stages;
    s_tick_calls[S_TICK_HOVER]++;
}


/** Link-only stand-in for mouse_enter_focus_tick (input/mouse/
 *  event.c) */
void mouse_enter_focus_tick(list_td *stages, const config_td *config)
{
    (void) stages;
    (void) config;
    s_tick_calls[S_TICK_ENTER_FOCUS]++;
}


/** Link-only stand-in for menu_confirm_dialog_tick (menu/dialog/
 *  confirm.c) */
void menu_confirm_dialog_tick(xcb_connection_t *connection,
        const config_td *config)
{
    (void) connection;
    (void) config;
    s_tick_calls[S_TICK_CONFIRM]++;
}


/** Link-only stand-in for menu_message_dialog_tick (menu/dialog/
 *  message.c) */
void menu_message_dialog_tick(xcb_connection_t *connection)
{
    (void) connection;
    s_tick_calls[S_TICK_MESSAGE]++;
}


/** Link-only stand-in for drag_warp_tick (input/mouse/drag/warp.c) */
void drag_warp_tick(xcb_connection_t *connection)
{
    (void) connection;
    s_tick_calls[S_TICK_WARP]++;
}


/** Link-only stand-in for drag_pan_tick (input/mouse/drag/pan.c) */
void drag_pan_tick(xcb_connection_t *connection)
{
    (void) connection;
    s_tick_calls[S_TICK_PAN]++;
}


/** Link-only stand-in for mouse_viewport_edge_tick
 *  (input/mouse/viewport_edge.c) */
void mouse_viewport_edge_tick(xcb_connection_t *connection)
{
    (void) connection;
    s_tick_calls[S_TICK_VIEWPORT_EDGE]++;
}


/** Link-only stand-in for wm_shutdown_tick (wm/shutdown.c) */
void wm_shutdown_tick(const wm_td *wm)
{
    (void) wm;
    s_tick_calls[S_TICK_SHUTDOWN]++;
}


/** Link-only stand-in for cctl_kill_tick (cctl/kill.c) */
void cctl_kill_tick(void)
{
    s_tick_calls[S_TICK_KILL]++;
}


/** Link-only stand-in for place_manual_tick (policy/placement/
 *  manual.c) */
void place_manual_tick(xcb_connection_t *connection)
{
    (void) connection;
    s_tick_calls[S_TICK_PLACE_MANUAL]++;
}


/** Link-only stand-in for memguard_tick (memguard.c) */
void memguard_tick(xcb_connection_t *connection, stage_td *stage,
        const config_td *config)
{
    (void) connection;
    (void) config;
    s_memguard_tick_calls++;
    s_memguard_last_stage = stage;
}


/**
 * @brief Build a loop context with every countdown reporting inactive
 */
static loop_ctx_td s_make_ctx(void)
{
    loop_ctx_td ctx;

    memset(&ctx, 0, sizeof(ctx));
    return ctx;
}


/* A null ctx returns the plain fallback timeout, without touching any
 * of the stand-ins above at all */
static void s_test_timeout_null_ctx(void)
{
    s_reset();
    TAP_EQ_INT(loop_timers_timeout(NULL), 1000,
            "loop_timers_timeout on a null ctx returns"
            " WM_EVENT_POLL_TIMEOUT_MS");
}


/* With every countdown reporting inactive (-1) or simply not open, the
 * fallback 1000 ms timeout is returned unchanged */
static void s_test_timeout_all_inactive(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    TAP_EQ_INT(loop_timers_timeout(&ctx), 1000,
            "loop_timers_timeout with every countdown inactive"
            " returns the plain 1000 ms fallback");
}


/* popup_ms_remaining/notify_desktop_ms_remaining are asked only while
 * their own is_open reports true; a countdown reported while closed is
 * ignored entirely */
static void s_test_timeout_popup_asked_only_when_open(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    s_popup_is_open = false;
    s_popup_ms = 5;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 1000,
            "a closed popup's own ms_remaining is never even asked,"
            " so it cannot tighten the timeout");

    s_reset();
    s_popup_is_open = true;
    s_popup_ms = 5;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 5,
            "an open popup's ms_remaining tightens the timeout down"
            " to 5");

    s_reset();
    s_notify_desktop_is_open = true;
    s_notify_desktop_ms = 12;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 12,
            "an open desktop-switch notification tightens the"
            " timeout down to 12");
}


/* Every unconditional countdown (asked regardless of any is_open of
 * its own) tightens the timeout exactly the same way, to whichever is
 * soonest */
static void s_test_timeout_unconditional_countdowns_tighten(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    s_systray_clock_ms = 900;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 900,
            "systray_clock_ms_remaining tightens the timeout");

    s_reset();
    s_urgency_blink_ms = 400;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 400,
            "urgency_blink_ms_remaining tightens the timeout");

    s_reset();
    s_ping_ms = 300;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 300,
            "ping_ms_remaining tightens the timeout");

    s_reset();
    s_cctl_sn_ms = 250;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 250,
            "cctl_sn_ms_remaining tightens the timeout");

    s_reset();
    s_mouse_hover_poll_ms = 150;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 150,
            "mouse_hover_poll_ms_remaining tightens the timeout");

    s_reset();
    s_mouse_enter_focus_ms = 120;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 120,
            "mouse_enter_focus_ms_remaining tightens the timeout");

    s_reset();
    s_menu_confirm_dialog_ms = 80;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 80,
            "menu_confirm_dialog_ms_remaining tightens the timeout");

    s_reset();
    s_menu_message_dialog_ms = 60;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 60,
            "menu_message_dialog_ms_remaining tightens the timeout");

    s_reset();
    s_drag_warp_ms = 40;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 40,
            "drag_warp_ms_remaining tightens the timeout");

    s_reset();
    s_drag_pan_ms = 37;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 37,
            "drag_pan_ms_remaining tightens the timeout");

    s_reset();
    s_viewport_edge_ms = 35;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 35,
            "mouse_viewport_edge_ms_remaining tightens the timeout");

    s_reset();
    s_wm_shutdown_ms = 30;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 30,
            "wm_shutdown_ms_remaining tightens the timeout");

    s_reset();
    s_cctl_kill_ms = 20;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 20,
            "cctl_kill_ms_remaining tightens the timeout");

    s_reset();
    s_place_manual_ms = 10;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 10,
            "place_manual_ms_remaining tightens the timeout");
}


/* With several countdowns active at once, the soonest deadline wins,
 * regardless of which order s_loop_timers_tighten happens to see them
 * in */
static void s_test_timeout_soonest_wins(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    s_popup_is_open = true;
    s_popup_ms = 500;
    s_systray_clock_ms = 200;
    s_ping_ms = 999;
    s_cctl_kill_ms = 5;
    s_place_manual_ms = 300;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 5,
            "with several countdowns active, the single soonest"
            " deadline (5) wins over every longer one");
}


/* A negative candidate never tightens anything, and a candidate no
 * shorter than the current timeout is likewise left alone: only a
 * strictly shorter, non-negative candidate ever lowers it */
static void s_test_timeout_ties_and_negatives_do_not_tighten(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    s_systray_clock_ms = 1000;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 1000,
            "a candidate exactly equal to the current timeout does"
            " not tighten it further");

    s_reset();
    s_systray_clock_ms = 300;
    s_ping_ms = 300;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 300,
            "a second candidate tied with the first leaves the"
            " timeout unchanged at that same value");

    s_reset();
    s_systray_clock_ms = -1;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 1000,
            "a negative candidate (-1, i.e., not currently active)"
            " never tightens the timeout");

    s_reset();
    s_systray_clock_ms = 0;
    TAP_EQ_INT(loop_timers_timeout(&ctx), 0,
            "a candidate of exactly 0 does tighten the timeout, since"
            " only a strictly negative value marks a countdown"
            " inactive");
}


/* loop_timers_tick on a null ctx calls nothing at all */
static void s_test_tick_null_ctx(void)
{
    s_reset();
    loop_timers_tick(NULL);

    for (int i = 0; i < 14; ++i) {
        TAP_EQ_INT(s_tick_calls[i], 0,
                "loop_timers_tick on a null ctx calls no subsystem"
                " tick");
    }
    TAP_EQ_INT(s_memguard_tick_calls, 0,
            "loop_timers_tick on a null ctx never runs the memory"
            " guard either");
}


/* On a real ctx, every one of the twelve unconditional subsystem ticks
 * runs exactly once */
static void s_test_tick_calls_every_subsystem_once(void)
{
    loop_ctx_td ctx = s_make_ctx();

    s_reset();
    ctx.restricted_memory_mib = 0u;
    ctx.stages = NULL;
    loop_timers_tick(&ctx);

    for (int i = 0; i < 14; ++i) {
        TAP_EQ_INT(s_tick_calls[i], 1,
                "loop_timers_tick runs each unconditional subsystem"
                " tick exactly once");
    }
}


/* memguard_tick runs only when restricted_memory_mib is nonzero and
 * the stages list is both non-NULL and non-empty, and, when it does
 * run, is handed exactly the first stage in that list */
static void s_test_tick_memguard_gating(void)
{
    loop_ctx_td ctx = s_make_ctx();
    list_td *stages;
    stage_td stage_a;
    stage_td stage_b;

    memset(&stage_a, 0, sizeof(stage_a));
    memset(&stage_b, 0, sizeof(stage_b));

    s_reset();
    ctx.restricted_memory_mib = 0u;
    ctx.stages = NULL;
    loop_timers_tick(&ctx);
    TAP_EQ_INT(s_memguard_tick_calls, 0,
            "memguard_tick does not run with restricted_memory_mib"
            " at 0, even with no stages list at all");

    stages = list_init(NULL);
    s_reset();
    ctx.restricted_memory_mib = 128u;
    ctx.stages = stages;
    loop_timers_tick(&ctx);
    TAP_EQ_INT(s_memguard_tick_calls, 0,
            "memguard_tick does not run against an empty stages"
            " list, even with a nonzero memory cap");

    list_ins_next(stages, NULL, &stage_a);
    list_ins_next(stages, list_tail(stages), &stage_b);
    s_reset();
    ctx.restricted_memory_mib = 128u;
    ctx.stages = stages;
    loop_timers_tick(&ctx);
    TAP_EQ_INT(s_memguard_tick_calls, 1,
            "memguard_tick runs exactly once with a nonzero memory"
            " cap and a non-empty stages list");
    TAP_OK(s_memguard_last_stage == &stage_a,
            "memguard_tick is handed exactly the first stage in"
            " the list");

    list_destroy(stages);
}


int main(void)
{
    TAP_PLAN(57);

    s_test_timeout_null_ctx();
    s_test_timeout_all_inactive();
    s_test_timeout_popup_asked_only_when_open();
    s_test_timeout_unconditional_countdowns_tighten();
    s_test_timeout_soonest_wins();
    s_test_timeout_ties_and_negatives_do_not_tighten();
    s_test_tick_null_ctx();
    s_test_tick_calls_every_subsystem_once();
    s_test_tick_memguard_gating();

    return TAP_DONE();
}
