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


/* System includes */
#include <stddef.h>     /* size_t */
#include <stdint.h>     /* uint8_t */

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
 * @brief Override the client's own active-state opacity
 *
 * Sets @c opacity_override.is_set_active/@c .active, so this one
 * client's own active-state opacity stops following the theme's own
 * @p window.active.opacity until unset (there is currently no way
 * to unset it once a rule has set it; see @c rules_apply_s's own
 * doc comment, rules/internal.h).
 *
 * @param client  Window whose active-state opacity to override
 * @param percent New opacity, @c 0 to @c 100
 *
 * @note A null @p client is a silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_set_opacity_active(client_td *client, uint8_t percent);

/**
 * @brief Override the client's own inactive-state opacity
 *
 * Sets @c opacity_override.is_set_inactive/@c .inactive, the
 * inactive-state counterpart to @a ccmd_client_set_opacity_active.
 *
 * @param client  Window whose inactive-state opacity to override
 * @param percent New opacity, @c 0 to @c 100
 *
 * @note A null @p client is a silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_set_opacity_inactive(client_td *client,
        uint8_t percent);

/**
 * @brief Override the client's own border color and width
 *
 * Sets @c border_override.is_set/@c .color/@c .width together, so
 * this one client's own border stops following the theme's own
 * @p window.active/@p .inactive.border until unset (there is
 * currently no way to unset it once set; see @c scratchpad_notice_
 * client_created's own doc comment, scratchpad.c, for the one
 * existing caller).  Deliberately narrow, the same as @a ccmd_client_apply_
 * geometry: only the state itself, nothing about re-applying the
 * border to the actual window right away, which stays each caller's
 * own concern (a caller wanting that immediately, rather than
 * waiting for the next natural @c client_border_apply call a focus
 * change already triggers, still has to make that call itself).
 *
 * @param client Window whose border to override
 * @param color  New border color, an @c 0xRRGGBB-style packed value
 * @param width  New border width in pixels
 *
 * @note A null @p client is a silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_set_border_override(client_td *client,
        uint32_t color, uint32_t width);

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
 * @brief Republish every @c _NET_WM_STATE atom a client currently
 *        holds, read straight off its own fields, in one single XCB
 *        write
 *
 * Openbox's own real answer to keeping @c _NET_WM_STATE in sync
 * (confirmed directly against its source, @c client_change_state in
 * @c client.c): rebuild the whole list from scratch every time, from
 * whichever of the client's own boolean fields are true right now,
 * rather than reading the property back first to add or remove one
 * specific atom.  See the full reasoning in @c cmds/client/ewmh.c,
 * right above the implementation, for exactly how each atom maps to
 * @p client's own fields.
 *
 * @param client Client whose current state to republish
 *
 * @note A null @p client or one with no @c ewmh connection is a
 *       silent no-op
 * @note Implemented in @c cmds/client/ewmh.c
 * @note Complexity: @e O(1)
 */
void ccmd_client_sync_states(client_td *client);

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
 * @note Complexity: @e O(min(d, @c WM_TRANSIENT_CHAIN_MAX_DEPTH) * k),
 *       where @e d is the true depth of mapped transient descendants
 *       and @e k is the number of direct transient children found at
 *       each step along the way
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

/**
 * @brief Move every transient descendant of a client onto whichever
 *        desktop is actually being looked at right now, wherever
 *        they currently are
 *
 * Openbox's own real answer to a transient family split across
 * desktops (confirmed directly against its source): a pinned parent
 * followed to a new desktop leaves its own modal dialog behind, but
 * the moment someone tries to focus that parent again, the dialog is
 * moved onto the desktop the parent is being interacted with on
 * right then, so it is right there to actually receive the
 * redirected focus.  Called from @a focus_apply (@c policy/focus.c)
 * only, immediately before its own redirect to @a ccmd_client_focus_
 * target, not from @a ccmd_client_focus itself (@c cmds/client/
 * focus.c): that function is also reached from purely automatic
 * focus restoration having nothing to do with someone actually
 * interacting with a client right now (@a surface_clients_show's own
 * "restore whichever client was last active" step on every desktop
 * switch foremost among them), and calling this there too dragged a
 * transient family across onto whatever desktop merely happened to
 * be switched to, chasing every desktop its pinned parent had ever
 * been focused on despite never being pinned itself.  Deliberately
 * the desktop currently viewed on the top parent's own surface, not
 * that top parent's own literal "home" desktop, since pinning a
 * client never actually moves it between desktops (see @a ccmd_
 * client_bring_family's own full doc comment, cmds/client/
 * transient.c, for why that distinction matters here specifically).
 *
 * @param client Client whose transient family to bring together;
 *               redirected to its own top-most ancestor first, the
 *               same way every other family-wide action in this
 *               project already does
 *
 * @note A null @p client, one whose top parent's own surface cannot
 *       be resolved, or one with no transient family at all is a
 *       silent no-op
 * @note Implemented in @c cmds/client/transient.c
 * @note Complexity: @e O(f), where @e f is the number of @p client's
 *       own top parent's transient descendants at every depth
 *       combined
 */
void ccmd_client_bring_family(client_td *client);

/**
 * @brief Collect every other member of a transient family found on
 *        one specific desktop into a newly allocated snapshot array
 *
 * Every family-wide action in this project (iconify, restore, pin,
 * unpin, desktop sends, and the like) needs the same thing: every
 * matching sibling collected into an array first, rather than acted
 * on directly while still walking the family tree, since an action
 * on one sibling (an iconify, a pin, a desktop move) can itself add,
 * remove, or otherwise touch entries in that same tree.  Factored
 * out once here instead of repeated at every call site; the caller
 * supplies its own loop over the result to actually act on each one,
 * since what to do with a family member is the one part every call
 * site still needs its own way.
 *
 * @param desktop   Desktop to restrict the result to
 * @param top       Family's own top-most ancestor (see @a ccmd_
 *                  client_transient_top_parent); excluded from the
 *                  result even if found on @p desktop itself
 * @param count_out Receives the number of clients collected; set to
 *                  @c 0 on any early return, including allocation
 *                  failure
 *
 * @return Newly allocated array of @c *count_out client pointers,
 *         the caller's own to @c free; @c NULL if @p desktop, @p top,
 *         or @p count_out is @c NULL, no match was found on @p
 *         desktop, or the allocation itself failed
 *
 * @note Implemented in @c cmds/client/transient.c
 * @note Complexity: @e O(f), where @e f is the number of @p top's
 *       own transient descendants at every depth combined
 */
client_td **ccmd_client_transient_family_snapshot(desktop_td *desktop,
        client_td *top, size_t *count_out);

/**
 * @brief Collect every other member of a transient family, found on
 *        any desktop of any surface, into a newly allocated snapshot
 *        array
 *
 * The all-desktops counterpart to @a ccmd_client_transient_family_
 * snapshot (see its own doc comment for the shared reasoning behind
 * collecting into a snapshot at all): every family-wide action that
 * is not itself about desktops (iconify, restore, hide, unhide, pin,
 * unpin) must find every family member regardless of which desktop
 * each one happens to be registered under, not just @p top's own;
 * those two can genuinely differ when @p top is pinned, since
 * pinning a client never actually moves it between desktops.
 * Scoping the search to @p top's own desktop alone, as the desktop-
 * move actions genuinely need to (@a enact_desktop_client_send, @a
 * hi_handle_net_wm_desktop, @a drag_warp_tick, and @a ccmd_client_
 * bring_family itself, which each still use @a ccmd_client_
 * transient_family_snapshot directly for exactly that reason),
 * silently fails to find a transient living elsewhere.  Counts every
 * match first, allocates exactly that many slots once, then fills
 * them in an identical second pass, rather than growing one array as
 * matches are found across desktops (a genuinely riskier design that
 * turned out to crash outright rather than just misbehave, when this
 * same idea was tried once already); see the full reasoning in
 * @c cmds/client/transient.c, right above the implementation.
 *
 * @param top       Family's own top-most ancestor (see @a ccmd_
 *                  client_transient_top_parent); excluded from the
 *                  result even where found
 * @param count_out Receives the number of clients collected; set to
 *                  @c 0 on any early return
 *
 * @return Newly allocated array of @c *count_out client pointers,
 *         the caller's own to @c free; @c NULL if @p top or @p
 *         count_out is @c NULL, no family member was found anywhere,
 *         or the allocation itself failed
 *
 * @note Implemented in @c cmds/client/transient.c
 * @note Complexity: @e O(f), where @e f is the number of @p top's
 *       own transient descendants at every depth combined
 */
client_td **ccmd_client_transient_family_snapshot_anywhere(
        client_td *top, size_t *count_out);

/**
 * @brief Link a newly managed client into its parent's transient
 *        tree, if @c transient_for names an already-managed client
 *
 * Called once, right after a newly mapped client is added to its own
 * desktop, so every other transient-family function in this project
 * can walk real @c client_td* pointers (@c transient_parent going
 * up, @c transients going down) instead of re-discovering "who is
 * transient for whom" by scanning every client on every desktop and
 * comparing @c transient_for window IDs each time it matters.
 *
 * @param client Newly managed client to link
 *
 * @note A null @p client, one not transient for anything, or one
 *       whose declared parent is not (or not yet) managed, is a
 *       silent no-op
 * @note Implemented in @c cmds/client/transient.c
 * @note Complexity: @e O(1)
 */
void client_link_transient(client_td *client);

/**
 * @brief Unlink a client from the transient tree before it stops
 *        being managed
 *
 * Removes @p client from its own parent's @c transients list in true
 * @e O(1), and orphans every one of @p client's own children by
 * clearing their own @c transient_parent/@c transient_node back to
 * @c NULL, since the parent they were transient for is going away.
 * Must be called before @p client itself is actually freed, from
 * whichever code path is unmanaging it.
 *
 * @param client Client about to stop being managed
 *
 * @note A null @p client is a silent no-op
 * @note Implemented in @c cmds/client/transient.c
 * @note Complexity: @e O(k), where @e k is the number of @p client's
 *       own direct transient children
 */
void client_unlink_transient(client_td *client);

/**
 * @brief Unmap a client's decoration target, correctly pre-arming
 *        @c ignore.unmap first
 *
 * Every place in this project that unmaps a client window-manager-
 * side (iconifying, hiding for another desktop, sending it
 * elsewhere) shares this exact same two-step shape: increment
 * @c client->ignore.unmap by however many @c UnmapNotify events the
 * unmap below is about to generate, THEN issue the unmap itself, so
 * @a handler_unmap_notify (@c handler/map.c) correctly recognizes
 * this as a window-manager-initiated unmap rather than the client
 * withdrawing itself.  Two events always arrive for @p target itself
 * (its own @c StructureNotify plus its parent's own
 * @c SubstructureNotify); one further event arrives for the
 * titlebar, if present, via the frame's own @c SubstructureNotify.
 *
 * A caller whose own @p target can differ from @p client->window
 * (the frame, when decorated, rather than the bare content window)
 * and that also needs the content window itself unmapped separately
 * (@a ccmd_client_iconify and @a ccmd_client_hide, cmds/client/
 * visibility.c, are the only two such callers today) still has to
 * account for, and issue, that additional unmap on its own right
 * after calling this: two more events arrive for @c client->window
 * in that case, matching this same two-events-per-window rule, and
 * this function only knows about the one @p target it was actually
 * given.
 *
 * @param client     Client being unmapped; its own @c ignore.unmap is
 *                    incremented here
 * @param connection Connection to issue the unmap requests on
 * @param target     Window to unmap (the frame if decorated, the
 *                    bare content window otherwise; see @a ccmd_
 *                    target_win, cmds/client/screen.c)
 *
 * @note A null @p client or @p connection is a silent no-op
 * @note Implemented in @c cmds/client/visibility.c
 * @note Complexity: @e O(1)
 */
void ccmd_client_unmap_decorated(client_td *client,
        xcb_connection_t *connection, xcb_window_t target);


#endif  /* ! CMDS_CCMD_BASIC_H */
