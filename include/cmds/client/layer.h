/**
 * @file cmds/client/layer.h
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

#ifndef CMDS_CCMD_LAYER_H
#define CMDS_CCMD_LAYER_H


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
void ccmd_client_raise(client_td *client);

/**
 * @brief Lower the client to the bottom of the stacking order
 *
 * @param client Window to lower
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_lower(client_td *client);

/**
 * @brief Place the client in the above layer
 *
 * @param client Window to layer above
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_layer_above(client_td *client);

/**
 * @brief Place the client in the normal (default) layer
 *
 * @param client Window to normalize
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_layer_normal(client_td *client);

/**
 * @brief Place the client in the below layer
 *
 * @param client Window to layer below
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_layer_below(client_td *client);

/**
 * @brief Cycle the client's layer:
 *        @e (normal -> above -> below -> normal -> above -> ...)
 *
 * @param client Window whose layer is to be cycled
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_cycle_layer(client_td *client);

/**
 * @brief Enforce layer stacking order for all clients in a desktop
 *
 * Raises every client marked @c CLIENT_LAYER_ABOVE to the top of the
 * X stacking order and lowers every client marked @c CLIENT_LAYER_BELOW
 * to the bottom, ensuring the WM-layer semantics are preserved after
 * any stacking operation performed by a client or by the window manager
 * itself.
 *
 * Afterward, if @p desktop's own currently focused client
 * (@c client_active_id) is fullscreen, it is raised once more, above
 * every other client on @p desktop including every other
 * @c CLIENT_LAYER_ABOVE one, the same way a fullscreen application
 * covers a taskbar or panel in most desktop environments.  This is
 * deliberately a stacking-order effect only, never a change to the
 * client's own @c properties.layer: losing focus to something else
 * needs no separate "restore" step of its own, since the very next
 * call to this same function (from wherever focus changed) simply
 * finds it no longer named by @c client_active_id, and it settles
 * back into its own real layer group through the ordinary pass above.
 *
 * @param desktop Desktop whose clients are to be restacked
 *
 * @note A no-op when @p desktop is null or has no stacking list
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
void ccmd_desktop_enforce_layers(desktop_td *desktop);


#endif  /* ! CMDS_CCMD_LAYER_H */
