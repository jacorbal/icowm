/**
 * @file cmds/layer.h
 *
 * @brief Client stacking-order command declarations
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

#ifndef CMDS_LAYER_H
#define CMDS_LAYER_H


/* Project includes */
#include <client.h>
#include <desktop.h>


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

/**
 * @brief Cycle the client's layer:
 *        @c (normal -> above -> below -> normal -> above -> ...)
 *
 * @param client Window whose layer is to be cycled
 *
 * @note Complexity: @e O(1)
 */
void wcmd_client_cycle_layer(client_td *client);

/**
 * @brief Enforce layer stacking order for all clients in a desktop
 *
 * Raises every client marked @c CLIENT_LAYER_ABOVE to the top of the
 * X stacking order and lowers every client marked @c CLIENT_LAYER_BELOW
 * to the bottom, ensuring the WM-layer semantics are preserved after
 * any stacking operation performed by a client or by the window manager
 * itself.
 *
 * @param desktop Desktop whose clients are to be restacked
 *
 * @note No-op when @p desktop is null or has no stacking list
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
void wcmd_desktop_enforce_layers(desktop_td *desktop);



#endif  /* ! CMDS_LAYER_H */
