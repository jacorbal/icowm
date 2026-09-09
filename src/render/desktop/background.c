/**
 * @file render/desktop/background.c
 *
 * @brief Desktop root window background rendering
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>     /* free */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/xcb/atom.h>
#include <utils/xcb/connection.h>

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <render/viewport/mesh.h>

/* Local includes */
#include <render/desktop/background.h>


/* Per-screen (not per-desktop) cache of the root window's last
 * solid-color fill, indexed by 'screen_id'.  Every virtual desktop on
 * a given screen shares that one same root window as an X resource, so
 * whether it still shows a particular desktop's configured color has to
 * be tracked per screen too, not per desktop.
 *
 * A field on 'desktop_td' itself would instead let each desktop believe
 * its color remains applied purely because it was the last one THAT
 * desktop painted, even after some other desktop sharing the same root
 * window repainted over it with a different one; and, since a config
 * reload does not reset any of this, leaves that other, now-stale color
 * on screen indefinitely, with no further repaint ever believing there
 * is anything left to fix. */
static bool s_root_bg_applied_once[CONFIG_MAX_SCREENS];
static uint32_t s_root_bg_color_applied[CONFIG_MAX_SCREENS];


/**
 * @brief Property names a wallpaper-setting tool might publish its own
 *        root window background pixmap under
 *
 * - @c ESETROOT_PMAP_ID: legacy @c Esetroot alias, also used by @c feh
 * - @c _XROOTPMAP_ID: set by @c Esetroot, @c feh, @c nitrogen,
 *   @c hsetroot, and others
 * - @c _XSETROOT_ID: set by @c xsetroot and @c xsetbg
 *
 * Shared between @c s_get_root_background_pixmap, which checks each in
 * turn for a pixmap value, and
 * @c render_desktop_background_property_is_pixmap, which the
 * @c PropertyNotify handler in handler/focus.c uses to recognize
 * a change to one of them on the root window.
 */
static const char *const s_bg_prop_names[3] = {
    "_XROOTPMAP_ID", "ESETROOT_PMAP_ID", "_XSETROOT_ID"
};


/**
 * @brief Cached, once-resolved atoms for @c s_bg_prop_names
 *
 * None of these ever changes once interned (an atom, once assigned by
 * the X server, is permanent for the life of the connection), so
 * resolving them again on every lookup would be pure waste; resolved
 * lazily by @a s_resolve_bg_atoms on first use, retried on any later
 * call for whichever of the three are still unresolved.
 *
 * @see @a s_resolve_bg_atoms's comment for why a resolution failure,
 *      unlike a success, is not permanent here
 */
static xcb_atom_t s_bg_atoms[3] = {
    XCB_ATOM_NONE, XCB_ATOM_NONE, XCB_ATOM_NONE
};


/**
 * @brief Cached resolution of the root window's background pixmap
 *
 * A well-behaved system sets this once (a wallpaper tool such as feh,
 * nitrogen, or hsetroot runs once at session start) and essentially
 * never changes it again during a normal session, so resolving it fresh
 * on every @a s_get_root_background_pixmap call (up to three property
 * fetches, each a round trip to the X server) would be paying that cost
 * repeatedly for something that stays the same almost every single
 * time.
 *
 * @note Cached here instead, and only re-resolved once
 *       @a render_desktop_background_cache_invalidate says the
 *       underlying property actually changed
 */
static bool s_bg_pixmap_resolved[CONFIG_MAX_SCREENS];
static xcb_pixmap_t s_bg_pixmap_cache[CONFIG_MAX_SCREENS];


/**
 * @brief Resolve @c s_bg_prop_names into @c s_bg_atoms, once
 *
 * @param connection XCB connection used to intern any atom not already
 *                   resolved from a previous call
 *
 * @note Complexity: @e O(1) once resolved; @e O(n) in the number of
 *       candidate properties the first time
 */
static void s_resolve_bg_atoms(xcb_connection_t *connection)
{
    bool any_unresolved = false;

    /* Re-attempts only whichever of the three atoms are still
     * 'XCB_ATOM_NONE', rather than giving up on all three permanently
     * the moment any single attempt is made.
     *
     * 'only_if_exists=true' in 'atom_intern' means a name that does not
     * exist yet on the X server resolves to none, which is correct at
     * that moment, but unlike a successful resolution (an atom, once it
     * exists, is permanent for the life of the connection) that failure
     * is not itself permanent.
     *
     * A wallpaper tool run for the first time after this module's first
     * lookup, before any of these three names had ever been interned by
     * anyone, would otherwise be watched for forever using an atom id
     * that was cached as none before it ever existed. */
    for (size_t i = 0; i < 3u; ++i) {
        if (s_bg_atoms[i] == XCB_ATOM_NONE) {
            any_unresolved = true;
            break;
        }
    }

    if (!any_unresolved) {
        return;
    }

    for (size_t i = 0; i < 3u; ++i) {
        if (s_bg_atoms[i] == XCB_ATOM_NONE) {
            s_bg_atoms[i] = atom_intern(connection, s_bg_prop_names[i],
                    true);
        }
    }
}


/**
 * @brief Retrieve the root window background pixmap if set
 *
 * Queries the root window for one of the standard background pixmap
 * properties (@c _XROOTPMAP_ID, @c ESETROOT_PMAP_ID, @c _XSETROOT_ID)
 * and returns the first non-@c XCB_NONE pixmap found, or @c XCB_NONE if
 * no valid pixmap is present.
 *
 * A cache hit costs nothing beyond returning the cached value, in
 * contrast to a miss, which pays for up to three property fetches, each
 * its round trip to the X server.
 *
 * @param connection XCB connection to the X server
 * @param root       Root window to query for background pixmap
 * @param screen_id  Screen the cache entry belongs to, one entry per
 *                   managed screen
 *
 * @return Root background pixmap, or @c XCB_NONE where none is
 *         available
 *
 * @note Resolved once and cached from then on
 * @note Complexity: @e O(1) on a cache hit; @e O(n) in the number of
 *       candidate properties on a cache miss
 *
 * @see @a s_bg_pixmap_resolved above
 */
static xcb_pixmap_t
    s_get_root_background_pixmap(xcb_connection_t *connection,
            xcb_window_t root, uint32_t screen_id)
{
    if (screen_id >= (uint32_t) CONFIG_MAX_SCREENS) {
        return XCB_NONE;
    }

    if (s_bg_pixmap_resolved[screen_id]) {
        LOGGER_TRACE("Root background pixmap cache hit for screen" \
                " %u: 0x%x", screen_id, s_bg_pixmap_cache[screen_id]);
        return s_bg_pixmap_cache[screen_id];
    }

    if (connection == NULL || root == XCB_WINDOW_NONE) {
        return XCB_NONE;
    }

    s_resolve_bg_atoms(connection);

    for (size_t i = 0; i < 3u; ++i) {
        xcb_get_property_reply_t *reply;
        xcb_pixmap_t pixmap = XCB_NONE;

        if (s_bg_atoms[i] == XCB_ATOM_NONE) {
            continue;
        }

        reply = xcb_get_property_reply(connection,
                xcb_get_property(connection, 0, root, s_bg_atoms[i],
                    XCB_ATOM_PIXMAP, 0, 1), NULL);
        if (reply == NULL) {
            continue;
        }

        if (reply->format == 32 && reply->value_len >= 1 &&
                xcb_get_property_value(reply) != NULL) {
            pixmap = *((xcb_pixmap_t *) xcb_get_property_value(reply));
        }
        free(reply);

        if (pixmap != XCB_NONE) {
            s_bg_pixmap_cache[screen_id] = pixmap;
            s_bg_pixmap_resolved[screen_id] = true;
            return pixmap;
        }
    }

    /* No wallpaper tool has set any of the candidate properties; that
     * is itself a stable outcome worth caching too, not just
     * a successful resolution, so a desktop with no such tool running
     * does not keep paying for this same negative lookup either */
    LOGGER_TRACE("No external root pixmap property found for screen" \
            " %u (checked atoms 0x%x, 0x%x, 0x%x); using configured" \
            " color", screen_id, s_bg_atoms[0], s_bg_atoms[1],
            s_bg_atoms[2]);
    s_bg_pixmap_cache[screen_id] = XCB_NONE;
    s_bg_pixmap_resolved[screen_id] = true;
    return XCB_NONE;
}


/* Invalidate the cached root window background pixmap on every screen */
void render_desktop_background_cache_invalidate(void)
{
    for (size_t i = 0; i < (size_t) CONFIG_MAX_SCREENS; ++i) {
        s_bg_pixmap_resolved[i] = false;
        s_bg_pixmap_cache[i] = XCB_NONE;
    }
}


/* Recognize whether an atom is one of the background pixmap properties
 * this module watches */
bool render_desktop_background_property_is_pixmap(
        xcb_connection_t *connection, xcb_atom_t atom)
{
    if (atom == XCB_ATOM_NONE) {
        return false;
    }

    s_resolve_bg_atoms(connection);

    for (size_t i = 0; i < 3u; ++i) {
        if (s_bg_atoms[i] != XCB_ATOM_NONE && s_bg_atoms[i] == atom) {
            return true;
        }
    }

    return false;
}


/* Draw the background of a desktop */
int render_desktop_background_render(desktop_td *desktop)
{
    xcb_screen_t *screen;
    uint32_t values[2];
    xcb_pixmap_t root_pixmap;

    if (desktop == NULL) {
        LOGGER_ERROR("Received null desktop pointer", L_NARG);
        return 1;
    }

    if (desktop->screen_id >= (uint32_t) CONFIG_MAX_SCREENS) {
        LOGGER_ERROR("Desktop %u ('%s') has an out-of-range screen_id" \
                " %u; cannot render its background",
                desktop->id, desktop->name, desktop->screen_id);
        return 1;
    }

    LOGGER_TRACE("Rendering background for desktop %u ('%s')" \
            " with color #%06x",
            desktop->id, desktop->name, desktop->background.bg.color);

    /* O(1): reuses the pointer 'desktop_init' (desktop.c) already
     * resolved once for this desktop, rather than re-walking every
     * screen from scratch (O(n)) on every single repaint */
    screen = desktop->screen;
    if (screen == NULL) {
        LOGGER_ERROR("Could not get screen for background rendering",
                L_NARG);
        return 1;
    }

    root_pixmap = s_get_root_background_pixmap(xcb_connection_get(),
            screen->root, desktop->screen_id);
    if (root_pixmap != XCB_NONE) {
        /* An external tool ('xsetbg', 'xsetroot', 'nitrogen', 'feh',
         * &c.) painted the root window and recorded the pixmap ID in
         * a well-known atom.  Record that fact so that subsequent
         * repaints do not overwrite the wallpaper with our color.
         */
        /* NOTE: Deliberately do NOT set 'XCB_CW_BACK_PIXMAP' on the
         *       root window to this pixmap.  Many setters free the
         *       pixmap after drawing (the pixels persist in the root
         *       drawable), so referencing it via 'XCB_CW_BACK_PIXMAP'
         *       would cause X to use a freed resource on the next
         *       'xcb_clear_area', and leading to a 'BadPixmap' or
         *       'BadDrawable' X error and an abrupt crash. */
        desktop->background.use_root_pixmap = true;
        /* So that if the WM ever owns the background again later (the
         * external pixmap atom disappears), the solid-color path below
         * always re-applies at least once even if that color happens to
         * equal whatever it last applied before this external pixmap
         * appeared.  Otherwise this screen's shared root window would
         * be left showing the stale external wallpaper under the
         * mistaken belief that the WM's color was already correctly in
         * place. */
        s_root_bg_applied_once[desktop->screen_id] = false;
        LOGGER_TRACE("External root pixmap 0x%x detected for" \
                " desktop %u ('%s'); skipping color fill",
                root_pixmap, desktop->id, desktop->name);
        return 0;
    }

    if (desktop->background.use_root_pixmap) {
        /* No pixmap atom found this time, but an external tool
         * previously painted the root window.  The pixels are still
         * there; leave the root window untouched so the wallpaper
         * remains visible. */
        LOGGER_TRACE("Preserving previous external background for" \
                " desktop %u ('%s')", desktop->id, desktop->name);
        return 0;
    }

    /* The mesh takes over the very background pixmap the color path
     * below would clear, so it is asked first and, where it applies,
     * owns the root window instead.  Its own cache decides whether
     * anything actually gets repainted, exactly as the color one
     * does; and 's_root_bg_applied_once' is cleared so that the color
     * path always re-applies at least once should the mesh later stop
     * applying, rather than leaving a stale mesh on screen under the
     * belief that the color was already correct. */
    if (viewport_mesh_is_visible(desktop)) {
        s_root_bg_applied_once[desktop->screen_id] = false;
        return viewport_mesh_render(xcb_connection_get(), desktop);
    }
    /* Deliberately not invalidated here.  Dropping the tile frees it
     * while the root's own 'XCB_CW_BACK_PIXMAP' still names it, and
     * the early return just below, taken whenever the color has not
     * changed, would leave the attribute pointing at a freed pixmap
     * for as long as it stays unchanged.  The tile is dropped after
     * the attribute has been pointed away from it instead, at the end
     * of this function. */

    /* No external background detected and the window manager owns the
     * background: apply the configured color and clear the root window
     * to make it visible.  Only actually do so when the color changed
     * since the last time this ran, or on the very first pass.  This
     * function runs on every 'is_current' full-desktop render (every
     * client gaining focus marks its desktop 'is_outdated', not just an
     * actual background change), so without this check every such
     * render would repeat the same full-screen
     * 'xcb_change_window_attributes' + 'xcb_clear_area' for a color
     * that never actually changed.  Checked and updated per screen (see
     * 's_root_bg_applied_once' above), not per desktop.  Every desktop
     * sharing this screen's one root window can otherwise repaint over
     * whichever color another one on the same screen applied, without
     * either ever detecting that the color actually showing has changed
     * since its last render. */
    if (s_root_bg_applied_once[desktop->screen_id] &&
            s_root_bg_color_applied[desktop->screen_id] ==
                desktop->background.bg.color) {
        return 0;
    }

    values[0] = XCB_BACK_PIXMAP_NONE;
    values[1] = desktop->background.bg.color;
    xcb_change_window_attributes(xcb_connection_get(), screen->root,
            XCB_CW_BACK_PIXMAP | XCB_CW_BACK_PIXEL, values);
    xcb_clear_area(xcb_connection_get(), 0, screen->root, 0, 0,
            screen->width_in_pixels, screen->height_in_pixels);
    s_root_bg_applied_once[desktop->screen_id] = true;
    s_root_bg_color_applied[desktop->screen_id] =
        desktop->background.bg.color;

    /* Only now: 'XCB_BACK_PIXMAP_NONE' above is what stopped the root
     * from naming whichever mesh tile was cached, so this is the
     * first moment at which freeing it cannot leave the attribute
     * pointing at a pixmap the server no longer has */
    viewport_mesh_cache_invalidate();
    viewport_mesh_cache_release_retired(xcb_connection_get());

    LOGGER_TRACE("Background rendered for desktop %u ('%s')",
            desktop->id, desktop->name);

    return 0;
}
