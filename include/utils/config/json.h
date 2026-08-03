/**
 * @file utils/config/json.h
 *
 * @brief Low-level JSON helper declarations
 *
 * Provides generic helpers for loading typed values from cJSON objects
 * and for reading and parsing JSON configuration files.
 *
 * @ingroup json JSON utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_CONFIG_JSON_H
#define UTILS_CONFIG_JSON_H


/* System includes */
#include <stdbool.h>
#include <stddef.h>     /* size_t */
#include <stdint.h>

/* JSON includes */
#include <cjson/cJSON.h>


/* Public interface */
/**
 * @brief Convert a hexadecimal color string to an unsigned 32-bit
 *        integer
 *
 * @param hex_color Hexadecimal color string, optionally prefixed with
 *                  @c '#'
 *
 * @return Color value as @c uint32_t, or @c 0 on parse failure
 *
 * @note Complexity: @e O(n), where @e n is the length of @p hex_color
 */
uint32_t json_hex2uint32(const char *hex_color);

/**
 * @brief Normalize a JSON field name to a canonical separator form
 *
 * Builds in @p field_norm a canonical version of @p field where every
 * separator '_' or '-' is replaced with '-', and ASCII uppercase
 * letters are converted to lowercase.
 *
 * @param field      Original field name
 * @param field_norm Destination buffer for the normalized field name
 * @param size       Size of @p field_norm in bytes
 *
 * @return @c true if normalization succeeded; @c false on invalid
 *         arguments or when the name does not fit in @p field_norm
 *
 * @note Complexity: @e O(n), where @e n is the length of @p field
 */
bool json_field_normalize(const char *field, char *field_norm,
        size_t size);

/**
 * @brief Retrieve a JSON object item by canonicalized field name
 *
 * Searches @p json for a field whose normalized name matches the
 * normalized version of @p field.  Both '_' and '-' are treated as the
 * same separator and normalized to '-'.
 *
 * @param json  JSON object to search
 * @param field Requested field name
 *
 * @return Matching @c cJSON item, or @c NULL if not found
 *
 * @note Complexity: @e O(k * n), where @e k is the number of fields in
 *       the object and @e n is the average field name length
 */
cJSON *json_get_item(cJSON *json, const char *field);

/**
 * @brief Load a color value from a JSON object into a @c uint32_t
 *
 * Extracts @p field from @p json and converts its hexadecimal string
 * value into a 32-bit color.  Field lookup is canonicalized.
 *
 * @param json  JSON object
 * @param field Name of the field to read
 * @param dest  Destination color value
 *
 * @return @c 0 on success, @c 1 on failure
 *
 * @note @p dest is left unchanged on failure
 * @note Complexity: @e O(k * n + m), where @e k is the number of
 *       fields, @e n is the average field name length, and @e m is the
 *       color string length
 */
int json_load_color(cJSON *json, const char *field, uint32_t *dest);

/**
 * @brief Load a string value from a JSON object into a buffer
 *
 * Extracts @p field from @p json and copies its string value into
 * @p dest.  Field lookup is canonicalized.
 *
 * @param json  JSON object
 * @param field Name of the field to read
 * @param dest  Destination buffer
 * @param size  Capacity of @p dest including the null terminator
 *
 * @return @c 0 on success, @c 1 on failure
 *
 * @note @p dest is always null-terminated on success
 * @note Complexity: @e O(k * n + m), where @e k is the number of
 *       fields, @e n is the average field name length, and @e m is the
 *       string length
 */
int json_load_string(cJSON *json, const char *field,
        char *dest, size_t size);

/**
 * @brief Load an unsigned integer value from a JSON object
 *
 * Extracts @p field from @p json and stores its integer value in
 * @p dest.  Field lookup is canonicalized.
 *
 * @param json  JSON object
 * @param field Name of the field to read
 * @param dest  Destination unsigned integer
 *
 * @return @c 0 on success, @c 1 on failure
 *
 * @note @p dest is left unchanged on failure
 * @note Complexity: @e O(k * n), where @e k is the number of fields and
 *       @e n is the average field name length
 */
int json_load_uint(cJSON *json, const char *field, unsigned int *dest);

/**
 * @brief Load a boolean value from a JSON object
 *
 * Extracts @p field from @p json and stores its boolean value in
 * @p dest.  Field lookup is canonicalized.
 *
 * @param json  JSON object
 * @param field Name of the field to read
 * @param dest  Destination boolean
 *
 * @return @c 0 on success, @c 1 on failure
 *
 * @note @p dest is left unchanged on failure
 * @note Complexity: @e O(k * n), where @e k is the number of fields and
 *       @e n is the average field name length
 */
int json_load_bool(cJSON *json, const char *field, bool *dest);

/**
 * @brief Read a JSON file into a dynamically allocated string
 *
 * Opens @p filename, reads its entire contents, and writes the address
 * of a null-terminated heap buffer to @p *data.  The caller is
 * responsible for freeing that buffer.
 *
 * @param filename Path to the JSON file
 * @param data     Output pointer; receives the allocated buffer address
 *
 * @return Status of the operation
 * @retval 0 Success
 * @retval 1 File not found or unreadable
 * @retval 2 Memory allocation failure
 *
 * @note @p *data is set to @c NULL on failure
 * @note Complexity: @e O(n), where @e n is the file size in bytes
 */
int json_load_file(const char *filename, char **data);

/**
 * @brief Parse a JSON configuration file into a @c cJSON object
 *
 * Reads @p filename and parses it into a heap-allocated @c cJSON object
 * written to @p *json_out.  Accepts both a bare JSON object and
 * a single-element JSON array whose first element is an object.
 *
 * @param filename  Path to the JSON file
 * @param json_out  Output pointer; receives the parsed object
 *
 * @return Status of the operation
 * @retval 0 Success
 * @retval 1 Error reading the file
 * @retval 2 Error parsing the JSON data
 *
 * @note The caller must call @c cJSON_Delete on @p *json_out when done
 * @note Complexity: @e O(n), where @e n is the file size in bytes
 */
int json_load_config(const char *filename, cJSON **json_out);


#endif  /* ! UTILS_CONFIG_JSON_H */
