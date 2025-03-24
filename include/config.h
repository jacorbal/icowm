/**
 * @file config.h
 *
 * @brief Configuration structures and procedures declaration
 *
 * The default configuration is taken from the configuration files on
 * the default configuration base directory.  This directory depends on
 * the environment variables: @e XDG_CONFIG_HOME/ICOWM_NAME_PROG if the
 * variable @c XDG_CONFIG_HOME is set, otherwise it will default to the
 * classic @e HOME/.ICOWM_NAME_PROG.
 */

#ifndef CONFIG_H
#define CONFIG_H


/* System includes */
#include <stdbool.h>    /* bool */


// TODO: Put this in another file
#define CONFIG_MAX_LENGTH_COMMAND (128)
#define CONFIG_MAX_LENGTH_BINDING (128)
#define CONFIG_MAX_LENGTH_OPTION (40)
#define CONFIG_MAX_LENGTH_FONTNAME (80)

#define CONFIG_MAX_LENGTH_NAME (256)
#define CONFIG_MAX_LENGHT_FILENAME (256)
#define CONFIG_MAX_LENGTH_PATH_BASE (1024)
#define CONFIG_MAX_LENGTH_PATH_CONFIG \
    (CONFIG_MAX_LENGTH_PATH_BASE + CONFIG_MAX_LENGHT_FILENAME)

#define CONFIG_DIR_BASE  "icowm"
#define CONFIG_DIR_THEMES "themes"
#define CONFIG_FILENAME_BASE "config.json"
#define CONFIG_FILENAME_BINDINGS "bindings.json"

#define CONFIG_MAX_SCREENS (4)      /* Initial number of screens */
#define CONFIG_MAX_DESKTOPS (10)    /* Initial desktops per screen */


/**
 * @brief Base settings configuration structure
 */
struct config_base_s {
    char theme[CONFIG_MAX_LENGHT_FILENAME];

    /* Desktops: number and which on is the default one */
    unsigned int screen_count;              /**< Number of screens */
    struct screens_s {
        unsigned int desktop_count;         /**< Desktop count in screen */
        unsigned int desktop_inaugural;     /**< Initial desktop */

        struct desktops_s {
            char name[CONFIG_MAX_LENGTH_NAME];     /**< Desktop name */
            struct desktop_settings_s {
                union {
                    //Pixmap image;           /**< Background image*/
                    unsigned long int color;            /**< Background color */
                } background;
            } settings;                     /**< Desktop settings */
        } desktops[CONFIG_MAX_DESKTOPS];    /**< Desktops per screen */
    } screens[CONFIG_MAX_SCREENS];          /**< All screens */

    /* Basic main programs: terminal and program launcher */
    struct programs_s {
        char terminal[CONFIG_MAX_LENGTH_COMMAND];
        char launcher[CONFIG_MAX_LENGTH_COMMAND];
        char file_manager[CONFIG_MAX_LENGTH_COMMAND];
        char web_browser[CONFIG_MAX_LENGTH_COMMAND];
        char editor[CONFIG_MAX_LENGTH_COMMAND];
    } programs;

    /* General behavior of environment towards windows */
    struct windows_s {
        unsigned int snap;
        struct {
            bool is_new_focused;
            bool is_raised_on_focus;
        } focus;
        struct {
            char policy[CONFIG_MAX_LENGTH_OPTION];
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
        char close[CONFIG_MAX_LENGTH_BINDING];
        char kill[CONFIG_MAX_LENGTH_BINDING];
        char info[CONFIG_MAX_LENGTH_BINDING];
        char cycle_prev[CONFIG_MAX_LENGTH_BINDING];
        char cycle_next[CONFIG_MAX_LENGTH_BINDING];

        /* Window movement, absolute and relative positions */
        struct move_s {
            struct relative_s {
                char right[CONFIG_MAX_LENGTH_BINDING];
                char left[CONFIG_MAX_LENGTH_BINDING];
                char up[CONFIG_MAX_LENGTH_BINDING];
                char down[CONFIG_MAX_LENGTH_BINDING];
            } relative;

            struct absolute_s {
                char top_left[CONFIG_MAX_LENGTH_BINDING];
                char top_right[CONFIG_MAX_LENGTH_BINDING];
                char bottom_left[CONFIG_MAX_LENGTH_BINDING];
                char bottom_right[CONFIG_MAX_LENGTH_BINDING];
            } absolute;
        } move;

        struct resize_s {
            char right[CONFIG_MAX_LENGTH_BINDING];
            char left[CONFIG_MAX_LENGTH_BINDING];
            char up[CONFIG_MAX_LENGTH_BINDING];
            char down[CONFIG_MAX_LENGTH_BINDING];
        } resize;

        struct desktop_s {
            char cycle_prev[CONFIG_MAX_LENGTH_BINDING];
            char cycle_next[CONFIG_MAX_LENGTH_BINDING];
        } desktop;
    } keyboard;

    /* Mouse bindings */
    struct mouse_s {
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
    struct window_theme {
        struct general_s {
            unsigned int border_width;
            bool is_decorated;
        } general;

        struct active_s {
            unsigned long int background_color;
            unsigned long int foreground_color;
            unsigned long int frame_color;
            char font[CONFIG_MAX_LENGTH_FONTNAME];
        } active;

        struct inactive_s {
            unsigned long int background_color;
            unsigned long int foreground_color;
            unsigned long int frame_color;
            char font[CONFIG_MAX_LENGTH_FONTNAME];
        } inactive;
    } window;

    /* Icons theme when iconized programs */
    struct icon_s {
        unsigned long int background_color;
        unsigned long int foreground_color;
        unsigned long int frame_color;
        unsigned int border_width;
        bool is_captioned;
        char font[CONFIG_MAX_LENGTH_FONTNAME];
    } icon;
};


/**
 * @brief Main configuration structure
 *
 * This structure encapsulates the main configuration, including base
 * settings, bindings, and theme.
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
 * Allocates memory for a new @e config_td structure and initializes its
 * fields to default values.
 *
 * @return Pointer to the initialized configuration structure, or @c NULL
 *         on failure
 *
 * @note Complexity: @e O(1), as it only involves memory allocation and
 *       initialization
 *
 * @see config_td
 */
config_td *config_init(void);

/**
 * @brief Destroy a configuration structure and free resources
 *
 * Frees the memory associated with a @e config_td structure.
 *
 * @param config Pointer to the configuration structure to destroy
 *
 * @note Complexity: @e O(1), as it only involves freeing memory
 *
 * @see config_td
 */
void config_destroy(config_td *config);

/**
 * @brief Populate the configuration structure with default values
 *
 * Sets default values for all fields in the given @e config_td
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
 * Loads configuration settings into the provided @e config_td structure
 * from predefined sources (configuration files) by invoking the
 * functions @e config_load_base, @e config_load_bindings and
 * @e config_load_theme.
 *
 * @param config Pointer to the configuration structure where to load
 *               the data
 *
 * @return 0 on success, or otherwise on error
 *
 * @note This function does not take into account default values,
 *       because it's invoked after calling @e config_set_default_values
 * @note Complexity: @e O(n), where @e n is the number of parameters
 *       loaded; this may involve reading from a file or similar
 *       operations
 *
 * @see config_load_base, config_load_bindings, config_load_theme
 */
int config_load(config_td *config);

/**
 * @brief Load base configuration settings from a JSON file
 *
 * Loads base configuration settings into the provided @e config_base_s
 * structure from the specified file.
 *
 * @param filename    The path to the configuration file
 * @param config_base Pointer to the base configuration structure to
 *                    populate
 *
 * @return 0 on success, or otherwise on error.
 *
 * @note Complexity: @e O(n), where @e n is the size of the
 *       configuration file being read
 *
 * see config_base_s
 */
int config_load_base(const char *filename,
        struct config_base_s *config_base);

/**
 * @brief Load key bindings from a JSON file
 *
 * Loads key bindings into the provided @e config_bindings_s structure
 * from the specified file.
 *
 * @param filename        Path to the key bindings configuration file
 * @param config_bindings Pointer to the bindings configuration
 *                        structure to populate
 *
 * @return 0 on success, or otherwise on error
 *
 * @note Complexity: @e O(n), where @e n is the size of the key bindings
 *       file being read
 *
 * @see config_bindings_s
 */
int config_load_bindings(const char *filename,
        struct config_bindings_s *config_bindings);

/**
 * @brief Load theme settings from a JSON file
 *
 * Loads theme settings into the provided @e config_theme_s structure
 * from the specified file.
 *
 * @param filename     The path to the theme configuration file.
 * @param config_theme Pointer to the theme configuration structure to
 *                     populate
 *
 * @return 0 on success, or otherwise on error
 *
 * @note Complexity: @e O(n), where @e n is the size of the theme file
 *       being read
 *
 * @see config_theme_s
 */
int config_load_theme(const char *filename,
        struct config_theme_s *config_theme);


#endif  /* ! CONFIG_H */
