/**
 * @file wm/ewmh.h
 *
 * @brief Root-window EWMH properties this window manager publishes
 *
 * What a compliant panel, pager or taskbar reads to learn what this
 * window manager is and what it supports.  Held apart from the rest of
 * @c wm.h because it is one protocol rather than one more thing the
 * window manager happens to do, and because everything on the other
 * side of it belongs to somebody else's program.
 *
 * Only the root window's properties are here.  What a client publishes
 * about itself goes through @c cmds/client/ewmh.h, and what a client
 * asks of the window manager arrives at @c handler/ewmh.h.
 *
 * @defgroup wmewmh Root EWMH properties
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_EWMH_H
#define WM_EWMH_H


/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Publish the EWMH metadata a compliant desktop reads
 *
 * Creates the supporting window and publishes @c _NET_SUPPORTED and
 * @c _NET_SUPPORTING_WM_CHECK on it.
 *
 * @param wm Window manager instance
 *
 * @return @c 0 on success, or non-zero on failure
 *
 * @note No matching teardown, this allocating nothing
 * @note It publishes properties on the root window, and a property is
 *       not freed but ceases to exist with the window carrying it
 * @note The EWMH connection those properties are written through is
 *       another matter, and is freed by @a wm_stop
 * @note Complexity: @e O(1)
 */
int wm_ewmh_init(const wm_td *wm);

/**
 * @brief Recompute and publish EWMH root properties
 *
 * Synchronizes core EWMH metadata for each managed screen, including
 * desktop counts, current desktop, workarea, client lists, and active
 * window.
 *
 * @param wm Window manager instance
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients across all desktops
 */
void wm_ewmh_sync(wm_td *wm);


#endif  /* ! WM_EWMH_H */
