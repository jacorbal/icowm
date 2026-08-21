/**
 * @file cmds/client/transient.c
 *
 * @brief Transient-family resolution shared by @c ccmd_client_iconify,
 *        @c ccmd_client_restore, and @c ccmd_client_focus
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
