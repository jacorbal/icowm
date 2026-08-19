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
