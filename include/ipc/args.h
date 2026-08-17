/**
 * @file ipc/args.h
 *
 * @brief Typed extraction of a request's own arguments
 *
 * Every command's own arguments live as extra fields alongside its
 * @p cmd field, in the same request object; these read one field at
 * a time by name, each failing cleanly (returning @c false, leaving
 * @p out untouched) when the field is missing or not of the expected
 * JSON type, rather than ever guessing a default a caller did not ask
 * for.
 *
 * @defgroup ipc_args IPC argument extraction
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_ARGS_H
#define IPC_ARGS_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* JSON includes */
#include <cjson/cJSON.h>


/* Public interface */
/**
 * @brief Read a required unsigned integer field
 *
 * Meant for IDs (client, desktop, surface) and other values that are
 * never meaningfully negative.  A negative JSON number is rejected
 * rather than silently reinterpreted.
 *
 * @param args  The request object
 * @param field Field name to read
 * @param out   Receives the value on success, untouched on failure
 *
 * @return @c true when @p field was present, a number, and non-negative
 *
 * @note Complexity: @e O(1)
 */
bool ipc_args_get_uint(const cJSON *args, const char *field,
        uint32_t *out);

/**
 * @brief Read a required signed integer field
 *
 * Meant for window coordinates, which are meaningfully negative on
 * a multi-monitor setup (a monitor placed above or to the left of the
 * one at the coordinate origin).
 *
 * @param args  The request object
 * @param field Field name to read
 * @param out   Receives the value on success, untouched on failure
 *
 * @return @c true when @p field was present and a number
 *
 * @note Complexity: @e O(1)
 */
bool ipc_args_get_int(const cJSON *args, const char *field,
        int32_t *out);

/**
 * @brief Read a required string field
 *
 * @param args  The request object
 * @param field Field name to read
 * @param out   Receives a pointer into @p args's own storage on
 *              success (valid only as long as @p args is), untouched on
 *              failure
 *
 * @return @c true when @p field was present and a string
 *
 * @note Complexity: @e O(1)
 */
bool ipc_args_get_string(const cJSON *args, const char *field,
        const char **out);

/**
 * @brief Read a required boolean field
 *
 * @param args  The request object
 * @param field Field name to read
 * @param out   Receives the value on success, untouched on failure
 *
 * @return @c true when @p field was present and a boolean
 *
 * @note Complexity: @e O(1)
 */
bool ipc_args_get_bool(const cJSON *args, const char *field, bool *out);


#endif  /* ! IPC_ARGS_H */
