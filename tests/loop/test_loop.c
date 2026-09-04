/**
 * @file tests/loop/test_loop.c
 *
 * @brief Test battery for the main loop's three early-return guard
 *        clauses (loop.c)
 *
 * loop_run's own body is almost entirely the real, unbounded
 * @c while(wm_is_running(wm)) event loop: real poll(2) waits, real
 * xcb_poll_for_event draining, and real signal/timer/dispatch/repaint
 * work every iteration, none of which can be driven to completion,
 * or even entered once, without a live X connection and a live wm
 * that eventually stops running of its own accord.  That whole body,
 * starting at "LOGGER_DEBUG("Entering main event loop"...)" through
 * the loop's own closing brace, is out of scope here for exactly the
 * same reason wm_start's own connect-then-run body is out of scope in
 * tests/wm/test_startup.c and tests/wm/test_lifecycle.c: standing in
 * for xcb_poll_for_event's own queue, xcb_flush, and every per-
 * iteration subsystem call just to prove loop_run can be made to loop
 * some fixed number of times would not exercise loop.c's own logic,
 * only the stand-ins' own scripted behavior.
 *
 * What is genuinely loop.c's own, testable in isolation, is the three
 * early-return guard clauses that run before the loop is ever
 * entered: (a) a null wm pointer or a wm reporting itself not
 * running, (b) loop_context_init reporting failure, and (c)
 * xcb_key_symbols_alloc returning NULL.  Each is reachable by
 * returning through the guard before any of the loop's own real body
 * runs at all, verified here by the fact that no stand-in past the
 * guard under test is ever called.
 *
 * wm_is_running and wm_set_keysyms are wm.c's own real accessors
 * (src/wm/instance.c), linked for real against a real 'struct wm_s'
 * built on the stack via 'wm/internal.h', the same pattern tests/loop/
 * test_context.c already uses for the very same struct.
 * loop_context_init itself is stood in below, rather than linking the
 * real src/loop/context.c, specifically so scenario (b) can force a
 * failure return that, in the real function, is otherwise
 * unreachable from loop_run's own call site (loop_run's own call
 * always passes a non-null wm, the only input loop_context_init's
 * real body ever fails on, having already excluded a null wm one
 * guard clause earlier); loop_context_init's own real logic is
 * already covered on its own terms in tests/loop/test_context.c.
 * Every other external symbol loop_run references past guard (b), and
 * xcb_key_symbols_alloc/xcb_key_symbols_free themselves, are link-
 * only, call-recording stand-ins below, none of which any scenario
 * here ever expects to be reached.
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
#include <xcb/xcb_keysyms.h>

/* Project includes */
#include <wm.h>
#include <wm/internal.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <loop.h>
#include <loop/context.h>


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


/* Controllable result for the loop_context_init stand-in */
static bool s_loop_context_init_return_value;
static int s_loop_context_init_calls;

/* Controllable result for the xcb_key_symbols_alloc stand-in */
static xcb_key_symbols_t *s_keysyms_alloc_return_value;
static int s_keysyms_alloc_calls;
static int s_keysyms_free_calls;

/* Recording for the past-guard-(c) stand-ins: none of these should
 * ever be reached by any scenario in this file */
static int s_keyboard_load_calls;
static int s_mouse_load_calls;
static int s_cctl_adopt_scan_calls;
static int s_wm_ewmh_sync_calls;
static int s_wm_startup_install_signals_calls;
static int s_wm_startup_install_crash_handlers_calls;
static int s_loop_signals_process_calls;
static int s_loop_pollset_wait_calls;
static int s_loop_timers_timeout_calls;
static int s_loop_timers_tick_calls;
static int s_loop_dispatch_event_calls;
static int s_xcb_poll_for_event_calls;
static int s_xcb_flush_calls;
static int s_wm_set_keysyms_calls;


static void s_reset(void)
{
    s_loop_context_init_return_value = true;
    s_loop_context_init_calls = 0;
    s_keysyms_alloc_return_value = (xcb_key_symbols_t *) (void *) 0x1;
    s_keysyms_alloc_calls = 0;
    s_keysyms_free_calls = 0;
    s_keyboard_load_calls = 0;
    s_mouse_load_calls = 0;
    s_cctl_adopt_scan_calls = 0;
    s_wm_ewmh_sync_calls = 0;
    s_wm_startup_install_signals_calls = 0;
    s_wm_startup_install_crash_handlers_calls = 0;
    s_loop_signals_process_calls = 0;
    s_loop_pollset_wait_calls = 0;
    s_loop_timers_timeout_calls = 0;
    s_loop_timers_tick_calls = 0;
    s_loop_dispatch_event_calls = 0;
    s_xcb_poll_for_event_calls = 0;
    s_xcb_flush_calls = 0;
    s_wm_set_keysyms_calls = 0;
}


/** Link-only stand-in for loop_context_init (loop/context.c): its own
 *  real logic is already covered on its own terms in tests/loop/
 *  test_context.c, not here; standing it in here lets scenario (b)
 *  force a failure return that is otherwise unreachable from loop_
 *  run's own real call site */
bool loop_context_init(loop_ctx_td *ctx, wm_td *wm)
{
    (void) ctx;
    (void) wm;
    s_loop_context_init_calls++;
    return s_loop_context_init_return_value;
}


/** Link-only stand-in for xcb_key_symbols_alloc (libxcb-keysyms):
 *  a real allocation dials the X server itself for its keycode range,
 *  nothing this file could fabricate without a live connection */
xcb_key_symbols_t *xcb_key_symbols_alloc(xcb_connection_t *c)
{
    (void) c;
    s_keysyms_alloc_calls++;
    return s_keysyms_alloc_return_value;
}


/** Link-only stand-in for xcb_key_symbols_free (libxcb-keysyms) */
void xcb_key_symbols_free(xcb_key_symbols_t *keysyms)
{
    (void) keysyms;
    s_keysyms_free_calls++;
}


/** Link-only stand-in for xcb_connection_get
 *  (utils/xcb/connection.c) */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/** Link-only stand-in for wm_startup_install_signals
 *  (wm/startup/install.c) */
int wm_startup_install_signals(void)
{
    s_wm_startup_install_signals_calls++;
    return 0;
}


/** Link-only stand-in for wm_startup_install_crash_handlers
 *  (wm/startup/install.c) */
int wm_startup_install_crash_handlers(void)
{
    s_wm_startup_install_crash_handlers_calls++;
    return 0;
}


/** Link-only stand-in for keyboard_load (input/kbd/bind.c) */
void keyboard_load(list_td *surfaces, xcb_key_symbols_t *keysyms,
        const config_td *config)
{
    (void) surfaces;
    (void) keysyms;
    (void) config;
    s_keyboard_load_calls++;
}


/** Link-only stand-in for mouse_load (input/mouse/bind.c) */
void mouse_load(list_td *surfaces, const config_td *config)
{
    (void) surfaces;
    (void) config;
    s_mouse_load_calls++;
}


/** Link-only stand-in for cctl_adopt_scan (cctl/adopt.c) */
void cctl_adopt_scan(const wm_td *wm)
{
    (void) wm;
    s_cctl_adopt_scan_calls++;
}


/** Link-only stand-in for loop_refresh_full (loop/refresh.c): its own
 *  real logic is already covered on its own terms in tests/loop/
 *  test_refresh.c, not here */
void loop_refresh_full(loop_ctx_td *ctx)
{
    (void) ctx;
}


/** Link-only stand-in for loop_refresh (loop/refresh.c): covered on
 *  its own terms in tests/loop/test_refresh.c, not here */
void loop_refresh(loop_ctx_td *ctx)
{
    (void) ctx;
}


/** Link-only stand-in for wm_ewmh_sync (wm/ewmh.c) */
void wm_ewmh_sync(wm_td *wm)
{
    (void) wm;
    s_wm_ewmh_sync_calls++;
}


/** Link-only stand-in for loop_signals_process (loop/signals.c):
 *  covered on its own terms in tests/loop/test_signals.c, not here */
bool loop_signals_process(loop_ctx_td *ctx)
{
    (void) ctx;
    s_loop_signals_process_calls++;
    return true;
}


/** Link-only stand-in for loop_pollset_wait (loop/pollset.c): covered
 *  on its own terms in tests/loop/test_pollset.c, not here */
bool loop_pollset_wait(loop_ctx_td *ctx, int timeout_ms)
{
    (void) ctx;
    (void) timeout_ms;
    s_loop_pollset_wait_calls++;
    return true;
}


/** Link-only stand-in for loop_timers_timeout (loop/timers.c):
 *  covered on its own terms in tests/loop/test_timers.c, not here */
int loop_timers_timeout(const loop_ctx_td *ctx)
{
    (void) ctx;
    s_loop_timers_timeout_calls++;
    return 0;
}


/** Link-only stand-in for loop_timers_tick (loop/timers.c): covered
 *  on its own terms in tests/loop/test_timers.c, not here */
void loop_timers_tick(loop_ctx_td *ctx)
{
    (void) ctx;
    s_loop_timers_tick_calls++;
}


/** Link-only stand-in for loop_dispatch_event (loop/dispatch.c):
 *  covered on its own terms in tests/loop/test_dispatch.c, not here
 */
void loop_dispatch_event(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    (void) ctx;
    (void) event;
    s_loop_dispatch_event_calls++;
}


/** Link-only stand-in for xcb_poll_for_event (libxcb): never expected
 *  to be reached by any scenario in this file, since every one
 *  returns before the real while loop is ever entered */
xcb_generic_event_t *xcb_poll_for_event(xcb_connection_t *c)
{
    (void) c;
    s_xcb_poll_for_event_calls++;
    return NULL;
}


/** Link-only stand-in for xcb_flush (libxcb) */
int xcb_flush(xcb_connection_t *c)
{
    (void) c;
    s_xcb_flush_calls++;
    return 1;
}


/**
 * @brief Build a real, zero-initialized 'struct wm_s' on the stack
 */
static wm_td s_make_wm(bool is_running)
{
    wm_td wm;

    memset(&wm, 0, sizeof(wm));
    wm.is_running = is_running;
    return wm;
}


/* A null wm pointer returns immediately, calling none of the guard
 * clauses past it, nor anything from the real loop body */
static void s_test_null_wm(void)
{
    s_reset();
    loop_run(NULL);

    TAP_EQ_INT(s_loop_context_init_calls, 0,
            "loop_run on a null wm pointer never reaches"
            " loop_context_init");
    TAP_EQ_INT(s_keysyms_alloc_calls, 0,
            "loop_run on a null wm pointer never reaches"
            " xcb_key_symbols_alloc");
    TAP_EQ_INT(s_wm_startup_install_signals_calls, 0,
            "loop_run on a null wm pointer never installs signal"
            " handlers");
}


/* A wm reporting itself not running returns immediately too, exactly
 * like a null wm pointer does */
static void s_test_wm_not_running(void)
{
    wm_td wm = s_make_wm(false);

    s_reset();
    loop_run(&wm);

    TAP_EQ_INT(s_loop_context_init_calls, 0,
            "loop_run on a wm reporting itself not running never"
            " reaches loop_context_init");
    TAP_EQ_INT(s_keysyms_alloc_calls, 0,
            "loop_run on a wm reporting itself not running never"
            " reaches xcb_key_symbols_alloc");
}


/* A running wm for which loop_context_init reports failure returns
 * before installing any signal handler or allocating a key-symbols
 * table */
static void s_test_context_init_fails(void)
{
    wm_td wm = s_make_wm(true);

    s_reset();
    s_loop_context_init_return_value = false;

    loop_run(&wm);

    TAP_EQ_INT(s_loop_context_init_calls, 1,
            "loop_run on a running wm reaches loop_context_init"
            " exactly once");
    TAP_EQ_INT(s_wm_startup_install_signals_calls, 0,
            "a failing loop_context_init means signal handlers are"
            " never installed");
    TAP_EQ_INT(s_keysyms_alloc_calls, 0,
            "a failing loop_context_init means"
            " xcb_key_symbols_alloc is never reached");
}


/* A running wm whose context resolves fine, but whose key-symbols
 * table allocation fails, still installs both signal handlers first
 * (a warn-only step, not fatal), then returns before ever calling
 * keyboard_load, mouse_load, cctl_adopt_scan, wm_ewmh_sync, or
 * entering the real while loop at all */
static void s_test_keysyms_alloc_fails(void)
{
    wm_td wm = s_make_wm(true);

    s_reset();
    s_keysyms_alloc_return_value = NULL;

    loop_run(&wm);

    TAP_EQ_INT(s_loop_context_init_calls, 1,
            "loop_run reaches loop_context_init exactly once before"
            " the key-symbols allocation");
    TAP_EQ_INT(s_wm_startup_install_signals_calls, 1,
            "signal handlers are still installed ahead of the"
            " key-symbols allocation, since that step is warn-only");
    TAP_EQ_INT(s_wm_startup_install_crash_handlers_calls, 1,
            "crash handlers are still installed ahead of the"
            " key-symbols allocation too");
    TAP_EQ_INT(s_keysyms_alloc_calls, 1,
            "xcb_key_symbols_alloc is reached exactly once");
    TAP_EQ_INT(s_keyboard_load_calls, 0,
            "a failed key-symbols allocation means keyboard_load is"
            " never reached");
    TAP_EQ_INT(s_mouse_load_calls, 0,
            "a failed key-symbols allocation means mouse_load is"
            " never reached");
    TAP_EQ_INT(s_cctl_adopt_scan_calls, 0,
            "a failed key-symbols allocation means cctl_adopt_scan is"
            " never reached");
    TAP_EQ_INT(s_wm_ewmh_sync_calls, 0,
            "a failed key-symbols allocation means wm_ewmh_sync is"
            " never reached");
    TAP_EQ_INT(s_loop_signals_process_calls, 0,
            "a failed key-symbols allocation means the real while"
            " loop, and therefore loop_signals_process, is never"
            " entered at all");
    TAP_EQ_INT(s_keysyms_free_calls, 0,
            "a failed key-symbols allocation returns before the"
            " matching xcb_key_symbols_free at the very end, since"
            " there is nothing valid to free");
}


int main(void)
{
    TAP_PLAN(18);

    s_test_null_wm();
    s_test_wm_not_running();
    s_test_context_init_fails();
    s_test_keysyms_alloc_fails();

    return TAP_DONE();
}
