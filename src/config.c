/**
 * @file config.h
 *
 * @brief Configuration structures and procedures implementation
 *
 * The configuration data is compiled in several JSON files and the
 * structure is populated by retrieving their contents.
 */
/*
 * NOTE(S):
 *
 * Regarding the forthcoming extant self (Sat Mar 22 05:01 CET 2025):
 *      The current state of this code is significantly suboptimal.
 *      I implore you to initiate refactoring at your earliest
 *      convenience, or at a time that is deemed more suitable.
 *
 * Regarding the whilom expired self (Sun Mar 23 06:12 CET 2025):
 *      Should've done it correctly from the get-go and avoided future
 *      headaches: *my* present headaches.
 */

/* System includes */
#include <stdbool.h>    /* bool, false, true */
#include <stdio.h>      /* FILE, snprintf */
#include <stdlib.h>     /* NULL, free, malloc, getenv, size_t */
#include <string.h>     /* strncpy, strlen */

/* External libraries */
#include <cjson/cJSON.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/**
 * @brief Convert a hexadecimal color string into a unsigned long
 *        integer
 *
 * @param hex_color Hexadecimal color string
 *
 * @return Color value as unsigned long integer
 *
 * @note The initial @p hex_color string could begin with character '#',
 *       for it's ignored
 * @note Complexity: @e O(n), where @e n is the length of the
 *       hexadecimal string
 */
static unsigned long int _hex2ul(const char *hex_color)
{
    unsigned long int color;

    if (hex_color[0] == '#') {
        hex_color++;
    }
    sscanf(hex_color, "%lx", &color);

    return color;
}


/**
 * @brief Copy a string safely into a destination buffer
 *
 * This function copies a string from source to destination ensuring
 * that the destination buffer does not overflow.  It null-terminates
 * the destination string.
 *
 * @param dest Pointer to the destination buffer
 * @param src  Pointer to the source string
 * @param size Maximum number of characters to copy, including the null
 *             terminator
 *
 * @note If the length of the source string exceeds @e size, the
 *       destination will be truncated. It will always be
 *       null-terminated
 *
 * @note Complexity: @e O(n), where @e n is the length of the source
 *       string or the specified size, whichever is smaller
 */
static void safe_strncpy(char *dest, const char *src, size_t size)
{
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
}


/**
 * @brief Load a string value from a JSON object into destination buffer
 *
 * Extracts a specified field from a JSON object and copies its value
 * into the destination buffer.  If the field is not found, the
 * destination buffer remains unchanged.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field The name of the field to extract
 * @param dest  Pointer to the destination buffer where value is copied
 * @param size  Maximum number of characters to copy including terminator
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to load string data
 *
 * @note The function ensures that the destination buffer does not
 *       overflow and is properly null-terminated.
 *
 * @note Complexity: @e O(1) for the search and @e O(m) for copying,
 *       where @e m is the length of the string being copied
 */
static int _json_load_string(cJSON *json, const char *field, char *dest,
        size_t size)
{
    cJSON *item;

    item = cJSON_GetObjectItem(json, field);
    if (item && cJSON_IsString(item)) {
        safe_strncpy(dest, item->valuestring, size);
        return 0;
    }

    LOGGER_NOTICE("Failed to load JSON string: '%s'", field);
    return 1;
}


/**
 * @brief Load unsigned integer value from a JSON object into an unsigned
 *        integer pointer
 *
 * Extracts a specified field from a JSON object and stores its unsigned
 * integer value in the provided destination pointer.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field The name of the field to extract
 * @param dest  Pointer to an unsigned integer where the value is stored
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to load unsigned int data
 *
 * @note If the field is absent or the value cannot be converted to an
 *       unsigned integer, the destination value remains unchanged.
 *
 * @note Complexity: @e O(1) for accessing the field and potential
 *       parsing cost
 */
static int _json_load_uint(cJSON *json, const char *field,
        unsigned int *dest)
{
    cJSON *item = cJSON_GetObjectItem(json, field);

    if (item && cJSON_IsNumber(item)) {
        *dest = (unsigned int) item->valueint;
        return 0;
    }

    LOGGER_NOTICE("Failed to load JSON unsigned integer: '%s'",
            field);
    return 1;
}


/**
 * @brief Load boolean value from a JSON object into a boolean pointer
 *
 * Extracts a specified field from a JSON object and stores its boolean
 * value in the provided destination pointer.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field The name of the field to extract
 * @param dest  Pointer to a boolean where the value will be stored
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to load boolean data
 *
 * @note If the field is absent or the value cannot be interpreted as
 *       a boolean, the destination value remains unchanged
 *
 * @note Complexity: @e O(1) for accessing the field and potential
 *       parsing cost
 */
static int _json_load_bool(cJSON *json, const char *field, bool *dest)
{
    cJSON *item;

    item = cJSON_GetObjectItem(json, field);
    if (item && cJSON_IsBool(item)) {
        *dest = cJSON_IsTrue(item);
        return 0;
    }

    LOGGER_NOTICE("Failed to load JSON boolean: '%s'", field);
    return 1;
}


/**
 * @brief Load JSON data from a file into a string
 *
 * Reads the contents of a specified file and stores it in a dynamically
 * allocated string.  The caller is responsible for freeing the memory
 * allocated for the string.
 *
 * @param filename The path to the file containing JSON data
 * @param data     Pointer to a string where the loaded data is stored
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 File not found or empty file
 * @retval  2 Failed to allocate memory
 *
 * @note This function checks if the file can be opened and reads the
 *       entire contents into memory. If an error occurs during file
 *       operations or memory allocation, it will return a non-zero
 *       value
 *
 * @note Complexity: @e O(n), where @e n is the size of the file
 */
static int _json_load_from_file(const char *filename, char **data)
{
    FILE *file;

    LOGGER_INFO("Parsing data from file: '%s'", filename);

    LOGGER_TRACE("Opening JSON file: '%s'", filename);
    file = fopen(filename, "r");
    if (!file) {
        LOGGER_NOTICE("File not found or unable to open:" \
                " '%s'; default values will be used", filename);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t length = (size_t) ftell(file);
    if (length == 0) {
        fclose(file);
        LOGGER_NOTICE("File is empty:" \
                " '%s'; default values will be used", filename);
        return 1;
    }
    fseek(file, 0, SEEK_SET);

    *data = malloc(length + 1);
    if (*data == NULL) {
        LOGGER_ERROR("Failed to allocate memory for file: '%s'",
                filename);
        fclose(file);
        return 2;
    }

    LOGGER_TRACE("Reading JSON file: '%s'", filename);
    fread(*data, 1, length, file);

    /* Make sure data is null-terminated */
    (*data)[length] = '\0';

    LOGGER_TRACE("Closing JSON file: '%s'", filename);
    fclose(file);

    return 0;
}


/**
 * @brief Load the configuration from a JSON file into a @c cJSON object
 *
 * @param filename The path to the file to be loaded
 * @param json_out Pointer to pointer that will hold the @c cJSON object
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Error loading file
 * @retval  2 Error parsing data
 */
static int _json_load_config(const char *filename, cJSON **json_out)
{
    char *data;

    if (_json_load_from_file(filename, &data) != 0) {
        return 1;
    }

    cJSON *json = cJSON_Parse(data);
    if (json == NULL) {
        LOGGER_WARNING("Failed to parse file:" \
                " '%s'; default configuration will be used",
                filename);
        LOGGER_TRACE("Error parsing JSON file:\n%s",
                cJSON_GetErrorPtr());
        free(data);
        return 2;
    }

    free(data);
    *json_out = json;
    return 0;
}


/**
 * @brief Set the configuration directory base
 *
 * Set the base path for the configuration directory based on the
 * environment variables @e XDG_CONFIG_HOME and @e HOME.  If neither is
 * set, it defaults to the current working directory.  The resulting
 * path is stored in the provided buffer @p config_dir_base.
 *
 * @param config_dir_base Pointer to a character array where the
 *                        configuration directory path will be stored
 *
 * @note The buffer should be at least @c MAX_PATH_BASE_LENGTH
 */
static void _config_dir_set(char *config_dir_base)
{
    const char *config_xdg_config_home = getenv("XDG_CONFIG_HOME");
    const char *config_home = getenv("HOME");

    if (config_xdg_config_home != NULL) {
        /* XDG_CONFIG_HOME/icowm */
        snprintf(config_dir_base, MAX_PATH_BASE_LENGTH, "%s%s",
                config_xdg_config_home, CONFIG_DIR_BASE);
    } else if (config_home != NULL) {
        /* HOME/.icowm */
        snprintf(config_dir_base, MAX_PATH_BASE_LENGTH, "%s/.%s",
                config_home, CONFIG_DIR_BASE);
    } else {
        /* $(pwd)/.icowm */
        snprintf(config_dir_base, MAX_PATH_BASE_LENGTH, "./%s",
                CONFIG_DIR_BASE);
    }
}


/* Initialize a new configuration structure */
config_td *config_init(void)
{
    config_td *config;

    LOGGER_DEBUG("Initializing configuration structure", L_NARG);

    config = malloc(sizeof(config_td));
    if (config == NULL) {
        LOGGER_ERROR("Failed to allocate memory for configuration" \
                " structure", L_NARG);
        return NULL;
    }

    LOGGER_DEBUG("Setting configuration to default values", L_NARG);
    config_set_default_values(config);

    return config;
}


/* Destroy a configuration structure and free resources */
void config_destroy(config_td *config)
{
    LOGGER_DEBUG("Destroying configuration structure", L_NARG);
    free(config);
}


/* Populate the configuration structure with default values */
void config_set_default_values(config_td *config)
{
    /* Assign predetermined values for base configuration */
    config->base.screen_count = 1;

    for (unsigned int i = 0; i < config->base.screen_count; ++i) {
        /* Set number of desktops per screen */
        config->base.screens[i].desktop_count = CONFIG_MAX_DESKTOPS;
        config->base.screens[i].desktop_inaugural = 0;

        /* All desktop settings */
        for (unsigned int j = 0;
             j < config->base.screens[i].desktop_count;
             ++j) {
            config->base.screens[i].desktops[j].settings.background.color
                = _hex2ul("#000000");
        }
    }

    strcpy(config->base.programs.terminal, "xterm");
    strcpy(config->base.programs.launcher, "gmrun");
    strcpy(config->base.programs.file_manager, "spacefm");
    strcpy(config->base.programs.web_browser, "firefox");
    strcpy(config->base.programs.web_browser, "gvim");
    config->base.windows.snap = 4;
    config->base.windows.focus.is_new_focused = true;
    config->base.windows.focus.is_raised_on_focus = false;
    strcpy(config->base.windows.placement.policy, "smart");
    config->base.windows.placement.is_centered = false;

    /* Assign predetermined values for bindings modifiers */
    strcpy(config->bindings.modc, "Control");
    strcpy(config->bindings.mods, "Shift");
    strcpy(config->bindings.modl, "Caps_Lock");
    strcpy(config->bindings.mod1, "Alt");
    strcpy(config->bindings.mod2, "Num_Lock");
    strcpy(config->bindings.mod3, "");
    strcpy(config->bindings.mod4, "Super");
    strcpy(config->bindings.mod5, "Hyper");

    /* Predetermined configuration for keybindings */
    strcpy(config->bindings.keyboard.terminal, "modc+mod1+Return");
    strcpy(config->bindings.keyboard.launcher, "modc+mod1+r");
    strcpy(config->bindings.keyboard.file_manager, "modc+mod1+q");
    strcpy(config->bindings.keyboard.web_browser, "modc+mod1+w");
    strcpy(config->bindings.keyboard.editor, "modc+mod1+e");
    strcpy(config->bindings.keyboard.center, "modc+mod1+g");
    strcpy(config->bindings.keyboard.maximize, "modc+mod1+m");
    strcpy(config->bindings.keyboard.fullscreen, "modc+mod1+f");
    strcpy(config->bindings.keyboard.shade, "modc+mod1+s");
    strcpy(config->bindings.keyboard.pin, "modc+mod1+p");
    strcpy(config->bindings.keyboard.iconify, "modc+mod1+i");
    strcpy(config->bindings.keyboard.close, "modc+mod1+c");
    strcpy(config->bindings.keyboard.kill, "modc+mod1+mods+Escape");
    strcpy(config->bindings.keyboard.info, "modc+mod1+mods+i");
    strcpy(config->bindings.keyboard.cycle_prev, "mod1+mods+Tab");
    strcpy(config->bindings.keyboard.cycle_next, "mod1+Tab");

    /* Predetermined configuration for movement with keyboard */
    strcpy(config->bindings.keyboard.move.relative.right,
            "modc+mod1+l");
    strcpy(config->bindings.keyboard.move.relative.left,
            "modc+mod1+h");
    strcpy(config->bindings.keyboard.move.relative.up,
            "modc+mod1+k");
    strcpy(config->bindings.keyboard.move.relative.down,
            "modc+mod1+j");
    strcpy(config->bindings.keyboard.move.absolute.top_left,
            "modc+mod1+y");
    strcpy(config->bindings.keyboard.move.absolute.top_right,
            "modc+mod1+u");
    strcpy(config->bindings.keyboard.move.absolute.bottom_left,
            "modc+mod1+b");
    strcpy(config->bindings.keyboard.move.absolute.bottom_right,
            "modc+mod1+n");
    strcpy(config->bindings.keyboard.resize.right,
            "modc+mod1+mods+l");
    strcpy(config->bindings.keyboard.resize.left,
            "modc+mod1+mods+h");
    strcpy(config->bindings.keyboard.resize.up,
            "modc+mod1+mods+k");
    strcpy(config->bindings.keyboard.resize.down,
            "modc+mod1+mods+j");
    strcpy(config->bindings.keyboard.desktop.cycle_prev,
            "modc+mod1+Left");
    strcpy(config->bindings.keyboard.desktop.cycle_next,
            "modc+mod1+Right");

    /* Predetermined configuration for mouse bindings */
    strcpy(config->bindings.mouse.move, "button1");
    strcpy(config->bindings.mouse.resize, "button2");
    strcpy(config->bindings.mouse.lower, "button3");
    strcpy(config->bindings.mouse.desktop.cycle_prev, "button4");
    strcpy(config->bindings.mouse.desktop.cycle_next, "button5");

    /* Predetermined values for a default theme */
    strcpy(config->theme.name, "Default (builtin)");
    config->theme.window.general.border_width = 2;
    config->theme.window.general.is_decorated = true;
    config->theme.window.active.background_color = _hex2ul("FFFFFF");
    config->theme.window.active.foreground_color = _hex2ul("000000");
    config->theme.window.active.frame_color = _hex2ul("222222");
    strcpy(config->theme.window.active.font, "monospace bold 9");
    config->theme.window.inactive.background_color = _hex2ul("000000");
    config->theme.window.inactive.foreground_color = _hex2ul("FFFFFF");
    config->theme.window.inactive.frame_color = _hex2ul("999999");
    strcpy(config->theme.window.inactive.font, "monospace 9");
    config->theme.icon.background_color = _hex2ul("FFFFFF");
    config->theme.icon.foreground_color = _hex2ul("000000");
    config->theme.icon.frame_color = _hex2ul("000000");
    config->theme.icon.border_width = 1;
    config->theme.icon.is_captioned = true;
    strcpy(config->theme.icon.font, "monospace 8");
}


/* Load all the configuration */
int config_load(config_td *config)
{
    char config_dir[MAX_PATH_BASE_LENGTH];
    char config_base_file[MAX_PATH_CONFIG_LENGTH];
    char config_bindings_file[MAX_PATH_CONFIG_LENGTH];
    char config_theme_file[MAX_PATH_CONFIG_LENGTH];

    _config_dir_set(config_dir);

    LOGGER_DEBUG("Loading configuration from files on: '%s'",
            config_dir);

    /* Set main base configuration path */
    snprintf(config_base_file, sizeof(config_base_file),
            "%s/%s", config_dir, CONFIG_FILENAME_BASE);

    /* Load base configuration */
    if (config_load_base(config_base_file, &(config->base)) != 0) {
        LOGGER_NOTICE("Base configuration could not be loaded;" \
                " default values will be used", L_NARG);
        return 1;
    }

    /* Set bindings configuration path */
    snprintf(config_bindings_file, sizeof(config_bindings_file),
            "%s/%s", config_dir, CONFIG_FILENAME_BINDINGS);

    /* Load bindings */
    if (config_load_bindings(config_bindings_file,
                &(config->bindings)) != 0) {
        LOGGER_ERROR("Failed to load bindings from:" \
                " '%s'; default bindings will be used",
                config_bindings_file);
    }

    /* Set theme file path */
    snprintf(config_theme_file, sizeof(config_theme_file),
            "%s/%s/%s.json", config_dir, CONFIG_DIR_THEMES,
            config->base.theme);

    /* Load theme if it's specified, i.e., not empty string */
    if (strlen(config->base.theme) == 0) {
        LOGGER_NOTICE("No theme specified in base configuration;" \
                " default will be used", L_NARG);
    } else {
        if (config_load_theme(config_theme_file,
                    &(config->theme)) != 0) {
            LOGGER_NOTICE("Failed to load theme from:" \
                    " '%s'; default theme will be used",
                    config_theme_file);
        }
    }

    return 0;
}


/* Load base configuration */
int config_load_base(const char *filename,
        struct config_base_s *config_base)
{
    cJSON *json;

    LOGGER_TRACE("Preparing to parse base configuration from file:" \
            " '%s'", filename);

    /* Load file, or exit */
    if (_json_load_config(filename, &json) != 0) {
        return 1;
    }

    /* Theme name*/
    if (_json_load_string(json, "theme", config_base->theme,
                MAX_FILENAME_LENGTH) != 0) {
        LOGGER_WARNING("Invalid theme specified:" \
                " '%s'; default configuration will be used",
                config_base->theme);
        config_base->theme[0] = '\0';
    }

    cJSON *screen_settings = cJSON_GetObjectItem(json, "screens");
    if (screen_settings) {
        /* Load total number of screen */
        _json_load_uint(screen_settings, "count",
                &config_base->screen_count);

        /* Get 'desktop' array inside 'settings' */
        cJSON *settings =
            cJSON_GetObjectItem(screen_settings, "settings");
        cJSON *desktops_array =
            cJSON_GetObjectItem(settings, "desktops");

        /* NOTE.  While I recognize this maze of if statements could
         *        benefit from finesse, I am stuck with it for now.
         *        A sophisticated refactor will come, but deadlines have
         *        a way of complicating matters. */
        if (desktops_array && cJSON_IsArray(desktops_array)) {
            unsigned int desktop_count =
                (unsigned int) cJSON_GetArraySize(desktops_array);
            for (unsigned int i = 0;
                 i < desktop_count && i < CONFIG_MAX_DESKTOPS;
                 ++i) {
                cJSON *desktop_item =
                    cJSON_GetArrayItem(desktops_array, (int) i);

                if (desktop_item) {
                    /* Load desktop 'count' and 'inaugural' */
                    _json_load_uint(desktop_item, "count",
                            &config_base->screens[i].desktop_count);
                    _json_load_uint(desktop_item, "inaugural",
                            &config_base->screens[i].desktop_inaugural);

                    /* Desktops, as screens, are zero-based indexed, so
                     * if the inaugural desktop is a number bigger than
                     * the desktop, it reverts to the first desktop of
                     * all: the 0th */
                    if (config_base->screens[i].desktop_inaugural >
                        config_base->screens[i].desktop_count - 1) {
                        config_base->screens[i].desktop_inaugural = 0;
                    }

                    /* Get 'settings' field for each desktop */
                    cJSON *desktop_settings =
                        cJSON_GetObjectItem(desktop_item, "settings");
                    if (desktop_settings &&
                            cJSON_IsArray(desktop_settings)) {
                        unsigned int settings_count =
                            (unsigned int)
                                cJSON_GetArraySize(desktop_settings);
                        for (unsigned int j = 0;
                                j < settings_count &&
                                j < CONFIG_MAX_DESKTOPS;
                             ++j) {
                            cJSON *setting_item =
                                cJSON_GetArrayItem(desktop_settings,
                                        (int) j);
                            if (setting_item) {
                                cJSON *background_color_item =
                                    cJSON_GetObjectItem(setting_item,
                                            "background_color");
                                if (background_color_item &&
                                        cJSON_IsString(background_color_item)) {
                                    config_base->screens[i].desktops[j].settings.background.color =
                                        _hex2ul(background_color_item->valuestring);
                                } /* ! if (background_color) */
                            } /* ! if (setting_item) */
                        } /* ! for(j in 0..settings_count) */
                    } /* ! if (desktop_settings) */
                } /* ! if (desktop_item) */
            } /* ! for (i in 0..desktop_count) */
        } /* ! if (desktops_array) */
    } /* ! if (screen_settings) */

    /* Load default programs */
    cJSON *programs = cJSON_GetObjectItem(json, "programs");
    if (programs) {
        _json_load_string(programs, "terminal",
                config_base->programs.terminal, MAX_COMMAND_LENGTH);
        _json_load_string(programs, "launcher",
                config_base->programs.launcher, MAX_COMMAND_LENGTH);
        _json_load_string(programs, "file_manager",
                config_base->programs.file_manager, MAX_COMMAND_LENGTH);
        _json_load_string(programs, "web_browser",
                config_base->programs.web_browser, MAX_COMMAND_LENGTH);
        _json_load_string(programs, "editor",
                config_base->programs.editor, MAX_COMMAND_LENGTH);
    }

    /* Load window base configuration */
    cJSON *windows = cJSON_GetObjectItem(json, "windows");
    if (windows) {
        _json_load_uint(windows, "snap", &config_base->windows.snap);
        cJSON *focus = cJSON_GetObjectItem(windows, "focus");
        if (focus) {
            _json_load_bool(focus, "is_new_focused",
                    &config_base->windows.focus.is_new_focused);
            _json_load_bool(focus, "is_raised_on_focus",
                    &config_base->windows.focus.is_raised_on_focus);
        }
        cJSON *placement = cJSON_GetObjectItem(windows, "placement");
        if (placement) {
            _json_load_string(placement, "policy",
                    config_base->windows.placement.policy,
                    MAX_OPTION_LENGTH);
            _json_load_bool(placement, "is_centered",
                    &config_base->windows.placement.is_centered);
        }
    }

    /* Free memory */
    cJSON_Delete(json);
    return 0;
}


/* Load bindings configuration */
int config_load_bindings(const char *filename,
        struct config_bindings_s *config_bindings)
{
    cJSON *json;

    LOGGER_TRACE("Parsing bindings configuration from file: '%s'",
            filename);

    /* Load file or exit */
    if (_json_load_config(filename, &json) != 0) {
        return 1;
    }

    /* Load keyboard modifiers */
    cJSON *modifiers = cJSON_GetObjectItem(json, "modifiers");
    if (modifiers) {
        _json_load_string(modifiers, "modc", config_bindings->modc,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(modifiers, "mods", config_bindings->mods,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(modifiers, "modl", config_bindings->modl,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(modifiers, "mod1", config_bindings->mod1,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(modifiers, "mod2", config_bindings->mod2,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(modifiers, "mod3", config_bindings->mod3,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(modifiers, "mod4", config_bindings->mod4,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(modifiers, "mod5", config_bindings->mod5,
                MAX_KEYBINDING_LENGTH);
    }

    /* Load keybindings */
    cJSON *keyboard = cJSON_GetObjectItem(json, "keyboard");
    if (keyboard) {
        _json_load_string(keyboard, "terminal",
                config_bindings->keyboard.terminal,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "launcher",
                config_bindings->keyboard.launcher,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "file_manager",
                config_bindings->keyboard.file_manager,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "web_browser",
                config_bindings->keyboard.web_browser,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "editor",
                config_bindings->keyboard.editor,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "center",
                config_bindings->keyboard.center,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "maximize",
                config_bindings->keyboard.maximize,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "fullscreen",
                config_bindings->keyboard.fullscreen,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "shade",
                config_bindings->keyboard.shade,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "pin",
                config_bindings->keyboard.pin,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "iconify",
                config_bindings->keyboard.iconify,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "close",
                config_bindings->keyboard.close,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "kill",
                config_bindings->keyboard.kill,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "info",
                config_bindings->keyboard.info,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "cycle-prev",
                config_bindings->keyboard.cycle_prev,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(keyboard, "cycle-next",
                config_bindings->keyboard.cycle_next,
                MAX_KEYBINDING_LENGTH);

        /* Keybindings for window movement */
        cJSON *move = cJSON_GetObjectItem(keyboard, "move");
        if (move) {
            cJSON *relative = cJSON_GetObjectItem(move, "relative");
            if (relative) {
                _json_load_string(relative, "right",
                        config_bindings->keyboard.move.relative.right,
                        MAX_KEYBINDING_LENGTH);
                _json_load_string(relative, "left",
                        config_bindings->keyboard.move.relative.left,
                        MAX_KEYBINDING_LENGTH);
                _json_load_string(relative, "up",
                        config_bindings->keyboard.move.relative.up,
                        MAX_KEYBINDING_LENGTH);
                _json_load_string(relative, "down",
                        config_bindings->keyboard.move.relative.down,
                        MAX_KEYBINDING_LENGTH);
            }

            cJSON *absolute = cJSON_GetObjectItem(move, "absolute");
            if (absolute) {
                _json_load_string(absolute, "top-left",
                        config_bindings->keyboard.move.absolute.top_left,
                        MAX_KEYBINDING_LENGTH);
                _json_load_string(absolute, "top-right",
                        config_bindings->keyboard.move.absolute.top_right,
                        MAX_KEYBINDING_LENGTH);
                _json_load_string(absolute, "bottom-left",
                        config_bindings->keyboard.move.absolute.bottom_left,
                        MAX_KEYBINDING_LENGTH);
                _json_load_string(absolute, "bottom-right",
                        config_bindings->keyboard.move.absolute.bottom_right,
                        MAX_KEYBINDING_LENGTH);
            }
        }

        /* Keybindings for window resizing */
        cJSON *resize = cJSON_GetObjectItem(keyboard, "resize");
        if (resize) {
            _json_load_string(resize, "right",
                    config_bindings->keyboard.resize.right,
                    MAX_KEYBINDING_LENGTH);
            _json_load_string(resize, "left",
                    config_bindings->keyboard.resize.left,
                    MAX_KEYBINDING_LENGTH);
            _json_load_string(resize, "up",
                    config_bindings->keyboard.resize.up,
                    MAX_KEYBINDING_LENGTH);
            _json_load_string(resize, "down",
                    config_bindings->keyboard.resize.down,
                    MAX_KEYBINDING_LENGTH);
        }

        /* Keybindings for desktop cycling */
        cJSON *desktop = cJSON_GetObjectItem(move, "desktop");
        if (desktop) {
            _json_load_string(desktop, "cycle_prev",
                    config_bindings->keyboard.desktop.cycle_prev,
                    MAX_KEYBINDING_LENGTH);
            _json_load_string(desktop, "cycle_next",
                    config_bindings->keyboard.desktop.cycle_next,
                    MAX_KEYBINDING_LENGTH);
        }
    }

    /* Load mouse bindings */
    cJSON *mouse = cJSON_GetObjectItem(json, "mouse");
    if (mouse) {
        _json_load_string(mouse, "move", config_bindings->mouse.move,
                MAX_KEYBINDING_LENGTH);
        _json_load_string(mouse, "resize",
                config_bindings->mouse.resize, MAX_KEYBINDING_LENGTH);
        _json_load_string(mouse, "lower", config_bindings->mouse.lower,
                MAX_KEYBINDING_LENGTH);

        /* Mouse bindings for desktop cycling */
        cJSON *desktop = cJSON_GetObjectItem(mouse, "desktop");
        if (desktop) {
            _json_load_string(desktop, "cycle_prev",
                    config_bindings->mouse.desktop.cycle_prev,
                    MAX_KEYBINDING_LENGTH);
            _json_load_string(desktop, "cycle_next",
                    config_bindings->mouse.desktop.cycle_next,
                    MAX_KEYBINDING_LENGTH);
        }
    }

    /* Free memory */
    cJSON_Delete(json);

    return 0;
}


/* Load theme configuration */
int config_load_theme(const char *filename,
        struct config_theme_s *config_theme)
{
    cJSON *json;

    LOGGER_TRACE("Parsing theme configuration from file: '%s'",
            filename);

    /* Load file or exit */
    if (_json_load_config(filename, &json) != 0) {
        return 1;
    }

    /* Load theme name identifier */
    _json_load_string(json, "name", config_theme->name,
            MAX_FONTNAME_LENGTH);

    /* Load window general appearance */
    cJSON *window = cJSON_GetObjectItem(json, "window");
    if (window) {
        cJSON *general = cJSON_GetObjectItem(window, "general");
        if (general) {
            _json_load_uint(general, "border_width",
                    &config_theme->window.general.border_width);
            _json_load_bool(general, "is_decorated",
                    &config_theme->window.general.is_decorated);
        }

        /* Load active window appearance */
        cJSON *active = cJSON_GetObjectItem(window, "active");
        if (active) {
            cJSON *background_color = cJSON_GetObjectItem(active,
                    "background_color");
            if (background_color) {
                config_theme->window.active.background_color =
                    _hex2ul(background_color->valuestring);
            }
            cJSON *foreground_color = cJSON_GetObjectItem(active,
                    "foreground_color");
            if (foreground_color) {
                config_theme->window.active.foreground_color =
                    _hex2ul(foreground_color->valuestring);
            }
            cJSON *frame_color = cJSON_GetObjectItem(active,
                    "frame_color");
            if (frame_color) {
                config_theme->window.active.frame_color =
                    _hex2ul(frame_color->valuestring);
            }
            _json_load_string(active, "font",
                    config_theme->window.active.font,
                    MAX_FONTNAME_LENGTH); }

        /* Load inactive window appearance */
        cJSON *inactive = cJSON_GetObjectItem(window, "inactive");
        if (inactive) {
            cJSON *background_color = cJSON_GetObjectItem(inactive,
                    "background_color");
            if (background_color) {
                config_theme->window.inactive.background_color =
                    _hex2ul(background_color->valuestring);
            }
            cJSON *foreground_color = cJSON_GetObjectItem(inactive,
                    "foreground_color");
            if (foreground_color) {
                config_theme->window.inactive.foreground_color =
                    _hex2ul(foreground_color->valuestring);
            }
            cJSON *frame_color = cJSON_GetObjectItem(inactive,
                    "frame_color");
            if (frame_color) {
                config_theme->window.inactive.frame_color =
                    _hex2ul(frame_color->valuestring);
            }
            _json_load_string(inactive, "font",
                    config_theme->window.inactive.font,
                    MAX_FONTNAME_LENGTH); }
    }

    /* Load icon appearance when iconizing */
    cJSON *icon = cJSON_GetObjectItem(json, "icon");
    if (icon) {
        cJSON *background_color = cJSON_GetObjectItem(icon,
                "background_color");
        if (background_color) {
            config_theme->icon.background_color =
                _hex2ul(background_color->valuestring);
        }
        cJSON *foreground_color = cJSON_GetObjectItem(icon,
                "foreground_color");
        if (foreground_color) {
            config_theme->icon.foreground_color =
                _hex2ul(foreground_color->valuestring);
        }
        cJSON *frame_color = cJSON_GetObjectItem(icon,
                "frame_color");
        if (frame_color) {
            config_theme->icon.frame_color =
                _hex2ul(frame_color->valuestring);
        }

        _json_load_uint(icon, "border_width",
                &config_theme->icon.border_width);
        _json_load_bool(icon, "is_captioned",
                &config_theme->icon.is_captioned);
        _json_load_string(icon, "font", config_theme->icon.font,
                MAX_FONTNAME_LENGTH);
    }

    /* Free memory */
    cJSON_Delete(json);

    return 0;
}
