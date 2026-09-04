/**
 * @file tests/utils/xcb/test_wait.c
 *
 * @brief Test battery for the bounded wait on an XCB connection's file
 *        descriptor
 *
 * Exercises 'xcb_wait_readable' (utils/xcb/wait.c) linked for real,
 * including its actual call to the real, unstubbed 'poll' (a genuine
 * kernel wait, not a stand-in): 'xcb_get_file_descriptor' is the only
 * XCB entry point it calls, stubbed below to hand back one end of
 * a real pipe this file creates itself, so the wait it drives is
 * exercised against a real, controllable file descriptor rather than
 * a fake one poll would just reject.
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
#include <unistd.h>     /* close, pipe, write */

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <harness/tap.h>
#include <utils/xcb/wait.h>


/* Controllable stand-in state */

static int s_fd_to_hand_back = -1;
static int s_get_fd_calls = 0;


/* Raw XCB stand-in (not linking libxcb at all) */

int xcb_get_file_descriptor(xcb_connection_t *c)
{
    (void) c;
    s_get_fd_calls++;
    return s_fd_to_hand_back;
}


/* A NULL connection is refused outright, without even asking for a
 * file descriptor */
static void s_test_null_connection_is_refused(void)
{
    bool ready;

    s_fd_to_hand_back = -1;
    s_get_fd_calls = 0;

    ready = xcb_wait_readable(NULL, 50);

    TAP_OK(!ready, "a NULL connection is never reported readable");
    TAP_EQ_INT(s_get_fd_calls, 0,
            "the file descriptor is never even requested for a NULL"
            " connection");
}


/* A pipe with data already sitting in it is reported readable well
 * within a generous timeout */
static void s_test_readable_fd_reports_true(void)
{
    int fds[2];
    bool ready;
    char byte = 'x';

    TAP_OK(pipe(fds) == 0, "a real pipe is created for this fixture");

    s_fd_to_hand_back = fds[0];
    TAP_OK(write(fds[1], &byte, 1) == 1,
            "one byte is written to the pipe's write end");

    ready = xcb_wait_readable((xcb_connection_t *) 1, 1000);

    TAP_OK(ready, "a pipe with data waiting is reported readable");

    close(fds[0]);
    close(fds[1]);
}


/* A pipe with nothing written to it, and no writer left to ever write
 * anything, times out and reports not readable */
static void s_test_empty_fd_times_out(void)
{
    int fds[2];
    bool ready;

    TAP_OK(pipe(fds) == 0, "a second real pipe is created");

    s_fd_to_hand_back = fds[0];
    /* Close the write end immediately: 'poll' then reports 'POLLHUP',
     * not 'POLLIN', so this also confirms the return value tracks
     * 'POLLIN' specifically rather than any nonzero 'revents' */
    close(fds[1]);

    ready = xcb_wait_readable((xcb_connection_t *) 1, 50);

    TAP_OK(!ready,
            "a pipe with its write end closed and nothing written is"
            " not reported readable");

    close(fds[0]);
}


int main(void)
{
    TAP_PLAN(7);

    s_test_null_connection_is_refused();
    s_test_readable_fd_reports_true();
    s_test_empty_fd_times_out();

    return TAP_DONE();
}
