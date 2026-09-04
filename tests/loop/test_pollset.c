/**
 * @file tests/loop/test_pollset.c
 *
 * @brief Test battery for the main loop's descriptor set builder and
 *        waiter (loop/pollset.c)
 *
 * loop_pollset_wait's own logic, a null-ctx guard, a connection-error
 * check that requests shutdown and bails out early, assembling one
 * pollfd array from the X connection descriptor plus whatever
 * ipc_poll_fds reports, and forwarding every descriptor poll() itself
 * reports readable to ipc_handle_readable, is exercised here against a
 * genuinely real poll(2) call: xcb_connection_get, xcb_connection_has_
 * error, and xcb_get_file_descriptor are link-only stand-ins below
 * (an XCB connection is opaque outside libxcb itself, so nothing here
 * could fabricate a real one), but ipc_poll_fds is stood in to hand
 * back real pipe file descriptors this file opens itself, so the
 * poll() call inside loop_pollset_wait genuinely blocks on, and
 * genuinely reports readiness for, real kernel descriptors instead of
 * fabricated ones. handler_connection_error_string and
 * wm_request_stop are link-only stand-ins too, wm_request_stop's
 * real body being wm.c's own singleton-mutating logic, already
 * covered on its own terms in tests/wm/test_lifecycle.c, not
 * pollset.c's.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* pipe, write, read, close */


/* System includes */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>     /* pipe, write, read, close */

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <logger.h>
#include <loop/context.h>
#include <loop/pollset.h>


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


/* Controllable results for the connection-related stand-ins below */
static int s_conn_error;
static int s_conn_fd;

/* Controllable results/recording for the IPC-related stand-ins */
static int s_ipc_fds_to_report[9];
static int s_ipc_fds_to_report_count;
static int s_wm_request_stop_calls;
static int s_ipc_handle_readable_calls;
static int s_ipc_handle_readable_last_fd;
static wm_td *s_ipc_handle_readable_last_wm;


static void s_reset(void)
{
    s_conn_error = 0;
    s_conn_fd = -1;
    memset(s_ipc_fds_to_report, 0, sizeof(s_ipc_fds_to_report));
    s_ipc_fds_to_report_count = 0;
    s_wm_request_stop_calls = 0;
    s_ipc_handle_readable_calls = 0;
    s_ipc_handle_readable_last_fd = -1;
    s_ipc_handle_readable_last_wm = NULL;
}


/** Link-only stand-in for xcb_connection_get (utils/xcb/connection.c):
 *  an XCB connection is opaque outside libxcb itself, so nothing here
 *  can construct a real one; every scenario reaches it only through
 *  loop_pollset_wait's own forwarding, never dereferencing it itself
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/** Link-only stand-in for xcb_connection_has_error (libxcb) */
int xcb_connection_has_error(xcb_connection_t *c)
{
    (void) c;
    return s_conn_error;
}


/** Link-only stand-in for xcb_get_file_descriptor (libxcb): hands back
 *  a real, scenario-provided descriptor (typically a pipe's read end),
 *  so the real poll() call inside loop_pollset_wait has a genuine
 *  kernel descriptor to wait on for the 'X connection' slot too */
int xcb_get_file_descriptor(xcb_connection_t *c)
{
    (void) c;
    return s_conn_fd;
}


/** Link-only stand-in for handler_connection_error_string
 *  (handler.c) */
const char *handler_connection_error_string(int error_code)
{
    (void) error_code;
    return "stand-in connection error";
}


/** Link-only stand-in for wm_request_stop (wm.c): wm_request_stop's
 *  own singleton-mutating logic is covered on its own terms in
 *  tests/wm/test_lifecycle.c, not here */
int wm_request_stop(void)
{
    s_wm_request_stop_calls++;
    return 0;
}


/** Link-only stand-in for ipc_poll_fds (ipc.c): copies back however
 *  many real descriptors a scenario populated in
 *  's_ipc_fds_to_report', exactly like the real function reports
 *  however many IPC descriptors are currently open */
int ipc_poll_fds(int *out_fds, int max)
{
    int n = (s_ipc_fds_to_report_count < max)
        ? s_ipc_fds_to_report_count : max;

    for (int i = 0; i < n; ++i) {
        out_fds[i] = s_ipc_fds_to_report[i];
    }
    return n;
}


/** Link-only stand-in for ipc_handle_readable (ipc.c) */
void ipc_handle_readable(wm_td *wm, int fd)
{
    s_ipc_handle_readable_calls++;
    s_ipc_handle_readable_last_fd = fd;
    s_ipc_handle_readable_last_wm = wm;
}


/**
 * @brief Build a loop context around a given wm pointer
 */
static loop_ctx_td s_make_ctx(wm_td *wm)
{
    loop_ctx_td ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.wm = wm;
    return ctx;
}


/* A null ctx is refused outright, touching neither the connection nor
 * any IPC descriptor */
static void s_test_null_ctx(void)
{
    s_reset();
    TAP_OK(!loop_pollset_wait(NULL, 0),
            "loop_pollset_wait on a null ctx returns false");
    TAP_EQ_INT(s_wm_request_stop_calls, 0,
            "a null ctx never reaches the connection-error check at"
            " all");
}


/* A connection reported in error requests a shutdown and bails out
 * immediately, never reaching poll() or any IPC descriptor */
static void s_test_connection_error_requests_stop(void)
{
    loop_ctx_td ctx = s_make_ctx(NULL);

    s_reset();
    s_conn_error = 1;

    TAP_OK(!loop_pollset_wait(&ctx, 0),
            "loop_pollset_wait on a connection reported in error"
            " returns false");
    TAP_EQ_INT(s_wm_request_stop_calls, 1,
            "a connection error requests exactly one shutdown via"
            " wm_request_stop");
}


/* With no connection error, a readable pipe on the 'X connection' fd
 * slot is reported ready (true) well before the timeout elapses, with
 * no IPC descriptor involved at all */
static void s_test_poll_x_connection_ready(void)
{
    loop_ctx_td ctx = s_make_ctx(NULL);
    int pipe_fds[2];
    bool result;

    TAP_OK(pipe(pipe_fds) == 0, "a real pipe was opened for this"
            " scenario");

    s_reset();
    s_conn_fd = pipe_fds[0];
    TAP_OK(write(pipe_fds[1], "x", 1) == 1,
            "a byte was written so the pipe's read end is readable");

    result = loop_pollset_wait(&ctx, 5000);

    TAP_OK(result,
            "loop_pollset_wait returns true when the 'X connection'"
            " descriptor is readable");
    TAP_EQ_INT(s_ipc_handle_readable_calls, 0,
            "a ready X connection descriptor is never forwarded to"
            " ipc_handle_readable, only IPC descriptors are");

    close(pipe_fds[0]);
    close(pipe_fds[1]);
}


/* A short timeout with nothing ever becoming readable elapses and
 * still reports true: an elapsed poll with no error is not a failure
 */
static void s_test_poll_timeout_elapses_cleanly(void)
{
    loop_ctx_td ctx = s_make_ctx(NULL);
    int pipe_fds[2];
    bool result;

    TAP_OK(pipe(pipe_fds) == 0, "a second real pipe was opened for"
            " this scenario");

    s_reset();
    s_conn_fd = pipe_fds[0];

    result = loop_pollset_wait(&ctx, 20);

    TAP_OK(result,
            "loop_pollset_wait on a timeout with nothing readable"
            " still returns true");
    TAP_EQ_INT(s_ipc_handle_readable_calls, 0,
            "nothing was reported readable, so ipc_handle_readable"
            " was never reached");

    close(pipe_fds[0]);
    close(pipe_fds[1]);
}


/* Every IPC descriptor poll() reports ready is forwarded to
 * ipc_handle_readable with the ctx's own wm pointer, while an IPC
 * descriptor that never becomes readable is left alone */
static void s_test_poll_ipc_descriptors_forwarded(void)
{
    int fake_wm_marker;
    loop_ctx_td ctx = s_make_ctx((wm_td *) &fake_wm_marker);
    int x_pipe[2];
    int ipc_pipe_ready[2];
    int ipc_pipe_idle[2];
    bool result;

    TAP_OK(pipe(x_pipe) == 0, "the X connection's own stand-in pipe"
            " was opened");
    TAP_OK(pipe(ipc_pipe_ready) == 0, "a ready IPC pipe was opened");
    TAP_OK(pipe(ipc_pipe_idle) == 0, "an idle IPC pipe was opened");

    s_reset();
    s_conn_fd = x_pipe[0];
    s_ipc_fds_to_report[0] = ipc_pipe_ready[0];
    s_ipc_fds_to_report[1] = ipc_pipe_idle[0];
    s_ipc_fds_to_report_count = 2;
    TAP_OK(write(ipc_pipe_ready[1], "y", 1) == 1,
            "a byte was written so the ready IPC pipe's read end is"
            " readable");

    result = loop_pollset_wait(&ctx, 5000);

    TAP_OK(result, "loop_pollset_wait returns true with a ready IPC"
            " descriptor present");
    TAP_EQ_INT(s_ipc_handle_readable_calls, 1,
            "exactly one IPC descriptor, the one actually readable,"
            " is forwarded to ipc_handle_readable");
    TAP_EQ_INT(s_ipc_handle_readable_last_fd, ipc_pipe_ready[0],
            "the descriptor forwarded is exactly the ready pipe's"
            " read end, not the idle one");
    TAP_OK(s_ipc_handle_readable_last_wm == (wm_td *) &fake_wm_marker,
            "ipc_handle_readable receives the ctx's own wm pointer"
            " unchanged");

    close(x_pipe[0]);
    close(x_pipe[1]);
    close(ipc_pipe_ready[0]);
    close(ipc_pipe_ready[1]);
    close(ipc_pipe_idle[0]);
    close(ipc_pipe_idle[1]);
}


int main(void)
{
    TAP_PLAN(19);

    s_test_null_ctx();
    s_test_connection_error_requests_stop();
    s_test_poll_x_connection_ready();
    s_test_poll_timeout_elapses_cleanly();
    s_test_poll_ipc_descriptors_forwarded();

    return TAP_DONE();
}
