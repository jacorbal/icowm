/**
 * @file config.h
 *
 * @brief Configuration structures and procedures declaration
 *
 * The default configuration is taken from the configuration files on
 * the default configuration base directory.  This directory depends on
 * the environment variables: @c XDG_CONFIG_HOME/ICOWM_NAME_PROG if the
 * variable @c XDG_CONFIG_HOME is set, otherwise it will default to the
 * classic @c HOME/.ICOWM_NAME_PROG.
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


/**
 * @brief Base settings configuration structure
 */
struct config_base_s {
    char theme[CONFIG_MAX_LENGTH_FILENAME];

    /* Desktops: number and which on is the default one */
    uint32_t screen_count;                      /**< Number of screens */
    struct {
        uint32_t desktop_count;                 /**< No. of desktops */
        uint32_t desktop_inaugural;             /**< Initial desktop */

        struct {
            char name[CONFIG_MAX_LENGTH_NAME];  /**< Desktop name */
            struct desktop_settings_s {
                union {
                    //Pixmap image;               /**< Background image */
                    uint32_t color;             /**< Background color */
                } background;
            } settings;                         /**< Desktop settings */
        } desktops[CONFIG_MAX_DESKTOPS];        /**< Desktops per screen */
    } screens[CONFIG_MAX_SCREENS];              /**< All screens */

    /* Basic main programs: terminal and program launcher */
    struct {
        char terminal[CONFIG_MAX_LENGTH_COMMAND];
        char launcher[CONFIG_MAX_LENGTH_COMMAND];
        char file_manager[CONFIG_MAX_LENGTH_COMMAND];
        char web_browser[CONFIG_MAX_LENGTH_COMMAND];
        char editor[CONFIG_MAX_LENGTH_COMMAND];
    } programs;

    /* General behavior of environment towards windows */
    struct {
        uint32_t snap;
        struct {
            bool is_new_focused;
            bool is_raised_on_focus;
            char policy[CONFIG_MAX_LENGTH_OPTION];  /* "click", "follow_mouse */
        } focus;
        struct {
            char policy[CONFIG_MAX_LENGTH_OPTION];  /* "smart",... */
            bool is_centered;
        } placement;
    } windows;
};


/**
 * @brief Keyboard and mouse bindings configuration structure
 */
struct config_bindings_s {
    /* Modifiers */
    char modc[CONFIG_MAX_LENGTH_BINDING];
    char mods[CONFIG_MAX_LENGTH_BINDING];
    char modl[CONFIG_MAX_LENGTH_BINDING];
    char mod1[CONFIG_MAX_LENGTH_BINDING];
    char mod2[CONFIG_MAX_LENGTH_BINDING];
    char mod3[CONFIG_MAX_LENGTH_BINDING];
    char mod4[CONFIG_MAX_LENGTH_BINDING];
    char mod5[CONFIG_MAX_LENGTH_BINDING];

    /* Keyboard bindings */
    struct keyboard_s {
        char terminal[CONFIG_MAX_LENGTH_BINDING];
        char launcher[CONFIG_MAX_LENGTH_BINDING];
        char file_manager[CONFIG_MAX_LENGTH_BINDING];
        char web_browser[CONFIG_MAX_LENGTH_BINDING];
        char editor[CONFIG_MAX_LENGTH_BINDING];
        char center[CONFIG_MAX_LENGTH_BINDING];
        char maximize[CONFIG_MAX_LENGTH_BINDING];
        char fullscreen[CONFIG_MAX_LENGTH_BINDING];
        char shade[CONFIG_MAX_LENGTH_BINDING];
        char pin[CONFIG_MAX_LENGTH_BINDING];
        char iconify[CONFIG_MAX_LENGTH_BINDING];
        char hide[CONFIG_MAX_LENGTH_BINDING];
        char close[CONFIG_MAX_LENGTH_BINDING];
        char kill[CONFIG_MAX_LENGTH_BINDING];
        char info[CONFIG_MAX_LENGTH_BINDING];
        char toggle_decoration[CONFIG_MAX_LENGTH_BINDING];
        char cycle_prev[CONFIG_MAX_LENGTH_BINDING];
        char cycle_next[CONFIG_MAX_LENGTH_BINDING];

        /* Window movement, absolute and relative positions */
        struct {
            struct relative_s {
                char right[CONFIG_MAX_LENGTH_BINDING];
                char left[CONFIG_MAX_LENGTH_BINDING];
                char up[CONFIG_MAX_LENGTH_BINDING];
                char down[CONFIG_MAX_LENGTH_BINDING];
            } relative;

            struct {
                char top_left[CONFIG_MAX_LENGTH_BINDING];
                char top_right[CONFIG_MAX_LENGTH_BINDING];
                char bottom_left[CONFIG_MAX_LENGTH_BINDING];
                char bottom_right[CONFIG_MAX_LENGTH_BINDING];
            } absolute;
        } move;

        struct {
            char right[CONFIG_MAX_LENGTH_BINDING];
            char left[CONFIG_MAX_LENGTH_BINDING];
            char up[CONFIG_MAX_LENGTH_BINDING];
            char down[CONFIG_MAX_LENGTH_BINDING];
        } resize;

        struct {
            char cycle_prev[CONFIG_MAX_LENGTH_BINDING];
            char cycle_next[CONFIG_MAX_LENGTH_BINDING];
            char cycle_icon_prev[CONFIG_MAX_LENGTH_BINDING];
            char cycle_icon_next[CONFIG_MAX_LENGTH_BINDING];
        } desktop;
    } keyboard;

    /* Mouse bindings */
    struct {
        char move[CONFIG_MAX_LENGTH_BINDING];
        char resize[CONFIG_MAX_LENGTH_BINDING];
        char lower[CONFIG_MAX_LENGTH_BINDING];
        struct {
            char cycle_prev[CONFIG_MAX_LENGTH_BINDING];
            char cycle_next[CONFIG_MAX_LENGTH_BINDING];
        } desktop;
    } mouse;
};


/**
 * @brief Theme-related configuration structure
 */
struct config_theme_s {
    /* Name of this theme, just for future reference if necessary */
    char name[80];

    /* Window theme */
    struct window_theme_s {
        struct general_s {
            uint32_t border_width;
            bool is_decorated;
        } general;

        struct {
            uint32_t background_color;
            uint32_t foreground_color;
            uint32_t border_color;
            char font[CONFIG_MAX_LENGTH_FONTNAME];
        } active;

        struct {
            uint32_t background_color;
            uint32_t foreground_color;
            uint32_t border_color;
            char font[CONFIG_MAX_LENGTH_FONTNAME];
        } inactive;
    } window;

    /* Icons theme when windows are iconified */
    struct {
        uint32_t background_color;
        uint32_t foreground_color;
        uint32_t border_color;
        uint32_t border_width;
        bool is_captioned;
        char font[CONFIG_MAX_LENGTH_FONTNAME];
    } icon;
};


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
} config_td;


/* Public interface */
/**
 * @brief Initialize a new structure for the configuration
 *
 * Allocates memory for a new @c config_td structure and initializes its
 * fields to default values.
 *
 * @return Pointer to the initialized configuration structure, or @c NULL
 *         on failure
 *
 * @note Complexity: @e O(1), as it only involves memory allocation and
 *       initialization
 *
 * @see @c config_td
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
 * @brief Populate the configuration structure with default values
 *
 * Sets default values for all fields in the given @c config_td
 * structure.
 *
 * @param config Pointer to the configuration structure to set the
 *               default values for
 *
 * @note This function is loaded before user configuration, as
 *       a fail-safe for fields not yet configured manually
 * @note Complexity: @e O(n), where @e n is the number of fields that
 *       need to be set
 */
void config_set_default_values(config_td *config);

/**
 * @brief Load all the configuration
 *
 * Loads configuration settings into the provided @c config_td structure
 * from predefined sources (configuration files) by invoking the
 * functions @a config_load_base, @a config_load_bindings and
 * @a config_load_theme.
 *
 * @param config            Pointer to the configuration structure where
 *                          to load the data
 * @param config_dir_prefix Configuration directory, or @c NULL to use
 *                          default value
 *
 * @return 0 on success, or otherwise
 *
 * @note This function does not take into account default values,
 *       because it's invoked after calling @a config_set_default_values
 * @note Complexity: @e O(n), where @e n is the number of parameters
 *       loaded because it involves reading from the configuration file
 *
 * @see @a config_load_base,
 *      @a config_load_bindings,
 *      @a config_load_theme
 */
int config_load(config_td *config, const char *config_dir_prefix);

/**
 * @brief Load base configuration settings from a JSON file
 *
 * Loads base configuration settings into the provided @c config_base_s
 * structure from the specified file.
 *
 * @param filename    The path to the configuration file
 * @param config_base Pointer to the base configuration structure to
 *                    populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the
 *       configuration file being read
 *
 * @see @c config_base_s
 */
int config_load_base(const char *filename,
        struct config_base_s *config_base);

/**
 * @brief Load key bindings from a JSON file
 *
 * Loads key bindings into the provided @c config_bindings_s structure
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
 * @see @c config_bindings_s
 */
int config_load_bindings(const char *filename,
        struct config_bindings_s *config_bindings);

/**
 * @brief Load theme settings from a JSON file
 *
 * Loads theme settings into the provided @c config_theme_s structure
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
 * @see @c config_theme_s
 */
int config_load_theme(const char *filename,
        struct config_theme_s *config_theme);


#endif  /* ! CONFIG_H */
