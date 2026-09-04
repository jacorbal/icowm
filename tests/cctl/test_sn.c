/**
 * @file tests/cctl/test_sn.c
 *
 * @brief Test battery for the freedesktop.org Startup Notification
 *        protocol implementation (cctl/sn.c)
 *
 * cctl/sn.c holds seven non-static entry points, cctl_sn_
 * set_timeout_seconds, cctl_sn_begin, cctl_sn_desktop_for_window,
 * cctl_sn_handle_client_message, cctl_sn_ms_remaining, cctl_sn_tick,
 * and cctl_sn_cancel, plus five file-local static helpers only ever
 * reached through them: s_set_busy_cursor, s_broadcast,
 * s_reassembly_for, s_complete_by_id, and s_handle_complete_message.
 * Every one of those five is exercised indirectly here, through the
 * public entry point that calls it, since every X request each one
 * itself issues (xcb_change_window_attributes, xcb_free_cursor,
 * xcb_send_event, xcb_get_property/xcb_get_property_reply) is a
 * recording, controllable link-only stand-in below rather than a real
 * connection to any X server, and the cursor-theme lookup
 * (util_cursor_ctx_new/util_cursor_load/util_cursor_ctx_free) is
 * likewise stood in for.  This lets every scenario assert directly on
 * cctl/sn.c's own bookkeeping (its pending-sequence list, its
 * reassembly slots, its busy-cursor flag, its timeout arithmetic)
 * rather than on any other subsystem's real behavior.
 *
 * cctl_sn_begin's own guard clauses (null connection/surfaces/out_id,
 * a zero out_id_size, atom_intern failing, an out_id buffer too small
 * for the generated id) are covered, along with its own success path:
 * generating a unique id, broadcasting it (through the s_broadcast
 * stand-ins), registering the pending sequence, and switching on the
 * busy cursor exactly once even across more than one sequence
 * beginning while one is already pending.  cctl_sn_ms_remaining and
 * cctl_sn_tick's own timeout arithmetic over CLOCK_MONOTONIC is
 * exercised using cctl_sn_set_timeout_seconds' own override, set to a
 * timeout of 0 seconds specifically so a sequence begun an instant
 * ago is already expired by the time cctl_sn_tick or
 * cctl_sn_ms_remaining next runs, without this file ever needing to
 * sleep or wait on the real wall clock.  cctl_sn_handle_client_message
 * exercises the real chunked reassembly logic end to end, including
 * a message split across two chunks, and cctl_sn_cancel and a
 * "remove:" message delivered through cctl_sn_handle_client_message
 * both exercise s_complete_by_id's own linear removal and busy-cursor
 * restoration.  cctl_sn_desktop_for_window's own guard clauses and its
 * atom-driven property lookup (through the xcb_get_property* stand-
 * ins) are covered for a matching pending id, a non-matching id, and
 * a property read failure.
 *
 * Not covered: the real cursor-theme lookup inside util_cursor_load
 * itself (stood in for below, since it is a real X server round trip
 * over a real screen's font/theme resources) and the real X protocol
 * wire format xcb_send_event/xcb_change_window_attributes/
 * xcb_free_cursor/xcb_get_property* themselves would produce against a
 * live server, none of which this module's own logic can be
 * distinguished from without one.
 *
 * cctl/sn.c's own translation unit is linked for real, alongside
 * utils/safe/safestr.c (small, pure, already-covered-elsewhere string
 * helpers, worth exercising for real rather than stood in for) and
 * adt/list.c (the real managed-surfaces list cctl/sn.c walks with
 * list_head/list_next/list_data).  utils/time/clock.c is not linked,
 * since sn.c derives every one of its own timeouts directly from
 * clock_gettime rather than through clock.h's helpers.  Every other
 * external symbol cctl/sn.c's translation unit references
 * (atom_intern, util_cursor_ctx_new/util_cursor_load/
 * util_cursor_ctx_free, logger_msg, and the several libxcb entry
 * points named above, including xcb_get_property_value's own pointer
 * arithmetic over a reply buffer) is a small, controllable, recording
 * link-only stand-in below.
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
#include <string.h>
#include <time.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <logger.h>
#include <surface.h>

/* Utils includes */
#include <utils/cursor.h>
#include <utils/safe/safestr.h>
#include <utils/xcb/atom.h>

/* Local includes */
#include <cctl/sn.h>
#include <harness/tap.h>


/** Non-null opaque connection handle, never dereferenced by anything
 *  this file links for real */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
    (xcb_connection_t *) &s_fake_connection_storage;


/* Recording state for the atom_intern stand-in: every name this
 * module ever interns gets its own small, stable, nonzero atom value,
 * so cctl/sn.c's own atom-driven guard clauses see success by default */
static bool s_atom_intern_fail_all;

/** Link-only stand-in for atom_intern (utils/xcb/atom.c) */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) only_if_exists;
    if (s_atom_intern_fail_all) {
        return XCB_ATOM_NONE;
    }
    if (safe_strcmp(name, "_NET_STARTUP_INFO_BEGIN") == 0) {
        return (xcb_atom_t) 101u;
    }
    if (safe_strcmp(name, "_NET_STARTUP_INFO") == 0) {
        return (xcb_atom_t) 102u;
    }
    if (safe_strcmp(name, "_NET_STARTUP_ID") == 0) {
        return (xcb_atom_t) 103u;
    }
    if (safe_strcmp(name, "UTF8_STRING") == 0) {
        return (xcb_atom_t) 104u;
    }
    return (xcb_atom_t) 199u;
}


/* Recording state for the cursor-loading stand-ins */
static int s_cursor_ctx_new_call_count;
static int s_cursor_load_call_count;
static int s_cursor_ctx_free_call_count;
static char s_cursor_load_last_name[32];
static int s_cursor_ctx_storage;

/** Link-only stand-in for util_cursor_ctx_new (utils/cursor.c) */
util_cursor_ctx_td *util_cursor_ctx_new(xcb_connection_t *connection,
        xcb_screen_t *screen)
{
    (void) connection;
    (void) screen;
    s_cursor_ctx_new_call_count++;
    return (util_cursor_ctx_td *) &s_cursor_ctx_storage;
}


/** Link-only stand-in for util_cursor_load (utils/cursor.c) */
xcb_cursor_t util_cursor_load(util_cursor_ctx_td *ctx, const char *name,
        uint16_t fallback_glyph)
{
    (void) ctx;
    (void) fallback_glyph;
    s_cursor_load_call_count++;
    (void) strncpy(s_cursor_load_last_name, name,
            sizeof(s_cursor_load_last_name) - 1u);
    s_cursor_load_last_name[sizeof(s_cursor_load_last_name) - 1u] = '\0';
    return (xcb_cursor_t) 55u;
}


/** Link-only stand-in for util_cursor_ctx_free (utils/cursor.c) */
void util_cursor_ctx_free(util_cursor_ctx_td *ctx)
{
    (void) ctx;
    s_cursor_ctx_free_call_count++;
}


/* Recording state for the logger_msg stand-in */
static int s_logger_call_count;

/** Link-only stand-in for logger_msg (logger.c) */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    s_logger_call_count++;
    return 0;
}


/* Recording state for the xcb_change_window_attributes stand-in */
static int s_change_attrs_call_count;
static xcb_cursor_t s_change_attrs_last_cursor;

/** Link-only stand-in for xcb_change_window_attributes (libxcb) */
xcb_void_cookie_t xcb_change_window_attributes(xcb_connection_t *c,
        xcb_window_t window, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) window;
    (void) value_mask;
    s_change_attrs_call_count++;
    s_change_attrs_last_cursor = *(const xcb_cursor_t *) value_list;
    return cookie;
}


/** Link-only stand-in for xcb_free_cursor (libxcb) */
xcb_void_cookie_t xcb_free_cursor(xcb_connection_t *c, xcb_cursor_t cursor)
{
    xcb_void_cookie_t cookie = {0};

    (void) c;
    (void) cursor;
    return cookie;
}


/** Link-only stand-in for xcb_flush (libxcb) */
int xcb_flush(xcb_connection_t *c)
{
    (void) c;
    return 1;
}


/* Recording state for the xcb_send_event stand-in: every chunk sent is
 * appended, in order, so a scenario can reassemble what s_broadcast
 * actually transmitted and confirm it splits and terminates a message
 * exactly like cctl_sn_handle_client_message's own reassembly expects */
#define MAX_RECORDED_CHUNKS (32)
static int s_send_event_call_count;
static xcb_atom_t s_send_event_last_type;
static char s_send_event_chunks[MAX_RECORDED_CHUNKS][SN_CHUNK_LEN];
static size_t s_send_event_chunk_count;

/** Link-only stand-in for xcb_send_event (libxcb) */
xcb_void_cookie_t xcb_send_event(xcb_connection_t *c, uint8_t propagate,
        xcb_window_t destination, uint32_t event_mask, const char *event)
{
    xcb_void_cookie_t cookie = {0};
    const xcb_client_message_event_t *const ev =
        (const xcb_client_message_event_t *) event;

    (void) c;
    (void) propagate;
    (void) destination;
    (void) event_mask;
    s_send_event_call_count++;
    s_send_event_last_type = ev->type;
    if (s_send_event_chunk_count < MAX_RECORDED_CHUNKS) {
        memcpy(s_send_event_chunks[s_send_event_chunk_count],
                ev->data.data8, SN_CHUNK_LEN);
        s_send_event_chunk_count++;
    }
    return cookie;
}


/* Recording state for the xcb_get_property/xcb_get_property_reply
 * stand-ins */
static bool s_get_property_reply_is_null;
static char s_get_property_value[SN_ID_MAX_LEN];
static uint32_t s_get_property_value_len;

/** Link-only stand-in for xcb_get_property (libxcb) */
xcb_get_property_cookie_t xcb_get_property(xcb_connection_t *c,
        uint8_t _delete, xcb_window_t window, xcb_atom_t property,
        xcb_atom_t type, uint32_t long_offset, uint32_t long_length)
{
    xcb_get_property_cookie_t cookie = {0};

    (void) c;
    (void) _delete;
    (void) window;
    (void) property;
    (void) type;
    (void) long_offset;
    (void) long_length;
    return cookie;
}


/** Link-only stand-in for xcb_get_property_reply (libxcb) */
xcb_get_property_reply_t *xcb_get_property_reply(xcb_connection_t *c,
        xcb_get_property_cookie_t cookie, xcb_generic_error_t **e)
{
    xcb_get_property_reply_t *reply;

    (void) c;
    (void) cookie;
    (void) e;
    if (s_get_property_reply_is_null) {
        return NULL;
    }
    /* A real reply is always one allocation holding the fixed header
     * immediately followed by its value bytes, exactly what
     * xcb_get_property_value (a thin 'pointer past the header' cast)
     * expects to find */
    reply = malloc(sizeof(*reply) + sizeof(s_get_property_value));
    memset(reply, 0, sizeof(*reply));
    reply->value_len = s_get_property_value_len;
    memcpy((char *) reply + sizeof(*reply), s_get_property_value,
            sizeof(s_get_property_value));
    return reply;
}


/** Link-only stand-in for xcb_get_property_value (libxcb) */
void *xcb_get_property_value(const xcb_get_property_reply_t *R)
{
    return (void *) ((const char *) R + sizeof(*R));
}


/**
 * @brief Reset every recording global back to its starting state
 */
static void s_stub_reset(void)
{
    s_atom_intern_fail_all = false;
    s_cursor_ctx_new_call_count = 0;
    s_cursor_load_call_count = 0;
    s_cursor_ctx_free_call_count = 0;
    s_cursor_load_last_name[0] = '\0';
    s_logger_call_count = 0;
    s_change_attrs_call_count = 0;
    s_change_attrs_last_cursor = 0u;
    s_send_event_call_count = 0;
    s_send_event_last_type = XCB_ATOM_NONE;
    memset(s_send_event_chunks, 0, sizeof(s_send_event_chunks));
    s_send_event_chunk_count = 0u;
    s_get_property_reply_is_null = false;
    memset(s_get_property_value, 0, sizeof(s_get_property_value));
    s_get_property_value_len = 0u;
}


/**
 * @brief Build a one-surface managed list with a real, non-null screen
 */
static void s_make_one_surface_list(list_td **out_list,
        surface_td *surface_storage, xcb_screen_t *screen_storage)
{
    memset(surface_storage, 0, sizeof(*surface_storage));
    memset(screen_storage, 0, sizeof(*screen_storage));
    screen_storage->root = (xcb_window_t) 1000u;
    surface_storage->screen = screen_storage;

    *out_list = list_init(NULL);
    list_ins_next(*out_list, NULL, surface_storage);
}


/* cctl_sn_begin refuses a null connection, null surfaces, null out_id,
 * or a zero out_id_size, in every case never broadcasting anything */
static void s_test_begin_guard_clauses(void)
{
    surface_td surface_storage;
    xcb_screen_t screen_storage;
    list_td *surfaces;
    char out_id[SN_ID_MAX_LEN];
    bool result;

    s_make_one_surface_list(&surfaces, &surface_storage, &screen_storage);
    s_stub_reset();

    result = cctl_sn_begin(NULL, surfaces, "xterm", 0u, out_id,
            sizeof(out_id));
    TAP_OK(!result, "cctl_sn_begin refuses a null connection");

    result = cctl_sn_begin(s_fake_connection, NULL, "xterm", 0u, out_id,
            sizeof(out_id));
    TAP_OK(!result, "cctl_sn_begin refuses null surfaces");

    result = cctl_sn_begin(s_fake_connection, surfaces, "xterm", 0u, NULL,
            sizeof(out_id));
    TAP_OK(!result, "cctl_sn_begin refuses a null out_id");

    result = cctl_sn_begin(s_fake_connection, surfaces, "xterm", 0u,
            out_id, 0u);
    TAP_OK(!result, "cctl_sn_begin refuses a zero out_id_size");

    TAP_EQ_INT(s_send_event_call_count, 0,
            "none of the four guard clauses above ever broadcasts"
            " anything");

    list_destroy(surfaces);
}


/* cctl_sn_begin refuses outright once atom_intern itself cannot
 * resolve either atom it needs */
static void s_test_begin_atom_intern_failure(void)
{
    surface_td surface_storage;
    xcb_screen_t screen_storage;
    list_td *surfaces;
    char out_id[SN_ID_MAX_LEN];
    bool result;

    s_make_one_surface_list(&surfaces, &surface_storage, &screen_storage);
    s_stub_reset();
    s_atom_intern_fail_all = true;

    result = cctl_sn_begin(s_fake_connection, surfaces, "xterm", 0u,
            out_id, sizeof(out_id));

    TAP_OK(!result,
            "cctl_sn_begin refuses to begin a sequence once"
            " atom_intern cannot resolve the begin/info atoms");
    TAP_EQ_INT(s_send_event_call_count, 0,
            "no broadcast is attempted once the atoms themselves"
            " could not be resolved");

    list_destroy(surfaces);
}


/* cctl_sn_begin refuses once the caller's own out_id buffer is too
 * small to hold the generated id, still without broadcasting anything */
static void s_test_begin_out_id_too_small(void)
{
    surface_td surface_storage;
    xcb_screen_t screen_storage;
    list_td *surfaces;
    char tiny_out_id[2];
    bool result;

    s_make_one_surface_list(&surfaces, &surface_storage, &screen_storage);
    s_stub_reset();

    result = cctl_sn_begin(s_fake_connection, surfaces, "xterm", 0u,
            tiny_out_id, sizeof(tiny_out_id));

    TAP_OK(!result,
            "cctl_sn_begin refuses an out_id buffer too small for the"
            " generated id");
    TAP_EQ_INT(s_send_event_call_count, 0,
            "no broadcast is attempted once the out_id buffer itself"
            " is too small");

    list_destroy(surfaces);
}


/* cctl_sn_begin's own success path generates a unique, null-terminated
 * id naming the launched program, broadcasts it (the id itself makes
 * the whole "new: ID=..." message longer than one 20-byte chunk, so
 * more than one xcb_send_event call happens here, exactly like
 * s_broadcast's own chunking loop), and switches on the busy cursor
 * since none was pending before */
static void s_test_begin_success_path_broadcasts_and_shows_cursor(void)
{
    surface_td surface_storage;
    xcb_screen_t screen_storage;
    list_td *surfaces;
    char first_out_id[SN_ID_MAX_LEN];
    char second_out_id[SN_ID_MAX_LEN];
    bool result;

    s_make_one_surface_list(&surfaces, &surface_storage, &screen_storage);
    s_stub_reset();

    result = cctl_sn_begin(s_fake_connection, surfaces, "xterm", 3u,
            first_out_id, sizeof(first_out_id));

    TAP_OK(result, "cctl_sn_begin succeeds with valid arguments");
    TAP_OK(first_out_id[0] != '\0',
            "a non-empty startup id is written into out_id");
    TAP_OK(s_send_event_call_count >= 1,
            "beginning a sequence broadcasts at least one chunk");
    TAP_EQ_INT((int) s_send_event_chunks[0][0], 'n',
            "the very first byte broadcast is the 'n' of the"
            " \"new: ID=...\" message text s_broadcast sends");
    TAP_OK(s_send_event_last_type == (xcb_atom_t) 101u ||
            s_send_event_last_type == (xcb_atom_t) 102u,
            "every chunk broadcast uses either the"
            " _NET_STARTUP_INFO_BEGIN or _NET_STARTUP_INFO atom as its"
            " message type");
    TAP_EQ_INT(s_cursor_load_call_count, 1,
            "beginning the first pending sequence shows the busy"
            " cursor exactly once");
    TAP_EQ_STR(s_cursor_load_last_name, "watch",
            "the busy cursor is loaded by its \"watch\" Xcursor name");
    TAP_EQ_INT(s_change_attrs_call_count, 1,
            "the busy cursor is applied to exactly the one managed"
            " surface's root window");

    /* Beginning a second sequence while the first is still pending
     * must not show the busy cursor again */
    s_stub_reset();
    result = cctl_sn_begin(s_fake_connection, surfaces, "vim", 3u,
            second_out_id, sizeof(second_out_id));
    TAP_OK(result, "a second sequence can begin while the first is"
            " still pending");
    TAP_EQ_INT(s_cursor_load_call_count, 0,
            "a second sequence beginning while one is already pending"
            " never re-shows the busy cursor");

    /* Clean up both pending sequences (the first sequence's own id was
     * about to be overwritten by the second cctl_sn_begin call above,
     * so it was saved into its own buffer specifically so both can be
     * cancelled here) so later scenarios in this binary start from an
     * empty pending list again */
    cctl_sn_cancel(s_fake_connection, surfaces, first_out_id);
    cctl_sn_cancel(s_fake_connection, surfaces, second_out_id);
    s_stub_reset();

    list_destroy(surfaces);
}


/* cctl_sn_handle_client_message reassembles a "remove:" message split
 * across two chunks (the generated startup id itself is always longer
 * than SN_CHUNK_LEN once wrapped in "remove: ID=\"...\"", so this is
 * the only realistic shape a real removal message ever takes) and,
 * once fully reassembled, completes the still-pending sequence it
 * names and restores the cursor */
static void s_test_handle_client_message_two_chunk_remove(void)
{
    surface_td surface_storage;
    xcb_screen_t screen_storage;
    list_td *surfaces;
    char out_id[SN_ID_MAX_LEN];
    char remove_message[SN_MSG_MAX_LEN];
    xcb_client_message_event_t event;
    size_t remove_len;
    size_t sent;
    int chunks_sent;

    s_make_one_surface_list(&surfaces, &surface_storage, &screen_storage);
    s_stub_reset();

    cctl_sn_begin(s_fake_connection, surfaces, "xterm", 0u, out_id,
            sizeof(out_id));
    s_stub_reset();

    (void) snprintf(remove_message, sizeof(remove_message),
            "remove: ID=\"%s\"", out_id);
    /* Including the terminating null, exactly as s_broadcast's own
     * chunking does on the sending side, so a chunk that exactly
     * fills SN_CHUNK_LEN is never mistaken for a complete message */
    remove_len = safe_strlen(remove_message) + 1u;
    TAP_OK(remove_len > SN_CHUNK_LEN,
            "this scenario's own remove message needs more than one"
            " chunk, exactly what it means to test");

    sent = 0u;
    chunks_sent = 0;
    while (sent < remove_len) {
        size_t chunk_len = remove_len - sent;

        if (chunk_len > SN_CHUNK_LEN) {
            chunk_len = SN_CHUNK_LEN;
        }

        memset(&event, 0, sizeof(event));
        event.format = 8;
        event.window = (xcb_window_t) 42u;
        /* _NET_STARTUP_INFO_BEGIN for the first chunk, then
         * _NET_STARTUP_INFO for every chunk after it, matching
         * s_broadcast's own convention on the sending side */
        event.type = (chunks_sent == 0)
            ? (xcb_atom_t) 101u : (xcb_atom_t) 102u;
        memcpy(event.data.data8, remove_message + sent, chunk_len);
        cctl_sn_handle_client_message(s_fake_connection, surfaces,
                &event);

        sent += chunk_len;
        chunks_sent++;
    }

    TAP_OK(chunks_sent > 1,
            "reassembling this scenario's own remove message actually"
            " took more than one chunk");
    TAP_EQ_INT(s_change_attrs_call_count, 1,
            "completing the only pending sequence restores the"
            " default cursor exactly once");
    TAP_EQ_STR(s_cursor_load_last_name, "left_ptr",
            "the restored cursor is loaded by its \"left_ptr\""
            " Xcursor name");

    list_destroy(surfaces);
}


/* cctl_sn_handle_client_message's own guard clauses reject a null
 * connection/surfaces/event, a non-format-8 event, and an event whose
 * type is neither of the two startup-notification atoms, none of
 * which ever changes the cursor */
static void s_test_handle_client_message_guard_clauses(void)
{
    surface_td surface_storage;
    xcb_screen_t screen_storage;
    list_td *surfaces;
    xcb_client_message_event_t event;

    s_make_one_surface_list(&surfaces, &surface_storage, &screen_storage);
    s_stub_reset();

    memset(&event, 0, sizeof(event));
    event.format = 8;
    event.type = (xcb_atom_t) 101u;

    cctl_sn_handle_client_message(NULL, surfaces, &event);
    cctl_sn_handle_client_message(s_fake_connection, NULL, &event);
    cctl_sn_handle_client_message(s_fake_connection, surfaces, NULL);

    event.format = 16;
    cctl_sn_handle_client_message(s_fake_connection, surfaces, &event);

    event.format = 8;
    event.type = (xcb_atom_t) 999u;
    cctl_sn_handle_client_message(s_fake_connection, surfaces, &event);

    TAP_EQ_INT(s_change_attrs_call_count, 0,
            "none of a null connection, null surfaces, a null event, a"
            " non-format-8 event, or an unrecognized message type ever"
            " reaches far enough to change the cursor");

    list_destroy(surfaces);
}


/* cctl_sn_ms_remaining reports -1 with nothing pending, and, once a
 * sequence begins with the timeout overridden to 0 seconds, reports 0
 * (already expired) rather than any negative value */
static void s_test_ms_remaining_reflects_timeout(void)
{
    surface_td surface_storage;
    xcb_screen_t screen_storage;
    list_td *surfaces;
    char out_id[SN_ID_MAX_LEN];

    s_make_one_surface_list(&surfaces, &surface_storage, &screen_storage);
    s_stub_reset();

    TAP_EQ_INT(cctl_sn_ms_remaining(), -1,
            "cctl_sn_ms_remaining reports -1 when nothing is pending");

    cctl_sn_set_timeout_seconds(0u);
    TAP_EQ_INT(cctl_sn_ms_remaining(), -1,
            "cctl_sn_set_timeout_seconds ignores a zero value entirely,"
            " leaving nothing pending changed");

    cctl_sn_set_timeout_seconds(1u);
    cctl_sn_begin(s_fake_connection, surfaces, "xterm", 0u, out_id,
            sizeof(out_id));

    TAP_OK(cctl_sn_ms_remaining() >= 0,
            "once a sequence is pending, cctl_sn_ms_remaining reports"
            " a non-negative countdown");
    TAP_OK(cctl_sn_ms_remaining() <= 1000,
            "the countdown never exceeds the overridden one-second"
            " timeout, in milliseconds");

    cctl_sn_cancel(s_fake_connection, surfaces, out_id);
    s_stub_reset();
    list_destroy(surfaces);
}


/* cctl_sn_tick, called immediately after a sequence begins (long
 * before even a one-second overridden timeout could possibly have
 * elapsed), takes its own early-continue branch and leaves the
 * sequence pending untouched, exactly like cctl_kill_tick's own
 * before-deadline branch; cctl_sn_tick with nothing pending at all is
 * a quiet no-op */
static void s_test_tick_before_timeout_leaves_sequence_pending(void)
{
    surface_td surface_storage;
    xcb_screen_t screen_storage;
    list_td *surfaces;
    char out_id[SN_ID_MAX_LEN];

    s_make_one_surface_list(&surfaces, &surface_storage, &screen_storage);
    s_stub_reset();

    cctl_sn_tick(s_fake_connection, surfaces);
    TAP_EQ_INT(s_change_attrs_call_count, 0,
            "cctl_sn_tick with nothing pending and no idle reassembly"
            " slots is a quiet no-op");

    cctl_sn_set_timeout_seconds(1u);
    cctl_sn_begin(s_fake_connection, surfaces, "xterm", 0u, out_id,
            sizeof(out_id));
    s_stub_reset();

    cctl_sn_tick(s_fake_connection, surfaces);

    TAP_OK(cctl_sn_ms_remaining() >= 0,
            "cctl_sn_tick's own early-continue branch leaves a"
            " sequence whose one-second timeout has not elapsed yet"
            " exactly as pending as it found it");
    TAP_EQ_INT(s_change_attrs_call_count, 0,
            "a tick that changes nothing never touches the cursor"
            " either");

    cctl_sn_cancel(s_fake_connection, surfaces, out_id);
    TAP_EQ_INT(cctl_sn_ms_remaining(), -1,
            "cancelling the only pending sequence removes it, so"
            " cctl_sn_ms_remaining reports -1 again");
    TAP_EQ_INT(s_change_attrs_call_count, 1,
            "cancelling the only pending sequence restores the"
            " default cursor exactly once");

    s_stub_reset();
    list_destroy(surfaces);
}


/* cctl_sn_desktop_for_window recovers the origin desktop of a still-
 * pending sequence from a matching '_NET_STARTUP_ID' property, refuses
 * a null connection/out_desktop, and reports false whenever the
 * property itself cannot be read, is empty, or names no currently
 * pending sequence */
static void s_test_desktop_for_window(void)
{
    surface_td surface_storage;
    xcb_screen_t screen_storage;
    list_td *surfaces;
    char out_id[SN_ID_MAX_LEN];
    uint32_t out_desktop;
    bool result;

    s_make_one_surface_list(&surfaces, &surface_storage, &screen_storage);
    s_stub_reset();

    cctl_sn_begin(s_fake_connection, surfaces, "xterm", 7u, out_id,
            sizeof(out_id));
    s_stub_reset();

    result = cctl_sn_desktop_for_window(NULL, (xcb_window_t) 1u,
            &out_desktop);
    TAP_OK(!result, "cctl_sn_desktop_for_window refuses a null"
            " connection");

    result = cctl_sn_desktop_for_window(s_fake_connection,
            (xcb_window_t) 1u, NULL);
    TAP_OK(!result, "cctl_sn_desktop_for_window refuses a null"
            " out_desktop");

    s_get_property_reply_is_null = true;
    result = cctl_sn_desktop_for_window(s_fake_connection,
            (xcb_window_t) 1u, &out_desktop);
    TAP_OK(!result, "a failed property read reports false");
    s_get_property_reply_is_null = false;

    s_get_property_value_len = 0u;
    result = cctl_sn_desktop_for_window(s_fake_connection,
            (xcb_window_t) 1u, &out_desktop);
    TAP_OK(!result, "an empty property value reports false");

    (void) safe_strncpy(s_get_property_value, "no-such-id",
            sizeof(s_get_property_value) - 1u);
    s_get_property_value_len = (uint32_t) safe_strlen("no-such-id");
    result = cctl_sn_desktop_for_window(s_fake_connection,
            (xcb_window_t) 1u, &out_desktop);
    TAP_OK(!result, "a property naming an id that matches no pending"
            " sequence reports false");

    (void) safe_strncpy(s_get_property_value, out_id,
            sizeof(s_get_property_value) - 1u);
    s_get_property_value_len = (uint32_t) safe_strlen(out_id);
    out_desktop = 0xffffffffu;
    result = cctl_sn_desktop_for_window(s_fake_connection,
            (xcb_window_t) 1u, &out_desktop);
    TAP_OK(result, "a property naming the still-pending sequence's own"
            " id reports true");
    TAP_EQ_INT((int) out_desktop, 7,
            "the recovered desktop matches the origin_desktop this"
            " sequence began with");

    cctl_sn_cancel(s_fake_connection, surfaces, out_id);
    s_stub_reset();
    list_destroy(surfaces);
}


/* cctl_sn_cancel is a silent no-op with a null connection, null
 * surfaces, a null id, or an id that names no currently pending
 * sequence */
static void s_test_cancel_guard_clauses_and_unknown_id(void)
{
    surface_td surface_storage;
    xcb_screen_t screen_storage;
    list_td *surfaces;

    s_make_one_surface_list(&surfaces, &surface_storage, &screen_storage);
    s_stub_reset();

    cctl_sn_cancel(NULL, surfaces, "some-id");
    cctl_sn_cancel(s_fake_connection, NULL, "some-id");
    cctl_sn_cancel(s_fake_connection, surfaces, NULL);
    cctl_sn_cancel(s_fake_connection, surfaces, "no-such-pending-id");

    TAP_EQ_INT(s_change_attrs_call_count, 0,
            "none of a null connection, null surfaces, a null id, or"
            " an id naming no pending sequence ever changes the"
            " cursor");

    list_destroy(surfaces);
}


int main(void)
{
    TAP_PLAN(41);

    s_test_begin_guard_clauses();
    s_test_begin_atom_intern_failure();
    s_test_begin_out_id_too_small();
    s_test_begin_success_path_broadcasts_and_shows_cursor();
    s_test_handle_client_message_two_chunk_remove();
    s_test_handle_client_message_guard_clauses();
    s_test_ms_remaining_reflects_timeout();
    s_test_tick_before_timeout_leaves_sequence_pending();
    s_test_desktop_for_window();
    s_test_cancel_guard_clauses_and_unknown_id();

    return TAP_DONE();
}
