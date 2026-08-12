/**
 * @file cmds/surface.h
 *
 * @brief Declaration of actions related to screen surface management
 *
 * @ingroup cmds
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_SCMD_H
#define CMDS_SCMD_H


/* Project includes */
#include <surface.h>


/* Public interface */
/**
 * @brief Switch the current view to a specified desktop
 *
 * @param surface    Pointer to the surface
 * @param desktop_id Target desktop index
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch(surface_td *surface,
        uint32_t desktop_id);

/**
 * @brief Switch the current view to the next desktop in the sequence
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_next(surface_td *surface);

/**
 * @brief Switch the current view to the previous desktop in the
 *        sequence
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch_prev(surface_td *surface);


#endif  /* ! CMDS_SCMD_H */
