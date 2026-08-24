/**
 * @file loop/pollset.c
 *
 * @brief Descriptor set the main event loop waits on
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* poll, strerror */


/* System includes */
#include <errno.h>      /* EINTR */
#include <poll.h>       /* poll */
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <string.h>     /* strerror */

/* XCB includes */
#include <xcb/xcb.h>

/* IPC includes */
#include <ipc.h>

/* Default initial values */
#include <defs/ipc.h>

/* Project includes */
#include <handler.h>
#include <logger.h>
#include <wm.h>

/* Local includes */
#include <loop/pollset.h>


/* Wait for the X connection or any IPC descriptor to be ready */
bool loop_pollset_wait(const loop_ctx_td *ctx, int timeout_ms)
{
    struct pollfd pfd[1 + IPC_MAX_CLIENTS + 1];
    int ipc_fds[IPC_MAX_CLIENTS + 1];
    int ipc_count;
    int nfds;
    int poll_status;
    int conn_error;

    if (ctx == NULL) {
        return false;
    }

    conn_error = xcb_connection_has_error(ctx->connection);
    if (conn_error != 0) {
        LOGGER_ERROR("X connection error detected (%s);" \
                " requesting shutdown",
                handler_connection_error_string(conn_error));
        wm_request_stop();
        return false;
    }

    ipc_count = ipc_poll_fds(ipc_fds,
            (int) (sizeof(ipc_fds) / sizeof(ipc_fds[0])));

    pfd[0].fd = xcb_get_file_descriptor(ctx->connection);
    pfd[0].events = POLLIN;
    pfd[0].revents = 0;
    nfds = 1;

    for (int i = 0; i < ipc_count; ++i) {
        pfd[nfds].fd = ipc_fds[i];
        pfd[nfds].events = POLLIN;
        pfd[nfds].revents = 0;
        ++nfds;
    }

    poll_status = poll(pfd, (nfds_t) nfds, timeout_ms);
    if (poll_status < 0 && errno != EINTR) {
        LOGGER_ERROR("Failed waiting on X connection: %s",
                strerror(errno));
        return false;
    }

    /* Every non-X11 descriptor 'poll' reported ready belongs to IPC
     * (index 0 is always the X connection, drained by the caller via
     * 'xcb_poll_for_event' instead of this array at all).
     * 'ipc_handle_readable' itself tells the listening socket apart
     * from an already-connected client, so nothing here needs to. */
    if (poll_status > 0) {
        for (int i = 1; i < nfds; ++i) {
            if ((pfd[i].revents & POLLIN) != 0) {
                ipc_handle_readable(ctx->wm, pfd[i].fd);
            }
        }
    }

    return true;
}
