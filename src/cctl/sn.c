/**
 * @file cctl/sn.c
 *
 * @brief freedesktop.org Startup Notification protocol implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf, NULL, size_t */
#include <stdlib.h>     /* free */
#include <string.h>     /* memset, memcpy, strchr, strstr */
#include <time.h>       /* CLOCK_MONOTONIC, clock_gettime, time */
#include <unistd.h>     /* getpid */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Default initial values */
#include <defs/cursor.h>

/* Utils includes */
#include <utils/cursor.h>
#include <utils/safe/safestr.h>
#include <utils/xcb/atom.h>

/* Project includes */
#include <logger.h>
#include <stage.h>

/* Local includes */
#include <cctl/sn.h>


/**
 * @brief One launch sequence this window manager started and is
 *        waiting on
 */
typedef struct {
    char id[SN_ID_MAX_LEN];
    struct timespec started_at;
    uint32_t desktop_id;
    pid_t pid;              /**< 0 until 'cctl_sn_associate_pid' sets
                                 it; the process this sequence
                                 actually launched */
} s_pending_td;


/**
 * @brief One in-progress reassembly of an incoming chunked text message
 */
typedef struct {
    size_t len;
    xcb_window_t window;
    bool in_use;
    char buf[SN_MSG_MAX_LEN];
    struct timespec updated_at;
} s_reassembly_td;


static s_pending_td s_pending[SN_MAX_PENDING];
static uint8_t s_pending_count = 0;
static s_reassembly_td s_reassembly[SN_MAX_REASSEMBLY];
static uint32_t s_id_counter = 0;
static uint32_t s_timeout_seconds = SN_TIMEOUT_SECONDS;
static bool s_cursor_busy = false;
static xcb_atom_t s_atom_begin = XCB_ATOM_NONE;
static xcb_atom_t s_atom_info = XCB_ATOM_NONE;
static xcb_atom_t s_atom_startup_id = XCB_ATOM_NONE;
static xcb_atom_t s_atom_utf8_string = XCB_ATOM_NONE;
static xcb_atom_t s_atom_wm_pid = XCB_ATOM_NONE;


/**
 * @brief Show or restore the busy (watch) cursor on every managed root
 *        window
 *
 * Loaded from the active cursor theme via @a util_cursor_load (same as
 * every other cursor in the project), falling back to the X core cursor
 * font automatically if the theme has no "watch"/"left_ptr" cursor.
 * The theme lookup itself is tied to one screen, but the resulting
 * cursor resource is valid to apply to every root window on the same
 * connection, so only the first managed stage's screen is used to
 * build it.
 *
 * @param connection XCB connection
 * @param stages     Managed stages, one root window per screen
 * @param busy       @c true to show the busy cursor, @c false to
 *                   restore the default one
 *
 * @note Complexity: @e O(s), where @e s is the number of managed
 *       stages
 */
static void s_set_busy_cursor(xcb_connection_t *connection,
        list_td *stages, bool busy)
{
    stage_td *first_stage = NULL;
    util_cursor_ctx_td *ctx;
    xcb_cursor_t cursor;
    uint32_t value;

    if (connection == NULL || stages == NULL) {
        return;
    }

    for (list_item_td *node = list_head(stages); node != NULL;
            node = list_next(node)) {
        stage_td *stage = (stage_td *) list_data(node);

        if (stage != NULL && stage->screen != NULL) {
            first_stage = stage;
            break;
        }
    }
    if (first_stage == NULL) {
        return;
    }

    ctx = util_cursor_ctx_new(connection, first_stage->screen);
    cursor = (busy)
        ? util_cursor_load(ctx, "watch", WM_CURSOR_WATCH_GLYPH)
        : util_cursor_load(ctx, "left_ptr", WM_CURSOR_LEFT_PTR_GLYPH);
    util_cursor_ctx_free(ctx);

    if (cursor == XCB_NONE) {
        return;
    }

    for (list_item_td *node = list_head(stages); node != NULL;
            node = list_next(node)) {
        stage_td *stage = (stage_td *) list_data(node);

        if (stage == NULL || stage->screen == NULL) {
            continue;
        }

        value = cursor;
        xcb_change_window_attributes(connection, stage->screen->root,
                XCB_CW_CURSOR, &value);
    }

    xcb_free_cursor(connection, cursor);
    xcb_flush(connection);
    s_cursor_busy = busy;
}


/**
 * @brief Broadcast one startup-notification text message, splitting it
 *        into @c SN_CHUNK_LEN byte format-8 'ClientMessage' chunks per
 *        the protocol
 *
 * The first chunk uses @c _NET_STARTUP_INFO_BEGIN as its message type.
 * Every following chunk (if the text does not fit in one) uses
 * @c _NET_STARTUP_INFO instead, exactly as @c libstartup-notification
 * does, so any conforming listener can reassemble it the same way
 * @a cctl_sn_handle_client_message does on the receiving end.
 *
 * @param connection XCB connection
 * @param stages     Managed stages, one root window per screen
 * @param text       Null-terminated ASCII message text, e.g.,
 *                   @c ("new: ID=\"...\"")
 *
 * @note Complexity: @e O(s * m), where @e s is the number of managed
 *       stages and @e m is the number of chunks @p text splits into
 */
static void s_broadcast(xcb_connection_t *connection, list_td *stages,
        const char *text)
{
    size_t text_len;
    size_t sent;
    bool first_chunk;

    if (connection == NULL || stages == NULL || text == NULL ||
            s_atom_begin == XCB_ATOM_NONE ||
            s_atom_info == XCB_ATOM_NONE) {
        return;
    }

    /* Including the terminating null lets the receiver recognize the
     * message's end the same way libstartup-notification does.  A chunk
     * that does not completely fill the 20-byte payload marks the
     * message as complete. */
    text_len = safe_strlen(text) + 1u;

    for (list_item_td *node = list_head(stages); node != NULL;
            node = list_next(node)) {
        stage_td *const stage = (stage_td *) list_data(node);

        if (stage == NULL || stage->screen == NULL) {
            continue;
        }

        sent = 0u;
        first_chunk = true;
        while (sent < text_len) {
            xcb_client_message_event_t ev;
            size_t chunk_len = text_len - sent;

            if (chunk_len > SN_CHUNK_LEN) {
                chunk_len = SN_CHUNK_LEN;
            }

            memset(&ev, 0, sizeof(ev));
            ev.response_type = XCB_CLIENT_MESSAGE;
            ev.format = 8;
            ev.window = stage->screen->root;
            ev.type = (first_chunk) ? s_atom_begin : s_atom_info;
            memcpy(ev.data.data8, text + sent, chunk_len);

            xcb_send_event(connection, 0, stage->screen->root,
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char *) &ev);

            sent += chunk_len;
            first_chunk = false;
        }
    }

    xcb_flush(connection);
}


/**
 * @brief Find (or start) the reassembly slot for @p window
 *
 * @param window Source window the chunks are arriving from
 *
 * @return Pointer to that window's reassembly slot, or @c NULL if every
 *         slot is already in use by a different window
 *
 * @note Complexity: @e O(r), where @e r is @c SN_MAX_REASSEMBLY
 */
static s_reassembly_td *s_reassembly_for(xcb_window_t window)
{
    s_reassembly_td *free_slot = NULL;
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        now.tv_sec = 0;
        now.tv_nsec = 0;
    }

    for (uint8_t i = 0u; i < SN_MAX_REASSEMBLY; ++i) {
        if (s_reassembly[i].in_use && s_reassembly[i].window == window) {
            s_reassembly[i].updated_at = now;
            return &s_reassembly[i];
        }
        if (!s_reassembly[i].in_use && free_slot == NULL) {
            free_slot = &s_reassembly[i];
        }
    }

    if (free_slot != NULL) {
        free_slot->in_use = true;
        free_slot->window = window;
        free_slot->len = 0u;
        free_slot->buf[0] = '\0';
        free_slot->updated_at = now;
    }

    return free_slot;
}


/**
 * @brief Remove pending sequence @p i outright, clearing the busy
 *        cursor once none remain
 *
 * @param connection XCB connection
 * @param stages     Every managed stage, to clear the busy cursor on
 * @param i          Index into @c s_pending to remove
 * @param reason     Logged alongside the sequence's own ID, past
 *                   tense (e.g., @c "completed by application")
 *
 * @note Complexity: @e O(1)
 */
static void s_complete_at(xcb_connection_t *connection,
        list_td *stages, uint8_t i, const char *restrict reason)
{
    LOGGER_DEBUG("Startup-notification sequence '%s' %s",
            s_pending[i].id, reason);
    s_pending[i] = s_pending[s_pending_count - 1u];
    --s_pending_count;
    if (s_pending_count == 0u && s_cursor_busy) {
        s_set_busy_cursor(connection, stages, false);
    }
}


/**
 * @brief Remove a pending sequence by ID, if still pending, and restore
 *        the normal cursor once none remain
 *
 * @param connection XCB connection
 * @param stages     Managed stages, one root window per screen
 * @param id         Startup ID to remove
 * @param reason     Short reason logged at debug level (e.g.,
 *                   "completed by application", "canceled")
 *
 * @note Complexity: @e O(p), where @e p is the number of currently
 *       pending sequences
 */
static void s_complete_by_id(xcb_connection_t *connection,
        list_td *stages, const char *restrict id,
        const char *restrict reason)
{
    for (uint8_t i = 0u; i < s_pending_count; ++i) {
        if (safe_strcmp(s_pending[i].id, id) == 0) {
            s_complete_at(connection, stages, i, reason);
            return;
        }
    }
}


/**
 * @brief Complete the pending sequence named by an incoming
 *        @c ("remove:" message), if one is still pending
 *
 * @param connection XCB connection
 * @param stages     Managed stages, one root window per screen
 * @param message    Complete, reassembled message text
 *
 * @note Complexity: @e O(p), where @e p is the number of currently
 *       pending sequences
 */
static void s_handle_complete_message(xcb_connection_t *connection,
        list_td *stages, const char *message)
{
    const char *id_start;
    const char *id_end;
    char id[SN_ID_MAX_LEN];
    size_t id_len;

    if (safe_strncmp(message, "remove:", 7u) != 0) {
        return;
    }

    id_start = strstr(message, "ID=\"");
    if (id_start == NULL) {
        return;
    }
    id_start += 4;

    id_end = strchr(id_start, '"');
    if (id_end == NULL || id_end <= id_start) {
        return;
    }

    id_len = (size_t) (id_end - id_start);
    if (id_len >= sizeof(id)) {
        id_len = sizeof(id) - 1u;
    }
    memcpy(id, id_start, id_len);
    id[id_len] = '\0';

    s_complete_by_id(connection, stages, id,
            "completed by application");
}


/* Begin a startup-notification sequence for a launched process */
bool cctl_sn_begin(xcb_connection_t *connection, list_td *stages,
        const char *restrict name, uint32_t origin_desktop,
        char *restrict out_id, size_t out_id_size)
{
    char id[SN_ID_MAX_LEN];
    char message[SN_MSG_MAX_LEN];
    struct timespec now;

    if (connection == NULL || stages == NULL || out_id == NULL ||
            out_id_size == 0u) {
        return false;
    }

    if (s_atom_begin == XCB_ATOM_NONE) {
        s_atom_begin = atom_intern(connection,
                "_NET_STARTUP_INFO_BEGIN", false);
    }
    if (s_atom_info == XCB_ATOM_NONE) {
        s_atom_info = atom_intern(connection,
                "_NET_STARTUP_INFO", false);
    }
    if (s_atom_begin == XCB_ATOM_NONE || s_atom_info == XCB_ATOM_NONE) {
        return false;
    }

    ++s_id_counter;
    (void) snprintf(id, sizeof(id), "icowm-%ld-%u_TIME%llu",
            (long) getpid(), s_id_counter,
            (unsigned long long) time(NULL));

    if (safe_strlen(id) >= out_id_size) {
        return false;
    }
    (void) safe_strncpy(out_id, id, out_id_size - 1u);
    out_id[out_id_size - 1u] = '\0';

    (void) snprintf(message, sizeof(message),
            "new: ID=\"%s\" NAME=\"%s\" SCREEN=0", id,
            (name != NULL) ? name : "");
    s_broadcast(connection, stages, message);

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        now.tv_sec = 0;
        now.tv_nsec = 0;
    }

    if (s_pending_count < SN_MAX_PENDING) {
        (void) safe_strncpy(s_pending[s_pending_count].id, id,
                sizeof(s_pending[s_pending_count].id) - 1u);
        s_pending[s_pending_count].id[
            sizeof(s_pending[s_pending_count].id) - 1u] = '\0';
        s_pending[s_pending_count].started_at = now;
        s_pending[s_pending_count].desktop_id = origin_desktop;
        s_pending[s_pending_count].pid = 0;
        ++s_pending_count;
    }

    LOGGER_DEBUG("Startup-notification sequence '%s' begun for '%s'",
            id, (name != NULL) ? name : "");

    if (!s_cursor_busy) {
        s_set_busy_cursor(connection, stages, true);
    }

    return true;
}


/* Record the PID of the process a pending sequence actually launched */
void cctl_sn_associate_pid(const char *id, pid_t pid)
{
    if (id == NULL) {
        return;
    }

    for (uint8_t i = 0u; i < s_pending_count; ++i) {
        if (safe_strcmp(s_pending[i].id, id) == 0) {
            s_pending[i].pid = pid;
            return;
        }
    }
}


/* Look up the desktop a mapped window's startup sequence began on */
bool cctl_sn_desktop_for_window(xcb_connection_t *connection,
        xcb_window_t window, uint32_t *out_desktop)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    char id[SN_ID_MAX_LEN];
    size_t id_len;

    if (connection == NULL || out_desktop == NULL) {
        return false;
    }

    if (s_atom_startup_id == XCB_ATOM_NONE) {
        s_atom_startup_id = atom_intern(connection,
                "_NET_STARTUP_ID", true);
    }
    if (s_atom_utf8_string == XCB_ATOM_NONE) {
        s_atom_utf8_string = atom_intern(connection,
                "UTF8_STRING", false);
    }
    if (s_atom_startup_id == XCB_ATOM_NONE ||
            s_atom_utf8_string == XCB_ATOM_NONE) {
        return false;
    }

    cookie = xcb_get_property(connection, 0, window, s_atom_startup_id,
            s_atom_utf8_string, 0, SN_ID_MAX_LEN - 1u);
    reply = xcb_get_property_reply(connection, cookie, NULL);
    if (reply == NULL) {
        return false;
    }
    if (reply->value_len == 0u) {
        free(reply);
        return false;
    }

    id_len = ((size_t) reply->value_len < sizeof(id) - 1u)
        ? (size_t) reply->value_len : sizeof(id) - 1u;
    memcpy(id, xcb_get_property_value(reply), id_len);
    id[id_len] = '\0';
    free(reply);

    for (uint8_t i = 0u; i < s_pending_count; ++i) {
        if (safe_strcmp(s_pending[i].id, id) == 0) {
            *out_desktop = s_pending[i].desktop_id;
            return true;
        }
    }

    return false;
}


/* End the pending sequence, if any, whose PID matches a newly mapped
 * window's own '_NET_WM_PID' */
bool cctl_sn_complete_for_pid(xcb_connection_t *connection,
        list_td *stages, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    pid_t window_pid;

    if (connection == NULL || stages == NULL ||
            window == XCB_WINDOW_NONE || s_pending_count == 0u) {
        return false;
    }

    if (s_atom_wm_pid == XCB_ATOM_NONE) {
        s_atom_wm_pid = atom_intern(connection, "_NET_WM_PID", false);
    }
    if (s_atom_wm_pid == XCB_ATOM_NONE) {
        return false;
    }

    cookie = xcb_get_property(connection, 0, window, s_atom_wm_pid,
            XCB_ATOM_CARDINAL, 0, 1);
    reply = xcb_get_property_reply(connection, cookie, NULL);
    if (reply == NULL) {
        return false;
    }
    if (reply->value_len == 0u || reply->format != 32u) {
        free(reply);
        return false;
    }

    window_pid = (pid_t) *(uint32_t *) xcb_get_property_value(reply);
    free(reply);

    for (uint8_t i = 0u; i < s_pending_count; ++i) {
        if (s_pending[i].pid != 0 && s_pending[i].pid == window_pid) {
            s_complete_at(connection, stages, i,
                    "completed by matching _NET_WM_PID");
            return true;
        }
    }

    return false;
}


/* Handle an incoming startup-notification 'ClientMessage' */
void cctl_sn_handle_client_message(xcb_connection_t *connection,
        list_td *stages, const xcb_client_message_event_t *event)
{
    s_reassembly_td *slot;
    size_t chunk_len;
    size_t copy_len;
    bool complete;

    if (connection == NULL || stages == NULL || event == NULL ||
            event->format != 8) {
        return;
    }
    if (event->type != s_atom_begin && event->type != s_atom_info) {
        return;
    }

    slot = s_reassembly_for(event->window);
    if (slot == NULL) {
        return;
    }

    chunk_len = safe_strnlen((const char *) event->data.data8,
            SN_CHUNK_LEN);
    complete = chunk_len < SN_CHUNK_LEN;

    copy_len = chunk_len;
    if (slot->len + copy_len >= sizeof(slot->buf)) {
        copy_len = sizeof(slot->buf) - 1u - slot->len;
    }
    if (copy_len > 0u) {
        memcpy(slot->buf + slot->len, event->data.data8, copy_len);
        slot->len += copy_len;
    }
    slot->buf[slot->len] = '\0';

    if (complete) {
        s_handle_complete_message(connection, stages, slot->buf);
        slot->in_use = false;
        slot->len = 0u;
    }
}


/* Milliseconds until the next pending sequence times out */
int cctl_sn_ms_remaining(void)
{
    struct timespec now;
    int64_t soonest_ms = -1;

    if (s_pending_count == 0u) {
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }

    for (uint8_t i = 0u; i < s_pending_count; ++i) {
        int64_t elapsed_ms =
            ((int64_t) now.tv_sec -
                (int64_t) s_pending[i].started_at.tv_sec) * 1000 +
            ((int64_t) now.tv_nsec -
                (int64_t) s_pending[i].started_at.tv_nsec) / 1000000;
        int64_t remaining_ms =
            ((int64_t) s_timeout_seconds * 1000) - elapsed_ms;

        if (remaining_ms < 0) {
            remaining_ms = 0;
        }
        if (soonest_ms < 0 || remaining_ms < soonest_ms) {
            soonest_ms = remaining_ms;
        }
    }

    return (int) soonest_ms;
}


/* Expire any pending sequence whose timeout has elapsed, and recycle
 * any reassembly slot that has sat idle past
 * 'SN_REASSEMBLY_TIMEOUT_MS' */
void cctl_sn_tick(xcb_connection_t *connection, list_td *stages)
{
    struct timespec now;
    uint8_t i;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return;
    }

    for (i = 0u; i < SN_MAX_REASSEMBLY; ++i) {
        int64_t idle_ms;

        if (!s_reassembly[i].in_use) {
            continue;
        }

        idle_ms =
            ((int64_t) now.tv_sec -
                (int64_t) s_reassembly[i].updated_at.tv_sec) * 1000 +
            ((int64_t) now.tv_nsec -
                (int64_t) s_reassembly[i].updated_at.tv_nsec) / 1000000;

        if (idle_ms >= (int64_t) SN_REASSEMBLY_TIMEOUT_MS) {
            LOGGER_DEBUG("Startup-notification reassembly for" \
                    " window 0x%x abandoned; slot recycled",
                    s_reassembly[i].window);
            s_reassembly[i].in_use = false;
            s_reassembly[i].len = 0u;
        }
    }

    if (s_pending_count == 0u || connection == NULL ||
            stages == NULL) {
        return;
    }

    i = 0u;
    while (i < s_pending_count) {
        int64_t elapsed_ms =
            ((int64_t) now.tv_sec -
                (int64_t) s_pending[i].started_at.tv_sec) * 1000 +
            ((int64_t) now.tv_nsec -
                (int64_t) s_pending[i].started_at.tv_nsec) / 1000000;

        if (elapsed_ms >= (int64_t) s_timeout_seconds * 1000) {
            LOGGER_DEBUG("Startup-notification sequence '%s' timed out",
                    s_pending[i].id);
            s_pending[i] = s_pending[s_pending_count - 1u];
            --s_pending_count;
        } else {
            ++i;
        }
    }

    if (s_pending_count == 0u && s_cursor_busy) {
        s_set_busy_cursor(connection, stages, false);
    }
}


/* Cancel a pending sequence immediately */
void cctl_sn_cancel(xcb_connection_t *connection, list_td *stages,
        const char *id)
{
    if (connection == NULL || stages == NULL || id == NULL) {
        return;
    }

    s_complete_by_id(connection, stages, id, "canceled");
}


/* Override how many seconds a sequence waits before being expired */
void cctl_sn_set_timeout_seconds(uint32_t seconds)
{
    if (seconds == 0u) {
        return;
    }

    s_timeout_seconds = seconds;
}
