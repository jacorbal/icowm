/**
 * @file cmds/client/transient.h
 *
 * @brief Functions on a client's transient family: the top-most
 *        ancestor, group-transient anchor resolution, cross-desktop
 *        bring-together, family-wide snapshots, and the transient
 *        tree's own link/unlink lifecycle
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

#ifndef CMDS_CCMD_TRANSIENT_H
#define CMDS_CCMD_TRANSIENT_H


/* System includes */
#include <stddef.h>     /* size_t */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>


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
 * @brief Resolve which currently-mapped sibling a client transient
 *        for its whole group (ICCCM §4.1.2.6) should be treated as
 *        attached to right now, for stacking, raising, and focus
 *        redirect
 *
 * Close kin to @c s_place_window_transient_centered's search
 * (@c policy/placement/window.c), used there for this same client's
 * initial centering, with one deliberate difference: this one
 * excludes another client also transient for its group, so an anchor
 * is always an actual application window of the group, never another
 * such dialog (see this function's implementation comment,
 * @c cmds/client/transient.c, for why two such dialogs resolving to
 * each other would matter here specifically, unlike for centering).
 * Resolved fresh each call, rather than cached, since which sibling
 * qualifies can change as windows map, unmap, or iconify around it.
 * A client with @c is_transient_for_group set has no @c
 * transient_parent (root is never a managed client for @c
 * client_link_transient's lookup to find), so wherever that field
 * would otherwise be read to find a specific parent, this is the
 * equivalent for one transient for its whole group instead.
 *
 * @param client Client to resolve an anchor for
 *
 * @return The first currently-mapped (non-iconified), non-group-
 *         transient sibling sharing @p client's group leader, on
 *         @p client's desktop, or @c NULL if @p client is not
 *         transient for its group, has no group leader, or no such
 *         sibling currently qualifies
 *
 * @note Implemented in @c cmds/client/transient.c
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p client's desktop
 */
client_td *client_group_transient_anchor(const client_td *client);

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
client_td **ccmd_client_transient_family_snapshot(const desktop_td *desktop,
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


#endif  /* ! CMDS_CCMD_TRANSIENT_H */

