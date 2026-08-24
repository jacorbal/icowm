/**
 * @file cmds/client/resize.h
 *
 * @brief Client resize command declarations
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

#ifndef CMDS_CCMD_RESIZE_H
#define CMDS_CCMD_RESIZE_H

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>



/**
 * @brief Resize the client to new dimensions
 *
 * @param client Window to resize
 * @param geom   New frame position and dimensions
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_resize(client_td *client, struct geometry_s geom);


/**
 * @brief Resize the client to new dimensions immediately, bypassing
 *        any in-flight @c _NET_WM_SYNC_REQUEST throttling
 *
 * @c ccmd_client_resize's own queue-behind-the-outstanding-
 * acknowledgment behavior exists to avoid piling up unacknowledged
 * configures during a live sequence of rapid resize calls (an
 * ordinary interactive drag).  It is the wrong behavior for a single,
 * already-final geometry with no further calls to follow, since a
 * client that happens to still be mid-exchange from an earlier,
 * unrelated resize would otherwise have this one silently queued
 * behind that exchange's own @c AlarmNotify, with nothing left to
 * ever flush it once no further resize call arrives to retry it.
 * Callers with exactly that shape (one call, known to be the last)
 * should call this instead of @a ccmd_client_resize.
 *
 * @param client Window to resize
 * @param geom   New frame position and dimensions
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_resize_force(client_td *client, struct geometry_s geom);


/**
 * @brief Apply a client's pending @c (_NET_WM_SYNC_REQUEST)-throttled
 *        resize
 *
 * Called from @a handler_sync_event when an @c AlarmNotify confirms the
 * client has redrawn to match the last size it was sent.  Clears the
 * client's wait state and, if a newer geometry arrived from
 * @a ccmd_client_resize while it was waiting, applies that geometry now
 * and sends the next sync request so the throttling pipeline keeps up
 * with an ongoing interactive resize.
 *
 * @param client Client whose alarm just fired
 *
 * @note A no-op for clients that are not currently waiting on an
 *       acknowledgement
 * @note Complexity: @e O(1)
 */
void ccmd_client_resize_flush_pending(client_td *client);


#endif  /* ! CMDS_CCMD_RESIZE_H */
