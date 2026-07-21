/**
 * @file cmd/scmd.h
 *
 * @brief Declaration of actions related to screen surface management
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */
 
#ifndef SMDS_WCMD_H
#define SMDS_WCMD_H


/* Project includes */
#include <actdata.h>
#include <surface.h>


/* Public interface */
/**
 * @brief Add a new desktop to a surface
 *
 * @param surface      Pointer to the surface
 * @param surface_data Data containing information for the new desktop
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_add(surface_td *surface,
        action_data_surface_td *surface_data);

/**
 * @brief Remove a specified desktop from the surface
 *
 * @param surface      Pointer to the surface
 * @param surface_data Data containing information for the desktop to
 *                     remove
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_rem(surface_td *surface,
        action_data_surface_td *surface_data);

/**
 * @brief Switch the current view to a specified desktop
 *
 * @param surface      Pointer to the surface
 * @param surface_data Data containing information for the desktop to
 *                     switch to
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_desktop_switch(surface_td *surface,
        action_data_surface_td *surface_data);

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

/**
 * @brief Toggle full screen surface mode
 *
 * Toggles the current surface between full screen and windowed modes
 *
 * @param surface Pointer to the surface
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_toggle_fullscreen(surface_td *surface);

/**
 * @brief Set the resolution of the surface
 *
 * @param surface      Pointer to the surface
 * @param surface_data Data containing new resolution settings
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_set_resolution(surface_td *surface,
        action_data_surface_td *surface_data);

/**
 * @brief Set the orientation of the surface
 *
 * @param surface      Pointer to the surface
 * @param surface_data Data containing new orientation settings
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_set_orientation(surface_td *surface,
        action_data_surface_td *surface_data);

/**
 * @brief Adjust the brightness of the surface
 *
 * @param surface      Pointer to the surface
 * @param surface_data Data containing brightness settings
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_set_brightness(surface_td *surface,
        action_data_surface_td *surface_data);

/**
 * @brief Adjust the contrast of the surface
 *
 * @param surface      Pointer to the surface
 * @param surface_data Data containing contrast settings
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_set_contrast(surface_td *surface,
        action_data_surface_td *surface_data);

/**
 * @brief Configure screen settings
 *
 * Configures various settings of the surface based on provided data
 *
 * @param surface      Pointer to the surface
 * @param surface_data Data containing configuration settings
 *
 * @note Complexity: @e O(1)
 */
void scmd_surface_configure_settings(surface_td *surface,
        action_data_surface_td *surface_data);


#endif  /* ! CMDS_SCMD_H */
