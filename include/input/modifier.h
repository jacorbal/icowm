/**
 * @file input/modifier.h
 *
 * @brief Modifier-token resolution shared by the keyboard and mouse
 *        binding loaders
 *
 * @defgroup input Keyboard and mouse input
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_MODIFIER_H
#define INPUT_MODIFIER_H


/* System includes */
#include <stdint.h>

/* Project includes */
#include <config.h>


/**
 * @brief Resolve configured modifier aliases such as @c modc or @c mods
 *
 * Expands symbolic modifier aliases from the configuration into their
 * actual configured string values.  If the token does not match a known
 * alias, the original token is returned unchanged.
 *
 * @param config Configuration holding the alias strings
 * @param token  Modifier token to resolve
 *
 * @return Resolved modifier string, or the original token if no alias
 *         matches
 *
 * @note Complexity: @e O(1)
 */
const char *im_resolve_modifier_token(const config_td *config,
        const char *token);

/**
 * @brief Map a single modifier token to an XCB modifier mask
 *
 * Converts a textual modifier name into the corresponding XCB modifier
 * mask.  Supports configured aliases, common modifier names, and some
 * alternative spellings.
 *
 * @param config Configuration holding the alias strings
 * @param token  Modifier token to parse
 *
 * @return Matching XCB modifier mask, or 0 if not recognized
 *
 * @note Complexity: @e O(1)
 */
uint16_t im_parse_modifier_token(const config_td *config,
        const char *token);


#endif  /* ! INPUT_MODIFIER_H */
