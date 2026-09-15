/**
 * @file wm/startup/handle.c
 *
 * @brief Signal handlers and the flags they set, queried back by the
 *        main loop
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* sigaction, sigemptyset */


/* System includes */
#include <signal.h>     /* sigaction, sigemptyset */
#include <stdbool.h>
#include <string.h>     /* memset */
#include <unistd.h>     /* write */

/* Project includes */
#include <logger.h>

/* Local includes */
#include <wm/startup/handle.h>


/**
 * @brief Flag written by the signal handler to request a graceful
 *        shutdown
 */
static volatile sig_atomic_t s_stop_signal_received = 0;


/**
 * @brief Flag written by @c SIGCONT (VT resume) to re-establish input
 *        grabs
 */
static volatile sig_atomic_t s_resume_signal_received = 0;


/**
 * @brief Flag written by the @c SIGHUP handler to request
 *        a configuration reload
 */
static volatile sig_atomic_t s_reload_signal_received = 0;


/**
 * @brief Flag written by @c SIGCHLD so the main loop can reap children
 */
static volatile sig_atomic_t s_child_reap_requested = 0;


/* Signal handler for termination signals */
void wm_startup_handle_signal(int signum)
{
    s_stop_signal_received = signum;
}


/* Signal handler for 'SIGHUP' (configuration reload) */
void wm_startup_handle_reload(int signum)
{
    (void) signum;
    s_reload_signal_received = 1;
}


/* Signal handler for'SIGCONT' (VT resume) */
void wm_startup_handle_resume(int signum)
{
    (void) signum;
    s_resume_signal_received = 1;
}


/* Signal handler for 'SIGCHLD' */
void wm_startup_handle_child(int signum)
{
    (void) signum;
    s_child_reap_requested = 1;
}


/* Async-signal-safe handler for fatal signals */
void wm_startup_handle_crash(int signum)
{
    static const char s_prefix[] = "icowm: fatal signal ";
    static const char s_suffix[] = "; terminating (see above for" \
        " which signal number; a core dump, if enabled, has the" \
        " rest)\n";
    char rev[4];
    char digits[4];
    int len = 0;
    int n = signum;
    struct sigaction sa;
    ssize_t write_result;

    /* Every 'write' result below is deliberately unchecked: this
     * handler is already on its way to re-raising 'signum' with its
     * default disposition right after, terminating the process either
     * way, so there is no meaningful recovery available if any one of
     * them fails too.  Each captured in a real variable rather than
     * cast to 'void' directly on the call, since GCC's own
     * 'warn_unused_result' on 'write' does not treat a bare '(void)'
     * cast as acknowledging it. */
    write_result = write(STDERR_FILENO, s_prefix, sizeof(s_prefix) - 1u);
    (void) write_result;

    if (n <= 0) {
        digits[len++] = '0';
    } else {
        int rlen = 0;

        while (n > 0 && rlen < (int) sizeof(rev)) {
            rev[rlen++] = (char) ('0' + (n % 10));
            n /= 10;
        }
        while (rlen > 0) {
            digits[len++] = rev[--rlen];
        }
    }
    write_result = write(STDERR_FILENO, digits, (size_t) len);
    (void) write_result;
    write_result = write(STDERR_FILENO, s_suffix, sizeof(s_suffix) - 1u);
    (void) write_result;

    /* Whatever the logger still holds goes out here, before the signal
     * is re-raised.  Its ordinary flush runs through 'fprintf' and
     * 'fflush', neither async-signal-safe, so without this the messages
     * leading up to a crash die with the process: the ones most worth
     * having.  'logger_emergency_flush' writes them with 'write' and
     * takes no lock; see its note on why that is sound here and nowhere
     * else. */
    logger_emergency_flush();

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    (void) sigaction(signum, &sa, NULL);

    (void) raise(signum);
}


/* Query whether a termination signal has been received */
bool wm_startup_requested_stop(void)
{
    return s_stop_signal_received != 0;
}


/* Query whether a 'SIGHUP' configuration-reload request was received */
bool wm_startup_requested_reload(void)
{
    if (s_reload_signal_received != 0) {
        s_reload_signal_received = 0;
        return true;
    }

    return false;
}


/* Query whether a 'SIGCONT' (VT resume) was received */
bool wm_startup_requested_resume(void)
{
    if (s_resume_signal_received != 0) {
        s_resume_signal_received = 0;
        return true;
    }

    return false;
}


/* Query whether a pending child-reap request was received */
bool wm_startup_requested_child_reap(void)
{
    if (s_child_reap_requested != 0) {
        s_child_reap_requested = 0;
        return true;
    }

    return false;
}
