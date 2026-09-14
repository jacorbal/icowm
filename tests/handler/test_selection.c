/**
 * @file tests/handler/test_selection.c
 *
 * @brief Test battery for handler_selection_clear (handler/selection.c)
 *
 * 'handler_selection_clear' only ever reads two things through 'wm':
 * whatever 'wm_ewmh_support_win' returns for it, and the 'wm' pointer
 * itself, forwarded unchanged to 'wm_shutdown_begin' if reached; both
 * are link-only stand-ins here, so 'wm' itself never needs to be
 * a real, fully-built window manager state, only some non-null value
 * consistent between what a scenario configures and what it asserts
 * was forwarded.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <handler.h>
#include <harness/tap.h>
#include <logger.h>
#include <wm.h>
#include <wm/shutdown.h>


/** Support window 'wm_ewmh_support_win' hands back for the 'wm'
 *  a scenario passes it */
static xcb_window_t s_stub_support_win;

/** Calls and last argument recorded for 'wm_shutdown_begin' */
static unsigned int s_call_shutdown_begin;
static const wm_td *s_last_shutdown_begin_wm;


static void s_reset(void)
{
    s_stub_support_win = (xcb_window_t) 0x100u;
    s_call_shutdown_begin = 0u;
    s_last_shutdown_begin_wm = NULL;
}


/** Link-only stand-in for logger_msg (logger.c): this file asserts on
 *  nothing it would print */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;

    return 0;
}


/** Link-only stand-in for wm_ewmh_support_win (wm.c) */
xcb_window_t wm_ewmh_support_win(const wm_td *wm)
{
    (void) wm;

    return s_stub_support_win;
}


/** Link-only stand-in for wm_shutdown_begin (wm/shutdown.c) */
void wm_shutdown_begin(const wm_td *wm)
{
    s_call_shutdown_begin++;
    s_last_shutdown_begin_wm = wm;
}


/* A null 'wm' or 'event' is a safe no-op */
static void s_test_null_guards(void)
{
    xcb_selection_clear_event_t event;
    wm_td *const wm = (wm_td *) 1;

    s_reset();
    event.owner = s_stub_support_win;

    handler_selection_clear(NULL, &event);
    handler_selection_clear(wm, NULL);

    TAP_OK(s_call_shutdown_begin == 0u,
            "a null wm or event never starts a shutdown");
}


/* An event naming this window manager's own support window starts
 * a coordinated shutdown, forwarding the same 'wm' it was given */
static void s_test_own_support_win_starts_shutdown(void)
{
    xcb_selection_clear_event_t event;
    wm_td *const wm = (wm_td *) 0x42;

    s_reset();
    event.owner = s_stub_support_win;

    handler_selection_clear(wm, &event);

    TAP_OK(s_call_shutdown_begin == 1u,
            "losing this instance's own manager selection starts"
            " a shutdown exactly once");
    TAP_OK(s_last_shutdown_begin_wm == wm,
            "the same wm the handler itself was given is the one"
            " forwarded to wm_shutdown_begin");
}


/* An event naming some other window is left alone: it belongs to
 * a selection this window manager never owned in the first place */
static void s_test_other_window_is_noop(void)
{
    xcb_selection_clear_event_t event;
    wm_td *const wm = (wm_td *) 1;

    s_reset();
    event.owner = (xcb_window_t) (s_stub_support_win + 1u);

    handler_selection_clear(wm, &event);

    TAP_OK(s_call_shutdown_begin == 0u,
            "an event for a window other than this instance's own"
            " support window never starts a shutdown");
}


int main(void)
{
    TAP_PLAN(4);

    s_test_null_guards();
    s_test_own_support_win_starts_shutdown();
    s_test_other_window_is_noop();

    return TAP_DONE();
}
