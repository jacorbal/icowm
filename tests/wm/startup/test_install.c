/**
 * @file tests/wm/startup/test_install.c
 *
 * @brief Test battery for wm/startup/install.c's signal-handler
 *        registration
 *
 * Both public functions here, wm_startup_install_signals and
 * wm_startup_install_crash_handlers, are pure sequences of real
 * 'sigaction' calls through the static 's_startup_install_handler'
 * helper: no NULL-input guard clause, no branch on any argument at
 * all (neither function takes one), and no X or process dependency
 * whatsoever.  'sigaction' itself is safe to call from an ordinary
 * test process for every signal these two functions register
 * (SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGCONT, SIGCHLD, SIGSEGV,
 * SIGABRT, SIGBUS, SIGFPE): installing a handler does not deliver the
 * signal, so none of these handlers ever actually runs as a side
 * effect of this file, only their registration is exercised.  This
 * file therefore covers both functions in full: their success return
 * value, and, by reading the disposition back with a second
 * 'sigaction' call, that the exact handler function pointer each one
 * is documented to install really did get installed for every signal
 * it claims to cover.
 *
 * This file's only real dependency, wm/startup/install.c, has no
 * seam to substitute even if there had been an external call worth
 * stubbing: every function it references (sigaction, sigemptyset,
 * memset, the four wm_startup_handle_* functions it wires up, and
 * LOGGER_ERROR on the failure path, never reached here since real
 * 'sigaction' calls for these ten signals do not fail in this
 * environment) is either a real libc call safe to exercise directly,
 * or wm/startup/handle.c's own real handlers, whose own bodies
 * already have dedicated coverage in test_handle.c and are never
 * invoked here regardless, since installing a disposition does not
 * deliver the signal.
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
#include <stdarg.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <wm/startup/handle.h>
#include <wm/startup/install.h>


/**
 * @brief Link-only stand-in for logger_msg
 *
 * A silent no-op; the real failure path that would call this through
 * LOGGER_ERROR is never reached, since real 'sigaction' calls for the
 * ten signals this file's two functions install never fail in this
 * environment, matching the same reasoning tests/wm/test_startup.c
 * already documents for this exact stub.
 *
 * @note Complexity: O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/**
 * @brief Link-only stand-in for logger_emergency_flush
 *
 * A silent no-op; wm_startup_handle_crash, the only function in
 * wm/startup/handle.c that calls it, is only registered as
 * a disposition here, never actually delivered or invoked, so this
 * stand-in exists only to satisfy the linker for handle.c, which
 * install.c's own functions reference by address.
 *
 * @note Complexity: O(1)
 */
void logger_emergency_flush(void)
{
}


/* Confirms 'signum's currently installed handler is exactly
 * 'expected', by reading its disposition back with 'sigaction' */
static bool s_handler_is(int signum, void (*expected)(int))
{
    struct sigaction sa;

    if (sigaction(signum, NULL, &sa) != 0) {
        return false;
    }

    return sa.sa_handler == expected;
}


/* wm_startup_install_signals reports success, and every one of the
 * six signals it documents installs exactly the handler
 * wm/startup/handle.h assigns it */
static void s_test_install_signals_wires_up_handlers(void)
{
    int result = wm_startup_install_signals();

    TAP_EQ_INT(result, 0,
            "wm_startup_install_signals returns 0 on success");
    TAP_OK(s_handler_is(SIGHUP, wm_startup_handle_reload),
            "SIGHUP is wired to wm_startup_handle_reload");
    TAP_OK(s_handler_is(SIGINT, wm_startup_handle_signal),
            "SIGINT is wired to wm_startup_handle_signal");
    TAP_OK(s_handler_is(SIGQUIT, wm_startup_handle_signal),
            "SIGQUIT is wired to wm_startup_handle_signal");
    TAP_OK(s_handler_is(SIGTERM, wm_startup_handle_signal),
            "SIGTERM is wired to wm_startup_handle_signal");
    TAP_OK(s_handler_is(SIGCONT, wm_startup_handle_resume),
            "SIGCONT is wired to wm_startup_handle_resume");
    TAP_OK(s_handler_is(SIGCHLD, wm_startup_handle_child),
            "SIGCHLD is wired to wm_startup_handle_child");
}


/* SIGCHLD is documented to be installed with SA_NOCLDSTOP set, unlike
 * the other five signals wm_startup_install_signals wires up */
static void s_test_install_signals_sigchld_flags(void)
{
    struct sigaction sa;
    int query_result = sigaction(SIGCHLD, NULL, &sa);

    TAP_EQ_INT(query_result, 0,
            "querying SIGCHLD's installed disposition succeeds");
    TAP_OK((sa.sa_flags & SA_NOCLDSTOP) != 0,
            "SIGCHLD is installed with SA_NOCLDSTOP set");
}


/* wm_startup_install_crash_handlers reports success, and every one
 * of the four fatal signals it documents installs
 * wm_startup_handle_crash */
static void s_test_install_crash_handlers_wires_up_handler(void)
{
    int result = wm_startup_install_crash_handlers();

    TAP_EQ_INT(result, 0,
            "wm_startup_install_crash_handlers returns 0 on success");
    TAP_OK(s_handler_is(SIGSEGV, wm_startup_handle_crash),
            "SIGSEGV is wired to wm_startup_handle_crash");
    TAP_OK(s_handler_is(SIGABRT, wm_startup_handle_crash),
            "SIGABRT is wired to wm_startup_handle_crash");
    TAP_OK(s_handler_is(SIGBUS, wm_startup_handle_crash),
            "SIGBUS is wired to wm_startup_handle_crash");
    TAP_OK(s_handler_is(SIGFPE, wm_startup_handle_crash),
            "SIGFPE is wired to wm_startup_handle_crash");
}


int main(void)
{
    TAP_PLAN(14);

    s_test_install_signals_wires_up_handlers();
    s_test_install_signals_sigchld_flags();
    s_test_install_crash_handlers_wires_up_handler();

    return TAP_DONE();
}
