/**
 * @file tests/client/test_props.c
 *
 * @brief Test battery for the window identity string readers
 *
 * Exercises 'client_props_get_wm_name', 'client_props_get_net_wm_name'
 * and 'client_props_get_wm_class' (client/props.c) linked against the
 * real source file.  Every XCB and EWMH call it makes is answered by
 * a controllable stand-in below, whose reply a test sets up right
 * before calling into 'client/props.c', so each case drives the real
 * parsing and buffer-truncation logic under test with a fake, but
 * fully deterministic, X server answer standing in for the real
 * round trip.  Nothing here ever needs a live X connection.
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Local includes */
#include <client.h>
#include <client/props.h>
#include <harness/tap.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>


/**
 * @brief Fake @c xcb_get_property_reply_t payload the next stubbed
 *        'xcb_get_property_reply' call answers with
 *
 * @c NULL @a s_fake_prop_value models the X server returning no
 * reply at all (as when a property is entirely absent); a non-null
 * one of length zero models a present, but empty, property.
 */
static const void *s_fake_prop_value;
static uint32_t s_fake_prop_value_len;


/**
 * @brief Link-only stand-in for @a xcb_get_property
 *
 * The real round-trip cookie is never a real request/reply pair
 * here, so its contents are never inspected; only its presence as a
 * distinct call site 'xcb_get_property_reply' below can pretend to
 * answer is what matters
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_get_property(xcb_connection_t *connection,
        uint8_t _delete, xcb_window_t window, xcb_atom_t property,
        xcb_atom_t type, uint32_t long_offset, uint32_t long_length)
{
    xcb_get_property_cookie_t cookie;

    (void) connection;
    (void) _delete;
    (void) window;
    (void) property;
    (void) type;
    (void) long_offset;
    (void) long_length;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Controllable stand-in for @a xcb_get_property_reply
 *
 * Answers with a heap-allocated reply wrapping whatever
 * @a s_fake_prop_value/@a s_fake_prop_value_len a test has set up, or
 * @c NULL when @a s_fake_prop_value itself is @c NULL, exactly as the
 * real function would on a failed request.  The real function's
 * contract has the caller @c free() the reply, so this allocates one
 * for real rather than returning a pointer into static storage.
 *
 * @note Complexity: @e O(n) in the fake value's length, to copy it
 *       into the allocated reply
 */
xcb_get_property_reply_t *xcb_get_property_reply(
        xcb_connection_t *connection, xcb_get_property_cookie_t cookie,
        xcb_generic_error_t **e)
{
    xcb_get_property_reply_t *reply;

    (void) connection;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }
    if (s_fake_prop_value == NULL) {
        return NULL;
    }

    reply = malloc(sizeof(*reply) + s_fake_prop_value_len);
    memset(reply, 0, sizeof(*reply));
    reply->value_len = s_fake_prop_value_len;
    if (s_fake_prop_value_len > 0u) {
        memcpy(reply + 1, s_fake_prop_value, s_fake_prop_value_len);
    }

    return reply;
}


/**
 * @brief Controllable stand-in for @a xcb_get_property_value
 *
 * Returns a pointer to the fake value bytes copied right after the
 * fixed reply header by @a xcb_get_property_reply above, matching the
 * real function's own layout convention closely enough for every
 * caller under test here, none of which cares about anything past
 * that.
 *
 * @note Complexity: @e O(1)
 */
void *xcb_get_property_value(const xcb_get_property_reply_t *reply)
{
    return (void *) (reply + 1);
}


/**
 * @brief Fake UTF-8 strings payload the next stubbed
 *        'xcb_ewmh_get_utf8_strings_reply' call answers with
 */
static const char *s_fake_utf8_strings;
static uint32_t s_fake_utf8_strings_len;
static uint8_t s_fake_utf8_strings_ok = 1u;


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_name
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_ewmh_get_wm_name(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Controllable stand-in for @a xcb_ewmh_get_utf8_strings_reply
 *
 * Answers with whatever @a s_fake_utf8_strings/@a
 * s_fake_utf8_strings_len a test has set up, returning
 * @a s_fake_utf8_strings_ok as the real function's own success
 * flag.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_ewmh_get_utf8_strings_reply(xcb_ewmh_connection_t *ewmh,
        xcb_get_property_cookie_t cookie,
        xcb_ewmh_get_utf8_strings_reply_t *data, xcb_generic_error_t **e)
{
    (void) ewmh;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }
    data->strings_len = s_fake_utf8_strings_len;
    data->strings = (char *) s_fake_utf8_strings;
    data->_reply = NULL;

    return s_fake_utf8_strings_ok;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_utf8_strings_reply_wipe
 *
 * Nothing here is ever actually allocated by the real
 * 'xcb_ewmh_get_utf8_strings_reply', since the stand-in above wires
 * 'data->strings' straight to test-owned static storage, so there is
 * nothing for this to free
 *
 * @note Complexity: @e O(1)
 */
void xcb_ewmh_get_utf8_strings_reply_wipe(
        xcb_ewmh_get_utf8_strings_reply_t *data)
{
    (void) data;
}


/**
 * @brief Link-only stand-in for @a atom_intern
 *
 * Reached only by 'client_props_refresh_role' and
 * 'client_props_refresh_colormap_windows', neither of which any test
 * here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) name;
    (void) only_if_exists;
    return 0u;
}


/**
 * @brief Link-only stand-in for @a safeflg_unset
 *
 * Reached only by 'client_props_refresh_normal_hints', which nothing
 * here calls.
 *
 * @note Complexity: @e O(1)
 */
int safeflg_unset(uint32_t *flags, uint32_t flag, uint32_t max_flags)
{
    (void) flags;
    (void) flag;
    (void) max_flags;
    return 0;
}


/**
 * @brief Link-only stand-in for @a xcb_connection_get
 *
 * Reached only by 'client_props_refresh_icon_name' and
 * 'client_props_refresh_name' (through
 * 's_client_read_legacy_name_prop'), neither of which any test here
 * calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_connection_get
 *
 * Reached only by 'client_props_refresh_icon_name' and
 * 'client_props_refresh_name', neither of which any test here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_icon_name
 *
 * Reached only by 'client_props_refresh_icon_name', which nothing
 * here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_ewmh_get_wm_icon_name(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_get_window_attributes
 *
 * Reached only by 'client_props_refresh_colormap_windows', which
 * nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_window_attributes_cookie_t xcb_get_window_attributes(
        xcb_connection_t *connection, xcb_window_t window)
{
    xcb_get_window_attributes_cookie_t cookie;

    (void) connection;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_get_window_attributes_reply
 *
 * Reached only by 'client_props_refresh_colormap_windows', which
 * nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_window_attributes_reply_t *xcb_get_window_attributes_reply(
        xcb_connection_t *connection,
        xcb_get_window_attributes_cookie_t cookie, xcb_generic_error_t **e)
{
    (void) connection;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }

    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_colormap_windows
 *
 * Reached only by 'client_props_refresh_colormap_windows', which
 * nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_icccm_get_wm_colormap_windows(
        xcb_connection_t *connection, xcb_window_t window,
        xcb_atom_t property)
{
    xcb_get_property_cookie_t cookie;

    (void) connection;
    (void) window;
    (void) property;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for
 *        @a xcb_icccm_get_wm_colormap_windows_reply
 *
 * Reached only by 'client_props_refresh_colormap_windows', which
 * nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_icccm_get_wm_colormap_windows_reply(
        xcb_connection_t *connection, xcb_get_property_cookie_t cookie,
        xcb_icccm_get_wm_colormap_windows_reply_t *windows,
        xcb_generic_error_t **e)
{
    (void) connection;
    (void) cookie;
    (void) windows;
    if (e != NULL) {
        *e = NULL;
    }

    return 0u;
}


/**
 * @brief Link-only stand-in for
 *        @a xcb_icccm_get_wm_colormap_windows_reply_wipe
 *
 * Reached only by 'client_props_refresh_colormap_windows', which
 * nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void xcb_icccm_get_wm_colormap_windows_reply_wipe(
        xcb_icccm_get_wm_colormap_windows_reply_t *windows)
{
    (void) windows;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_normal_hints
 *
 * Reached only by 'client_props_refresh_normal_hints', which nothing
 * here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_icccm_get_wm_normal_hints(
        xcb_connection_t *connection, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) connection;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_normal_hints_reply
 *
 * Reached only by 'client_props_refresh_normal_hints', which nothing
 * here calls.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_icccm_get_wm_normal_hints_reply(xcb_connection_t *connection,
        xcb_get_property_cookie_t cookie,
        xcb_size_hints_t *hints, xcb_generic_error_t **e)
{
    (void) connection;
    (void) cookie;
    (void) hints;
    if (e != NULL) {
        *e = NULL;
    }

    return 0u;
}


/* A null buffer is rejected without ever touching the X server */
static void s_test_wm_name_null_buffer_returns_zero(void)
{
    size_t result;

    s_fake_prop_value = "irrelevant";
    s_fake_prop_value_len = 10u;
    result = client_props_get_wm_name(NULL, 1u, NULL, 32u);

    TAP_EQ_INT((long) result, 0, "a null buffer returns a zero length");
}


/* A zero-size buffer is rejected without ever touching the X server */
static void s_test_wm_name_zero_size_returns_zero(void)
{
    char buffer[32];
    size_t result;

    s_fake_prop_value = "irrelevant";
    s_fake_prop_value_len = 10u;
    result = client_props_get_wm_name(NULL, 1u, buffer, 0u);

    TAP_EQ_INT((long) result, 0, "a zero-size buffer returns a zero"
            " length");
}


/* A present, non-empty property is copied verbatim and terminated */
static void s_test_wm_name_normal_read(void)
{
    char buffer[32];
    size_t result;

    s_fake_prop_value = "xterm";
    s_fake_prop_value_len = 5u;
    result = client_props_get_wm_name(NULL, 1u, buffer, sizeof(buffer));

    TAP_EQ_INT((long) result, 5,
            "a normal WM_NAME read reports its true length");
    TAP_EQ_STR(buffer, "xterm",
            "a normal WM_NAME read copies the name verbatim");
}


/* A missing property (no reply at all) clears the buffer to empty */
static void s_test_wm_name_missing_property_clears_buffer(void)
{
    char buffer[32] = "stale";
    size_t result;

    s_fake_prop_value = NULL;
    s_fake_prop_value_len = 0u;
    result = client_props_get_wm_name(NULL, 1u, buffer, sizeof(buffer));

    TAP_EQ_INT((long) result, 0,
            "a missing WM_NAME property reports a zero length");
    TAP_EQ_STR(buffer, "",
            "a missing WM_NAME property clears the buffer");
}


/* A property longer than the destination buffer is truncated to fit,
 * always leaving room for the terminator */
static void s_test_wm_name_truncates_to_buffer_size(void)
{
    char buffer[4];
    size_t result;

    s_fake_prop_value = "a much too long window title";
    s_fake_prop_value_len = 29u;
    result = client_props_get_wm_name(NULL, 1u, buffer, sizeof(buffer));

    TAP_EQ_INT((long) result, 3,
            "an oversized WM_NAME is truncated to buffer_sz - 1");
    TAP_EQ_STR(buffer, "a m",
            "the truncated WM_NAME keeps only what fits, still"
            " terminated");
}


/* A null ewmh connection is rejected without touching the buffer */
static void s_test_net_wm_name_null_ewmh_returns_zero(void)
{
    char buffer[32] = "stale";
    size_t result;

    s_fake_utf8_strings = "Firefox";
    s_fake_utf8_strings_len = 7u;
    s_fake_utf8_strings_ok = 1u;
    result = client_props_get_net_wm_name(NULL, 1u, buffer,
            sizeof(buffer));

    TAP_EQ_INT((long) result, 0,
            "a null EWMH connection returns a zero length");
    TAP_EQ_STR(buffer, "stale",
            "a null EWMH connection leaves the buffer untouched");
}


/* A null buffer is rejected without ever touching the X server */
static void s_test_net_wm_name_null_buffer_returns_zero(void)
{
    xcb_ewmh_connection_t ewmh;
    size_t result;

    memset(&ewmh, 0, sizeof(ewmh));
    s_fake_utf8_strings = "Firefox";
    s_fake_utf8_strings_len = 7u;
    s_fake_utf8_strings_ok = 1u;
    result = client_props_get_net_wm_name(&ewmh, 1u, NULL, 32u);

    TAP_EQ_INT((long) result, 0, "a null buffer returns a zero length");
}


/* A successful UTF-8 reply is copied verbatim and terminated */
static void s_test_net_wm_name_normal_read(void)
{
    xcb_ewmh_connection_t ewmh;
    char buffer[32];
    size_t result;

    memset(&ewmh, 0, sizeof(ewmh));
    s_fake_utf8_strings = "Firefox";
    s_fake_utf8_strings_len = 7u;
    s_fake_utf8_strings_ok = 1u;
    result = client_props_get_net_wm_name(&ewmh, 1u, buffer,
            sizeof(buffer));

    TAP_EQ_INT((long) result, 7,
            "a normal _NET_WM_NAME read reports its true length");
    TAP_EQ_STR(buffer, "Firefox",
            "a normal _NET_WM_NAME read copies the name verbatim");
}


/* A failed EWMH reply clears the buffer to empty, same as a missing
 * legacy property */
static void s_test_net_wm_name_failed_reply_clears_buffer(void)
{
    xcb_ewmh_connection_t ewmh;
    char buffer[32] = "stale";
    size_t result;

    memset(&ewmh, 0, sizeof(ewmh));
    s_fake_utf8_strings = NULL;
    s_fake_utf8_strings_len = 0u;
    s_fake_utf8_strings_ok = 0u;
    result = client_props_get_net_wm_name(&ewmh, 1u, buffer,
            sizeof(buffer));

    TAP_EQ_INT((long) result, 0,
            "a failed _NET_WM_NAME reply reports a zero length");
    TAP_EQ_STR(buffer, "",
            "a failed _NET_WM_NAME reply clears the buffer");
}


/* An oversized UTF-8 reply is truncated to fit the destination */
static void s_test_net_wm_name_truncates_to_buffer_size(void)
{
    xcb_ewmh_connection_t ewmh;
    char buffer[4];
    size_t result;

    memset(&ewmh, 0, sizeof(ewmh));
    s_fake_utf8_strings = "a much too long title";
    s_fake_utf8_strings_len = 22u;
    s_fake_utf8_strings_ok = 1u;
    result = client_props_get_net_wm_name(&ewmh, 1u, buffer,
            sizeof(buffer));

    TAP_EQ_INT((long) result, 3,
            "an oversized _NET_WM_NAME is truncated to buffer_sz - 1");
    TAP_EQ_STR(buffer, "a m",
            "the truncated _NET_WM_NAME keeps only what fits, still"
            " terminated");
}


/* A null class buffer is rejected */
static void s_test_wm_class_null_class_buf_fails(void)
{
    char inst[32];
    int result;

    s_fake_prop_value = "xterm\0XTerm";
    s_fake_prop_value_len = 11u;
    result = client_props_get_wm_class(NULL, 1u, NULL, 32u, inst,
            sizeof(inst));

    TAP_EQ_INT(result, -1, "a null class buffer fails with -1");
}


/* A zero-size class buffer is rejected */
static void s_test_wm_class_zero_class_sz_fails(void)
{
    char class_buf[32];
    int result;

    result = client_props_get_wm_class(NULL, 1u, class_buf, 0u, NULL,
            0u);

    TAP_EQ_INT(result, -1, "a zero-size class buffer fails with -1");
}


/* A non-null instance buffer with a zero size is rejected, since it
 * cannot be null-terminated safely */
static void s_test_wm_class_zero_inst_sz_with_inst_buf_fails(void)
{
    char class_buf[32];
    char inst_buf[32];
    int result;

    result = client_props_get_wm_class(NULL, 1u, class_buf,
            sizeof(class_buf), inst_buf, 0u);

    TAP_EQ_INT(result, -1,
            "a non-null instance buffer with a zero size fails with"
            " -1");
}


/* A normal 'WM_CLASS' property splits cleanly on its null separator
 * into instance and class */
static void s_test_wm_class_normal_read(void)
{
    char class_buf[32];
    char inst_buf[32];
    int result;

    s_fake_prop_value = "xterm\0XTerm";
    s_fake_prop_value_len = 11u;
    result = client_props_get_wm_class(NULL, 1u, class_buf,
            sizeof(class_buf), inst_buf, sizeof(inst_buf));

    TAP_EQ_INT(result, 0, "a normal WM_CLASS read succeeds");
    TAP_EQ_STR(inst_buf, "xterm",
            "the instance name is the text before the separator");
    TAP_EQ_STR(class_buf, "XTerm",
            "the class name is the text after the separator");
}


/* A missing 'WM_CLASS' property fails, and clears both buffers to
 * empty first */
static void s_test_wm_class_missing_property_fails(void)
{
    char class_buf[32] = "stale";
    char inst_buf[32] = "stale";
    int result;

    s_fake_prop_value = NULL;
    s_fake_prop_value_len = 0u;
    result = client_props_get_wm_class(NULL, 1u, class_buf,
            sizeof(class_buf), inst_buf, sizeof(inst_buf));

    TAP_EQ_INT(result, -1, "a missing WM_CLASS property fails with -1");
    TAP_EQ_STR(class_buf, "",
            "a missing WM_CLASS property still clears the class"
            " buffer");
    TAP_EQ_STR(inst_buf, "",
            "a missing WM_CLASS property still clears the instance"
            " buffer");
}


/* A 'WM_CLASS' property with no null separator at all is read
 * entirely as the class, with an empty instance */
static void s_test_wm_class_no_separator_is_all_class(void)
{
    char class_buf[32];
    char inst_buf[32] = "stale";
    int result;

    s_fake_prop_value = "nosplit";
    s_fake_prop_value_len = 7u;
    result = client_props_get_wm_class(NULL, 1u, class_buf,
            sizeof(class_buf), inst_buf, sizeof(inst_buf));

    TAP_EQ_INT(result, 0,
            "a WM_CLASS property with no separator still succeeds");
    TAP_EQ_STR(inst_buf, "",
            "with no separator, the whole value has no instance"
            " name at all, so it stays empty");
    TAP_EQ_STR(class_buf, "nosplit",
            "...and the whole value is read as the class, not a"
            " substring missing its first byte");
}


/* A null instance buffer is accepted; only the class is filled in */
static void s_test_wm_class_null_inst_buf_is_optional(void)
{
    char class_buf[32];
    int result;

    s_fake_prop_value = "xterm\0XTerm";
    s_fake_prop_value_len = 11u;
    result = client_props_get_wm_class(NULL, 1u, class_buf,
            sizeof(class_buf), NULL, 0u);

    TAP_EQ_INT(result, 0,
            "a null instance buffer is optional and still succeeds");
    TAP_EQ_STR(class_buf, "XTerm",
            "the class name is still filled in with no instance"
            " buffer at all");
}


/* Both instance and class names are truncated to fit their own
 * destination buffers */
static void s_test_wm_class_truncates_both_names(void)
{
    char class_buf[3];
    char inst_buf[3];
    int result;

    s_fake_prop_value = "instance\0classname";
    s_fake_prop_value_len = 18u;
    result = client_props_get_wm_class(NULL, 1u, class_buf,
            sizeof(class_buf), inst_buf, sizeof(inst_buf));

    TAP_EQ_INT(result, 0,
            "a WM_CLASS read still succeeds when both names must be"
            " truncated");
    TAP_EQ_STR(inst_buf, "in",
            "the instance name is truncated to its own buffer size");
    TAP_EQ_STR(class_buf, "cl",
            "the class name is truncated to its own buffer size");
}


/* The two name refreshes answer whether the value actually moved.
 * Some clients rewrite the very same title and icon name every few
 * seconds; the caller repaints on a change, and repainting to arrive
 * at what is already drawn clears the client's window for nothing */
static void s_test_name_refreshes_report_only_real_changes(void)
{
    client_td client;
    char name[CONFIG_MAX_LENGTH_NAME];
    char visible[CONFIG_MAX_LENGTH_NAME];
    char icon_name[CONFIG_MAX_LENGTH_NAME];

    memset(&client, 0, sizeof(client));
    client.info.name = name;
    client.info.visible_name = visible;
    client.icon_info.visible_icon_name = icon_name;
    name[0] = '\0';
    visible[0] = '\0';
    icon_name[0] = '\0';

    /* The EWMH stand-in answers NULL, so both take their legacy
     * 'WM_NAME'/'WM_ICON_NAME' path, fed by the property stub */
    s_fake_prop_value = "Inbox";
    s_fake_prop_value_len = 5u;

    TAP_OK(client_props_refresh_name(&client),
            "a first title is reported as a change");
    TAP_EQ_STR(client.info.name, "Inbox", "and is the one stored");
    TAP_OK(!client_props_refresh_name(&client),
            "the very same title again is reported as no change");

    TAP_OK(client_props_refresh_icon_name(&client),
            "a first icon name is reported as a change");
    TAP_OK(!client_props_refresh_icon_name(&client),
            "and the very same one again is not");

    s_fake_prop_value = "Inbox (1)";
    s_fake_prop_value_len = 9u;
    TAP_OK(client_props_refresh_name(&client),
            "a different title is reported as a change");
    TAP_EQ_STR(client.info.name, "Inbox (1)",
            "and replaces what was stored");
    TAP_OK(client_props_refresh_icon_name(&client),
            "and so is a different icon name");

    s_fake_prop_value = NULL;
    s_fake_prop_value_len = 0u;
}


int main(void)
{
    TAP_PLAN(42);

    s_test_wm_name_null_buffer_returns_zero();
    s_test_wm_name_zero_size_returns_zero();
    s_test_wm_name_normal_read();
    s_test_wm_name_missing_property_clears_buffer();
    s_test_wm_name_truncates_to_buffer_size();
    s_test_net_wm_name_null_ewmh_returns_zero();
    s_test_net_wm_name_null_buffer_returns_zero();
    s_test_net_wm_name_normal_read();
    s_test_net_wm_name_failed_reply_clears_buffer();
    s_test_net_wm_name_truncates_to_buffer_size();
    s_test_wm_class_null_class_buf_fails();
    s_test_wm_class_zero_class_sz_fails();
    s_test_wm_class_zero_inst_sz_with_inst_buf_fails();
    s_test_wm_class_normal_read();
    s_test_wm_class_missing_property_fails();
    s_test_wm_class_no_separator_is_all_class();
    s_test_wm_class_null_inst_buf_is_optional();
    s_test_wm_class_truncates_both_names();


    s_test_name_refreshes_report_only_real_changes();

    return TAP_DONE();
}
