/**
 * @file cmds/client/icon.h
 *
 * @brief Functions on an already-iconified client's icon
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
 * @brief Move an already-iconified client's icon to a fresh,
 *        non-overlapping spot where the one it holds is now occupied
 *
 * For a client whose icon window already exists, unlike
 * @a ccmd_client_iconify, which creates one from nothing: @p client's
 * saved @c icon_pos is tested against every other already-mapped icon
 * on whichever desktop @p client is on at the time and, only where
 * that exact spot is taken, a fresh position is resolved and the icon
 * window moved to it.  A no-op otherwise, so an icon that still has
 * a free spot keeps it exactly where it was.
 *
 * Meant for a client whose desktop changed out from under it without
 * anyone having moved its icon (@a surface_action_desktop_remove,
 * @c surface.h, evacuating every client still on a desktop being
 * removed, foremost among the ways that happens).  The ordinary
 * "reuse the saved position unless claimed" reasoning
 * @a ccmd_client_iconify applies to a freshly iconified client has no
 * counterpart for one arriving on a desktop it was never iconified
 * on.
 *
 * @param client Client whose icon position to test and, where needed,
 *               relocate
 *
 * @note The fresh position is resolved exactly as one for a newly
 *       iconified client is, against the monitor @p client sits on
 *       and clear of both the configured desktop margins and the
 *       system tray, the two paths sharing one computation so that
 *       they cannot answer differently
 * @note A no-op where @p client is @c NULL, has no icon window, or is
 *       not iconified
 * @note Complexity: @e O(n), where @e n is the number of already-
 *       iconified clients on @p client's current desktop
 */
void ccmd_client_relocate_icon_if_taken(client_td *client);

/**
 * @brief Create the client's icon window if it does not exist yet, or
 *        reposition the existing one at its saved coordinates
 *
 * A new window is placed either at the client's remembered
 * @c icon_pos (if any, and not since claimed by another icon) or
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
