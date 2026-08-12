/**
 * @file ipc/dispatch.h
 *
 * @brief Shared "resolve a client, act on it, report success" wrapper
 *
 * The overwhelming majority of client actions (see @c enact.h) take
 * only a @c client_id and report a bare success once done; this is
 * the one place that resolves the client, calls whichever single
 * action a command needs, and builds the response, so those command
 * handlers stay a one-line call into this instead of each repeating
 * the same resolve/call/respond shape on its own.
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


/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>


/**
 * @brief One client action's own function pointer shape
 *
 * @param wm      Window manager instance (only a handful of actions,
 *                e.g. focus, actually need more than @p client
 *                itself; every other one ignores the rest of these
 *                parameters)
 * @param client  The resolved client to act on
 * @param surface The client's own surface
 * @param desktop The client's own desktop
 */
typedef void (*ipc_client_action_fn)(wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop);


/* Public interface */
/**
 * @brief Resolve a request's own @c client_id, run one action on it,
 *        and report the outcome
 *
 * @param wm     Window manager instance
 * @param args   The request object; must have a numeric @c client_id
 * @param action The one action to run once the client is found
 *
 * @return The standard success or failure response (see
 *         @c ipc_resolve_client in ipc/resolve.h for the failure
 *         wording when @c client_id is missing or names no current
 *         client)
 *
 * @note Complexity: @e O(1)
 */
cJSON *ipc_dispatch_client_action(wm_td *wm, const cJSON *args,
        ipc_client_action_fn action);


#endif  /* ! IPC_DISPATCH_H */
