/**
 * @file cmds/client/meta.h
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

#ifndef CMDS_CCMD_META_H
#define CMDS_CCMD_META_H


/* Project includes */
#include <client.h>


/* Public interface */
/**
 * @brief Rename the client window
 *
 * Updates both @c WM_NAME and @c _NET_WM_NAME properties.
 *
 * @param client Window to rename
 * @param name   New title
 *
 * @note Complexity: @e O(n), where @e n is the length of the new name
 */
void ccmd_client_rename(client_td *client, const char *name);

/**
 * @brief Change the @c WM_CLASS of the client window
 *
 * Updates both instance and class strings in the @c WM_CLASS property.
 *
 * @param client        Window to reclassify
 * @param class_name    New class string
 * @param instance_name New instance string
 *
 * @note Complexity: @e O(n), where @e n is the combined class name
 *       length
 */
void ccmd_client_reclass(client_td *client,
        const char *restrict class_name,
        const char *restrict instance_name);

/**
 * @brief Change the @c WM_WINDOW_ROLE of the client window
 *
 * @param client Window to change its role
 * @param role   New role name
 *
 * @note Complexity: @e O(n), where @e n is the length of the role name
 */
void ccmd_client_rerole(client_td *client, const char *role);

/**
 * @brief Set the icon name for the client window
 *
 * Updates @c WM_ICON_NAME and @c _NET_WM_ICON_NAME properties.
 *
 * @param client    Window to set icon
 * @param icon_name New icon name
 *
 * @note Complexity: @e O(n), where @e n is the length of the icon name
 */
void ccmd_client_set_icon(client_td *client, const char *icon_name);


#endif  /* ! CMDS_CCMD_META_H */
