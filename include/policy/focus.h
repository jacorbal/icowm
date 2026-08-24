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

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <surface.h>


/* Public interface */
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
