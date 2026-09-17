/**
 * @file tests/input/mouse/test_cursor.c
 *
 * @brief Test battery for resize-border cursor loading and per-window
 *        application
 *
 * 'util_cursor_ctx_new'/'util_cursor_load'/'util_cursor_ctx_free'
 * (utils/cursor.c) resolve an Xcursor theme through 'libxcb-cursor'
 * against a live display, so they are replaced by controlled
 * stand-ins that hand back a distinct, deterministic cursor id per
 * requested name; that is what lets 'mouse_resize_cursor_for_axes'
 * be checked against the exact zone each axis/anchor combination
 * ought to resolve to, rather than merely a nonzero value.
 * 'xcb_setup_roots_iterator'/'xcb_get_setup'/'xcb_free_cursor'/
 * 'xcb_change_window_attributes' are real libxcb protocol entry
 * points needing a live X connection to answer for real, so they too
 * are replaced by link-only or recording stand-ins; this test never
 * links against libxcb itself; only its headers, for the type
 * declarations.  'lookup_find_client' (lookup.c) is a genuinely
 * cross-module dependency (client/desktop/stage lookup across the
 * whole managed window tree), so it is replaced by a controlled
 * stand-in that hands back a client fixture built by each scenario.
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
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <logger.h>
#include <utils/cursor.h>

/* Local includes */
#include <harness/tap.h>
#include <input/mouse/cursor.h>


/** Non-null opaque handle standing in for a real xcb_connection_t */
static int s_fake_connection_storage;
static xcb_connection_t *const s_fake_connection =
        (xcb_connection_t *) &s_fake_connection_storage;

/** Non-null opaque handle standing in for a real list_td of stages */
static int s_fake_stages_storage;
static list_td *const s_fake_stages = (list_td *) &s_fake_stages_storage;

/** Non-null opaque handle standing in for a real util_cursor_ctx_td */
static int s_fake_ctx_storage;
static util_cursor_ctx_td *const s_fake_ctx =
        (util_cursor_ctx_td *) &s_fake_ctx_storage;

/** Client 'lookup_find_client' should hand back, or @c NULL */
static client_td *s_lookup_result;
static int s_call_lookup_find_client;

/** Call counters and last-seen arguments for the window-attribute
 *  change, reset by s_reset before each scenario */
static int s_call_change_window_attributes;
static xcb_window_t s_last_change_window;
static uint32_t s_last_change_cursor;
static int s_call_free_cursor;


/**
 * @brief Reset every recording stand-in's state before a scenario
 *
 * @note Complexity: @e O(1)
 */
static void s_reset(void)
{
    s_lookup_result = NULL;
    s_call_lookup_find_client = 0;
    s_call_change_window_attributes = 0;
    s_last_change_window = XCB_WINDOW_NONE;
    s_last_change_cursor = 0;
    s_call_free_cursor = 0;
}


/**
 * @brief Controlled stand-in for @a util_cursor_ctx_new
 *
 * @note Complexity: @e O(1)
 */
util_cursor_ctx_td *util_cursor_ctx_new(xcb_connection_t *connection,
        xcb_screen_t *screen)
{
    (void) connection;
    (void) screen;
    return s_fake_ctx;
}


/**
 * @brief Controlled stand-in for @a util_cursor_load
 *
 * Hands back a distinct, deterministic id per standard Xcursor name so
 * that later scenarios can tell which zone's cursor was actually
 * looked up rather than merely that some nonzero id was
 *
 * @note Complexity: @e O(1)
 */
xcb_cursor_t util_cursor_load(util_cursor_ctx_td *ctx, const char *name,
        uint16_t fallback_glyph)
{
    (void) ctx;
    (void) fallback_glyph;

    if (strcmp(name, "left_ptr") == 0) { return 1; }
    if (strcmp(name, "top_side") == 0) { return 2; }
    if (strcmp(name, "bottom_side") == 0) { return 3; }
    if (strcmp(name, "right_side") == 0) { return 4; }
    if (strcmp(name, "left_side") == 0) { return 5; }
    if (strcmp(name, "top_right_corner") == 0) { return 6; }
    if (strcmp(name, "top_left_corner") == 0) { return 7; }
    if (strcmp(name, "bottom_right_corner") == 0) { return 8; }
    if (strcmp(name, "bottom_left_corner") == 0) { return 9; }
    if (strcmp(name, "fleur") == 0) { return 10; }
    return 0;
}


/**
 * @brief Link-only stand-in for @a util_cursor_ctx_free
 *
 * @note Complexity: @e O(1)
 */
void util_cursor_ctx_free(util_cursor_ctx_td *ctx)
{
    (void) ctx;
}


/**
 * @brief Link-only stand-in for @a xcb_get_setup
 *
 * Never dereferenced by anything other than the paired
 * 'xcb_setup_roots_iterator' stand-in below, so returning @c NULL is
 * enough
 *
 * @note Complexity: @e O(1)
 */
const struct xcb_setup_t *xcb_get_setup(xcb_connection_t *connection)
{
    (void) connection;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_setup_roots_iterator
 *
 * 'mouse_resize_cursors_init' only ever reads the iterator's own
 * '.data' field, handing it straight to 'util_cursor_ctx_new' (itself
 * a controlled stand-in above that ignores its 'screen' argument), so
 * a zeroed iterator is enough
 *
 * @note Complexity: @e O(1)
 */
xcb_screen_iterator_t xcb_setup_roots_iterator(const xcb_setup_t *setup)
{
    xcb_screen_iterator_t it;

    (void) setup;
    memset(&it, 0, sizeof(it));
    return it;
}


/**
 * @brief Recording stand-in for @a xcb_free_cursor
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_free_cursor(xcb_connection_t *connection,
        xcb_cursor_t cursor)
{
    xcb_void_cookie_t cookie;

    (void) connection;
    (void) cursor;
    memset(&cookie, 0, sizeof(cookie));
    s_call_free_cursor++;
    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_change_window_attributes
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_change_window_attributes(xcb_connection_t *connection,
        xcb_window_t window, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie;
    const uint32_t *values;

    (void) connection;
    memset(&cookie, 0, sizeof(cookie));
    s_call_change_window_attributes++;
    s_last_change_window = window;
    if (value_mask == XCB_CW_CURSOR && value_list != NULL) {
        values = value_list;
        s_last_change_cursor = values[0];
    }

    return cookie;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 *
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict msg, ...)
{
    (void) level;
    (void) prefix;
    (void) msg;
    return 0;
}


/**
 * @brief Controlled stand-in for @a lookup_find_client
 *
 * @note Complexity: @e O(1)
 */
client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **out_stage, desktop_td **out_desktop)
{
    (void) stages;
    (void) window;
    s_call_lookup_find_client++;
    if (out_stage != NULL) {
        *out_stage = NULL;
    }

    if (out_desktop != NULL) {
        *out_desktop = NULL;
    }

    return s_lookup_result;
}


/* Before init, every accessor reads zeroed (unallocated) cursor ids */
static void s_test_before_init_all_zero(void)
{
    TAP_EQ_INT((int) mouse_plain_cursor(), 0,
            "the plain cursor is 0 before mouse_resize_cursors_init runs");
    TAP_EQ_INT((int) mouse_cursor_move(), 0,
            "the move cursor is 0 before mouse_resize_cursors_init runs");
    TAP_EQ_INT((int) mouse_resize_cursor_for_axes(true, true, true, true),
            0, "a resize-axis cursor is 0 before init runs too");
}


/* mouse_resize_cursors_init is a no-op with a NULL connection */
static void s_test_init_null_connection(void)
{
    mouse_resize_cursors_init(NULL);

    TAP_EQ_INT((int) mouse_plain_cursor(), 0,
            "a NULL connection never allocates the plain cursor");
}


/* After init, the plain and move cursors match the ids the loader
 * handed back for their own standard Xcursor names */
static void s_test_init_populates_cursors(void)
{
    mouse_resize_cursors_init(s_fake_connection);

    TAP_EQ_INT((int) mouse_plain_cursor(), 1,
            "the plain cursor matches 'left_ptr's loaded id");
    TAP_EQ_INT((int) mouse_cursor_move(), 10,
            "the move cursor matches 'fleur's loaded id");
}


/* Calling init again is a no-op: it never reloads anything once the
 * plain cursor slot is already populated */
static void s_test_init_is_idempotent(void)
{
    /* mouse_resize_cursors_init was already called by
     * s_test_init_populates_cursors above; this second call must not
     * disturb the already-loaded ids */
    mouse_resize_cursors_init(s_fake_connection);

    TAP_EQ_INT((int) mouse_plain_cursor(), 1,
            "a second init call leaves the plain cursor id untouched");
}


/* Every width/height/anchor combination resolves to its own distinct,
 * documented zone once the cursor table is populated */
static void s_test_resize_cursor_for_axes_mapping(void)
{
    TAP_EQ_INT((int) mouse_resize_cursor_for_axes(false, false, false, false),
            1, "no axis at all resolves to the plain (NONE) cursor");

    TAP_EQ_INT((int) mouse_resize_cursor_for_axes(false, true, false, true),
            2, "height only, anchored at the bottom, is the N (top_side)" \
            " cursor");
    TAP_EQ_INT((int) mouse_resize_cursor_for_axes(false, true, false, false),
            3, "height only, anchored at the top, is the S (bottom_side)" \
            " cursor");
    TAP_EQ_INT((int) mouse_resize_cursor_for_axes(true, false, false, false),
            4, "width only, anchored at the left, is the E (right_side)" \
            " cursor");
    TAP_EQ_INT((int) mouse_resize_cursor_for_axes(true, false, true, false),
            5, "width only, anchored at the right, is the W (left_side)" \
            " cursor");

    TAP_EQ_INT((int) mouse_resize_cursor_for_axes(true, true, false, true),
            6, "both axes, anchored bottom-left, is the NE" \
            " (top_right_corner) cursor");
    TAP_EQ_INT((int) mouse_resize_cursor_for_axes(true, true, true, true),
            7, "both axes, anchored bottom-right, is the NW" \
            " (top_left_corner) cursor");
    TAP_EQ_INT((int) mouse_resize_cursor_for_axes(true, true, false, false),
            8, "both axes, anchored top-left, is the SE" \
            " (bottom_right_corner) cursor");
    TAP_EQ_INT((int) mouse_resize_cursor_for_axes(true, true, true, false),
            9, "both axes, anchored top-right, is the SW" \
            " (bottom_left_corner) cursor");
}


/* mouse_resize_cursors_destroy frees every allocated cursor once, and
 * is a no-op with a NULL connection */
static void s_test_destroy(void)
{
    s_call_free_cursor = 0;

    mouse_resize_cursors_destroy(NULL);
    TAP_EQ_INT(s_call_free_cursor, 0,
            "a NULL connection never frees any cursor");

    mouse_resize_cursors_destroy(s_fake_connection);
    /* 9 resize-zone cursors (NONE, N, S, E, W, NE, NW, SE, SW) plus
     * the move cursor: 10 distinct ids were loaded by
     * s_test_init_populates_cursors above, so 10 frees are expected */
    TAP_EQ_INT(s_call_free_cursor, 10,
            "destroying frees exactly the plain, resize-zone, and move" \
            " cursors, all 10 of them");

    TAP_EQ_INT((int) mouse_plain_cursor(), 0,
            "the plain cursor reads back 0 again once destroyed");
    TAP_EQ_INT((int) mouse_cursor_move(), 0,
            "the move cursor reads back 0 again once destroyed");
}


/* mouse_resize_cursor_update returns NULL, without touching the
 * cursor attribute, for a NULL connection, NULL stages, or before
 * mouse_resize_cursors_init has ever run */
static void s_test_update_guard_clauses(void)
{
    client_td *result;

    /* Cursors were freed by s_test_destroy above, so the table is
     * back to its pre-init all-zero state; re-populate it first so
     * only the guard clauses under test here are exercised */
    mouse_resize_cursors_init(s_fake_connection);

    s_reset();
    result = mouse_resize_cursor_update(NULL, s_fake_stages,
            (xcb_window_t) 1, (struct position_s) { 0, 0 });
    TAP_NULL(result, "a NULL connection returns NULL");
    TAP_EQ_INT(s_call_lookup_find_client, 0,
            "a NULL connection never reaches lookup_find_client");

    s_reset();
    result = mouse_resize_cursor_update(s_fake_connection, NULL,
            (xcb_window_t) 1, (struct position_s) { 0, 0 });
    TAP_NULL(result, "NULL stages returns NULL");
    TAP_EQ_INT(s_call_lookup_find_client, 0,
            "NULL stages never reaches lookup_find_client either");
}


/* mouse_resize_cursor_update returns NULL and skips the cursor
 * attribute change entirely when no client owns the window */
static void s_test_update_no_client(void)
{
    client_td *result;

    s_reset();
    s_lookup_result = NULL;

    result = mouse_resize_cursor_update(s_fake_connection, s_fake_stages,
            (xcb_window_t) 5, (struct position_s) { 10, 10 });

    TAP_NULL(result, "no owning client at all returns NULL");
    TAP_EQ_INT(s_call_change_window_attributes, 0,
            "no owning client skips the cursor-attribute change");
}


/* mouse_resize_cursor_update returns NULL and skips the cursor
 * change for a client that is not resizable */
static void s_test_update_not_resizable(void)
{
    client_td client;
    client_td *result;

    memset(&client, 0, sizeof(client));
    /* client_is_resizable(w) requires CLIENT_FLAG_RESIZABLE among its
     * properties.flags; leaving every flag at 0 keeps it false */

    s_reset();
    s_lookup_result = &client;

    result = mouse_resize_cursor_update(s_fake_connection, s_fake_stages,
            (xcb_window_t) 6, (struct position_s) { 10, 10 });

    TAP_NULL(result, "a non-resizable client returns NULL");
    TAP_EQ_INT(s_call_change_window_attributes, 0,
            "a non-resizable client skips the cursor-attribute change");
}


/* mouse_resize_cursor_update, for a resizable client hit outside any
 * border zone, still applies the plain cursor and returns the client */
static void s_test_update_resizable_zone_none(void)
{
    client_td client;
    client_td *result;

    memset(&client, 0, sizeof(client));
    client.frame = 0;   /* undecorated: bounds equal its own geometry */
    client.properties.flags = CLIENT_FLAG_RESIZABLE;
    client.layout.geometry.cur.pos.x = 0;
    client.layout.geometry.cur.pos.y = 0;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 200u;

    s_reset();
    s_lookup_result = &client;

    /* Dead center: far from every border, so S_RESIZE_ZONE_NONE */
    result = mouse_resize_cursor_update(s_fake_connection, s_fake_stages,
            (xcb_window_t) 7, (struct position_s) { 100, 100 });

    TAP_OK(result == &client,
            "a resizable client's own pointer is returned");
    TAP_EQ_INT(s_call_change_window_attributes, 1,
            "the cursor attribute is changed exactly once");
    TAP_EQ_INT((int) s_last_change_window, 7,
            "the attribute change targets the event's own window");
    TAP_EQ_INT((int) s_last_change_cursor, 1,
            "the interior of a resizable client gets the plain" \
            " (left_ptr) cursor");
}


/* mouse_resize_cursor_update, for a point on the client's left border,
 * applies the W (left_side) cursor */
static void s_test_update_resizable_zone_west(void)
{
    client_td client;
    client_td *result;

    memset(&client, 0, sizeof(client));
    client.frame = 0;
    client.properties.flags = CLIENT_FLAG_RESIZABLE;
    client.layout.geometry.cur.pos.x = 0;
    client.layout.geometry.cur.pos.y = 0;
    client.layout.geometry.cur.dim.w = 200u;
    client.layout.geometry.cur.dim.h = 200u;

    s_reset();
    s_lookup_result = &client;

    /* x=-1 is just past the left edge (margin is 0 since
     * WM_RESIZE_GRAB_THRESHOLD is 0 and the undecorated client's
     * theme border width is 0 too), y=100 is nowhere near top or
     * bottom; 'near_left' requires 'root_pos.x < b.left + margin_left'
     * (strictly less), so x=0 itself would miss and fall through to
     * S_RESIZE_ZONE_NONE instead */
    result = mouse_resize_cursor_update(s_fake_connection, s_fake_stages,
            (xcb_window_t) 8, (struct position_s) { -1, 100 });

    TAP_OK(result == &client, "a border hit still returns the client");
    TAP_EQ_INT((int) s_last_change_cursor, 5,
            "the left border gets the W (left_side) cursor");
}


int main(void)
{
    TAP_PLAN(34);

    s_test_before_init_all_zero();
    s_test_init_null_connection();
    s_test_init_populates_cursors();
    s_test_init_is_idempotent();
    s_test_resize_cursor_for_axes_mapping();
    s_test_destroy();
    s_test_update_guard_clauses();
    s_test_update_no_client();
    s_test_update_not_resizable();
    s_test_update_resizable_zone_none();
    s_test_update_resizable_zone_west();

    return TAP_DONE();
}
