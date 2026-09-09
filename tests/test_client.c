/**
 * @file tests/test_client.c
 *
 * @brief Test battery for user-time tracking and visible-name
 *        truncation bookkeeping
 *
 * Exercises 'client_note_user_time', 'client_last_user_time',
 * 'client_update_user_time' and 'client_sync_visible_name'
 * (client.c) linked against the real source file, since none of the
 * four lives anywhere extractable on its own.  Every other external
 * symbol 'client.c' pulls in is a link-only stand-in below, standing
 * in for a whole client lifecycle (creation, decoration, property
 * refresh, teardown) that no test here ever drives; the real
 * 'safe_strcmp'/'safe_strncpy'/'safe_strlen' are linked for real
 * instead of stubbed, since 'client_sync_visible_name' depends on
 * their exact truncation semantics.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/sync.h>

/* Local includes */
#include <client.h>
#include <client/props.h>
#include <config.h>
#include <defs/config.h>
#include <harness/tap.h>
#include <logger.h>
#include <render/wmicon.h>


/**
 * @brief Link-only stand-in for @a atom_intern
 *
 * Reached only by client creation and colormap-window refresh paths,
 * neither of which any test here calls.
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
 * @brief Link-only stand-in for @a atom_set_window_opacity
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw)
{
    (void) connection;
    (void) window;
    (void) raw;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_grab_buttons
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_grab_buttons(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_sync_states
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_sync_states(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a ccmd_client_update_allowed_actions
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_update_allowed_actions(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a ccmd_set_wm_state
 *
 * Reached only by client creation and teardown, neither of which any
 * test here calls.
 *
 * @note Complexity: @e O(1)
 */
void ccmd_set_wm_state(client_td *client, uint32_t state,
        xcb_window_t icon_window)
{
    (void) client;
    (void) state;
    (void) icon_window;
}


/**
 * @brief Link-only stand-in for @a ci_alloc_strings
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
int ci_alloc_strings(client_td *client)
{
    (void) client;
    return 0;
}


/**
 * @brief Link-only stand-in for @a ci_create_decorations
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
int ci_create_decorations(client_td *client)
{
    (void) client;
    return 0;
}


/**
 * @brief Link-only stand-in for @a ci_set_decoration_defaults
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void ci_set_decoration_defaults(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a client_props_get_net_wm_name
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
size_t client_props_get_net_wm_name(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, char *buffer, size_t buffer_sz)
{
    (void) ewmh;
    (void) window;
    (void) buffer;
    (void) buffer_sz;
    return 0u;
}


/**
 * @brief Link-only stand-in for @a client_props_get_wm_class
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
int client_props_get_wm_class(xcb_connection_t *connection,
        xcb_window_t window,
        char *restrict class_buf, size_t class_sz,
        char *restrict inst_buf, size_t inst_sz)
{
    (void) connection;
    (void) window;
    (void) class_buf;
    (void) class_sz;
    (void) inst_buf;
    (void) inst_sz;
    return -1;
}


/**
 * @brief Link-only stand-in for @a client_props_get_wm_name
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
size_t client_props_get_wm_name(xcb_connection_t *connection,
        xcb_window_t window, char *buffer, size_t buffer_sz)
{
    (void) connection;
    (void) window;
    (void) buffer;
    (void) buffer_sz;
    return 0u;
}


/**
 * @brief Link-only stand-in for @a client_props_refresh_colormap_windows
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void client_props_refresh_colormap_windows(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a client_props_refresh_icon_name
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
bool client_props_refresh_icon_name(client_td *client)
{
    (void) client;
    return false;
}


/**
 * @brief Link-only stand-in for @a client_props_refresh_normal_hints
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void client_props_refresh_normal_hints(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a client_unlink_transient
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void client_unlink_transient(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a config_theme_opacity_to_raw
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint32_t config_theme_opacity_to_raw(uint8_t percent)
{
    (void) percent;
    return 0u;
}


/**
 * @brief Link-only stand-in for @a cycle_notice_client_destroyed
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void cycle_notice_client_destroyed(const client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a wincmenu_notice_client_destroyed
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void wincmenu_notice_client_destroyed(const client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a winlist_notice_client_destroyed
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void winlist_notice_client_destroyed(const client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a iconmenu_notice_client_destroyed
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void iconmenu_notice_client_destroyed(const client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a focus_order_add
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void focus_order_add(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a focus_order_remove
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void focus_order_remove(const client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a gethostname
 *
 * Reached only by client creation (to compare against
 * @c WM_CLIENT_MACHINE), which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
int gethostname(char *name, size_t len)
{
    (void) len;
    name[0] = '\0';
    return 0;
}


/**
 * @brief Link-only stand-in for @a logger_msg
 *
 * Reached only by error paths in client creation and teardown,
 * neither of which any test here calls.
 *
 * @note Complexity: @e O(1)
 */
int logger_msg(enum logger_level_e level, const char *restrict prefix,
        const char *restrict fmt, ...)
{
    (void) level;
    (void) prefix;
    (void) fmt;
    return 0;
}


/**
 * @brief Link-only stand-in for @a mouse_plain_cursor
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_cursor_t mouse_plain_cursor(void)
{
    return 0u;
}


/**
 * @brief Link-only stand-in for @a safe_free_var
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
int safe_free_var(void **first, ...)
{
    (void) first;
    return 0;
}


/**
 * @brief Link-only stand-in for @a safeflg_set
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
int safeflg_set(uint32_t *flags, uint32_t flag, uint32_t max_flags)
{
    (void) flags;
    (void) flag;
    (void) max_flags;
    return 0;
}


/**
 * @brief Link-only stand-in for @a safeflg_unset
 *
 * Reached only by client creation, which nothing here calls.
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
 * @brief Link-only stand-in for @a scratchpad_notice_client_created
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void scratchpad_notice_client_created(client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a scratchpad_notice_client_destroyed
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void scratchpad_notice_client_destroyed(const client_td *client)
{
    (void) client;
}


/**
 * @brief Link-only stand-in for @a stacking_remove
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
int stacking_remove(const client_td *client)
{
    (void) client;
    return 0;
}


/**
 * @brief Link-only stand-in for @a wm_sync_is_available
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
bool wm_sync_is_available(void)
{
    return false;
}


/**
 * @brief Link-only stand-in for @a wmicon_invalidate
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void wmicon_invalidate(xcb_connection_t *connection,
        wmicon_cache_td *cache)
{
    (void) connection;
    (void) cache;
}


/**
 * @brief Link-only stand-in for @a xcb_change_window_attributes
 *
 * Reached only by @a client_subscribe_colormap_windows, which nothing
 * here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_change_window_attributes(xcb_connection_t *c,
        xcb_window_t window, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) window;
    (void) value_mask;
    (void) value_list;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_configure_window
 *
 * Reached only by client geometry paths, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_configure_window(xcb_connection_t *c,
        xcb_window_t window, uint16_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) window;
    (void) value_mask;
    (void) value_list;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Controllable stand-in for @a xcb_connection_get
 *
 * A test that needs 'client_sync_visible_name' to reach its real
 * work sets @a s_fake_connection_present to @c true first, so this
 * answers with a non-null, if fake, connection pointer; every other
 * test leaves it @c false, matching the real function's own
 * "no connection yet" answer before @c wm_init runs.
 *
 * @note Complexity: @e O(1)
 */
static bool s_fake_connection_present;
static int s_fake_connection_token;

xcb_connection_t *xcb_connection_get(void)
{
    return (s_fake_connection_present)
        ? (xcb_connection_t *) &s_fake_connection_token : NULL;
}


/**
 * @brief Controllable stand-in for @a xcb_ewmh_connection_get
 *
 * A test that needs 'client_sync_visible_name' to reach its real work
 * sets @a s_fake_ewmh_present to @c true first, so this answers with
 * a non-null, if fake, EWMH connection pointer; every other test
 * leaves it @c false, exercising the null-guard path instead.
 *
 * @note Complexity: @e O(1)
 */
static bool s_fake_ewmh_present;
static xcb_ewmh_connection_t s_fake_ewmh;

xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return (s_fake_ewmh_present) ? &s_fake_ewmh : NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_atoms_reply_wipe
 *
 * Reached only by client property refresh, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void xcb_ewmh_get_atoms_reply_wipe(xcb_ewmh_get_atoms_reply_t *data)
{
    (void) data;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_cardinal_reply
 *
 * Reached only by client property refresh, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_ewmh_get_cardinal_reply(xcb_ewmh_connection_t *ewmh,
        xcb_get_property_cookie_t cookie, uint32_t *cardinal,
        xcb_generic_error_t **e)
{
    (void) ewmh;
    (void) cookie;
    (void) cardinal;
    if (e != NULL) {
        *e = NULL;
    }

    return 0u;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_pid
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_ewmh_get_wm_pid(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_state
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_ewmh_get_wm_state(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_strut
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_ewmh_get_wm_strut(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_strut_reply
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_ewmh_get_wm_strut_reply(xcb_ewmh_connection_t *ewmh,
        xcb_get_property_cookie_t cookie,
        xcb_ewmh_get_extents_reply_t *struts, xcb_generic_error_t **e)
{
    (void) ewmh;
    (void) cookie;
    (void) struts;
    if (e != NULL) {
        *e = NULL;
    }

    return 0u;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_strut_partial
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_ewmh_get_wm_strut_partial(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_strut_partial_reply
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_ewmh_get_wm_strut_partial_reply(xcb_ewmh_connection_t *ewmh,
        xcb_get_property_cookie_t cookie,
        xcb_ewmh_wm_strut_partial_t *struts, xcb_generic_error_t **e)
{
    (void) ewmh;
    (void) cookie;
    (void) struts;
    if (e != NULL) {
        *e = NULL;
    }

    return 0u;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_user_time
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_ewmh_get_wm_user_time(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_user_time_window
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_ewmh_get_wm_user_time_window(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_window_type
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_ewmh_get_wm_window_type(
        xcb_ewmh_connection_t *ewmh, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) ewmh;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_ewmh_get_wm_window_type_reply
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_ewmh_get_wm_window_type_reply(xcb_ewmh_connection_t *ewmh,
        xcb_get_property_cookie_t cookie,
        xcb_ewmh_get_atoms_reply_t *name, xcb_generic_error_t **e)
{
    (void) ewmh;
    (void) cookie;
    (void) name;
    if (e != NULL) {
        *e = NULL;
    }

    return 0u;
}


/**
 * @brief Recording stand-in for @a xcb_ewmh_set_wm_state
 *
 * Captures whether it was called at all and, if so, its @p window and
 * @p list_len, so a test on @a client_sync_visible_name's sibling
 * paths could inspect them; unused by the truncation tests
 * themselves, which instead pass their own @c set_fn stand-in
 * directly, but still needed to satisfy the link.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_ewmh_set_wm_state(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t list_len, xcb_atom_t *list)
{
    xcb_void_cookie_t cookie;

    (void) ewmh;
    (void) window;
    (void) list_len;
    (void) list;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_generate_id
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint32_t xcb_generate_id(xcb_connection_t *c)
{
    (void) c;
    return 0u;
}


/**
 * @brief Link-only stand-in for @a xcb_get_geometry
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_geometry_cookie_t xcb_get_geometry(xcb_connection_t *c,
        xcb_drawable_t drawable)
{
    xcb_get_geometry_cookie_t cookie;

    (void) c;
    (void) drawable;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_get_geometry_reply
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_geometry_reply_t *xcb_get_geometry_reply(xcb_connection_t *c,
        xcb_get_geometry_cookie_t cookie, xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }

    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_get_property
 *
 * Reached only by client property refresh paths, which nothing here
 * calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_get_property(xcb_connection_t *c,
        uint8_t _delete, xcb_window_t window, xcb_atom_t property,
        xcb_atom_t type, uint32_t long_offset, uint32_t long_length)
{
    xcb_get_property_cookie_t cookie;

    (void) c;
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
 * @brief Link-only stand-in for @a xcb_get_property_reply
 *
 * Reached only by client property refresh paths, which nothing here
 * calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_reply_t *xcb_get_property_reply(xcb_connection_t *c,
        xcb_get_property_cookie_t cookie, xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }

    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_get_property_value
 *
 * Reached only by client property refresh paths, which nothing here
 * calls.
 *
 * @note Complexity: @e O(1)
 */
void *xcb_get_property_value(const xcb_get_property_reply_t *r)
{
    (void) r;
    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_get_property_value_length
 *
 * Reached only by client property refresh paths, which nothing here
 * calls.
 *
 * @note Complexity: @e O(1)
 */
int xcb_get_property_value_length(const xcb_get_property_reply_t *r)
{
    (void) r;
    return 0;
}


/**
 * @brief Link-only stand-in for @a xcb_get_window_attributes
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_window_attributes_cookie_t xcb_get_window_attributes(
        xcb_connection_t *c, xcb_window_t window)
{
    xcb_get_window_attributes_cookie_t cookie;

    (void) c;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_get_window_attributes_reply
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_window_attributes_reply_t *xcb_get_window_attributes_reply(
        xcb_connection_t *c, xcb_get_window_attributes_cookie_t cookie,
        xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    if (e != NULL) {
        *e = NULL;
    }

    return NULL;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_text_property_reply_wipe
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void xcb_icccm_get_text_property_reply_wipe(
        xcb_icccm_get_text_property_reply_t *prop)
{
    (void) prop;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_client_machine
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_icccm_get_wm_client_machine(
        xcb_connection_t *c, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) c;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_client_machine_reply
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_icccm_get_wm_client_machine_reply(xcb_connection_t *c,
        xcb_get_property_cookie_t cookie,
        xcb_icccm_get_text_property_reply_t *prop, xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    (void) prop;
    if (e != NULL) {
        *e = NULL;
    }

    return 0u;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_hints
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_icccm_get_wm_hints(xcb_connection_t *c,
        xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) c;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_hints_reply
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_icccm_get_wm_hints_reply(xcb_connection_t *c,
        xcb_get_property_cookie_t cookie, xcb_icccm_wm_hints_t *hints,
        xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    (void) hints;
    if (e != NULL) {
        *e = NULL;
    }

    return 0u;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_protocols
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_icccm_get_wm_protocols(xcb_connection_t *c,
        xcb_window_t window, xcb_atom_t wm_protocol_atom)
{
    xcb_get_property_cookie_t cookie;

    (void) c;
    (void) window;
    (void) wm_protocol_atom;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_protocols_reply
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_icccm_get_wm_protocols_reply(xcb_connection_t *c,
        xcb_get_property_cookie_t cookie,
        xcb_icccm_get_wm_protocols_reply_t *protocols,
        xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    (void) protocols;
    if (e != NULL) {
        *e = NULL;
    }

    return 0u;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_protocols_reply_wipe
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void xcb_icccm_get_wm_protocols_reply_wipe(
        xcb_icccm_get_wm_protocols_reply_t *protocols)
{
    (void) protocols;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_transient_for
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_get_property_cookie_t xcb_icccm_get_wm_transient_for(
        xcb_connection_t *c, xcb_window_t window)
{
    xcb_get_property_cookie_t cookie;

    (void) c;
    (void) window;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_icccm_get_wm_transient_for_reply
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
uint8_t xcb_icccm_get_wm_transient_for_reply(xcb_connection_t *c,
        xcb_get_property_cookie_t cookie, xcb_window_t *prop,
        xcb_generic_error_t **e)
{
    (void) c;
    (void) cookie;
    (void) prop;
    if (e != NULL) {
        *e = NULL;
    }

    return 0u;
}


/**
 * @brief Recording stand-in for @a xcb_delete_property
 *
 * Captures the last @p window and @p property it was called with, so
 * a test on the "no longer truncated" branch of
 * 'client_sync_visible_name' can confirm it fired, and on exactly
 * which atom.
 *
 * @note Complexity: @e O(1)
 */
static xcb_window_t s_deleted_property_window;
static xcb_atom_t s_deleted_property_atom;
static int s_delete_property_call_count;

xcb_void_cookie_t xcb_delete_property(xcb_connection_t *c,
        xcb_window_t window, xcb_atom_t property)
{
    xcb_void_cookie_t cookie;

    (void) c;
    s_deleted_property_window = window;
    s_deleted_property_atom = property;
    ++s_delete_property_call_count;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_sync_create_alarm
 *
 * Reached only by client creation, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_sync_create_alarm(xcb_connection_t *c,
        xcb_sync_alarm_t id, uint32_t value_mask, const void *value_list)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) id;
    (void) value_mask;
    (void) value_list;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_sync_destroy_alarm
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
xcb_void_cookie_t xcb_sync_destroy_alarm(xcb_connection_t *c,
        xcb_sync_alarm_t alarm)
{
    xcb_void_cookie_t cookie;

    (void) c;
    (void) alarm;
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Link-only stand-in for @a xcb_window_destroy
 *
 * Reached only by client teardown, which nothing here calls.
 *
 * @note Complexity: @e O(1)
 */
void xcb_window_destroy(xcb_window_t window)
{
    (void) window;
}


/**
 * @brief Recording stand-in for a @c set_fn callback passed to
 *        @a client_sync_visible_name
 *
 * Stands in for whichever real EWMH setter (e.g.,
 * @c xcb_ewmh_set_wm_visible_name) a real caller passes, capturing
 * every argument it was invoked with instead of ever touching the X
 * server.
 *
 * @note Complexity: @e O(1)
 */
static int s_set_fn_call_count;
static xcb_window_t s_set_fn_window;
static uint32_t s_set_fn_list_len;
static char s_set_fn_text[CONFIG_MAX_LENGTH_NAME];

static xcb_void_cookie_t s_recording_set_fn(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, uint32_t list_len, const char *text)
{
    xcb_void_cookie_t cookie;

    (void) ewmh;
    ++s_set_fn_call_count;
    s_set_fn_window = window;
    s_set_fn_list_len = list_len;
    strncpy(s_set_fn_text, text, sizeof(s_set_fn_text) - 1u);
    s_set_fn_text[sizeof(s_set_fn_text) - 1u] = '\0';
    memset(&cookie, 0, sizeof(cookie));

    return cookie;
}


/**
 * @brief Reset every recording stand-in's captured state
 *
 * Run before each 'client_sync_visible_name' case, so one test's
 * calls into the recording stand-ins never leak into the next.
 *
 * @note Complexity: @e O(1)
 */
static void s_reset_recorders(void)
{
    s_set_fn_call_count = 0;
    s_set_fn_window = 0u;
    s_set_fn_list_len = 0u;
    s_set_fn_text[0] = '\0';
    s_deleted_property_window = 0u;
    s_deleted_property_atom = 0u;
    s_delete_property_call_count = 0;
}


/* A strictly newer timestamp updates the last-seen user time */
static void s_test_note_user_time_newer_updates(void)
{
    client_note_user_time(100u);
    TAP_EQ_INT((long) client_last_user_time(), 100,
            "a first, newer timestamp updates the last seen one");

    client_note_user_time(200u);
    TAP_EQ_INT((long) client_last_user_time(), 200,
            "a second, still newer timestamp updates it again");
}


/* An older or equal timestamp is ignored, never moving time backward */
static void s_test_note_user_time_older_is_ignored(void)
{
    client_note_user_time(500u);
    client_note_user_time(400u);
    TAP_EQ_INT((long) client_last_user_time(), 500,
            "an older timestamp never moves the last seen one"
            " backward");

    client_note_user_time(500u);
    TAP_EQ_INT((long) client_last_user_time(), 500,
            "an equal timestamp leaves the last seen one unchanged");
}


/* A null client is a no-op, never a crash */
static void s_test_update_user_time_null_client_is_a_no_op(void)
{
    client_update_user_time(NULL, 42u);
    TAP_OK(true, "a null client is a no-op, never a crash");
}


/* A strictly newer timestamp updates just this one client's own
 * user_time, independent of the global last-seen one */
static void s_test_update_user_time_newer_updates_client(void)
{
    client_td *client = calloc(1, sizeof(*client));

    client->user_time = 10u;
    client_update_user_time(client, 20u);

    TAP_EQ_INT((long) client->user_time, 20,
            "a newer timestamp updates this client's own user_time");

    free(client);
}


/* An older timestamp leaves a client's own user_time untouched */
static void s_test_update_user_time_older_is_ignored(void)
{
    client_td *client = calloc(1, sizeof(*client));

    client->user_time = 100u;
    client_update_user_time(client, 30u);

    TAP_EQ_INT((long) client->user_time, 100,
            "an older timestamp leaves this client's own user_time"
            " untouched");

    free(client);
}


/* A null client, ewmh connection, cached/full_name/rendered buffer or
 * set_fn is a no-op, never a crash, and never touches the cached
 * buffer */
static void s_test_sync_visible_name_null_args_are_a_no_op(void)
{
    client_td *client = calloc(1, sizeof(*client));
    char cached[CONFIG_MAX_LENGTH_NAME] = "stale";

    s_reset_recorders();
    s_fake_ewmh_present = false;
    client_sync_visible_name(client, cached, "full", "full",
            s_recording_set_fn, 1u);
    TAP_EQ_STR(cached, "stale",
            "a null EWMH connection leaves the cached buffer"
            " untouched");
    TAP_EQ_INT(s_set_fn_call_count, 0,
            "a null EWMH connection never invokes the set_fn");

    s_fake_ewmh_present = true;
    client_sync_visible_name(NULL, cached, "full", "full",
            s_recording_set_fn, 1u);
    TAP_EQ_STR(cached, "stale",
            "a null client leaves the cached buffer untouched");

    client_sync_visible_name(client, NULL, "full", "full",
            s_recording_set_fn, 1u);
    client_sync_visible_name(client, cached, NULL, "full",
            s_recording_set_fn, 1u);
    client_sync_visible_name(client, cached, "full", NULL,
            s_recording_set_fn, 1u);
    client_sync_visible_name(client, cached, "full", "full", NULL, 1u);
    TAP_EQ_STR(cached, "stale",
            "a null cached/full_name/rendered/set_fn argument leaves"
            " the cached buffer untouched");
    TAP_EQ_INT(s_set_fn_call_count, 0,
            "no null-argument case ever invokes the set_fn");

    s_fake_ewmh_present = false;
    free(client);
}


/* When the rendered text was actually truncated (differs from the
 * full name), the cached buffer is updated to the rendered text and
 * the property is set through 'set_fn' */
static void s_test_sync_visible_name_truncated_sets_property(void)
{
    client_td *client = calloc(1, sizeof(*client));
    char cached[CONFIG_MAX_LENGTH_NAME] = "";

    client->window = 777u;
    s_reset_recorders();
    s_fake_ewmh_present = true;

    client_sync_visible_name(client, cached, "A Very Long Window Title",
            "A Very Long...", s_recording_set_fn, 42u);

    TAP_EQ_STR(cached, "A Very Long...",
            "a truncated rendered name is cached verbatim");
    TAP_EQ_INT(s_set_fn_call_count, 1,
            "a truncated rendered name invokes the set_fn exactly"
            " once");
    TAP_EQ_INT((long) s_set_fn_window, 777,
            "the set_fn is invoked on the client's own window");
    TAP_EQ_STR(s_set_fn_text, "A Very Long...",
            "the set_fn receives the truncated rendered text");
    TAP_EQ_INT(s_delete_property_call_count, 0,
            "a truncated rendered name never deletes the property");

    s_fake_ewmh_present = false;
    free(client);
}


/* When the cached buffer already holds the same rendered text, a
 * still-truncated update is a no-op: the set_fn is not invoked again */
static void s_test_sync_visible_name_truncated_already_cached_is_a_no_op(void)
{
    client_td *client = calloc(1, sizeof(*client));
    char cached[CONFIG_MAX_LENGTH_NAME] = "Same Truncated...";

    s_reset_recorders();
    s_fake_ewmh_present = true;

    client_sync_visible_name(client, cached, "Same Truncated Full Name",
            "Same Truncated...", s_recording_set_fn, 42u);

    TAP_EQ_INT(s_set_fn_call_count, 0,
            "an already-cached, still-truncated name never invokes"
            " the set_fn again");
    TAP_EQ_STR(cached, "Same Truncated...",
            "an already-cached, still-truncated name leaves the"
            " cached buffer as it was");

    s_fake_ewmh_present = false;
    free(client);
}


/* When the rendered text matches the full name (no longer truncated),
 * the cached buffer is updated to the full name and the property is
 * deleted rather than set */
static void s_test_sync_visible_name_untruncated_deletes_property(void)
{
    client_td *client = calloc(1, sizeof(*client));
    char cached[CONFIG_MAX_LENGTH_NAME] = "A Very Long...";

    client->window = 555u;
    s_reset_recorders();
    s_fake_ewmh_present = true;
    s_fake_connection_present = true;

    client_sync_visible_name(client, cached, "Short Title", "Short Title",
            s_recording_set_fn, 99u);

    TAP_EQ_STR(cached, "Short Title",
            "an untruncated rendered name is cached as the full name");
    TAP_EQ_INT(s_set_fn_call_count, 0,
            "an untruncated rendered name never invokes the set_fn");
    TAP_EQ_INT(s_delete_property_call_count, 1,
            "an untruncated rendered name deletes the property"
            " exactly once");
    TAP_EQ_INT((long) s_deleted_property_window, 555,
            "the deleted property targets the client's own window");
    TAP_EQ_INT((long) s_deleted_property_atom, 99,
            "the deleted property is the one the caller named");

    s_fake_connection_present = false;
    s_fake_ewmh_present = false;
    free(client);
}


/* When the rendered text already matches both the full name and the
 * cached buffer, an untruncated update is a no-op: the property is
 * never deleted again */
static void
    s_test_sync_visible_name_untruncated_already_cached_is_a_no_op(void)
{
    client_td *client = calloc(1, sizeof(*client));
    char cached[CONFIG_MAX_LENGTH_NAME] = "Same Title";

    s_reset_recorders();
    s_fake_ewmh_present = true;
    s_fake_connection_present = true;

    client_sync_visible_name(client, cached, "Same Title", "Same Title",
            s_recording_set_fn, 99u);

    TAP_EQ_INT(s_delete_property_call_count, 0,
            "an already-cached, untruncated name never deletes the"
            " property again");
    TAP_EQ_STR(cached, "Same Title",
            "an already-cached, untruncated name leaves the cached"
            " buffer as it was");

    s_fake_connection_present = false;
    s_fake_ewmh_present = false;
    free(client);
}


int main(void)
{
    TAP_PLAN(26);

    s_test_note_user_time_newer_updates();
    s_test_note_user_time_older_is_ignored();
    s_test_update_user_time_null_client_is_a_no_op();
    s_test_update_user_time_newer_updates_client();
    s_test_update_user_time_older_is_ignored();
    s_test_sync_visible_name_null_args_are_a_no_op();
    s_test_sync_visible_name_truncated_sets_property();
    s_test_sync_visible_name_truncated_already_cached_is_a_no_op();
    s_test_sync_visible_name_untruncated_deletes_property();
    s_test_sync_visible_name_untruncated_already_cached_is_a_no_op();

    return TAP_DONE();
}
