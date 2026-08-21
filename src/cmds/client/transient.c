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
#include <adt/list.h>
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
 *        transient child of a client, on any desktop of any surface
 *
 * A single step of @a ccmd_client_focus_target's own walk; split out
 * on its own since that walk needs to re-resolve the search fresh at
 * every step (the target client can change desktop as the walk
 * descends, in principle, even though it never will in practice for
 * how this is actually used today).  Deliberately searches every
 * desktop of every surface (the same full traversal @a lookup_find_
 * client itself does, lookup.c), not just @p client's own desktop:
 * a transient family is meant to always move together (see @a ccmd_
 * client_iconify's own doc comment, cmds/client/visibility.c, and
 * every desktop-move site that keeps a family together across a
 * send), but should a family ever end up split across desktops
 * anyway (a bug in one of those sites not yet found, a third-party
 * pager moving only one member of it, and the like), this search
 * still has to actually find the child wherever it now sits; a
 * search scoped to @p client's own desktop alone would silently fail
 * to find it there, falling back to focusing @p client itself, which
 * is exactly the mismatch between real X11 input focus and this
 * window manager's own "active client" bookkeeping that used to
 * leave keyboard shortcuts unable to find anything to act on at all
 * once the dialog later closed.  The @c CLIENT_FLAG_HIDDEN check
 * specifically matters for a client in the middle of closing: @a
 * handler_unmap_notify (@c handler/map.c) marks a withdrawing client
 * hidden before it ever calls @a client_focus_fallback, and a
 * fallback landing back on this child's own parent must not find
 * this same closing child here and redirect focus right back onto
 * it.
 *
 * @param client Client whose direct transient children to search
 *
 * @return The first matching child found, or @c NULL if @p client is
 *         @c NULL or has no such child
 *
 * @note Complexity: @e O(s * d * n), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e n
 *       the number of clients per desktop
 */
static client_td *s_client_mapped_transient_child(client_td *client)
{
    client_td *child = NULL;
    list_td *surfaces;

    if (client == NULL) {
        return NULL;
    }

    surfaces = wm_get_surfaces();
    if (surfaces == NULL) {
        return NULL;
    }

    for (list_item_td *snode = list_head(surfaces);
            snode != NULL && child == NULL; snode = list_next(snode)) {
        surface_td *const surface = (surface_td *) list_data(snode);
        cdlist_item_td *dnode;
        const cdlist_item_td *dinitial;

        if (surface == NULL || surface->desktops == NULL ||
                cdlist_size(surface->desktops) == 0) {
            continue;
        }

        dnode = cdlist_head(surface->desktops);
        dinitial = dnode;
        if (dnode == NULL) {
            continue;
        }

        do {
            desktop_td *const desktop = (desktop_td *) cdlist_data(dnode);

            if (desktop != NULL && desktop->clients != NULL) {
                void *elem;

                ohtbl_foreach(desktop->clients, elem) {
                    client_td *const candidate = (client_td *) elem;

                    if (candidate != NULL && candidate != client &&
                            candidate->transient_for ==
                                client->window &&
                            !client_is_iconified(candidate) &&
                            !client_is_locked(candidate) &&
                            !(candidate->properties.flags &
                                CLIENT_FLAG_HIDDEN)) {
                        child = candidate;
                        break;
                    }
                }
            }
            dnode = cdlist_next(dnode);
        } while (dnode != NULL && dnode != dinitial && child == NULL);
    }

    return child;
}


/**
 * @brief Per-candidate callback used by @a s_visit_family_anywhere
 *
 * @param candidate Family member found; never @c NULL
 * @param ctx       Caller-supplied context, passed through unchanged
 */
typedef void (*s_family_visitor_fn)(client_td *candidate, void *ctx);


/**
 * @brief Walk every desktop of every surface, calling @p visit once
 *        for each other member of @p top's own transient family
 *        found
 *
 * The shared traversal behind both passes @a ccmd_client_transient_
 * family_snapshot_anywhere needs (count, then fill): rather than
 * duplicating the same surface/desktop/client walk once per pass, or
 * growing a single array with 'realloc' desktop by desktop (a
 * genuinely riskier design that turned out to crash outright rather
 * than just misbehave, when this same idea was tried once already),
 * this walks the whole structure exactly the same way every time and
 * simply hands each match to @p visit, which decides what counting
 * or filling means on its own.
 *
 * @param top   Family's own top-most ancestor; @p visit is never
 *              called with @p top itself
 * @param visit Callback invoked once per matching family member
 * @param ctx   Opaque context passed through to every @p visit call
 *
 * @note A null @p top or @p visit is a silent no-op
 * @note Complexity: @e O(s * d * n), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e n
 *       the number of clients per desktop
 */
static void s_visit_family_anywhere(client_td *top,
        s_family_visitor_fn visit, void *ctx)
{
    list_td *surfaces;

    if (top == NULL || visit == NULL) {
        return;
    }

    surfaces = wm_get_surfaces();
    if (surfaces == NULL) {
        return;
    }

    for (list_item_td *snode = list_head(surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *const surface = (surface_td *) list_data(snode);
        cdlist_item_td *dnode;
        const cdlist_item_td *dinitial;

        if (surface == NULL || surface->desktops == NULL ||
                cdlist_size(surface->desktops) == 0) {
            continue;
        }

        dnode = cdlist_head(surface->desktops);
        dinitial = dnode;
        if (dnode == NULL) {
            continue;
        }

        do {
            desktop_td *const desktop = (desktop_td *) cdlist_data(dnode);

            if (desktop != NULL && desktop->clients != NULL) {
                void *elem;

                ohtbl_foreach(desktop->clients, elem) {
                    client_td *const candidate = (client_td *) elem;

                    if (candidate != NULL && candidate != top &&
                            ccmd_client_transient_top_parent(
                                candidate) == top) {
                        visit(candidate, ctx);
                    }
                }
            }
            dnode = cdlist_next(dnode);
        } while (dnode != NULL && dnode != dinitial);
    }
}


/**
 * @brief @a s_family_visitor_fn that only counts matches, for @a
 *        ccmd_client_transient_family_snapshot_anywhere's own first,
 *        sizing pass
 *
 * @param candidate Ignored
 * @param ctx       @c size_t* accumulator to increment
 */
static void s_family_snapshot_count_visitor(client_td *candidate,
        void *ctx)
{
    size_t *const count = (size_t *) ctx;

    (void) candidate;
    (*count)++;
}


/**
 * @brief Context for @a s_family_snapshot_fill_visitor
 */
struct s_family_snapshot_fill_ctx {
    client_td **members;    /**< Destination array, pre-sized */
    size_t capacity;        /**< Number of slots @c members has */
    size_t count;           /**< Number of slots filled so far */
};


/**
 * @brief @a s_family_visitor_fn that fills a pre-sized array, for @a
 *        ccmd_client_transient_family_snapshot_anywhere's own
 *        second, filling pass
 *
 * Never writes past @c ctx->capacity even if somehow handed more
 * matches than the first, sizing pass counted (nothing mutates the
 * client tree between the two passes in practice, so the two counts
 * always agree, but a stray write past the end of a heap allocation
 * is exactly the class of bug worth guarding against unconditionally
 * rather than trusting that invariant alone).
 *
 * @param candidate Family member to store
 * @param ctx       @c struct @a s_family_snapshot_fill_ctx*
 */
static void s_family_snapshot_fill_visitor(client_td *candidate,
        void *ctx)
{
    struct s_family_snapshot_fill_ctx *const fill =
        (struct s_family_snapshot_fill_ctx *) ctx;

    if (fill->count < fill->capacity) {
        fill->members[fill->count] = candidate;
        fill->count++;
    }
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
    while (top->transient_for != XCB_WINDOW_NONE &&
            depth < WM_TRANSIENT_CHAIN_MAX_DEPTH) {
        client_td *const parent = lookup_find_client(wm_get_surfaces(),
                top->transient_for, NULL, NULL);

        /* Stop at a declared parent that is not (or not yet) a managed
         * client, and at a self-referencing 'transient_for' (a
         * malformed or malicious client naming itself), rather than
         * looping forever on either. */
        if (parent == NULL || parent == top) {
            break;
        }
        top = parent;
        depth++;
    }

    return top;
}


/**
 * @brief Collect every other member of a transient family found on
 *        one specific desktop into a newly allocated snapshot array
 *
 * Every family-wide action in this project (iconify, restore, pin,
 * unpin, desktop sends, and the like) needs the exact same two steps
 * before it can safely touch more than one client at once: collect
 * every matching sibling into an array first, rather than acting on
 * each one directly from inside an @c ohtbl_foreach pass, since an
 * action on one sibling (an iconify, a pin, a desktop move) can
 * itself add, remove, or otherwise touch entries in @p desktop's own
 * client table, and iterating and mutating that same table at once
 * is undefined behavior for @c ohtbl_foreach; and size the array
 * correctly up front from @p desktop's own client count.  This
 * function is exactly those two steps, factored out once instead of
 * repeated at every call site; the caller supplies its own loop over
 * the result to actually act on each one, since what to do with a
 * family member is the one part every call site still needs its own
 * way.
 *
 * @param desktop   Desktop to scan
 * @param top       Family's own top-most ancestor (see @a ccmd_
 *                  client_transient_top_parent); excluded from the
 *                  result even if found on @p desktop itself
 * @param count_out Receives the number of clients collected; set to
 *                  @c 0 on any early return, including allocation
 *                  failure
 *
 * @return Newly allocated array of @c *count_out client pointers,
 *         the caller's own to @c free; @c NULL if @p desktop, @p top,
 *         or @p count_out is @c NULL, @p desktop has no clients at
 *         all, or the allocation itself failed
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
client_td **ccmd_client_transient_family_snapshot(desktop_td *desktop,
        client_td *top, size_t *count_out)
{
    client_td **members;
    size_t capacity;
    void *elem;

    if (count_out != NULL) {
        *count_out = 0;
    }

    if (desktop == NULL || top == NULL || desktop->clients == NULL ||
            count_out == NULL) {
        return NULL;
    }

    capacity = ohtbl_size(desktop->clients);
    members = malloc(capacity * sizeof(*members));
    if (members == NULL) {
        return NULL;
    }

    ohtbl_foreach(desktop->clients, elem) {
        client_td *const candidate = (client_td *) elem;

        if (candidate != NULL && candidate != top &&
                ccmd_client_transient_top_parent(candidate) == top) {
            members[*count_out] = candidate;
            (*count_out)++;
        }
    }

    return members;
}


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
 * each one happens to be registered under, not just @p top's own —
 * those two can genuinely differ when @p top is pinned, since
 * pinning a client never actually moves it between desktops (it
 * stays registered under whichever one it was originally on
 * forever; see @a ccmd_client_bring_family's own doc comment below
 * for the fuller reasoning), while an un-pinned transient dialog of
 * it is registered under whichever desktop happened to be current
 * when it was created.  Scoping the search to @p top's own desktop
 * alone, as the desktop-move actions genuinely need to (@a enact_
 * desktop_client_send, @a hi_handle_net_wm_desktop, @a drag_warp_
 * tick, and @a ccmd_client_bring_family itself, which each still use
 * @a ccmd_client_transient_family_snapshot directly for exactly that
 * reason), silently fails to find a transient living elsewhere:
 * hiding or iconifying a pinned parent this way leaves its own
 * dialog neither hidden nor found again on restore, stranding it
 * invisible with no way back.
 *
 * Counts every match first via @a s_visit_family_anywhere, allocates
 * exactly that many slots once, then fills them in a second,
 * identical pass, rather than growing one array as matches are found
 * (a genuinely riskier design that turned out to crash outright
 * rather than just misbehave, when this same idea was tried once
 * already): nothing mutates the client tree between the two passes,
 * so both always find the exact same matches in the exact same
 * order, and @a s_family_snapshot_fill_visitor never writes past the
 * array's own true size regardless.
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
 * @note Complexity: @e O(s * d * n), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e n
 *       the number of clients per desktop
 */
client_td **ccmd_client_transient_family_snapshot_anywhere(
        client_td *top, size_t *count_out)
{
    size_t total;
    client_td **members;
    struct s_family_snapshot_fill_ctx fill;

    if (count_out != NULL) {
        *count_out = 0;
    }

    if (top == NULL || count_out == NULL) {
        return NULL;
    }

    total = 0;
    s_visit_family_anywhere(top, s_family_snapshot_count_visitor,
            &total);
    if (total == 0) {
        return NULL;
    }

    members = malloc(total * sizeof(*members));
    if (members == NULL) {
        return NULL;
    }

    fill.members = members;
    fill.capacity = total;
    fill.count = 0;
    s_visit_family_anywhere(top, s_family_snapshot_fill_visitor, &fill);

    *count_out = fill.count;
    return members;
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
 * whichever action applies — @c client_iconify(self, FALSE, ...) to
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
 * @note Complexity: @e O(s * d * n), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e n
 *       the number of clients per desktop
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
        desktop_td *const home = wm_get_client_desktop(siblings[i]);

        if (home != NULL && home != target) {
            (void) desktop_action_client_rem(home, siblings[i]);
            (void) desktop_action_client_add(target, siblings[i]);
            siblings[i]->desktop_id = target->id;
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
 * @note Complexity: @e O(min(d, @c WM_TRANSIENT_CHAIN_MAX_DEPTH) *
 *       s * d2 * n), where @e d is the true depth of mapped transient
 *       descendants, @e s is the number of surfaces, @e d2 the
 *       number of desktops per surface, and @e n the number of
 *       clients per desktop
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
