/**
 * @file policy/stacking.c
 *
 * @brief Stacking order over every managed client, implementation
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
#include <stdint.h>
#include <stdlib.h>     /* NULL */

/* ADT includes */
#include <adt/cdlist.h>

/* Project includes */
#include <client.h>
#include <desktop.h>

/* Local includes */
#include <policy/stacking.h>


/**
 * @brief Every managed client, lowest in the stack first
 *
 * One list for the whole session rather than one per desktop.  See
 * @c policy/stacking.h for why.  The clients belong to the tables of
 * their desktops, so this is created without a destructor.  It refers
 * to them and owns none of them.
 */
static cdlist_td *s_stacking = NULL;


/**
 * @brief Create the stacking order on first use
 *
 * @return @c true when the list is available
 *
 * @note Complexity: @e O(1)
 */
static bool s_stacking_ensure(void)
{
    if (s_stacking == NULL) {
        s_stacking = cdlist_init(NULL);
    }

    return s_stacking != NULL;
}


/**
 * @brief Whether a desktop is the one showing a given client
 *
 * @param desktop Desktop to ask
 * @param client  Client to look for
 *
 * @return @c true when @p desktop holds @p client
 *
 * @note Asked of the desktop's client table, which is what records
 *       where a client belongs; the order itself says nothing about it
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static bool s_stacking_shows_client(const desktop_td *desktop,
        const client_td *client)
{
    return client != NULL &&
        desktop_find_client_by_id(desktop, client->id) == client;
}


/**
 * @brief Find the node holding a client, and the one before it
 *
 * @param client   Client to look for
 * @param out_prev Receives the node before the one found, which
 *                 @a cdlist_rem_next needs, or @c NULL when the client
 *                 sits at the head; may itself be @c NULL
 *
 * @return The node holding @p client, or @c NULL when it is absent
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
static cdlist_item_td *s_stacking_node_find(const client_td *client,
        cdlist_item_td **out_prev)
{
    cdlist_item_td *node;
    cdlist_item_td *behind = NULL;
    const cdlist_item_td *initial;

    if (out_prev != NULL) {
        *out_prev = NULL;
    }

    if (s_stacking == NULL || cdlist_size(s_stacking) == 0u) {
        return NULL;
    }

    node = cdlist_head(s_stacking);
    initial = node;
    if (node == NULL) {
        return NULL;
    }

    /* The list is circular: its tail wraps back to its head rather
     * than handing back a null, so where the walk began is what says
     * it is over */
    do {
        if ((const client_td *) cdlist_data(node) == client) {
            if (out_prev != NULL) {
                *out_prev = behind;
            }
            return node;
        }
        behind = node;
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return NULL;
}


/**
 * @brief Take a client out of the order, if it is in it
 *
 * @param client Client to detach
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
static void s_stacking_detach(const client_td *client)
{
    const cdlist_item_td *node;
    cdlist_item_td *prev = NULL;
    void *removed = NULL;

    node = s_stacking_node_find(client, &prev);
    if (node == NULL) {
        return;
    }

    (void) cdlist_rem_next(s_stacking, prev, &removed);
}


/**
 * @brief The lowest client in the order that a desktop shows
 *
 * @param desktop Desktop to look for
 *
 * @return That client, or @c NULL when the desktop shows none
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
static client_td *s_stacking_first_of(const desktop_td *desktop)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (s_stacking == NULL || cdlist_size(s_stacking) == 0u) {
        return NULL;
    }

    node = cdlist_head(s_stacking);
    initial = node;
    if (node == NULL) {
        return NULL;
    }

    do {
        client_td *const client = (client_td *) cdlist_data(node);

        if (s_stacking_shows_client(desktop, client)) {
            return client;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return NULL;
}


/* Make sure the stacking order is ready for a desktop's clients */
int stacking_create(const desktop_td *desktop)
{
    if (desktop == NULL) {
        return -1;
    }

    /* Nothing is created per desktop: there is one order for the whole
     * session, brought into being by whichever desktop is made first.
     * The call is kept so that a desktop still declares its need for
     * one, and so that failing to allocate it is reported where a
     * desktop can still be abandoned. */
    return (s_stacking_ensure()) ? 0 : 1;
}


/* Forget a desktop's clients, and the order itself once it is empty */
void stacking_destroy(const desktop_td *desktop)
{
    const client_td *client;

    if (desktop == NULL || s_stacking == NULL) {
        return;
    }

    /* Only this desktop's clients go, the order being shared.  A
     * desktop going away must not take another's windows with it.
     *
     * One is found and detached at a time, rather than detaching while
     * walking: detaching frees the very node a walk would be standing
     * on, and the desktop's clients are not contiguous in an order
     * that spans them all. */
    client = s_stacking_first_of(desktop);
    while (client != NULL) {
        s_stacking_detach(client);
        client = s_stacking_first_of(desktop);
    }

    /* The last desktop to go takes the order with it */
    if (cdlist_size(s_stacking) == 0u) {
        cdlist_destroy(s_stacking);
        s_stacking = NULL;
    }
}


/* Place a client at the top of the stack */
int stacking_add(const desktop_td *desktop, client_td *client)
{
    if (desktop == NULL || client == NULL || !s_stacking_ensure()) {
        return -1;
    }

    /* A client already there keeps the height it has.  Adding is for a
     * window arriving, and one that is already stacked has not */
    if (s_stacking_node_find(client, NULL) != NULL) {
        return 0;
    }

    return (cdlist_ins_next(s_stacking, cdlist_tail(s_stacking),
                client) == 0) ? 0 : 1;
}


/* Forget a client that is no longer managed */
int stacking_remove(const client_td *client)
{
    if (client == NULL) {
        return -1;
    }

    s_stacking_detach(client);

    return 0;
}


/* Move a client to the top of the stack */
int stacking_raise(const desktop_td *desktop, client_td *client)
{
    if (desktop == NULL || client == NULL || s_stacking == NULL) {
        return -1;
    }

    if (s_stacking_node_find(client, NULL) == NULL) {
        return 1;
    }

    s_stacking_detach(client);

    return (cdlist_ins_next(s_stacking, cdlist_tail(s_stacking),
                client) == 0) ? 0 : 1;
}


/* Move a client to the bottom of the stack */
int stacking_lower(const desktop_td *desktop, client_td *client)
{
    if (desktop == NULL || client == NULL || s_stacking == NULL) {
        return -1;
    }

    if (s_stacking_node_find(client, NULL) == NULL) {
        return 1;
    }

    s_stacking_detach(client);

    /* Inserted after no node at all, which 'cdlist_ins_next' takes as
     * the head of an otherwise untouched list */
    return (cdlist_ins_next(s_stacking, NULL, client) == 0) ? 0 : 1;
}


/* The client currently at the bottom of the whole session's
 * stacking order */
client_td *stacking_bottom(void)
{
    cdlist_item_td *node;

    if (s_stacking == NULL || cdlist_size(s_stacking) == 0u) {
        return NULL;
    }

    node = cdlist_head(s_stacking);
    return (node != NULL) ? (client_td *) cdlist_data(node) : NULL;
}


/* How many clients a desktop's stacking order holds */
uint32_t stacking_count(const desktop_td *desktop)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;
    uint32_t count = 0u;

    if (desktop == NULL || s_stacking == NULL ||
            cdlist_size(s_stacking) == 0u) {
        return 0u;
    }

    node = cdlist_head(s_stacking);
    initial = node;
    if (node == NULL) {
        return 0u;
    }

    do {
        if (s_stacking_shows_client(desktop,
                    (const client_td *) cdlist_data(node))) {
            count++;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    return count;
}


/* Visit a desktop's clients from the bottom of the stack up */
void stacking_walk(const desktop_td *desktop,
        stacking_visitor_fn visit, void *data)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (desktop == NULL || s_stacking == NULL || visit == NULL ||
            cdlist_size(s_stacking) == 0u) {
        return;
    }

    node = cdlist_head(s_stacking);
    initial = node;
    if (node == NULL) {
        return;
    }

    do {
        client_td *const client = (client_td *) cdlist_data(node);

        /* Advanced before visiting, so that a visitor removing the
         * client it was handed cannot leave this walk holding a node
         * that is no longer in the list */
        node = cdlist_next(node);

        if (s_stacking_shows_client(desktop, client)) {
            visit(client, data);
        }
    } while (node != NULL && node != initial);
}


/* Visit a desktop's clients from the top of the stack down */
void stacking_walk_down(const desktop_td *desktop,
        stacking_visitor_fn visit, void *data)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (desktop == NULL || s_stacking == NULL || visit == NULL ||
            cdlist_size(s_stacking) == 0u) {
        return;
    }

    node = cdlist_tail(s_stacking);
    initial = node;
    if (node == NULL) {
        return;
    }

    do {
        client_td *const client = (client_td *) cdlist_data(node);

        node = cdlist_prev(node);

        if (s_stacking_shows_client(desktop, client)) {
            visit(client, data);
        }
    } while (node != NULL && node != initial);
}
