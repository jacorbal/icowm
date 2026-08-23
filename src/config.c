/**
 * @file config.c
 *
 * @brief Configuration init, destroy, defaults, and load dispatcher
 *        implementation
 */
/*
 *  MUSINGS AND ADMONITIONS UNTO MINE OWN REFLECTIVE INNER SELF, WHICH
 *                           DWELLETH WITHIN
 *
 * Regarding mine own forthcoming self (Sat Mar 22 05:01 CET 2025):
 *      The present state of this code is grievously suboptimal.
 *      I implore thee to undertake its refactoring at thy earliest
 *      convenience, or at such hour as seemeth more fitting.
 *
 * Regarding the whilom expired self (Sun Mar 23 06:12 CET 2025):
 *      Thou shouldst have done it rightly from the very outset, and
 *      spared future headaches: *my* current headaches.
 *
 * Regarding the erstwhile selves now faded (Sun Jan 11 22:18 CET 2026):
 *      I find myself ensnared in the dire consequences of this wretched
 *      code, which continues to vex my weary soul with its torment.
 *      Each passing hour doth remind me of the ill-advised choices of
 *      yore; verily, I remain a prisoner of mine own flawed creations.
 *
 * Regarding my rambling selves of yesteryear (Sat Feb 14 11:36 CET 2026):
 *      Pish!  Let it matter not, ye idle knaves!  I shall alter naught!
 *
 * Regarding the self now undone by toil (Sun Aug 09 04:38 CET 2026):
 *      Much have I attempted, and all that mortal vigour would permit;
 *      beyond this point, I can but yield.  I did endeavour to bear
 *      this cursed contrivance further, yet my spirit faltered and my
 *      limbs denied me.  Alas, by fatigue am I brought low;
 *      O weariness, begotten of refactoring, thou most merciless of
 *      foes!
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
#include <stdlib.h>     /* NULL, calloc, free */
#include <string.h>     /* memcpy */

/* Utils includes */
#include <utils/config/json.h>
#include <utils/config/path.h>
#include <utils/safe/safestr.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>


/**
 * @brief Path of the theme file 'config_load' most recently found
 *        specified by @c config.json but missing; empty when none is
 *        currently missing
 */
static char s_missing_theme_file[CONFIG_MAX_LENGTH_PATH_THEME] = "";


/* Resolve the configuration directory from a prefix, or environment
 * variables when none is given (see config.h for the fallback order) */
void config_resolve_dir(const char *restrict config_dir_prefix,
        char *restrict config_dir_base)
{
    if (config_dir_prefix) {
        char temp_path[CONFIG_MAX_LENGTH_PATH_BASE];

        snprintf(temp_path, CONFIG_MAX_LENGTH_PATH_BASE,
                "%s", config_dir_prefix);
        path_simplify(temp_path);
        safe_strncpy(config_dir_base, temp_path,
                CONFIG_MAX_LENGTH_PATH_BASE);
        return;
    }

    xdg_resolve_dir(XDG_DIR_CONFIG, "./" CONFIG_DIR_BASE,
            config_dir_base, CONFIG_MAX_LENGTH_PATH_BASE);
}


/**
 * @brief Populate the configuration structure with an ordinary
 *        session's own default values
 *
 * A thin dispatcher that delegates to each module's own
 * @a config_set_default_*_values (@c config/base/defaults.c,
 * @c config/randr.c, @c config/bindings.c, @c config/a11y.c,
 * @c config/theme.c), rather than setting any field directly itself.
 *
 * @param config Pointer to the configuration structure to set the
 *               default values for
 *
 * @note Restricted-memory mode NEVER calls this
 * @note This function is loaded before user configuration, as
 *       a fail-safe for fields not yet configured manually
 * @note Complexity: @e O(n), where @e n is the number of fields that
 *       need to be set, across every module this delegates to
 *
 * @see @a config_set_default_values_memguard in @c config/memguard.h
 *      for its own completely separate profile, which this function
 *      knows nothing about.
 */
static void s_config_set_default_values(config_td *config)
{
    config_set_default_base_values(&config->base, &config->desktops);
    config_set_default_randr_values(&config->randr);
    config_set_default_bindings_values(&config->bindings);
    config_set_default_a11y_values(&config->a11y);
    config_set_default_theme_values(&config->theme);
}


/* Settle a theme's own final display name, once it is known whether
 * a theme file was actually loaded and whether that file itself set
 * its own "name" */
void ci_config_resolve_theme_name(struct config_theme_s *theme,
        const char *theme_file_name, bool theme_loaded)
{
    char combined[sizeof(theme->name)];
    size_t name_len;
    size_t file_len;
    size_t pos;
    size_t avail;
    size_t fit;

    if (theme == NULL) {
        return;
    }

    if (theme_file_name == NULL || theme_file_name[0] == '\0' ||
            !theme_loaded) {
        safe_strncpy(theme->name, "Default (builtin)",
                sizeof(theme->name));
        return;
    }

    if (theme->name[0] == '\0') {
        /* The loaded file set no "name" of its own: falls back to the
         * file's own short name (the same string "theme": "<this>" in
         * memguard.json/config.json names, not a path or the ".json"
         * extension), same as if that had been its "name" all along. */
        safe_strncpy(theme->name, theme_file_name, sizeof(theme->name));
        return;
    }

    /* Built with explicit, provably bounded 'memcpy' calls rather than
     * 'snprintf' with two '%s' arguments of a priori unknown length:
     * GCC's own '-Wformat-truncation' analysis cannot trace that the
     * combined length here can never exceed 'combined''s own size
     * through reasoning this indirect, and warns as if the call could
     * write past it even though it provably cannot, the very same
     * issue, and the same fix, already applied in 's_message_wrap_text'
     * (in 'menu/dialog/message.c').
     *
     * Truncates either string in turn (theme name first, then the file
     * name) rather than failing outright when the two together would
     * not fit: this is a display label, not something anything else
     * parses back apart, so a truncated one is a fully acceptable
     * outcome here, unlike it would be for, say, a file path. */
    name_len = safe_strlen(theme->name);
    file_len = safe_strlen(theme_file_name);

    /* Reserves the 4 bytes " (" + ")" + '\0' need beyond whatever
     * room the two strings themselves take up. */
    avail = (sizeof(combined) > 4u) ? sizeof(combined) - 4u : 0u;
    fit = (name_len > avail) ? avail : name_len;
    memcpy(combined, theme->name, fit);
    pos = fit;

    combined[pos] = ' ';
    ++pos;
    combined[pos] = '(';
    ++pos;

    /* Reserves the 2 bytes ')' + '\0' still need beyond 'pos'. */
    avail = (pos < sizeof(combined) - 2u)
        ? sizeof(combined) - 2u - pos
        : 0u;
    fit = (file_len > avail) ? avail : file_len;
    memcpy(combined + pos, theme_file_name, fit);
    pos += fit;

    combined[pos] = ')';
    ++pos;
    combined[pos] = '\0';

    safe_strncpy(theme->name, combined, sizeof(theme->name));
}


/* Initialize a new configuration structure */
config_td *config_init(void)
{
    config_td *config;

    LOGGER_DEBUG("Initializing configuration structure", L_NARG);

    config = calloc(1, sizeof(config_td));
    if (config == NULL) {
        LOGGER_ERROR("Failed to allocate memory for configuration" \
                " structure", L_NARG);
        return NULL;
    }

    LOGGER_DEBUG("Setting configuration to default values", L_NARG);
    s_config_set_default_values(config);

    return config;
}


/* Destroy a configuration structure and free resources */
void config_destroy(config_td *config)
{
    LOGGER_DEBUG("Destroying configuration structure", L_NARG);
    if (config != NULL) {
        free(config);
    }
}


/* Clear whichever theme file config_load last recorded as specified
 * but not actually found */
void config_missing_theme_reset(void)
{
    s_missing_theme_file[0] = '\0';
}


/* The theme file path config_load most recently found specified but
 * missing, if any */
const char *config_missing_theme_get(void)
{
    return (s_missing_theme_file[0] != '\0')
        ? s_missing_theme_file : NULL;
}


/* Load all the configuration */
int config_load(config_td *config, const char *config_prefix)
{
    char config_dir[CONFIG_MAX_LENGTH_PATH_BASE];
    char config_base_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_bindings_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_theme_file[CONFIG_MAX_LENGTH_PATH_THEME];
    char config_randr_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_a11y_file[CONFIG_MAX_LENGTH_PATH_CONFIG];

    config_resolve_dir(config_prefix, config_dir);

    LOGGER_DEBUG("Loading configuration from files on: '%s'",
            config_dir);

    /* Set main base configuration path */
    snprintf(config_base_file, sizeof(config_base_file),
            "%s/%s", config_dir, CONFIG_FILENAME_BASE);

    /* Load base configuration */
    if (config_load_base(config_base_file, &(config->base),
                &(config->desktops)) != 0) {
        LOGGER_WARNING("Base configuration could not be loaded;" \
                " default values will be used", L_NARG);
        return 1;
    }
    LOGGER_DEBUG("Loaded base configuration from '%s'",
            config_base_file);

    /* Set bindings configuration path */
    snprintf(config_bindings_file, sizeof(config_bindings_file),
            "%s/%s", config_dir, CONFIG_FILENAME_BINDINGS);

    /* Load bindings */
    if (config_load_bindings(config_bindings_file,
                &(config->bindings)) != 0) {
        LOGGER_ERROR("Failed to load bindings from:" \
                " '%s'; default bindings will be used",
                config_bindings_file);
    } else {
        LOGGER_DEBUG("Loaded key/mouse bindings from '%s'",
                config_bindings_file);
    }

    /* Set theme file path */
    snprintf(config_theme_file,
            sizeof(config_theme_file),
            "%s/%s/%s.json", config_dir, CONFIG_DIR_THEMES,
            config->base.theme);

    /* Reset every field back to its own known default first, on every
     * call here, not just the first: 'config_load_theme' below only
     * ever overwrites whichever fields the theme file itself specifies,
     * so without this a reload that switched to a theme missing some
     * field the previous one did specify would leave that field stuck
     * at the old theme's own value instead of falling back to this
     * default. */
    config_set_default_theme_values(&config->theme);

    /* Load theme if it's specified, i.e., not empty string */
    if (safe_strlen(config->base.theme) == 0) {
        LOGGER_NOTICE("No theme specified in base configuration;" \
                " default will be used", L_NARG);
        ci_config_resolve_theme_name(&config->theme, config->base.theme,
                false);
    } else {
        /* Snapshot the syntax-error count before attempting the load,
         * so a load failure can be told apart from one that
         * 'json_load_config' (via 'config_load_theme') already recorded
         * there itself: a theme file that exists but fails to parse is
         * a syntax error like any other JSON file's, and already
         * covered that way; a theme file that simply is not there at
         * all is a different, narrower case, worth its own note (see
         * 'config_missing_theme_get') precisely because that one,
         * unlike a syntax error, is otherwise silent by design. */
        uint32_t syntax_errors_before = json_syntax_errors_count();
        bool theme_loaded;

        LOGGER_DEBUG("Loading theme '%s' from '%s'",
                config->base.theme, config_theme_file);
        theme_loaded = (config_load_theme(config_theme_file,
                    &(config->theme)) == 0);
        if (!theme_loaded) {
            LOGGER_WARNING("Failed to load theme from:" \
                    " '%s'; default theme will be used",
                    config_theme_file);
            if (json_syntax_errors_count() == syntax_errors_before) {
                safe_strncpy(s_missing_theme_file, config_theme_file,
                        sizeof(s_missing_theme_file));
            }
        }
        ci_config_resolve_theme_name(&config->theme, config->base.theme,
                theme_loaded);
        if (theme_loaded) {
            LOGGER_DEBUG("Loaded theme '%s' (\"%s\") from '%s'",
                    config->base.theme, config->theme.name,
                    config_theme_file);
        }
    }

    /* Set RandR config file path and load (optional) */
    snprintf(config_randr_file, sizeof(config_randr_file),
            "%s/%s", config_dir, CONFIG_FILENAME_RANDR);
    if (config_load_randr(config_randr_file, &(config->randr)) != 0) {
        LOGGER_DEBUG("RandR configuration not found or could not be" \
                " loaded from '%s'; output profiles disabled",
                config_randr_file);
    } else {
        LOGGER_DEBUG("RandR configuration loaded from '%s'" \
                " (is-enabled=%d, outputs=%u)",
                config_randr_file,
                (int) config->randr.is_enabled,
                config->randr.output_count);
    }

    /* Set a11y config file path and load (optional) */
    snprintf(config_a11y_file, sizeof(config_a11y_file),
            "%s/%s", config_dir, CONFIG_FILENAME_A11Y);
    if (config_load_a11y(config_a11y_file,
                &(config->a11y)) != 0) {
        LOGGER_DEBUG("Accessibility (a11y) configuration not found or" \
                " could not be loaded from '%s'; built-in defaults" \
                " kept", config_a11y_file);
    } else {
        LOGGER_DEBUG("Accessibility (a11y) configuration loaded" \
                " from '%s'", config_a11y_file);
    }

    return 0;
}
