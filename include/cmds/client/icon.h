/**
 * @file cmds/client/icon.h
 *
 * @brief Functions on an already-iconified client's own icon
 *        placement
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

#ifndef CMDS_CCMD_ICON_H
#define CMDS_CCMD_ICON_H


/* System includes */
#include <stdint.h>

/* Type includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Move an already-iconified client's own icon to a fresh,
 *        non-overlapping spot if its current one is now occupied
 *
 * For a client whose icon window already exists (unlike
 * @a ccmd_client_iconify, which creates one from scratch): checks
 * @p client's own current @c icon_x/icon_y against every other
 * already-mapped icon on whichever desktop @p client is on right
 * now, and, only if that exact spot is taken, resolves a new one via
 * @a place_icon_apply and moves the icon window there on screen if it
 * is currently mapped.  A no-op otherwise, so an icon that still has
 * a free spot keeps it exactly where it was.
 *
 * Meant for a client whose desktop just changed out from under it
 * without the person ever explicitly moving its icon themselves
 * (@a surface_action_desktop_remove, surface.h, evacuating every
 * client still on the desktop being removed foremost among them):
 * the ordinary "reuse the saved position unless claimed" logic
 * @a ccmd_client_iconify itself already applies to a freshly iconified
 * client has no equivalent for one that arrives on a desktop it was
 * never actually iconified on.
 *
 * @param client Client whose own icon position to check and, if
 *               needed, relocate
 *
 * @note No-op if @p client is @c NULL, has no icon window, or is not
 *       currently iconified
 * @note Complexity: @e O(n), where @e n is the number of already-
 *       iconified clients on @p client's own current desktop
 */
void ccmd_client_relocate_icon_if_taken(client_td *client);

/**
 * @brief Create the client's icon window if it does not exist yet, or
 *        reposition the existing one at its saved coordinates
 *
 * A new window is placed either at the client's own remembered
 * @c icon_x/icon_y (if any, and not since claimed by another icon) or
 * via @c place_icon_apply otherwise, then created with the theme's
 * inactive icon colors.  An already-existing icon window is simply
 * re-configured to its saved position, which may have changed since
 * if the user dragged it.
 *
 * @param client     Client whose icon window to create or reposition
 * @param icon_h_out Icon window height, including the caption band if
 *                   the theme captions icons
 *
 * @note Complexity: @e O(n), where @e n is the number of already-
 *       iconified clients on the same desktop
 */
void ccmd_client_ensure_icon_window(client_td *client,
        uint16_t icon_h_out);


#endif  /* ! CMDS_CCMD_ICON_H */
