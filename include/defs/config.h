/**
 * @brief Definitions related to the configuration structure
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

/* Base default directories */
#define CONFIG_DIR_BASE  "icowm"
#define CONFIG_DIR_THEMES "themes"
#define CONFIG_FILENAME_BASE "config.json"
#define CONFIG_FILENAME_BINDINGS "bindings.json"

/* Default values when no value is given */
#define CONFIG_MAX_SCREENS (2)      /* Initial number of screens */
#define CONFIG_MAX_DESKTOPS (10)    /* Initial desktops per screen */


#endif  /* ! DEFS_CONFIG_H */
