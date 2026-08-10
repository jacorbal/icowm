/**
 * @file utils/xcb/atom.h
 *
 * @brief Single-atom interning by name, with a clean @c XCB_ATOM_NONE
 *        on any failure
 *
 * Every call site across the project that needs one X atom by its
 * string name (an EWMH/ICCCM property this window manager itself
 * reads or writes, or one it merely checks for) used to reimplement
 * this exact request/reply/free sequence on its own; this is the one
 * shared place for it now.
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
 * @param connection      XCB connection
 * @param name            Null-terminated atom name
 * @param only_if_exists  @c true to only look the atom up, never
 *                        creating it server-side if no client has
 *                        interned it yet (the right choice for
 *                        checking whether some other, possibly
 *                        absent, tool already set a property); @c
 *                        false to create it if necessary (the right
 *                        choice for a well-known EWMH/ICCCM atom this
 *                        window manager itself relies on existing)
 *
 * @return The interned atom, or @c XCB_ATOM_NONE if @p connection or
 *         @p name is @c NULL, or the request itself failed
 *
 * @note Complexity: @e O(1), a single round trip to the X server
 */
xcb_atom_t atom_intern(xcb_connection_t *connection, const char *name,
        bool only_if_exists);


/**
 * @brief Resolve an X atom to its string name
 *
 * The inverse of @c atom_intern: given an atom, recovers the
 * null-terminated string it was interned from (e.g., an
 * @c xcb_randr_monitor_info_t's own @c name field, an X atom rather
 * than a plain string, to match a RandR output's real name such as
 * @c "HDMI-1" against a configured profile).
 *
 * @param connection    XCB connection
 * @param atom          Atom to resolve
 * @param out_name      Receives the resolved name, always
 *                       null-terminated; left as an empty string on
 *                       any failure
 * @param out_name_size Size of @p out_name in bytes; the name is
 *                       truncated to fit if longer
 *
 * @return @c true if @p atom was resolved, @c false if @p connection
 *         or @p out_name is @c NULL, @p out_name_size is @c 0, @p atom
 *         is @c XCB_ATOM_NONE, or the request itself failed
 *
 * @note Complexity: @e O(1), a single round trip to the X server
 */
bool atom_name(xcb_connection_t *connection, xcb_atom_t atom,
        char *out_name, size_t out_name_size);


#endif  /* ! UTILS_XCB_ATOM_H */
