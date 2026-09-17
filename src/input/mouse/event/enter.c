/**
 * @file input/mouse/event/enter.c
 *
 * @brief Mouse enter-notify handling and hover-triggered focus state
 *
 * One of the files @c input/mouse/event/ is made of;
 * carries its @c s_enter_focus_active state, used by nothing
 * outside this file and @c handler_focus_in (@c handler/focus.c), which
 * clears it.  See @c input/mouse/event/press.c's comment for the
 * reasoning behind the three-way split.
 *
 * Also carries @c s_pending_window and @c s_pending_due, the state
 * behind @c windows.focus.delay-ms: with the delay left at its default
 * of 250, sloppy focus still applies the instant @a mouse_handle_enter
 * sees it, exactly as before this pair existed; set above @c 0, that
 * same function arms a deadline here instead of focusing right away,
 * left for @a mouse_enter_focus_tick to carry out once it elapses, or
 * for @a mouse_enter_focus_cancel to drop if the pointer leaves first.
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <time.h>       /* clock_gettime, struct timespec */

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Menu includes */
#include <menu/cycle.h>

/* Policy includes */
#include <policy/focus.h>

/* Utils includes */
#include <utils/time/clock.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <stage.h>

/* Local includes */
#include <input/mouse/cursor.h>
#include <input/mouse/event.h>
#include <input/mouse/hover.h>
#include <input/mouse/internal.h>


/**
 * @brief Flag set while a hover-triggered focus transfer is in flight
 *
 * Set in @a mouse_handle_enter before calling @a focus_apply and
 * cleared in @a handler_focus_in when the corresponding @c FocusIn
 * event arrives.
 */
static bool s_enter_focus_active = false;

/**
 * @brief Window a delayed sloppy-focus is currently pending for, or
 *        @c XCB_WINDOW_NONE for none
 *
 * Armed by @a mouse_handle_enter when @c windows.focus.delay-ms is
 * above 0, carried out by @a mouse_enter_focus_tick once
 * @a s_pending_due arrives, and dropped by @a mouse_enter_focus_cancel
 * if the pointer leaves @p s_pending_window first.
 */
static xcb_window_t s_pending_window = XCB_WINDOW_NONE;

/**
 * @brief Absolute time @a s_pending_window's delayed focus becomes due
 */
static struct timespec s_pending_due;


/* Re-evaluate the resize cursor, then apply focus-follows-mouse, on
 * an enter-notify event */
void mouse_handle_enter(xcb_connection_t *connection,
        list_td *stages, xcb_enter_notify_event_t *event,
        const config_td *config)
{
    client_td *client;
    client_td *entered;
    desktop_td *desktop;
    stage_td *stage;

    if (connection == NULL || event == NULL || config == NULL) {
        return;
    }

    LOGGER_TRACE("Enter notify (event=0x%x, child=0x%x," \
            " root=%+d%+d, mode=%u, detail=%u)",
            event->event, event->child, event->root_x, event->root_y,
            event->mode, event->detail);

    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        return;
    }

    /* Independent of focus-follows-mouse below.  A resizable client
     * that selects 'PointerMotion' for its purposes (common in GTK/Qt
     * applications tracking hover for their UI) intercepts motion
     * events at the X11 propagation level before they ever reach
     * 'mouse_handle_motion_hover', so the cursor set while hovering
     * this client's border never gets re-evaluated once the pointer
     * moves on into that client's content area; this 'EnterNotify',
     * unlike motion, still fires reliably since it was selected
     * directly on this client's window (see 'client.c'), giving the
     * resize-cursor logic a second, independent chance to catch what
     * motion alone might have missed. */
    entered = mouse_resize_cursor_update(connection, stages,
            event->event,
            (struct position_s) { event->root_x, event->root_y });

    /* An undecorated client has no separate frame window to fall back
     * on at all: moving from its border to its interior (or back)
     * happens entirely within this one same window, with no crossing
     * whatsoever for any further 'EnterNotify' to catch, and its
     * 'PointerMotion' may be just as intercepted as any other client's;
     * only a periodic poll (see 'mouse_hover_poll_tick') can still
     * catch that transition, so track it for one here. */
    mouse_hover_track((entered != NULL && entered->frame == 0)
            ? event->event : XCB_WINDOW_NONE);

    if (event->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
        return;
    }

    if (!focus_is_sloppy(config)) {
        return;
    }

    /* While the cycle menu is open, navigating it restacks each newly
     * selected client just under the popup (see
     * 'mi_cycle_preview_apply', 'menu/cycle/draw.c'), which can raise
     * that client out from under a pointer that never itself moved,
     * generating a perfectly genuine 'EnterNotify' for it.  Without
     * this, sloppy focus took that as license to hand it real input
     * focus mid-cycle, stealing it away from the cycle menu window
     * (see 'cycle_init', 'menu/cycle.c', which focuses the menu
     * itself so the modifier's eventual release reaches it): the
     * automatic keyboard grab a cycle-next/prev binding leaves
     * active (see 'keyboard_handle_release', 'input/kbd/event.c')
     * only lasts as long as that trigger key itself stays down, not
     * the modifier, so by the time the user actually releases the
     * modifier, nothing is left to route that release back to this
     * window manager at all, and the menu stays on screen with
     * nothing left to close it. */
    if (cycle_is_open()) {
        return;
    }

    client = lookup_find_client(stages, event->event, NULL,
            &desktop);
    if (client == NULL) {
        return;
    }

    stage = lookup_stage_for_root(stages, event->root);
    if (stage == NULL || desktop == NULL) {
        return;
    }

    if (config->base.windows.focus.delay_ms == 0u ||
            clock_gettime(CLOCK_MONOTONIC, &s_pending_due) != 0) {
        s_pending_window = XCB_WINDOW_NONE;
        s_enter_focus_active = true;
        focus_apply(stages, stage, desktop, client, false, config);
        return;
    }

    clock_add_ms(&s_pending_due, config->base.windows.focus.delay_ms);
    s_pending_window = event->event;
}


/* Query whether a hover-triggered focus transfer is in progress */
bool mouse_enter_focus_is_active(void)
{
    return s_enter_focus_active;
}


/* Clear the hover-triggered focus flag */
void mouse_enter_focus_clear(void)
{
    s_enter_focus_active = false;
}


/* Cancel a pending delayed sloppy-focus if it targets 'window' */
void mouse_enter_focus_cancel(xcb_window_t window)
{
    if (window != XCB_WINDOW_NONE && window == s_pending_window) {
        s_pending_window = XCB_WINDOW_NONE;
    }
}


/* Milliseconds until the pending delayed sloppy-focus becomes due */
int mouse_enter_focus_ms_remaining(void)
{
    if (s_pending_window == XCB_WINDOW_NONE) {
        return -1;
    }

    return (int) clock_ms_until(&s_pending_due);
}


/* Apply the pending delayed sloppy-focus, if one is due */
void mouse_enter_focus_tick(list_td *stages, const config_td *config)
{
    client_td *client;
    desktop_td *desktop;
    stage_td *stage;
    xcb_window_t window;

    if (stages == NULL || config == NULL ||
            s_pending_window == XCB_WINDOW_NONE ||
            mouse_enter_focus_ms_remaining() > 0) {
        return;
    }

    window = s_pending_window;
    s_pending_window = XCB_WINDOW_NONE;

    if (!focus_is_sloppy(config)) {
        return;
    }

    /* Same reasoning as 'mouse_handle_enter''s own 'cycle_is_open'
     * guard above: a delayed sloppy-focus becoming due while the
     * cycle menu happens to still be open is exactly as capable of
     * stealing its real input focus and leaving the menu stuck. */
    if (cycle_is_open()) {
        return;
    }

    client = lookup_find_client(stages, window, &stage, &desktop);
    if (client == NULL || stage == NULL || desktop == NULL) {
        return;
    }

    s_enter_focus_active = true;
    focus_apply(stages, stage, desktop, client, false, config);
}
