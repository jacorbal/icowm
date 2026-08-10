/**
 * @file cmds/meta.h
 *
 * @brief Client metadata command declarations
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

#ifndef CMDS_META_H
#define CMDS_META_H


/* Project includes */
#include <actdata.h>
#include <client.h>


/* Public interface */
/**
 * @brief Rename the client window
 *
 * Updates both @c WM_NAME and @c _NET_WM_NAME properties.
 *
 * @param client      Window to rename
 * @param client_data Data containing the new name
 *
 * @note Complexity: @e O(n), where @e n is the length of the new name
 */
void wcmd_client_rename(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Change the @c WM_CLASS of the client window
 *
 * Updates both instance and class strings in the @c WM_CLASS property.
 *
 * @param client      Window to reclassify
 * @param client_data Data containing the new class strings
 *
 * @note Complexity: @e O(n), where @e n is the combined class name
 *       length
 */
void wcmd_client_reclass(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Change the @c WM_WINDOW_ROLE of the client window
 *
 * @param client      Window to change its role
 * @param client_data Data containing the new role name
 *
 * @note Complexity: @e O(n), where @e n is the length of the role name
 */
void wcmd_client_rerole(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Set the icon name for the client window
 *
 * Updates @c WM_ICON_NAME and @c _NET_WM_ICON_NAME properties.
 *
 * @param client      Window to set icon
 * @param client_data Data containing the icon name
 *
 * @note Complexity: @e O(n), where @e n is the length of the icon name
 */
void wcmd_client_set_icon(client_td *client,
        action_data_client_td *client_data);


#endif  /* ! CMDS_META_H */
