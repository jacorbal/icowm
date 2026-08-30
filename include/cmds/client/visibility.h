/**
 * @file cmds/client/visibility.h
 *
 * @brief Functions on a client's iconify, hide, unhide, and shared
 *        unmap plumbing
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

#ifndef CMDS_CCMD_VISIBILITY_H
#define CMDS_CCMD_VISIBILITY_H


/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Perform the action to iconify (and minimize it)
 *
 * @param client Window to iconify
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_iconify(client_td *client);

/**
 * @brief Hide the client by minimizing it without iconifying
 *
 * @param client Window to hide
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_hide(client_td *client);

/**
 * @brief Show (unhide) the client
 *
 * @param client Window to unhide
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_unhide(client_td *client);

/**
 * @brief Unmap a client's decoration target, correctly pre-arming
 *        @c ignore.unmap first
 *
 * Every place in this project that unmaps a client window-manager-
 * side (iconifying, hiding for another desktop, sending it
 * elsewhere) shares this exact same two-step shape: increment
 * @c client->ignore.unmap by however many @c UnmapNotify events the
 * unmap below is about to generate, THEN issue the unmap itself, so
 * @a handler_unmap_notify (@c handler/map.c) correctly recognizes
 * this as a window-manager-initiated unmap rather than the client
 * withdrawing itself.  Two events always arrive for @p target itself
 * (its @c StructureNotify plus its parent's
 * @c SubstructureNotify); one further event arrives for the
 * titlebar, if present, via the frame's @c SubstructureNotify.
 *
 * A caller whose @p target can differ from @p client->window
 * (the frame, when decorated, rather than the bare content window)
 * and that also needs the content window itself unmapped separately
 * (@a ccmd_client_iconify and @a ccmd_client_hide, cmds/client/
 * visibility.c, are the only two such callers today) still has to
 * account for, and issue, that additional unmap on its own right
 * after calling this: two more events arrive for @c client->window
 * in that case, matching this same two-events-per-window rule, and
 * this function only knows about the one @p target it was actually
 * given.
 *
 * @param client     Client being unmapped; its @c ignore.unmap is
 *                    incremented here
 * @param target     Window to unmap: the frame when decorated, the
 *                   bare content window otherwise, as
 *                   @a ccmd_target_win resolves it
 *
 * @note A null @p client is a silent no-op
 * @note Complexity: @e O(1)
 */
void ccmd_client_unmap_decorated(client_td *client,
        xcb_window_t target);


#endif  /* ! CMDS_CCMD_VISIBILITY_H */
