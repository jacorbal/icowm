/**
 * @file utils/xcb/atom.h
 *
 * @brief X atom lookup by name, and a couple of small property writes
 *        shared across more than one call site
 *
 * Every call site across the project that needs one X atom by its
 * string name (an EWMH/ICCCM property this window manager itself reads
 * or writes, or one it merely checks for) used to reimplement
 * this exact request/reply/free sequence on its own.
 *
 * @defgroup utils_xcb XCB protocol helpers
 * @ingroup utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_XCB_ATOM_H
#define UTILS_XCB_ATOM_H


/* System includes */
#include <stdbool.h>
#include <stddef.h>     /* size_t */

/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Intern an X atom by its string name
 *
 * Cached internally by (@p name, @p only_if_exists): a name already
 * resolved once returns instantly from that cache on every later call,
 * with no X server round trip at all, since the small, fixed set of
 * well-known EWMH/ICCCM atom names this project ever interns never
 * changes meaning for the lifetime of the connection.
 *
 * @param connection      XCB connection
 * @param name            Null-terminated atom name
 * @param only_if_exists  @c true to only look the atom up, never
 *                        creating it server-side if no client has
 *                        interned it yet (the right choice for checking
 *                        whether some other, possibly absent, tool
 *                        already set a property); @c false to create it
 *                        if necessary (the right choice for
 *                        a well-known EWMH/ICCCM atom this window
 *                        manager itself relies on existing)
 *
 * @return The interned atom, or @c XCB_ATOM_NONE if @p connection or
 *         @p name is @c NULL, or the request itself failed
 *
 * @note Complexity: @e O(1) on a cache hit; otherwise @e O(1) plus a
 *       single round trip to the X server to resolve and cache it
 */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists);


/**
 * @brief Resolve an X atom to its string name
 *
 * The inverse of @a atom_intern, i.e, given an atom, recovers the
 * null-terminated string it was interned from (e.g., an
 * @c xcb_randr_monitor_info_t's own @p name field, an X atom rather
 * than a plain string, to match a RandR output's real name such as
 * "HDMI-1" against a configured profile).
 *
 * @param connection    XCB connection
 * @param atom          Atom to resolve
 * @param out_name      Receives the resolved name, always
 *                      null-terminated; left as an empty string on any
 *                      failure
 * @param out_name_size Size of @p out_name in bytes; the name is
 *                      truncated to fit if longer
 *
 * @return Status of the operation
 * @retval  true if @p atom was resolved
 * @retval false if @p connection or @p out_name is @c NULL,
 *               @p out_name_size is @c 0, @p atom is @c XCB_ATOM_NONE,
 *               or the request itself failed
 *
 * @note Complexity: @e O(1), a single round trip to the X server
 */
bool atom_name(xcb_connection_t *connection, xcb_atom_t atom,
        char *out_name, size_t out_name_size);


/**
 * @brief Publish @c _NET_WM_WINDOW_OPACITY on a window
 *
 * @param connection XCB connection
 * @param window     Window to publish the property on
 * @param raw        The already-converted 32-bit value to publish
 *
 * @note This window manager never composites anything itself, so the
 *       value only has any visible effect once a compositing manager
 *       reads it back off the window and acts on it
 * @note Complexity: @e O(1)
 *
 * @see @a config_theme_opacity_to_raw in @c config.h, for converting
 *      a theme's own 0 to 100 percentage into this
 */
void atom_set_window_opacity(xcb_connection_t *connection,
        xcb_window_t window, uint32_t raw);


#endif  /* ! UTILS_XCB_ATOM_H */
