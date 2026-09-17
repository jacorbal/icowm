/**
 * @file lookup.h
 *
 * @brief Window, client, stage, and desktop lookup API
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

/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Find the stage whose root window matches @p root
 *
 * @param stages Singly-linked list of @c stage_td pointers
 * @param root   Root window ID to search for
 *
 * @return Pointer to the matching stage, or @c NULL if not found
 *
 * @note Complexity: @e O(n), where @e n is the number of stages
 */
stage_td *lookup_stage_for_root(list_td *stages,
        xcb_window_t root);

/**
 * @brief Return the currently active desktop for a stage
 *
 * @param stage Pointer to the stage
 *
 * @return Pointer to the current desktop, or @c NULL on error
 *
 * @note Complexity: @e O(1)
 */
desktop_td *lookup_current_desktop(stage_td *stage);

/**
 * @brief Search all stages and desktops for a client by window ID
 *
 * @param stages      Singly-linked list of @c stage_td pointers
 * @param window      X window ID to search for
 * @param out_stage   If non-null, receives the owning stage pointer
 * @param out_desktop If non-null, receives the owning desktop pointer
 *
 * @return Pointer to the client, or @c NULL if not found
 *
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       stages, @e d the number of desktops per stage, and @e c the
 *       hash-table lookup cost per desktop
 */
client_td *lookup_find_client(list_td *stages, xcb_window_t window,
        stage_td **out_stage, desktop_td **out_desktop);


#endif  /* ! LOOKUP_H */
