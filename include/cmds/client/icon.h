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


/* Project includes */
#include <client.h>


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


#endif  /* ! CMDS_CCMD_ICON_H */
