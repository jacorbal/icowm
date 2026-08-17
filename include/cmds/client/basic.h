/**
 * @file cmds/client/basic.h
 *
 * @brief Functions on executions over clients using the XCB interface
 *        with needed EWMH and ICCCM updates
 *
 * @defgroup cmds Client, desktop, and surface commands
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_CCMD_BASIC_H
#define CMDS_CCMD_BASIC_H


/* Command includes */
#include <cmds/client/state.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>


/* Public interface */
/**
 * @brief Perform the action to close the client
 *
 * @param client Window to close
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_close(client_td *client);

/**
 * @brief Forcibly terminate the client's connection to the X server
 *
 * Unlike @a ccmd_client_close (which only destroys the client's window
 * resource), this severs the client's entire X connection at the
 * protocol level via @a xcb_kill_client, matching the conventional
 * "force kill an unresponsive window" behavior (e.g., @c xkill).
 * Intended as a last resort for clients that do not react to a normal
 * close request.
 *
 * @param client Window whose owning client connection should be
 *               forcibly terminated
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_kill(client_td *client);

/**
 * @brief Restore the client to its original state
 *
 * @param client Window to restore
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_restore(client_td *client);

/**
 * @brief Focus on the given client
 *
 * @param client Window to focus
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_focus(client_td *client);

/**
 * @brief Transfer input focus away from a client that is leaving the
 *        current visible focus chain (closed, iconified, hidden, or
 *        no longer the desktop's own remembered active client), to
 *        the most recently used other visible, focusable client on
 *        the same desktop
 *
 * Searches @p desktop's own stacking order from the top down for the
 * first client that is not @p exclude, not hidden, not shaded, not
 * iconified, focusable, not flagged @c CLIENT_FLAG_NO_FOCUS_FALLBACK
 * (the scratchpad; see @a client_set_no_focus_fallback), and not
 * flagged @c CLIENT_FLAG_SKIP_TASKBAR unless it is modal, urgent, or
 * a dialog (each already important enough on its own to reach for
 * regardless, the same three exceptions @c CLIENT_TYPE_DIALOG,
 * @a client_is_modal, and @a client_is_urgent already carve out
 * elsewhere for the identical reasoning).  The winner, if any, is
 * given real focus through @a ccmd_client_focus itself (not a raw
 * @a xcb_set_input_focus), so urgency clearing, the ICCCM input
 * model, @c WM_TAKE_FOCUS, and every other side effect real focus
 * already carries apply here exactly as they do anywhere else focus
 * is granted.  Relinquishes focus to @c PointerRoot instead when no
 * candidate qualifies, so the desktop is never left with stale
 * keyboard focus on a client no longer meant to hold it.
 *
 * Deliberately never falls back onto a client that merely happens to
 * be visible without anyone having actually focused it themselves,
 * e.g., a pinned window on loan from whichever desktop it actually
 * got focused on.  Callers that only want a fallback under that
 * narrower condition already gate the call on their own desktop's
 * remembered active client having been genuinely set (see
 * @a surface_clients_show's own two-block split, @c surface/actions.c,
 * for exactly this distinction).
 *
 * @param desktop Desktop whose stacking order is searched, and whose
 *                own @c client_active_id / @p focus_dirty are updated
 * @param surface Surface @p desktop belongs to, marked outdated
 * @param exclude Client to exclude from the search (the one losing
 *                focus); may be null
 *
 * @note No-op if @p desktop is null
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
void client_focus_fallback(desktop_td *desktop, surface_td *surface,
        const client_td *exclude);

/**
 * @brief Remove focus from the given client
 *
 * @param client Window to unfocus
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unfocus(client_td *client);

/**
 * @brief Perform the action to iconify (and minimize it)
 *
 * @param client Window to iconify
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_iconify(client_td *client);

/**
 * @brief Hide the client by minimizing it without iconifying
 *
 * @param client Window to hide
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_hide(client_td *client);

/**
 * @brief Show (unhide) the client
 *
 * @param client Window to unhide
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unhide(client_td *client);

/**
 * @brief Pin the client (visible on all desktops)
 *
 * @param client Window to pin
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_pin(client_td *client);

/**
 * @brief Unpin the client
 *
 * @param client Window to unpin
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unpin(client_td *client);

/**
 * @brief Toggle pin mode for the client
 *
 * @param client Window to toggle pin state
 *
 * @note No-op on a surface with only one desktop: stickiness has
 *       nothing to actually toggle when there is only the one
 * @note Complexity: @e O(1)
 */
void ccmd_client_toggle_pin(client_td *client);

/**
 * @brief Mark the client as urgent (requesting attention)
 *
 * @param client Window to mark as urgent
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_urge(client_td *client);

/**
 * @brief Clear urgency marking from the client
 *
 * @param client Window to clear urgency
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unurge(client_td *client);

/**
 * @brief Publish @c _NET_WM_ALLOWED_ACTIONS for a client
 *
 * Computes the set of EWMH actions currently permitted for @p client
 * based on its resizable, focusable, and decoration properties, and
 * writes the result to the @c _NET_WM_ALLOWED_ACTIONS window property.
 * Must be called whenever the client's capabilities change (e.g., after
 * toggling resizability or decoration).
 *
 * @param client Client whose allowed-actions property should be updated
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_update_allowed_actions(client_td *client);


#endif  /* ! CMDS_CCMD_BASIC_H */
