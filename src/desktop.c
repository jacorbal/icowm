/**
 * @file desktop.c
 *
 * @brief Desktop lifecycle: init, update, destroy, rename, background
 *
 * Implements @c desktop_init, @c desktop_destroy, @c desktop_update,
 * @c desktop_update_full, @c desktop_update_workarea, @c desktop_clear,
 * @c desktop_action_rename, and @c desktop_action_background_update.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* NULL, free, malloc */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Default initial values */
#include <defs/desktop.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>  /* Open-addressed hash table (closed hashing) */

/* Utils includes */
#include <utils/hash/murmurhash.h>
#include <utils/safe/safestr.h>

/* Project includes */
#include <client.h>
#include <logger.h>
#include <memguard.h>
#include <monitor.h>
#include <surface.h>

/* Local includes */
#include <desktop.h>


/**
 * @brief Check whether two inclusive integer ranges overlap
 *
 * Tests whether the range defined by @p a_start and @p a_end intersects
 * the range defined by @p b_start and @p b_end, treating both endpoints
 * as inclusive.  If the first range is given as @c (0..0), it is
 * treated as unbounded so legacy @c _NET_WM_STRUT values without
 * explicit start/end coordinates still match any target range.  Invalid
 * second ranges, where @p b_end is less than @p b_start, are rejected.
 *
 * @param a_start Start of the first range
 * @param a_end   End of the first range
 * @param b_start Start of the second range
 * @param b_end   End of the second range
 *
 * @return @c true if the ranges overlap, otherwise @c false
 *
 * @note Complexity: @e O(1)
 */
static bool s_ranges_overlap(int32_t a_start, int32_t a_end,
        int32_t b_start, int32_t b_end)
{
    if (b_end < b_start) {
        return false;
    }

    /* Legacy '_NET_WM_STRUT' has no start/end fields; treat 0..0 as
     * unbounded so those struts still reserve space. */
    if (a_start == 0 && a_end == 0) {
        return true;
    }

    if (a_start > a_end) {
        int32_t tmp;

        tmp = a_start;
        a_start = a_end;
        a_end = tmp;
    }

    return !(a_end < b_start || a_start > b_end);
}


/**
 * @brief Fold one more strut into a running maximum reservation on each
 *        of the four edges of one region
 *
 * Shared by @c desktop_update_workarea for both a stacked client's
 * @c layout.strut_partial and the systray's own reservation, and for
 * both the whole surface's own work area and each individual
 * monitor's own.  The two strut sources are folded identically
 * either way; each edge keeps whichever single source reserves the
 * most there, the struts are not summed together (unlike
 * @a config_desktop_s's @p margins, a deliberately different, additive
 * case).
 *
 * @param strut         Strut to fold in; a no-op when null
 * @param region_min_x  Region's own minimum X coordinate, for the
 *                      top/bottom range overlap check; @c 0 for the
 *                      whole surface, a monitor's own @c x otherwise
 * @param region_max_x  Region's own maximum X coordinate, same axis
 * @param region_min_y  Region's own minimum Y coordinate, for the
 *                      left/right range overlap check; @c 0 for the
 *                      whole surface, a monitor's own @c y otherwise
 * @param region_max_y  Region's own maximum Y coordinate, same axis
 * @param left          Running left reservation, updated in place
 * @param right         Running right reservation, updated in place
 * @param top           Running top reservation, updated in place
 * @param bottom        Running bottom reservation, updated in place
 *
 * @note Complexity: @e O(1)
 *
 * @see @a systray_get_reserved_strut and @a desktop_update_workarea
 */
static void s_fold_strut(const struct strut_partial_s *strut,
        int32_t region_min_x, int32_t region_max_x,
        int32_t region_min_y, int32_t region_max_y,
        int32_t *restrict left, int32_t *restrict right,
        int32_t *restrict top, int32_t *restrict bottom)
{
    if (strut == NULL) {
        return;
    }

    if (strut->sides.left > *left &&
            s_ranges_overlap(strut->start.left, strut->end.left,
                region_min_y, region_max_y)) {
        *left = strut->sides.left;
    }

    if (strut->sides.right > *right &&
            s_ranges_overlap(strut->start.right, strut->end.right,
                region_min_y, region_max_y)) {
        *right = strut->sides.right;
    }

    if (strut->sides.top > *top &&
            s_ranges_overlap(strut->start.top, strut->end.top,
                region_min_x, region_max_x)) {
        *top = strut->sides.top;
    }

    if (strut->sides.bottom > *bottom &&
            s_ranges_overlap(strut->start.bottom, strut->end.bottom,
                region_min_x, region_max_x)) {
        *bottom = strut->sides.bottom;
    }
}


/**
 * @brief Primary stable hash function for client entries
 *
 * Computes a reproducible 32-bit MurmurHash3 value using the client's
 * identifier as key and a fixed seed to ensure stable distribution
 * across runs.
 *
 * @param data Pointer to a @c client_td structure (may be null)
 *
 * @return Hash value as @c size_t
 *
 * @note A null client is treated as key value 0
 * @note Uses seed @c DESKTOP_HASH_SEED_PRIMARY
 * @note Complexity: @e O(1)
 */
static size_t s_h1(const void *data)
{
    const client_td *client = (const client_td *) data;
    const uint32_t key = (client == NULL) ? 0u : client->id;

    /* Stable primary hash using a fixed seed (0x9E3779B9, 32-bit golden
     * ratio, 2^32/phi) to ensure good dispersion and reproducible
     * results across runs. */
    return (size_t) murmurhash3_32(&key, sizeof(key),
            DESKTOP_HASH_SEED_PRIMARY);
}


/**
 * @brief Secondary stable hash function for double hashing
 *
 * Computes an auxiliary MurmurHash3 value using a different fixed seed
 * to reduce correlation with the primary hash.  The result is
 * guaranteed to be non-zero to ensure a valid probing step.
 *
 * @param data Pointer to a @c client_td structure (may be null)
 *
 * @return Non-zero hash value as @c size_t
 *
 * @note A null client is treated as key value 0
 * @note Uses seed @c DESKTOP_HASH_SEED_SECONDARY
 * @note Complexity: @e O(1)
 */
static size_t s_h2(const void *data)
{
    const client_td *client = (const client_td *) data;
    const uint32_t key = (client == NULL) ? 0u : client->id;
    size_t hash2 = (size_t) murmurhash3_32(&key, sizeof(key),
            DESKTOP_HASH_SEED_SECONDARY);

    /* Stable secondary hash using a different fixed seed (0x85EBCA6B)
     * to reduce correlation with 'h1'.  The result is forced to be
     * non-zero to guarantee a valid step size in double hashing. */
    hash2 = (hash2 == 0u) ? 1u : hash2;

    /* Internal invariant, not external input validation that verifies
     * this function's documented postcondition (never zero) holds after
     * the line just above, so a future edit to that forcing logic that
     * accidentally breaks it is caught immediately in a debug build
     * rather than silently producing an invalid double-hashing step
     * size. */
    assert(hash2 != 0u);

    return hash2;
}


/**
 * @brief Compare two clients by identifier
 *
 * Determines whether two client entries represent the same logical
 * entity by comparing their unique identifiers.
 *
 * @param key1 Pointer to first @c client_td
 * @param key2 Pointer to second @c client_td
 *
 * @return Comparison result
 * @retval true  Both clients have the same identifier
 * @retval false Identifiers differ
 *
 * @note Behavior is undefined if either pointer is null
 * @note Complexity: @e O(1)
 */
static bool s_client_match(const void *key1, const void *key2)
{
    const client_td *client1 = (const client_td *) key1;
    const client_td *client2 = (const client_td *) key2;

    /* Internal invariant, not external input validation: this is an
     * ohtbl comparator, called only by 'ohtbl.c''s internals with
     * entries already stored in the table, never with
     * attacker-controlled or user-controlled input.
     *
     * Catches a future bug in that internal calling logic immediately
     * in a debug build instead of the bare, unexplained segfault the
     * dereferences just below would otherwise produce.
     *
     * Compiled out entirely in the default release build ('NDEBUG'). */
    assert(key1 != NULL);
    assert(key2 != NULL);

    return client1->id == client2->id;
}


/**
 * @brief Compute one work area, struts and margins folded in, scoped
 *        to a single rectangular region
 *
 * Shared by @a desktop_update_workarea for both the whole surface's
 * own @c workarea and each individual monitor's own entry in
 * @c monitor_workareas, the exact same reservation math either way,
 * only the region it is scoped to differing: the whole surface for
 * the former, one monitor's own physical extent for the latter.
 *
 * @param desktop              Desktop whose stacking list to scan
 *                             for client struts
 * @param region_x             Region's own left edge, in surface
 *                             coordinates
 * @param region_y             Region's own top edge, in surface
 *                             coordinates
 * @param region_w             Region's own width
 * @param region_h             Region's own height
 * @param apply_margin_left    Whether this region's own left edge
 *                             coincides with a side of the surface
 *                             @p config_desktop's own @p margins
 *                             should actually reserve on
 * @param apply_margin_right   Same, for the right edge
 * @param apply_margin_top     Same, for the top edge
 * @param apply_margin_bottom  Same, for the bottom edge
 * @param config_desktop       Active desktop-behavior configuration,
 *                             for its own @p margins; a @c NULL
 *                             treats every margin as @c 0
 * @param systray_strut        The systray's own current reservation;
 *                             a @c NULL value folds in nothing
 * @param ignore_struts        When @c true, neither @p systray_strut
 *                             nor any client's own strut is folded
 *                             in, only whichever margins @p
 *                             apply_margin_* select
 *
 * @return The resulting work area, in surface coordinates
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static struct geometry_s s_desktop_compute_workarea(
        const desktop_td *desktop,
        int32_t region_x, int32_t region_y,
        uint32_t region_w, uint32_t region_h,
        bool apply_margin_left, bool apply_margin_right,
        bool apply_margin_top, bool apply_margin_bottom,
        const struct config_desktop_s *config_desktop,
        const struct strut_partial_s *systray_strut,
        bool ignore_struts)
{
    struct geometry_s result;
    int32_t left = 0;
    int32_t right = 0;
    int32_t top = 0;
    int32_t bottom = 0;
    int32_t new_w;
    int32_t new_h;
    int32_t region_max_x = (region_w == 0u)
        ? region_x - 1 : region_x + (int32_t) (region_w - 1u);
    int32_t region_max_y = (region_h == 0u)
        ? region_y - 1 : region_y + (int32_t) (region_h - 1u);

    if (!ignore_struts && desktop->stacking != NULL &&
            cdlist_size(desktop->stacking) > 0) {
        cdlist_item_td *initial;
        cdlist_item_td *node;

        /* Aggregate maximum strut on each edge across all stacked
         * clients */
        initial = cdlist_head(desktop->stacking);
        node = initial;
        do {
            client_td *c = (client_td *) cdlist_data(node);

            if (c != NULL) {
                s_fold_strut(&c->layout.strut_partial,
                        region_x, region_max_x,
                        region_y, region_max_y,
                        &left, &right, &top, &bottom);
            }
            node = cdlist_next(node);
        } while (node != NULL && node != initial);
    }

    /* The window manager's own built-in systray is not a managed
     * client (its dock window is override-redirect; see
     * 'systray_protocol_window_ensure'), so it never appears in
     * 'desktop->stacking' above and needs folding in separately
     * here; aggregated the exact same way, since it is a strut
     * source like any other from this function's own point of
     * view.  Skipped, like every other strut above, when
     * 'ignore_struts' asks for the full region. */
    if (!ignore_struts) {
        s_fold_strut(systray_strut, region_x, region_max_x,
                region_y, region_max_y,
                &left, &right, &top, &bottom);
    }

    /* Configured margins ('config.json''s 'desktops.margins') add on
     * top of whatever clients themselves already reserve on each
     * edge above, rather than only keeping whichever of the two is
     * larger.  They cover a distinct case (a program that reserves
     * screen space without publishing '_NET_WM_STRUT'/'_NET_WM_
     * STRUT_PARTIAL' itself, e.g., Conky) so both are meant to
     * coexist, not override one another.  Applied even with no
     * clients at all (the early return this replaced never used to
     * reach here), so a configured margin still reserves its space
     * on an empty desktop.  Only on whichever side of this region
     * actually coincides with that same side of the whole surface,
     * per 'apply_margin_left'/etc: an internal boundary between two
     * monitors is not "the screen edge" a margin is meant to carve
     * out in the first place. */
    if (config_desktop != NULL) {
        if (apply_margin_left) {
            left += (int32_t) config_desktop->margins.left;
        }
        if (apply_margin_right) {
            right += (int32_t) config_desktop->margins.right;
        }
        if (apply_margin_top) {
            top += (int32_t) config_desktop->margins.top;
        }
        if (apply_margin_bottom) {
            bottom += (int32_t) config_desktop->margins.bottom;
        }
    }

    new_w = (int32_t) region_w - left - right;
    new_h = (int32_t) region_h - top - bottom;

    result.pos.x = region_x + left;
    result.pos.y = region_y + top;
    result.dim.w = (new_w > 0) ? (uint32_t) new_w : 0u;
    result.dim.h = (new_h > 0) ? (uint32_t) new_h : 0u;

    return result;
}


/* Initialize a new desktop */
desktop_td *desktop_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        uint32_t screen_id, uint32_t desktop_id,
        config_td *config)
{
    desktop_td *desktop;
    xcb_screen_t *screen;
    xcb_screen_iterator_t iter;
    uint32_t initial_positions;

    LOGGER_DEBUG("Initializing desktop %u on screen %u",
            desktop_id, screen_id);

    desktop = malloc(sizeof(desktop_td));
    if (desktop == NULL) {
        LOGGER_ERROR("Failed to allocate memory for" \
                " desktop %u on screen %u", desktop_id, screen_id);
        return NULL;
    }

    /* Establish the basics */
    desktop->screen_id = screen_id;
    desktop->id = desktop_id;
    desktop->client_active_id = 0;
    desktop->ewmh = ewmh;
    desktop->connection = connection;

    /* Get the configuration */
    desktop->config = config;

    /* Set desktop name.  The config-provided name is copied with
     * 'safe_strncpy' ('utils/safe/safestr.h') instead of
     * 'snprintf("%s", ...)' because its source field is wider than
     * 'desktop->name'.  GCC's option '-Wformat-truncation' cannot prove
     * the copy never truncates, and truncating a name that does not fit
     * is the desired, harmless behavior here anyway. */
    if (config->base.screens[screen_id].desktops[desktop_id].name[0] ==
        '\0') {
        snprintf(desktop->name, WM_DESKTOP_MAX_LENGTH_NAME,
                "Desktop %u", desktop_id);
    } else {
        safe_strncpy(desktop->name,
                config->base.screens[screen_id].desktops[desktop_id]
                    .name,
                WM_DESKTOP_MAX_LENGTH_NAME);
    }

    /* Set background color: a desktop entry that set its own
     * 'background-color' (vid. 'config.json') always keeps it; one that
     * did not (still holding 'WM_DESKTOP_BG_COLOR_UNSET', the sentinel
     * every entry starts with) falls back to
     * 'theme.desktop.color.background' (vid. 'theme.json') instead. */
    desktop->background.is_image = false;
    desktop->background.use_root_pixmap = false;
    desktop->background.bg.color =
        config->base.screens[screen_id].desktops[desktop_id]
            .settings.background.color;
    if (desktop->background.bg.color == WM_DESKTOP_BG_COLOR_UNSET) {
        desktop->background.bg.color =
            config->theme.desktop.color.background;
    }

    LOGGER_TRACE("Initializing client list structure for" \
            " desktop %u ('%s') on screen %u",
            desktop_id, desktop->name, screen_id);

    /* Initialize hash table for quick client lookup.
     * In restricted-memory mode, sized to what 'memguard_max_clients'
     * actually expects this desktop to ever hold instead of the usual,
     * much larger 'WM_DESKTOP_INITIAL_CAPACITY'.  That default is meant
     * for an ordinary session where the number of windows someone might
     * open is not meaningfully bounded, which defeats the whole point
     * of a mode meant to keep memory use predictable.  Doubled rather
     * than sized to the cap exactly, since 'ohtbl_insert' resizes once
     * occupancy reaches OHTBL_MAX_LOAD_FACTOR (75%) of positions;
     * sizing to the cap exactly would mean hitting that threshold, and
     * doubling the table anyway, before the cap itself is ever
     * reached. */
    initial_positions = memguard_max_clients();
    initial_positions = (initial_positions > 0u)
        ? (initial_positions * 2u)
        : WM_DESKTOP_INITIAL_CAPACITY;

    desktop->clients =
        ohtbl_init(initial_positions, 0,
                s_h1, s_h2, s_client_match,
                (void(*)(void *)) client_destroy);
    if (desktop->clients == NULL) {
        LOGGER_ERROR("Failed to allocate memory for" \
                " client hash table on desktop %u ('%s') on screen %u",
                desktop_id, desktop->name, screen_id);
        free(desktop);
        return NULL;
    }

    LOGGER_TRACE("Initializing stacking list structure for" \
            " desktop %u ('%s') on screen %u",
            desktop_id, desktop->name, screen_id);

    /* Initialize circular list for rendering in stacking order.
     * Ownership of client memory is managed by 'desktop->clients' */
    desktop->stacking = cdlist_init(NULL);
    if (desktop->stacking == NULL) {
        LOGGER_ERROR("Failed to allocate memory for stacking list" \
                " on desktop %u ('%s') on screen %u",
                desktop_id, desktop->name, screen_id);
        ohtbl_destroy(desktop->clients);
        free(desktop);
        return NULL;
    }

    /* Get XCB screen to obtain dimensions */
    iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
    screen = NULL;

    /* Iterate through screens to find the "correct" one */
    for (uint32_t i = 0; i < screen_id && iter.rem > 0; ++i) {
        xcb_screen_next(&iter);
    }

    if (iter.rem == 0 || iter.data == NULL) {
        LOGGER_ERROR("Invalid screen ID %u, could not retrieve" \
                " screen information", screen_id);
        cdlist_destroy(desktop->stacking);
        ohtbl_destroy(desktop->clients);
        free(desktop);
        return NULL;
    }

    screen = iter.data;

    /* Kept for every later 'O(1)' lookup of this desktop's own screen.
     * This same walk already had to resolve it just above to read its
     * dimensions. */
    desktop->screen = screen;

    /* Initialize geometry with screen dimensions */
    desktop->geometry = (struct geometry_s) {
        .pos = {.x = 0, .y = 0},
        .dim = {.w = screen->width_in_pixels,
                .h = screen->height_in_pixels}
    };

    /* Initialize workarea to full screen; updated once clients with
     * struts are adopted via 'desktop_update_workarea' */
    desktop->workarea = desktop->geometry;

    /* Mark desktop as outdated to trigger initial render */
    desktop->is_outdated = true;
    desktop->focus_dirty = true;
    desktop->is_urgent = false;

    LOGGER_TRACE("Initialized desktop %u ('%s') on screen %u" \
            " with geometry %ux%u",
            desktop_id, desktop->name, screen_id,
            desktop->geometry.dim.w, desktop->geometry.dim.h);

    return desktop;
}


/* Recompute work area from client struts */
void desktop_update_workarea(desktop_td *desktop,
        const surface_td *surface,
        const struct config_desktop_s *config_desktop,
        const struct strut_partial_s *systray_strut,
        bool ignore_struts)
{
    struct dimensions_s screen_dim;

    if (desktop == NULL || surface == NULL) {
        return;
    }

    screen_dim = surface->properties.dim;

    desktop->workarea = s_desktop_compute_workarea(desktop,
            0, 0, screen_dim.w, screen_dim.h,
            true, true, true, true,
            config_desktop, systray_strut, ignore_struts);

    LOGGER_TRACE("Desktop %u workarea: %ux%u%+d%+d",
            desktop->id,
            desktop->workarea.dim.w, desktop->workarea.dim.h,
            desktop->workarea.pos.x, desktop->workarea.pos.y);

    desktop->monitor_workarea_count = surface->monitor_count;
    for (uint32_t m = 0u; m < surface->monitor_count; ++m) {
        const monitor_td *mon = &surface->monitors[m];
        bool at_left = (mon->x == 0);
        bool at_top = (mon->y == 0);
        bool at_right = (mon->x + (int32_t) mon->w ==
                (int32_t) screen_dim.w);
        bool at_bottom = (mon->y + (int32_t) mon->h ==
                (int32_t) screen_dim.h);

        desktop->monitor_workareas[m] = s_desktop_compute_workarea(
                desktop, mon->x, mon->y, mon->w, mon->h,
                at_left, at_right, at_top, at_bottom,
                config_desktop, systray_strut, ignore_struts);

        LOGGER_TRACE("Desktop %u monitor %u workarea:" \
                " %ux%u%+d%+d",
                desktop->id, m,
                desktop->monitor_workareas[m].dim.w,
                desktop->monitor_workareas[m].dim.h,
                desktop->monitor_workareas[m].pos.x,
                desktop->monitor_workareas[m].pos.y);
    }
}


/* Free memory for allocated desktop */
void desktop_destroy(desktop_td *desktop)
{
    if (desktop == NULL) {
        return;
    }

    LOGGER_DEBUG("Destroying desktop %u ('%s')",
            desktop->id, desktop->name);

    /* Destroy stacking list (clients not destroyed here, just the list) */
    LOGGER_TRACE("Deallocating stacking list on desktop %u ('%s')",
            desktop->id, desktop->name);
    if (desktop->stacking != NULL) {
        cdlist_destroy(desktop->stacking);
        desktop->stacking = NULL;
    }

    /* Destroy hash table (also destroys all clients via client_destroy
     * callback) */
    LOGGER_TRACE("Deallocating clients on desktop %u ('%s')",
            desktop->id, desktop->name);
    if (desktop->clients != NULL) {
        ohtbl_destroy(desktop->clients);
        desktop->clients = NULL;
    }

    /* Free background image path if it exists */
    if (desktop->background.is_image &&
            desktop->background.bg.image_path != NULL) {
        LOGGER_TRACE("Deallocating image on desktop %u ('%s')",
                desktop->id, desktop->name);
        free(desktop->background.bg.image_path);
        desktop->background.bg.image_path = NULL;
    }

    LOGGER_TRACE("Destroying desktop %u ('%s')",
            desktop->id, desktop->name);
    free(desktop);
}


/* Mark a desktop and every one of its own clients as outdated */
void desktop_mark_outdated(desktop_td *desktop)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;

    if (desktop == NULL) {
        return;
    }

    desktop->is_outdated = true;

    if (desktop->stacking == NULL) {
        return;
    }

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return;
    }

    initial = node;
    do {
        client_td *const client = (client_td *) cdlist_data(node);

        if (client != NULL) {
            client->is_outdated = true;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);
}
