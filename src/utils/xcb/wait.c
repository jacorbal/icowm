/**
 * @file utils/xcb/wait.c
 *
 * @brief Bounded wait for an XCB connection's own file descriptor to
 *        become readable
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
#include <poll.h>       /* struct pollfd, poll, POLLIN */

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <utils/xcb/wait.h>


/* Wait, with a bounded timeout, for a connection to have something
 * to read */
bool xcb_wait_readable(xcb_connection_t *connection, int timeout_ms)
{
    struct pollfd pfd;
    int ret;

    if (connection == NULL) {
        return false;
    }

    pfd.fd = xcb_get_file_descriptor(connection);
    pfd.events = POLLIN;
    pfd.revents = 0;

    ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return false;
    }

    return (pfd.revents & POLLIN) != 0;
}
