/**
 * @file tests/cmds/client/test_meta.c
 *
 * @brief Test battery for a client's rename, 'WM_CLASS', window role,
 *        and icon name mutators
 *
 * Exercises 'ccmd_client_rename', 'ccmd_client_reclass',
 * 'ccmd_client_rerole', and 'ccmd_client_set_icon' (cmds/client/
 * meta.c) linked against the real 'safe_strdup'/'safe_strlen'
 * ('utils/safe/safestr.c'), so the exact string ownership each
 * function performs (freeing the previous value, duplicating the new
 * one) runs for real.  'xcb_connection_get', 'xcb_ewmh_connection_
 * get', 'xcb_change_property', 'xcb_ewmh_set_wm_name', 'xcb_ewmh_
 * set_wm_icon_name', 'xcb_icccm_set_wm_class', and 'atom_intern' are
 * all recording stand-ins: every one of them is a real X11 wire
 * request (or, for 'atom_intern', a round trip that blocks on one),
 * none of which can run without a live X server, so each is replaced
 * here with a version that only records what it was asked to send,
 * letting every scenario below assert on the exact bytes and window
 * each mutator handed to the display server instead.
 *
 * 'ccmd_client_rerole' additionally exercises the "'atom_intern'
 * returns 'XCB_ATOM_NONE'" guard clause, which no other function
 * under test here has an equivalent of.
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
#include <cmds/client/meta.h>
#include <harness/tap.h>
#include <utils/safe/safestr.h>
#include <utils/xcb/atom.h>


/** Recorded arguments of the most recent 'xcb_change_property' call,
 *  reset by 's_reset' before each scenario */
static int s_change_property_calls;
static xcb_window_t s_change_property_window;
static xcb_atom_t s_change_property_property;
static xcb_atom_t s_change_property_type;
static uint32_t s_change_property_data_len;
static char s_change_property_data[256];

/** Recorded arguments of the most recent 'xcb_ewmh_set_wm_name'
 *  call */
static int s_ewmh_set_wm_name_calls;
static xcb_window_t s_ewmh_set_wm_name_window;
static char s_ewmh_set_wm_name_data[256];

/** Recorded arguments of the most recent 'xcb_ewmh_set_wm_icon_name'
 *  call */
static int s_ewmh_set_wm_icon_name_calls;
static xcb_window_t s_ewmh_set_wm_icon_name_window;
static char s_ewmh_set_wm_icon_name_data[256];

/** Recorded arguments of the most recent 'xcb_icccm_set_wm_class'
 *  call */
static int s_icccm_set_wm_class_calls;
static xcb_window_t s_icccm_set_wm_class_window;
static uint32_t s_icccm_set_wm_class_len;
static char s_icccm_set_wm_class_data[256];

/** Recorded arguments of the most recent 'atom_intern' call, plus a
 *  test-controlled return value */
static int s_atom_intern_calls;
static char s_atom_intern_name[64];
static xcb_atom_t s_atom_intern_return;


/**
 * @brief Recording stand-in for @a xcb_connection_get
 *
 * The exact pointer returned is never dereferenced anywhere in this
 * file's stand-ins, so a fixed non-null sentinel is enough to prove
 * every caller under test received "a connection" without needing a
 * real one.
 *
 * @note Complexity: @e O(1)
 */
xcb_connection_t *xcb_connection_get(void)
{
    static int s_sentinel;
    return (xcb_connection_t *) &s_sentinel;
}


/**
 * @brief Recording stand-in for @a xcb_ewmh_connection_get
 *
 * See @a xcb_connection_get's own comment above for why a fixed
 * sentinel is enough here too.
 *
 * @note Complexity: @e O(1)
 */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    static int s_sentinel;
    return (xcb_ewmh_connection_t *) &s_sentinel;
}


/**
 * @brief Recording stand-in for @a xcb_change_property
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_change_property(xcb_connection_t *c, uint8_t mode,
        xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
        uint8_t format, uint32_t data_len, const void *data)
{
    xcb_void_cookie_t cookie = { 0 };
    size_t copy_len;

    (void) c;
    (void) mode;
    (void) format;

    s_change_property_calls++;
    s_change_property_window = window;
    s_change_property_property = property;
    s_change_property_type = type;
    s_change_property_data_len = data_len;

    copy_len = (data_len < sizeof(s_change_property_data) - 1u) ?
            data_len : sizeof(s_change_property_data) - 1u;
    memset(s_change_property_data, 0, sizeof(s_change_property_data));
    if (data != NULL && copy_len > 0u) {
        memcpy(s_change_property_data, data, copy_len);
    }

    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_wm_name
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_wm_name(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t strings_len, const char *strings)
{
    xcb_void_cookie_t cookie = { 0 };
    size_t copy_len;

    (void) ewmh;

    s_ewmh_set_wm_name_calls++;
    s_ewmh_set_wm_name_window = window;

    copy_len = (strings_len < sizeof(s_ewmh_set_wm_name_data) - 1u) ?
            strings_len : sizeof(s_ewmh_set_wm_name_data) - 1u;
    memset(s_ewmh_set_wm_name_data, 0, sizeof(s_ewmh_set_wm_name_data));
    if (strings != NULL && copy_len > 0u) {
        memcpy(s_ewmh_set_wm_name_data, strings, copy_len);
    }

    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_wm_icon_name
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_wm_icon_name(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t strings_len, const char *strings)
{
    xcb_void_cookie_t cookie = { 0 };
    size_t copy_len;

    (void) ewmh;

    s_ewmh_set_wm_icon_name_calls++;
    s_ewmh_set_wm_icon_name_window = window;

    copy_len = (strings_len < sizeof(s_ewmh_set_wm_icon_name_data) - 1u) ?
            strings_len : sizeof(s_ewmh_set_wm_icon_name_data) - 1u;
    memset(s_ewmh_set_wm_icon_name_data, 0,
            sizeof(s_ewmh_set_wm_icon_name_data));
    if (strings != NULL && copy_len > 0u) {
        memcpy(s_ewmh_set_wm_icon_name_data, strings, copy_len);
    }

    return cookie;
}


/**
 * @brief Recording stand-in for @a xcb_icccm_set_wm_class
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_icccm_set_wm_class(xcb_connection_t *c,
        xcb_window_t window, uint32_t class_len, const char *class_name)
{
    xcb_void_cookie_t cookie = { 0 };
    size_t copy_len;

    (void) c;

    s_icccm_set_wm_class_calls++;
    s_icccm_set_wm_class_window = window;
    s_icccm_set_wm_class_len = class_len;

    copy_len = (class_len < sizeof(s_icccm_set_wm_class_data) - 1u) ?
            class_len : sizeof(s_icccm_set_wm_class_data) - 1u;
    memset(s_icccm_set_wm_class_data, 0,
            sizeof(s_icccm_set_wm_class_data));
    if (class_name != NULL && copy_len > 0u) {
        memcpy(s_icccm_set_wm_class_data, class_name, copy_len);
    }

    return cookie;
}


/**
 * @brief Test-controlled stand-in for @a atom_intern
 *
 * Records the requested atom name and hands back whatever
 * 's_atom_intern_return' currently holds, so a scenario can drive
 * both the ordinary resolved-atom path and the
 * @c XCB_ATOM_NONE guard clause in 'ccmd_client_rerole' directly.
 *
 * @note Complexity: @e O(1)
 */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    (void) connection;
    (void) only_if_exists;

    s_atom_intern_calls++;
    memset(s_atom_intern_name, 0, sizeof(s_atom_intern_name));
    if (name != NULL) {
        (void) strncpy(s_atom_intern_name, name,
                sizeof(s_atom_intern_name) - 1u);
    }

    return s_atom_intern_return;
}


static void s_reset(void)
{
    s_change_property_calls = 0;
    s_change_property_window = 0;
    s_change_property_property = 0;
    s_change_property_type = 0;
    s_change_property_data_len = 0;
    memset(s_change_property_data, 0, sizeof(s_change_property_data));

    s_ewmh_set_wm_name_calls = 0;
    s_ewmh_set_wm_name_window = 0;
    memset(s_ewmh_set_wm_name_data, 0, sizeof(s_ewmh_set_wm_name_data));

    s_ewmh_set_wm_icon_name_calls = 0;
    s_ewmh_set_wm_icon_name_window = 0;
    memset(s_ewmh_set_wm_icon_name_data, 0,
            sizeof(s_ewmh_set_wm_icon_name_data));

    s_icccm_set_wm_class_calls = 0;
    s_icccm_set_wm_class_window = 0;
    s_icccm_set_wm_class_len = 0;
    memset(s_icccm_set_wm_class_data, 0,
            sizeof(s_icccm_set_wm_class_data));

    s_atom_intern_calls = 0;
    memset(s_atom_intern_name, 0, sizeof(s_atom_intern_name));
    s_atom_intern_return = (xcb_atom_t) 999;
}


static client_td *s_make_client(uint32_t id)
{
    client_td *client = calloc(1, sizeof(*client));

    client->id = (xcb_window_t) id;
    client->window = (xcb_window_t) id;
    return client;
}


/* ccmd_client_rename: NULL client or NULL name is a silent no-op */
static void s_test_rename_null_guards(void)
{
    client_td *client = s_make_client(1u);

    s_reset();
    ccmd_client_rename(NULL, "ignored");
    TAP_EQ_INT(s_change_property_calls, 0,
            "a NULL client leaves 'xcb_change_property' untouched");

    ccmd_client_rename(client, NULL);
    TAP_EQ_INT(s_change_property_calls, 0,
            "a NULL name leaves 'xcb_change_property' untouched too");

    free(client);
}


/* ccmd_client_rename: a valid rename frees the old name, duplicates
 * the new one, and pushes both 'WM_NAME' and '_NET_WM_NAME' */
static void s_test_rename_updates_name_and_properties(void)
{
    client_td *client = s_make_client(2u);

    client->info.name = safe_strdup("old title");

    s_reset();
    ccmd_client_rename(client, "new title");

    TAP_EQ_STR(client->info.name, "new title",
            "'info.name' is replaced with a fresh duplicate");
    TAP_EQ_INT(s_change_property_calls, 1,
            "'xcb_change_property' ran exactly once");
    TAP_EQ_INT((long) s_change_property_window, (long) client->window,
            "the property was targeted at the client's own window");
    TAP_EQ_INT((long) s_change_property_property,
            (long) XCB_ATOM_WM_NAME,
            "the property changed was 'WM_NAME'");
    TAP_EQ_INT((long) s_change_property_type, (long) XCB_ATOM_STRING,
            "the property's type was 'STRING'");
    TAP_EQ_STR(s_change_property_data, "new title",
            "the bytes sent were the new title itself");
    TAP_EQ_INT(s_ewmh_set_wm_name_calls, 1,
            "'xcb_ewmh_set_wm_name' ran exactly once too");
    TAP_EQ_INT((long) s_ewmh_set_wm_name_window, (long) client->window,
            "the EWMH name update targeted the same window");
    TAP_EQ_STR(s_ewmh_set_wm_name_data, "new title",
            "the EWMH name update carried the same new title");

    free(client->info.name);
    free(client);
}


/* ccmd_client_reclass: NULL client, class, or instance is a silent
 * no-op */
static void s_test_reclass_null_guards(void)
{
    client_td *client = s_make_client(3u);

    s_reset();
    ccmd_client_reclass(NULL, "cls", "inst");
    TAP_EQ_INT(s_icccm_set_wm_class_calls, 0,
            "a NULL client leaves 'xcb_icccm_set_wm_class' untouched");

    ccmd_client_reclass(client, NULL, "inst");
    TAP_EQ_INT(s_icccm_set_wm_class_calls, 0,
            "a NULL class name leaves it untouched too");

    ccmd_client_reclass(client, "cls", NULL);
    TAP_EQ_INT(s_icccm_set_wm_class_calls, 0,
            "a NULL instance name leaves it untouched as well");

    free(client);
}


/* ccmd_client_reclass: a valid call replaces both class name slots
 * and sends one combined, doubly null-terminated buffer per ICCCM
 * 4.1.2.5 */
static void s_test_reclass_updates_class_and_property(void)
{
    client_td *client = s_make_client(4u);

    client->info.class_name[0] = safe_strdup("OldClass");
    client->info.class_name[1] = safe_strdup("oldinst");

    s_reset();
    ccmd_client_reclass(client, "NewClass", "newinst");

    TAP_EQ_STR(client->info.class_name[0], "NewClass",
            "'info.class_name[0]' is replaced with the new class");
    TAP_EQ_STR(client->info.class_name[1], "newinst",
            "'info.class_name[1]' is replaced with the new instance");
    TAP_EQ_INT(s_icccm_set_wm_class_calls, 1,
            "'xcb_icccm_set_wm_class' ran exactly once");
    TAP_EQ_INT((long) s_icccm_set_wm_class_window, (long) client->window,
            "the class change targeted the client's own window");
    TAP_EQ_INT((long) s_icccm_set_wm_class_len,
            (long) (strlen("NewClass") + 1u + strlen("newinst") + 1u),
            "the combined buffer's length covers both strings plus" \
            " both null terminators");
    TAP_EQ_STR(s_icccm_set_wm_class_data, "NewClass",
            "the first null-terminated string in the buffer is the" \
            " new class name");
    TAP_EQ_STR(s_icccm_set_wm_class_data + strlen("NewClass") + 1u,
            "newinst",
            "the second null-terminated string, right after the" \
            " first one's own terminator, is the new instance name");

    free(client->info.class_name[0]);
    free(client->info.class_name[1]);
    free(client);
}


/* ccmd_client_rerole: NULL client or NULL role is a silent no-op */
static void s_test_rerole_null_guards(void)
{
    client_td *client = s_make_client(5u);

    s_reset();
    ccmd_client_rerole(NULL, "role");
    TAP_EQ_INT(s_atom_intern_calls, 0,
            "a NULL client leaves 'atom_intern' untouched");

    ccmd_client_rerole(client, NULL);
    TAP_EQ_INT(s_atom_intern_calls, 0,
            "a NULL role leaves 'atom_intern' untouched too");

    free(client);
}


/* ccmd_client_rerole: a resolved atom updates 'role_name' and sends
 * the property */
static void s_test_rerole_updates_role_and_property(void)
{
    client_td *client = s_make_client(6u);

    client->info.role_name = safe_strdup("old-role");

    s_reset();
    s_atom_intern_return = (xcb_atom_t) 4242;
    ccmd_client_rerole(client, "new-role");

    TAP_EQ_STR(client->info.role_name, "new-role",
            "'info.role_name' is replaced with a fresh duplicate");
    TAP_EQ_INT(s_atom_intern_calls, 1,
            "'atom_intern' ran exactly once to resolve" \
            " 'WM_WINDOW_ROLE'");
    TAP_EQ_STR(s_atom_intern_name, "WM_WINDOW_ROLE",
            "the atom name requested was 'WM_WINDOW_ROLE' itself");
    TAP_EQ_INT(s_change_property_calls, 1,
            "'xcb_change_property' ran once the atom resolved");
    TAP_EQ_INT((long) s_change_property_property,
            (long) s_atom_intern_return,
            "the property changed was the atom 'atom_intern' handed" \
            " back");
    TAP_EQ_STR(s_change_property_data, "new-role",
            "the bytes sent were the new role name itself");

    free(client->info.role_name);
    free(client);
}


/* ccmd_client_rerole: role_name is still replaced even when
 * 'atom_intern' cannot resolve 'WM_WINDOW_ROLE' at all, but no
 * property change is ever attempted in that case */
static void s_test_rerole_atom_none_guard(void)
{
    client_td *client = s_make_client(7u);

    s_reset();
    s_atom_intern_return = XCB_ATOM_NONE;
    ccmd_client_rerole(client, "unreachable-role");

    TAP_EQ_STR(client->info.role_name, "unreachable-role",
            "'info.role_name' is still replaced before the atom" \
            " lookup's own result is even known");
    TAP_EQ_INT(s_change_property_calls, 0,
            "'XCB_ATOM_NONE' short-circuits before any property" \
            " change is attempted");

    free(client->info.role_name);
    free(client);
}


/* ccmd_client_set_icon: NULL client or NULL icon name is a silent
 * no-op */
static void s_test_set_icon_null_guards(void)
{
    client_td *client = s_make_client(8u);

    s_reset();
    ccmd_client_set_icon(NULL, "icon");
    TAP_EQ_INT(s_change_property_calls, 0,
            "a NULL client leaves 'xcb_change_property' untouched");

    ccmd_client_set_icon(client, NULL);
    TAP_EQ_INT(s_change_property_calls, 0,
            "a NULL icon name leaves it untouched too");

    free(client);
}


/* ccmd_client_set_icon: a valid call pushes both 'WM_ICON_NAME' and
 * '_NET_WM_ICON_NAME', without touching 'info' at all (unlike the
 * other three mutators, this one keeps no local copy of its own) */
static void s_test_set_icon_updates_both_properties(void)
{
    client_td *client = s_make_client(9u);

    s_reset();
    ccmd_client_set_icon(client, "new-icon");

    TAP_EQ_INT(s_change_property_calls, 1,
            "'xcb_change_property' ran exactly once");
    TAP_EQ_INT((long) s_change_property_property,
            (long) XCB_ATOM_WM_ICON_NAME,
            "the property changed was 'WM_ICON_NAME'");
    TAP_EQ_STR(s_change_property_data, "new-icon",
            "the bytes sent were the new icon name itself");
    TAP_EQ_INT(s_ewmh_set_wm_icon_name_calls, 1,
            "'xcb_ewmh_set_wm_icon_name' ran exactly once too");
    TAP_EQ_INT((long) s_ewmh_set_wm_icon_name_window,
            (long) client->window,
            "the EWMH icon name update targeted the same window");
    TAP_EQ_STR(s_ewmh_set_wm_icon_name_data, "new-icon",
            "the EWMH icon name update carried the same new name");

    free(client);
}


int main(void)
{
    TAP_PLAN(39);

    s_test_rename_null_guards();
    s_test_rename_updates_name_and_properties();
    s_test_reclass_null_guards();
    s_test_reclass_updates_class_and_property();
    s_test_rerole_null_guards();
    s_test_rerole_updates_role_and_property();
    s_test_rerole_atom_none_guard();
    s_test_set_icon_null_guards();
    s_test_set_icon_updates_both_properties();

    return TAP_DONE();
}
