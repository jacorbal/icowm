/**
 * @file ipc/dispatch.h
 *
 * @brief Shared "resolve a client, act on it, report success" wrapper
 *
 * The overwhelming majority of client actions take only a @p client_id
 * and report a bare success once done; this is the one place that
 * resolves the client, calls whichever single action a command needs,
 * and builds the response, so those command handlers stay a one-line
 * call into this instead of each repeating the same
 * resolve/call/respond shape on its own.
 *
 * @see @c enact.h
 *
 * @defgroup ipc_dispatch IPC client-action dispatch
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_DISPATCH_H
#define IPC_DISPATCH_H

/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>


/**
 * @brief One client action's function pointer shape
 *
 * @param wm      Window manager instance
 * @param client  The resolved client to act on
 * @param stage   The client's stage
 * @param desktop The client's desktop
 *
 * @note Only a handful of actions, e.g., @c focus, actually need more
 *       than @p client itself; every other one ignores the rest of
 *       these parameters
 */
typedef void (*ipc_client_action_fn)(const wm_td *wm, client_td *client,
        stage_td *stage, desktop_td *desktop);


/**
 * @brief Resolve a request's @c client_id, run one action on it, and
 *        report the outcome
 *
 * @param wm     Window manager instance
 * @param args   The request object; must have a numeric @p client_id
 * @param action The one action to run once the client is found
 *
 * @return The standard success or failure response
 *
 * @see @p ipc_resolve_client in @c ipc/resolve.h for the failure
 *      wording when @p client_id is missing or names no current client
 *
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       stages, @e d the number of desktops per stage, and @e c the
 *       hash-table lookup cost per desktop; inherited from
 *       @a ip_resolve_client in @c ipc/resolve.h
 */
cJSON *ipc_dispatch_client_action(const wm_td *wm, const cJSON *args,
        ipc_client_action_fn action);


#endif  /* ! IPC_DISPATCH_H */
