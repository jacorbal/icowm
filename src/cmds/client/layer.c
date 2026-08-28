/**
 * @file cmds/client/layer.c
 *
 * @brief Client stacking-order command implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <policy/stacking.h>
#include <logger.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/internal.h>
#include <cmds/client/layer.h>
#include <cmds/client/screen.h>
#include <cmds/client/transient.h>


/**
 * @brief Enforce layer stacking and request a redraw after a client's
 *        layer changes
 *
 * Shared by @c ccmd_client_layer_above, @c ccmd_client_layer_normal,
 * and @c ccmd_client_layer_below below, which only differ in the new
 * @c client->properties.layer value and which @c _NET_WM_STATE atoms
 * to add or remove for it.
 *
 * @param client  Client whose layer just changed
 * @param desktop Desktop @p client is on, or @c NULL to skip
 *                re-enforcing layer stacking
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop (see @c ccmd_desktop_enforce_layers)
 */
static void s_client_layer_finish(client_td *client, desktop_td *desktop)
{
    if (desktop != NULL) {
        ccmd_desktop_enforce_layers(desktop);
    }

    wm_request_client_redraw(client);
    xcb_flush(client->connection);
}


/**
 * @brief Whether a client should be treated as its own top-level
 *        entry point for @a s_enforce_layer_place_family, rather
 *        than placed as part of some ancestor's own family cluster
 *
 * @c false exactly when @p client has a @c transient_parent (or, for
 * one transient for its whole group, ICCCM §4.1.2.6, a resolved
 * @a client_group_transient_anchor, @c cmds/client/transient.c) that
 * is itself on the same desktop and in the same layer as @p client:
 * that parent's own call to @a s_enforce_layer_place_family already
 * recurses into @p client (see that function's comment), so
 * treating it as a second, independent entry point here would place
 * it twice, the second time breaking the family clustering the first
 * placement already established.  @c true whenever the parent is
 * missing, on a different desktop, or in a different layer: none of
 * those get a recursive visit from that parent's own placement, so
 * @p client only ever gets placed at all by being its own entry
 * point.
 *
 * @param client Client to classify
 * @param desktop Desktop this layer pass is currently placing
 * @param layer   Layer this pass is currently placing
 *
 * @note Complexity: @e O(1) for a specific-parent transient,
 *       @e O(n) for one transient for its whole group (see
 *       @a client_group_transient_anchor's complexity note)
 */
static bool s_enforce_layer_is_top_level(const client_td *client,
        const desktop_td *desktop, uint16_t layer)
{
    const client_td *parent = client->transient_parent;

    if (parent == NULL && client->is_transient_for_group) {
        parent = client_group_transient_anchor(client);
    }

    return parent == NULL || parent->desktop_id != desktop->id ||
        parent->properties.layer != layer;
}


/**
 * @brief Place one client, then recursively place its own transient
 *        descendants immediately above it, within one layer pass of
 *        @a ccmd_desktop_enforce_layers
 *
 * Openbox's own real answer to keeping a transient family together
 * during restacking (confirmed directly against its source,
 * @c restack_windows in @c stacking.c): a dialog belongs directly
 * above the window it is transient for, not wherever it happens to
 * fall in whatever order the rest of the desktop's own clients are
 * otherwise sorted in.  Recurses depth-first through @p top's own
 * @c transients (see its comment, client.h), each child
 * placed immediately after its own parent and before the parent's
 * next sibling, so a whole family clusters together as one
 * contiguous block within its shared layer; a child in a different
 * layer than @p top is left for that other layer's own pass instead
 * (see @a s_enforce_layer_is_top_level), matching Openbox's own
 * identical @c ch->layer @c == @c selected->layer condition.
 *
 * @param top          Client to place, then recurse from
 * @param desktop      Desktop this layer pass is placing; only a
 *                      descendant registered under this same desktop
 *                      is placed by this same call, matching a
 *                      pinned client staying registered under
 *                      whichever desktop it was originally on
 *                      forever rather than actually moving between
 *                      desktops (see @a ccmd_client_bring_family's
 *                      comment, cmds/client/transient.c, for
 *                      the fuller reasoning)
 * @param layer        Layer this pass is placing; only a descendant
 *                      sharing this exact layer with @p top is
 *                      placed by this same call
 * @param prev_target   The previous client's own target window
 *                      placed so far across the whole layer pass, or
 *                      @c XCB_WINDOW_NONE for the very first;
 *                      updated in place as each client here is
 *                      placed, so the caller's own next sibling
 *                      stacks above whatever this call last placed
 * @param depth        Current recursion depth; the caller's own
 *                      first call always passes @c 0
 *
 * @note A null @p top, or exceeding @c WM_TRANSIENT_CHAIN_MAX_DEPTH,
 *       is a silent no-op (for the depth guard, orphaning whatever
 *       part of a pathologically deep or cyclical chain remains
 *       beyond it, rather than looping forever)
 * @note Complexity: @e O(f), where @e f is the number of @p top's
 *       own transient descendants, at every depth combined, sharing
 *       both @p desktop and @p layer with it
 */
static void s_enforce_layer_place_family(client_td *top,
        desktop_td *desktop, uint16_t layer,
        xcb_window_t *prev_target, uint32_t depth)
{
    xcb_window_t target;
    cdlist_item_td *cnode;

    if (top == NULL || depth >= WM_TRANSIENT_CHAIN_MAX_DEPTH) {
        return;
    }

    target = ccmd_target_win(top);
    if (target != XCB_WINDOW_NONE) {
        if (*prev_target == XCB_WINDOW_NONE) {
            /* The first client placed across the whole layer pass:
             * anchor it explicitly just above the tray (if any)
             * rather than an unqualified 'below' with no sibling; see
             * 'ccmd_desktop_enforce_layers''s comment on this
             * exact reasoning. */
            xcb_window_t tray_below = systray_below_window();

            if (tray_below != XCB_WINDOW_NONE) {
                xcb_configure_window(top->connection, target,
                        XCB_CONFIG_WINDOW_SIBLING |
                        XCB_CONFIG_WINDOW_STACK_MODE,
                        (const uint32_t[]) {
                        tray_below, XCB_STACK_MODE_ABOVE
                        });
            } else {
                xcb_configure_window(top->connection, target,
                        XCB_CONFIG_WINDOW_STACK_MODE,
                        (const uint32_t[]) {
                        XCB_STACK_MODE_BELOW
                        });
            }
        } else {
            xcb_configure_window(top->connection, target,
                    XCB_CONFIG_WINDOW_SIBLING |
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const uint32_t[]) {
                    *prev_target, XCB_STACK_MODE_ABOVE
                    });
        }
        *prev_target = target;
    }

    if (top->transients != NULL) {
        const cdlist_item_td *cinitial;

        cnode = cdlist_head(top->transients);
        cinitial = cnode;
        if (cnode != NULL) {
            do {
                client_td *const child = (client_td *) cdlist_data(cnode);

                if (child != NULL && child->desktop_id == desktop->id &&
                        child->properties.layer == layer) {
                    s_enforce_layer_place_family(child, desktop, layer,
                            prev_target, depth + 1);
                }
                cnode = cdlist_next(cnode);
            } while (cnode != NULL && cnode != cinitial);
        }
    }

    /* A client transient for its whole group (ICCCM §4.1.2.6) has no
     * 'transient_parent' to appear in 'top->transients' above; found
     * here instead by checking whether 'top' is currently the one
     * @a client_group_transient_anchor (@c cmds/client/transient.c)
     * resolves it to, the same check @a s_enforce_layer_is_top_level
     * already made to keep it from also being placed as a second,
     * separate entry point earlier in this same pass. */
    if (top->desktop_id == desktop->id && desktop->clients != NULL) {
        void *elem;

        ohtbl_foreach(desktop->clients, elem) {
            client_td *const gchild = (client_td *) elem;

            if (gchild != NULL && gchild != top &&
                    gchild->is_transient_for_group &&
                    gchild->desktop_id == desktop->id &&
                    gchild->properties.layer == layer &&
                    client_group_transient_anchor(gchild) == top) {
                s_enforce_layer_place_family(gchild, desktop, layer,
                        prev_target, depth + 1);
            }
        }
    }
}


/**
 * @brief What @a s_enforce_layer_visit carries across one layer's pass
 */
struct s_enforce_layer_ctx_s {
    desktop_td *desktop;        /**< Desktop being restacked */
    uint16_t layer;             /**< Layer this pass is placing */
    xcb_window_t *prev_target;  /**< Window placed just below */
};


/**
 * @brief Place one client, if it belongs to the layer being done
 *
 * @param client Client reached by the walk
 * @param data   Pointer to this walk's own layer context
 *
 * @note Complexity: @e O(f), where @e f is the size of the client's
 *       own transient family
 */
static void s_enforce_layer_visit(client_td *client, void *data)
{
    struct s_enforce_layer_ctx_s *const enforce_ctx = data;

    if (client == NULL || enforce_ctx == NULL ||
            client->properties.layer != enforce_ctx->layer ||
            !s_enforce_layer_is_top_level(client, enforce_ctx->desktop,
                enforce_ctx->layer)) {
        return;
    }

    s_enforce_layer_place_family(client, enforce_ctx->desktop,
            enforce_ctx->layer, enforce_ctx->prev_target, 0);
}


/* Raise the client to the top of the stacking order */
void ccmd_client_raise(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Raising client window=0x%x", client->window);

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        (void) desktop_action_client_send_front(desktop, client);
        ccmd_desktop_enforce_layers(desktop);
    } else {
        uint32_t values[] = { XCB_STACK_MODE_ABOVE };
        xcb_window_t target = ccmd_target_win(client);
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_STACK_MODE, values);
        xcb_flush(client->connection);
    }

}


/* Lower the client to the bottom of the stacking order */
void ccmd_client_lower(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL) {
        return;
    }

    LOGGER_TRACE("Lowering client window=0x%x", client->window);

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        (void) desktop_action_client_send_back(desktop, client);
        ccmd_desktop_enforce_layers(desktop);
    } else {
        uint32_t values[] = { XCB_STACK_MODE_BELOW };
        xcb_window_t target = ccmd_target_win(client);
        xcb_configure_window(client->connection, target,
                XCB_CONFIG_WINDOW_STACK_MODE, values);
        xcb_flush(client->connection);
    }
}


/* Place the client in the above layer */
void ccmd_client_layer_above(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL || client_is_locked(client)) {
        return;
    }

    LOGGER_TRACE("Setting client window=0x%x to layer 'above'",
            client->window);
    client->properties.layer = CLIENT_LAYER_ABOVE;

    ccmd_client_sync_states(client);

    desktop = wm_get_client_desktop(client);
    s_client_layer_finish(client, desktop);
}


/* Place the client in the normal (default) layer */
void ccmd_client_layer_normal(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL || client_is_locked(client)) {
        return;
    }

    LOGGER_TRACE("Setting client window=0x%x to layer 'normal'",
            client->window);
    client->properties.layer = CLIENT_LAYER_NORMAL;

    ccmd_client_sync_states(client);

    desktop = wm_get_client_desktop(client);
    s_client_layer_finish(client, desktop);
}


/* Place the client in the below layer */
void ccmd_client_layer_below(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL || client_is_locked(client)) {
        return;
    }

    LOGGER_TRACE("Setting client window=0x%x to layer 'below'",
            client->window);
    client->properties.layer = CLIENT_LAYER_BELOW;

    ccmd_client_sync_states(client);

    desktop = wm_get_client_desktop(client);
    s_client_layer_finish(client, desktop);
}


/* Cycle layer: 'normal -> above -> below -> normal -> above -> ...' */
void ccmd_client_cycle_layer(client_td *client)
{
    if (client == NULL) {
        return;
    }

    if (client->properties.layer == CLIENT_LAYER_NORMAL) {
        ccmd_client_layer_above(client);
    } else if (client->properties.layer == CLIENT_LAYER_ABOVE) {
        ccmd_client_layer_below(client);
    } else {
        ccmd_client_layer_normal(client);
    }
}


/* Enforce layer stacking order for all clients in a desktop */
void ccmd_desktop_enforce_layers(desktop_td *desktop)
{
    enum client_layer_e layer_order[] = {
        CLIENT_LAYER_BELOW,
        CLIENT_LAYER_NORMAL,
        CLIENT_LAYER_ABOVE
    };
    xcb_window_t prev_target;
    struct s_enforce_layer_ctx_s enforce_ctx;

    if (desktop == NULL || stacking_count(desktop) == 0u) {
        return;
    }

    prev_target = XCB_WINDOW_NONE;
    enforce_ctx.desktop = desktop;
    enforce_ctx.prev_target = &prev_target;

    for (size_t li = 0;
            li < sizeof(layer_order) / sizeof(layer_order[0]);
            ++li) {
        enforce_ctx.layer = (uint16_t) layer_order[li];
        stacking_walk(desktop, s_enforce_layer_visit, &enforce_ctx);
    }

    /* A fullscreen client that currently holds focus always sits
     * above every other client on this desktop, including every
     * other ABOVE-layer one, the same way a fullscreen application
     * covers a taskbar or panel in most desktop environments.
     * Deliberately a stacking-order effect only, applied here fresh
     * on every call rather than by ever writing 'properties.layer'
     * itself: the client's own real layer stays exactly what it was
     * chosen to be the whole time, so losing focus to something else
     * needs no separate "restore" step of its own, just correctly
     * falling out of this check and settling back into that real
     * layer group via the loop above, on whatever future call to
     * this same function focus next moves away on. */
    if (desktop->client_active_id != 0u) {
        client_td *active = desktop_find_client_by_id(desktop,
                desktop->client_active_id);

        if (active != NULL && client_is_fullscreen(active)) {
            xcb_window_t active_target = ccmd_target_win(active);

            if (active_target != XCB_WINDOW_NONE) {
                xcb_configure_window(active->connection, active_target,
                        XCB_CONFIG_WINDOW_STACK_MODE,
                        (const uint32_t[]) { XCB_STACK_MODE_ABOVE });
            }
        }
    }

    if (desktop->connection != NULL) {
        xcb_flush(desktop->connection);
    }
}
