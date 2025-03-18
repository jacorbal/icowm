/**
 * @file config.h
 *
 * @brief Configuration structures and procedures implementation
 *
 * The configuration is gather by several JSON files
 */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External libraries includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>

static const int logging_level = LOG_INFO;

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
 * @brief Convert a hexadecimal color string into a unsigned long
 *        integer
 *
 * @param hex_color Hexadecimal color string
 *
 * @return Color value as unsigned long integer
 *
 * @note The initial @e hex_color string could begin with character '#',
 *       for it's ignored
 * @note Complexity: @e O(n), where @e n is the length of the
 *       hexadecimal string
 */
unsigned long _hex2ul(const char *hex_color)
{
    unsigned long color;

    if (hex_color[0] == '#') {
        hex_color++;
    }
    sscanf(hex_color, "%lx", &color);

    return color;
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
 * @return 0 on success, or otherwise on error
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

    logger(stderr, logging_level, LOG_INFO,
            "Opening '%s'...", filename);
    file = fopen(filename, "r");
    if (!file) {
        logger(stderr, logging_level, LOG_NOTICE,
                "Cannot find or open file: '%s'; using default values",
                filename);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t length = (size_t) ftell(file);
    fseek(file, 0, SEEK_SET);

    *data = malloc(length + 1);
    if (*data == NULL) {
        logger(stderr, logging_level, LOG_ERROR,
                "Cannot allocate memory for file '%s'", filename);
        fclose(file);
        return 2;
    }

    fread(*data, 1, length, file);
    (*data)[length] = '\0';
    fclose(file);

    return 0;
}


/**
 * @brief Load a string value from a JSON object into a destination buffer
 *
 * Extracts a specified field from a JSON object and copies its value
 * into the destination buffer.  If the field is not found, the
 * destination buffer remains unchanged.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field The name of the field to extract
 * @param dest  Pointer to the destination buffer where value is copied
 * @param size  Maximum number of characters to copy, including terminator
 *
 * @return 0 on success, or otherwise on error
 *
 * @note The function ensures that the destination buffer does not
 *       overflow and is properly null-terminated.
 *
 * @note Complexity: @e O(1) for the search and @e O(m) for copying,
 *       where @e m is the length of the string being copied
 */
static int _json_load_object(cJSON *json, const char *field, char *dest,
        size_t size)
{
    cJSON *item;

    item = cJSON_GetObjectItem(json, field);
    if (item) {
        safe_strncpy(dest, item->valuestring, size);
    }

    return 0;
}

/**
 * @brief Load an integer value from a JSON object into an integer pointer
 *
 * Extracts a specified field from a JSON object and stores its integer
 * value in the provided destination pointer.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field The name of the field to extract
 * @param dest  Pointer to an integer where the value will be stored
 *
 * @return 0 on success, or otherwise on error
 *
 * @note If the field is absent or the value cannot be converted to an
 *       integer, the destination value remains unchanged
 *
 * @note Complexity: @e O(1) for accessing the field and potential
 *       parsing cost
 */
static int _json_load_int(cJSON *json, const char *field, int *dest)
{
    cJSON *item;

    item = cJSON_GetObjectItem(json, field);
    if (item && cJSON_IsNumber(item)) {
        *dest = item->valueint;
        return 0;
    }

    return 1;
}

/**
 * @brief Load an unsigned integer value from a JSON object into an
 *        unsigned integer pointer
 *
 * Extracts a specified field from a JSON object and stores its unsigned
 * integer value in the provided destination pointer.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field The name of the field to extract
 * @param dest  Pointer to an unsigned integer where the value is stored
 *
 * @return 0 on success, or otherwise on error
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
        *dest = (unsigned int)item->valueint;
        return 0;
    }

    return 1;
}


/**
 * @brief Load a boolean value from a JSON object into a boolean pointer
 *
 * Extracts a specified field from a JSON object and stores its boolean
 * value in the provided destination pointer.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field The name of the field to extract
 * @param dest  Pointer to a boolean where the value will be stored
 *
 * @return 0 on success, non-zero value on error (e.g., field not found
 *         or value is not a boolean)
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

    return 1;
}


/* Initialize a new configuration structure */
config_td *config_init(void)
{
    config_td *config;

    config = malloc(sizeof(config_td));
    if (config == NULL) {
        logger(stderr, logging_level, LOG_ERROR,
                "Cannot assign memory to store configuration");
        return NULL;
    }
    config_set_default_values(config);

    return config;
}


/* Destroy a configuration structure and free resources */
void config_destroy(config_td *config)
{
    free(config);
}


/* Populate the configuration structure with default values */
void config_set_default_values(config_td *config)
{
    /* Assign predetermined values for base configuration */
    strcpy(config->base.theme, "default");    
    config->base.desktops.number = 4;
    config->base.desktops.inaugural = 1;
    strcpy(config->base.programs.terminal, "xterm");
    strcpy(config->base.programs.launcher, "gmrun");
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
    strcpy(config->bindings.keyboard.center, "modc+mod1+g");
    strcpy(config->bindings.keyboard.maximize, "modc+mod1+m");
    strcpy(config->bindings.keyboard.shade, "modc+mod1+s");
    strcpy(config->bindings.keyboard.pin, "modc+mod1+p");
    strcpy(config->bindings.keyboard.iconify, "modc+mod1+i");
    strcpy(config->bindings.keyboard.close, "modc+mod1+c");
    strcpy(config->bindings.keyboard.kill, "modc+mod1+mods+Escape");
    strcpy(config->bindings.keyboard.info, "modc+mod1+mods+i");
    strcpy(config->bindings.keyboard.cycle_prev, "mod1+mods+Tab");
    strcpy(config->bindings.keyboard.cycle_next, "mod1+Tab");

    /* Predetermined configuration for movement with keyboard */
    strcpy(config->bindings.keyboard.move.relative.right, "modc+mod1+l");
    strcpy(config->bindings.keyboard.move.relative.left, "modc+mod1+h");
    strcpy(config->bindings.keyboard.move.relative.up, "modc+mod1+k");
    strcpy(config->bindings.keyboard.move.relative.down, "modc+mod1+j");
    strcpy(config->bindings.keyboard.move.absolute.top_left, "modc+mod1+y");
    strcpy(config->bindings.keyboard.move.absolute.top_right, "modc+mod1+u");
    strcpy(config->bindings.keyboard.move.absolute.bottom_left, "modc+mod1+b");
    strcpy(config->bindings.keyboard.move.absolute.bottom_right, "modc+mod1+n");
    strcpy(config->bindings.keyboard.resize.right, "modc+mod1+mods+l");
    strcpy(config->bindings.keyboard.resize.left, "modc+mod1+mods+h");
    strcpy(config->bindings.keyboard.resize.up, "modc+mod1+mods+k");
    strcpy(config->bindings.keyboard.resize.down, "modc+mod1+mods+j");
    strcpy(config->bindings.keyboard.desktop.cycle_prev, "modc+mod1+Left");
    strcpy(config->bindings.keyboard.desktop.cycle_next, "modc+mod1+Right");

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
    config->theme.icon.border_width = 1;
    config->theme.icon.is_captioned = true;
    strcpy(config->theme.icon.font, "monospace 8");
}


/* Load all the configuration */
int config_load(config_td *config)
{
    char config_base_file[MAX_PATH_LENGTH];
    char config_bindings_file[MAX_PATH_LENGTH];
    char config_theme_file[MAX_PATH_LENGTH];

    /* Build paths */
    snprintf(config_base_file, sizeof(config_base_file), "%s%s",
            CONFIG_DIR, CONFIG_FILENAME_BASE);
    snprintf(config_bindings_file, sizeof(config_bindings_file), "%s%s",
            CONFIG_DIR, CONFIG_FILENAME_BINDINGS);

    /* Load base configuration */
    if (config_load_base(config_base_file, &(config->base)) != 0) {
        logger(stderr, logging_level, LOG_NOTICE,
            "Cannot load base configuration; using default");
        return 1;
    }

    /* Build theme path */
    snprintf(config_theme_file, sizeof(config_theme_file),
            "%s%s%s.json", CONFIG_DIR, CONFIG_DIR_THEMES,
            config->base.theme);

    /* Check theme string is not too long */
    if (strlen(config->base.theme) >
            (MAX_FILENAME_LENGTH - strlen(CONFIG_DIR_THEMES) - 5)) {
        logger(stderr, logging_level, LOG_WARNING,
            "Value for 'config.theme' too long; using default theme");
        config->base.theme[0] = '\0';
//        return 1;
    }

    /* Load bindings */
    if (config_load_bindings(config_bindings_file,
                &(config->bindings)) != 0) {
        logger(stderr, logging_level, LOG_ERROR,
                "Cannot load bindings from '%s'",
                config_bindings_file);
    }

    /* Load theme if it's specified, i.e., not empty string */
    if (strcmp(config->base.theme, "") != 0) {
        FILE *theme_file = fopen(config_theme_file, "r");

        if (theme_file) {
            fclose(theme_file);
            if (config_load_theme(config_theme_file,
                        &(config->theme)) != 0) {
                logger(stderr, logging_level, LOG_NOTICE,
                        "Cannot load theme from '%s'",
                        config_theme_file);
            }
        } else {
            logger(stderr, logging_level, LOG_NOTICE,
                    "Cannot find theme file '%s'; using default",
                    config_theme_file);
            }
    } else {
        logger(stderr, logging_level, LOG_NOTICE,
                "No theme in base configuration; using default");
    }

    return 0;
}


/* Load base configuration */
int config_load_base(const char *filename,
        struct config_base_s *config_base)
{
    char *data;

    /* Load file, or exit */
    if (_json_load_from_file(filename, &data) != 0) {
        return 1;
    }

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        logger(stderr, logging_level, LOG_WARNING,
                "Cannot parser file '%s'; using default configuration", 
                cJSON_GetErrorPtr());
        free(data);
        return 2;
    }

    /* Theme name*/
    _json_load_object(json, "theme", config_base->theme,
            MAX_FILENAME_LENGTH);

    /* Desktop information */
    cJSON *desktops = cJSON_GetObjectItem(json, "desktops");
    if (desktops) {
        _json_load_int(desktops, "number",
                &config_base->desktops.number);
        _json_load_int(desktops, "inaugural",
                &config_base->desktops.inaugural);
    }

    /* Load default programs */
    cJSON *programs = cJSON_GetObjectItem(json, "programs");
    if (programs) {
        _json_load_object(programs, "terminal",
                config_base->programs.terminal, MAX_COMMAND_LENGTH);
        _json_load_object(programs, "launcher",
                config_base->programs.launcher, MAX_COMMAND_LENGTH);
    }

    /* Load window base configuration */
    cJSON *windows = cJSON_GetObjectItem(json, "windows");
    if (windows) {
        _json_load_int(windows, "snap", &config_base->windows.snap);
        cJSON *focus = cJSON_GetObjectItem(windows, "focus");
        if (focus) {
            _json_load_bool(focus, "is_new_focused",
                    &config_base->windows.focus.is_new_focused);
            _json_load_bool(focus, "is_raised_on_focus",
                    &config_base->windows.focus.is_raised_on_focus);
        }
        cJSON *placement = cJSON_GetObjectItem(windows, "placement");
        if (placement) {
            _json_load_object(placement, "policy",
                    config_base->windows.placement.policy,
                    MAX_OPTION_LENGTH);
            _json_load_bool(placement, "is_centered",
                    &config_base->windows.placement.is_centered);
        }
    }

    /* Free memory */
    cJSON_Delete(json);
    free(data);
    return 0;
}



/* Load bindings configuration */
int config_load_bindings(const char *filename,
        struct config_bindings_s *config_bindings)
{
    char *data;

    /* Load file or exit */
    if (_json_load_from_file(filename, &data) != 0) {
        return 1;
    }

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        logger(stderr, logging_level, LOG_WARNING,
                "Cannot parser file '%s'; using default configuration", 
                cJSON_GetErrorPtr());
        free(data);
        return 2;
    }

    /* Load keyboard modifiers */
    cJSON *modifiers = cJSON_GetObjectItem(json, "modifiers");
    if (modifiers) {
        _json_load_object(modifiers, "modc", config_bindings->modc, MAX_KEYBINDING_LENGTH);
        _json_load_object(modifiers, "mods", config_bindings->mods, MAX_KEYBINDING_LENGTH);
        _json_load_object(modifiers, "modl", config_bindings->modl, MAX_KEYBINDING_LENGTH);
        _json_load_object(modifiers, "mod1", config_bindings->mod1, MAX_KEYBINDING_LENGTH);
        _json_load_object(modifiers, "mod2", config_bindings->mod2, MAX_KEYBINDING_LENGTH);
        _json_load_object(modifiers, "mod3", config_bindings->mod3, MAX_KEYBINDING_LENGTH);
        _json_load_object(modifiers, "mod4", config_bindings->mod4, MAX_KEYBINDING_LENGTH);
        _json_load_object(modifiers, "mod5", config_bindings->mod5, MAX_KEYBINDING_LENGTH);
    }

    /* Load keybindings */
    cJSON *keyboard = cJSON_GetObjectItem(json, "keyboard");
    if (keyboard) {
        _json_load_object(keyboard, "terminal", config_bindings->keyboard.terminal, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "launcher", config_bindings->keyboard.launcher, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "center", config_bindings->keyboard.center, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "maximize", config_bindings->keyboard.maximize, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "shade", config_bindings->keyboard.shade, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "pin", config_bindings->keyboard.pin, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "iconify", config_bindings->keyboard.iconify, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "close", config_bindings->keyboard.close, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "kill", config_bindings->keyboard.kill, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "info", config_bindings->keyboard.info, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "cycle-prev", config_bindings->keyboard.cycle_prev, MAX_KEYBINDING_LENGTH);
        _json_load_object(keyboard, "cycle-next", config_bindings->keyboard.cycle_next, MAX_KEYBINDING_LENGTH);

        /* Keybindings for window movement */
        cJSON *move = cJSON_GetObjectItem(keyboard, "move");
        if (move) {
            cJSON *relative = cJSON_GetObjectItem(move, "relative");
            if (relative) {
                _json_load_object(relative, "right", config_bindings->keyboard.move.relative.right, MAX_KEYBINDING_LENGTH);
                _json_load_object(relative, "left", config_bindings->keyboard.move.relative.left, MAX_KEYBINDING_LENGTH);
                _json_load_object(relative, "up", config_bindings->keyboard.move.relative.up, MAX_KEYBINDING_LENGTH);
                _json_load_object(relative, "down", config_bindings->keyboard.move.relative.down, MAX_KEYBINDING_LENGTH);
            }

            cJSON *absolute = cJSON_GetObjectItem(move, "absolute");
            if (absolute) {
                _json_load_object(absolute, "top-left", config_bindings->keyboard.move.absolute.top_left, MAX_KEYBINDING_LENGTH);
                _json_load_object(absolute, "top-right", config_bindings->keyboard.move.absolute.top_right, MAX_KEYBINDING_LENGTH);
                _json_load_object(absolute, "bottom-left", config_bindings->keyboard.move.absolute.bottom_left, MAX_KEYBINDING_LENGTH);
                _json_load_object(absolute, "bottom-right", config_bindings->keyboard.move.absolute.bottom_right, MAX_KEYBINDING_LENGTH);
            }
        }

        /* Keybindings for window resizing */
        cJSON *resize = cJSON_GetObjectItem(keyboard, "resize");
        if (resize) {
            _json_load_object(resize, "right", config_bindings->keyboard.resize.right, MAX_KEYBINDING_LENGTH);
            _json_load_object(resize, "left", config_bindings->keyboard.resize.left, MAX_KEYBINDING_LENGTH);
            _json_load_object(resize, "up", config_bindings->keyboard.resize.up, MAX_KEYBINDING_LENGTH);
            _json_load_object(resize, "down", config_bindings->keyboard.resize.down, MAX_KEYBINDING_LENGTH);
        }

        /* Keybindings for desktop cycling */
        cJSON *desktop = cJSON_GetObjectItem(move, "desktop");
        if (desktop) {
            _json_load_object(desktop, "cycle_prev",
                    config_bindings->keyboard.desktop.cycle_prev,
                    MAX_KEYBINDING_LENGTH);
            _json_load_object(desktop, "cycle_next",
                    config_bindings->keyboard.desktop.cycle_next,
                    MAX_KEYBINDING_LENGTH);
        }
    }

    /* Load mouse bindings */
    cJSON *mouse = cJSON_GetObjectItem(json, "mouse");
    if (mouse) {
        _json_load_object(mouse, "move", config_bindings->mouse.move,
                MAX_KEYBINDING_LENGTH);
        _json_load_object(mouse, "resize",
                config_bindings->mouse.resize, MAX_KEYBINDING_LENGTH);
        _json_load_object(mouse, "lower", config_bindings->mouse.lower,
                MAX_KEYBINDING_LENGTH);

        /* Mouse bindings for desktop cycling */
        cJSON *desktop = cJSON_GetObjectItem(mouse, "desktop");
        if (desktop) {
            _json_load_object(desktop, "cycle_prev",
                    config_bindings->mouse.desktop.cycle_prev,
                    MAX_KEYBINDING_LENGTH);
            _json_load_object(desktop, "cycle_next",
                    config_bindings->mouse.desktop.cycle_next,
                    MAX_KEYBINDING_LENGTH);
        }
    }

    /* Free memory */
    cJSON_Delete(json);
    free(data);

    return 0;
}


/* Load theme configuration */
int config_load_theme(const char *filename,
        struct config_theme_s *config_theme)
{
    char *data;

    /* Load file or exit */
    if (_json_load_from_file(filename, &data) != 0) {
        return 1;
    }

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        logger(stderr, logging_level, LOG_WARNING,
                "Cannot parser file '%s'; using default configuration", 
                cJSON_GetErrorPtr());
        free(data);
        return 2;
    }

    /* Load theme name identifier */
    _json_load_object(json, "name", config_theme->name,
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
            _json_load_object(active, "font",
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
            _json_load_object(inactive, "font",
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

        _json_load_uint(icon, "border_width",
                &config_theme->icon.border_width);
        _json_load_bool(icon, "is_captioned",
                &config_theme->icon.is_captioned);
        _json_load_object(icon, "font", config_theme->icon.font,
                MAX_FONTNAME_LENGTH);
    }

    /* Free memory */
    cJSON_Delete(json);
    free(data);

    return 0;
}
