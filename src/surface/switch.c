/**
 * @file surface/switch.c
 *
 * @brief Surface desktop management to add and remove desktops
 *
 * Implements the surface-level operations that create or destroy
 * desktops, including the fullscreen-surface toggle.  Switching the
 * current desktop itself lives in @c cmds/surface.c, alongside the
 * pinned-client transfer a real desktop switch also needs; client
 * visibility management and RandR operations live in
 * @c surface/actions.c.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/xcb/connection.h>

/* Policy includes */
#include <policy/stacking.h>

/* Command includes */
#include <cmds/client/ewmh.h>
#include <cmds/client/flags.h>
#include <cmds/client/icon.h>
#include <cmds/client/maximize.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <memguard.h>

/* Local includes */
#include <surface.h>


/**
 * @brief Mark every one of a surface's desktops, and every client on
 *        each of them, as outdated
 *
 * @c render_client_titlebar_repaint_content
 * (@c render/client/titlebar.c) recomputes whether the pin button
 * belongs on a client's titlebar (@c hide_pin) fresh every time it
 * runs, from @p surface's current @c desktop_count, but reaching it
 * takes clearing two separate gates, not one: @a desktop_render_full
 * only actually renders a desktop whose @c is_outdated is set
 * (@a surface_render_current_desktop, @c render/surface.c), and, once
 * inside, @a desktop_render_one_client only repaints a client's
 * titlebar content when that client's @c is_outdated is
 * @e also set (@c render/desktop.c).
 *
 * @a surface_action_desktop_add / @a _remove used to mark only
 * @p surface itself, leaving every existing client's pin button stale
 * (present or missing) until some unrelated event (a focus change, in
 * practice, which marks both the one desktop involved and its one
 * client) happened to clear both gates for it on its own.
 *
 * @param surface Surface whose desktops and clients should all be
 *                marked outdated
 *
 * @note No-op if @p surface or its desktop list is @c NULL
 * @note Complexity: @e O(n), where @e n is the total number of clients
 *       across every one of @p surface's desktops
 */
static void s_surface_mark_all_desktops_outdated(surface_td *surface)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (surface == NULL || surface->desktops == NULL) {
        return;
    }

    node = cdlist_head(surface->desktops);
    if (node == NULL) {
        return;
    }

    initial = node;
    do {
        desktop_mark_outdated((desktop_td *) cdlist_data(node));
        node = cdlist_next(node);
    } while (node != NULL && node != initial);
}


/**
 * @brief Grow this surface's configured desktop-grid layout by exactly
 *        one row or column, whichever @c orientation treats as the
 *        non-primary axis, so it can hold one more desktop than its
 *        @c rows @c * @c columns currently can
 *
 * Growing the non-primary axis, never the primary one @c orientation
 * itself fills first (@c columns for @c horizontal, @c rows for
 * @c vertical), is what keeps every desktop already placed in the grid
 * exactly where it already was: that primary axis is the divisor
 * @a s_layout_row_col (@c surface/desktops.c) itself uses to translate
 * a flat index into its row/column, so changing it reflows every index
 * past the first row (or column) into a whole new position, while
 * growing the other axis instead only ever opens up an entirely new,
 * previously nonexistent row (or column) beyond the last one, leaving
 * every existing index's division and remainder, and so its translated
 * position, completely unaffected.
 *
 * A no-op when there is already enough spare capacity
 * (@c rows @c * @c columns already exceeds the desktop count about to
 * exist) to just fill a desktop-less gap cell instead, the common case
 * once a screen has been through more than one add and remove cycle.
 * Never switches the surface's currently viewed desktop, whether it
 * grows anything or not, and whichever row or column that view happens
 * to already be on.  Adding a desktop is purely a "create it" action
 * here, the exact same as it was before a grid layout existed at all,
 * regardless of which one, if any, the user doing the adding happens to
 * be looking at right now.
 *
 * @param surface     Surface whose layout to grow
 * @param new_count   The desktop count this surface is about to have
 *                    once the desktop currently being added actually
 *                    exists
 *
 * @note Complexity: @e O(1)
 */
static void s_surface_layout_grow_for(surface_td *surface,
        uint32_t new_count)
{
    struct config_desktop_layout_s *layout;

    if (surface == NULL || surface->config == NULL ||
            surface->id >= (uint32_t) CONFIG_MAX_SCREENS) {
        return;
    }

    layout = &surface->config->base.screens[surface->id].desktop_layout;
    if (layout->rows * layout->columns >= new_count) {
        /* Already enough room; the new desktop simply fills an
         * existing gap cell */
        return;
    }

    if (layout->orientation == CONFIG_DESKTOP_ORIENTATION_HORIZONTAL) {
        layout->rows++;
    } else {
        layout->columns++;
    }
}


/**
 * @brief Shrink this surface's configured desktop-grid layout by
 *        exactly one row or column, whichever @c orientation treats as
 *        the non-primary axis, if the desktop just removed was that
 *        axis' last remaining member
 *
 * The exact inverse of @a s_surface_layout_grow_for.  Since @c remove
 * only ever takes the highest-numbered desktop, and fill order always
 * places that one in the last row (or column) that has any member at
 * all, removing it leaves that same row (or column) genuinely empty
 * only when it was that row's (or column's) sole occupant to begin
 * with, in which case shrinking the non-primary axis back by one
 * restores exactly the shape @a s_surface_layout_grow_for last grew it
 * from.
 *
 * A no-op otherwise (that row or column still has another real desktop
 * left in it), and a no-op once the non-primary axis is already down to
 * a single row or column, so this never shrinks a surface's layout
 * below @c 1 on either axis.
 *
 * @param surface   Surface whose layout to shrink
 * @param new_count The desktop count this surface now has, after the
 *                  desktop just removed no longer exists
 *
 * @note Complexity: @e O(1)
 */
static void s_surface_layout_shrink_after(surface_td *surface,
        uint32_t new_count)
{
    struct config_desktop_layout_s *layout;
    uint32_t primary;
    uint32_t *secondary;

    if (surface == NULL || surface->config == NULL ||
            surface->id >= (uint32_t) CONFIG_MAX_SCREENS) {
        return;
    }

    layout = &surface->config->base.screens[surface->id].desktop_layout;
    if (layout->orientation == CONFIG_DESKTOP_ORIENTATION_HORIZONTAL) {
        primary = layout->columns;
        secondary = &layout->rows;
    } else {
        primary = layout->rows;
        secondary = &layout->columns;
    }

    if (*secondary > 1u && new_count <= (*secondary - 1u) * primary) {
        (*secondary)--;
    }
}


/**
 * @brief How many clients one desktop can be emptied of in a single
 *        batch
 *
 * A desktop holding more than this is drained one batch at a time
 * instead of needing an array sized for the worst case up front.
 *
 * @see @a s_surface_desktop_evacuate's comment
 */
#define SWITCH_EVACUATE_MAX_CLIENTS (256)


/**
 * @brief What @a s_client_evacuate_visit is gathering into
 */
struct s_evacuate_ctx_s {
    client_td **out;    /**< Array the clients are put in */
    int capacity;       /**< How many it holds */
    int count;          /**< How many have been put in so far */
};


/**
 * @brief Gather one client, up to the array's capacity
 *
 * @param client Client reached by the walk
 * @param data   Pointer to the @c s_evacuate_ctx_s being filled
 *
 * @note Complexity: @e O(1)
 */
static void s_client_evacuate_visit(client_td *client, void *data)
{
    struct s_evacuate_ctx_s *const evacuate_ctx = data;

    if (client == NULL || evacuate_ctx == NULL ||
            evacuate_ctx->count >= evacuate_ctx->capacity) {
        return;
    }

    evacuate_ctx->out[evacuate_ctx->count++] = client;
}


/**
 * @brief Move every client still on @p from_desktop to @p to_desktop,
 *        updating EWMH @c _NET_WM_DESKTOP along the way
 *
 * Gathers up to @c SWITCH_EVACUATE_MAX_CLIENTS clients before moving
 * any of them, rather than repeatedly taking whichever client the
 * desktop holds first: moving one takes it off @p from_desktop, so
 * a walk that moved as it went would be reading a set it was itself
 * changing.  A desktop holding more than one batch's worth simply runs
 * another gather-and-move round, since a client that this round already
 * moved no longer shows up under @p from_desktop for the next one to
 * find; @p from_desktop ends up fully drained regardless of how many
 * clients it started with, rather than silently keeping whatever did
 * not fit in a single fixed-size array.
 *
 * A round is only repeated after one that both filled its batch and
 * actually moved at least one client in it, so a client that keeps
 * failing to move (@a desktop_action_client_move refusing every
 * attempt, most likely because @p to_desktop itself is somehow out of
 * room) is retried exactly once more and then left in place rather than
 * retried forever.
 *
 * @param from_desktop Desktop being emptied
 * @param to_desktop   Desktop every client moves to
 *
 * @note No-op if either desktop is @c NULL, or if @p from_desktop has
 *       no clients to begin with
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p from_desktop
 */
static void s_surface_desktop_evacuate(desktop_td *from_desktop,
        desktop_td *to_desktop)
{
    client_td *clients[SWITCH_EVACUATE_MAX_CLIENTS];
    struct s_evacuate_ctx_s evacuate_ctx;
    bool made_progress;

    if (from_desktop == NULL || to_desktop == NULL) {
        return;
    }

    do {
        evacuate_ctx.out = clients;
        evacuate_ctx.capacity =
            (int) (sizeof(clients) / sizeof(clients[0]));
        evacuate_ctx.count = 0;
        stacking_walk(from_desktop, s_client_evacuate_visit,
                &evacuate_ctx);

        made_progress = false;
        for (int index = 0; index < evacuate_ctx.count; ++index) {
            client_td *const client = clients[index];

            if (client == NULL) {
                continue;
            }

            if (desktop_action_client_move(from_desktop, to_desktop,
                    client) == 0) {
                made_progress = true;

                /* An iconified client keeps the icon position it
                 * already had on 'from_desktop'; that exact spot is
                 * only a coincidence on 'to_desktop', which may already
                 * have an icon of its own sitting right there.
                 * Relocated to a free spot, the same way a client
                 * repositions its icon (or gets a fresh one) whenever
                 * a saved position turns out already claimed; see
                 * 'ccmd_client_relocate_icon_if_taken' (see
                 * 'cmds/client/basic.h') for the exact same 'unless
                 * claimed' logic applied to a freshly (re-)iconified
                 * client. */
                ccmd_client_relocate_icon_if_taken(client);
                ccmd_publish_wm_desktop(client, to_desktop->id);
            }
        }
    } while (made_progress &&
            evacuate_ctx.count == evacuate_ctx.capacity);
}


/**
 * @brief Re-apply one client's maximized geometry
 *
 * @param client Client reached by the walk
 * @param data   Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_client_refill_visit(client_td *client, void *data)
{
    (void) data;

    if (client != NULL) {
        ccmd_client_refill_maximized(client);
    }
}


/**
 * @brief Re-fill every already-maximized client's geometry
 *        across every one of a surface's desktops
 *
 * @a ccmd_client_refill_maximized (cmds/client/geom.c) resolves and
 * applies one client's workarea fresh; run here for every client
 * on every desktop @p surface owns, right after its workarea
 * actually changed (@a surface_action_toggle_strutless_maximize), so an
 * already-maximized window visibly grows or shrinks into the panel-
 * reserved space that mode just set aside or folded back in, rather
 * than silently staying at whatever size it already was until the
 * user happens to un-maximize and re-maximize it by hand.
 *
 * @param surface Surface whose maximized clients should be
 *                re-filled
 *
 * @note No-op if @p surface or its desktop list is @c NULL
 * @note Complexity: @e O(n), where @e n is the total number of
 *       clients across every one of @p surface's desktops
 */
static void s_surface_refill_maximized_clients(surface_td *surface)
{
    cdlist_item_td *dnode;
    const cdlist_item_td *dinitial;

    if (surface == NULL || surface->desktops == NULL) {
        return;
    }

    dnode = cdlist_head(surface->desktops);
    if (dnode == NULL) {
        return;
    }

    dinitial = dnode;
    do {
        const desktop_td *const d = (desktop_td *) cdlist_data(dnode);

        stacking_walk(d, s_client_refill_visit, NULL);
        dnode = cdlist_next(dnode);
    } while (dnode != NULL && dnode != dinitial);
}


/* Add a new desktop to the surface */
int surface_action_desktop_add(surface_td *surface)
{
    desktop_td *desktop;

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    /* Restricted-memory mode is deliberately locked to exactly one
     * desktop, always; see 'config_set_default_values_memguard'
     * (config/memguard/defaults.c), which never lets 'memguard.json'
     * override 'desktop_count' away from its hardcoded '1u'.
     * Refused here too, not just left to whichever caller happens to
     * check first, so every path that could reach this function
     * (the window list's "Add new desktop" entry, its keyboard
     * shortcut, and any future one) is covered by the same single
     * guard. */
    if (memguard_max_clients() > 0u) {
        LOGGER_NOTICE("Cannot add another desktop to surface %u:" \
                " restricted-memory mode is locked to a single" \
                " desktop", surface->id);
        return 1;
    }

    /* 'config_base->screens[screen_id].desktops[desktop_id]'
     * (desktop.c, 's_desktop_read_config_settings' and its
     * caller) is a fixed-size 'CONFIG_MAX_DESKTOPS' array indexed by
     * this new desktop's ID, itself always 'desktop_count'
     * before the increment below; refused outright once that would
     * reach or exceed the array's real capacity, rather than
     * indexing past its end. */
    if (surface->desktop_count >= (uint32_t) CONFIG_MAX_DESKTOPS) {
        LOGGER_NOTICE("Cannot add another desktop to surface %u:" \
                " already at the configured maximum of %d",
                surface->id, CONFIG_MAX_DESKTOPS);
        return 1;
    }

    LOGGER_DEBUG("Adding new desktop to surface %u", surface->id);

    s_surface_layout_grow_for(surface, surface->desktop_count + 1u);

    desktop = desktop_init(xcb_connection_get(),
            surface->id,
            surface->desktop_count,
            surface->config);
    if (desktop == NULL) {
        LOGGER_ERROR("Failed to initialize new desktop on surface %u",
                surface->id);
        return 1;
    }

    if (surface_desktop_add(surface, desktop) != 0) {
        LOGGER_ERROR("Failed to add desktop to surface %u",
                surface->id);
        desktop_destroy(desktop);
        return 1;
    }

    s_surface_mark_all_desktops_outdated(surface);
    surface->is_outdated = true;

    return 0;
}


/* Remove the last desktop from the surface */
int surface_action_desktop_remove(surface_td *surface)
{
    cdlist_item_td *tail_item;
    cdlist_item_td *fallback_item;
    desktop_td *desktop;
    desktop_td *fallback;
    bool was_current;

    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Removing desktop from surface %u", surface->id);

    /* Need at least two desktops to remove one */
    if (surface->desktop_count <= 1) {
        LOGGER_NOTICE("Cannot remove the last desktop on surface %u",
                surface->id);
        return 1;
    }

    tail_item = cdlist_tail(surface->desktops);
    if (tail_item == NULL) {
        return 1;
    }

    desktop = (desktop_td *) cdlist_data(tail_item);
    if (desktop == NULL) {
        return 1;
    }

    /* The desktop immediately before the tail becomes both the new
     * tail once this one is gone, and the fallback home for any
     * client still on it: 'desktop_destroy' (via
     * 'surface_desktop_rem' below) frees its 'clients' hash
     * table through a 'client_destroy' callback on every entry left
     * in it, which would otherwise silently destroy every real,
     * live application window still on this desktop instead of just
     * the virtual desktop container itself. */
    fallback_item = cdlist_prev(tail_item);
    fallback = (fallback_item != NULL)
        ? (desktop_td *) cdlist_data(fallback_item) : NULL;
    if (fallback == NULL) {
        LOGGER_ERROR("No fallback desktop available on surface %u",
                surface->id);
        return 1;
    }

    was_current = (desktop->id == surface->desktop_cur);
    if (was_current) {
        surface_clients_hide(surface, surface->desktop_cur);
    }

    s_surface_desktop_evacuate(desktop, fallback);

    /* If the desktop to be removed is the current one, switch first.
     * Set 'desktop_cur' to 'fallback' directly, the exact same
     * desktop 's_surface_desktop_evacuate' just above already moved
     * every client onto, rather than through
     * 'surface_desktop_select_west': west is grid-aware since
     * desktops gained a configurable
     * row/column layout, and can genuinely find nothing at all once
     * the desktop being removed sits at the west edge of its own
     * row (column 0), even though 'fallback' itself, one row/column
     * over in list order, is right there and perfectly valid; west
     * failing there left 'desktop_cur' pointing at 'desktop' itself,
     * which 'surface_desktop_rem' below is about to destroy, a
     * dangling reference to a desktop that no longer exists. */
    if (was_current) {
        surface->desktop_cur = fallback->id;
        surface_clients_show(surface, surface->desktop_cur);
    } else if (fallback->id == surface->desktop_cur) {
        /* The removed desktop was not the one on screen, but its
         * fallback already was, so neither branch above ever ran a
         * 'show' cycle for it: without this, every client (and every
         * iconified client's icon window) that
         * 's_surface_desktop_evacuate' just moved onto it stays
         * exactly as mapped or
         * unmapped as it was on the desktop just destroyed, which for
         * anything that was not the surface's current desktop
         * before this whole operation started means unmapped, i.e.,
         * invisible, with nothing else left to ever map it.  No further
         * desktop switch is coming (fallback is already current), and
         * with only the two desktops involved existing at all, there
         * may be nowhere left to switch to and back from, even by
         * hand. */
        surface_clients_show(surface, surface->desktop_cur);
    }

    if (surface_desktop_rem(surface, desktop->id) != 0) {
        LOGGER_ERROR("Failed to remove desktop from surface %u",
                surface->id);
        return 1;
    }

    s_surface_layout_shrink_after(surface, surface->desktop_count);

    s_surface_mark_all_desktops_outdated(surface);
    surface->is_outdated = true;

    return 0;
}


/* Toggle strutless-maximization mode */
int surface_action_toggle_strutless_maximize(surface_td *surface)
{
    if (surface == NULL) {
        LOGGER_ERROR("Invalid surface pointer", L_NARG);
        return -1;
    }

    LOGGER_DEBUG("Toggling strutless-maximization mode on surface %u",
            surface->id);

    surface->strutless_maximize = !surface->strutless_maximize;

    /* Recompute every desktop's work area right away.  Struts are
     * now folded in, or set aside, differently than a moment ago (see
     * 'desktop_update_workarea''s 'ignore_struts' parameter,
     * desktop.h), and nothing else is guaranteed to trigger that
     * recomputation on its own until some unrelated event (a client
     * mapping, an RandR change, and so on) happens to call
     * 'surface_refresh_workareas' next. */
    surface_refresh_workareas(surface);

    /* Grow or shrink every already-maximized client into whichever
     * workarea it now resolves to, immediately.  The toggle would
     * otherwise have no visible effect at all on a window that was
     * already maximized before it ran, since maximize geometry is
     * only ever computed once, at the moment a client is actually
     * maximized, not continuously re-derived from the desktop's
     * workarea afterward. */
    s_surface_refill_maximized_clients(surface);

    /* Every desktop needs its redraw, not just 'surface' itself:
     * see 's_surface_mark_all_desktops_outdated''s comment above
     * for the identical reasoning already applied to desktop add and
     * remove. */
    s_surface_mark_all_desktops_outdated(surface);
    surface->is_outdated = true;

    return 0;
}
