/**
 * @file desktop/dfind.c
 *
 * @brief Looking a client up on a desktop
 *
 * Kept apart from the rest of @c desktop/ because it is what decides
 * which desktop a client belongs to, and so is what every walk over
 * the stacking order filters by.  Left among the desktop's other
 * client operations, anything wanting only this lookup would drag in
 * every one of them and, through those, most of the rest of the
 * window manager: a heavy dependency for a table lookup, and one the
 * tests were paying too.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>
#include <stdlib.h>     /* NULL */

/* ADT includes */
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <desktop.h>


/* Find the client on a desktop matching a given client ID */
client_td *desktop_find_client_by_id(const desktop_td *desktop,
        uint32_t id)
{
    void *elem;

    if (desktop == NULL || desktop->clients == NULL) {
        return NULL;
    }

    /* Walked over the desktop's own client table rather than its
     * stacking order.  Both hold the same clients, so the answer is
     * the same either way, but the table is what actually records
     * which desktop a client belongs to, which is the question being
     * asked here.  Keeping this off the stacking order also leaves it
     * free to be asked in the other direction, by a walk wanting to
     * know whether a client it reached is on a given desktop. */
    ohtbl_foreach(desktop->clients, elem) {
        client_td *const client = (client_td *) elem;

        if (client != NULL && client->id == id) {
            return client;
        }
    }

    return NULL;
}
