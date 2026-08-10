/**
 * @file utils/xcb/selection.h
 *
 * @brief ICCCM manager-selection acquisition shared by every built-in
 *        selection-owning manager
 *
 * @ingroup utils_xcb
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_XCB_SELECTION_H
#define UTILS_XCB_SELECTION_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Acquire an ICCCM manager selection, announcing it on @p root
 *
 * Sets ownership of @p selection_atom on @p window, verifies the
 * server actually granted it (another already-running manager may
 * already hold it), and if so broadcasts the standard @c MANAGER
 * client message on @p root so other tools notice.
 *
 * @param connection     XCB connection
 * @param window         Window that should own the selection
 * @param selection_atom Manager-selection atom to acquire (e.g.
 *                       @c _NET_SYSTEM_TRAY_S0)
 * @param manager_atom   Interned @c MANAGER atom, used as the
 *                       broadcast client message's own type
 * @param root           Root window the @c MANAGER message is sent on
 *
 * @return @c true if ownership was acquired, @c false if another
 *         manager already owns the selection
 *
 * @note Complexity: @e O(1), two round trips
 */
bool util_xcb_acquire_manager_selection(xcb_connection_t *connection,
        xcb_window_t window, xcb_atom_t selection_atom,
        xcb_atom_t manager_atom, xcb_window_t root);


#endif  /* ! UTILS_XCB_SELECTION_H */
