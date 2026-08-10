/**
 * @file utils/xcb/atom.c
 *
 * @brief Single-atom interning implementation
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
#include <stddef.h>     /* size_t */
#include <stdlib.h>     /* NULL, free */
#include <string.h>     /* memcpy */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Local includes */
#include <utils/xcb/atom.h>


/* Intern an X atom by its string name */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists)
{
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom = XCB_ATOM_NONE;

    if (connection == NULL || name == NULL) {
        return XCB_ATOM_NONE;
    }

    reply = xcb_intern_atom_reply(connection,
            xcb_intern_atom(connection, (uint8_t) only_if_exists,
                (uint16_t) safe_strlen(name), name),
            NULL);
    if (reply != NULL) {
        atom = reply->atom;
        free(reply);
    }

    return atom;
}


/* Resolve an X atom to its string name */
bool atom_name(xcb_connection_t *connection, xcb_atom_t atom,
        char *out_name, size_t out_name_size)
{
    xcb_get_atom_name_reply_t *reply;
    int name_len;
    size_t copy_len;

    if (connection == NULL || out_name == NULL || out_name_size == 0u) {
        return false;
    }
    out_name[0] = '\0';

    if (atom == XCB_ATOM_NONE) {
        return false;
    }

    reply = xcb_get_atom_name_reply(connection,
            xcb_get_atom_name(connection, atom), NULL);
    if (reply == NULL) {
        return false;
    }

    name_len = xcb_get_atom_name_name_length(reply);
    if (name_len < 0) {
        name_len = 0;
    }
    copy_len = ((size_t) name_len < out_name_size - 1u)
        ? (size_t) name_len : out_name_size - 1u;
    memcpy(out_name, xcb_get_atom_name_name(reply), copy_len);
    out_name[copy_len] = '\0';
    free(reply);

    return true;
}
