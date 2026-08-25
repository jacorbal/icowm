/**
 * @file client/props.c
 *
 * @brief Client X11 property readers
 *
 * Concentrates all X server property queries needed to populate the
 * @c info sub-struct of a managed client.  Functions here have no side
 * effects on the X server and do not touch client state beyond writing
 * into caller-supplied buffers.
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
#include <stdlib.h>     /* free */
#include <string.h>     /* memcpy, memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/xcb/atom.h>

/* Local includes */
#include <client.h>
#include <client/internal.h>


/**
 * @brief Read a legacy Latin-1 ICCCM string property into up to two
 *        destination buffers
 *
 * Shared fallback path for @c client_props_refresh_icon_name and
 * @c client_props_refresh_name: both first try the UTF-8 EWMH
 * property (@c _NET_WM_ICON_NAME or @c _NET_WM_NAME) and only fall
 * back to this ICCCM one (@c WM_ICON_NAME or @c WM_NAME) when that
 * fails.
 *
 * @param client         Client whose window property is read
 * @param atom           ICCCM atom to read
 * @param dest1          First destination buffer (always written when
 *                        the property is present; at least 255 bytes)
 * @param dest2          Second destination buffer kept in sync with
 *                        @p dest1, or @c NULL when there is only one
 * @param clear_on_empty When @c true, an empty or missing property
 *                        clears @p dest1 to an empty string; when
 *                        @c false, @p dest1 (and @p dest2) are left
 *                        unchanged so a transient property removal
 *                        does not blank an already-known name
 *
 * @note Complexity: @e O(1), a single round trip
 */
static void s_client_read_legacy_name_prop(client_td *client,
        xcb_atom_t atom, char *restrict dest1, char *restrict dest2,
        bool clear_on_empty)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

    cookie = xcb_get_property(client->connection, 0, client->window,
            atom, XCB_ATOM_STRING, 0, 255);
    reply = xcb_get_property_reply(client->connection, cookie, NULL);

    if (reply != NULL && reply->value_len > 0) {
        size_t len = (reply->value_len < 255u)
            ? reply->value_len : 254u;
        const char *value = (char *) xcb_get_property_value(reply);

        memcpy(dest1, value, len);
        dest1[len] = '\0';
        if (dest2 != NULL) {
            memcpy(dest2, value, len);
            dest2[len] = '\0';
        }
    } else if (clear_on_empty) {
        dest1[0] = '\0';
    }

    if (reply != NULL) {
        free(reply);
    }
}


/* Retrieve the 'WM_NAME' property of a window */
size_t ci_get_wm_name(xcb_connection_t *connection,
        xcb_window_t window, char *buffer, size_t buffer_sz)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    size_t name_len = 0;

    if (buffer == NULL || buffer_sz == 0) {
        return 0;
    }

    cookie = xcb_get_property(connection, 0, window,
            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 255);
    reply = xcb_get_property_reply(connection, cookie, NULL);

    if (reply != NULL && reply->value_len > 0) {
        name_len = (reply->value_len < buffer_sz - 1) ?
            reply->value_len : buffer_sz - 1;
        memcpy(buffer, xcb_get_property_value(reply), name_len);
        buffer[name_len] = '\0';
        free(reply);
    } else {
        buffer[0] = '\0';
        if (reply != NULL) {
            free(reply);
        }
    }

    return name_len;
}


/* Retrieve the '_NET_WM_NAME' property of a window (UTF-8) */
size_t ci_get_net_wm_name(xcb_ewmh_connection_t *ewmh,
        xcb_window_t window, char *buffer, size_t buffer_sz)
{
    xcb_ewmh_get_utf8_strings_reply_t reply;
    size_t len = 0;

    if (ewmh == NULL || buffer == NULL || buffer_sz == 0) {
        return 0;
    }

    memset(&reply, 0, sizeof(reply));
    if (xcb_ewmh_get_wm_name_reply(ewmh,
                xcb_ewmh_get_wm_name(ewmh, window),
                &reply, NULL) && reply.strings_len > 0) {
        len = (reply.strings_len < buffer_sz - 1u)
            ? reply.strings_len : buffer_sz - 1u;
        memcpy(buffer, reply.strings, len);
        buffer[len] = '\0';
        xcb_ewmh_get_utf8_strings_reply_wipe(&reply);
    } else {
        buffer[0] = '\0';
    }

    return len;
}


/* Retrieve the 'WM_CLASS' property of a window */
int ci_get_wm_class(xcb_connection_t *connection,
        xcb_window_t window,
        char *restrict class_buf, size_t class_sz,
        char *restrict inst_buf, size_t inst_sz)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

    if (class_buf == NULL || class_sz == 0 ||
            (inst_buf != NULL && inst_sz == 0)) {
        return -1;
    }

    cookie = xcb_get_property(connection, 0, window,
            XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 255);
    reply = xcb_get_property_reply(connection, cookie, NULL);

    class_buf[0] = '\0';
    if (inst_buf != NULL) {
        inst_buf[0] = '\0';
    }

    if (reply != NULL && reply->value_len > 0) {
        char *const value = (char *) xcb_get_property_value(reply);
        size_t value_len = reply->value_len;
        size_t inst_len = 0;
        size_t copy_len;

        /* 'WM_CLASS' format: "instance\0class\0"
         * Find the first null terminator to separate the two strings */
        for (size_t i = 0; i < value_len; ++i) {
            if (value[i] == '\0') {
                inst_len = i;
                break;
            }
        }

        /* Copy instance name to the destination buffer if provided */
        if (inst_buf != NULL && inst_len > 0) {
            copy_len = (inst_len < inst_sz - 1) ?
                inst_len : inst_sz - 1;
            memcpy(inst_buf, value, copy_len);
            inst_buf[copy_len] = '\0';
        }

        /* Copy class name; class starts after 'instance_name + 1' */
        if (inst_len + 1 < value_len) {
            size_t class_len = value_len - inst_len - 1;
            copy_len = (class_len < class_sz - 1) ?
                class_len : class_sz - 1;
            memcpy(class_buf, value + inst_len + 1, copy_len);
            class_buf[copy_len] = '\0';
        }

        free(reply);
        return 0;
    }

    if (reply != NULL) {
        free(reply);
    }

    return -1;
}


/* Update a managed client's icon name from the X server */
void client_props_refresh_icon_name(client_td *client)
{
    xcb_ewmh_get_utf8_strings_reply_t net_reply;

    if (client == NULL) {
        return;
    }

    /* Prefer '_NET_WM_ICON_NAME', which is UTF-8, over
     * 'WM_ICON_NAME', which is Latin-1 */
    memset(&net_reply, 0, sizeof(net_reply));
    if (client->ewmh != NULL &&
            xcb_ewmh_get_wm_icon_name_reply(client->ewmh,
                xcb_ewmh_get_wm_icon_name(client->ewmh, client->window),
                &net_reply, NULL) &&
            net_reply.strings_len > 0) {
        size_t len = (net_reply.strings_len < (CONFIG_MAX_LENGTH_NAME - 1u))
            ? net_reply.strings_len
            : (CONFIG_MAX_LENGTH_NAME - 2u);

        memcpy(client->icon_info.visible_icon_name, net_reply.strings, len);
        client->icon_info.visible_icon_name[len] = '\0';
        xcb_ewmh_get_utf8_strings_reply_wipe(&net_reply);
        return;
    }

    /* Fall back to 'WM_ICON_NAME' */
    s_client_read_legacy_name_prop(client, XCB_ATOM_WM_ICON_NAME,
            client->icon_info.visible_icon_name, NULL, true);
}


/* Update a managed client's name from the X server */
void client_props_refresh_name(client_td *client)
{
    xcb_ewmh_get_utf8_strings_reply_t net_reply;

    if (client == NULL) {
        return;
    }

    /* Prefer '_NET_WM_NAME' (UTF-8) over 'WM_NAME' (Latin-1) */
    memset(&net_reply, 0, sizeof(net_reply));
    if (client->ewmh != NULL &&
            xcb_ewmh_get_wm_name_reply(client->ewmh,
                xcb_ewmh_get_wm_name(client->ewmh, client->window),
                &net_reply, NULL) &&
            net_reply.strings_len > 0) {
        size_t len = (net_reply.strings_len < (CONFIG_MAX_LENGTH_NAME - 1u))
            ? net_reply.strings_len
            : (CONFIG_MAX_LENGTH_NAME - 2u);

        memcpy(client->info.name, net_reply.strings, len);
        memcpy(client->info.visible_name, net_reply.strings, len);
        client->info.name[len] = '\0';
        client->info.visible_name[len] = '\0';
        xcb_ewmh_get_utf8_strings_reply_wipe(&net_reply);
        return;
    }

    /* Fall back to 'WM_NAME'; a transient removal of both properties
     * should not blank an already-known name, so nothing is cleared
     * here when the property is absent or empty. */
    s_client_read_legacy_name_prop(client, XCB_ATOM_WM_NAME,
            client->info.name, client->info.visible_name, false);
}


/* Update a managed client's role from 'WM_WINDOW_ROLE' */
void client_props_refresh_role(client_td *client)
{
    xcb_atom_t role_atom;

    if (client == NULL || client->info.role_name == NULL) {
        return;
    }

    role_atom = atom_intern(client->connection, "WM_WINDOW_ROLE", true);
    if (role_atom == XCB_ATOM_NONE) {
        client->info.role_name[0] = '\0';
        return;
    }

    s_client_read_legacy_name_prop(client, role_atom,
            client->info.role_name, NULL, true);
}


/* Re-read 'WM_NORMAL_HINTS' and update the client's size-hint fields */
void client_props_refresh_normal_hints(client_td *client)
{
    xcb_size_hints_t hints;

    if (client == NULL) {
        return;
    }

    memset(&hints, 0, sizeof(hints));
    memset(&client->hints_icccm.size, 0, sizeof(client->hints_icccm.size));
    if (!xcb_icccm_get_wm_normal_hints_reply(client->connection,
                xcb_icccm_get_wm_normal_hints(client->connection,
                    client->window),
                &hints, NULL)) {
        return;
    }

    client->hints_icccm.size.is_valid = true;

    /* ICCCM 4.1.2.3: a client that sets 'USPosition' or 'PPosition'
     * is making a specific, deliberate request for where it wants to
     * appear, not leaving the decision to this window manager's own
     * placement policy; both flags are honored the same way, since
     * ICCCM itself does not require distinguishing a user's own
     * explicit choice (US) from a program's own default (P) here. */
    if (hints.flags & (XCB_ICCCM_SIZE_HINT_US_POSITION |
                XCB_ICCCM_SIZE_HINT_P_POSITION)) {
        client->hints_icccm.size.has_position = true;
        client->hints_icccm.size.req_pos.x = (int32_t) hints.x;
        client->hints_icccm.size.req_pos.y = (int32_t) hints.y;
    }

    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
        client->hints_icccm.size.min.w = (uint32_t) hints.min_width;
        client->hints_icccm.size.min.h = (uint32_t) hints.min_height;
    }

    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
        client->hints_icccm.size.max.w = (uint32_t) hints.max_width;
        client->hints_icccm.size.max.h = (uint32_t) hints.max_height;
    }

    if (hints.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
        client->hints_icccm.size.base.w = (uint32_t) hints.base_width;
        client->hints_icccm.size.base.h = (uint32_t) hints.base_height;
    }

    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) {
        client->hints_icccm.size.inc.w = (uint32_t) hints.width_inc;
        client->hints_icccm.size.inc.h = (uint32_t) hints.height_inc;
    }

    /* Always wins over 'windows.gravity' in 'config.json' ('client.h',
     * 'layout.gravity' itself), including on a later hint update like
     * this one, per ICCCM's own "MUST honor" mandate; that config field
     * is a fallback for a client that never states its own gravity, not
     * an override for one that does. */
    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY) {
        client->layout.gravity = (uint16_t) hints.win_gravity;
    }

    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_ASPECT) {
        client->hints_icccm.size.aspect.min.num =
            (int32_t) hints.min_aspect_num;
        client->hints_icccm.size.aspect.min.den =
            (int32_t) hints.min_aspect_den;
        client->hints_icccm.size.aspect.max.num =
            (int32_t) hints.max_aspect_num;
        client->hints_icccm.size.aspect.max.den =
            (int32_t) hints.max_aspect_den;
    }

    /* ICCCM §4.1.2.3: a fixed-size window has min == max in at least
     * one axis.  Some applications (e.g., gmrun) constrain only height,
     * leaving width free; the window is still effectively non-resizable
     * from the window manager's perspective and must not be
     * maximized or resized. */
    if ((hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) &&
            (hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
        bool fixed_w = (hints.min_width > 0 &&
                hints.min_width == hints.max_width);
        bool fixed_h = (hints.min_height > 0 &&
                hints.min_height == hints.max_height);
        if (fixed_w || fixed_h) {
            client_forbid_resize(client);
        }
    }
}


/* Refresh a client's own 'WM_COLORMAP_WINDOWS' list */
void client_props_refresh_colormap_windows(client_td *client)
{
    xcb_atom_t colormap_windows_atom;
    xcb_icccm_get_wm_colormap_windows_reply_t reply;

    if (client == NULL) {
        return;
    }

    client->colormap_windows.count = 0u;

    colormap_windows_atom = atom_intern(client->connection,
            "WM_COLORMAP_WINDOWS", false);
    if (colormap_windows_atom == XCB_ATOM_NONE) {
        return;
    }

    if (!xcb_icccm_get_wm_colormap_windows_reply(client->connection,
                xcb_icccm_get_wm_colormap_windows(client->connection,
                    client->window, colormap_windows_atom),
                &reply, NULL)) {
        return;
    }

    /* ICCCM §4.1.8: the list is in the client's own priority order;
     * entries past 'WM_COLORMAP_WINDOWS_MAX' are already its own
     * lowest-priority ones, so simply not tracking them is the
     * correct degradation, not an arbitrary truncation. */
    for (uint32_t i = 0u;
            i < reply.windows_len && i < WM_COLORMAP_WINDOWS_MAX; ++i) {
        xcb_get_window_attributes_cookie_t wac =
            xcb_get_window_attributes(client->connection,
                    reply.windows[i]);
        xcb_get_window_attributes_reply_t *const war =
            xcb_get_window_attributes_reply(client->connection, wac,
                    NULL);

        client->colormap_windows.windows[i] = reply.windows[i];
        client->colormap_windows.colormap_ids[i] = (war != NULL)
            ? war->colormap : (xcb_colormap_t) XCB_NONE;
        client->colormap_windows.count += 1u;
        if (war != NULL) {
            free(war);
        }
    }

    xcb_icccm_get_wm_colormap_windows_reply_wipe(&reply);
}
