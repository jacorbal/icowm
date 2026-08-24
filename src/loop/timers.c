/**
 * @file loop/timers.c
 *
 * @brief Countdown aggregation for the main event loop
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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Policy includes */
#include <policy/urgency.h>

/* Input includes */
#include <input/mouse/drag/warp.h>
#include <input/mouse/hover.h>

/* Menu includes */
#include <menu/dialog/confirm.h>
#include <menu/dialog/message.h>
#include <menu/notify/desktop.h>
#include <menu/popup.h>

/* Default initial values */
#include <defs/loop.h>

/* Project includes */
#include <cctl/kill.h>
#include <cctl/sn.h>
#include <memguard.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>
#include <wm/shutdown.h>

/* Local includes */
#include <loop/timers.h>


/**
 * @brief Tighten a poll timeout to a candidate deadline, if sooner
 *
 * Shared by every countdown check in @a loop_timers_timeout, which
 * otherwise each repeated the same "is this candidate both valid and
 * sooner than what we already have" test.
 *
 * @param poll_timeout_ms Current timeout, in milliseconds; lowered in
 *                        place when @p candidate_ms is sooner
 * @param candidate_ms    A countdown's own remaining time, or a
 *                        negative value when that countdown is not
 *                        currently active at all
 *
 * @note Complexity: @e O(1)
 */
static void s_loop_timers_tighten(int *poll_timeout_ms,
        int candidate_ms)
{
    if (candidate_ms >= 0 && candidate_ms < *poll_timeout_ms) {
        *poll_timeout_ms = candidate_ms;
    }
}


/* Compute how long the loop may block waiting for input */
int loop_timers_timeout(const loop_ctx_td *ctx)
{
    int poll_timeout_ms = WM_EVENT_POLL_TIMEOUT_MS;

    if (ctx == NULL) {
        return poll_timeout_ms;
    }

    /* The info popup and the desktop-switch notification are the two
     * that report a remaining time even while closed, so both are
     * asked only while actually open */
    if (popup_is_open()) {
        s_loop_timers_tighten(&poll_timeout_ms, popup_ms_remaining());
    }

    if (notify_desktop_is_open()) {
        s_loop_timers_tighten(&poll_timeout_ms,
                notify_desktop_ms_remaining());
    }

    s_loop_timers_tighten(&poll_timeout_ms,
            systray_clock_ms_remaining());

    s_loop_timers_tighten(&poll_timeout_ms,
            urgency_blink_ms_remaining(ctx->config));

    s_loop_timers_tighten(&poll_timeout_ms, cctl_sn_ms_remaining());

    /* Shorter still while a resize-cursor poll target is being
     * tracked (see 'mouse_hover_poll_tick' in input/mouse/hover.h),
     * so an undecorated client's cursor gets re-evaluated promptly as
     * the pointer moves within it. */
    s_loop_timers_tighten(&poll_timeout_ms,
            mouse_hover_poll_ms_remaining());

    /* Shorter still while a confirm dialog has a timer of its own
     * running (see 'menu_confirm_dialog_tick' in menu/dialog/
     * confirm.h): a pending click-triggered close/accept, or a
     * countdown timeout that needs its visible number to advance once
     * a second and, once it fully elapses, to act. */
    s_loop_timers_tighten(&poll_timeout_ms,
            menu_confirm_dialog_ms_remaining());

    /* Shorter still while the message dialog has a click-triggered
     * close of its own pending (see 'menu_message_dialog_tick' in
     * menu/dialog/message.h). */
    s_loop_timers_tighten(&poll_timeout_ms,
            menu_message_dialog_ms_remaining());

    /* Shorter still while a window drag is holding the pointer
     * against a warp-eligible screen edge (see 'drag_warp_tick' in
     * input/mouse/drag.h), so it still switches desktops once its own
     * countdown elapses even with no further 'MotionNotify' arriving
     * to drive it. */
    s_loop_timers_tighten(&poll_timeout_ms, drag_warp_ms_remaining());

    /* Shorter still while a coordinated shutdown (see
     * 'wm_shutdown_tick' in wm/shutdown.h) is waiting on managed
     * clients to close on their own, so the timeout that forces the
     * rest closed elapses promptly instead of waiting for the next
     * unrelated event to wake the loop up. */
    s_loop_timers_tighten(&poll_timeout_ms, wm_shutdown_ms_remaining());

    /* Shorter still while a process kill is pending escalation to
     * 'SIGKILL' (see 'cctl_kill_tick' in wm/kill.h), for the same
     * reason. */
    s_loop_timers_tighten(&poll_timeout_ms, cctl_kill_ms_remaining());

    return poll_timeout_ms;
}


/* Give every countdown its chance to act */
void loop_timers_tick(const loop_ctx_td *ctx)
{
    if (ctx == NULL) {
        return;
    }

    systray_clock_tick();
    urgency_blink_tick(ctx->surfaces, ctx->config);
    cctl_sn_tick(ctx->connection, ctx->surfaces);
    mouse_hover_poll_tick(ctx->connection, ctx->surfaces);
    menu_confirm_dialog_tick(ctx->connection, ctx->config);
    menu_message_dialog_tick(ctx->connection);
    drag_warp_tick(ctx->connection);
    wm_shutdown_tick(ctx->wm);
    cctl_kill_tick();

    /* The memory guard runs against one surface only, and only when
     * the window manager was started with a cap at all */
    if (ctx->restricted_memory_mib > 0u && ctx->surfaces != NULL &&
            !list_is_empty(ctx->surfaces)) {
        memguard_tick(ctx->connection,
                (surface_td *) list_data(list_head(ctx->surfaces)),
                ctx->config);
    }
}
