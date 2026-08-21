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


/* ICCCM WM_STATE property values (ICCCM section 4.1.3.1) */
#define CCMD_WM_STATE_WITHDRAWN (0u)
#define CCMD_WM_STATE_NORMAL (1u)
#define CCMD_WM_STATE_ICONIC (3u)


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
 * The winner, if any, is
 * given real focus through @a ccmd_client_focus itself (not a raw
 * @c xcb_set_input_focus), so urgency clearing, the ICCCM input
 * model, @c WM_TAKE_FOCUS, and every other side effect real focus
 * already carries apply here exactly as they do anywhere else focus
 * is granted.  Relinquishes focus to @c PointerRoot instead when no
 * candidate qualifies, so the desktop is never left with stale
 * keyboard focus on a client no longer meant to hold it.
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
 *                own @c client_active_id / @c focus_dirty are updated
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
 * @brief Move an already-iconified client's own icon to a fresh,
 *        non-overlapping spot if its current one is now occupied
 *
 * For a client whose icon window already exists (unlike
 * @a ccmd_client_iconify, which creates one from scratch): checks
 * @p client's own current @c icon_x/icon_y against every other
 * already-mapped icon on whichever desktop @p client is on right
 * now, and, only if that exact spot is taken, resolves a new one via
 * @a place_icon_apply and moves the icon window there on screen if it
 * is currently mapped.  A no-op otherwise, so an icon that still has
 * a free spot keeps it exactly where it was.
 *
 * Meant for a client whose desktop just changed out from under it
 * without the person ever explicitly moving its icon themselves
 * (@a surface_action_desktop_remove, surface.h, evacuating every
 * client still on the desktop being removed foremost among them):
 * the ordinary "reuse the saved position unless claimed" logic
 * @a ccmd_client_iconify itself already applies to a freshly iconified
 * client has no equivalent for one that arrives on a desktop it was
 * never actually iconified on.
 *
 * @param client Client whose own icon position to check and, if
 *               needed, relocate
 *
 * @note No-op if @p client is @c NULL, has no icon window, or is not
 *       currently iconified
 * @note Complexity: @e O(n), where @e n is the number of already-
 *       iconified clients on @p client's own current desktop
 */
void ccmd_client_relocate_icon_if_taken(client_td *client);

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

/**
 * @brief Return the frame window when decorated, otherwise the client
 *        window
 *
 * @param client Pointer to the client to inspect
 *
 * @return Frame window when available, client window otherwise;
 *         @c XCB_WINDOW_NONE if @p client is @c NULL
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client);

/**
 * @brief Find which monitor a client is currently on
 *
 * Resolves @p client's surface from the global @c wm singleton, then
 * finds whichever of that surface's monitors @p client's own center
 * point currently falls on.
 *
 * @param client      Client to resolve a monitor for
 * @param out_surface Receives the resolved surface (may be @c NULL)
 * @param out_monitor Receives the resolved monitor's raw geometry
 *                     (screen edges, not adjusted for panel/dock
 *                     struts)
 *
 * @return @c true on success, @c false if the client's surface could
 *         not be found; callers fall back to @c ccmd_screen_dim's raw
 *         screen size in that case
 *
 * @note Implemented in @c cmds/client/screen.c
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
bool ccmd_client_monitor(client_td *client, surface_td **out_surface,
        monitor_td *out_monitor);

/**
 * @brief Passively grab all mouse buttons on an undecorated client
 *
 * Installs a synchronous passive grab on the client window so the
 * window manager can focus the client on click before replaying or
 * consuming the button event.
 *
 * @param client Pointer to the client
 *
 * @note Implemented in @c cmds/client/grab.c
 * @note Complexity: @e O(1)
 */
void ccmd_client_grab_buttons(client_td *client);

/**
 * @brief Write the ICCCM @c WM_STATE property for a client
 *
 * Stores the client state and optional icon window in the legacy
 * @c WM_STATE property expected by pagers, taskbars, and older X11
 * clients.
 *
 * @param client      Pointer to the client
 * @param state       ICCCM window-manager state value
 * @param icon_window Icon window associated with @p state, or
 *                    @c XCB_NONE
 *
 * @note Implemented in @c cmds/client/ewmh.c
 * @note Complexity: @e O(n), where @e n is the length of @c WM_STATE
 */
void ccmd_set_wm_state(client_td *client,
        uint32_t state, xcb_window_t icon_window);

/**
 * @brief Remove the ICCCM @c WM_STATE property from a client
 *
 * Deletes the legacy @c WM_STATE property, typically when the client is
 * being withdrawn from window-manager control.
 *
 * @param client Pointer to the client
 *
 * @note Implemented in @c cmds/client/ewmh.c
 * @note Complexity: @e O(n), where @e n is the length of @c WM_STATE
 */
void ccmd_clear_wm_state(client_td *client);

/**
 * @brief Add multiple EWMH window states to a client
 *
 * @param client     Pointer to the client
 * @param num_states Number of state name strings that follow
 * @param ...        @c (const char*) state name arguments
 *
 * @note Implemented in @c cmds/client/ewmh.c
 * @note Complexity: @e O(n), where @e n is @p num_states
 */
void ccmd_add_states(client_td *client, uint32_t num_states, ...);

/**
 * @brief Remove multiple EWMH window states from a client
 *
 * @param client     Pointer to the client
 * @param num_states Number of state name strings that follow
 * @param ...        @c (const char*) state name arguments
 *
 * @note Implemented in @c cmds/client/ewmh.c
 * @note Complexity: @e O(n * m), where @e n is @p num_states and @e m
 *       is the current number of window states
 */
void ccmd_rem_states(client_td *client, uint32_t num_states, ...);

/**
 * @brief Walk down from a client to whichever mapped transient
 *        descendant should actually receive focus in its place
 *
 * ICCCM §4.1.2.6 dialogs exist to demand a specific answer before
 * their own parent is usable again in any meaningful sense.  Called
 * both from @a ccmd_client_focus itself (@c cmds/client/focus.c) and
 * from @a focus_apply (@c policy/focus.c): the latter needs its own
 * copy of the redirected client, resolved before it does any of its
 * own "currently active client" bookkeeping (@c desktop->
 * client_active_id and the stacking-order raise), since a callee
 * reassigning its own local copy of a pointer parameter (inside
 * @a ccmd_client_focus) can never be observed by its caller.  Without
 * this, a caller of @a focus_apply targeting the parent (a plain
 * click, sloppy focus, restoring the group, and the like) leaves
 * @c client_active_id on the parent even though real X11 input focus
 * correctly ends up on the dialog, and that mismatch is what leaves
 * keyboard shortcuts unable to find "the active client" at all once
 * the dialog later closes.
 *
 * @param client Client focus was actually requested for
 *
 * @return The deepest mapped transient descendant found, or
 *         @p client itself if it has none (or @p client is @c NULL)
 *
 * @note Implemented in @c cmds/client/transient.c
 * @note Searches every desktop of every surface at each step, not
 *       just the current target's own desktop, so a transient family
 *       that ends up split across desktops (which should never
 *       happen by design, but is not assumed here) is still found
 *       correctly rather than silently falling back to focusing the
 *       parent.
 * @note Complexity: @e O(min(d, @c WM_TRANSIENT_CHAIN_MAX_DEPTH) *
 *       s * d2 * n), where @e d is the true depth of mapped
 *       transient descendants, @e s is the number of surfaces,
 *       @e d2 the number of desktops per surface, and @e n the
 *       number of clients per desktop
 */
client_td *ccmd_client_focus_target(client_td *client);

/**
 * @brief Walk up a client's @c WM_TRANSIENT_FOR chain to its top-most
 *        managed ancestor
 *
 * ICCCM §4.1.2.6 lets a dialog be transient for another dialog, which
 * is itself transient for a third window, and so on; this follows that
 * whole chain to find the one client at its root, the "main"
 * application window the entire chain ultimately belongs to.  Used by
 * @a ccmd_client_iconify and @a ccmd_client_restore (@c cmds/client/
 * visibility.c and @c cmds/client/focus.c) and by @a enact_desktop_
 * client_send (@c enact/desktop.c) to redirect an iconify, restore,
 * or desktop change requested on any single member of a transient
 * family to the family as a whole, the same way a person would
 * expect minimizing (or sending to another desktop) a "save changes?"
 * prompt to take its parent editor window down with it, not leave
 * the two stranded apart.
 *
 * @param client Client whose transient chain to walk; returned as-is
 *               if it is not transient for anything, or if its
 *               declared parent is not (or not yet) a managed client
 *
 * @return The top-most client in the chain, or @c NULL if @p client
 *         itself is @c NULL
 *
 * @note Implemented in @c cmds/client/transient.c
 * @note Complexity: @e O(min(d, @c WM_TRANSIENT_CHAIN_MAX_DEPTH)),
 *       where @e d is the true depth of the transient chain
 */
client_td *ccmd_client_transient_top_parent(client_td *client);


#endif  /* ! CMDS_CCMD_BASIC_H */
