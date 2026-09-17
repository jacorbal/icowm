/**
 * @file enact.h
 *
 * @brief Every action this window manager can carry out on itself as
 *        a whole, one typed function per action
 *
 * Holds only the @a enact_wm_* actions, the handful that act on the
 * window manager itself rather than on any one client, desktop, or
 * stage.  Everything else that acts on one of those is split into
 * its own header instead, @c enact/client.h, @c enact/desktop.h, and
 * @c enact/stage.h, so a file that only needs one of those does not
 * also pull in, and rebuild against, every other one declared
 * alongside it.
 *
 * Each @a enact_* function, here and in its sibling headers, is the
 * single place in the whole project where its corresponding action
 * actually happens.  A caller anywhere else (a keybinding handler,
 * a menu callback, an EWMH message handler, a rule) calls the matching
 * @a enact_* function directly, with its typed parameters, instead of
 * reaching into one of the @c cmds/client/ headers or @c cmds/stage.h
 * itself.  Searching for an action's enum name always leads back to
 * exactly one function here or in one of these sibling headers.
 *
 * @see @c action.h
 * @see @c enact/client.h, @c enact/desktop.h, @c enact/stage.h
 *
 * @defgroup enact Action execution
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef ENACT_H
#define ENACT_H


/* Type includes */
#include <types/handles.h>


/* 'action_wm_e' */

/**
 * @brief Request that the window manager stop and exit
 *
 * @return @c 0 on success, non-zero otherwise
 *
 * @note Complexity: @e O(1)
 */
int enact_wm_exit(void);

/**
 * @brief Request that the window manager stop and restart itself in
 *        place, keeping every managed client open
 *
 * @return @c 0 on success, non-zero otherwise
 *
 * @note Complexity: @e O(1)
 */
int enact_wm_restart(void);

/**
 * @brief Reload the window manager's configuration
 *
 * @param wm Window manager instance
 *
 * @return @c 0 on success, non-zero otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the
 *       configuration being reloaded
 */
int enact_wm_configuration_reload(const wm_td *wm);


#endif  /* ! ENACT_H */
