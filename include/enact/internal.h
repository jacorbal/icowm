/**
 * @file enact/internal.h
 *
 * @brief Private declarations shared across the enact/ modules
 *
 * @c enact.c was split by domain into @c enact/client.c,
 * @c enact/desktop.c, and @c enact/surface.c (@c enact.c itself keeps
 * only the two window-manager-level actions, @a enact_wm_exit and
 * @a enact_wm_configuration_reload, that do not belong to any one of
 * those three domains).  @a enact_broadcast_client_event is the only
 * helper any of those files need from one another: primarily a
 * client-domain concern, implemented in @c enact/client.c, but also
 * needed by @c enact/desktop.c's own @a enact_desktop_client_send,
 * @a _send_front, and @a _send_back, each of which broadcasts a
 * client-level IPC event about the one client they each act on.
 *
 * @note This header is private to @c enact/ and must not be included
 *       outside of it.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef ENACT_INTERNAL_H
#define ENACT_INTERNAL_H


/* System includes */
#include <stdint.h>

/* Project includes */
#include <types/handles.h>


/**
 * @brief Broadcast an IPC event carrying one client's own identifying
 *        fields
 *
 * @param client Client the event is about
 * @param type   IPC event bitmask (a single @c IPC_EVENT_* value; see
 *               ipc.h)
 *
 * @note No-op if @p client is @c NULL
 * @note Complexity: @e O(1)
 */
void enact_broadcast_client_event(client_td *client, uint32_t type);


#endif /* ! ENACT_INTERNAL_H */
