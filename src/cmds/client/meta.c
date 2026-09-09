/**
 * @file cmds/client/meta.c
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
#include <utils/safe/safestr.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>

/* Project includes */
#include <client.h>
#include <logger.h>

/* Local includes */
#include <cmds/client/meta.h>


/* Rename the client window */
void ccmd_client_rename(client_td *client, const char *name)
{
    char *new_name;

    if (client == NULL || name == NULL) {
        return;
    }

    new_name = safe_strdup(name);
    if (new_name == NULL) {
        return;
    }

    LOGGER_TRACE("Renaming client window=0x%x to '%s'",
            client->window, name);

    free(client->info.name);
    client->info.name = new_name;

    xcb_change_property(xcb_connection_get(),
            XCB_PROP_MODE_REPLACE,
            client->window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,
            (uint32_t) safe_strlen(client->info.name),
            client->info.name);

    xcb_ewmh_set_wm_name(xcb_ewmh_connection_get(),
            client->window,
            (uint32_t) safe_strlen(client->info.name),
            client->info.name);
}


/* Change the 'WM_CLASS' of the client window */
void ccmd_client_reclass(client_td *client,
        const char *restrict class_name,
        const char *restrict instance_name)
{
    char *wm_class_combined;
    size_t wm_class_combined_len;
    size_t len0;
    size_t len1;
    char *new_class;
    char *new_instance;

    if (client == NULL ||
            class_name == NULL || instance_name == NULL) {
        return;
    }

    new_class = safe_strdup(class_name);
    new_instance = safe_strdup(instance_name);
    if (new_class == NULL || new_instance == NULL) {
        free(new_class);
        free(new_instance);
        return;
    }

    LOGGER_TRACE("Setting 'WM_CLASS' for client window=0x%x to" \
            " '%s'/'%s'", client->window, class_name, instance_name);

    free(client->info.class_name[0]);
    free(client->info.class_name[1]);
    client->info.class_name[0] = new_class;
    client->info.class_name[1] = new_instance;

    /* The 'WM_CLASS' property contains two consecutive null-terminated
     * strings (ICCCM v 2.0, §4.1.2.5).  A single buffer is built
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

        xcb_icccm_set_wm_class(xcb_connection_get(), client->window,
                (uint32_t) wm_class_combined_len, wm_class_combined);
        free(wm_class_combined);
    } else {
        xcb_icccm_set_wm_class(xcb_connection_get(), client->window,
                (uint32_t) (len0 + 1), client->info.class_name[0]);
    }
}


/* Change the 'WM_WINDOW_ROLE' of the client window */
void ccmd_client_rerole(client_td *client, const char *role)
{
    xcb_atom_t role_atom;
    char *new_role;

    if (client == NULL || role == NULL) {
        return;
    }

    new_role = safe_strdup(role);
    if (new_role == NULL) {
        return;
    }

    LOGGER_TRACE("Setting 'WM_WINDOW_ROLE' for client window=0x%x to" \
            " '%s'", client->window, role);

    free(client->info.role_name);
    client->info.role_name = new_role;

    role_atom = atom_intern(xcb_connection_get(),
            "WM_WINDOW_ROLE", false);
    if (role_atom == XCB_ATOM_NONE) {
        return;
    }

    xcb_change_property(xcb_connection_get(),
            XCB_PROP_MODE_REPLACE,
            client->window,
            role_atom,
            XCB_ATOM_STRING,
            8,
            (uint32_t) safe_strlen(client->info.role_name),
            client->info.role_name);
}


/* Set the icon name for the client window */
void ccmd_client_set_icon(client_td *client, const char *icon_name)
{
    if (client == NULL || icon_name == NULL) {
        return;
    }

    LOGGER_TRACE("Setting icon name for client window=0x%x to '%s'",
            client->window, icon_name);

    xcb_change_property(xcb_connection_get(),
            XCB_PROP_MODE_REPLACE,
            client->window,
            XCB_ATOM_WM_ICON_NAME,
            XCB_ATOM_STRING,
            8,
            (uint32_t) safe_strlen(icon_name),
            icon_name);

    xcb_ewmh_set_wm_icon_name(xcb_ewmh_connection_get(),
            client->window,
            (uint32_t) safe_strlen(icon_name),
            icon_name);
}
