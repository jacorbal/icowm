/**
 * @file tests/ipc/test_readable.c
 *
 * @brief Test battery for the IPC socket's own line-buffering and
 *        readable-descriptor dispatch logic
 *
 * Covers 'ipc_handle_readable' and 'ipc_poll_fds' entirely through
 * 'ipc_test_inject_client' (see its own comment in ipc.h) paired
 * with a real 'socketpair', never through 'ipc_init' itself:
 * 'ipc_init' creates a real runtime directory and binds a real
 * 'AF_UNIX' socket at a real path, neither of which either
 * function's own logic depends on. A 'socketpair' end is a real,
 * connected, bidirectional file descriptor the kernel itself
 * manages, so the module under test sees genuine 'read'/'write'
 * behavior throughout, not a simulation of it.
 *
 * 'ipc_commands_dispatch' itself is stubbed locally to simply echo
 * the request text back as the response, rather than linking the
 * real command-dispatch subsystem (which needs a real 'wm_td' and
 * pulls in a much larger dependency graph of its own, already
 * covered by tests/ipc/test_dispatch.c and tests/ipc/test_commands.c):
 * this file's own point is the line-buffering around it (multiple
 * lines in one read, a line split across two reads, the
 * too-long-without-a-newline case), not the dispatch logic itself.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/ipc.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Local includes */
#include <ipc.h>
#include <harness/tap.h>


/* Trivial, deterministic link-time stand-in; see this file's own top
 * comment for why. Echoes the request back, prefixed, so a test can
 * tell exactly which line reached it. NULL is never returned here,
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
 * @brief Create a connected socket pair, non-blocking on both ends
 *
 * @param out_a First end
 * @param out_b Second end
 *
 * @return @c true on success
 */
static bool s_make_socketpair(int *out_a, int *out_b)
{
    int fds[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        return false;
    }
    if (fcntl(fds[0], F_SETFL, O_NONBLOCK) != 0 ||
            fcntl(fds[1], F_SETFL, O_NONBLOCK) != 0) {
        close(fds[0]);
        close(fds[1]);
        return false;
    }

    *out_a = fds[0];
    *out_b = fds[1];
    return true;
}


/**
 * @brief Read whatever is currently available on @p fd into @p buf
 *
 * @param fd       Descriptor to read from
 * @param buf      Destination buffer
 * @param buf_size Size of @p buf
 *
 * @return Bytes read (0 or more), or a negative value on error or
 *         when nothing was available at all
 */
static ssize_t s_read_available(int fd, char *buf, size_t buf_size)
{
    return read(fd, buf, buf_size - 1u);
}


static void s_test_single_line_dispatched_once(void)
{
    int a;
    int b;
    int slot;
    char reply[256];
    ssize_t n;

    ipc_test_reset();
    ipc_test_set_listening_fd(999);
    TAP_OK(s_make_socketpair(&a, &b), "socketpair created");

    slot = ipc_test_inject_client(a);
    TAP_OK(slot >= 0, "client slot claimed");

    (void) write(b, "hello\n", 6u);
    ipc_handle_readable(NULL, a);

    memset(reply, 0, sizeof(reply));
    n = s_read_available(b, reply, sizeof(reply));
    TAP_OK(n > 0, "a response came back");
    TAP_EQ_STR(reply, "echo:hello\n",
            "the single line was dispatched and echoed with its" \
            " own trailing newline appended");

    close(a);
    close(b);
    ipc_test_reset();
}


static void s_test_multiple_lines_in_one_read_both_dispatched(void)
{
    int a;
    int b;
    char reply[256];
    size_t total = 0u;

    ipc_test_reset();
    ipc_test_set_listening_fd(999);
    TAP_OK(s_make_socketpair(&a, &b), "socketpair created");
    ipc_test_inject_client(a);

    (void) write(b, "first\nsecond\n", 13u);
    ipc_handle_readable(NULL, a);

    /* Both responses arrive back to back; read until both own
     * echoes are seen or no more is available */
    memset(reply, 0, sizeof(reply));
    for (int tries = 0; tries < 4; ++tries) {
        ssize_t n = s_read_available(b, reply + total,
                sizeof(reply) - total);
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

    close(a);
    close(b);
    ipc_test_reset();
}


static void s_test_line_split_across_two_reads(void)
{
    int a;
    int b;
    char reply[256];
    ssize_t n;

    ipc_test_reset();
    ipc_test_set_listening_fd(999);
    TAP_OK(s_make_socketpair(&a, &b), "socketpair created");
    ipc_test_inject_client(a);

    (void) write(b, "par", 3u);
    ipc_handle_readable(NULL, a);

    memset(reply, 0, sizeof(reply));
    n = s_read_available(b, reply, sizeof(reply));
    TAP_OK(n < 0 || n == 0,
            "no response yet: the line has no newline, so nothing" \
            " is dispatched");

    (void) write(b, "tial\n", 5u);
    ipc_handle_readable(NULL, a);

    memset(reply, 0, sizeof(reply));
    n = s_read_available(b, reply, sizeof(reply));
    TAP_OK(n > 0, "a response arrives once the newline finally does");
    TAP_EQ_STR(reply, "echo:partial\n",
            "the two reads were correctly reassembled into one" \
            " complete line before being dispatched");

    close(a);
    close(b);
    ipc_test_reset();
}


static void s_test_poll_fds_lists_only_active_clients(void)
{
    int a1;
    int b1;
    int a2;
    int b2;
    int out_fds[IPC_MAX_CLIENTS + 1];
    int count;
    bool found_a1 = false;
    bool found_a2 = false;

    ipc_test_reset();
    TAP_OK(s_make_socketpair(&a1, &b1) && s_make_socketpair(&a2, &b2),
            "two socketpairs created");
    ipc_test_inject_client(a1);
    ipc_test_inject_client(a2);

    /* A fake, arbitrary positive value; see ipc_test_set_listening_fd's
     * own comment in ipc.h for why this is safe */
    ipc_test_set_listening_fd(999);

    count = ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    TAP_EQ_INT(count, 3,
            "the listening socket plus both injected clients: 3" \
            " total");

    for (int i = 0; i < count; ++i) {
        if (out_fds[i] == a1) { found_a1 = true; }
        if (out_fds[i] == a2) { found_a2 = true; }
    }
    TAP_OK(found_a1 && found_a2,
            "both injected clients' own fds are present in the list");

    close(a1);
    close(b1);
    close(a2);
    close(b2);
    ipc_test_reset();
}


static void s_test_inject_beyond_capacity_fails(void)
{
    int fds_a[IPC_MAX_CLIENTS];
    int fds_b[IPC_MAX_CLIENTS];
    bool all_ok = true;
    int extra_a;
    int extra_b;

    ipc_test_reset();

    for (int i = 0; i < IPC_MAX_CLIENTS && all_ok; ++i) {
        all_ok = s_make_socketpair(&fds_a[i], &fds_b[i]);
        if (all_ok) {
            all_ok = ipc_test_inject_client(fds_a[i]) >= 0;
        }
    }
    TAP_OK(all_ok, "filling every client slot succeeds");

    TAP_OK(s_make_socketpair(&extra_a, &extra_b),
            "one more socketpair created");
    TAP_EQ_INT(ipc_test_inject_client(extra_a), -1,
            "injecting one more past the limit fails");

    for (int i = 0; i < IPC_MAX_CLIENTS; ++i) {
        close(fds_a[i]);
        close(fds_b[i]);
    }
    close(extra_a);
    close(extra_b);
    ipc_test_reset();
}


int main(void)
{
    TAP_PLAN(17);

    s_test_single_line_dispatched_once();
    s_test_multiple_lines_in_one_read_both_dispatched();
    s_test_line_split_across_two_reads();
    s_test_poll_fds_lists_only_active_clients();
    s_test_inject_beyond_capacity_fails();

    return TAP_DONE();
}
