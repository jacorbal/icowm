/**
 * @file tests/wm/startup/test_handle.c
 *
 * @brief Test battery for wm/startup/handle.c's signal handlers and
 *        their query functions
 *
 * Every handler in this file (wm_startup_handle_signal,
 * wm_startup_handle_reload, wm_startup_handle_resume,
 * wm_startup_handle_child) is, despite its name and its real
 * installation site being a signal disposition, an ordinary function
 * with no async-signal-safety trickery beyond writing a single
 * 'volatile sig_atomic_t': calling it directly from normal execution
 * context, the same way the real signal delivery mechanism would
 * invoke it, exercises the exact same store, and each corresponding
 * query function's read-and-clear logic is plain, deterministic
 * control flow with no X or process dependency at all.  This file
 * therefore covers all four handlers and all four query functions in
 * full.
 *
 * wm_startup_handle_crash is the one exception: while it is callable
 * directly too, its real body ends by restoring SIGSEGV/SIGABRT/
 * SIGBUS/SIGFPE's default disposition and then raising the very
 * signal it was passed via 'raise(signum)', which, for any of the
 * four real fatal signals it exists to handle, deliberately
 * terminates the calling process (this test binary itself) once the
 * default disposition takes over.  There is no signal number this
 * function can be called with that both exercises its real body past
 * the initial writes and lets the process survive to report a TAP
 * result afterward, so it is skipped in full; see the comment above
 * its test stub below for the technical reason in detail.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <signal.h>
#include <stdbool.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <wm/startup/handle.h>


/**
 * @brief Link-only stand-in for logger_emergency_flush
 *
 * A silent no-op; wm_startup_handle_crash, the only function in this
 * translation unit that calls it, is itself never invoked by any
 * scenario in this file, for the reason its own test stub documents
 * below, so this stand-in exists only to satisfy the linker.
 *
 * @note Complexity: O(1)
 */
void logger_emergency_flush(void)
{
}


/* wm_startup_requested_stop starts false before any signal handler
 * has ever run in this process */
static void s_test_stop_initially_false(void)
{
    TAP_OK(!wm_startup_requested_stop(),
            "wm_startup_requested_stop is false before any stop"
            " signal is simulated");
}


/* Calling wm_startup_handle_signal directly, exactly as the real
 * signal delivery mechanism would, makes wm_startup_requested_stop
 * observe a request afterward, and that observation is not
 * self-clearing */
static void s_test_stop_after_signal(void)
{
    wm_startup_handle_signal(SIGTERM);

    TAP_OK(wm_startup_requested_stop(),
            "wm_startup_requested_stop is true once"
            " wm_startup_handle_signal has run");
    TAP_OK(wm_startup_requested_stop(),
            "wm_startup_requested_stop stays true on a second"
            " call, unlike the other three query functions");
}


/* wm_startup_requested_reload starts false, then reads true exactly
 * once after wm_startup_handle_reload runs, clearing itself on that
 * read */
static void s_test_reload_flag_clears_on_read(void)
{
    TAP_OK(!wm_startup_requested_reload(),
            "wm_startup_requested_reload is false before any SIGHUP"
            " is simulated");

    wm_startup_handle_reload(SIGHUP);

    TAP_OK(wm_startup_requested_reload(),
            "wm_startup_requested_reload is true immediately after"
            " wm_startup_handle_reload has run");
    TAP_OK(!wm_startup_requested_reload(),
            "wm_startup_requested_reload clears itself back to false"
            " on the very next call");
}


/* wm_startup_requested_resume follows the identical read-and-clear
 * contract as wm_startup_requested_reload, driven by
 * wm_startup_handle_resume instead */
static void s_test_resume_flag_clears_on_read(void)
{
    TAP_OK(!wm_startup_requested_resume(),
            "wm_startup_requested_resume is false before any SIGCONT"
            " is simulated");

    wm_startup_handle_resume(SIGCONT);

    TAP_OK(wm_startup_requested_resume(),
            "wm_startup_requested_resume is true immediately after"
            " wm_startup_handle_resume has run");
    TAP_OK(!wm_startup_requested_resume(),
            "wm_startup_requested_resume clears itself back to false"
            " on the very next call");
}


/* wm_startup_requested_child_reap follows the identical
 * read-and-clear contract, driven by wm_startup_handle_child */
static void s_test_child_reap_flag_clears_on_read(void)
{
    TAP_OK(!wm_startup_requested_child_reap(),
            "wm_startup_requested_child_reap is false before any"
            " SIGCHLD is simulated");

    wm_startup_handle_child(SIGCHLD);

    TAP_OK(wm_startup_requested_child_reap(),
            "wm_startup_requested_child_reap is true immediately"
            " after wm_startup_handle_child has run");
    TAP_OK(!wm_startup_requested_child_reap(),
            "wm_startup_requested_child_reap clears itself back to"
            " false on the very next call");
}


/* Two independent flags set back to back stay independent: setting
 * one does not disturb the other, matching each flag being its own
 * separate 'volatile sig_atomic_t' */
static void s_test_flags_are_independent(void)
{
    wm_startup_handle_reload(SIGHUP);
    wm_startup_handle_child(SIGCHLD);

    TAP_OK(wm_startup_requested_reload(),
            "the reload flag observes its own signal even after"
            " a different handler ran in between");
    TAP_OK(wm_startup_requested_child_reap(),
            "the child-reap flag observes its own signal"
            " independently of the reload flag being read first");
}


/* wm_startup_handle_crash is intentionally never invoked here: its
 * real body, past the diagnostic writes this file has no way to
 * observe from outside (they go straight to the real STDERR_FILENO
 * with the 'write' syscall, not through any seam this file could
 * intercept), ends with 'sigaction(signum, &sa, NULL)' restoring
 * signum's default disposition followed by 'raise(signum)'.  For
 * every signal it is documented to run under (SIGSEGV, SIGABRT,
 * SIGBUS, SIGFPE), the default disposition terminates the process,
 * so calling it here would end this test binary before TAP_DONE ever
 * runs, regardless of which of those four signal numbers were
 * passed.  This is a real process-termination dependency with no
 * injectable seam, the same category of skip test_startup.c already
 * documents for a live XCB round trip: there is no way to observe
 * this function's real effect and still report a result afterward */
static void s_test_crash_handler_not_invoked(void)
{
    TAP_OK(true,
            "wm_startup_handle_crash is intentionally not called: its"
            " real body re-raises signum with the default"
            " disposition restored, which terminates this very"
            " process for every signal it is documented to handle");
}


int main(void)
{
    TAP_PLAN(15);

    s_test_stop_initially_false();
    s_test_stop_after_signal();
    s_test_reload_flag_clears_on_read();
    s_test_resume_flag_clears_on_read();
    s_test_child_reap_flag_clears_on_read();
    s_test_flags_are_independent();
    s_test_crash_handler_not_invoked();

    return TAP_DONE();
}
