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
#define MAX_COMMAND_LENGTH (128)
#define MAX_KEYBINDING_LENGTH (128)
#define MAX_OPTION_LENGTH (40)
#define MAX_FONTNAME_LENGTH (80)

#define MAX_FILENAME_LENGTH (256)
#define MAX_PATH_BASE_LENGTH (1024)
#define MAX_PATH_CONFIG_LENGTH \
    (MAX_PATH_BASE_LENGTH + MAX_FILENAME_LENGTH)

#define CONFIG_DIR_BASE  "icowm"
#define CONFIG_DIR_THEMES "themes"
#define CONFIG_FILENAME_BASE "config.json"
#define CONFIG_FILENAME_BINDINGS "bindings.json"

#define COLOR unsigned long int
#define CONFIG_MAX_SCREENS (10)     /* Maximum screens */
#define CONFIG_MAX_DESKTOPS (10)    /* Maximum desktops per screen */


/**
 * @brief Base settings configuration structure
 */
struct config_base_s {
    char theme[MAX_FILENAME_LENGTH];

    /* Desktops: number and which on is the default one */
    unsigned int screen_count;              /**< Number of screens */
    struct screens_s {
        unsigned int desktop_count;         /**< Desktop count in screen */
        unsigned int desktop_inaugural;     /**< Initial desktop */

        struct desktops_s {
            struct desktop_settings_s {
                union {
                    //Pixmap image;           /**< Background image*/
                    COLOR color;            /**< Background color */
                } background;
            } settings;                     /**< Desktop settings */
        } desktops[CONFIG_MAX_DESKTOPS];    /**< Desktops per screen */
    } screens[CONFIG_MAX_SCREENS];          /**< All screens */

    /* Basic main programs: terminal and program launcher */
    struct programs_s {
        char terminal[MAX_COMMAND_LENGTH];
        char launcher[MAX_COMMAND_LENGTH];
        char file_manager[MAX_COMMAND_LENGTH];
        char web_browser[MAX_COMMAND_LENGTH];
        char editor[MAX_COMMAND_LENGTH];
    } programs;

    /* General behavior of environment towards windows */
    struct windows_s {
        unsigned int snap;
        struct {
            bool is_new_focused;
            bool is_raised_on_focus;
        } focus;
        struct {
            char policy[MAX_OPTION_LENGTH];
            bool is_centered;
        } placement;
    } windows;
};


/**
 * @brief Keyboard and mouse bindings configuration structure
 */
struct config_bindings_s {
    /* Modifiers */
    char modc[MAX_KEYBINDING_LENGTH];
    char mods[MAX_KEYBINDING_LENGTH];
    char modl[MAX_KEYBINDING_LENGTH];
    char mod1[MAX_KEYBINDING_LENGTH];
    char mod2[MAX_KEYBINDING_LENGTH];
    char mod3[MAX_KEYBINDING_LENGTH];
    char mod4[MAX_KEYBINDING_LENGTH];
    char mod5[MAX_KEYBINDING_LENGTH];

    /* Keyboard bindings */
    struct keyboard_s {
        char terminal[MAX_KEYBINDING_LENGTH];
        char launcher[MAX_KEYBINDING_LENGTH];
        char file_manager[MAX_KEYBINDING_LENGTH];
        char web_browser[MAX_KEYBINDING_LENGTH];
        char editor[MAX_KEYBINDING_LENGTH];
        char center[MAX_KEYBINDING_LENGTH];
        char maximize[MAX_KEYBINDING_LENGTH];
        char fullscreen[MAX_KEYBINDING_LENGTH];
        char shade[MAX_KEYBINDING_LENGTH];
        char pin[MAX_KEYBINDING_LENGTH];
        char iconify[MAX_KEYBINDING_LENGTH];
        char close[MAX_KEYBINDING_LENGTH];
        char kill[MAX_KEYBINDING_LENGTH];
        char info[MAX_KEYBINDING_LENGTH];
        char cycle_prev[MAX_KEYBINDING_LENGTH];
        char cycle_next[MAX_KEYBINDING_LENGTH];

        /* Window movement, absolute and relative positions */
        struct move_s {
            struct relative_s {
                char right[MAX_KEYBINDING_LENGTH];
                char left[MAX_KEYBINDING_LENGTH];
                char up[MAX_KEYBINDING_LENGTH];
                char down[MAX_KEYBINDING_LENGTH];
            } relative;

            struct absolute_s {
                char top_left[MAX_KEYBINDING_LENGTH];
                char top_right[MAX_KEYBINDING_LENGTH];
                char bottom_left[MAX_KEYBINDING_LENGTH];
                char bottom_right[MAX_KEYBINDING_LENGTH];
            } absolute;
        } move;

        struct resize_s {
            char right[MAX_KEYBINDING_LENGTH];
            char left[MAX_KEYBINDING_LENGTH];
            char up[MAX_KEYBINDING_LENGTH];
            char down[MAX_KEYBINDING_LENGTH];
        } resize;

        struct desktop_s {
            char cycle_prev[MAX_KEYBINDING_LENGTH];
            char cycle_next[MAX_KEYBINDING_LENGTH];
        } desktop;
    } keyboard;

    /* Mouse bindings */
    struct mouse_s {
        char move[MAX_KEYBINDING_LENGTH];
        char resize[MAX_KEYBINDING_LENGTH];
        char lower[MAX_KEYBINDING_LENGTH];
        struct {
            char cycle_prev[MAX_KEYBINDING_LENGTH];
            char cycle_next[MAX_KEYBINDING_LENGTH];
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
            COLOR background_color;
            COLOR foreground_color;
            COLOR frame_color;
            char font[MAX_FONTNAME_LENGTH];
        } active;

        struct inactive_s {
            COLOR background_color;
            COLOR foreground_color;
            COLOR frame_color;
            char font[MAX_FONTNAME_LENGTH];
        } inactive;
    } window;

    /* Icons theme when iconized programs */
    struct icon_s {
        COLOR background_color;
        COLOR foreground_color;
        COLOR frame_color;
        unsigned int border_width;
        bool is_captioned;
        char font[MAX_FONTNAME_LENGTH];
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
