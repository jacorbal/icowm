/**
 * @file ipc/response.h
 *
 * @brief Shared IPC response-building helpers
 *
 * The two shapes of response every command's own handler ends with
 * either a bare success, or a failure carrying a human-readable reason.
 * Neither builder does anything else (no logging, no side effects), so
 * every handler stays free to add its own result fields to the object
 * it gets back before returning it.
 *
 * @defgroup ipc_response IPC response building
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_RESPONSE_H
#define IPC_RESPONSE_H


/* JSON includes */
#include <cjson/cJSON.h>


/* Public interface */
/**
 * @brief Build a bare success response
 *
 * @return A newly allocated @c ({"ok": true}) object, ready for the
 *         caller to add its own result fields to, or @c NULL on
 *         allocation failure
 *
 * @note Complexity: @e O(1)
 */
cJSON *ipc_response_ok(void);

/**
 * @brief Build a standard failure response
 *
 * @param message Human-readable reason, copied into the response's own
 *                @c error field
 *
 * @return A newly allocated @c ({"ok": false, "error": message})
 *         object, or @c NULL on allocation failure
 *
 * @note Complexity: @e O(1)
 */
cJSON *ipc_response_error(const char *message);


#endif  /* ! IPC_RESPONSE_H */
