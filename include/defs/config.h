/**
 * @file config.h
 *
 * @brief Definitions related to the configuration structure
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_CONFIG_H
#define DEFS_CONFIG_H


/* Maximum length in strings */
#define CONFIG_MAX_LENGTH_COMMAND (128)
#define CONFIG_MAX_LENGTH_BINDING (128)
#define CONFIG_MAX_LENGTH_OPTION (40)
#define CONFIG_MAX_LENGTH_FONTNAME (80)

/* Maximum length for file names and paths */
#define CONFIG_MAX_LENGTH_NAME (256)
#define CONFIG_MAX_LENGTH_FILENAME (256)
#define CONFIG_MAX_LENGTH_PATH_BASE (1024)
#define CONFIG_MAX_LENGTH_PATH_CONFIG \
    ((CONFIG_MAX_LENGTH_PATH_BASE) + (CONFIG_MAX_LENGTH_FILENAME))
#define CONFIG_MAX_LENGTH_PATH_THEME \
    ((CONFIG_MAX_LENGTH_PATH_BASE) + (CONFIG_MAX_LENGTH_FILENAME) \
     + (CONFIG_MAX_LENGTH_FILENAME))

/* Base default directories */
#define CONFIG_DIR_BASE  "icowm"
#define CONFIG_DIR_THEMES "themes"
#define CONFIG_FILENAME_BASE "config.json"
#define CONFIG_FILENAME_BINDINGS "bindings.json"

/* Default values when no value is given */
#define CONFIG_MAX_SCREENS (6)      /**< Initial max. number of screens */
#define CONFIG_MAX_DESKTOPS (10)    /**< Initial max. desktops per screen */


#endif  /* ! DEFS_CONFIG_H */
