/**
 * @file ipc.c
 *
 * @brief IPC control socket implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* fcntl, F_SETFL, O_NONBLOCK */


/* System includes */
#include <errno.h>
#include <fcntl.h>      /* fcntl, F_SETFL, O_NONBLOCK */
#include <stdbool.h>
#include <stdio.h>      /* NULL, snprintf */
#include <stdlib.h>     /* free */
#include <string.h>     /* memchr, memmove, memset, strerror */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>     /* close, getuid, read, unlink, write */

/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/config.h>
#include <defs/ipc.h>

/* Utils includes */
#include <utils/config/path.h>
#include <utils/safe/safestr.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <ipc.h>
#include <ipc/commands.h>


/**
 * @brief One connected client's read state
 */
struct s_ipc_client_s {
    size_t buf_len;                    /**< Bytes currently buffered,
                                            not yet a complete line */
    int fd;                            /**< -1 when this slot is free */

    /** Bitmask of 'enum ipc_event_type_e'; 0 means none, which is
     *  correctly the same as this static array's zero-initialized
     *  default */
    uint32_t subscribed_events;

    char buf[IPC_MSG_MAX_LENGTH];
};


/** Every currently connected client, indexed by slot */
static struct s_ipc_client_s s_clients[IPC_MAX_CLIENTS];

/**
 * @brief Whether @c s_clients has had every slot's @c fd set to -1 yet
 *
 * Needed because static storage only zero-initializes it by default,
 * and @c 0 is itself a valid, real file descriptor (@c stdin); every
 * "is this slot free" check in this file relies on @c -1 specifically
 * meaning free, so every slot must be set to that explicitly, once,
 * before any of those checks run for the first time.
 */
static bool s_clients_initialized = false;


/** Listening socket descriptor, or -1 when not up */
static int s_ipc_fd = -1;


/** Full path of the socket file currently bound, for 'ipc_destroy' to
 *  unlink; empty when not up */
static char s_ipc_socket_path[CONFIG_MAX_LENGTH_PATH_BASE] = { 0 };


/**
 * @brief One event type's name and bit, shared by both directions of
 *        the name-to-bit mapping below
 *
 * Subscribing reads a name off the wire and needs its bit; broadcasting
 * has a bit and needs to write its name back out, so the two stay in
 * step by construction rather than by two lists someone has to remember
 * to edit together.
 */
struct s_ipc_event_def_s {
    const char *name;
    uint32_t bit;
};


/**
 * @brief Event structure
 */
static const struct s_ipc_event_def_s s_event_defs[] = {
    { "window_mapped",    IPC_EVENT_WINDOW_MAPPED },
    { "window_closed",    IPC_EVENT_WINDOW_CLOSED },
    { "desktop_switched", IPC_EVENT_DESKTOP_SWITCHED },
    { "focus_changed",    IPC_EVENT_FOCUS_CHANGED },
    { "urgency_set",      IPC_EVENT_URGENCY_SET },
    { "urgency_cleared",  IPC_EVENT_URGENCY_CLEARED },
    { "window_moved",     IPC_EVENT_WINDOW_MOVED },
    { "window_resized",   IPC_EVENT_WINDOW_RESIZED },
    { "rule_applied",     IPC_EVENT_RULE_APPLIED },
    { "pin_set",              IPC_EVENT_PIN_SET },
    { "pin_cleared",          IPC_EVENT_PIN_CLEARED },
    { "fullscreen_set",       IPC_EVENT_FULLSCREEN_SET },
    { "fullscreen_cleared",   IPC_EVENT_FULLSCREEN_CLEARED },
    { "shade_set",            IPC_EVENT_SHADE_SET },
    { "shade_cleared",        IPC_EVENT_SHADE_CLEARED },
    { "hide_set",             IPC_EVENT_HIDE_SET },
    { "hide_cleared",         IPC_EVENT_HIDE_CLEARED },
    { "decoration_set",       IPC_EVENT_DECORATION_SET },
    { "decoration_cleared",   IPC_EVENT_DECORATION_CLEARED },
    { "client_iconified",     IPC_EVENT_CLIENT_ICONIFIED },
    { "client_deiconified",   IPC_EVENT_CLIENT_DEICONIFIED },
    { "layer_changed",        IPC_EVENT_LAYER_CHANGED },
    { "client_desktop_changed", IPC_EVENT_CLIENT_DESKTOP_CHANGED },
    { "client_renamed",       IPC_EVENT_CLIENT_RENAMED },
    { "client_reclassed",     IPC_EVENT_CLIENT_RECLASSED },
    { "client_reroled",       IPC_EVENT_CLIENT_REROLED },
    { "client_icon_changed",  IPC_EVENT_CLIENT_ICON_CHANGED },
    { "desktop_background_changed", IPC_EVENT_DESKTOP_BACKGROUND_CHANGED },
    { "desktop_shown",        IPC_EVENT_DESKTOP_SHOWN },
    { "desktop_hidden",       IPC_EVENT_DESKTOP_HIDDEN },
    { "config_reloaded",      IPC_EVENT_CONFIG_RELOADED },
    { "stacking_changed",     IPC_EVENT_STACKING_CHANGED },
};


/**
 * @brief Ensure the runtime directory exists, belongs to the calling
 *        user, and has exactly 'IPC_RUNTIME_DIR_MODE' permissions
 *
 * Creates it fresh when nothing is there yet.  When something already
 * is (most commonly @c XDG_RUNTIME_DIR itself, already created by the
 * session; occasionally a leftover subdirectory of ours from a previous
 * run), verifies it is actually a directory this user owns before
 * trusting it, since the '/tmp' fallback path is a location other users
 * on the same system can also write to, and re-applies the mode
 * regardless of whether it already matched, rather than assuming
 * a pre-existing directory's permissions were already correct.
 *
 * @param dir Path to the runtime directory
 *
 * @return 0 on success, -1 on failure (reason logged)
 *
 * @note Complexity: @e O(1)
 */
static int s_runtime_dir_ensure(const char *dir)
{
    struct stat st;

    if (mkdir(dir, IPC_RUNTIME_DIR_MODE) == 0) {
        return 0;
    }

    if (errno != EEXIST) {
        LOGGER_ERROR("Failed to create IPC runtime directory '%s': %s",
                dir, strerror(errno));
        return -1;
    }

    if (stat(dir, &st) != 0) {
        LOGGER_ERROR("IPC runtime directory '%s' exists but could" \
                " not be inspected: %s", dir, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        LOGGER_ERROR("IPC runtime path '%s' exists and is not a" \
                " directory", dir);
        return -1;
    }

    if (st.st_uid != getuid()) {
        LOGGER_ERROR("IPC runtime directory '%s' exists but is not" \
                " owned by this user; refusing to use it", dir);
        return -1;
    }

    if (chmod(dir, IPC_RUNTIME_DIR_MODE) != 0) {
        LOGGER_ERROR("Failed to set permissions on IPC runtime" \
                " directory '%s': %s", dir, strerror(errno));
        return -1;
    }

    return 0;
}


/**
 * @brief Close one connected client's descriptor and free its slot
 *
 * @param idx Index into 's_clients'
 *
 * @note Complexity: @e O(1)
 */
static void s_client_close(int idx)
{
    if (s_clients[idx].fd == -1) {
        return;
    }
    (void) close(s_clients[idx].fd);
    s_clients[idx].fd = -1;
    s_clients[idx].buf_len = 0;
    s_clients[idx].subscribed_events = 0;
}


#define S_IPC_EVENT_COUNT \
    (sizeof(s_event_defs) / sizeof(s_event_defs[0]))


/**
 * @brief Look up one event type's name
 *
 * @param name Event name, as given on the wire
 *
 * @return The matching bit, or @c 0 (no @c enum @c ipc_event_type_e
 *         value is ever itself @c 0) when @p name is not a recognized
 *         event
 *
 * @note Complexity: @e O(1) (a handful of entries, checked linearly)
 */
static uint32_t s_event_name_to_bit(const char *name)
{
    for (size_t i = 0; i < S_IPC_EVENT_COUNT; ++i) {
        if (safe_strcmp(s_event_defs[i].name, name) == 0) {
            return s_event_defs[i].bit;
        }
    }
    return 0;
}


/**
 * @brief Look up one event type's bit
 *
 * @param type The event type
 *
 * @return Its name, or @c NULL when @p type does not match any known
 *         single event bit
 *
 * @note Complexity: @e O(1) (a handful of entries, checked linearly)
 */
static const char *s_event_bit_to_name(uint32_t type)
{
    for (size_t i = 0; i < S_IPC_EVENT_COUNT; ++i) {
        if (s_event_defs[i].bit == type) {
            return s_event_defs[i].name;
        }
    }
    return NULL;
}


/**
 * @brief Whether an 'errno' value from a non-blocking socket call
 *        means "nothing ready right now", not a real error
 *
 * POSIX allows @c EAGAIN and @c EWOULDBLOCK to be either the same or
 * two distinct values, depending on the platform; checking both by
 * name, unconditionally, is the traditional portable idiom for that
 * reason.
 *
 * On glibc/Linux they are defined to the exact same number, which turns
 * that same traditional check into a comparison against itself twice,
 * something GCC's '-Wlogical-op' rightly flags.
 * The '#if' below only compares against @c EWOULDBLOCK separately when
 * it is actually a distinct value in the first place, so this stays the
 * fully portable check on every POSIX system, while never tripping that
 * warning on the one where the two would already be redundant.
 *
 * @param err The @c errno value to check
 *
 * @return @c true when @p err indicates the call would have blocked
 *
 * @note Complexity: @e O(1)
 */
static bool s_errno_is_would_block(int err)
{
#if EAGAIN == EWOULDBLOCK
    return err == EAGAIN;
#else
    return err == EAGAIN || err == EWOULDBLOCK;
#endif
}


/**
 * @brief Accept one pending connection on the listening socket
 *
 * Dropped immediately, with a logged reason, when either accepting it
 * or making it non-blocking fails outright, or when every slot in
 * @c s_clients is already in use.
 *
 * @note Complexity: @e O(n), where @e n is @c IPC_MAX_CLIENTS (the
 *       free-slot scan)
 */
static void s_new_client_accept(void)
{
    int fd = accept(s_ipc_fd, NULL, NULL);
    int flags;
    int slot = -1;

    if (fd < 0) {
        if (!s_errno_is_would_block(errno)) {
            LOGGER_WARNING("Failed to accept an IPC connection: %s",
                    strerror(errno));
        }
        return;
    }

    /* 'accept' does not inherit the listening socket's 'SOCK_NONBLOCK'
     * flag onto the connection it hands back, so every accepted client
     * is made non-blocking here, the POSIX way ('fcntl' / 'F_SETFL' /
     * 'O_NONBLOCK'), rather than the Linux-only 'accept4' shortcut this
     * project's POSIX version target does not cover. */
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        LOGGER_WARNING("Failed to make an accepted IPC connection" \
                " non-blocking: %s", strerror(errno));
        (void) close(fd);
        return;
    }

    for (int i = 0; i < IPC_MAX_CLIENTS; ++i) {
        if (s_clients[i].fd == -1) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        LOGGER_WARNING("IPC client limit (%d) reached;" \
                " dropping a new connection", IPC_MAX_CLIENTS);
        (void) close(fd);
        return;
    }

    s_clients[slot].fd = fd;
    s_clients[slot].buf_len = 0;
}


/**
 * @brief Read whatever is currently available from one connected client
 *        and dispatch every complete line it contains
 *
 * @param wm  Window manager instance, passed through to each dispatched
 *            command
 * @param idx Index into @c s_clients
 *
 * @note Complexity: @e O(n), where @e n is the number of complete lines
 *       found in this one read
 */
static void s_handle_client_data(wm_td *wm, int idx)
{
    struct s_ipc_client_s *const c = &s_clients[idx];
    ssize_t n;

    /* Room for at least one more byte plus the buffer's null terminator
     * is always kept free, so a line that exactly fills the rest of
     * 'buf' is still safe to null-terminate below. */
    n = read(c->fd, c->buf + c->buf_len,
            sizeof(c->buf) - c->buf_len - 1u);

    if (n == 0) {
        s_client_close(idx);
        return;
    }
    if (n < 0) {
        if (!s_errno_is_would_block(errno)) {
            LOGGER_WARNING("IPC client read error: %s", strerror(errno));
            s_client_close(idx);
        }
        return;
    }

    c->buf_len += (size_t) n;
    c->buf[c->buf_len] = '\0';

    /* Every complete ('\n'-terminated) line currently buffered is
     * dispatched in this same call, not just the first one.  A fast
     * client (or one that simply queued several requests before this
     * descriptor was next polled) can have more than one ready at once,
     * and leaving the rest for a future 'poll' wakeup would delay them
     * for no reason. */
    for (;;) {
        char *newline = memchr(c->buf, '\n', c->buf_len);
        char *response;
        size_t line_len;
        size_t remaining;

        if (newline == NULL) {
            break;
        }

        *newline = '\0';
        response = ipc_commands_dispatch(wm, c->buf, idx);
        if (response != NULL) {
            size_t resp_len = safe_strlen(response);
            ssize_t written = write(c->fd, response, resp_len);
            ssize_t nl_written = -1;

            if (written >= 0 && (size_t) written == resp_len) {
                nl_written = write(c->fd, "\n", 1);
            }

            if (written < 0 || (size_t) written != resp_len ||
                    nl_written != 1) {
                /* A short or failed write here means the client either
                 * is not reading its responses or has gone away; either
                 * way, the connection is no longer usable and the
                 * simplest correct response is to drop it rather than
                 * track a partial-write backlog for what is meant to
                 * stay a small local control socket, not
                 * a general-purpose one.  This also covers the response
                 * writing fully but the trailing newline not: a client
                 * that only ever sees a line without its terminator can
                 * never tell the response actually ended there. */
                LOGGER_WARNING("Short write to an IPC client;" \
                        " dropping its connection", L_NARG);
                free(response);
                s_client_close(idx);
                return;
            }
            free(response);
        }

        /* Shift whatever came after this line (the start of the next
         * one, or nothing yet) down to the front of the buffer, so the
         * next loop iteration (or the next 'read' call entirely) sees
         * it at offset 0 again. */
        line_len = (size_t) (newline - c->buf) + 1u;
        remaining = c->buf_len - line_len;
        memmove(c->buf, c->buf + line_len, remaining);
        c->buf_len = remaining;
        c->buf[c->buf_len] = '\0';
    }

    if (c->buf_len >= sizeof(c->buf) - 1u) {
        LOGGER_WARNING("IPC client sent a line longer than" \
                " IPC_MSG_MAX_LENGTH (%d bytes) without a newline;" \
                " dropping its connection", IPC_MSG_MAX_LENGTH);
        s_client_close(idx);
    }
}


/* Initialize the IPC control socket */
int ipc_init(void)
{
    char runtime_dir[CONFIG_MAX_LENGTH_PATH_BASE];
    char tmp_fallback[CONFIG_MAX_LENGTH_PATH_BASE];
    char socket_path[CONFIG_MAX_LENGTH_PATH_BASE];
    struct sockaddr_un addr;
    int fd;

    if (s_ipc_fd != -1) {
        return 0;   /* Already up; not an error */
    }

    if (!s_clients_initialized) {
        for (int i = 0; i < IPC_MAX_CLIENTS; ++i) {
            s_clients[i].fd = -1;
            s_clients[i].buf_len = 0;
            s_clients[i].subscribed_events = 0;
        }
        s_clients_initialized = true;
    }

    (void) snprintf(tmp_fallback, sizeof(tmp_fallback), "%s%u",
            IPC_TMP_FALLBACK_PREFIX, (unsigned int) getuid());

    xdg_resolve_dir(XDG_DIR_RUNTIME, tmp_fallback,
            runtime_dir, sizeof(runtime_dir));

    if (s_runtime_dir_ensure(runtime_dir) != 0) {
        return -1;
    }

    /* Built with 'safe_strncpy'/'safe_strncat' rather than
     * 'snprintf("%s/%s", ...)' on purpose.  Both take the full
     * destination size and truncate safely against it, exactly like
     * 'snprintf' does, but neither is a 'printf'-family call, so
     * neither one gives GCC's '-Wformat-truncation' anything to reason
     * about in the first place.  That checker judges a '%s' argument by
     * its source array's declared capacity, not by what a function like
     * 'xdg_resolve_dir' actually promises to leave in it, so composing
     * same-sized path buffers through it always reads as a possible
     * overflow to the compiler even when it can never really happen;
     * growing the destination past its neighbors only relocates the
     * same mismatch to whichever buffer receives it next (as happened
     * here, into 's_ipc_socket_path' below, previously copied via that
     * same 'snprintf ("%s", ...)' pattern). */
    safe_strncpy(socket_path, runtime_dir, sizeof(socket_path));
    safe_strncat(socket_path, "/", sizeof(socket_path));
    safe_strncat(socket_path, IPC_SOCKET_FILENAME, sizeof(socket_path));

    if (safe_strlen(socket_path) >= sizeof(addr.sun_path)) {
        LOGGER_ERROR("IPC socket path '%s' is too long for" \
                " 'sockaddr_un' (%zu bytes available)",
                socket_path, sizeof(addr.sun_path));
        return -1;
    }

    /* A leftover file from a run that did not shut down cleanly (crash,
     * 'SIGKILL') rather than a second live instance; see this
     * function's comment in 'ipc.h' for why that is the only
     * possibility left by the time this ever runs.
     *
     * 'unlink' failing only because there was nothing there to remove
     * ('ENOENT') is expected and fine.  Any other failure means 'bind'
     * below would fail anyway, so it is caught there instead of
     * duplicating the same check twice. */
    if (unlink(socket_path) != 0 && errno != ENOENT) {
        LOGGER_WARNING("Could not remove existing IPC socket file" \
                " '%s': %s", socket_path, strerror(errno));
    }

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        LOGGER_ERROR("Failed to create IPC socket: %s",
                strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, socket_path, safe_strlen(socket_path) + 1u);

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        LOGGER_ERROR("Failed to bind IPC socket to '%s': %s",
                socket_path, strerror(errno));
        (void) close(fd);
        return -1;
    }

    if (listen(fd, IPC_LISTEN_BACKLOG) != 0) {
        LOGGER_ERROR("Failed to listen on IPC socket '%s': %s",
                socket_path, strerror(errno));
        (void) close(fd);
        (void) unlink(socket_path);
        return -1;
    }

    s_ipc_fd = fd;
    safe_strncpy(s_ipc_socket_path, socket_path,
            sizeof(s_ipc_socket_path));

    LOGGER_INFO("IPC control socket listening at '%s'", socket_path);

    return 0;
}


/* Destroy the IPC control socket */
void ipc_destroy(void)
{
    if (s_ipc_fd == -1) {
        return;
    }

    for (int i = 0; i < IPC_MAX_CLIENTS; ++i) {
        if (s_clients[i].fd != -1) {
            (void) close(s_clients[i].fd);
            s_clients[i].fd = -1;
            s_clients[i].buf_len = 0;
            s_clients[i].subscribed_events = 0;
        }
    }

    (void) close(s_ipc_fd);
    s_ipc_fd = -1;

    if (s_ipc_socket_path[0] != '\0') {
        (void) unlink(s_ipc_socket_path);
        s_ipc_socket_path[0] = '\0';
    }
}


/* Subscribe the given connection to one or more events */
cJSON *ipc_client_subscribe(int client_idx, const cJSON *args)
{
    cJSON *const events = cJSON_GetObjectItem(args, "events");
    cJSON *item;
    cJSON *resp;
    uint32_t requested = 0;

    if (events == NULL || !cJSON_IsArray(events) ||
            cJSON_GetArraySize(events) == 0) {
        resp = cJSON_CreateObject();
        if (resp != NULL) {
            cJSON_AddBoolToObject(resp, "ok", 0);
            cJSON_AddStringToObject(resp, "error",
                    "missing or empty 'events' array");
        }
        return resp;
    }

    cJSON_ArrayForEach(item, events) {
        uint32_t bit;

        if (!cJSON_IsString(item)) {
            resp = cJSON_CreateObject();
            if (resp != NULL) {
                cJSON_AddBoolToObject(resp, "ok", 0);
                cJSON_AddStringToObject(resp, "error",
                        "'events' must be an array of strings");
            }
            return resp;
        }
        bit = s_event_name_to_bit(item->valuestring);
        if (bit == 0) {
            resp = cJSON_CreateObject();
            if (resp != NULL) {
                cJSON_AddBoolToObject(resp, "ok", 0);
                cJSON_AddStringToObject(resp, "error",
                        "unknown event name in 'events'");
            }
            return resp;
        }
        requested |= (uint32_t) bit;
    }

    s_clients[client_idx].subscribed_events |= requested;

    resp = cJSON_CreateObject();
    if (resp != NULL) {
        cJSON_AddBoolToObject(resp, "ok", 1);
    }
    return resp;
}


/* Unsubscribe the given connection from one or more events, or from
 * every event it was subscribed to when 'events' is left out */
cJSON *ipc_client_unsubscribe(int client_idx, const cJSON *args)
{
    cJSON *const events = cJSON_GetObjectItem(args, "events");
    cJSON *resp;

    if (events == NULL) {
        s_clients[client_idx].subscribed_events = 0;
    } else if (!cJSON_IsArray(events)) {
        resp = cJSON_CreateObject();
        if (resp != NULL) {
            cJSON_AddBoolToObject(resp, "ok", 0);
            cJSON_AddStringToObject(resp, "error",
                    "'events' must be an array of strings");
        }
        return resp;
    } else {
        cJSON *item;

        cJSON_ArrayForEach(item, events) {
            uint32_t bit;

            if (!cJSON_IsString(item)) {
                resp = cJSON_CreateObject();
                if (resp != NULL) {
                    cJSON_AddBoolToObject(resp, "ok", 0);
                    cJSON_AddStringToObject(resp, "error",
                            "'events' must be an array of strings");
                }
                return resp;
            }
            bit = s_event_name_to_bit(item->valuestring);
            /* An unrecognized name here is not an error the way it is
             * for 'subscribe': the caller could not have been
             * subscribed to it in the first place, so there is nothing
             * to undo, the same as unsubscribing from an event never
             * subscribed to at all is not an error either. */
            s_clients[client_idx].subscribed_events &= ~(uint32_t) bit;
        }
    }

    resp = cJSON_CreateObject();
    if (resp != NULL) {
        cJSON_AddBoolToObject(resp, "ok", 1);
    }
    return resp;
}


/* Send one event line to every currently subscribed client */
void ipc_broadcast_event(uint32_t type, cJSON *fields)
{
    const char *name = s_event_bit_to_name(type);
    cJSON *envelope;
    char *line;
    size_t len;

    if (s_ipc_fd == -1 || name == NULL) {
        cJSON_Delete(fields);
        return;
    }

    envelope = (fields != NULL) ? fields : cJSON_CreateObject();
    if (envelope == NULL) {
        return;
    }
    cJSON_AddStringToObject(envelope, "event", name);

    line = cJSON_PrintUnformatted(envelope);
    cJSON_Delete(envelope);
    if (line == NULL) {
        return;
    }
    len = safe_strlen(line);

    for (int i = 0; i < IPC_MAX_CLIENTS; ++i) {
        if (s_clients[i].fd == -1 ||
                (s_clients[i].subscribed_events & (uint32_t) type) == 0) {
            continue;
        }
        if (write(s_clients[i].fd, line, len) != (ssize_t) len ||
                write(s_clients[i].fd, "\n", 1) != 1) {
            s_client_close(i);
        }
    }

    free(line);
}


/* List every file descriptor the IPC subsystem currently wants
 * polled */
int ipc_poll_fds(int *out_fds, int max)
{
    int count = 0;

    if (s_ipc_fd == -1 || out_fds == NULL || max <= 0) {
        return 0;
    }

    out_fds[count++] = s_ipc_fd;

    for (int i = 0; i < IPC_MAX_CLIENTS && count < max; ++i) {
        if (s_clients[i].fd != -1) {
            out_fds[count++] = s_clients[i].fd;
        }
    }

    return count;
}


/* Handle a single IPC-related descriptor a poll call reported as
 * readable */
void ipc_handle_readable(wm_td *wm, int fd)
{
    if (s_ipc_fd == -1) {
        return;
    }

    if (fd == s_ipc_fd) {
        s_new_client_accept();
        return;
    }

    for (int i = 0; i < IPC_MAX_CLIENTS; ++i) {
        if (s_clients[i].fd == fd) {
            s_handle_client_data(wm, i);
            return;
        }
    }
}
