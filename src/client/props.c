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
#include <stddef.h>
#include <stdlib.h>     /* free */
#include <string.h>     /* memcpy, memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Command includes */
#include <cmds/ccmd.h>

/* Local includes */
#include <client.h>
#include <client/internal.h>


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
        char *class_buf, size_t class_sz,
        char *inst_buf, size_t inst_sz)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    char *value;
    size_t value_len;
    size_t inst_len;
    size_t copy_len;

    if (class_buf == NULL || class_sz == 0) {
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
        value = (char *) xcb_get_property_value(reply);
        value_len = reply->value_len;

        /* 'WM_CLASS' format: "instance\0class\0"
         * Find the first null terminator to separate the two strings */
        inst_len = 0;
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
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

    if (client == NULL) {
        return;
    }

    /* Prefer '_NET_WM_ICON_NAME' (UTF-8) over 'WM_ICON_NAME' (Latin-1) */
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
    cookie = xcb_get_property(client->connection, 0, client->window,
            XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING, 0, 255);
    reply = xcb_get_property_reply(client->connection, cookie, NULL);

    if (reply != NULL && reply->value_len > 0) {
        size_t len = (reply->value_len < 255u)
            ? reply->value_len : 254u;
        char *value = (char *) xcb_get_property_value(reply);

        memcpy(client->icon_info.visible_icon_name, value, len);
        client->icon_info.visible_icon_name[len] = '\0';
    } else {
        client->icon_info.visible_icon_name[0] = '\0';
    }

    if (reply != NULL) {
        free(reply);
    }
}


/* Update a managed client's name from the X server */
void client_props_refresh_name(client_td *client)
{
    xcb_ewmh_get_utf8_strings_reply_t net_reply;
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

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

    /* Fall back to 'WM_NAME' */
    cookie = xcb_get_property(client->connection, 0, client->window,
            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 255);
    reply = xcb_get_property_reply(client->connection, cookie, NULL);

    if (reply != NULL && reply->value_len > 0) {
        size_t len = (reply->value_len < 255u)
            ? reply->value_len : 254u;
        char *value = (char *) xcb_get_property_value(reply);

        memcpy(client->info.name, value, len);
        memcpy(client->info.visible_name, value, len);
        client->info.name[len] = '\0';
        client->info.visible_name[len] = '\0';
    }

    if (reply != NULL) {
        free(reply);
    }
}


/* Update a managed client's role from 'WM_WINDOW_ROLE' */
void client_props_refresh_role(client_td *client)
{
    xcb_intern_atom_reply_t *role_atom_reply;
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

    if (client == NULL || client->info.role_name == NULL) {
        return;
    }

    role_atom_reply = xcb_intern_atom_reply(client->connection,
            xcb_intern_atom(client->connection, 1,
                (uint16_t) safe_strlen("WM_WINDOW_ROLE"),
                "WM_WINDOW_ROLE"), NULL);
    if (role_atom_reply == NULL) {
        client->info.role_name[0] = '\0';
        return;
    }

    cookie = xcb_get_property(client->connection, 0, client->window,
            role_atom_reply->atom, XCB_ATOM_STRING, 0, 255);
    reply = xcb_get_property_reply(client->connection, cookie, NULL);

    if (reply != NULL && reply->value_len > 0) {
        size_t len = (reply->value_len < 255u) ? reply->value_len : 254u;
        char *value = (char *) xcb_get_property_value(reply);

        memcpy(client->info.role_name, value, len);
        client->info.role_name[len] = '\0';
    } else {
        client->info.role_name[0] = '\0';
    }

    if (reply != NULL) {
        free(reply);
    }

    free(role_atom_reply);
}


/* Re-read 'WM_HINTS' and update the client's input model, urgency, and
 * group */
void client_props_refresh_wm_hints(client_td *client)
{
    xcb_icccm_wm_hints_t hints;
    xcb_get_property_cookie_t cookie;
    int ok;

    memset(&hints, 0, sizeof(hints));
    cookie = xcb_icccm_get_wm_hints(client->connection, client->window);
    ok = xcb_icccm_get_wm_hints_reply(client->connection, cookie,
            &hints, NULL);

    if (!ok) {
        return;
    }

    if (hints.flags & XCB_ICCCM_WM_HINT_INPUT) {
        client->wm_input_hint = (hints.input != 0);
    }

    if (hints.flags & XCB_ICCCM_WM_HINT_WINDOW_GROUP) {
        client->group_leader = hints.window_group;
    }

    if (hints.flags & XCB_ICCCM_WM_HINT_X_URGENCY) {
        wcmd_client_set_urgent(client);
    } else {
        wcmd_client_clear_urgent(client);
    }
}


/* Re-read 'WM_NORMAL_HINTS' and update the client's size-hint fields */
void client_props_refresh_normal_hints(client_td *client)
{
    xcb_size_hints_t hints;

    if (client == NULL) {
        return;
    }

    memset(&hints, 0, sizeof(hints));
    memset(&client->size_hints, 0, sizeof(client->size_hints));
    if (!xcb_icccm_get_wm_normal_hints_reply(client->connection,
                xcb_icccm_get_wm_normal_hints(client->connection,
                    client->window),
                &hints, NULL)) {
        return;
    }

    client->size_hints.valid = true;

    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
        client->size_hints.min_w = (int32_t) hints.min_width;
        client->size_hints.min_h = (int32_t) hints.min_height;
    }

    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
        client->size_hints.max_w = (int32_t) hints.max_width;
        client->size_hints.max_h = (int32_t) hints.max_height;
    }

    if (hints.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
        client->size_hints.base_w = (int32_t) hints.base_width;
        client->size_hints.base_h = (int32_t) hints.base_height;
    }

    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) {
        client->size_hints.inc_w = (int32_t) hints.width_inc;
        client->size_hints.inc_h = (int32_t) hints.height_inc;
    }

    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY) {
        client->layout.gravity = (uint16_t) hints.win_gravity;
    }

    /* ICCCM §4.1.2.3: a fixed-size window has min == max */
    if ((hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) &&
            (hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) &&
            hints.min_width > 0 && hints.min_height > 0 &&
            hints.min_width == hints.max_width &&
            hints.min_height == hints.max_height) {
        client_unset_resizable(client);
    }
}

