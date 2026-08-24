/**
 * @file config.h
 *
 * @brief Configuration structures and procedures declaration
 *
 * The default configuration is taken from the configuration files on
 * the default configuration base directory.  This directory depends on
 * the environment variables: @c XDG_CONFIG_HOME/ICOWM_NAME_PROG if the
 * variable @c XDG_CONFIG_HOME is set, otherwise it will default to the
 * classic @c ($HOME/.ICOWM_NAME_PROG).
 *
 * @defgroup config Configuration loading
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_H
#define CONFIG_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Default initial values */
#include <defs/config.h>

/* Local includes */
#include <config/a11y.h>
#include <config/base.h>
#include <config/bindings.h>
#include <config/desktops.h>
#include <config/randr.h>
#include <config/theme.h>


/**
 * @brief Main configuration structure
 *
 * Encapsulates the main configuration, including base settings,
 * bindings, and theme.
 */
typedef struct {
    struct config_base_s base;
    struct config_bindings_s bindings;
    struct config_theme_s theme;
    struct config_randr_s randr;
    struct config_desktop_s desktops;
    struct config_a11y_s a11y;
} config_td;


/* Public interface */
/**
 * @brief Initialize a new structure for the configuration
 *
 * Allocates memory for a new @c config_td structure and initializes
 * its fields to an ordinary session's own default values.
 *
 * @return Pointer to the initialized configuration structure, or
 *         @c NULL on failure
 * 
 * @note The caller takes ownership of the returned configuration and
 *       releases it with @a config_destroy
 * @note Restricted-memory mode never calls this
 * @note Complexity: @e O(1), as it only involves memory allocation
 *       and initialization
 *
 * @see @a config_memguard_init in @c config/memguard.h for its own
 *      completely separate path, which this function knows nothing
 *      about
 */
config_td *config_init(void);

/**
 * @brief Destroy a configuration structure and free resources
 *
 * Frees the memory associated with a @c config_td structure.
 *
 * @param config Pointer to the configuration structure to destroy
 *
 * @note Complexity: @e O(1), as it only involves freeing memory
 *
 * @see @c config_td
 */
void config_destroy(config_td *config);

/**
 * @brief Populate default values for one theme structure
 *
 * Used both as the compiled-in fallback theme (via
 * @a config_set_default_values & @a config_set_default_values_memguard
 * in @c config/memguard.h) and, before applying any theme file found,
 * as the known-good starting point that file's own fields then overlay.
 *
 * @a config_load_theme only ever overwrites whichever fields a theme
 * file specifies, never resets the rest on its own, so a caller that
 * skips this first and reuses whatever @p theme already held from
 * a previous load would leave a field the new file no longer specifies
 * (e.g., a boolean like @p window.is-decorated) stuck at its old value
 * instead of falling back to this default.  @a config_load calls this
 * itself before loading a theme file on every call, not just the first,
 * for that reason.
 *
 * @param theme Theme structure to populate
 *
 * @note Complexity: @e O(n), where @e n is the number of fields that
 *       need to be set
 */
void config_set_default_theme_values(struct config_theme_s *theme);

/**
 * @brief Populate default values for one accessibility (a11y)
 *        structure
 *
 * Used both as the initial process-wide default and, before applying
 * any @c a11y.json found, as the known-good starting point that file's
 * own fields then overlay.
 *
 * The same rationale as @a config_set_default_theme_values above
 * applies here: a reload whose @c a11y.json skips a field, or is absent
 * entirely, or whose @p is-enabled has just turned @c false, must not
 * leave that field stuck at whatever an earlier, still-enabled load
 * happened to set it to.  @a config_load_a11y calls this itself before
 * parsing a file on every call, not just the first, for exactly that
 * very same reason.
 *
 * @param a11y Accessibility structure to populate
 *
 * @note Complexity: @e O(1)
 */
void config_set_default_a11y_values(struct config_a11y_s *a11y);

/**
 * @brief Load all of an ordinary session's own configuration
 *
 * Loads configuration settings into the provided @c config_td structure
 * from predefined sources (@c config.json, @c bindings.json, the named
 * theme file, and @c randr.json), by invoking @a config_load_base,
 * @a config_load_bindings, @a config_load_theme, and
 * @a config_load_randr in that order.
 *
 * @param config            Pointer to the configuration structure where
 *                          to load the data
 * @param config_dir_prefix Configuration directory, or @c NULL to use
 *                          default value
 *
 * @return 0 on success, or otherwise
 *
 * @note Restricted-memory mode NEVER calls this
 * @note This function does not take into account default values,
 *       because it's invoked after calling @a config_set_default_values
 * @note Complexity: @e O(n), where @e n is the number of parameters
 *       loaded because it involves reading from the configuration file
 *
 * @see @a config_load_memguard in @c config/memguard.h for its own
 *      completely separate path (@c memguard.json instead of
 *      @c config.json, @c randr.json never read at all), which this
 *      function knows nothing about
 * @see @a config_load_base,
 *      @a config_load_bindings,
 *      @a config_load_theme
 */
int config_load(config_td *config, const char *config_dir_prefix);

/**
 * @brief Clear whichever theme file @c config_load last recorded as
 *        specified by @c config.json but not actually found
 *
 * Called once at the start of a full configuration-loading sequence,
 * the same as @a json_syntax_errors_reset (@c utils/config/json.h), so
 * a note from a previous load or reload is never repeated for a theme
 * that has since been fixed, or attributed to the wrong one.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a wm_start and @c wm_action_config_reload
 */
void config_missing_theme_reset(void);

/**
 * @brief The theme file path @c config_load most recently found
 *        specified by @c config.json but missing, if any
 *
 * Distinct from a theme file that was found but failed to parse as JSON
 * (that case is already covered by @a json_syntax_errors_get, since
 * @a config_load_theme goes through @a json_load_config the same as any
 * other configuration file); this is specifically for @c config.json
 * parsing successfully, naming a theme, and that theme's own file
 * simply not existing.
 *
 * @return The path, or @c NULL if no theme is currently missing
 *
 * @note Complexity: @e O(1)
 */
const char *config_missing_theme_get(void);

/**
 * @brief Resolve the configuration directory from a prefix, or from
 *        environment variables when none is given
 *
 * Resolution order: @p config_dir_prefix, if given; otherwise
 * @c (${XDG_CONFIG_HOME}/icowm); otherwise @c (${HOME}/.icowm);
 * otherwise @c (./.icowm) in the current working directory.
 *
 * @param config_dir_prefix Configuration directory, or @c NULL to
 *                          resolve it from the environment instead
 * @param config_dir_base   Buffer to receive the resolved path
 *
 * @note @p config_dir_base should be at least
 *       @c CONFIG_MAX_LENGTH_PATH_BASE bytes
 * @note Complexity: @e O(1)
 */
void config_resolve_dir(const char *restrict config_dir_prefix,
        char *restrict config_dir_base);

/**
 * @brief Populate default values for the base and desktop-navigation
 *        configuration structures
 *
 * Used both as the initial process-wide default and, before applying
 * any @c config.json (or @c memguard.json) found, as the known-good
 * starting point that file's own fields then overlay.
 *
 * @param config_base    Base configuration structure to populate
 * @param config_desktop Desktop-behavior structure to populate
 *
 * @note Complexity: @e O(s * d), where @e s is @c CONFIG_MAX_SCREENS
 *       and @e d is @c CONFIG_MAX_DESKTOPS
 */
void config_set_default_base_values(struct config_base_s *config_base,
        struct config_desktop_s *config_desktop);

/**
 * @brief Load base configuration settings from a JSON file
 *
 * Loads base configuration settings into the provided @p config_base_s
 * structure from the specified file, and desktop-navigation /
 * reserved-space behavior into @p config_desktop from that same file,
 * since both live in @c config.json.
 *
 * @param filename       The path to the configuration file
 * @param config_base    Pointer to the base configuration structure
 *                       to populate
 * @param config_desktop Pointer to the desktop-behavior structure to
 *                       populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the
 *       configuration file being read
 *
 * @see @c config_base_s
 * @see @c config_desktop_s
 */
int config_load_base(const char *filename,
        struct config_base_s *config_base,
        struct config_desktop_s *config_desktop);

/**
 * @brief Populate default values for the keyboard and mouse bindings
 *        configuration structure
 *
 * Used both as the initial process-wide default and, before applying
 * any @c bindings.json found, as the known-good starting point that
 * file's own fields then overlay.
 *
 * @param config_bindings Bindings configuration structure to populate
 *
 * @note Complexity: @e O(1)
 */
void config_set_default_bindings_values(
        struct config_bindings_s *config_bindings);

/**
 * @brief Load key bindings from a JSON file
 *
 * Loads key bindings into the provided @p config_bindings_s structure
 * from the specified file.
 *
 * @param filename        Path to the key bindings configuration file
 * @param config_bindings Pointer to the bindings configuration
 *                        structure to populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the key bindings
 *       file being read
 *
 * @see @p config_bindings_s
 */
int config_load_bindings(const char *filename,
        struct config_bindings_s *config_bindings);

/**
 * @brief Load theme settings from a JSON file
 *
 * Loads theme settings into the provided @p config_theme_s structure
 * from the specified file.
 *
 * @param filename     The path to the theme configuration file.
 * @param config_theme Pointer to the theme configuration structure to
 *                     populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the theme file
 *       being read
 *
 * @see @p config_theme_s
 */
int config_load_theme(const char *filename,
        struct config_theme_s *config_theme);

/**
 * @brief Convert a theme's own 0 to 100 opacity percentage to the
 *        32-bit value @c _NET_WM_WINDOW_OPACITY itself expects
 *
 * The property's own valid range is @c 0 (fully transparent) to
 * @c 0xffffffff (fully opaque); this scales @p percent linearly onto
 * that range, matching the same formula every compositing manager
 * already assumes for its own atom.
 *
 * @param percent Opacity percentage, 0 to 100
 *
 * @return The 32-bit value to publish on @c _NET_WM_WINDOW_OPACITY
 *
 * @note A value above 100 is treated as 100, since
 *       @p config_theme_style_s.opacity is meant to already be within
 *       range by the time this runs, this is only a last defensive
 *       clamp
 * @note Complexity: @e O(1)
 */
uint32_t config_theme_opacity_to_raw(uint8_t percent);

/**
 * @brief Populate default values for one RandR output-profile
 *        structure
 *
 * Used both as the initial process-wide default and, before applying
 * any @c randr.json found, as the known-good starting point that file's
 * own fields then overlay.
 *
 * @param config_randr RandR configuration structure to populate
 *
 * @note Complexity: @e O(1)
 */
void config_set_default_randr_values(
        struct config_randr_s *config_randr);

/**
 * @brief Load XRandR output profiles from a JSON file
 *
 * Loads per-output profile settings into the provided @p config_randr_s
 * structure from the specified file.
 *
 * @param filename     Path to the RandR configuration file
 * @param config_randr Pointer to the RandR configuration structure to
 *                     populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(n), where @e n is the number of output entries
 *       in the file
 *
 * @see @p config_randr_s
 */
int config_load_randr(const char *filename,
        struct config_randr_s *config_randr);


/**
 * @brief Load accessibility (a11y) settings from a JSON file
 *
 * Loads timing and visual-feedback overrides into the provided
 * @p config_a11y_s structure from the specified file.  A missing file,
 * or one that omits some field, leaves whatever @p config_a11y already
 * held (its compiled-in default) untouched for that field, so a partial
 * file only ever overrides what it actually names.
 *
 * @param filename    Path to the a11y configuration file
 * @param config_a11y Pointer to the a11y configuration structure to
 *                    populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(1)
 *
 * @see @p config_a11y_s
 */
int config_load_a11y(const char *filename,
        struct config_a11y_s *config_a11y);


#endif  /* ! CONFIG_H */
