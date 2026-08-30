/**
 * @file cmds/client/flags.h
 *
 * @brief Functions on a client's pin, opacity, border, urgency, and
 *        allowed-actions flags
 *
 * @defgroup cmds Client, desktop, and surface commands
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_CCMD_FLAGS_H
#define CMDS_CCMD_FLAGS_H


/* System includes */
#include <stdint.h>

/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Pin the client (visible on all desktops)
 *
 * @param client Window to pin
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_pin(client_td *client);

/**
 * @brief Unpin the client
 *
 * @param client Window to unpin
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unpin(client_td *client);

/**
 * @brief Toggle pin mode for the client
 *
 * @param client Window to toggle pin state
 *
 * @note No-op on a surface with only one desktop: stickiness has
 *       nothing to actually toggle when there is only the one
 * @note Complexity: @e O(1)
 */
void ccmd_client_toggle_pin(client_td *client);

/**
 * @brief Override the client's active-state opacity
 *
 * Sets @c opacity_override.is_set_active/@c .active, so this one
 * client's active-state opacity stops following the theme's
 * @p window.active.opacity until unset (there is currently no way
 * to unset it once a rule has set it; see @c rules_apply_s's
 * doc comment, rules/internal.h).
 *
 * @param client  Window whose active-state opacity to override
 * @param percent New opacity, @c 0 to @c 100
 *
 * @note A null @p client is a silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_set_opacity_active(client_td *client, uint8_t percent);

/**
 * @brief Override the client's inactive-state opacity
 *
 * Sets @c opacity_override.is_set_inactive/@c .inactive, the
 * inactive-state counterpart to @a ccmd_client_set_opacity_active.
 *
 * @param client  Window whose inactive-state opacity to override
 * @param percent New opacity, @c 0 to @c 100
 *
 * @note A null @p client is a silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_set_opacity_inactive(client_td *client,
        uint8_t percent);

/**
 * @brief Override the client's border color and width
 *
 * Sets @c border_override.is_set/@c .color/@c .width together, so
 * this one client's border stops following the theme's
 * @p window.active/@p .inactive.border until unset (there is
 * currently no way to unset it once set; see
 * @a scratchpad_notice_client_created in @c scratchpad.c for the
 * one existing caller).  Deliberately narrow, the same as
 * @a ccmd_client_apply_geometry: only the state itself, nothing
 * about re-applying the
 * border to the actual window right away, which stays each caller's
 * own concern (a caller wanting that immediately, rather than
 * waiting for the next natural render pass a focus
 * change already triggers, still has to make that call itself).
 *
 * @param client Window whose border to override
 * @param color  New border color, an @c 0xRRGGBB-style packed value
 * @param width  New border width in pixels
 *
 * @note A null @p client is a silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_set_border_override(client_td *client,
        uint32_t color, uint32_t width);

/**
 * @brief Mark the client as urgent (requesting attention)
 *
 * @param client Window to mark as urgent
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_urge(client_td *client);

/**
 * @brief Clear urgency marking from the client
 *
 * @param client Window to clear urgency
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unurge(client_td *client);

/**
 * @brief Publish @c _NET_WM_ALLOWED_ACTIONS for a client
 *
 * Computes the set of EWMH actions currently permitted for @p client
 * based on its resizable, focusable, and decoration properties, and
 * writes the result to the @c _NET_WM_ALLOWED_ACTIONS window property.
 * Must be called whenever the client's capabilities change (e.g., after
 * toggling resizability or decoration).
 *
 * @param client Client whose allowed-actions property should be updated
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_update_allowed_actions(client_td *client);


#endif  /* ! CMDS_CCMD_FLAGS_H */
