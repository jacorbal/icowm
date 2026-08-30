/**
 * @file wm/startup/selection.h
 *
 * @brief Acquiring the ICCCM manager selection for every managed
 *        screen
 *
 * @ingroup startup
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_STARTUP_SELECTION_H
#define WM_STARTUP_SELECTION_H


/* System includes */
#include <stdbool.h>

/* Project includes */
#include <types/handles.h>


/**
 * @brief Acquire the @c WM_Sn manager selection on every managed
 *        screen, taking over an already-running window manager's
 *        ownership when asked to
 *
 * Per ICCCM §2.8, this checks, for each screen in turn, whether its
 * own @c WM_S<n> selection (@c n being the screen number) already has
 * an owner:
 *
 * - If it does not, the window this function creates for the purpose
 *   (shared across every screen, later reused by
 *   @a wm_startup_ewmh_init, @c wm/ewmhinit.c, as the
 *   @c _NET_SUPPORTING_WM_CHECK window) is made the new owner directly.
 * - If it does, and @p replace_requested is @c false, this fails
 *   outright: two window managers are not meant to coexist on the
 *   same screen.
 * - If it does, and @p replace_requested is @c true, this selects
 *   @c StructureNotify on the previous owner's window, takes
 *   ownership itself, and then waits, bounded by
 *   @c WM_SN_REPLACE_TIMEOUT_MS (@c defs/ewmh.h), for a
 *   @c DestroyNotify on that previous owner's window: per the same
 *   ICCCM section, a manager losing its selection must release
 *   every resource it managed and then destroy the window that owned
 *   it, in that order, so seeing it destroyed is how this knows the
 *   previous manager is genuinely done and it is now safe to proceed
 *   to @c SubstructureRedirect.
 *
 * On success, also sends the @c MANAGER @c ClientMessage announcement
 * ICCCM §2.8 calls for, to any other client watching for one.
 *
 * @param wm                Window manager state
 * @param replace_requested Whether to take over an already-running
 *                          window manager's ownership instead of
 *                          refusing to start against it (@c -r)
 *
 * @return 0 on success, -1 if @p wm or its members are null, another
 *         window manager already owns a screen's selection and
 *         @p replace_requested is @c false, or the previous owner did
 *         not relinquish it within @c WM_SN_REPLACE_TIMEOUT_MS
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       screens
 */
int wm_startup_acquire_selection(wm_td *wm, bool replace_requested);


#endif  /* ! WM_STARTUP_SELECTION_H */
