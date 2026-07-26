/**
 * @file cmds/layer.h
 *
 * @brief Client stacking-order command declarations
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

#ifndef CMDS_LAYER_H
#define CMDS_LAYER_H


/* Project includes */
#include <client.h>


/* Public interface */
/**
 * @brief Raise the client to the top of the stacking order
 *
 * @param client Window to raise
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_raise(client_td *client);

/**
 * @brief Lower the client to the bottom of the stacking order
 *
 * @param client Window to lower
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_lower(client_td *client);

/**
 * @brief Place the client in the above layer
 *
 * @param client Window to layer above
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_layer_above(client_td *client);

/**
 * @brief Place the client in the normal (default) layer
 *
 * @param client Window to normalize
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_layer_normal(client_td *client);

/**
 * @brief Place the client in the below layer
 *
 * @param client Window to layer below
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_layer_below(client_td *client);


#endif  /* ! CMDS_LAYER_H */
