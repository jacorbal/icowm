/**
 * @file cmds/client/transient.c
 *
 * @brief Transient-family resolution and snapshot collection, shared
 *        by every family-wide action across the project: iconify,
 *        restore, focus, pin/unpin, and desktop moves
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>
#include <stdlib.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Default initial values */
#include <defs/client.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/basic.h>
#include <cmds/client/internal.h>


/**
 * @brief Find the mapped, non-iconified, unlocked, un-hidden direct
 *        transient child of a client, if it has one
 *
 * A single step of @a ccmd_client_focus_target's own walk; split out
 * on its own to keep that walk's loop body simple.  Walks @p
 * client's own @c transients list directly (see its own doc comment,
 * client.h): no lookup, no scan of any other client on any desktop,
 * just the handful of pointers @p client's own direct children
 * actually are.  The @c CLIENT_FLAG_HIDDEN check specifically
 * matters for a client in the middle of closing: @a handler_unmap_
 * notify (@c handler/map.c) marks a withdrawing client hidden before
 * it ever calls @a client_focus_fallback, and a fallback landing back
 * on this child's own parent must not find this same closing child
 * here and redirect focus right back onto it.
 *
 * @param client Client whose direct transient children to search
 *
 * @return The first matching child found, or @c NULL if @p client is
 *         @c NULL or has no such child
 *
 * @note Complexity: @e O(k), where @e k is the number of @p client's
 *       own direct transient children
 */
static client_td *s_client_mapped_transient_child(client_td *client)
{
    cdlist_item_td *item;
    const cdlist_item_td *initial;
    desktop_td *desktop;
    xcb_window_t leader;
    void *elem;

    if (client == NULL) {
        return NULL;
    }

    if (client->transients != NULL) {
        item = cdlist_head(client->transients);
        initial = item;
        if (item != NULL) {
            do {
                client_td *const candidate = (client_td *) cdlist_data(item);

                if (candidate != NULL &&
                        !client_is_iconified(candidate) &&
                        !client_is_locked(candidate) &&
                        !(candidate->properties.flags &
                            CLIENT_FLAG_HIDDEN)) {
                    return candidate;
                }
                item = cdlist_next(item);
            } while (item != NULL && item != initial);
        }
    }

    /* No specific-parent child qualified: a client transient for the
     * whole group (ICCCM §4.1.2.6) has no 'transient_parent' to
     * appear in the list just searched above, so it is looked for
     * here separately instead, by group membership rather than a
     * resolved anchor (unlike 's_enforce_layer_place_family' and
     * 's_desktop_transients_raise', client.c and desktop/dclient.c,
     * this has no "only ever trigger once" constraint to protect: any
     * group member redirecting focus to the same open dialog is the
     * whole point, not a bug to guard against). */
    leader = client_group_leader(client);
    if (leader == XCB_WINDOW_NONE) {
        return NULL;
    }

    desktop = wm_get_client_desktop(client);
    if (desktop == NULL || desktop->clients == NULL) {
        return NULL;
    }

    ohtbl_foreach(desktop->clients, elem) {
        client_td *const candidate = (client_td *) elem;

        if (candidate != NULL && candidate != client &&
                candidate->is_transient_for_group &&
                client_group_leader(candidate) == leader &&
                !client_is_iconified(candidate) &&
                !client_is_locked(candidate) &&
                !(candidate->properties.flags & CLIENT_FLAG_HIDDEN)) {
            return candidate;
        }
    }

    return NULL;
}


/**
 * @brief Per-candidate callback used by @a s_visit_descendants
 *
 * @param candidate Family member found; never @c NULL
 * @param ctx       Caller-supplied context, passed through unchanged
 */
typedef void (*s_family_visitor_fn)(client_td *candidate, void *ctx);


/**
 * @brief Recursively walk every transient descendant of a node
 *        (children, grandchildren, and so on), calling @p visit once
 *        for each
 *
 * The shared traversal behind every family-wide snapshot in this
 * file: rather than scanning every client on every desktop and
 * comparing @c transient_for window IDs (the only way this used to
 * be possible, before @c transients existed as a real, maintained
 * list; see its own doc comment, client.h), this walks only @p
 * node's own actual descendants, following real pointers, so its own
 * cost is proportional to the family's own size rather than to how
 * many other, unrelated clients happen to be managed.
 *
 * @param node  Client whose descendants to walk; @p visit is never
 *              called with @p node itself
 * @param visit Callback invoked once per descendant found
 * @param ctx   Opaque context passed through to every @p visit call
 * @param depth Current recursion depth; callers of this function
 *              itself always pass @c 0
 *
 * @note A null @p node or @p visit, or one with no @c transients at
 *       all, is a silent no-op
 * @note Complexity: @e O(f), where @e f is the number of @p node's
 *       own transient descendants at every depth combined
 */
static void s_visit_descendants(client_td *node,
        s_family_visitor_fn visit, void *ctx, uint32_t depth)
{
    cdlist_item_td *item;
    const cdlist_item_td *initial;

    if (node == NULL || visit == NULL || node->transients == NULL ||
            depth >= WM_TRANSIENT_CHAIN_MAX_DEPTH) {
        return;
    }

    item = cdlist_head(node->transients);
    initial = item;
    if (item == NULL) {
        return;
    }

    do {
        client_td *const child = (client_td *) cdlist_data(item);

        if (child != NULL) {
            visit(child, ctx);
            s_visit_descendants(child, visit, ctx, depth + 1);
        }
        item = cdlist_next(item);
    } while (item != NULL && item != initial);
}


/**
 * @brief Context shared by @a s_family_snapshot_visitor's counting
 *        and filling passes, both used by @a s_family_snapshot
 *
 * @c members is @c NULL during the first, counting pass (nothing to
 * write yet, @c count just accumulates a total) and points at a
 * freshly, exactly sized allocation during the second, filling pass.
 */
struct s_family_snapshot_ctx {
    uint32_t desktop_filter; /**< @c WM_DESKTOP_ID_ALL matches every
                                   desktop */
    client_td **members;     /**< @c NULL during the counting pass */
    size_t capacity;         /**< Slots @c members has (filling pass) */
    size_t count;            /**< Matches found so far */
};


/**
 * @brief @a s_family_visitor_fn shared by both @a ccmd_client_
 *        transient_family_snapshot and its all-desktops counterpart,
 *        via @a s_family_snapshot
 *
 * @param candidate Family member found
 * @param ctx       @c struct @a s_family_snapshot_ctx*
 */
static void s_family_snapshot_visitor(client_td *candidate, void *ctx)
{
    struct s_family_snapshot_ctx *const snap =
        (struct s_family_snapshot_ctx *) ctx;

    if (candidate == NULL) {
        return;
    }

    if (snap->desktop_filter != WM_DESKTOP_ID_ALL &&
            candidate->desktop_id != snap->desktop_filter) {
        return;
    }

    if (snap->members == NULL) {
        snap->count++;
        return;
    }

    /* Never writes past 'capacity' even if somehow handed more
     * matches than the first, counting pass counted (nothing mutates
     * the transient tree between the two passes in practice, so both
     * always find the exact same matches, but a stray write past the
     * end of a heap allocation is exactly the class of bug worth
     * guarding against unconditionally rather than trusting that
     * invariant alone). */
    if (snap->count < snap->capacity) {
        snap->members[snap->count] = candidate;
        snap->count++;
    }
}


/**
 * @brief Collect every transient descendant of @p top, optionally
 *        restricted to one desktop, into a newly allocated snapshot
 *        array
 *
 * Shared implementation behind both @a ccmd_client_transient_family_
 * snapshot (@p desktop_filter set to a specific desktop's own @c id)
 * and @a ccmd_client_transient_family_snapshot_anywhere (@p desktop_
 * filter set to @c WM_DESKTOP_ID_ALL, matching every desktop):
 * counts every match first via @a s_visit_descendants, allocates
 * exactly that many slots once, then fills them in an identical
 * second pass, rather than growing one array as matches are found (a
 * genuinely riskier design that turned out to crash outright rather
 * than just misbehave, when this same idea was tried once already,
 * back when this whole file still had to scan every desktop instead
 * of walking a real tree).
 *
 * @param top             Family's own top-most ancestor; excluded
 *                        from the result even where found
 * @param desktop_filter  A specific desktop's own @c id to restrict
 *                        the result to, or @c WM_DESKTOP_ID_ALL to
 *                        match every desktop
 * @param count_out       Receives the number of clients collected;
 *                        set to @c 0 on any early return
 *
 * @return Newly allocated array of @c *count_out client pointers,
 *         the caller's own to @c free; @c NULL if @p top or @p
 *         count_out is @c NULL, no match was found, or the
 *         allocation itself failed
 *
 * @note Complexity: @e O(f), where @e f is the number of @p top's
 *       own transient descendants at every depth combined
 */
static client_td **s_family_snapshot(client_td *top,
        uint32_t desktop_filter, size_t *count_out)
{
    struct s_family_snapshot_ctx ctx;
    client_td **members;

    if (count_out != NULL) {
        *count_out = 0;
    }

    if (top == NULL || count_out == NULL) {
        return NULL;
    }

    ctx.desktop_filter = desktop_filter;
    ctx.members = NULL;
    ctx.capacity = 0;
    ctx.count = 0;
    s_visit_descendants(top, s_family_snapshot_visitor, &ctx, 0);

    if (ctx.count == 0) {
        return NULL;
    }

    members = malloc(ctx.count * sizeof(*members));
    if (members == NULL) {
        return NULL;
    }

    ctx.members = members;
    ctx.capacity = ctx.count;
    ctx.count = 0;
    s_visit_descendants(top, s_family_snapshot_visitor, &ctx, 0);

    *count_out = ctx.count;
    return members;
}


/* Walk up a client's 'WM_TRANSIENT_FOR' chain to its top-most managed
 * ancestor */
client_td *ccmd_client_transient_top_parent(client_td *client)
{
    client_td *top;
    uint32_t depth;

    if (client == NULL) {
        return NULL;
    }

    top = client;
    depth = 0;
    while (top->transient_parent != NULL &&
            depth < WM_TRANSIENT_CHAIN_MAX_DEPTH) {
        top = top->transient_parent;
        depth++;
    }

    return top;
}


/* Resolve which currently-mapped sibling a client transient for its
 * whole group should be treated as attached to right now */
client_td *client_group_transient_anchor(const client_td *client)
{
    xcb_window_t leader;
    desktop_td *desktop;
    void *elem;

    if (client == NULL || !client->is_transient_for_group) {
        return NULL;
    }

    leader = client_group_leader(client);
    if (leader == XCB_WINDOW_NONE) {
        return NULL;
    }

    desktop = wm_get_client_desktop(client);
    if (desktop == NULL || desktop->clients == NULL) {
        return NULL;
    }

    ohtbl_foreach(desktop->clients, elem) {
        client_td *const sibling = (client_td *) elem;

        /* Excludes another client also transient for the group: an
         * anchor is meant to be an actual application window of the
         * group, never another such dialog.  Without this, two
         * group-transient dialogs sharing the same group could
         * resolve to each other (whichever @c ohtbl_foreach happens
         * to visit first), and every caller walking from an anchor
         * back into the family (@a s_enforce_layer_place_family,
         * @c cmds/client/layer.c; @a s_desktop_transients_raise,
         * @c desktop/dclient.c) would recurse into that same pair of
         * dialogs endlessly. */
        if (sibling != NULL && sibling != client &&
                !sibling->is_transient_for_group &&
                client_group_leader(sibling) == leader &&
                sibling->properties.state !=
                    (uint16_t) CLIENT_STATE_ICONIFIED) {
            return sibling;
        }
    }

    return NULL;
}


/**
 * @brief Collect every other member of a transient family found on
 *        one specific desktop into a newly allocated snapshot array
 *
 * Every family-wide action in this project (iconify, restore, pin,
 * unpin, desktop sends, and the like) needs the same thing: every
 * matching sibling collected into an array first, rather than acted
 * on directly while still walking the family tree, since an action
 * on one sibling (an iconify, a pin, a desktop move) can itself add,
 * remove, or otherwise touch entries in that same tree, and altering
 * a structure while still walking it is asking for trouble regardless
 * of which structure it is.  A thin wrapper over @a s_family_snapshot
 * with its own @p desktop_filter set to @p desktop's own @c id; see
 * that function's own doc comment for the full reasoning.  The
 * caller supplies its own loop over the result to actually act on
 * each one, since what to do with a family member is the one part
 * every call site still needs its own way.
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
 * @note Complexity: @e O(f), where @e f is the number of @p top's
 *       own transient descendants at every depth combined
 */
client_td **ccmd_client_transient_family_snapshot(desktop_td *desktop,
        client_td *top, size_t *count_out)
{
    if (count_out != NULL) {
        *count_out = 0;
    }

    if (desktop == NULL) {
        return NULL;
    }

    return s_family_snapshot(top, desktop->id, count_out);
}


/**
 * @brief Collect every other member of a transient family, found on
 *        any desktop of any surface, into a newly allocated snapshot
 *        array
 *
 * The all-desktops counterpart to @a ccmd_client_transient_family_
 * snapshot just above: every family-wide action that is not itself
 * about desktops (iconify, restore, hide, unhide, pin, unpin) must
 * find every family member regardless of which desktop each one
 * happens to be registered under, not just @p top's own; those two
 * can genuinely differ when @p top is pinned, since pinning a client
 * never actually moves it between desktops (it stays registered
 * under whichever one it was originally on forever; see @a ccmd_
 * client_bring_family's own doc comment below for the fuller
 * reasoning), while an un-pinned transient dialog of it is registered
 * under whichever desktop happened to be current when it was
 * created.  Restricting the search to @p top's own desktop alone, as
 * the desktop-move actions genuinely need to (@a enact_desktop_
 * client_send, @a hi_handle_net_wm_desktop, @a drag_warp_tick, and
 * @a ccmd_client_bring_family itself, which each still use @a ccmd_
 * client_transient_family_snapshot directly for exactly that reason),
 * silently fails to find a transient living elsewhere: hiding or
 * iconifying a pinned parent this way leaves its own dialog neither
 * hidden nor found again on restore, stranding it invisible with no
 * way back.  A thin wrapper over @a s_family_snapshot with its own
 * @p desktop_filter set to @c WM_DESKTOP_ID_ALL, matching every
 * desktop; see that function's own doc comment for the full
 * reasoning behind the two-pass count-then-fill approach.
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
 * @note Complexity: @e O(f), where @e f is the number of @p top's
 *       own transient descendants at every depth combined
 */
client_td **ccmd_client_transient_family_snapshot_anywhere(
        client_td *top, size_t *count_out)
{
    return s_family_snapshot(top, WM_DESKTOP_ID_ALL, count_out);
}


/**
 * @brief Bring every transient descendant of a client onto whichever
 *        desktop is actually being looked at right now, revealing
 *        any iconified or hidden one along the way
 *
 * Openbox's own real answer to a transient family split across
 * desktops or visibility states (confirmed directly against its
 * source, @c client_bring_modal_windows / @c client_bring_windows_
 * recursive in @c client.c): a pinned parent followed to a new
 * desktop leaves its own modal dialog behind, exactly as it started
 * out, but the moment someone tries to focus that parent again, the
 * dialog is moved onto the desktop the parent is being interacted
 * with on right then, not before, so it is right there to actually
 * receive the redirected focus (see @a ccmd_client_focus_target's
 * own doc comment) instead of popping the person back to wherever it
 * happened to be left.  Openbox's own version does not stop at the
 * desktop mismatch either: @c client_bring_windows_recursive checks
 * @e both @c !screen_compare_desktops(self->desktop, desktop) (wrong
 * desktop) @e and @c (iconic && self->iconic) (still iconic), taking
 * whichever action applies: @c client_iconify(self, FALSE, ...) to
 * un-iconify, or @c client_set_desktop(self, desktop, ...) to
 * relocate.  This mirrors both halves: a family member left
 * iconified or hidden (this project's own two separate visibility
 * states, where Openbox has only the one) is revealed via @a ccmd_
 * client_restore or @a ccmd_client_unhide, not just silently left
 * that way forever with no redirect ever able to find it again,
 * since @a ccmd_client_focus_target's own walk (see its doc comment)
 * only ever considers a mapped, non-iconified, non-hidden candidate
 * in the first place.  Relocated first, then revealed, whenever both
 * apply, so revealing it maps it in the right place to begin with
 * rather than on whatever desktop it happened to still be iconified
 * or hidden on.
 *
 * Called from @a ccmd_client_focus itself (@c cmds/client/focus.c),
 * immediately before that same redirect, for exactly this reason.
 *
 * Deliberately the desktop currently being viewed on @p client's own
 * top parent's own surface, not that top parent's own literal "home"
 * desktop (@a wm_get_client_desktop): those two are almost always the
 * same desktop, but not when the top parent is itself pinned, since
 * pinning a client never actually moves it between desktops (it
 * stays registered under whichever one it was originally on forever;
 * see @a surface_clients_hide's own doc comment, surface/actions/
 * clients.c, for how pin visibility is really achieved).
 *
 * The relocation step itself is deliberately only the data move
 * (desktop membership, stacking list, @c desktop_id): unlike an
 * explicit desktop send (@a enact_desktop_client_send, @c enact/
 * desktop.c) or an EWMH one (@a hi_handle_net_wm_desktop, @c
 * handler/ewmhmsg.c), nothing here is visibly dragged across the
 * screen or needs its own unmap/remap dance for that part, since a
 * family member not already mapped on the desktop being looked at
 * was, by definition, not visible there to begin with.
 *
 * @param client Client whose transient family to bring together;
 *               redirected to its own top-most ancestor first, the
 *               same way every other family-wide action in this
 *               project already does
 *
 * @note A null @p client, one whose top parent's own surface cannot
 *       be resolved, or one with no transient family at all is a
 *       silent no-op
 * @note Complexity: @e O(f), where @e f is the number of @p client's
 *       own top parent's transient descendants at every depth
 *       combined
 */
void ccmd_client_bring_family(client_td *client)
{
    client_td *top;
    desktop_td *target;
    surface_td *top_surface;
    size_t count;
    client_td **siblings;

    if (client == NULL) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    /* The desktop actually being looked at right now, not
     * necessarily 'top''s own literal "home" desktop: a pinned
     * client stays registered under whichever desktop it was
     * originally created on forever (pin is achieved purely by
     * exempting it from the hide/show cycle 'surface_clients_hide'/
     * '_show', surface/actions/clients.c, runs on every switch,
     * never by actually moving it between desktops), so using
     * 'wm_get_client_desktop(top)' here would "bring" a transient
     * onto a desktop nobody is even looking at whenever 'top' itself
     * happens to be pinned, leaving that transient mapped (via
     * 'ccmd_client_focus''s own unconditional 'xcb_map_window' on
     * whatever it redirects to) but still homed on its own original
     * desktop: visible on every desktop from then on,
     * indistinguishable from being pinned itself, yet with its own
     * pin indicator never lit, since nothing ever actually pinned
     * it. */
    top_surface = wm_get_surface_by_id(top->screen_id);
    target = (top_surface != NULL)
        ? surface_desktop_get(top_surface, top_surface->desktop_cur)
        : wm_get_client_desktop(top);
    if (target == NULL) {
        return;
    }

    siblings = ccmd_client_transient_family_snapshot_anywhere(top,
            &count);
    if (siblings == NULL) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        /* Compares 'desktop_id' directly first, at no cost beyond a
         * field read on 'siblings[i]' itself: the common case is
         * already on the right desktop (every desktop-move cascade
         * in this project works to keep a family together in the
         * first place), so the one lookup 'wm_get_client_desktop'
         * genuinely costs is worth paying only when a member truly
         * needs relocating. */
        if (siblings[i]->desktop_id != target->id) {
            desktop_td *const home = wm_get_client_desktop(siblings[i]);

            if (home != NULL) {
                (void) desktop_action_client_rem(home, siblings[i]);
                (void) desktop_action_client_add(target, siblings[i]);
                siblings[i]->desktop_id = target->id;
            }
        }

        if (client_is_iconified(siblings[i])) {
            ccmd_client_restore(siblings[i]);
        } else if (client_is_hidden(siblings[i])) {
            ccmd_client_unhide(siblings[i]);
        }
    }

    free(siblings);
}


/**
 * @brief Walk down from a client to whichever mapped transient
 *        descendant should actually receive focus in its place
 *
 * ICCCM §4.1.2.6 dialogs exist to demand a specific answer before
 * their own parent is usable again in any meaningful sense; sending
 * real input focus to the parent instead, while such a dialog of its
 * own still sits mapped and waiting, would let keystrokes land on a
 * window that has nothing useful to do with them and leave the
 * dialog itself reachable only by clicking it directly with the
 * mouse.  Called from @a focus_apply itself (@c policy/focus.c), the
 * one place every real focus-granting path already converges on (see
 * that function's own doc comment), so this applies uniformly
 * regardless of whether the original request came from a click, a
 * restore, a cycle-menu selection, or anywhere else that ends up
 * asking for @p client by name specifically.  Descends through more
 * than one level (a dialog with its own further dialog on top of it)
 * the same way @a ccmd_client_transient_top_parent ascends through
 * more than one, with the same cycle guard.
 *
 * @param client Client focus was actually requested for
 *
 * @return The deepest mapped transient descendant found, or
 *         @p client itself if it has none (or @p client is @c NULL)
 *
 * @note Complexity: @e O(min(d, @c WM_TRANSIENT_CHAIN_MAX_DEPTH) * k),
 *       where @e d is the true depth of mapped transient descendants
 *       and @e k is the number of direct transient children found at
 *       each step along the way (each client's own @c transients
 *       list; see its doc comment, client.h)
 */
client_td *ccmd_client_focus_target(client_td *client)
{
    client_td *target;
    uint32_t depth;

    if (client == NULL) {
        return NULL;
    }

    target = client;
    depth = 0;
    while (depth < WM_TRANSIENT_CHAIN_MAX_DEPTH) {
        client_td *const child = s_client_mapped_transient_child(target);

        if (child == NULL) {
            break;
        }
        target = child;
        depth++;
    }

    return target;
}


/**
 * @brief Link a newly managed client into its parent's transient
 *        tree, if @c transient_for names an already-managed client
 *
 * This is what makes every other function in this file @e O(1) per
 * step instead of a scan of every client on every desktop: rather
 * than re-discovering "who is transient for whom" from scratch on
 * every walk, by comparing @c transient_for window IDs across the
 * whole managed set, the relationship is captured once, right here,
 * as real pointers on both ends (@c transient_parent going up, an
 * entry in @c transients going down) that later code just follows
 * directly.  Openbox's own @c client_update_transient_for /
 * @c client_update_transient_tree (client.c) does the same thing for
 * the same reason, maintaining @c self->transients and @c self->
 * parents as real lists rather than resolving @c WM_TRANSIENT_FOR
 * fresh each time it matters; this is the same idea without
 * Openbox's own additional window-group machinery, which this
 * project has no equivalent of.
 *
 * @c transient_for itself is read once, early, at @a client_init
 * time (@c s_client_read_wm_hints_and_leader, client.c), before
 * @p client is even added to a desktop; resolving the actual parent
 * pointer waits until here, called right after that, because the
 * lookup below needs the whole managed-client machinery, not just
 * this one client's own fields, and because a parent that has not
 * mapped yet (however unusual) simply cannot be linked to yet.
 *
 * New children are linked in at the head of their parent's own
 * @c transients list (matching Openbox's own @c g_slist_prepend for
 * the identical purpose): the only way to know which node an
 * insertion created, given @a cdlist_ins_next never hands the new
 * node back itself, is to insert at a known position and immediately
 * read it straight back out.
 *
 * @param client Newly managed client to link
 *
 * @note A null @p client, one not transient for anything, one
 *       transient for itself, or one whose declared parent is not
 *       (or not yet) managed, is a silent no-op; @p client is simply
 *       never linked into any parent's tree in that last case, the
 *       same as if it were not transient for anything at all
 * @note Complexity: @e O(1)
 */
void client_link_transient(client_td *client)
{
    client_td *parent;

    if (client == NULL || client->transient_for == XCB_WINDOW_NONE) {
        return;
    }

    parent = lookup_find_client(wm_get_surfaces(), client->transient_for,
            NULL, NULL);
    if (parent == NULL || parent == client) {
        return;
    }

    if (parent->transients == NULL) {
        parent->transients = cdlist_init(NULL);
        if (parent->transients == NULL) {
            return;
        }
    }

    if (cdlist_ins_next(parent->transients, NULL, client) == 0) {
        client->transient_parent = parent;
        client->transient_node = cdlist_head(parent->transients);
    }
}


/**
 * @brief Unlink a client from the transient tree before it stops
 *        being managed
 *
 * Removes @p client from its own parent's @c transients list in true
 * @e O(1) (see @c transient_node's own doc comment, client.h: with
 * that node cached, @c cdlist_rem_next on its own @c prev needs no
 * search at all), and orphans every one of @p client's own children
 * by clearing their own @c transient_parent/@c transient_node back
 * to @c NULL: the parent they were transient for is going away, so
 * there is nothing left for them to be transient for anymore, the
 * same way a dialog whose own parent closes simply becomes an
 * ordinary standalone window from that point on rather than staying
 * attached to a client that no longer exists.
 *
 * @param client Client about to stop being managed
 *
 * @note A null @p client is a silent no-op
 * @note Complexity: @e O(k), where @e k is the number of @p client's
 *       own direct transient children
 */
void client_unlink_transient(client_td *client)
{
    void *discarded;

    if (client == NULL) {
        return;
    }

    if (client->transient_parent != NULL &&
            client->transient_node != NULL &&
            client->transient_parent->transients != NULL) {
        (void) cdlist_rem_next(client->transient_parent->transients,
                client->transient_node->prev, &discarded);
    }
    client->transient_parent = NULL;
    client->transient_node = NULL;

    if (client->transients != NULL) {
        while (!cdlist_is_empty(client->transients)) {
            void *removed = NULL;
            client_td *child;

            (void) cdlist_rem_next(client->transients, NULL, &removed);
            child = (client_td *) removed;
            if (child != NULL) {
                child->transient_parent = NULL;
                child->transient_node = NULL;
            }
        }
        cdlist_destroy(client->transients);
        client->transients = NULL;
    }
}
