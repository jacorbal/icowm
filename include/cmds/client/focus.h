/**
 * @file cmds/client/focus.h
 *
 * @brief Functions on a client's close, kill, restore, and focus
 *        lifecycle
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

#ifndef CMDS_CCMD_FOCUS_H
#define CMDS_CCMD_FOCUS_H


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
 * elsewhere for the identical reasoning).
 *
 * Run twice, not once: a first pass over that same search restricted
 * to clients sharing @p exclude's own @c WM_CLIENT_LEADER (ICCCM
 * §4.1.2.5) takes precedence over an equally-recent but unrelated
 * window, the same group-awareness @a place_window_apply
 * (policy/placement/window.c) already applies when placing a new
 * sibling window, and the same reasoning Openbox's own
 * @c focus_valid_target (focus.c) weighs group membership for.
 * A second, plain pass with no group restriction runs only when the
 * first finds nothing, so a client with no group-mates left visible
 * falls back exactly as it always did.
 *
 * The winner, if any, is given real focus through
 * @a ccmd_client_focus itself, not a raw @c xcb_set_input_focus, so
 * urgency
 * clearing, the ICCCM input model, @c WM_TAKE_FOCUS, and every other
 * side effect real focus already carries apply here exactly as they
 * do anywhere else focus is granted.  Relinquishes focus to
 * @c PointerRoot instead when no candidate qualifies, so the desktop is
 * never left with stale keyboard focus on a client no longer meant
 * to hold it.
 *
 * Deliberately never falls back onto a client that merely happens to
 * be visible without anyone having actually focused it themselves,
 * e.g., a pinned window on loan from whichever desktop it actually
 * got focused on: callers that only want a fallback under that
 * narrower condition already gate the call on their own desktop's
 * remembered active client having been genuinely set (see
 * @c surface_clients_show's own two-block split,
 * surface/actions/clients.c, for exactly this distinction).
 *
 * @param desktop Desktop whose stacking order is searched, and whose
 *                own @c client_active_id / @c is_focus_dirty are updated
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


#endif  /* ! CMDS_CCMD_FOCUS_H */
