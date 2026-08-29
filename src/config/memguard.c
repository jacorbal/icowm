/**
 * @file config/memguard.c
 *
 * @brief Restricted-memory mode's own configuration profile
 *        (@c memguard.json) implementation
 *
 * Owns only the top-level orchestration: allocating the structure and
 * driving @c memguard.json's own load sequence in order.  The fixed
 * default profile lives in @c config/memguard/defaults.c, parsing
 * @c memguard.json itself in @c config/memguard/load.c, and applying
 * this mode's own theme restrictions in @c config/memguard/theme.c.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* calloc */

/* Utils includes */
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/config.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>
#include <config/memguard.h>


/* Allocate a new configuration structure, without populating it */
config_td *config_memguard_init(void)
{
    config_td *config;

    LOGGER_DEBUG("Initializing restricted-memory mode configuration" \
            " structure", L_NARG);

    config = calloc(1, sizeof(config_td));
    if (config == NULL) {
        LOGGER_ERROR("Failed to allocate memory for restricted-" \
                "memory mode configuration structure", L_NARG);
    }

    return config;
}


/* Load restricted-memory mode's own configuration, entirely
 * independent of config_load's own config.json path */
int config_load_memguard(config_td *config, const char *config_prefix)
{
    char config_dir[CONFIG_MAX_LENGTH_PATH_BASE];
    char config_memguard_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_bindings_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_theme_file[CONFIG_MAX_LENGTH_PATH_THEME];
    char config_a11y_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    int result = 0;

    config_set_default_values_memguard(config);

    config_resolve_dir(config_prefix, config_dir);

    LOGGER_DEBUG("Loading restricted-memory mode configuration from" \
            " files on: '%s'", config_dir);

    (void) snprintf(config_memguard_file, sizeof(config_memguard_file),
            "%s/%s", config_dir, CONFIG_FILENAME_MEMGUARD);
    if (ci_memguard_load_json(config_memguard_file, config) != 0) {
        LOGGER_WARNING("Restricted-memory mode configuration could" \
                " not be loaded from '%s'; its own fixed defaults" \
                " will be used", config_memguard_file);
        result = 1;
    } else {
        LOGGER_DEBUG("Loaded restricted-memory mode configuration" \
                " from '%s'", config_memguard_file);
    }

    (void) snprintf(config_bindings_file, sizeof(config_bindings_file),
            "%s/%s", config_dir, CONFIG_FILENAME_BINDINGS);
    if (config_load_bindings(config_bindings_file,
                &(config->bindings)) != 0) {
        LOGGER_ERROR("Failed to load bindings from: '%s'; default" \
                " bindings will be used", config_bindings_file);
    } else {
        LOGGER_DEBUG("Loaded key/mouse bindings from '%s'",
                config_bindings_file);
    }

    (void) snprintf(config_theme_file, sizeof(config_theme_file),
            "%s/%s/%s.json", config_dir, CONFIG_DIR_THEMES,
            config->base.theme);
    if (safe_strlen(config->base.theme) == 0u) {
        LOGGER_NOTICE("No theme specified in restricted-memory mode" \
                " configuration; default will be used", L_NARG);
        config_resolve_theme_name(&config->theme, config->base.theme,
                false);
    } else {
        bool theme_loaded;

        LOGGER_DEBUG("Loading theme '%s' from '%s'",
                config->base.theme, config_theme_file);
        theme_loaded = (config_load_theme(config_theme_file,
                    &(config->theme)) == 0);
        if (!theme_loaded) {
            LOGGER_WARNING("Failed to load theme from: '%s';" \
                    " default theme will be used", config_theme_file);
        }
        config_resolve_theme_name(&config->theme, config->base.theme,
                theme_loaded);
        if (theme_loaded) {
            LOGGER_DEBUG("Loaded theme '%s' (\"%s\") from '%s'",
                    config->base.theme, config->theme.name,
                    config_theme_file);
        }
    }

    ci_memguard_restrict_theme(config);

    /* Accessibility (a11y): loaded as its own independent file, the
     * same way 'bindings.json' and the active theme file already are
     * above, rather than folded into 'memguard.json' itself; wanting
     * to save memory is never a reason to also give up basic
     * accessibility accommodations */
    (void) snprintf(config_a11y_file, sizeof(config_a11y_file),
            "%s/%s", config_dir, CONFIG_FILENAME_A11Y);
    if (config_load_a11y(config_a11y_file, &(config->a11y)) != 0) {
        LOGGER_DEBUG("Accessibility (a11y) configuration not found or" \
                " could not be loaded from '%s'; built-in defaults" \
                " kept", config_a11y_file);
    } else {
        LOGGER_DEBUG("Accessibility (a11y) configuration loaded" \
                " from '%s'", config_a11y_file);
    }

    return result;
}
