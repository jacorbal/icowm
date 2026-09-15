/**
 * @file surface/desktop.h
 *
 * @brief Desktop management for a surface: add, remove, look up,
 *        navigate, select, label, walk, and count
 *
 * Split out of @c surface.h, alongside its sibling @c surface
 * headers, so a file that only needs desktop management does not
 * also pull in, and rebuild against, every other unrelated surface
 * concern (viewport, monitor, workarea, action, client) declared in
 * the same file.
 *
 * @see @p surface_s, in @c surface.h
 *
 * @defgroup surface_desktop Surface desktop management
 * @ingroup surface
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SURFACE_DESKTOP_H
#define SURFACE_DESKTOP_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>     /* size_t */

/* Project includes */
#include <surface.h>
#include <desktop.h>


/**
 * @brief Add a new desktop to the list
 *
 * @param surface Pointer to the surface structure
 * @param desktop Pointer to the desktop to be added
 *
 * @return Status of the adding operation
 * @retval  0 Success
 * @retval  1 Failed to insert the desktop to the list
 * @retval -1 Invalid surface
 *
 * @note Complexity: @e O(1)
 */
int surface_desktop_add(surface_td *surface, desktop_td *desktop);

/**
 * @brief Remove a desktop from the list by its ID
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the desktop to be removed
 *
 * @return Status of the removal operation
 * @retval  0 Success
 * @retval  1 Failed to remove the desktop from the list
 * @retval  2 Could not find the desktop matching that ID
 * @retval -1 Invalid surface or no desktops
 *
 * @note Complexity: @e O(d), where @e d is @p surface's desktop count
 */
int surface_desktop_rem(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Get a desktop from the list by its ID
 *
 * Retrieves a pointer to a desktop with the specified ID from the
 * surface.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the desktop to retrieve
 *
 * @return Pointer to the desktop if found, or @c NULL otherwise
 */
desktop_td *surface_desktop_get(surface_td *surface,
        uint32_t desktop_id);

/**
 * @brief Compose the label naming the desktop a client is on
 *
 * The one place that decides how a desktop is named to the user, so
 * that everything showing one says it the same way.  Three cases, and
 * each leaves out what would not help:
 *
 * - a client pinned across every desktop names none of them;
 * - a layout of more than one row gives the desktop's ID and its row
 *   and column, the grid being what makes a coordinate mean something;
 * - anything else gives the ID alone, a single row's coordinate saying
 *   no more than the ID already does.
 *
 * A session with only one desktop is not a case here.  The caller that
 * can meet one, the search menu, leaves the label out entirely rather
 * than naming the only desktop there is.
 *
 * @param surface      Surface the desktop belongs to
 * @param desktop_id   Desktop to name
 * @param desktop_name Its name, or @c NULL when it has none
 * @param is_pinned    Whether the client is on every desktop
 * @param shows_name   Whether to append @p desktop_name, which the
 *                     overlay wants and a list of windows does not:
 *                     there the window's title identifies the entry,
 *                     and the name would take width from it
 * @param out_label    Receives the label; emptied when it cannot be
 *                     made
 * @param length       Size of @p out_label, in bytes
 *
 * @note @p shows_name does not apply to a pinned client
 * @note There is no one desktop to name, so there is no name to append
 *       either
 * @note Complexity: @e O(1)
 */
void surface_desktop_label(const surface_td *surface,
        uint32_t desktop_id, const char *desktop_name, bool is_pinned,
        bool shows_name, char *out_label, size_t length);

/**
 * @brief Get the desktop toward the north in the configured layout,
 *        optionally cycling
 *
 * A no-op search (always @c NULL, cycling or not) on a surface with no
 * @c topology.screens.desktops layout configured, the same as on any
 * single-row layout: there is no "north" to find at all when every
 * desktop already sits in the one and only row.  Skips past any
 * desktop-less gap cell a configured layout's @c rows @c *
 * @c columns may legitimately exceed the real desktop count with,
 * rather than landing on one.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, wraps to the bottom row of the same
 *                   column when already at the top
 *
 * @return Pointer to the desktop toward the north, or @c NULL if none
 *         exists in that direction
 *
 * @note Complexity: @e O(m), where @e m is @p layout's
 *       @c rows @c * @c columns
 *
 * @see @c ci_config_load_screens's comment, @c config/base/desktops.c
 */
desktop_td *surface_desktop_north(surface_td *surface,
        uint32_t desktop_id, bool cycle);

/**
 * @brief Get the desktop toward the south in the configured layout,
 *        optionally cycling
 *
 * A no-op search (always @c NULL, cycling or not) on a surface with no
 * @c topology.screens.desktops layout configured, the same as on any
 * single-row layout: there is no "south" to find at all when every
 * desktop already sits in the one and only row.  Skips past any
 * desktop-less gap cell the same way @a surface_desktop_north does.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, wraps to the top row of the same column
 *                   when already at the bottom
 *
 * @note Complexity: @e O(m), where @e m is @p layout's
 *       @c rows @c * @c columns
 *
 * @return Pointer to the desktop toward the south, or @c NULL if none
 *         exists in that direction
 */
desktop_td *surface_desktop_south(surface_td *surface,
        uint32_t desktop_id, bool cycle);

/**
 * @brief Get the desktop toward the west in the configured layout,
 *        optionally cycling
 *
 * Searches for the desktop with the given ID and returns the one toward
 * the west of it: list-previous on a surface with no
 * @c topology.screens.desktops layout configured (the common case,
 * still the default), the desktop one cell west along the configured
 * grid otherwise, whatever @c desktop_id that cell's
 * @c orientation/@c corner combination happens to hold (never simply
 * "@c desktop_id @c - @c 1": once @c corner is anything other than
 * top-left, a lower ID can sit visually east of a higher one, not west;
 * see @a s_layout_row_col's comment, @c surface/desktops.c, for the
 * full reasoning), skipping past any desktop-less gap cell along the
 * way.  If the current desktop is the westmost in its row, and cycling
 * is enabled, wraps to the eastmost desktop in that same row.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, wraps to the eastmost desktop in the
 *                   same row when already at the westmost
 *
 * @note Complexity: @e O(m), where @e m is @p layout's
 *       @c rows @c * @c columns
 *
 * @return Pointer to the desktop toward the west, or @c NULL if not
 *         found or not valid
 */
desktop_td *surface_desktop_west(surface_td *surface,
        uint32_t desktop_id, bool cycle);

/**
 * @brief Get the desktop toward the east in the configured layout,
 *        optionally cycling
 *
 * Searches for the desktop with the given ID and returns the one toward
 * the east of it: list-next on a surface with no
 * @c topology.screens.desktops layout configured (the common case,
 * still the default), the desktop one cell east along the configured
 * grid otherwise (see @a surface_desktop_west's doc comment for the
 * fuller reasoning on why this is not simply @c (desktop_id + 1),
 * skipping past any desktop-less gap cell along the way.  If the
 * current desktop is the eastmost in its row, and cycling is enabled,
 * wraps to the westmost desktop in that same row.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, wraps to the westmost desktop in the
 *                   same row when already at the eastmost
 *
 * @note Complexity: @e O(m), where @e m is @p layout's
 *       @c rows @c * @c columns
 *
 * @return Pointer to the desktop toward the east, or @c NULL if not
 *         found or not valid
 */
desktop_td *surface_desktop_east(surface_td *surface,
        uint32_t desktop_id, bool cycle);

/**
 * @brief Select the desktop toward the north, optionally cycling
 *
 * Attempts to select the desktop toward the north of the current one
 * (see @a surface_desktop_north's comment).  Updates @p desktop_cur
 * only on success.
 *
 * @param surface Pointer to the surface structure
 * @param cycle   If @c true, wraps to the bottom row of the same column
 *                when already at the top
 *
 * @return Status of the selection
 * @retval  0 Success
 * @retval  1 No desktop to the north found
 * @retval -1 Invalid surface or no desktops
 *
 * @note Complexity: @e O(m), where @e m is @p layout's
 *       @c rows @c * @c columns
 */
int surface_desktop_select_north(surface_td *surface, bool cycle);

/**
 * @brief Select the desktop toward the south, optionally cycling
 *
 * Attempts to select the desktop toward the south of the current one
 * (see @a surface_desktop_south's comment).  Updates @p desktop_cur
 * only on success.
 *
 * @param surface Pointer to the surface structure
 * @param cycle   If @c true, wraps to the top row of the same column
 *                when already at the bottom
 *
 * @return Status of the selection
 * @retval  0 Success
 * @retval  1 No desktop to the south found
 * @retval -1 Invalid surface or no desktops
 *
 * @note Complexity: @e O(m), where @e m is @p layout's
 *       @c rows @c * @c columns
 */
int surface_desktop_select_south(surface_td *surface, bool cycle);

/**
 * @brief Select the desktop toward the west (list-previous), optionally
 *        cycling
 *
 * Attempts to select the desktop toward the west of the current one
 * (see @a surface_desktop_west's comment for what "west" means with and
 * without a configured layout).  If the current desktop is the first
 * and cycle mode is enabled, it will select the last desktop, updating
 * @p desktop_cur.
 *
 * @param surface Pointer to the surface structure
 * @param cycle   If @c true, will cycle to the last desktop if the
 *                current is the first
 *
 * @return Status of the selection
 * @retval  0 Success
 * @retval  1 No desktop to the west found
 * @retval -1 Invalid surface or no desktops
 *
 * @note Complexity: @e O(m), where @e m is @p layout's
 *       @c rows @c * @c columns
 */
int surface_desktop_select_west(surface_td *surface, bool cycle);

/**
 * @brief Select the desktop toward the east (list-next), optionally
 *        cycling
 *
 * Attempts to select the desktop toward the east of the current one
 * (see @a surface_desktop_east's comment for what "east" means with and
 * without a configured layout).  If the current desktop is the last and
 * cycle mode is enabled, it will select the first desktop, updating
 * @p desktop_cur.
 *
 * @param surface Pointer to the surface structure
 * @param cycle   If @c true, will cycle to the first desktop if the
 *                current is the last
 *
 * @return Status of the selection
 * @retval  0 Success
 * @retval  1 No desktop to the east found
 * @retval -1 Invalid surface or no desktops
 *
 * @note Complexity: @e O(m), where @e m is @p layout's
 *       @c rows @c * @c columns
 */
int surface_desktop_select_east(surface_td *surface, bool cycle);

/**
 * @brief Select a specific desktop by its ID
 *
 * Selects a desktop by its ID.  If the ID is valid, it changes the
 * current desktop to the specified ID updating @p desktop_cur.  Returns
 * an error and updates nothing if the ID is not found in the list of
 * desktops.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the desktop to select
 *
 * @return Status of the selection
 * @retval  0 Success
 * @retval  1 No next desktop found
 * @retval -1 Invalid surface or no desktops
 *
 * @note Complexity: @e O(d), where @e d is @p surface's desktop count
 */
int surface_desktop_select(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Visit every desktop a surface holds, in order
 *
 * What a caller wanting all of them asks for, rather than walking the
 * list itself: how a surface keeps its desktops is its business, and
 * a caller that only wanted to count them or read their names had to
 * know it was a circular list to find out.
 *
 * @param surface Surface whose desktops to visit; may be @c NULL
 * @param visit   Called once per desktop; may be @c NULL
 * @param data    Handed to @p visit untouched
 *
 * @note The whole list is visited: no visitor can end the walk early,
 *       which is why one that stops on a condition records that in its
 *       own @p data and ignores what follows
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       @p surface
 */
void surface_desktop_walk_all(const surface_td *surface,
        surface_desktop_visitor_fn visit, void *data);

/**
 * @brief How many desktops a surface holds
 *
 * @param surface Surface to ask; may be @c NULL
 *
 * @return That count, or @c 0
 *
 * @note Complexity: @e O(1)
 */
uint32_t surface_desktop_count(const surface_td *surface);


#endif /* SURFACE_DESKTOP_H */
