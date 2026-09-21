/**
 * @file cmds/client/focus.h
 *
 * @brief Functions on a client's close, kill, restore, and focus
 *        lifecycle
 *
 * @defgroup cmds Client, desktop, and stage commands
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


/* Type includes */
#include <types/handles.h>

/* Project includes */
#include <desktop.h>


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
 * "Original" depends on where the client currently sits.  An iconified
 * client is restored by bringing it back onto the desktop, its own
 * icon box destroyed and its saved geometry re-applied, but whatever
 * maximized or full screen bit it held before being iconified is left
 * exactly as it was; only the iconified state itself is undone.
 *
 * A client already on the desktop is restored one layer at a time
 * instead, outermost first: full screen over a maximized window comes
 * back maximized, and a second restore takes that away in turn.  This
 * relies on @a ccmd_client_maximize (and @a ccmd_client_maximize_horz
 * / @a _vert) already being their own toggle, each one demoting a
 * client already holding the exact state being asked for back to
 * normal instead of re-applying it; restore simply calls whichever one
 * matches the outermost state currently held, rather than needing an
 * "undo maximize" of its own.
 *
 * The client's whole transient family is brought along too: every
 * other member still iconified is restored first, and a transient
 * sibling left merely hidden, not iconified, when the family went down
 * together is unhidden the same way, before @p client's own top parent
 * is finally restored last.
 *
 * @param client Window to restore
 *
 * @note Complexity: @e O(f), where @e f is the number of @p client's
 *       own transient descendants at every depth combined
 */
void ccmd_client_restore(client_td *client);

/**
 * @brief Show, everywhere outside the X server's own focus, that
 *        a client now holds input focus
 *
 * Clears its urgency, installs its colormaps, sets its
 * @c _NET_WM_STATE_FOCUSED, repaints its frame as active and names it
 * in @c _NET_ACTIVE_WINDOW.  Leaves the real input focus itself untouched:
 * @a ccmd_client_focus gives it first, and @a focus_adopt calls this
 * alone for a client that already took it on its own.
 *
 * @param client Client that holds input focus
 *
 * @note A no-op if @p client is @c NULL
 * @note Complexity: @e O(c), where @e c is the number of colormap
 *       windows @p client lists
 */
void ccmd_client_focus_publish(client_td *client);

/**
 * @brief Give the focus back to a client its desktop already counts as
 *        active, if it is on screen
 *
 * For a command that remaps or reshapes a client (shading, full screen,
 * decoration) and must hand the focus back to it when it was the one in
 * use, without taking the focus from any other window: a client that is
 * not the active one of its desktop, or not on screen, is left alone.
 *
 * @param client Client to refocus
 *
 * @note A no-op for a @c NULL @p client
 * @note Complexity: @e O(1)
 */
void ccmd_client_refocus_if_active(client_td *client);

/**
 * @brief Focus on the given client
 *
 * @param client Window to focus
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_focus(client_td *client);

/**
 * @brief Make a client the active one of its own desktop
 *
 * The whole of what "this window is now the one in use" means.  The
 * client previously active gives up its focus decoration, this one
 * takes the desktop's active slot, rises to the top of the stacking
 * order and receives real input focus.
 *
 * Held in one place because a window arriving back on screen does all
 * four wherever it arrives from, and doing three of them is what left
 * a window still wearing the active border after another had taken the
 * focus from it.  An undecorated window is where that shows.  Its
 * border is an attribute written only when focus changes, whereas
 * a decorated one is repainted from its focus state on the next pass
 * and quietly corrects itself.
 *
 * @param client Client to make active; may be null
 *
 * @note A client that cannot take focus is left alone entirely,
 *       stacking order included: it was never going to hold the
 *       desktop's active slot
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients, from finding the previously active one
 */
void ccmd_client_make_active(client_td *client);

/**
 * @brief Transfer input focus away from a client that is leaving the
 *        current visible focus chain (closed, iconified, hidden, or no
 *        longer the desktop's remembered active client), to the most
 *        recently used other visible, focusable client on the same
 *        desktop
 *
 * Searches @p desktop's stacking order from the top down for the first
 * client that is not @p exclude, not hidden, not shaded, not iconified,
 * focusable, not flagged @c CLIENT_FLAG_NO_FOCUS_FALLBACK (the
 * scratchpad; see @a client_set_no_focus_fallback), and not flagged
 * @c CLIENT_FLAG_SKIP_TASKBAR unless it is modal, urgent, or a dialog
 * (each already important enough on its own to reach for regardless,
 * the same three exceptions @c CLIENT_TYPE_DIALOG, @a client_is_modal,
 * and @a client_is_urgent already carve out elsewhere for the identical
 * reasoning).
 *
 * With @c windows.focus.group-fallback set, run twice, not once:
 * a first pass over that same search restricted to clients sharing
 * @p exclude's @c WM_CLIENT_LEADER (ICCCM §4.1.2.5), or failing that
 * its @c WM_HINTS window group, takes precedence over an
 * equally-recent but unrelated window, the same group-awareness
 * @a place_window_apply (@c policy/placement/window.c) already applies
 * when placing a new sibling window.  A second, plain pass with no
 * group restriction runs only when the first finds nothing, so a client
 * with no group-mates left visible falls back exactly as it always did.
 * Unset, which is the default, only that plain pass runs.
 *
 * The winner, if any, is given real focus through @a ccmd_client_focus
 * itself, not a raw @c xcb_set_input_focus, so urgency clearing, the
 * ICCCM input model, @c WM_TAKE_FOCUS, and every other side effect real
 * focus already carries apply here exactly as they do anywhere else
 * focus is granted.  Relinquishes focus to @c PointerRoot instead when
 * no candidate qualifies, so the desktop is never left with stale
 * keyboard focus on a client no longer meant to hold it.
 *
 * Deliberately never falls back onto a client that merely happens to be
 * visible without anyone having actually focused it themselves, e.g.,
 * a pinned window on loan from whichever desktop it actually got
 * focused on: callers that only want a fallback under that narrower
 * condition already gate the call on their own desktop's remembered
 * active client having been genuinely set (see
 * @c stage_client_show_all's two-block split,
 * @c stage/actions/client.c, for exactly this distinction).
 *
 * @param desktop Desktop whose stacking order is searched, and whose
 *                @c client_active_id and @c is_focus_dirty are updated
 * @param stage   Stage @p desktop belongs to, marked outdated
 * @param exclude Client to exclude from the search (the one losing
 *                focus); may be null.  Unfocused in place when no
 *                replacement candidate is found
 *
 * @note No-op if @p desktop is null
 * @note On a desktop other than the one @p stage shows, only records
 *       the replacement as that desktop's active client, since no
 *       window there can take the focus and giving it up would take it
 *       from the desktop that is shown; a pinned @p exclude is on
 *       screen anyway and takes the normal path
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
void client_focus_fallback(desktop_td *desktop, stage_td *stage,
        client_td *exclude);

/**
 * @brief Show, everywhere outside the X server's own focus, that
 *        a client no longer holds input focus
 *
 * Clears its @c _NET_WM_STATE_FOCUSED and repaints its frame as
 * inactive.  Unlike @a ccmd_client_unfocus, never moves the real input
 * focus, which may already be on another window that must keep it.
 *
 * @param client Client that lost input focus
 *
 * @note A no-op if @p client is @c NULL
 * @note Complexity: @e O(1)
 */
void ccmd_client_unfocus_publish(client_td *client);

/**
 * @brief Remove focus from the given client
 *
 * @param client Window to unfocus
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unfocus(client_td *client);

/**
 * @brief Transfer focus away from a client that is losing it
 *
 * Thin wrapper resolving @p client's stage and desktop before
 * deferring to @a client_focus_fallback itself; a no-op unless
 * @p client is genuinely that desktop's current active client, since
 * some other, already-unfocused client being hidden or iconified has no
 * focus of its own to hand off in the first place.
 *
 * Note that the desktop resolved is the one @p client lives on and not
 * whichever is showing, the two being different whenever a client loses
 * focus while the user is looking elsewhere.
 *
 * @param client Client that is being hidden or iconified.  Unfocused in
 *               place when no replacement candidate is found
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       that desktop
 */
void ccmd_client_focus_fallback(client_td *client);


#endif  /* ! CMDS_CCMD_FOCUS_H */
