/**
 * @file cmds/client/transient.c
 *
 * @brief Transient-family resolution and snapshot collection, shared
 *        by every family-wide action across the project.  Iconify,
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
#include <stage.h>
#include <stage/desktop.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/focus.h>
#include <cmds/client/transient.h>
#include <cmds/client/visibility.h>


/**
 * @brief Context shared by @a s_family_snapshot_visitor's counting and
 *        filling passes, both used by @a s_family_snapshot
 *
 * @c members is @c NULL during the first, counting pass (nothing to
 * write yet, @c count just accumulates a total) and points at
 * a freshly, exactly sized allocation during the second, filling pass.
 */
struct s_family_snapshot_ctx {
    client_td **members;     /**< @c NULL during the counting pass */
    size_t capacity;         /**< Slots @c members has (filling pass) */
    size_t count;            /**< Matches found so far */
    uint32_t desktop_filter; /**< @c WM_DESKTOP_ID_ALL matches every
                                  desktop */
};


/**
 * @brief Find the mapped, non-iconified, unlocked, un-hidden direct
 *        transient child of a client, if it has one
 *
 * A single step of @a ccmd_client_focus_target's walk; split out on its
 * own to keep that walk's loop body simple.  Walks @p client's
 * @c transients list directly (see its comment, client.h): no lookup,
 * no scan of any other client on any desktop, just the handful of
 * pointers @p client's direct children actually are.
 *
 * The @c CLIENT_FLAG_HIDDEN check specifically matters for a client in
 * the middle of closing: @a handler_window_unmap_notify in @c handler/map.c
 * marks a withdrawing client hidden before it ever calls
 * @a client_focus_fallback, and a fallback landing back on this child's
 * parent must not find this same closing child here and redirect focus
 * right back onto it.
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
    desktop_td *desktop;
    xcb_window_t leader;
    void *elem;

    if (client == NULL) {
        return NULL;
    }

    if (client->transients != NULL) {
        const cdlist_item_td *initial;

        item = cdlist_head(client->transients);
        initial = item;
        if (item != NULL) {
            do {
                client_td *const candidate =
                    (client_td *) cdlist_data(item);

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
     * whole group (ICCCM §4.1.2.6) has no 'transient_parent' to appear
     * in the list just searched above, so it is looked for here
     * separately instead, by group membership rather than a resolved
     * anchor (unlike 's_enforce_layer_place_family' and
     * 's_desktop_transients_raise', client.c and desktop/dclient.c,
     * this has no "only ever trigger once" constraint to protect.  Any
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
 * @brief Recursively walk every transient descendant of a node
 *        (children, grandchildren, and so on), calling @p visit once
 *        for each
 *
 * The shared traversal behind every family-wide snapshot in this file:
 * rather than scanning every client on every desktop and comparing
 * @c transient_for window IDs (the only way this used to be possible,
 * before @c transients existed as a real, maintained list; see its
 * comment, @c client.h), this walks only @p node's actual descendants,
 * following real pointers, so its cost is proportional to the family's
 * size rather than to how many other, unrelated clients happen to be
 * managed.
 *
 * @param node Client whose descendants to walk; @p visit is never
 *              called with @p node itself
 * @param visit Callback invoked once per descendant found
 * @param ctx   Opaque context passed through to every @p visit call
 * @param depth Current recursion depth; callers of this function itself
 *              always pass @c 0
 *
 * @note A null @p node or @p visit, or one with no @c transients at
 *       all, is a silent no-op
 * @note Complexity: @e O(f), where @e f is the number of @p node's own
 *       transient descendants at every depth combined
 */
static void s_visit_descendants(client_td *node,
        ccmd_family_fn visit, void *ctx, uint32_t depth)
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
 * @brief @a ccmd_family_fn shared by both
 *        @a ccmd_client_transient_family_snapshot and its all-desktops
 *        counterpart, via @a s_family_snapshot
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

    /* Never writes past 'capacity' even if somehow handed more matches
     * than the first, counting pass counted (nothing mutates the
     * transient tree between the two passes in practice, so both always
     * find the exact same matches, but a stray write past the end of
     * a heap allocation is exactly the class of bug worth guarding
     * against unconditionally rather than trusting that invariant
     * alone). */
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
 * Shared implementation behind both
 * @a ccmd_client_transient_family_snapshot, whose @p desktop_filter
 * names a specific desktop's @c id, and
 * @a ccmd_client_transient_family_snapshot_anywhere, whose
 * @p desktop_filter is @c WM_DESKTOP_ID_ALL and matches every desktop:
 * counts every match first via @a s_visit_descendants, allocates
 * exactly that many slots once, then fills them in an identical second
 * pass, rather than growing one array as matches are found (a genuinely
 * riskier design that turned out to crash outright rather than just
 * misbehave, when this same idea was tried once already, back when this
 * whole file still had to scan every desktop instead of walking a real
 * tree).
 *
 * @param top Family's top-most ancestor; excluded from the
 *                       result even where found
 * @param desktop_filter A specific desktop's @c id to restrict the
 *                       result to, or @c WM_DESKTOP_ID_ALL to match
 *                       every desktop
 * @param count_out Receives the number of clients collected; set
 *                       to @c 0 on any early return
 *
 * @return Newly allocated array of @c *count_out client pointers, the
 *         caller's to @c free; @c NULL if @p top or @p count_out is
 *         @c NULL, no match was found, or the allocation itself failed
 *
 * @note Complexity: @e O(f), where @e f is the number of @p top's own
 *       transient descendants at every depth combined
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

        /* Excludes another client also transient for the group.  An
         * anchor is meant to be an actual application window of the
         * group, never another such dialog.  Without this, two
         * group-transient dialogs sharing the same group could resolve
         * to each other (whichever 'ohtbl_foreach' happens to visit
         * first), and every caller walking from an anchor back into the
         * family ('s_enforce_layer_place_family',
         * 'cmds/client/layer.c'; 's_desktop_transients_raise',
         * 'desktop/dclient.c') would recurse into that same pair of
         * dialogs endlessly. */
        if (sibling != NULL && sibling != client &&
                !sibling->is_transient_for_group &&
                client_group_leader(sibling) == leader &&
                !client_is_iconified(sibling)) {
            return sibling;
        }
    }

    return NULL;
}


/* Collect every other member of a transient family found on one
 * specific desktop into a newly allocated snapshot array */
client_td
    **ccmd_client_transient_family_snapshot(const desktop_td *desktop,
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


/* Collect every other member of a transient family, found on any
 * desktop of any stage, into a newly allocated snapshot array */
client_td **ccmd_client_transient_family_snapshot_anywhere(
        client_td *top, size_t *count_out)
{
    return s_family_snapshot(top, WM_DESKTOP_ID_ALL, count_out);
}


/* Apply an action to every transient descendant of a client */
void ccmd_client_family_apply(client_td *top, ccmd_family_fn fn,
        void *ctx)
{
    s_visit_descendants(top, fn, ctx, 0);
}


/* Bring every transient descendant of a client onto whichever desktop
 * is actually being looked at right now, revealing any iconified or
 * hidden one along the way */
void ccmd_client_bring_family(client_td *client)
{
    client_td *top;
    desktop_td *target;
    stage_td *top_stage;
    size_t count;
    client_td **siblings;

    if (client == NULL) {
        return;
    }

    top = ccmd_client_transient_top_parent(client);
    if (top == NULL) {
        return;
    }

    /* The desktop actually being looked at right now, not necessarily
     * 'top''s literal "home" desktop.  A pinned client stays registered
     * under whichever desktop it was originally created on forever (pin
     * is achieved purely by exempting it from the hide/show cycle
     * 'stage_client_hide_all'/ '_show', 'stage/actions/client.c',
     * runs on every switch, never by actually moving it between
     * desktops), so using 'wm_get_client_desktop(top)' here would
     * "bring" a transient onto a desktop nobody is even looking at
     * whenever 'top' itself happens to be pinned, leaving that
     * transient mapped (via 'ccmd_client_focus''s unconditional
     * 'xcb_map_window' on whatever it redirects to) but still homed on
     * its own original desktop: visible on every desktop from then on,
     * indistinguishable from being pinned itself, yet with its pin
     * indicator never lit, since nothing ever actually pinned it. */
    top_stage = wm_get_stage_by_id(top->screen_id);
    target = (top_stage != NULL)
        ? stage_desktop_get(top_stage, top_stage->desktop_cur)
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
        /* Compares 'desktop_id' directly first, at no cost beyond
         * a field read on 'siblings[i]' itself: the common case is
         * already on the right desktop (every desktop-move cascade in
         * this project works to keep a family together in the first
         * place), so the one lookup 'wm_get_client_desktop' genuinely
         * costs is worth paying only when a member truly needs
         * relocating. */
        if (siblings[i]->desktop_id != target->id) {
            desktop_td *const home = wm_get_client_desktop(siblings[i]);

            if (home != NULL) {
                (void) desktop_action_client_move(home, target,
                        siblings[i]);
            }
        }

        /* A transient sibling is never revealed here: a dialog stays
         * out of automatic discovery the same way it stays out of the
         * taskbar and the cycle menu (see 'ccmd_client_iconify''s own
         * comment, cmds/client/visibility.c), which is exactly what
         * turned this loop into a client's own GIMP color-picker
         * dialog being unhidden on every single click anywhere in the
         * application, each time immediately closed again by GIMP's
         * own code the moment it noticed. */
        if (client_is_transient(siblings[i])) {
            continue;
        }

        if (client_is_iconified(siblings[i])) {
            ccmd_client_restore(siblings[i]);
        } else if (client_is_hidden(siblings[i])) {
            ccmd_client_unhide(siblings[i]);
        }
    }

    free(siblings);
}


/* Walk down from a client to whichever mapped transient descendant
 * should actually receive focus in its place */
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
        client_td *const child =
            s_client_mapped_transient_child(target);

        if (child == NULL) {
            break;
        }
        target = child;
        depth++;
    }

    return target;
}


/* Link a newly managed client into its parent's transient tree, if
 * 'transient_for' names an already-managed client */
void client_link_transient(client_td *client)
{
    client_td *parent;

    if (client == NULL || client->transient_for == XCB_WINDOW_NONE) {
        return;
    }

    parent = lookup_find_client(wm_get_stages(),
            client->transient_for, NULL, NULL);
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


/* Unlink a client from the transient tree before it stops being
 * managed */
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
