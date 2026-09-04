/**
 * @file tests/test_ipc.c
 *
 * @brief Test battery for the IPC control socket's own lifecycle,
 *        connection handling, and event subscription bookkeeping
 *
 * Exercises 'ipc_init', 'ipc_destroy', 'ipc_poll_fds', 'ipc_handle_
 * readable', 'ipc_client_subscribe', 'ipc_client_unsubscribe', and
 * 'ipc_broadcast_event' (ipc.c) end to end, entirely through their
 * own real public API and a real, connected 'AF_UNIX' socket: 'ipc_
 * init' binds a real listening socket under a real (test-local)
 * runtime directory, a real client 'connect's to it, 'ipc_poll_fds'
 * finds the listening descriptor becoming readable, and 'ipc_handle_
 * readable' genuinely accepts the connection, all without any X
 * server, mock file descriptors, or test-only hooks, since none of
 * that machinery depends on one.  'ipc_commands_dispatch' (ipc/
 * commands.c) is the one link-only stand-in in this file: it is a
 * genuinely separate module (already covered on its own by tests/
 * ipc/test_dispatch.c and tests/ipc/test_commands.c) that 'ipc.c'
 * only ever calls through, so replacing it here lets a real line
 * written to the connected socket travel all the way through 'ipc.c'
 * own line-buffering and dispatch-and-respond logic while also
 * capturing the exact 'client_idx' 'ipc.c' assigned that connection,
 * the one piece of information no public function otherwise exposes.
 * That captured index is what then lets 'ipc_client_subscribe' and
 * 'ipc_client_unsubscribe' be called directly, against the exact
 * live slot 'ipc.c' itself allocated for a real connection, rather
 * than a guessed or fabricated one.
 *
 * 'XDG_RUNTIME_DIR' is deliberately overridden to a fresh directory
 * this file creates under '/tmp' for its own run, both so this suite
 * never binds over a real IcoWM instance's own control socket and so
 * repeated runs never trip the stale-'EEXIST'-but-wrong-owner guard
 * in 's_runtime_dir_ensure'.
 *
 * 'wm_td' is only ever passed through to the stubbed 'ipc_commands_
 * dispatch' below, never dereferenced by 'ipc.c' itself, so 'NULL'
 * stands in for it throughout, the same way 'ipc_handle_readable'
 * is documented to accept whatever the caller already has on hand.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#define _POSIX_C_SOURCE 200112L /* setenv, unsetenv */

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

/* Local includes */
#include <harness/tap.h>
#include <ipc.h>


/** Captured 'client_idx' of the most recently dispatched request,
 *  and how many requests have been dispatched so far in total;
 *  reset by 's_reset' before each scenario */
static int s_dispatch_calls;
static int s_dispatch_last_client_idx;


/**
 * @brief Link-only stand-in for @a ipc_commands_dispatch
 *
 * Echoes the request back, prefixed, exactly like the equivalent
 * stand-in in tests/ipc/test_readable.c, plus records which
 * 'client_idx' this specific call arrived with so a scenario here
 * can turn around and call 'ipc_client_subscribe'/'ipc_client_
 * unsubscribe' against that exact live slot.
 *
 * @note Complexity: @e O(n), where @e n is the length of @p request
 */
char *ipc_commands_dispatch(wm_td *wm, const char *request,
        int client_idx)
{
    char *response;
    size_t len;

    (void) wm;

    s_dispatch_calls++;
    s_dispatch_last_client_idx = client_idx;

    len = strlen(request) + 6u;
    response = malloc(len);
    if (response != NULL) {
        (void) snprintf(response, len, "echo:%s", request);
    }
    return response;
}


static void s_reset_dispatch_recording(void)
{
    s_dispatch_calls = 0;
    s_dispatch_last_client_idx = -1;
}


/**
 * @brief Point 'XDG_RUNTIME_DIR' at a fresh, private directory for
 *        the whole run
 *
 * Done once from 'main', before any 'ipc_init' call, so every
 * scenario below binds its socket under a location this test process
 * owns outright, never a real desktop session's own runtime
 * directory.  Built from the process's own PID rather than through
 * 'mkdtemp', which needs a POSIX feature test macro beyond this
 * project's own '_POSIX_C_SOURCE' target; a PID-qualified path
 * already is not shared by any two processes running at once, which
 * is all the uniqueness this one-off test directory ever needs.
 */
static void s_use_private_runtime_dir(void)
{
    char dir[256];

    (void) snprintf(dir, sizeof(dir), "/tmp/icowm-test-ipc-%ld",
            (long) getpid());
    (void) mkdir(dir, 0700);
    (void) setenv("XDG_RUNTIME_DIR", dir, 1);
}


/**
 * @brief Connect a fresh client socket to whichever path 'ipc_init'
 *        most recently bound
 *
 * Reads the same 'XDG_RUNTIME_DIR' variable 'ipc_init' itself
 * resolves against, rather than duplicating its own guess at the
 * path, so this stays correct even if that resolution logic ever
 * changes.
 *
 * @return A connected, blocking client descriptor, or -1 on failure
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
 * @brief Drive one connection all the way from 'connect' through a
 *        real 'ipc_handle_readable'-driven accept
 *
 * @param out_client_fd Receives the connected client's own socket
 *
 * @return The 'client_idx' 'ipc.c' assigned the accepted connection,
 *         recovered by sending one dispatched line through it and
 *         reading the stubbed handler's own recorded index back, or
 *         -1 on any failure along the way
 */
static int s_accept_one_client(int *out_client_fd)
{
    int out_fds[IPC_MAX_CLIENTS + 1];
    int count;
    int listening_fd = -1;
    int client_fd;
    char reply[256];
    ssize_t n;

    client_fd = s_connect_client();
    if (client_fd < 0) {
        return -1;
    }

    count = ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    if (count < 1) {
        close(client_fd);
        return -1;
    }
    listening_fd = out_fds[0];

    ipc_handle_readable(NULL, listening_fd);

    s_reset_dispatch_recording();
    (void) write(client_fd, "ping\n", 5u);

    /* The freshly accepted connection's own descriptor is whichever
     * one 'ipc_poll_fds' now reports beyond the listening socket
     * that was not there before; simplest to find directly is to
     * just feed every non-listening descriptor to 'ipc_handle_
     * readable' and see which one actually produces a dispatch. */
    count = ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    for (int i = 0; i < count; ++i) {
        if (out_fds[i] == listening_fd) {
            continue;
        }
        ipc_handle_readable(NULL, out_fds[i]);
        if (s_dispatch_calls > 0) {
            break;
        }
    }

    if (s_dispatch_calls != 1) {
        close(client_fd);
        return -1;
    }

    memset(reply, 0, sizeof(reply));
    n = read(client_fd, reply, sizeof(reply) - 1u);
    if (n <= 0 || strcmp(reply, "echo:ping\n") != 0) {
        close(client_fd);
        return -1;
    }

    *out_client_fd = client_fd;
    return s_dispatch_last_client_idx;
}


/* ipc_init followed immediately by ipc_destroy tears the listening
 * socket back down cleanly, and a second ipc_init afterward succeeds
 * again from a clean slate */
static void s_test_init_destroy_lifecycle(void)
{
    int status;

    status = ipc_init();
    TAP_EQ_INT(status, 0, "ipc_init succeeds");

    TAP_EQ_INT(ipc_init(), 0,
            "calling ipc_init again while already up is a" \
            " successful no-op");

    ipc_destroy();

    status = ipc_init();
    TAP_EQ_INT(status, 0,
            "ipc_init succeeds again after a clean ipc_destroy");

    ipc_destroy();
}


/* ipc_poll_fds reports nothing at all once the socket has never been
 * brought up, or after it was already torn down */
static void s_test_poll_fds_empty_when_down(void)
{
    int out_fds[4];

    ipc_destroy();
    TAP_EQ_INT(ipc_poll_fds(out_fds, 4), 0,
            "ipc_poll_fds reports zero descriptors while the socket" \
            " is down");
}


/* ipc_poll_fds lists exactly the listening socket, and nothing else,
 * right after ipc_init with no connections yet */
static void s_test_poll_fds_lists_listening_socket_only(void)
{
    int out_fds[IPC_MAX_CLIENTS + 1];
    int count;

    TAP_EQ_INT(ipc_init(), 0, "ipc_init succeeds");

    count = ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    TAP_EQ_INT(count, 1,
            "only the listening socket itself is reported with no" \
            " clients connected yet");
    TAP_OK(out_fds[0] >= 0,
            "the one descriptor reported is a valid, non-negative" \
            " file descriptor");

    ipc_destroy();
}


/* ipc_handle_readable, given the listening socket, genuinely accepts
 * a real connecting client, which ipc_poll_fds then reports too */
static void s_test_handle_readable_accepts_real_connection(void)
{
    int client_fd = -1;
    int client_idx;
    int out_fds[IPC_MAX_CLIENTS + 1];
    int count;

    TAP_EQ_INT(ipc_init(), 0, "ipc_init succeeds");

    client_idx = s_accept_one_client(&client_fd);
    TAP_OK(client_idx >= 0,
            "a real client was accepted and its own dispatched" \
            " request was echoed back correctly");

    count = ipc_poll_fds(out_fds, IPC_MAX_CLIENTS + 1);
    TAP_EQ_INT(count, 2,
            "the listening socket plus the one now-connected" \
            " client are both reported");

    if (client_fd >= 0) {
        close(client_fd);
    }
    ipc_destroy();
}


/* ipc_client_subscribe rejects a missing, empty, wrongly typed, or
 * unrecognized events list, without ever touching subscription state */
static void s_test_subscribe_rejects_bad_input(void)
{
    int client_fd = -1;
    int client_idx;
    cJSON *args;
    cJSON *resp;
    cJSON *ok_field;

    TAP_EQ_INT(ipc_init(), 0, "ipc_init succeeds");
    client_idx = s_accept_one_client(&client_fd);
    TAP_OK(client_idx >= 0, "a real client was accepted");

    args = cJSON_CreateObject();
    resp = ipc_client_subscribe(client_idx, args);
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsFalse(ok_field),
            "a missing 'events' field is rejected");
    cJSON_Delete(args);
    cJSON_Delete(resp);

    args = cJSON_CreateObject();
    cJSON_AddItemToObject(args, "events", cJSON_CreateArray());
    resp = ipc_client_subscribe(client_idx, args);
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsFalse(ok_field),
            "an empty 'events' array is rejected");
    cJSON_Delete(args);
    cJSON_Delete(resp);

    args = cJSON_CreateObject();
    cJSON_AddItemToObject(args, "events",
            cJSON_CreateStringArray(
                    (const char *[]) { "not_a_real_event" }, 1));
    resp = ipc_client_subscribe(client_idx, args);
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsFalse(ok_field),
            "an unrecognized event name is rejected");
    cJSON_Delete(args);
    cJSON_Delete(resp);

    if (client_fd >= 0) {
        close(client_fd);
    }
    ipc_destroy();
}


/* ipc_client_subscribe with a valid event name later lets
 * ipc_broadcast_event reach that same client; ipc_client_unsubscribe
 * with no 'events' field then clears every subscription at once, and
 * that same broadcast no longer reaches it */
static void s_test_subscribe_then_broadcast_then_unsubscribe_all(void)
{
    int client_fd = -1;
    int client_idx;
    cJSON *args;
    cJSON *resp;
    cJSON *ok_field;
    char buf[256];
    ssize_t n;

    TAP_EQ_INT(ipc_init(), 0, "ipc_init succeeds");
    client_idx = s_accept_one_client(&client_fd);
    TAP_OK(client_idx >= 0, "a real client was accepted");

    args = cJSON_CreateObject();
    cJSON_AddItemToObject(args, "events",
            cJSON_CreateStringArray(
                    (const char *[]) { "window_mapped" }, 1));
    resp = ipc_client_subscribe(client_idx, args);
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "subscribing to a real, recognized event name succeeds");
    cJSON_Delete(args);
    cJSON_Delete(resp);

    ipc_broadcast_event(IPC_EVENT_WINDOW_MAPPED, NULL);

    memset(buf, 0, sizeof(buf));
    n = read(client_fd, buf, sizeof(buf) - 1u);
    TAP_OK(n > 0 && strstr(buf, "\"event\":\"window_mapped\"") != NULL,
            "the subscribed client receives the broadcast event," \
            " naming it by its wire name");

    args = cJSON_CreateObject();
    resp = ipc_client_unsubscribe(client_idx, args);
    ok_field = cJSON_GetObjectItem(resp, "ok");
    TAP_OK(ok_field != NULL && cJSON_IsTrue(ok_field),
            "unsubscribing from everything at once (no 'events'" \
            " field) succeeds");
    cJSON_Delete(args);
    cJSON_Delete(resp);

    ipc_broadcast_event(IPC_EVENT_WINDOW_MAPPED, NULL);

    n = fcntl(client_fd, F_SETFL, O_NONBLOCK);
    TAP_EQ_INT((int) n, 0, "the client socket was made non-blocking" \
            " to check for absence of a second event");
    memset(buf, 0, sizeof(buf));
    n = read(client_fd, buf, sizeof(buf) - 1u);
    TAP_OK(n <= 0,
            "after unsubscribing from everything, the same broadcast" \
            " no longer reaches this client");

    if (client_fd >= 0) {
        close(client_fd);
    }
    ipc_destroy();
}


/* ipc_broadcast_event is a safe no-op, freeing its fields argument,
 * when the socket is not up at all */
static void s_test_broadcast_noop_when_down(void)
{
    cJSON *fields = cJSON_CreateObject();

    ipc_destroy();
    cJSON_AddStringToObject(fields, "marker", "value");

    /* Nothing observable to assert on beyond "this does not crash"
     * (verified by AddressSanitizer/UndefinedBehaviorSanitizer, since
     * 'fields' is always freed internally either way); TAP_OK below
     * exists so this scenario still counts toward the plan and
     * confirms execution reached this point. */
    ipc_broadcast_event(IPC_EVENT_WINDOW_MAPPED, fields);
    TAP_OK(true,
            "broadcasting while the socket is down returns safely" \
            " without crashing");
}


int main(void)
{
    TAP_PLAN(23);

    s_use_private_runtime_dir();

    s_test_init_destroy_lifecycle();
    s_test_poll_fds_empty_when_down();
    s_test_poll_fds_lists_listening_socket_only();
    s_test_handle_readable_accepts_real_connection();
    s_test_subscribe_rejects_bad_input();
    s_test_subscribe_then_broadcast_then_unsubscribe_all();
    s_test_broadcast_noop_when_down();

    return TAP_DONE();
}
