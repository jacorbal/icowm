/**
 * @file cmds/meta.c
 *
 * @brief Client metadata command implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

/* Utils includes */
#include <utils/safestr.h>

/* Project includes */
#include <actdata.h>
#include <client.h>

/* Local includes */
#include <cmds/meta.h>
#include <cmds/util.h>


/* Rename the client window */
void wcmd_client_rename(client_td *client,
        action_data_client_td *client_data)
{
    if (client == NULL || client_data == NULL) {
        return;
    }

    free(client->info.name);
    client->info.name = safe_strdup(client_data->new_data.str.str0);

    xcb_change_property(client->connection,
            XCB_PROP_MODE_REPLACE,
            client->window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,
            (uint32_t) safe_strlen(client->info.name),
            client->info.name);

    xcb_ewmh_set_wm_name(client->ewmh, client->window,
            (uint32_t) safe_strlen(client->info.name),
            client->info.name);
}


/* Change the 'WM_CLASS' of the client window */
void wcmd_client_reclass(client_td *client,
        action_data_client_td *client_data)
{
    char *wm_class_combined;
    size_t wm_class_combined_len;
    size_t len0;
    size_t len1;

    if (client == NULL || client_data == NULL) {
        return;
    }

    free(client->info.class_name[0]);
    free(client->info.class_name[1]);
    client->info.class_name[0] =
        safe_strdup(client_data->new_data.str.str0);
    client->info.class_name[1] =
        safe_strdup(client_data->new_data.str.str1);

    /* The 'WM_CLASS' property contains two consecutive null-terminated
     * strings (ICCCM v 2.0, § 4.1.2.5).  A single buffer is built
     * manually so that both null terminators land in the buffer passed
     * to 'xcb_icccm_set_wm_class'. */
    len0 = safe_strlen(client->info.class_name[0]);
    len1 = safe_strlen(client->info.class_name[1]);
    wm_class_combined_len = (len0 + 1) + (len1 + 1);
    wm_class_combined = malloc(wm_class_combined_len);
    if (wm_class_combined) {
        memcpy(wm_class_combined, client->info.class_name[0], len0);
        wm_class_combined[len0] = '\0';
        memcpy(wm_class_combined + len0 + 1,
                client->info.class_name[1], len1);
        wm_class_combined[len0 + 1 + len1] = '\0';

        xcb_icccm_set_wm_class(client->connection, client->window,
                (uint32_t) wm_class_combined_len, wm_class_combined);
        free(wm_class_combined);
    } else {
        xcb_icccm_set_wm_class(client->connection, client->window,
                (uint32_t) (len0 + 1), client->info.class_name[0]);
    }
}


/* Change the 'WM_WINDOW_ROLE' of the client window */
void wcmd_client_rerole(client_td *client,
        action_data_client_td *client_data)
{
    xcb_intern_atom_cookie_t role_atom_cookie;
    xcb_intern_atom_reply_t *role_atom_reply;

    if (client == NULL || client_data == NULL) {
        return;
    }

    free(client->info.role_name);
    client->info.role_name =
        safe_strdup(client_data->new_data.str.str0);

    role_atom_cookie = xcb_intern_atom(client->connection, 0,
            (uint16_t) safe_strlen("WM_WINDOW_ROLE"), "WM_WINDOW_ROLE");
    role_atom_reply = xcb_intern_atom_reply(client->connection,
            role_atom_cookie, NULL);

    if (!role_atom_reply) {
        return;
    }

    xcb_change_property(client->connection,
            XCB_PROP_MODE_REPLACE,
            client->window,
            role_atom_reply->atom,
            XCB_ATOM_STRING,
            8,
            (uint32_t) safe_strlen(client->info.role_name),
            client->info.role_name);

    free(role_atom_reply);
}


/* Set the icon name for the client window */
void wcmd_client_set_icon(client_td *client,
        action_data_client_td *client_data)
{
    if (client == NULL || client_data == NULL) {
        return;
    }

    xcb_change_property(client->connection,
            XCB_PROP_MODE_REPLACE,
            client->window,
            XCB_ATOM_WM_ICON_NAME,
            XCB_ATOM_STRING,
            8,
            (uint32_t) safe_strlen(client_data->new_data.str.str0),
            client_data->new_data.str.str0);

    xcb_ewmh_set_wm_icon_name(client->ewmh, client->window,
            (uint32_t) safe_strlen(client_data->new_data.str.str0),
            client_data->new_data.str.str0);
}
