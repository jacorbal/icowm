/**
 * @file lookup.h
 *
 * @brief Window, client, surface, and desktop lookup API
 *
 * Pure-lookup functions that search the managed window hierarchy
 * without modifying any state.  No module at a lower layer may depend
 * on this header; all higher-layer modules that need a lookup must
 * include it directly.
 *
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef LOOKUP_H
#define LOOKUP_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>


/* Public interface */
/**
 * @brief Find the surface whose root window matches @p root
 *
 * @param surfaces Singly-linked list of @c surface_td pointers
 * @param root     Root window ID to search for
 *
 * @return Pointer to the matching surface, or @c NULL if not found
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
surface_td *lookup_surface_for_root(list_td *surfaces,
        xcb_window_t root);

/**
 * @brief Return the currently active desktop for a surface
 *
 * @param surface Pointer to the surface
 *
 * @return Pointer to the current desktop, or @c NULL on error
 *
 * @note Complexity: @e O(1)
 */
desktop_td *lookup_current_desktop(surface_td *surface);

/**
 * @brief Search all surfaces and desktops for a client by window ID
 *
 * @param surfaces    Singly-linked list of @c surface_td pointers
 * @param window      X window ID to search for
 * @param out_surface If non-null, receives the owning surface pointer
 * @param out_desktop If non-null, receives the owning desktop pointer
 *
 * @return Pointer to the client, or @c NULL if not found
 *
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e c the
 *       hash-table lookup cost per desktop
 */
client_td *lookup_find_client(list_td *surfaces, xcb_window_t window,
        surface_td **out_surface, desktop_td **out_desktop);


#endif  /* ! LOOKUP_H */
