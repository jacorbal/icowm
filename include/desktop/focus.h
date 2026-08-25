/**
 * @file desktop/focus.h
 *
 * @brief Per-desktop most-recently-used focus order
 *
 * A desktop keeps two orders over the same clients, and they answer
 * different questions.  @c desktop_td.stacking is where each window
 * sits from front to back, which is what the screen shows and what
 * @c _NET_CLIENT_LIST_STACKING publishes.  @c desktop_td.focus_order,
 * which this file maintains, is which window was focused most
 * recently, which is what decides where focus goes when the focused
 * window closes, and the order the cycle menu walks.
 *
 * Keeping them apart is the point.  While one list served both,
 * focusing a window had to move it in the stacking list even when the
 * window was not being raised, since that was the only place the
 * recency could be recorded; the next desktop switch then replayed
 * that order onto the X server and the window rose after all,
 * although nobody had asked it to.  A window manager whose
 * @c windows.focus.raise is off, which is its default, cannot express
 * "focused but not raised" with one list at all.
 *
 * Openbox draws the same distinction, holding a @c stacking_list and a
 * @c focus_order side by side; its own fallback walks the latter.  The
 * difference here is that its list is one global one filtered by
 * desktop as it goes, because its clients are global and carry a
 * desktop field, whereas here a desktop owns its clients, so the list
 * belongs to the desktop and needs no filtering.
 *
 * The head is the most recently focused client and the tail the least.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DESKTOP_FOCUS_H
#define DESKTOP_FOCUS_H

/* System includes */
#include <stdbool.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Move a client to the head of its desktop's focus order
 *
 * Called whenever a client actually receives focus, and never as a
 * side effect of raising, moving or restacking it: what this records
 * is that the person was last working in this window, which is a
 * different fact from where the window sits on screen.
 *
 * A client not yet in the list is inserted; one already there is moved
 * rather than duplicated.
 *
 * @param desktop Desktop owning the order
 * @param client  Client that just received focus
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval -1 Invalid @p desktop or @p client
 * @retval  1 The insertion itself failed
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop, as the client is looked up before being moved
 */
int desktop_focus_order_to_top(desktop_td *desktop, client_td *client);

/**
 * @brief Add a client to the tail of its desktop's focus order
 *
 * For a client that has just appeared without taking focus: it belongs
 * in the order so that a later fallback can reach it, but at the far
 * end, since it is the least recently used thing there rather than the
 * most.
 *
 * A client already in the list is left where it is, so that this is
 * safe to call on a path that may run twice.
 *
 * @param desktop Desktop owning the order
 * @param client  Client to record
 *
 * @return Status of the operation
 * @retval  0 Success, or the client was already recorded
 * @retval -1 Invalid @p desktop or @p client
 * @retval  1 The insertion itself failed
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
int desktop_focus_order_add(desktop_td *desktop, client_td *client);

/**
 * @brief Remove a client from its desktop's focus order
 *
 * Called as a client leaves the desktop, whether by being destroyed,
 * by withdrawing itself, or by being sent elsewhere.  A client left in
 * the order after it is gone would be handed focus by the next
 * fallback that reached it.
 *
 * Removing a client that is not in the list is not an error.
 *
 * @param desktop Desktop owning the order
 * @param client  Client to forget
 *
 * @return Status of the operation
 * @retval  0 Success, or the client was not in the order
 * @retval -1 Invalid @p desktop or @p client
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
int desktop_focus_order_remove(desktop_td *desktop, client_td *client);

/**
 * @brief Return the most recently focused client matching a predicate
 *
 * Walks the order from its head, which is the most recently focused
 * client, and returns the first one @p is_valid accepts.
 *
 * @param desktop  Desktop owning the order
 * @param is_valid Predicate deciding whether a candidate qualifies;
 *                 receives each candidate and @p exclude in turn
 * @param exclude  Client to pass to @p is_valid as the one being
 *                 replaced, which may be @c NULL
 *
 * @return The first accepted client, or @c NULL when none qualifies
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
client_td *desktop_focus_order_first(desktop_td *desktop,
        bool (*is_valid)(const client_td *candidate,
                const client_td *exclude),
        const client_td *exclude);

#endif /* !DESKTOP_FOCUS_H */
