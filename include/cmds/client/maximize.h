/**
 * @file cmds/client/maximize.h
 *
 * @brief Client maximize/fullscreen command declarations
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

#ifndef CMDS_CCMD_MAXIMIZE_H
#define CMDS_CCMD_MAXIMIZE_H


/* Project includes */
#include <client.h>


/* Public interface */
/**
 * @brief Re-fill an already-maximized client's own geometry against
 *        its current workarea
 *
 * A maximized client's own geometry, grown or shrunk in place, is only
 * ever right immediately after actually maximizing it: anything that
 * later changes what its own workarea resolves to (a panel mapped or
 * unmapped, @c desktops.margins reloaded, or the surface's
 * strutless-maximization mode, @a surface_action_toggle_strutless_maximize,
 * @c surface.h, toggled) leaves it still filling wherever the OLD
 * workarea was, not the new one, until something re-applies its
 * maximize geometry from scratch.  This does exactly that: resolved
 * against @a ccmd_client_resolve_workarea (the same resolution
 * @a ccmd_client_maximize itself already uses), so the client ends up
 * exactly refilling the workarea as it now stands, the same as if it
 * had only just been maximized.
 *
 * Only the axis (or axes) @p client's own @c properties.state actually
 * names gets touched: a client maximized on one axis alone keeps its
 * own other axis exactly as it already was, rather than growing it to
 * fill the workarea too and silently turning a horizontal- or
 * vertical-only maximize into a full one.
 *
 * @param client Client to re-fill; a no-op unless it is currently
 *               maximized on at least one axis
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_refill_maximized(client_td *client);


/**
 * @brief Maximize the client horizontally
 *
 * @param client Window to maximize horizontally
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void ccmd_client_maximize_horz(client_td *client);


/**
 * @brief Maximize the client vertically
 *
 * @param client Window to maximize vertically
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void ccmd_client_maximize_vert(client_td *client);


/**
 * @brief Demote a single axis's maximize state alone, without
 *        touching geometry at all
 *
 * For a caller that has already applied the correct un-maximized
 * geometry itself (a mouse-drag resize crossing the resistance
 * threshold on a maximized axis, live, on the very same motion
 * event; see @c drag_update, input/mouse/drag.c), unlike @c
 * ccmd_client_maximize_horz/@c _vert's own demote branch, which
 * always restores geometry from @c layout.geometry.old itself as
 * part of the same call.
 *
 * @param client Client whose axis just stopped being maximized
 * @param dir    @c 1 for horizontal, @c 2 for vertical; matches
 *               @c ccmd_client_maximize_horz/@c _vert's own axis
 *               numbering
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_demote_axis_state(client_td *client, int dir);


/**
 * @brief Promote a single axis's maximize state back, the exact
 *        inverse of @c ccmd_client_demote_axis_state, without
 *        touching geometry at all
 *
 * For a caller whose own drag has already re-frozen that axis back
 * at its maximized geometry itself (a mouse-drag resize dragged back
 * under the resistance threshold before release, live, on the very
 * same motion event; see @c drag_update, input/mouse/drag.c): the
 * live, reversible half of the same mechanism @c ccmd_client_demote_
 * axis_state's own doc comment describes, restoring @c MAXIMIZED
 * itself rather than @c NORMAL when the other axis is already
 * maximized on its own, @c MAXIMIZED_HORZ/@c _VERT otherwise.
 *
 * @param client Client whose axis just became maximized again
 * @param dir    @c 1 for horizontal, @c 2 for vertical; matches
 *               @c ccmd_client_maximize_horz/@c _vert's own axis
 *               numbering
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_promote_axis_state(client_td *client, int dir);


/**
 * @brief Maximize the client entirely
 *
 * @param client Window to maximize
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
void ccmd_client_maximize(client_td *client);


#endif  /* ! CMDS_CCMD_MAXIMIZE_H */
