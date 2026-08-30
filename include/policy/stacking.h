/**
 * @file policy/stacking.h
 *
 * @brief Stacking order over every managed client
 *
 * Which window is drawn over which.  One order for the whole session
 * rather than one per desktop, which is how Openbox holds its
 * @c stacking_list, and for the same reason.  A client's height is a
 * fact about the client, not about the desktop it happens to be shown
 * on.
 *
 * With a list per desktop, an omnipresent window had to be carried
 * from one to the next on every desktop change, and each carry had to
 * decide where in the destination it landed.  There is no right answer
 * to that: the destination holds different windows, so any height
 * chosen for it there is a guess.  Holding one order removes the
 * question rather than answering it.
 *
 * The order runs from the bottom of the stack to the top, matching what
 * @c _NET_CLIENT_LIST_STACKING publishes and the direction the X
 * server's restacking wants.
 *
 * No caller sees which container holds it.  That was not true before:
 * the list was a public member of @c desktop_td and some eighty places
 * walked it with the container's primitives, so any change to how
 * it is held meant touching every one of them.  Walking it is done
 * through @a stacking_walk and @a stacking_walk_down instead, which
 * take a visitor called once per client.
 *
 * @defgroup stacking Client stacking order
 * @ingroup policy
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_STACKING_H
#define POLICY_STACKING_H

/* System includes */
#include <stdint.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Called once per client by @a stacking_walk
 *
 * @param client Client reached by the walk; never @c NULL
 * @param data   Whatever the caller handed the walk
 */
typedef void (*stacking_visitor_fn)(client_td *client, void *data);


/**
 * @brief Make sure the stacking order is ready for a desktop's clients
 *
 * Nothing is created per desktop: there is one order for the whole
 * session, brought into being by whichever desktop is made first.  The
 * call is kept so that a desktop still declares its need for one, and
 * so that failing to allocate it is reported where a desktop can still
 * be abandoned.
 *
 * @param desktop Desktop about to use the order
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval -1 Invalid @p desktop
 * @retval  1 The order itself could not be allocated
 *
 * @note The order refers to clients and owns none of them
 * @note Destroying it never destroys a client
 * @note Complexity: @e O(1)
 */
int stacking_create(const desktop_td *desktop);

/**
 * @brief Forget a desktop's clients, and the order once it is empty
 *
 * Only the clients @p desktop shows are forgotten.  Order is shared,
 * so a desktop going away must not take another's windows with it.  The
 * order itself is released by whichever desktop leaves it empty.
 *
 * @param desktop Desktop whose clients to forget; may be @c NULL
 *
 * @note Leaves the clients themselves alone, the desktop's client
 *       table being what owns them
 * @note Complexity: @e O(n * n) in the worst case, @e n being the
 *       number of managed clients: each is found and detached in
 *       turn, since detaching frees the node a walk would stand on
 */
void stacking_destroy(const desktop_td *desktop);

/**
 * @brief Place a client at the top of the stack
 *
 * For a window the user is meant to see straight away, which is
 * every ordinary new one.
 *
 * @param desktop Desktop to place it on
 * @param client  Client to place
 *
 * @return Status of the operation
 * @retval  0 Success, or the client was already in the order
 * @retval -1 Invalid @p desktop or @p client
 * @retval  1 The insertion itself failed
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
int stacking_add(const desktop_td *desktop, client_td *client);

/**
 * @brief Forget a client that is no longer managed
 *
 * Called as a client is destroyed, and never because it changed
 * desktop.  The order spans every managed client whichever desktop
 * shows it, so a client on its way from one to another stays in it and
 * keeps the height it had; taking it out and putting it back is what
 * used to send an omnipresent window to the top of every desktop it
 * visited.
 *
 * No desktop is named, there being one order for all of them.
 *
 * @param client Client to forget
 *
 * @return Status of the operation
 * @retval  0 Success, or the client was not in the order
 * @retval -1 Invalid @p client
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
int stacking_remove(const client_td *client);

/**
 * @brief Move a client to the top of the stack
 *
 * @param desktop Desktop the client is on
 * @param client  Client to raise
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval -1 Invalid @p desktop or @p client
 * @retval  1 The client is not in this desktop's order
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
int stacking_raise(const desktop_td *desktop, client_td *client);

/**
 * @brief Move a client to the bottom of the stack
 *
 * @param desktop Desktop the client is on
 * @param client  Client to lower
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval -1 Invalid @p desktop or @p client
 * @retval  1 The client is not in this desktop's order
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
int stacking_lower(const desktop_td *desktop, client_td *client);

/**
 * @brief How many clients a desktop's stacking order holds
 *
 * @param desktop Desktop to count for; may be @c NULL
 *
 * @return The number of clients in the order that @p desktop shows
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients: the order spans them all and is filtered here
 */
uint32_t stacking_count(const desktop_td *desktop);

/**
 * @brief Visit a desktop's clients from the bottom of the stack up
 *
 * The direction anything that draws or restacks wants.  Each client is
 * reached after whatever it covers.
 *
 * Clients of other desktops are passed over, so a caller sees exactly
 * the windows @p desktop draws, in the order it draws them.
 *
 * @param desktop Desktop whose clients to visit; may be @c NULL
 * @param visit   Called once per client; may be @c NULL
 * @param data    Handed to @p visit untouched
 *
 * @note The whole order is visited: no visitor can end the walk early,
 *       which is why one that stops on a condition records that in
 *       its @p data and ignores what follows
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
void stacking_walk(const desktop_td *desktop,
        stacking_visitor_fn visit, void *data);

/**
 * @brief Visit a desktop's clients from the top of the stack down
 *
 * The direction a caller wants when asking which window is in front,
 * hit-testing among them or offering them in order of interest.
 *
 * @param desktop Desktop whose clients to visit; may be @c NULL
 * @param visit   Called once per client; may be @c NULL
 * @param data    Handed to @p visit untouched
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
void stacking_walk_down(const desktop_td *desktop,
        stacking_visitor_fn visit, void *data);

#endif /* !POLICY_STACKING_H */
