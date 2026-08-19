/**
 * @file tests/test_sn.c
 *
 * @brief Test battery for the startup-notification module's own
 *        timeout and reassembly logic
 *
 * Covers 'sn_ms_remaining', 'sn_tick', 'sn_cancel', and
 * 'sn_handle_client_message' entirely through 'sn_test_add_pending'
 * and 'sn_test_set_atoms' (see their own comments in sn.h), never
 * through 'sn_begin' itself: 'sn_begin' broadcasts a real message and
 * shows a real cursor over an actual XCB connection, neither of which
 * any test here needs in order to exercise the timeout and
 * reassembly logic that is the real point of this module.  A dummy,
 * never-dereferenced-for-real 'connection'/'surfaces' pair is passed
 * to 'sn_tick'/'sn_cancel' purely to satisfy their own null guards;
 * this is safe because 'sn_test_reset' leaves 's_cursor_busy' false
 * throughout every test here (only 'sn_begin', never called),
 * so the one path that would actually dereference them
 * ('s_set_busy_cursor', reached only once every pending sequence has
 * ended while the cursor was showing as busy) is never taken.
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
#include <stddef.h>
#include <stdio.h>      /* snprintf */
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/cursor.h>
#include <utils/safe/safestr.h>
#include <utils/xcb/atom.h>

/* Default initial values */
#include <defs/sn.h>

/* Local includes */
#include <sn.h>
#include <harness/tap.h>


/* Trivial link-time stand-ins for the cursor and atom-resolution
 * helpers 'sn_begin' alone would otherwise pull in: 'sn_begin' is
 * never called anywhere in this file (see this file's own top
 * comment for why), so none of these ever actually run, and only
 * need to exist for the linker's own sake. Local, rather than
 * linking the real 'utils/cursor.c'/'utils/xcb/atom.c', since the
 * real cursor helper alone pulls in XCB cursor-theme support this
 * suite has no use for and no need to build against. */
util_cursor_ctx_td *util_cursor_ctx_new(xcb_connection_t *connection,
        xcb_screen_t *screen)
{
    (void) connection;
    (void) screen;
    return NULL;
}


xcb_cursor_t util_cursor_load(util_cursor_ctx_td *ctx, const char *name,
        uint16_t glyph)
{
    (void) ctx;
    (void) name;
    (void) glyph;
    return XCB_NONE;
}


void util_cursor_ctx_free(util_cursor_ctx_td *ctx)
{
    (void) ctx;
}


xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) name;
    (void) only_if_exists;
    return XCB_ATOM_NONE;
}


xcb_void_cookie_t xcb_change_window_attributes(xcb_connection_t *c,
        xcb_window_t window, uint32_t value_mask,
        const void *value_list)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) c;
    (void) window;
    (void) value_mask;
    (void) value_list;
    return cookie;
}


xcb_void_cookie_t xcb_free_cursor(xcb_connection_t *c,
        xcb_cursor_t cursor)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) c;
    (void) cursor;
    return cookie;
}


xcb_void_cookie_t xcb_send_event(xcb_connection_t *c, uint8_t propagate,
        xcb_window_t destination, uint32_t event_mask,
        const char *event)
{
    xcb_void_cookie_t cookie = { 0 };

    (void) c;
    (void) propagate;
    (void) destination;
    (void) event_mask;
    (void) event;
    return cookie;
}


int xcb_flush(xcb_connection_t *c)
{
    (void) c;
    return 1;
}


/** Never actually dereferenced; see this file's own top comment */
static xcb_connection_t *const s_fake_connection = (xcb_connection_t *) 1;

/** An empty, but non-null, surfaces list; every function under test
 *  here only ever walks it when restoring the cursor, a path this
 *  whole file's own top comment already explains is never reached */
static list_td s_fake_surfaces_storage;
static list_td *const s_fake_surfaces = &s_fake_surfaces_storage;

#define TEST_ATOM_BEGIN ((xcb_atom_t) 111)
#define TEST_ATOM_INFO  ((xcb_atom_t) 222)


static void s_test_ms_remaining_with_nothing_pending(void)
{
    sn_test_reset();

    TAP_EQ_INT(sn_ms_remaining(), -1, "nothing pending, -1");
}


static void s_test_ms_remaining_with_one_pending(void)
{
    int remaining;

    sn_test_reset();
    TAP_OK(sn_test_add_pending("seq-a", 0u), "add_pending succeeds");

    remaining = sn_ms_remaining();
    TAP_OK(remaining >= 0, "a pending sequence reports a" \
            " non-negative remaining time");

    sn_test_reset();
}


static void s_test_ms_remaining_picks_the_closest_deadline(void)
{
    int remaining;

    sn_test_reset();

    /* Both started 'SN_TIMEOUT_SECONDS' worth of ms ago minus a
     * small and a large margin respectively, so 'seq-soon' is
     * unambiguously the one closer to timing out */
    sn_test_add_pending("seq-far", 1000u);
    sn_test_add_pending("seq-soon",
            (SN_TIMEOUT_SECONDS * 1000u) - 500u);

    remaining = sn_ms_remaining();
    TAP_OK(remaining >= 0 && remaining < 1000,
            "the soonest of two deadlines wins, not the first or" \
            " last added");

    sn_test_reset();
}


static void s_test_add_pending_beyond_capacity_fails(void)
{
    bool ok = true;
    char id[16];

    sn_test_reset();

    for (int i = 0; i < SN_MAX_PENDING && ok; ++i) {
        (void) snprintf(id, sizeof(id), "seq-%d", i);
        ok = sn_test_add_pending(id, 0u);
    }
    TAP_OK(ok, "filling every pending slot succeeds");
    TAP_OK(!sn_test_add_pending("seq-overflow", 0u),
            "one more past the limit fails instead of overflowing");

    sn_test_reset();
}


static void s_test_tick_leaves_a_not_yet_due_sequence_pending(void)
{
    sn_test_reset();
    sn_test_add_pending("seq-a", 0u);

    sn_tick(s_fake_connection, s_fake_surfaces);

    TAP_OK(sn_ms_remaining() >= 0,
            "still pending after a tick, since its own timeout has" \
            " not elapsed yet");

    sn_test_reset();
}


static void s_test_tick_expires_a_due_sequence(void)
{
    sn_test_reset();
    sn_test_add_pending("seq-a", (SN_TIMEOUT_SECONDS * 1000u) + 1000u);

    sn_tick(s_fake_connection, s_fake_surfaces);

    TAP_EQ_INT(sn_ms_remaining(), -1,
            "a sequence started longer ago than the timeout is" \
            " expired by the next tick");

    sn_test_reset();
}


static void s_test_tick_only_expires_due_sequences(void)
{
    int remaining;

    sn_test_reset();
    sn_test_add_pending("seq-due",
            (SN_TIMEOUT_SECONDS * 1000u) + 1000u);
    sn_test_add_pending("seq-fresh", 0u);

    sn_tick(s_fake_connection, s_fake_surfaces);

    remaining = sn_ms_remaining();
    TAP_OK(remaining >= 0,
            "the fresh sequence alone is still pending after the" \
            " tick that expired the due one");

    sn_test_reset();
}


static void s_test_cancel_removes_the_named_sequence_only(void)
{
    sn_test_reset();
    sn_test_add_pending("seq-a", 0u);
    sn_test_add_pending("seq-b", 0u);

    sn_cancel(s_fake_connection, s_fake_surfaces, "seq-a");

    /* Only 'seq-b' should remain; cancel one more time, by its own
     * id, to confirm it (not some other slot) is what is left */
    sn_cancel(s_fake_connection, s_fake_surfaces, "seq-b");
    TAP_EQ_INT(sn_ms_remaining(), -1,
            "cancelling both sequences by their own ids leaves" \
            " nothing pending");

    sn_test_reset();
}


static void s_test_cancel_unknown_id_is_a_no_op(void)
{
    sn_test_reset();
    sn_test_add_pending("seq-a", 0u);

    sn_cancel(s_fake_connection, s_fake_surfaces, "seq-does-not-exist");

    TAP_OK(sn_ms_remaining() >= 0,
            "cancelling an id that was never pending leaves the" \
            " real one untouched");

    sn_test_reset();
}


/**
 * @brief Send one 20-byte-or-shorter chunk to 'sn_handle_client_message'
 *
 * @param window Source window
 * @param type   Atom identifying which of the two message kinds this
 *               chunk belongs to
 * @param text   Chunk text, at most 'SN_CHUNK_LEN' (20) bytes
 */
static void s_send_chunk(xcb_window_t window, xcb_atom_t type,
        const char *text)
{
    xcb_client_message_event_t ev;
    size_t len;

    memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.format = 8;
    ev.window = window;
    ev.type = type;

    /* Raw 'memcpy' of exactly 'len' bytes, not 'safe_strncpy' (which
     * always reserves its own last byte for a null terminator, so it
     * could never actually fill all 20 bytes of a genuinely full
     * chunk the way the real wire protocol does): a real ClientMessage's
     * own data8 payload is 20 raw bytes, not a null-terminated C
     * string, and 'sn_handle_client_message' itself relies on this
     * (via 'safe_strnlen', capped at SN_CHUNK_LEN) to tell a full,
     * still-more-coming chunk apart from a shorter, message-ending
     * one. */
    len = safe_strlen(text);
    if (len > 20u) {
        len = 20u;
    }
    memcpy(ev.data.data8, text, len);

    sn_handle_client_message(s_fake_connection, s_fake_surfaces, &ev);
}


static void s_test_handle_message_single_chunk_completes_sequence(void)
{
    sn_test_reset();
    sn_test_set_atoms(TEST_ATOM_BEGIN, TEST_ATOM_INFO);
    sn_test_add_pending("a", 0u);

    /* 14 bytes, under the 20-byte payload, so this single chunk
     * alone already marks the message complete; see 's_broadcast''s
     * own comment in sn.c */
    s_send_chunk((xcb_window_t) 42, TEST_ATOM_BEGIN,
            "remove: ID=\"a\"");

    TAP_EQ_INT(sn_ms_remaining(), -1,
            "a single-chunk 'remove:' message naming the pending" \
            " sequence completes it");

    sn_test_reset();
}


static void s_test_handle_message_reassembles_across_chunks(void)
{
    sn_test_reset();
    sn_test_set_atoms(TEST_ATOM_BEGIN, TEST_ATOM_INFO);
    sn_test_add_pending("split-id", 0u);

    /* 'remove: ID="split-id"' is 21 bytes; split at the exact
     * 20-byte boundary so the first chunk (being a full,
     * unshortened 20 bytes) does NOT yet mark the message complete,
     * forcing genuine reassembly across the two */
    s_send_chunk((xcb_window_t) 42, TEST_ATOM_BEGIN,
            "remove: ID=\"split-id");
    TAP_OK(sn_ms_remaining() >= 0,
            "still pending after only the first, still-full chunk" \
            " arrives");

    s_send_chunk((xcb_window_t) 42, TEST_ATOM_INFO, "\"");
    TAP_EQ_INT(sn_ms_remaining(), -1,
            "completed once the second, shorter chunk finishes the" \
            " message");

    sn_test_reset();
}


static void s_test_handle_message_different_windows_dont_interfere(void)
{
    sn_test_reset();
    sn_test_set_atoms(TEST_ATOM_BEGIN, TEST_ATOM_INFO);
    sn_test_add_pending("win-a-id", 0u);
    sn_test_add_pending("win-b-id", 0u);

    /* Interleaved, still-partial (exactly 20-byte) chunks from two
     * different windows; each window's own reassembly slot must
     * stay independent */
    s_send_chunk((xcb_window_t) 100, TEST_ATOM_BEGIN,
            "remove: ID=\"win-a-id");
    s_send_chunk((xcb_window_t) 200, TEST_ATOM_BEGIN,
            "remove: ID=\"win-b-id");
    s_send_chunk((xcb_window_t) 100, TEST_ATOM_INFO, "\"");

    TAP_OK(sn_ms_remaining() >= 0,
            "window 100's own completed message finished 'win-a-id'" \
            " alone; 'win-b-id' is still pending");

    s_send_chunk((xcb_window_t) 200, TEST_ATOM_INFO, "\"");
    TAP_EQ_INT(sn_ms_remaining(), -1,
            "finishing window 200's own message too now completes" \
            " the last one");

    sn_test_reset();
}


static void s_test_handle_message_wrong_atom_type_ignored(void)
{
    xcb_client_message_event_t ev;

    sn_test_reset();
    sn_test_set_atoms(TEST_ATOM_BEGIN, TEST_ATOM_INFO);
    sn_test_add_pending("seq-a", 0u);

    memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.format = 8;
    ev.window = (xcb_window_t) 42;
    ev.type = (xcb_atom_t) 999999;
    (void) safe_strncpy((char *) ev.data.data8,
            "remove: ID=\"seq-a\"", 20u);
    sn_handle_client_message(s_fake_connection, s_fake_surfaces, &ev);

    TAP_OK(sn_ms_remaining() >= 0,
            "a ClientMessage whose type is neither known atom is" \
            " ignored, leaving the sequence pending");

    sn_test_reset();
}


static void s_test_reset_clears_everything(void)
{
    sn_test_set_atoms(TEST_ATOM_BEGIN, TEST_ATOM_INFO);
    sn_test_add_pending("seq-a", 0u);
    TAP_OK(sn_ms_remaining() >= 0, "something pending before reset");

    sn_test_reset();

    TAP_EQ_INT(sn_ms_remaining(), -1,
            "nothing pending immediately after reset");
}


int main(void)
{
    TAP_PLAN(19);

    s_test_ms_remaining_with_nothing_pending();
    s_test_ms_remaining_with_one_pending();
    s_test_ms_remaining_picks_the_closest_deadline();
    s_test_add_pending_beyond_capacity_fails();
    s_test_tick_leaves_a_not_yet_due_sequence_pending();
    s_test_tick_expires_a_due_sequence();
    s_test_tick_only_expires_due_sequences();
    s_test_cancel_removes_the_named_sequence_only();
    s_test_cancel_unknown_id_is_a_no_op();
    s_test_handle_message_single_chunk_completes_sequence();
    s_test_handle_message_reassembles_across_chunks();
    s_test_handle_message_different_windows_dont_interfere();
    s_test_handle_message_wrong_atom_type_ignored();
    s_test_reset_clears_everything();

    return TAP_DONE();
}
