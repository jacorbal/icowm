/**
 * @file surface/desktops.c
 *
 * @brief Desktop-list membership and grid navigation for a surface
 *
 * One of the files @c surface/ is made of; see
 * @c surface.c's comment for why.  @c _prev/@c _next are now
 * @c _west and @c _east, joined by @c _north and @c _south:
 * a flat desktop list has no genuine "previous" or "next" of
 * its, only a configured @c topology.screens.desktops layout's
 * own reading order does, and that same order runs one of two ways
 * depending on @c orientation, so a name tied to whichever axis a
 * one-row (or one-column) surface happens to default to would mean
 * the opposite of itself the moment a real 2-D layout is configured.
 * A compass direction has no such ambiguity: @c _west is the exact
 * same desktop @c _prev always was, @c _east the exact same as
 * @c _next, on every surface that never configures a layout at all
 * (the common case, still the default, @c _north/@c _south then
 * finding nothing at all to move to, the one direction that never
 * existed before this), and the same names keep meaning exactly what
 * they say, visually, once a layout is configured, regardless of
 * which corner @c desktop_id @c 0 itself starts counting from (see
 * @a s_layout_row_col's comment for why a naive "list-
 * previous"/"list-next" is not enough on its own to guarantee that
 * once @c corner is anything other than top-left).
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
#include <stddef.h>     /* NULL, size_t */
#include <stdint.h>
#include <stdio.h>      /* snprintf */

/* ADT includes */
#include <adt/cdlist.h>

/* Project includes */
#include <config.h>
#include <i18n.h>
#include <desktop.h>

/* Local includes */
#include <surface.h>
#include <defs/uistr.h>


/**
 * @brief Convert a flat desktop index into its row/column
 *        position within a configured layout
 *
 * The @c orientation and @c corner math is not evaluated as eight
 * separate cases, one per combination, the way Openbox's
 * equivalent (@c get_row_col, screen.c) does: every corner reduces to
 * the same top-left computation for whichever axis @c orientation
 * treats as primary, then a single, independent flip per axis
 * (mirroring @c row within @c rows, @c col within @c columns) for
 * whichever half of @c corner names that side, confirmed against
 * Openbox's eight-case version, index by index, across every
 * shape/orientation/corner combination before this replaced it.
 *
 * @param index    Flat desktop index; assumed to already fall within
 *                 @p layout's @c rows @c * @c columns extent
 * @param layout   Layout to interpret @p index against
 * @param row_out  Resulting row, updated in place
 * @param col_out  Resulting column, updated in place
 *
 * @note Complexity: @e O(1)
 */
static void s_layout_row_col(uint32_t index,
        const struct config_desktop_layout_s *layout,
        uint32_t *row_out, uint32_t *col_out)
{
    bool horz = (layout->orientation ==
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL);
    uint32_t primary = horz ? layout->columns : layout->rows;
    uint32_t r = horz ? index / primary : index % primary;
    uint32_t c = horz ? index % primary : index / primary;

    switch (layout->corner) {
    case CONFIG_DESKTOP_CORNER_TOP_LEFT:
        break;
    case CONFIG_DESKTOP_CORNER_TOP_RIGHT:
        c = layout->columns - 1u - c;
        break;
    case CONFIG_DESKTOP_CORNER_BOTTOM_LEFT:
        r = layout->rows - 1u - r;
        break;
    case CONFIG_DESKTOP_CORNER_BOTTOM_RIGHT:
        r = layout->rows - 1u - r;
        c = layout->columns - 1u - c;
        break;
    }

    *row_out = r;
    *col_out = c;
}


/**
 * @brief Convert a row/column position back into its flat
 *        desktop index within a configured layout
 *
 * The exact inverse of @a s_layout_row_col: the same per-axis corner
 * flip, applied to @p row/@p col before combining them, undoes
 * itself correctly since mirroring within a fixed extent twice is
 * always the identity.  @p row/@p col are taken as @c int64_t,
 * rather than @c uint32_t the way a valid position always ends up
 * being, specifically so a caller mid-step, namely
 * @a s_surface_desktop_direction below, can pass a tentative,
 * possibly negative
 * one-past-the-edge position straight through without checking for
 * unsigned underflow itself first; this function's bounds check
 * catches that either way.
 *
 * @param row      Row to convert; a null result if negative or
 *                 @c >= @p layout's @c rows
 * @param col      Column to convert; a null result if negative or
 *                 @c >= @p layout's @c columns
 * @param layout   Layout to interpret @p row/@p col against
 * @param index_out Resulting flat index, updated in place only on a
 *                 @c true return
 *
 * @return @c false if @p row/@p col falls outside @p layout's
 *         @c rows/@c columns extent at all
 *
 * @note Complexity: @e O(1)
 */
static bool s_layout_index(int64_t row, int64_t col,
        const struct config_desktop_layout_s *layout,
        uint32_t *index_out)
{
    bool horz;
    uint32_t r;
    uint32_t c;

    if (row < 0 || col < 0 || (uint32_t) row >= layout->rows ||
            (uint32_t) col >= layout->columns) {
        return false;
    }

    horz = (layout->orientation ==
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL);
    r = (uint32_t) row;
    c = (uint32_t) col;

    switch (layout->corner) {
    case CONFIG_DESKTOP_CORNER_TOP_LEFT:
        break;
    case CONFIG_DESKTOP_CORNER_TOP_RIGHT:
        c = layout->columns - 1u - c;
        break;
    case CONFIG_DESKTOP_CORNER_BOTTOM_LEFT:
        r = layout->rows - 1u - r;
        break;
    case CONFIG_DESKTOP_CORNER_BOTTOM_RIGHT:
        r = layout->rows - 1u - r;
        c = layout->columns - 1u - c;
        break;
    }

    *index_out = horz ? (r * layout->columns + c)
        : (c * layout->rows + r);
    return true;
}


/**
 * @brief One compass direction a desktop grid can be stepped in
 */
enum s_grid_direction_e {
    S_GRID_NORTH,
    S_GRID_SOUTH,
    S_GRID_EAST,
    S_GRID_WEST
};


/**
 * @brief Step from one desktop to its grid neighbor in a given
 *        compass direction, skipping past any desktop-less gap cell
 *        a configured layout's @c rows @c * @c columns may
 *        legitimately exceed the real desktop count with (see
 *        @c ci_config_load_screens's comment, config/base/
 *        desktops.c, for why a gap like that is accepted rather
 *        than rejected outright)
 *
 * Unlike Openbox's equivalent (@c screen_find_desktop, screen.c),
 * whose single, crude nudge forward on landing in a gap cell does
 * not reliably clear more than one gap cell in a row, this steps
 * again, in the same direction, for as long as landing in a gap cell
 * keeps happening, up to the grid's full cell count before
 * giving up: a genuinely bounded search, not a fixed one-step
 * allowance.
 *
 * @param surface    Surface whose configured layout to navigate
 * @param desktop_id Starting desktop's flat index
 * @param direction  Compass direction to step in
 * @param cycle      Whether stepping past the grid's edge wraps
 *                   around to the opposite edge on that same axis,
 *                   rather than stopping
 *
 * @return The neighboring desktop, or @c NULL if @p surface/its
 *         configuration is unavailable, or if no desktop exists in
 *         @p direction at all (either the grid's edge, not
 *         cycling, or every remaining cell that way is a gap)
 *
 * @note Complexity: @e O(m), where @e m is @p layout's @c rows
 *       @c * @c columns
 */
static desktop_td *s_surface_desktop_direction(surface_td *surface,
        uint32_t desktop_id, enum s_grid_direction_e direction,
        bool cycle)
{
    const struct config_desktop_layout_s *layout;
    uint32_t row;
    uint32_t col;
    int64_t dr;
    int64_t dc;
    uint32_t total_cells;

    if (surface == NULL || surface->config == NULL ||
            surface->id >= (uint32_t) CONFIG_MAX_SCREENS) {
        return NULL;
    }

    layout = &surface->config->base.screens[surface->id].desktop_layout;
    s_layout_row_col(desktop_id, layout, &row, &col);

    switch (direction) {
    case S_GRID_NORTH:
        dr = -1;
        dc = 0;
        break;
    case S_GRID_SOUTH:
        dr = 1;
        dc = 0;
        break;
    case S_GRID_EAST:
        dr = 0;
        dc = 1;
        break;
    case S_GRID_WEST:
        dr = 0;
        dc = -1;
        break;
    }

    total_cells = layout->rows * layout->columns;
    for (uint32_t step = 0u; step < total_cells; ++step) {
        int64_t next_row = (int64_t) row + dr;
        int64_t next_col = (int64_t) col + dc;
        uint32_t candidate;

        if (next_row < 0 || next_row >= (int64_t) layout->rows ||
                next_col < 0 || next_col >= (int64_t) layout->columns) {
            if (!cycle) {
                return NULL;
            }
            /* '((x % n) + n) % n', not a plain 'x % n': C's '%'
             * can return a negative result for a negative left-hand
             * side (e.g., '-1 % 2' is '-1', not '1'), which a raw
             * cast back to 'uint32_t' would turn into a huge,
             * genuinely wrong value rather than the intended
             * wraparound one. */
            next_row = ((next_row % (int64_t) layout->rows) +
                    (int64_t) layout->rows) % (int64_t) layout->rows;
            next_col = ((next_col % (int64_t) layout->columns) +
                    (int64_t) layout->columns) %
                (int64_t) layout->columns;
        }
        row = (uint32_t) next_row;
        col = (uint32_t) next_col;

        if (s_layout_index(row, col, layout, &candidate) &&
                candidate < surface->desktop_count) {
            return surface_desktop_get(surface, candidate);
        }
    }

    return NULL;
}


/* Get a desktop's row/column position in its surface's
 * configured layout */
bool surface_desktop_row_col(const surface_td *surface,
        uint32_t desktop_id, uint32_t *row_out, uint32_t *col_out)
{
    const struct config_desktop_layout_s *layout;

    if (surface == NULL || surface->config == NULL ||
            surface->id >= (uint32_t) CONFIG_MAX_SCREENS ||
            row_out == NULL || col_out == NULL) {
        return false;
    }

    layout = &surface->config->base.screens[surface->id].desktop_layout;
    s_layout_row_col(desktop_id, layout, row_out, col_out);
    return true;
}


/* Compose the label naming the desktop a client is on */
void surface_desktop_label(const surface_td *surface,
        uint32_t desktop_id, const char *desktop_name, bool is_pinned,
        bool shows_name, char *out_label, size_t length)
{
    uint32_t row = 0u;
    uint32_t col = 0u;
    bool has_row_col;
    bool shows_row_col;

    if (out_label == NULL || length == 0u) {
        return;
    }
    out_label[0] = '\0';

    if (surface == NULL) {
        return;
    }

    /* A pinned client is on every desktop, so there is no one desktop
     * to name and nothing for 'shows_name' to append either */
    if (is_pinned) {
        (void) snprintf(out_label, length, "%s",
                _(STR_SEARCH_ALL_DESKTOPS));
        return;
    }

    /* Only worth showing the coordinate once the grid is genuinely
     * more than the one row a desktop's ID already fully describes
     * on its own; see 'surface_desktop_row_col' above for what "row 0"
     * always means on a linear (or unconfigured) layout, the exact
     * case this excludes. */
    has_row_col = surface_desktop_row_col(surface, desktop_id,
            &row, &col);
    shows_row_col = has_row_col && surface->config != NULL &&
        surface->id < (uint32_t) CONFIG_MAX_SCREENS &&
        surface->config->base.screens[surface->id]
            .desktop_layout.rows > 1u;

    if (shows_name && desktop_name != NULL &&
            desktop_name[0] != '\0') {
        if (shows_row_col) {
            (void) snprintf(out_label, length, "[%u (%u, %u)] -- %s",
                    desktop_id, row, col, desktop_name);
        } else {
            (void) snprintf(out_label, length, "[%u] -- %s",
                    desktop_id, desktop_name);
        }
    } else if (shows_row_col) {
        (void) snprintf(out_label, length, "[%u (%u, %u)]",
                desktop_id, row, col);
    } else {
        (void) snprintf(out_label, length, "[%u]", desktop_id);
    }
}


/* Add a new desktop to the list */
int surface_desktop_add(surface_td *surface, desktop_td *desktop)
{
    if (surface == NULL || desktop == NULL) {
        return -1;
    }

    /* Add to the tail of the circular linked list */
    if (cdlist_ins_next(surface->desktops,
                cdlist_tail(surface->desktops), desktop) != 0) {
        /* Failed to add to the list */
        return 1;
    }

    /* Update the count of desktops */
    surface->desktop_count++;

    return 0;
}


/* Remove a desktop from the list by its ID */
int surface_desktop_rem(surface_td *surface, uint32_t desktop_id)
{
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktops == NULL ||
            surface->desktop_count == 0) {
        return -1;
    }

    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0;
            i < surface->desktop_count && current_item != NULL;
            ++i) {
        desktop_td *const desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            void *removed_desktop = NULL;
            /* Remove the desktop */
            if (cdlist_rem_next(surface->desktops,
                        cdlist_prev(current_item),
                        &removed_desktop) != 0) {
                /* Failed to remove from list */
                return 1;
            }
            desktop_destroy((desktop_td *) ((removed_desktop != NULL)
                        ? removed_desktop : (void *) desktop));

            /* Update the count of desktops */
            surface->desktop_count--;
            return 0;
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop not found */
    return 2;
}


/* Get a desktop from the list by its ID */
desktop_td *surface_desktop_get(surface_td *surface,
        uint32_t desktop_id)
{
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktops == NULL ||
            surface->desktop_count == 0) {
        return NULL;
    }

    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0; i < surface->desktop_count && current_item != NULL;
            ++i) {
        desktop_td *const desktop = (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            return desktop;
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop ID not found */
    return NULL;
}


/* Get the desktop toward the north in the configured layout,
 * optionally cycling */
desktop_td *surface_desktop_north(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    return s_surface_desktop_direction(surface, desktop_id,
            S_GRID_NORTH, cycle);
}


/* Get the desktop toward the south in the configured layout,
 * optionally cycling */
desktop_td *surface_desktop_south(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    return s_surface_desktop_direction(surface, desktop_id,
            S_GRID_SOUTH, cycle);
}


/* Get the desktop toward the west in the configured layout,
 * optionally cycling */
desktop_td *surface_desktop_west(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    return s_surface_desktop_direction(surface, desktop_id,
            S_GRID_WEST, cycle);
}


/* Get the desktop toward the east in the configured layout,
 * optionally cycling */
desktop_td *surface_desktop_east(surface_td *surface,
        uint32_t desktop_id, bool cycle)
{
    return s_surface_desktop_direction(surface, desktop_id,
            S_GRID_EAST, cycle);
}


/* Select the desktop toward the west (list-previous), optionally
 * cycling */
int surface_desktop_select_west(surface_td *surface, bool cycle)
{
    const desktop_td *west_desktop;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    west_desktop =
        surface_desktop_west(surface, surface->desktop_cur, cycle);
    if (west_desktop) {
        /* Update ID of new current desktop */
        surface->desktop_cur = west_desktop->id;
        return 0;
    }

    /* No desktop to the west found */
    return 1;
}


/* Select the desktop toward the east (list-next), optionally
 * cycling */
int surface_desktop_select_east(surface_td *surface, bool cycle)
{
    const desktop_td *east_desktop;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    east_desktop =
        surface_desktop_east(surface, surface->desktop_cur, cycle);
    if (east_desktop) {
        /* Update ID of new current desktop */
        surface->desktop_cur = east_desktop->id;
        return 0;
    }

    /* No desktop to the east found */
    return 1;
}


/* Select the desktop toward the north, optionally cycling */
int surface_desktop_select_north(surface_td *surface, bool cycle)
{
    const desktop_td *north_desktop;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    north_desktop =
        surface_desktop_north(surface, surface->desktop_cur, cycle);
    if (north_desktop) {
        /* Update ID of new current desktop */
        surface->desktop_cur = north_desktop->id;
        return 0;
    }

    /* No desktop to the north found */
    return 1;
}


/* Select the desktop toward the south, optionally cycling */
int surface_desktop_select_south(surface_td *surface, bool cycle)
{
    const desktop_td *south_desktop;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    south_desktop =
        surface_desktop_south(surface, surface->desktop_cur, cycle);
    if (south_desktop) {
        /* Update ID of new current desktop */
        surface->desktop_cur = south_desktop->id;
        return 0;
    }

    /* No desktop to the south found */
    return 1;
}


/* Select a specific desktop by ID */
int surface_desktop_select(surface_td *surface, uint32_t desktop_id)
{
    cdlist_item_td *current_item;

    if (surface == NULL || surface->desktop_count == 0) {
        return -1;
    }

    /* Iterate through the desktops list to check if ID is valid */
    current_item = cdlist_head(surface->desktops);
    for (size_t i = 0; i < surface->desktop_count; ++i) {
        const desktop_td *desktop =
            (desktop_td *) cdlist_data(current_item);
        if (desktop->id == desktop_id) {
            /* Update ID of new current desktop */
            surface->desktop_cur = desktop_id;
            return 0;
        }
        current_item = cdlist_next(current_item);
    }

    /* Desktop ID not found */
    return 1;
}


/* Visit every desktop a surface holds, in order */
void surface_desktops_walk(const surface_td *surface,
        surface_desktop_visitor_fn visit, void *data)
{
    cdlist_item_td *node;

    if (surface == NULL || surface->desktops == NULL || visit == NULL) {
        return;
    }

    cdlist_foreach(surface->desktops, node) {
        desktop_td *const desktop = (desktop_td *) cdlist_data(node);

        if (desktop != NULL) {
            visit(desktop, data);
        }
    }
}


/* How many desktops a surface holds */
uint32_t surface_desktop_count(const surface_td *surface)
{
    if (surface == NULL || surface->desktops == NULL) {
        return 0u;
    }

    return (uint32_t) cdlist_size(surface->desktops);
}
