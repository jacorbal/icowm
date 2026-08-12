/**
 * @file ipc/commands.h
 *
 * @brief IPC command dispatch table interface
 *
 * Declares the single entry point that turns one parsed request line
 * into a response line, routing by its own @c "cmd" field through a
 * table of named handlers.  Every handler is a thin wrapper around
 * an action IcoWM already exposes to the keyboard and mouse (see
 * @c enact.h): the IPC socket is a third way to reach that same
 * catalog of actions, not a separate one of its own.
 *
 * @defgroup ipc_commands IPC command dispatch
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_COMMANDS_H
#define IPC_COMMANDS_H


/* Project includes */
#include <wm.h>


/* Public interface */
/**
 * @brief Handle one complete IPC request line and produce a response
 *
 * @p request is expected to be a single JSON object with at least a
 * string @c "cmd" field (e.g. @c {"cmd": "list_desktops"}); any
 * other fields are that command's own arguments.  The response is
 * always a JSON object with at least a boolean @c "ok" field: @c
 * true with the command's own result fields alongside it on
 * success, @c false with a string @c "error" field describing what
 * went wrong (an unknown command, a missing or malformed argument,
 * a client or desktop ID that does not currently exist) on failure.
 * A malformed request (not valid JSON, or valid JSON missing @c
 * "cmd") is itself reported the same way, never left unanswered.
 * @c "subscribe" and @c "unsubscribe" are handled ahead of the
 * ordinary table (see @c ipc_client_subscribe/@c ipc_client_
 * unsubscribe in @c ipc.h): the only two commands whose own effect
 * belongs to @p client_idx's own connection rather than to @p wm.
 *
 * @param wm         Window manager instance
 * @param request    Null-terminated request line, without its own
 *                   trailing newline
 * @param client_idx Index of the connection @p request arrived on,
 *                   passed through to @c ipc_client_subscribe/@c
 *                   ipc_client_unsubscribe only; every other
 *                   command ignores it entirely
 *
 * @return A newly allocated, null-terminated JSON response line
 *         (without a trailing newline; the caller adds one if
 *         needed), which the caller must free.  Only @c NULL on
 *         outright memory allocation failure.
 *
 * @note Complexity: @e O(n), where @e n is the number of clients or
 *       desktops a listing command has to walk; @e O(1) for every
 *       other command
 */
char *ipc_commands_dispatch(wm_td *wm, const char *request,
        int client_idx);


#endif  /* ! IPC_COMMANDS_H */
