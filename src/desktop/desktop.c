/**
 * @file desktop/desktop.c
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* strncpy */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/ohtbl.h>  /* Open-addressed hash table (closed hashing) */

/* Utils includes */
#include <utils/hash/murmurhash.h>
#include <utils/safe/safestr.h>

/* Project includes */
#include <client.h>
#include <logger.h>

/* Local includes */
#include <desktop.h>


/**
 * @brief Check whether two inclusive integer ranges overlap
 *
 * Tests whether the range defined by @p a_start and @p a_end intersects
 * the range defined by @p b_start and @p b_end, treating both endpoints
 * as inclusive. If the first range is given as @c 0..0, it is treated as
 * unbounded so legacy @c _NET_WM_STRUT values without explicit start/end
 * coordinates still match any target range. Invalid second ranges, where
 * @p b_end is less than @p b_start, are rejected.
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
/* Check if two inclusive integer ranges overlap */
static bool s_ranges_overlap(int32_t a_start, int32_t a_end,
        int32_t b_start, int32_t b_end)
{
    int32_t tmp;

    if (b_end < b_start) {
        return false;
    }

    /* Legacy '_NET_WM_STRUT' has no start/end fields; treat 0..0 as
     * unbounded so those struts still reserve space. */
    if (a_start == 0 && a_end == 0) {
        return true;
    }

    if (a_start > a_end) {
        tmp = a_start;
        a_start = a_end;
        a_end = tmp;
    }

    return !(a_end < b_start || a_start > b_end);
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
    return (hash2 == 0u) ? 1u : hash2;
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

    return client1->id == client2->id;
}


/* Initialize a new desktop */
desktop_td *desktop_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        uint32_t screen_id, uint32_t desktop_id,
        struct config_base_s *config_base,
        struct config_theme_s *config_theme)
{
    desktop_td *desktop;
    xcb_screen_t *screen;
    xcb_screen_iterator_t iter;

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
    desktop->config_base = config_base;
    desktop->config_theme = config_theme;

    /* Set desktop name.  The config-provided name is copied with
     * 'safe_strncpy' instead of 'snprintf("%s", ...)' because its
     * source field is wider than 'desktop->name'*/
    /* GCC's option '-Wformat-truncation' cannot prove the copy never
     * truncates, and truncating a name that does not fit is the
     * desired, harmless behavior here anyway. */
    if (config_base->screens[screen_id].desktops[desktop_id].name[0] == '\0') {
        snprintf(desktop->name, WM_DESKTOP_MAX_LENGTH_NAME,
                "Desktop %u", desktop_id);
    } else {
        safe_strncpy(desktop->name,
                config_base->screens[screen_id].desktops[desktop_id].name,
                WM_DESKTOP_MAX_LENGTH_NAME);
    }

    /* Set background color */
    desktop->background.is_image = false;
    desktop->background.use_root_pixmap = false;
    desktop->background.bg.color =
        config_base->screens[screen_id].desktops[desktop_id].settings.background.color;

    LOGGER_TRACE("Initializing client list structure for" \
            " desktop %u ('%s') on screen %u",
            desktop_id, desktop->name, screen_id);

    /* Initialize hash table for quick client lookup */
    desktop->clients =
        ohtbl_init(WM_DESKTOP_INITIAL_CAPACITY, 0,
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

    LOGGER_TRACE("Initialized desktop %u ('%s') on screen %u" \
            " with geometry %ux%u",
            desktop_id, desktop->name, screen_id,
            desktop->geometry.dim.w, desktop->geometry.dim.h);

    return desktop;
}


/* Recompute work area from client struts */
void desktop_update_workarea(desktop_td *desktop,
        uint32_t screen_w, uint32_t screen_h)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    int32_t left = 0;
    int32_t right = 0;
    int32_t top = 0;
    int32_t bottom = 0;
    int32_t new_w;
    int32_t new_h;
    int32_t screen_max_x;
    int32_t screen_max_y;

    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        if (desktop != NULL) {
            desktop->workarea.pos.x = 0;
            desktop->workarea.pos.y = 0;
            desktop->workarea.dim.w = screen_w;
            desktop->workarea.dim.h = screen_h;
        }

        return;
    }

    screen_max_x = (screen_w == 0u) ? -1 : (int32_t) (screen_w - 1u);
    screen_max_y = (screen_h == 0u) ? -1 : (int32_t) (screen_h - 1u);

    /* Aggregate maximum strut on each edge across all stacked clients */
    initial = cdlist_head(desktop->stacking);
    node = initial;
    do {
        client_td *c = (client_td *) cdlist_data(node);

        if (c != NULL) {
            if (c->layout.strut_partial.sides.left > left &&
                    s_ranges_overlap(
                        c->layout.strut_partial.start.left,
                        c->layout.strut_partial.end.left,
                        0, screen_max_y)) {
                left = c->layout.strut_partial.sides.left;
            }

            if (c->layout.strut_partial.sides.right > right &&
                    s_ranges_overlap(
                        c->layout.strut_partial.start.right,
                        c->layout.strut_partial.end.right,
                        0, screen_max_y)) {
                right = c->layout.strut_partial.sides.right;
            }

            if (c->layout.strut_partial.sides.top > top &&
                    s_ranges_overlap(
                        c->layout.strut_partial.start.top,
                        c->layout.strut_partial.end.top,
                        0, screen_max_x)) {
                top = c->layout.strut_partial.sides.top;
            }

            if (c->layout.strut_partial.sides.bottom > bottom &&
                    s_ranges_overlap(
                        c->layout.strut_partial.start.bottom,
                        c->layout.strut_partial.end.bottom,
                        0, screen_max_x)) {
                bottom = c->layout.strut_partial.sides.bottom;
            }
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    new_w = (int32_t) screen_w - left - right;
    new_h = (int32_t) screen_h - top  - bottom;

    desktop->workarea.pos.x = left;
    desktop->workarea.pos.y = top;
    desktop->workarea.dim.w = (new_w > 0) ? (uint32_t) new_w : 0U;
    desktop->workarea.dim.h = (new_h > 0) ? (uint32_t) new_h : 0U;

    LOGGER_TRACE("Desktop %u workarea: %ux%u+%d+%d",
            desktop->id,
            desktop->workarea.dim.w, desktop->workarea.dim.h,
            desktop->workarea.pos.x, desktop->workarea.pos.y);
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


/* Soft desktop update */
void desktop_update(desktop_td *desktop)
{
//    LOGGER_TRACE("Updating desktop %u ('%s')",
//            desktop->id, desktop->name);

    /* Establish that this desktop is already updated */
    desktop->is_outdated = false;
}


/* Full desktop update */
void desktop_update_full(desktop_td *desktop)
{
    void *elem;

    LOGGER_TRACE("Fully updating desktop %u ('%s')",
            desktop->id, desktop->name);

    /* Soft update */
    desktop_update(desktop);

    /* Update all clients on the hash table */
    ohtbl_foreach(desktop->clients, elem) {
        client_update((client_td *) elem);
    }

    LOGGER_TRACE("Updated desktop %u ('%s')",
            desktop->id, desktop->name);
}


/* Clear a desktop by removing all its clients */
void desktop_clear(desktop_td *desktop)
{
    client_td *client;

    if (desktop == NULL) {
        return;
    }

    LOGGER_DEBUG("Preparing to clear desktop %u ('%s')",
            desktop->id, desktop->name);

    if (desktop->stacking != NULL) {
        while (true) {
            client = NULL;
            if (cdlist_rem_next(desktop->stacking, NULL,
                        (void **) &client) != 0) {
                break;
            }
            /* The list may legitimately contain null data pointers;
             * destroy only valid clients. */
            if (client != NULL) {
                client_destroy(client);
            }
        }
    }

    if (desktop->clients != NULL) {
        ohtbl_reset(desktop->clients);
    }
}


/* Rename the desktop */
int desktop_action_rename(desktop_td *desktop, const char *name)
{
    LOGGER_DEBUG("Renaming desktop %u ('%s') to '%s'",
            desktop->id, desktop->name, name);
    if (desktop == NULL || name == NULL) {
        LOGGER_ERROR("Invalid desktop or name pointer", L_NARG);
        return -1;
    }

    snprintf(desktop->name, WM_DESKTOP_MAX_LENGTH_NAME, "%s", name);
    desktop->name[WM_DESKTOP_MAX_LENGTH_NAME - 1] = '\0';
    desktop->is_outdated = true;

    return 0;
}


/* Update the desktop background color */
int desktop_action_background_update(desktop_td *desktop, uint32_t color)
{
    LOGGER_DEBUG("Updating background color of desktop %u ('%s')"
            " to 0x%08x", desktop->id, desktop->name, color);

    if (desktop == NULL) {
        return -1;
    }

    desktop->background.is_image = false;
    desktop->background.use_root_pixmap = false;
    desktop->background.bg.color = color;
    desktop->is_outdated = true;

    return 0;
}
