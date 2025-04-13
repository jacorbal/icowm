/**
 * @file cmds/wmcmd.h
 *
 * @brief Declaration of functions for actions related to the windowm
 *        manager
 */
/*
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_WMCMD_H
#define CMDS_WMCMD_H


/* System includes */
#include <stdbool.h>

/* Project includes */
#include <actdata.h>
#include <wm.h>


/* Public interface */
/**
 * @brief Reload the current configuration for the window manager
 *
 * @param wm Pointer to the window manager instance
 *
 * @return Return @c true if the reload was successful, or otherwise
 *
 * @note Complexity: @e O(1)
 */
bool wmcmd_configuration_reload(wm_td *wm);

/**
 * @brief Save the current configuration of the window manager
 *
 * @param wm Pointer to the window manager instance
 *
 * @return Return @c true if the save was successful, or otherwise
 *
 * @note Complexity: @e O(1)
 */
bool wmcmd_configuration_save(wm_td *wm);

/**
 * @brief Add a new surface to the window manager
 *
 * @param wm      Pointer to the window manager instance
 * @param wm_data Pointer to the action data for the surface to add
 *
 * @note Complexity: @e O(1)
 */
void wmcmd_surface_add(wm_td *wm, action_data_wm_td *wm_data);

/**
 * @brief Remove a specified surface from the window manager
 *
 * @param wm      Pointer to the window manager instance
 * @param wm_data Pointer to the action data for the surface to remove
 *
 * @note Complexity: @e O(1)
 */
void wmcmd_surface_remove(wm_td *wm, action_data_wm_td *wm_data);

/**
 * @brief Prepare to exit the window manager
 *
 * Cleans up resources and exits the window manager application.  All
 * the memory resources taken by the window manager structure are
 * handled by the function @a wm_stop, but this clear the surfaces,
 * desktops and clients before doing that.
 *
 * @param wm Pointer to the window manager instance
 *
 * @note Complexity: @e O(1)
 */
void wmcmd_exit(wm_td *wm);


#endif  /* ! CMDS_WMCMD_H */
