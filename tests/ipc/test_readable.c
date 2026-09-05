/**
 * @file tests/ipc/test_readable.c
 *
 * @brief Test battery for the IPC socket's own line-buffering and
 *        readable-descriptor dispatch logic
 *
 * Covers 'ipc_handle_readable' and 'ipc_poll_fds' entirely through
 * real, connected 'AF_UNIX' sockets driven by a real 'ipc_init',
 * the same way 'tests/test_ipc.c' does: a real client 'connect's to
 * the real listening socket, 'ipc_poll_fds' finds it, and 'ipc_
 * handle_readable' genuinely accepts and later services it, all
 * without any X server or mock file descriptors.  This file's own
 * focus is what 'ipc_handle_readable' does once a connection is
 * already open (multiple lines arriving in one read, a line split
 * across two reads, and a connection accepted once every client
 * slot is already taken), rather than the accept path itself, which
 * 'tests/test_ipc.c' already covers.
 *
 * 'ipc_commands_dispatch' itself is stubbed locally to simply echo
 * the request text back as the response, rather than linking the
 * real command-dispatch subsystem (which needs a real 'wm_td' and
 * pulls in a much larger dependency graph of its own, already
 * covered by tests/ipc/test_dispatch.c and tests/ipc/test_commands.c):
 * this file's own point is the line-buffering around it (multiple
 * lines in one read, a line split across two reads), not the
 * dispatch logic itself.
 *
 * 'XDG_RUNTIME_DIR' is deliberately overridden to a fresh directory
 * this file creates under '/tmp' for its own run, both so this suite
 * never binds over a real IcoWM instance's own control socket and so
 * repeated runs never trip the stale-'EEXIST'-but-wrong-owner guard
 * in 's_runtime_dir_ensure'.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#define _POSIX_C_SOURCE 200112L /* setenv */

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/ipc.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Local includes */
#include <harness/tap.h>
#include <ipc.h>


/* Trivial, deterministic link-time stand-in; see this file's own top
 * comment for why.  Echoes the request back, prefixed, so a test can
 * tell exactly which line reached it.  NULL is never returned here,
 * so every dispatched line always produces a response. */
char *ipc_commands_dispatch(wm_td *wm, const char *request,
        int client_idx)
{
    char *response;
    size_t len;

    (void) wm;
    (void) client_idx;

    len = safe_strlen(request) + 6u;
    response = malloc(len);
    if (response != NULL) {
        (void) safe_strncpy(response, "echo:", len);
        (void) safe_strncat(response, request, len);
    }
    return response;
}


/**
 * @brief Point 'XDG_RUNTIME_DIR' at a fresh, private directory for
 *        the whole run
 *
 * Built from the process's own PID, the same way 'tests/test_ipc.c'
 * builds its own, so this file's socket never collides with a real
 * session's, nor with that other file's, even if both happened to
 * run at once
 */
static void s_use_private_runtime_dir(void)
{
    char dir[256];

    (void) snprintf(dir, sizeof(dir), "/tmp/icowm-test-readable-%ld",
            (long) getpid());
    (void) mkdir(dir, 0700);
    (void) setenv("XDG_RUNTIME_DIR", dir, 1);
}


/**
 * @brief Connect a fresh, blocking client socket to whichever path
 *        'ipc_init' most recently bound
 *
 * @return A connected client descriptor, or -1 on failure
 */
static int s_connect_client(void)
{
    struct sockaddr_un addr;
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    int fd;

    if (runtime_dir == NULL) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void) snprintf(addr.sun_path, sizeof(addr.sun_path),
            "%s/icowm/%s", runtime_dir, IPC_SOCKET_FILENAME);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}


/**
 * @brief Connect one more client and drive its acceptance through a
 *        real 'ipc_handle_readable' call on the listening socket
 *
 * @param listening_fd The real listening socket, as reported by a
 *                      prior 'ipc_poll_fds' call
 * @param known_fds     Every non-listening descriptor already
 *                       accounted for by the caller
 * @param known_count   How many entries 'known_fds' holds
 * @param out_client_fd Receives the connecting client's own socket
 *
 * @return The freshly accepted connection's own descriptor, as
 *         'ipc_poll_fds' now reports it, or -1 on any failure along
 *         the way, including the new connection being dropped
 *         instead of accepted (a full client table, say)
 */
static int s_connect_and_accept(int listening_fd, const int *known_fds,
        int known_count, int *out_client_fd)
{
    int client_fd;
    int out_fds[IPC_MAX_CLIENTS + 1];
    int count;
    bool is_known;

    client_fd = s_connect_client();
    if (client_fd < 0) {
        return -1;
    }

    ipc_handle_readable(NULL, listening_fd);

    count = ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    for (int i = 0; i < count; ++i) {
        if (out_fds[i] == listening_fd) {
            continue;
        }
        is_known = false;
        for (int j = 0; j < known_count; ++j) {
            if (out_fds[i] == known_fds[j]) {
                is_known = true;
                break;
            }
        }
        if (!is_known) {
            *out_client_fd = client_fd;
            return out_fds[i];
        }
    }

    close(client_fd);
    return -1;
}


/* A single line, written and read back in one shot, is dispatched
 * exactly once and echoed with its own trailing newline intact */
static void s_test_single_line_dispatched_once(void)
{
    int out_fds[IPC_MAX_CLIENTS + 1];
    int listening_fd;
    int client_fd = -1;
    int accepted_fd;
    char reply[256];
    ssize_t n;

    TAP_EQ_INT(ipc_init(), 0, "ipc_init succeeds");
    (void) ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    listening_fd = out_fds[0];

    accepted_fd = s_connect_and_accept(listening_fd, NULL, 0,
            &client_fd);
    TAP_OK(accepted_fd >= 0, "a real client was accepted");

    (void) write(client_fd, "hello\n", 6u);
    ipc_handle_readable(NULL, accepted_fd);

    memset(reply, 0, sizeof(reply));
    n = read(client_fd, reply, sizeof(reply) - 1u);
    TAP_OK(n > 0, "a response came back");
    TAP_EQ_STR(reply, "echo:hello\n",
            "the single line was dispatched and echoed with its" \
            " own trailing newline appended");

    if (client_fd >= 0) {
        close(client_fd);
    }
    ipc_destroy();
}


/* Two lines arriving in a single read are both dispatched, one call
 * to ipc_handle_readable reaching both of their own echoes */
static void s_test_multiple_lines_in_one_read_both_dispatched(void)
{
    int out_fds[IPC_MAX_CLIENTS + 1];
    int listening_fd;
    int client_fd = -1;
    int accepted_fd;
    char reply[256];
    ssize_t n;
    size_t total = 0u;

    TAP_EQ_INT(ipc_init(), 0, "ipc_init succeeds");
    (void) ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    listening_fd = out_fds[0];

    accepted_fd = s_connect_and_accept(listening_fd, NULL, 0,
            &client_fd);
    TAP_OK(accepted_fd >= 0, "a real client was accepted");

    (void) write(client_fd, "first\nsecond\n", 13u);
    ipc_handle_readable(NULL, accepted_fd);

    /* Both responses arrive back to back; read until both own
     * echoes are seen or no more is available */
    memset(reply, 0, sizeof(reply));
    (void) fcntl(client_fd, F_SETFL, O_NONBLOCK);
    for (int tries = 0; tries < 4; ++tries) {
        n = read(client_fd, reply + total, sizeof(reply) - total - 1u);
        if (n <= 0) {
            break;
        }
        total += (size_t) n;
    }

    TAP_OK(strstr(reply, "echo:first\n") != NULL,
            "the first line's own response is present");
    TAP_OK(strstr(reply, "echo:second\n") != NULL,
            "the second line's own response is present too, from" \
            " the very same read");

    if (client_fd >= 0) {
        close(client_fd);
    }
    ipc_destroy();
}


/* A line split across two separate reads is only dispatched once its
 * newline finally arrives, correctly reassembled */
static void s_test_line_split_across_two_reads(void)
{
    int out_fds[IPC_MAX_CLIENTS + 1];
    int listening_fd;
    int client_fd = -1;
    int accepted_fd;
    char reply[256];
    ssize_t n;

    TAP_EQ_INT(ipc_init(), 0, "ipc_init succeeds");
    (void) ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    listening_fd = out_fds[0];

    accepted_fd = s_connect_and_accept(listening_fd, NULL, 0,
            &client_fd);
    TAP_OK(accepted_fd >= 0, "a real client was accepted");
    (void) fcntl(client_fd, F_SETFL, O_NONBLOCK);

    (void) write(client_fd, "par", 3u);
    ipc_handle_readable(NULL, accepted_fd);

    memset(reply, 0, sizeof(reply));
    n = read(client_fd, reply, sizeof(reply) - 1u);
    TAP_OK(n < 0 || n == 0,
            "no response yet: the line has no newline, so nothing" \
            " is dispatched");

    (void) write(client_fd, "tial\n", 5u);
    ipc_handle_readable(NULL, accepted_fd);

    memset(reply, 0, sizeof(reply));
    n = read(client_fd, reply, sizeof(reply) - 1u);
    TAP_OK(n > 0, "a response arrives once the newline finally does");
    TAP_EQ_STR(reply, "echo:partial\n",
            "the two reads were correctly reassembled into one" \
            " complete line before being dispatched");

    if (client_fd >= 0) {
        close(client_fd);
    }
    ipc_destroy();
}


/* ipc_poll_fds lists the listening socket plus exactly every client
 * actually accepted so far, no more and no less */
static void s_test_poll_fds_lists_only_active_clients(void)
{
    int out_fds[IPC_MAX_CLIENTS + 1];
    int listening_fd;
    int client_fd_1 = -1;
    int client_fd_2 = -1;
    int accepted_1;
    int accepted_2;
    int count;
    bool found_1 = false;
    bool found_2 = false;

    TAP_EQ_INT(ipc_init(), 0, "ipc_init succeeds");
    (void) ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    listening_fd = out_fds[0];

    accepted_1 = s_connect_and_accept(listening_fd, NULL, 0,
            &client_fd_1);
    TAP_OK(accepted_1 >= 0, "the first client was accepted");
    accepted_2 = s_connect_and_accept(listening_fd, &accepted_1, 1,
            &client_fd_2);
    TAP_OK(accepted_2 >= 0, "the second client was accepted too");

    count = ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    TAP_EQ_INT(count, 3,
            "the listening socket plus both accepted clients: 3" \
            " total");

    for (int i = 0; i < count; ++i) {
        if (out_fds[i] == accepted_1) { found_1 = true; }
        if (out_fds[i] == accepted_2) { found_2 = true; }
    }
    TAP_OK(found_1 && found_2,
            "both accepted clients' own descriptors are present in" \
            " the list");

    if (client_fd_1 >= 0) {
        close(client_fd_1);
    }
    if (client_fd_2 >= 0) {
        close(client_fd_2);
    }
    ipc_destroy();
}


/* Once every client slot is already taken, one more connection is
 * accepted at the socket level and then dropped, rather than
 * displacing an existing client or growing the reported list */
static void s_test_capacity_limit_drops_extra_connection(void)
{
    int out_fds[IPC_MAX_CLIENTS + 1];
    int listening_fd;
    int client_fds[IPC_MAX_CLIENTS];
    int accepted_fds[IPC_MAX_CLIENTS];
    int extra_client_fd = -1;
    int extra_accepted;
    bool all_ok = true;
    char reply[256];
    ssize_t n;
    int count;

    TAP_EQ_INT(ipc_init(), 0, "ipc_init succeeds");
    (void) ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    listening_fd = out_fds[0];

    for (int i = 0; i < IPC_MAX_CLIENTS && all_ok; ++i) {
        accepted_fds[i] = s_connect_and_accept(listening_fd,
                accepted_fds, i, &client_fds[i]);
        all_ok = accepted_fds[i] >= 0;
    }
    TAP_OK(all_ok, "filling every client slot succeeds");

    extra_client_fd = s_connect_client();
    TAP_OK(extra_client_fd >= 0, "one more client connects");
    (void) fcntl(extra_client_fd, F_SETFL, O_NONBLOCK);

    ipc_handle_readable(NULL, listening_fd);

    count = ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    TAP_EQ_INT(count, IPC_MAX_CLIENTS + 1,
            "the reported list still holds only the listening" \
            " socket plus the already-full set of clients");

    extra_accepted = -1;
    for (int i = 0; i < count; ++i) {
        if (out_fds[i] == listening_fd) {
            continue;
        }
        extra_accepted = out_fds[i];
        for (int j = 0; j < IPC_MAX_CLIENTS; ++j) {
            if (out_fds[i] == accepted_fds[j]) {
                extra_accepted = -1;
                break;
            }
        }
        if (extra_accepted >= 0) {
            break;
        }
    }
    TAP_OK(extra_accepted < 0,
            "no new descriptor beyond the already-full set of" \
            " clients was added");

    memset(reply, 0, sizeof(reply));
    n = read(extra_client_fd, reply, sizeof(reply) - 1u);
    TAP_EQ_INT((int) n, 0,
            "the extra connection was accepted and then closed" \
            " right back, instead of being kept as a client");

    for (int i = 0; i < IPC_MAX_CLIENTS; ++i) {
        if (client_fds[i] >= 0) {
            close(client_fds[i]);
        }
    }
    if (extra_client_fd >= 0) {
        close(extra_client_fd);
    }
    ipc_destroy();
}


int main(void)
{
    TAP_PLAN(24);

    s_use_private_runtime_dir();

    s_test_single_line_dispatched_once();
    s_test_multiple_lines_in_one_read_both_dispatched();
    s_test_line_split_across_two_reads();
    s_test_poll_fds_lists_only_active_clients();
    s_test_capacity_limit_drops_extra_connection();

    return TAP_DONE();
}
