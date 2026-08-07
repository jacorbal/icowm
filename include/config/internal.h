/**
 * @file config/internal.h
 *
 * @brief Private helpers shared across config implementation modules
 *
 * Declares static helper functions that are used by more than one of
 * the config translation units (@c config/base.c, @c config.c,
 * @c config/randr.c) but must not be exposed as part of the public
 * configuration API declared in @c config.h.
 *
 * @note This header is private to the config subsystem and must not be
 *       included outside of @c src/config/
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_INTERNAL_H
#define CONFIG_INTERNAL_H


/* Default initial values */
#include <defs/config.h>


/**
 * @brief Resolve and write the configuration directory base path
 *
 * Writes the effective configuration directory into @p config_dir_base.
 * Resolution order: @p config_dir_prefix (when non-empty) >
 * @c $XDG_CONFIG_HOME/icowm > @c $HOME/.icowm > @c ./icowm.
 *
 * @param config_dir_prefix Caller-supplied prefix, or @c NULL to use
 *                          the environment-based default
 * @param config_dir_base   Buffer that receives the resolved path;
 *                          should be at least
 *                          @c CONFIG_MAX_LENGTH_PATH_BASE bytes long
 *
 * @note Implemented in @c config.c
 * @note Complexity: @e O(1)
 */
void ci_config_dir_set(const char *config_dir_prefix,
        char *config_dir_base);


#endif  /* ! CONFIG_INTERNAL_H */
