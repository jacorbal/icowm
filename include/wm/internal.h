/**
 * @file wm/internal.h
 *
 * @brief Private state and helpers shared across wm sub-modules
 *
 * Declares the singleton @c wm_td pointer that is defined in
 * @c wm.c, and helpers shared by more than one of @c wm/shutdown.c,
 * @c wm/ewmhinit.c, and @c wm/action.c.
 *
 * @note This header is private to the wm subsystem and must not be
 *       included outside of @c src/wm/, for it is NOT part of the
 *       public API
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_INTERNAL_H
#define WM_INTERNAL_H


/* Project includes */
#include <client.h>
#include <wm.h>


/**
 * @brief Singleton window manager instance
 *
 * @note Defined in @c wm.c
 * @note All wm sub-modules access it through this declaration
 */
extern wm_td *wm;

/**
 * @brief Visit every currently managed client across every surface
 *        and desktop, optionally applying an action to each
 *
 * Shared by @c wm/shutdown.c and @c wm/ewmhinit.c, so both walk the
 * exact same enumeration instead of each keeping its own separate
 * copy of this traversal.
 *
 * @param action   Called once per client found, with @p userdata
 *                 passed through unchanged, or @c NULL to only count
 *                 clients without acting on any
 * @param userdata Passed through to @p action on every call,
 *                 untouched otherwise
 *
 * @return Number of managed clients found
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       managed clients across every surface and desktop
 */
uint32_t wm_for_each_client(void (*action)(client_td *client, void *userdata),
        void *userdata);


#endif  /* ! WM_INTERNAL_H */
