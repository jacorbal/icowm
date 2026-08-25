/**
 * @file policy/focus.h
 *
 * @brief Client focus policy and application
 *
 * Declares the focus-policy predicate and the function that applies
 * input focus to a managed client, handling the previous focus,
 * optional raise, and surface/desktop outdated marking.
 *
 * @defgroup policy Client focus and placement policy
 * @ingroup client
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_FOCUS_H
#define POLICY_FOCUS_H

/* System includes */
#include <stdbool.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Record a newly managed client in the focus order
 *
 * The focus order is one list over every managed client, held
 * most-recently-focused first, and it is what decides where focus
 * goes whenever the window holding it stops being able to.  It is
 * deliberately not the stacking order, which says what is drawn over
 * what: raising a window changes that one and focusing a window does
 * not, so a single list cannot answer both questions.
 *
 * One list for the session rather than one per desktop, which is how
 * Openbox holds its own @c focus_order.  A client keeps its place
 * while moving from desktop to desktop, and a desktop's focus is
 * worked out by filtering this order rather than remembered
 * separately, where it would go stale the moment the window it named
 * was iconified, hidden, or carried elsewhere.
 *
 * A client is placed at the far end here, as the least recently used
 * thing: it exists, so a fallback must be able to reach it, but
 * nobody has worked in it yet.
 *
 * @param client Client that has just come under management
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
void focus_order_add(client_td *client);

/**
 * @brief Forget a client that is no longer managed
 *
 * Called as a client is destroyed, and never merely because it
 * changed desktop: the order spans every managed client whichever
 * desktop holds it, and that is exactly what lets a pinned window
 * keep its place while following the person around.
 *
 * @param client Client to forget
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
void focus_order_remove(const client_td *client);

/**
 * @brief Move a client to the front of the focus order
 *
 * @param client Client that has just received focus
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
void focus_order_to_top(client_td *client);

/**
 * @brief Most recently focused client on a desktop that may hold
 *        focus now
 *
 * Walks the order from its front and returns the first client that
 * belongs to @p desktop and that @p is_valid accepts.
 *
 * @param desktop  Desktop to restrict the search to
 * @param is_valid Predicate deciding whether a candidate qualifies
 * @param data     Opaque pointer handed to @p is_valid, carrying
 *                 whatever else that predicate needs to decide
 *
 * @return The client to focus, or @c NULL when none qualifies
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
client_td *focus_order_best(const desktop_td *desktop,
        bool (*is_valid)(const client_td *candidate, void *data),
        void *data);

/**
 * @brief Visit every client of a desktop in focus order
 *
 * Calls @p visit on each client belonging to @p desktop, from the
 * most recently focused toward the least.  For a caller that needs
 * the whole ordering rather than only its first eligible member, the
 * cycle menu being the one that does.
 *
 * @param desktop Desktop to restrict the walk to
 * @param visit   Called once per client, with @p data passed through
 * @param data    Opaque pointer handed to @p visit
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
void focus_order_walk(const desktop_td *desktop,
        void (*visit)(client_td *client, void *data), void *data);

/**
 * @brief Release the focus order
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
void focus_order_destroy(void);

/**
 * @brief Determine whether the loaded focus policy follows the pointer
 *
 * @param cfg Configuration to query; may be null
 *
 * @return @c true when focus should follow mouse enter events
 *
 * @note Complexity: @e O(1)
 */
bool focus_is_sloppy(const config_td *cfg);

/**
 * @brief Focus a client and keep focus-related state in sync
 *
 * Updates the active client for the desktop, sends focus and unfocus
 * events as needed, optionally raises the client, marks affected
 * surface and desktop as outdated, and triggers an immediate repaint.
 * Also, regardless of @p raise or @p cfg's own raise-on-focus policy,
 * re-enforces @p desktop's layer stacking whenever @p client or the
 * client just losing focus is fullscreen, so a focused fullscreen
 * client stays above everything else and one that just lost focus
 * falls back into its own real layer immediately either way (see
 * @a ccmd_desktop_enforce_layers's comment).
 *
 * @param surfaces All managed surfaces (needed for unfocus lookup)
 * @param surface  Surface containing the client
 * @param desktop  Desktop tracking the active client
 * @param client   Client to focus
 * @param raise    Whether the client should be raised immediately
 * @param cfg      Active configuration (for raise-on-focus policy)
 *
 * @note Complexity: @e O(1) for focus bookkeeping; up to @e O(n * m)
 *       when immediate surface redraw is triggered after raising, or
 *       @e O(n) when only the fullscreen-related re-enforcement above
 *       runs
 *
 * @note Passing @c NULL for @p surfaces suppresses the unfocus-previous
 *       step; this is safe when the caller has already handled it.
 *
 * @note No-op, leaving whichever client already holds real keyboard
 *       focus untouched, when @a client_accepts_input_focus
 *       (@c client.h) is false for @p client: unfocusing whatever
 *       currently has focus in favor of a client that can never
 *       actually receive it under its own declared ICCCM input
 *       model would leave keyboard input directed nowhere.  Most
 *       callers already gate on @a client_is_focusable before
 *       reaching here, but that macro is about window @e type, not
 *       the ICCCM input model this note is about; see both macros'
 *       own comments in @c client.h for the distinction.
 */
void focus_apply(list_td *surfaces, surface_td *surface,
        desktop_td *desktop, client_td *client,
        bool raise, const config_td *cfg);


#endif  /* ! POLICY_FOCUS_H */
