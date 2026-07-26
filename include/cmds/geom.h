/**
 * @file cmds/geom.h
 *
 * @brief Client geometry command declarations
 *
 * @ingroup cmds Client commands subsystem
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_GEOM_H
#define CMDS_GEOM_H


/* Project includes */
#include <actdata.h>
#include <client.h>


/* Public interface */
/**
 * @brief Move the client to a new position
 *
 * @param client      Window to move
 * @param client_data Data containing the new position
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_move(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Center the client on its current screen
 *
 * @param client Window to center
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void wcmd_client_center(client_td *client);

/**
 * @brief Resize the client to new dimensions
 *
 * @param client      Window to resize
 * @param client_data Data containing the new size
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_resize(client_td *client,
        action_data_client_td *client_data);

/**
 * @brief Maximize the client horizontally
 *
 * @param client Window to maximize horizontally
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void wcmd_client_maximize_horz(client_td *client);

/**
 * @brief Maximize the client vertically
 *
 * @param client Window to maximize vertically
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void wcmd_client_maximize_vert(client_td *client);

/**
 * @brief Maximize the client entirely
 *
 * @param client Window to maximize
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void wcmd_client_maximize(client_td *client);


#endif  /* ! CMDS_GEOM_H */
