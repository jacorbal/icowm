/**
 * @file desktop/focus.c
 *
 * @brief Per-desktop most-recently-used focus order implementation
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
#include <stdlib.h>     /* NULL */

/* ADT includes */
#include <adt/cdlist.h>

/* Local includes */
#include <desktop/focus.h>
#include <client.h>
#include <desktop.h>


/**
 * @brief Find the list node holding a given client
 *
 * @param desktop Desktop owning the order
 * @param client  Client to look for
 * @param prev    Receives the node before the one found, which
 *                @a cdlist_rem_next needs, or @c NULL when the client
 *                is at the head; may itself be @c NULL when the
 *                caller does not need it
 *
 * @return The node holding @p client, or @c NULL when it is not in
 *         the order
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
static cdlist_item_td *s_focus_order_find(const desktop_td *desktop,
        const client_td *client, cdlist_item_td **prev)
{
    cdlist_item_td *node;
    cdlist_item_td *behind = NULL;
    const cdlist_item_td *initial;

    if (prev != NULL) {
        *prev = NULL;
    }

    if (desktop->focus_order == NULL ||
            cdlist_size(desktop->focus_order) == 0u) {
        return NULL;
    }

    node = cdlist_head(desktop->focus_order);
    initial = node;
    if (node == NULL) {
        return NULL;
    }

    do {
        if ((const client_td *) cdlist_data(node) == client) {
            if (prev != NULL) {
                *prev = behind;
            }
            return node;
        }
        behind = node;
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return NULL;
}


/* Move a client to the head of its desktop's focus order */
int desktop_focus_order_to_top(desktop_td *desktop, client_td *client)
{
    cdlist_item_td *node;
    cdlist_item_td *prev = NULL;

    if (desktop == NULL || client == NULL ||
            desktop->focus_order == NULL) {
        return -1;
    }

    node = s_focus_order_find(desktop, client, &prev);
    if (node != NULL) {
        void *removed = NULL;

        /* Already the head: nothing to do, and removing then
         * reinserting would churn a node for no reason */
        if (node == cdlist_head(desktop->focus_order)) {
            return 0;
        }
        (void) cdlist_rem_next(desktop->focus_order, prev, &removed);
    }

    /* Inserted after no node at all, which 'cdlist_ins_next' takes as
     * the head of an otherwise untouched list */
    if (cdlist_ins_next(desktop->focus_order, NULL, client) != 0) {
        return 1;
    }

    return 0;
}


/* Add a client to the tail of its desktop's focus order */
int desktop_focus_order_add(desktop_td *desktop, client_td *client)
{
    if (desktop == NULL || client == NULL ||
            desktop->focus_order == NULL) {
        return -1;
    }

    /* Left where it is if already recorded, so that a path running
     * twice does not move a client the person did use recently down
     * to the far end of the order */
    if (s_focus_order_find(desktop, client, NULL) != NULL) {
        return 0;
    }

    if (cdlist_ins_next(desktop->focus_order,
                cdlist_tail(desktop->focus_order), client) != 0) {
        return 1;
    }

    return 0;
}


/* Remove a client from its desktop's focus order */
int desktop_focus_order_remove(desktop_td *desktop, client_td *client)
{
    cdlist_item_td *node;
    cdlist_item_td *prev = NULL;
    void *removed = NULL;

    if (desktop == NULL || client == NULL ||
            desktop->focus_order == NULL) {
        return -1;
    }

    node = s_focus_order_find(desktop, client, &prev);
    if (node == NULL) {
        return 0;
    }

    (void) cdlist_rem_next(desktop->focus_order, prev, &removed);

    return 0;
}


/* Return the most recently focused client matching a predicate */
client_td *desktop_focus_order_first(desktop_td *desktop,
        bool (*is_valid)(const client_td *candidate,
                const client_td *exclude),
        const client_td *exclude)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (desktop == NULL || is_valid == NULL ||
            desktop->focus_order == NULL ||
            cdlist_size(desktop->focus_order) == 0u) {
        return NULL;
    }

    node = cdlist_head(desktop->focus_order);
    initial = node;
    if (node == NULL) {
        return NULL;
    }

    do {
        client_td *const candidate = (client_td *) cdlist_data(node);

        if (candidate != NULL && is_valid(candidate, exclude)) {
            return candidate;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return NULL;
}
