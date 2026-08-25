/**
 * @file cctl/adopt.h
 *
 * @brief Window manager startup scan: adopting pre-existing windows
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CCTL_ADOPT_H
#define CCTL_ADOPT_H


/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Adopt all pre-existing mapped windows at window manager
 *        startup
 *
 * Queries the window tree for each screen in @p wm and calls
 * @c client_init on any already-mapped, non-override-redirect child.
 *
 * @param wm Pointer to the window manager singleton
 *
 * @note Complexity: @e O(n), where @e n is the total number of
 *       pre-existing windows across all screens
 */
void cctl_adopt_scan(const wm_td *wm);


#endif  /* ! CCTL_ADOPT_H */
