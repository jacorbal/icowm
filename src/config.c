/**
 * @file config.c
 *
 * @brief Configuration structures and procedures implementation
 *
 * The configuration data is compiled in several JSON files and the
 * structure is populated by retrieving their contents.
 */
/*
 * NOTE, i.e., MUSINGS AND ADMONITIONS TO MINE OWN REFLECTIVE INNER SELF:
 *
 * Regarding the forthcoming extant self (Sat Mar 22 05:01 CET 2025):
 *      The current state of this code is significantly suboptimal.
 *      I implore you to initiate refactoring at your earliest
 *      convenience, or at a time that is deemed more suitable.
 *
 * Regarding the whilom expired self (Sun Mar 23 06:12 CET 2025):
 *      Should've done it correctly from the very outset and avoided
 *      future headaches: *my* current headaches.
 *
 * Regarding the erstwhile selves now faded (Sun Jan 11 22:18 CET 2026):
 *      I find myself ensnared in the dire consequences of this wretched
 *      code, which continues to vex my weary soul with its torment.
 *      Each passing hour doth remind me of the ill-advised choices of
 *      yore; verily, I remain a prisoner of my own flawed creations.
 *
 * Regarding my rambling selves of yesteryear (Sat Feb 14 11:36 CET 2026):
 *      Pish, let it matter not, ye idle knaves!  I shall change naught!
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
#include <stdio.h>      /* FILE, snprintf */
#include <stdlib.h>     /* NULL, free, malloc, getenv, size_t */

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/path.h>
#include <utils/safestr.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/**
 * @brief Convert a hexadecimal color string into a unsigned 32-bit
 *        integer
 *
 * @param hex_color Hexadecimal color string
 *
 * @return Color value as unsigned 32-bit integer
 *
 * @note The initial @p hex_color string could begin with character '#',
 *       for it's ignored
 * @note Complexity: @e O(n), where @e n is the length of the
 *       hexadecimal string
 */
static uint32_t s_hex2uint32(const char *hex_color)
{
    uint32_t color = 0;

    if (hex_color[0] == '#') {
        hex_color++;
    }
    if (sscanf(hex_color, "%x", &color) != 1) {
        LOGGER_NOTICE("Failed to parse hexadecimal color '%s';" \
                " defaulting to '#000000'", hex_color);
        return 0;
    }

    return color;
}


/**
 * @brief Normalize a JSON field name into a canonical separator form
 *
 * Builds in @p field_norm a canonical version of @p field where every
 * separator '_' or '-' is replaced with the canonical separator '-'.
 *
 * @param field      Original field name
 * @param field_norm Destination buffer for canonical field name
 * @param size       Size of @p field_norm
 *
 * @return Whether normalization succeeded
 * @retval true  The normalized field fits in the destination buffer
 * @retval false Invalid arguments or destination buffer too small
 *
 * @note Complexity: @e O(n), where @e n is the length of @p field
 */
static bool s_json_field_normalize(const char *field, char *field_norm,
        size_t size)
{
    size_t i;

    if (field == NULL || field_norm == NULL || size == 0) {
        return false;
    }

    for (i = 0; field[i] != '\0' && i < size - 1; ++i) {
        if (field[i] == '_' || field[i] == '-') {
            field_norm[i] = '-';
        } else {
            field_norm[i] = field[i];
        }
    }

    if (field[i] != '\0') {
        field_norm[0] = '\0';
        return false;
    }

    field_norm[i] = '\0';
    return true;
}


/**
 * @brief Retrieve a JSON object item by canonicalized field name
 *
 * Searches a JSON object for a field whose normalized name matches the
 * normalized version of @p field.  Both '_' and '-' are treated as the
 * same separator and normalized to '-'.
 *
 * @param json  JSON object to search
 * @param field Requested field name
 *
 * @return Matching cJSON item or @c NULL if not found
 *
 * @note Complexity: @e O(k * n), where @e k is the number of fields in
 *       the object and @e n is the average field name length
 */
static cJSON *s_json_get_object_item_normalized(cJSON *json,
        const char *field)
{
    cJSON *item;
    char field_norm[CONFIG_MAX_LENGTH_NAME];
    char item_norm[CONFIG_MAX_LENGTH_NAME];

    if (json == NULL || field == NULL || !cJSON_IsObject(json)) {
        return NULL;
    }

    if (!s_json_field_normalize(field, field_norm,
                sizeof(field_norm))) {
        return NULL;
    }

    cJSON_ArrayForEach(item, json) {
        if (item->string == NULL) {
            continue;
        }

        if (!s_json_field_normalize(item->string, item_norm,
                    sizeof(item_norm))) {
            continue;
        }

        if (safe_strcmp(field_norm, item_norm) == 0) {
            return item;
        }
    }

    return NULL;
}


/**
 * @brief Load a color value from a JSON object into an unsigned 32-bit
 * integer
 *
 * Extracts a specified field from a JSON object and converts its string
 * value from hexadecimal notation into a 32-bit color.  Field lookup is
 * canonicalized so that '-' and '_' are treated identically.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field Name of the field to extract
 * @param dest  Pointer to the destination color value
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to load color data
 *
 * @note If the field is absent or is not a string, the destination
 *       value remains unchanged.
 * @note Complexity: @e O(k * n + m), where @e k is the number of fields
 *       in the object, @e n is the average field name length, and
 *       @e m is the length of the hexadecimal color string
 */
static int s_json_load_color(cJSON *json, const char *field,
        uint32_t *dest)
{
    cJSON *item;

    item = s_json_get_object_item_normalized(json, field);
    if (item && cJSON_IsString(item)) {
        *dest = s_hex2uint32(item->valuestring);
        return 0;
    }

    LOGGER_NOTICE("Failed to load JSON color string: '%s'", field);
    return 1;
}


/**
 * @brief Load a string value from a JSON object into destination buffer
 *
 * Extracts a specified field from a JSON object and copies its value
 * into the destination buffer.  Field lookup is canonicalized so that
 * '-' and '_' are treated identically.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field Name of the field to extract
 * @param dest  Pointer to the destination buffer where value is copied
 * @param size  Maximum number of characters to copy including terminator
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to load string data
 *
 * @note The function ensures that the destination buffer does not
 *       overflow and is properly null-terminated
 * @note Complexity: @e O(k * n + m), where @e k is the number of fields
 *       in the object, @e n is the average field name length, and
 *       @e m is the length of the string being copied
 */
static int s_json_load_string(cJSON *json, const char *field, char *dest,
        size_t size)
{
    cJSON *item;

    item = s_json_get_object_item_normalized(json, field);
    if (item && cJSON_IsString(item)) {
        safe_strncpy(dest, item->valuestring, size);
        return 0;
    }

    LOGGER_NOTICE("Failed to load JSON string: '%s'", field);
    return 1;
}


/**
 * @brief Load a desktop entry from a JSON object
 *
 * Loads the desktop name and its background color from a JSON object
 * into the provided output fields.  Missing or invalid fields leave the
 * corresponding destination values unchanged.
 *
 * @param desktop_json JSON object with desktop settings
 * @param name_out     Destination desktop name
 * @param settings_out Destination desktop settings
 *
 * @note Complexity: @e O(n + m), where @e n is the length of the JSON
 *        field names processed and @e m is the length of the loaded
 *        desktop name string
 */
static void s_config_load_desktop_entry(cJSON *desktop_json,
        char *name_out, struct desktop_settings_s *settings_out)
{
    if (desktop_json == NULL || name_out == NULL ||
            settings_out == NULL) {
        return;
    }

    s_json_load_string(desktop_json, "name", name_out,
            CONFIG_MAX_LENGTH_NAME);

    if (s_json_load_color(desktop_json, "background-color",
                &settings_out->background.color) != 0) {
        LOGGER_NOTICE("Failed to load JSON string:"
                " 'background-color'; desktop '%s' keeps its"
                " default background color", name_out);
    }
}


/**
 * @brief Load unsigned integer value from a JSON object into an
 * unsigned integer pointer
 *
 * Extracts a specified field from a JSON object and stores its unsigned
 * integer value in the provided destination pointer.  Field lookup is
 * canonicalized so that '-' and '_' are treated identically.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field Name of the field to extract
 * @param dest  Pointer to an unsigned integer where the value is stored
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to load unsigned int data
 *
 * @note If the field is absent or the value cannot be converted to an
 *       unsigned integer, the destination value remains unchanged
 * @note Complexity: @e O(k * n), where @e k is the number of fields in
 *       the object and @e n is the average field name length
 */
static int s_json_load_uint(cJSON *json, const char *field,
        unsigned int *dest)
{
    cJSON *item;

    item = s_json_get_object_item_normalized(json, field);
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
 * value in the provided destination pointer.  Field lookup is
 * canonicalized so that '-' and '_' are treated identically.
 *
 * @param json  Pointer to the JSON object to extract data from
 * @param field Name of the field to extract
 * @param dest  Pointer to a boolean where the value will be stored
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to load boolean data
 *
 * @note If the field is absent or the value cannot be interpreted as
 *       a boolean, the destination value remains unchanged
 * @note Complexity: @e O(k * n), where @e k is the number of fields in
 *       the object and @e n is the average field name length
 */
static int s_json_load_bool(cJSON *json, const char *field, bool *dest)
{
    cJSON *item;

    item = s_json_get_object_item_normalized(json, field);
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
static int s_json_load_from_file(const char *filename, char **data)
{
    FILE *file;
    size_t length;
    size_t nread;
    long file_length;

    LOGGER_INFO("Parsing data from file '%s'", filename);

    if (data == NULL) {
        LOGGER_ERROR("Received 'NULL' output pointer for file '%s'",
                filename);
        return 1;
    }

    *data = NULL;   /* Always leave caller with a known value on error */

    LOGGER_TRACE("Opening JSON file '%s'", filename);
    file = fopen(filename, "r");
    if (!file) {
        LOGGER_NOTICE("File not found or unable to open:" \
                " '%s'; default values will be used", filename);
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        LOGGER_NOTICE("Unable to seek JSON file '%s'; default values"
                " will be used", filename);
        return 1;
    }
    file_length = ftell(file);

    if (file_length <= 0) {
        fclose(file);
        LOGGER_NOTICE("File '%s' is empty or unreadable; default values"
                " will be used", filename);
        return 1;
    }

    length = (size_t) file_length;
/*
    if (length == 0) {
        fclose(file);
        LOGGER_NOTICE("File '%s' is empty; default values" \
                " will be used", filename);
        return 1;
    }
*/
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        LOGGER_NOTICE("Unable to rewind JSON file '%s'; default values"
                " will be used", filename);
        return 1;
    }

    *data = malloc(length + 1);
    if (*data == NULL) {
        LOGGER_ERROR("Failed to allocate memory for file '%s'",
                filename);
        fclose(file);
        return 2;
    }

    LOGGER_TRACE("Reading JSON file '%s'", filename);
    nread = fread(*data, 1, length, file);
    if (nread != length) {
        LOGGER_NOTICE("Failed to read full JSON file '%s'; default"
                " values will be used", filename);
        free(*data);
        *data = NULL;
        fclose(file);
        return 1;
    }

    /* Make sure data is null-terminated */
    (*data)[length] = '\0';

    LOGGER_TRACE("Closing JSON file '%s'", filename);
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
static int s_json_load_config(const char *filename, cJSON **json_out)
{
    cJSON *json_root;
    char *data;

    if (s_json_load_from_file(filename, &data) != 0) {
        return 1;
    }

    json_root = cJSON_Parse(data);
    if (json_root == NULL) {
        LOGGER_WARNING("Failed to parse file '%s';" \
                " default configuration will be used", filename);
        LOGGER_TRACE("Error parsing JSON file\n%s", cJSON_GetErrorPtr());
        free(data);
        return 2;
    }

    if (!cJSON_IsObject(json_root)) {
        if (cJSON_IsArray(json_root) && cJSON_GetArraySize(json_root) >= 1) {
            cJSON *array_first = cJSON_GetArrayItem(json_root, 0);

            if (array_first && cJSON_IsObject(array_first)) {
                cJSON *json_dup = cJSON_Duplicate(array_first, cJSON_True);
                cJSON_Delete(json_root);
                if (json_dup == NULL) {
                    LOGGER_WARNING("Failed to duplicate configuration" \
                            " object from '%s'; default configuration" \
                            " will be used", filename);
                    free(data);
                    return 2;
                }
                json_root = json_dup;
                LOGGER_NOTICE("Using first object from top-level array" \
                        " in '%s' as compatibility fallback", filename);
            } else {
                LOGGER_WARNING("Invalid top-level JSON in '%s'; expected" \
                        " an object and default configuration will be used",
                        filename);
                cJSON_Delete(json_root);
                free(data);
                return 2;
            }
        } else {
            LOGGER_WARNING("Invalid top-level JSON in '%s'; expected" \
                    " an object and default configuration will be used",
                    filename);
            cJSON_Delete(json_root);
            free(data);
            return 2;
        }
    }

    free(data);
    *json_out = json_root;
    return 0;
}


/**
 * @brief Set the configuration directory base
 *
 * Set the base path for the configuration directory based on the
 * environment variables @c XDG_CONFIG_HOME and @c HOME.  If neither is
 * set, it defaults to the current working directory.  The resulting
 * path is stored in the provided buffer @p config_dir_base.
 *
 * @param config_dir_prefix Base configuration directory, or @c NULL to
 *                          use the default directory
 * @param config_dir_base   Pointer to a character array where the
 *                          configuration directory path will be stored
 *
 * @note The buffer should be at least @c CONFIG_MAX_LENGTH_PATH_BASE
 */
static void s_config_dir_set(const char *config_dir_prefix,
        char *config_dir_base)
{
    const char *config_xdg_config_home = getenv("XDG_CONFIG_HOME");
    const char *config_home = getenv("HOME");
    char temp_path[CONFIG_MAX_LENGTH_PATH_BASE];

    if (config_dir_prefix) {
        snprintf(temp_path, CONFIG_MAX_LENGTH_PATH_BASE,
                "%s", config_dir_prefix);

    } else if (config_xdg_config_home) {
        /* "${XDG_CONFIG_HOME}/icowm" */
        snprintf(temp_path, CONFIG_MAX_LENGTH_PATH_BASE,
                "%s/%s", config_xdg_config_home, CONFIG_DIR_BASE);
    } else if (config_home) {
        /* "${HOME}/.icowm" */
        snprintf(temp_path, CONFIG_MAX_LENGTH_PATH_BASE,
                "%s/.%s", config_home, CONFIG_DIR_BASE);
    } else {
        /* "$(pwd)/.icowm"; let's hope there's always a "${HOME}" */
        snprintf(temp_path, CONFIG_MAX_LENGTH_PATH_BASE,
                "./%s", CONFIG_DIR_BASE);
    }

    path_simplify(temp_path);
    safe_strncpy(config_dir_base, temp_path, CONFIG_MAX_LENGTH_PATH_BASE);

    /* Use generated path in case of error */
    snprintf(config_dir_base, CONFIG_MAX_LENGTH_PATH_BASE,
            "%s", temp_path);
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
    config_set_default_values(config);

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


/* Populate the configuration structure with default values */
void config_set_default_values(config_td *config)
{
    /* Assign predetermined values for base configuration */
    config->base.theme[0] = '\0';
    config->base.screen_count = 1;

    LOGGER_TRACE("Setting configuration for each screen", L_NARG);
    for (unsigned int i = 0; i < config->base.screen_count; ++i) {
        /* Set number of desktops per screen */
        config->base.screens[i].desktop_count = CONFIG_MAX_DESKTOPS;
        config->base.screens[i].desktop_inaugural = 0;

        /* All desktop settings */
        LOGGER_TRACE("Setting desktops configuration on screen %u", i);
        for (unsigned int j = 0;
                j < config->base.screens[i].desktop_count;
                ++j) {
            char desktop_name[CONFIG_MAX_LENGTH_NAME];
            snprintf(desktop_name, sizeof(desktop_name),
                    "Desktop %u", j);
            safe_strncpy(config->base.screens[i].desktops[j].name,
                desktop_name, CONFIG_MAX_LENGTH_NAME);

            config->base.screens[i].desktops[j].settings.background.color
                = s_hex2uint32("#000000");
        }
    }

    LOGGER_TRACE("Setting default base programs", L_NARG);
    safe_strcpy(config->base.programs.terminal, "xterm");
    safe_strcpy(config->base.programs.launcher, "gmrun");
    safe_strcpy(config->base.programs.file_manager, "spacefm");
    safe_strcpy(config->base.programs.web_browser, "firefox");
    safe_strcpy(config->base.programs.editor, "gvim");
    config->base.windows.snap = 4;
    config->base.windows.focus.is_new_focused = true;
    config->base.windows.focus.is_raised_on_focus = false;
    safe_strcpy(config->base.windows.focus.policy, "click");
    safe_strcpy(config->base.windows.placement.policy, "smart");
    config->base.windows.placement.is_centered = false;

    /* Assign predetermined values for bindings modifiers */
    LOGGER_TRACE("Setting default bindings modifiers", L_NARG);
    safe_strcpy(config->bindings.modc, "Control");
    safe_strcpy(config->bindings.mods, "Shift");
    safe_strcpy(config->bindings.modl, "Caps_Lock");
    safe_strcpy(config->bindings.mod1, "Alt");
    safe_strcpy(config->bindings.mod2, "Num_Lock");
    safe_strcpy(config->bindings.mod3, "");
    safe_strcpy(config->bindings.mod4, "Super");
    safe_strcpy(config->bindings.mod5, "Hyper");

    /* Predetermined configuration for keybindings */
    LOGGER_TRACE("Setting default keybindings", L_NARG);
    safe_strcpy(config->bindings.keyboard.terminal, "modc+mod1+Return");
    safe_strcpy(config->bindings.keyboard.launcher, "modc+mod1+r");
    safe_strcpy(config->bindings.keyboard.file_manager, "modc+mod1+q");
    safe_strcpy(config->bindings.keyboard.web_browser, "modc+mod1+w");
    safe_strcpy(config->bindings.keyboard.editor, "modc+mod1+e");
    safe_strcpy(config->bindings.keyboard.center, "modc+mod1+g");
    safe_strcpy(config->bindings.keyboard.maximize, "modc+mod1+m");
    safe_strcpy(config->bindings.keyboard.fullscreen, "modc+mod1+f");
    safe_strcpy(config->bindings.keyboard.shade, "modc+mod1+s");
    safe_strcpy(config->bindings.keyboard.pin, "modc+mod1+p");
    safe_strcpy(config->bindings.keyboard.iconify, "modc+mod1+i");
    safe_strcpy(config->bindings.keyboard.close, "modc+mod1+c");
    safe_strcpy(config->bindings.keyboard.kill, "modc+mod1+mods+Escape");
    safe_strcpy(config->bindings.keyboard.info, "modc+mod1+mods+i");
    safe_strcpy(config->bindings.keyboard.cycle_prev, "mod1+mods+Tab");
    safe_strcpy(config->bindings.keyboard.cycle_next, "mod1+Tab");
    safe_strcpy(config->bindings.keyboard.hide, "modc+mod1+mods+h");
    safe_strcpy(config->bindings.keyboard.toggle_decoration, "modc+mod1+d");

    /* Predetermined configuration for movement with keyboard */
    LOGGER_TRACE("Setting default movement/resizing keybindings",
            L_NARG);
    safe_strcpy(config->bindings.keyboard.move.relative.right,
            "modc+mod1+l");
    safe_strcpy(config->bindings.keyboard.move.relative.left,
            "modc+mod1+h");
    safe_strcpy(config->bindings.keyboard.move.relative.up,
            "modc+mod1+k");
    safe_strcpy(config->bindings.keyboard.move.relative.down,
            "modc+mod1+j");
    safe_strcpy(config->bindings.keyboard.move.absolute.top_left,
            "modc+mod1+y");
    safe_strcpy(config->bindings.keyboard.move.absolute.top_right,
            "modc+mod1+u");
    safe_strcpy(config->bindings.keyboard.move.absolute.bottom_left,
            "modc+mod1+b");
    safe_strcpy(config->bindings.keyboard.move.absolute.bottom_right,
            "modc+mod1+n");
    safe_strcpy(config->bindings.keyboard.resize.right,
            "modc+mod1+mods+l");
    safe_strcpy(config->bindings.keyboard.resize.left,
            "modc+mod1+mods+h");
    safe_strcpy(config->bindings.keyboard.resize.up,
            "modc+mod1+mods+k");
    safe_strcpy(config->bindings.keyboard.resize.down,
            "modc+mod1+mods+j");
    safe_strcpy(config->bindings.keyboard.desktop.cycle_prev,
            "modc+mod1+Left");
    safe_strcpy(config->bindings.keyboard.desktop.cycle_next,
            "modc+mod1+Right");
    safe_strcpy(config->bindings.keyboard.desktop.cycle_icon_prev,
            "modc+mod1+mods+Left");
    safe_strcpy(config->bindings.keyboard.desktop.cycle_icon_next,
            "modc+mod1+mods+Right");

    /* Predetermined configuration for mouse bindings */
    LOGGER_TRACE("Setting default mouse bindings", L_NARG);
    safe_strcpy(config->bindings.mouse.move, "button1");
    safe_strcpy(config->bindings.mouse.resize, "button2");
    safe_strcpy(config->bindings.mouse.lower, "button3");
    safe_strcpy(config->bindings.mouse.desktop.cycle_prev, "button4");
    safe_strcpy(config->bindings.mouse.desktop.cycle_next, "button5");

    /* Predetermined values for a default theme */
    LOGGER_TRACE("Setting default theme", L_NARG);
    safe_strcpy(config->theme.name, "Default (builtin)");
    config->theme.window.general.border_width = 2;
    config->theme.window.general.is_decorated = true;
    config->theme.window.active.background_color = s_hex2uint32("FFFFFF");
    config->theme.window.active.foreground_color = s_hex2uint32("000000");
    config->theme.window.active.border_color = s_hex2uint32("222222");
    safe_strcpy(config->theme.window.active.font, "monospace bold 9");
    config->theme.window.inactive.background_color = s_hex2uint32("000000");
    config->theme.window.inactive.foreground_color = s_hex2uint32("FFFFFF");
    config->theme.window.inactive.border_color = s_hex2uint32("999999");
    safe_strcpy(config->theme.window.inactive.font, "monospace 9");
    config->theme.icon.background_color = s_hex2uint32("FFFFFF");
    config->theme.icon.foreground_color = s_hex2uint32("000000");
    config->theme.icon.border_color = s_hex2uint32("000000");
    config->theme.icon.border_width = 1;
    config->theme.icon.is_captioned = true;
    safe_strcpy(config->theme.icon.font, "monospace 8");
}


/* Load all the configuration */
int config_load(config_td *config, const char *config_prefix)
{
    char config_dir[CONFIG_MAX_LENGTH_PATH_BASE];
    char config_base_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_bindings_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_theme_file[CONFIG_MAX_LENGTH_PATH_THEME];

    s_config_dir_set(config_prefix, config_dir);

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
    snprintf(config_theme_file,
            sizeof(config_theme_file),
            "%s/%s/%s.json", config_dir, CONFIG_DIR_THEMES,
            config->base.theme);

    /* Load theme if it's specified, i.e., not empty string */
    if (safe_strlen(config->base.theme) == 0) {
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
    cJSON *programs;
    cJSON *windows;
    cJSON *screen_settings;

    LOGGER_TRACE("Preparing to parse base configuration from file" \
            " '%s'", filename);

    /* Load file, or exit */
    if (s_json_load_config(filename, &json) != 0) {
        return 1;
    }

    /* Theme name */
    if (s_json_load_string(json, "theme", config_base->theme,
                CONFIG_MAX_LENGTH_FILENAME) != 0) {
        LOGGER_WARNING("Invalid theme specified:" \
                " '%s'; default configuration will be used",
                config_base->theme);
        config_base->theme[0] = '\0';
    }

    screen_settings = cJSON_GetObjectItem(json, "screens");
    if (screen_settings == NULL) {
        LOGGER_NOTICE("No 'screens' object found in '%s';" \
                " desktop settings, including background colors," \
                " will keep their default values", filename);
    } else {
        cJSON *settings;
        cJSON *desktops_array;

        /* Load total number of screen */
        s_json_load_uint(screen_settings, "count",
                &config_base->screen_count);

        /* Get 'desktop' array inside 'settings' */
        settings =
            cJSON_GetObjectItem(screen_settings, "settings");
        desktops_array =
            cJSON_GetObjectItem(settings, "desktops");

        /* NOTE: While I recognize this maze of if statements could
         *       benefit from finesse, I am stuck with it for now.
         *       A sophisticated refactor will come, but deadlines have
         *       a way of complicating matters. */
        if (desktops_array && cJSON_IsArray(desktops_array)) {
            unsigned int desktop_count =
                (unsigned int) cJSON_GetArraySize(desktops_array);
            cJSON *first_desktop_item;
            bool uses_nested_screen_layout = false;

            desktop_count = (desktop_count > CONFIG_MAX_DESKTOPS)
                ? CONFIG_MAX_DESKTOPS
                : desktop_count;
            first_desktop_item = cJSON_GetArrayItem(desktops_array, 0);
            if (first_desktop_item && cJSON_IsObject(first_desktop_item)) {
                if (cJSON_GetObjectItem(first_desktop_item, "settings") ||
                        cJSON_GetObjectItem(first_desktop_item, "count") ||
                        cJSON_GetObjectItem(first_desktop_item,
                            "inaugural")) {
                    uses_nested_screen_layout = true;
                }
            }

            if (!uses_nested_screen_layout) {
                config_base->screens[0].desktop_count = desktop_count;

                for (unsigned int i = 0;
                        i < desktop_count && i < CONFIG_MAX_DESKTOPS;
                        ++i) {
                    cJSON *desktop_item;

                    desktop_item =
                        cJSON_GetArrayItem(desktops_array, (int) i);
                    if (desktop_item == NULL) {
                        continue;
                    }
                    s_config_load_desktop_entry(desktop_item,
                            config_base->screens[0].desktops[i].name,
                            &config_base->screens[0].desktops[i].settings);
                }
            } else {
                /* Each entry of 'desktops_array' represents a screen in
                 * this layout, so the bound must be
                 * 'CONFIG_MAX_SCREENS', not 'CONFIG_MAX_DESKTOPS';
                 * otherwise 'config_base->screens[i]' would be written
                 * out of bounds */
                for (unsigned int i = 0;
                        i < desktop_count && i < CONFIG_MAX_SCREENS;
                        ++i) {
                    cJSON *desktop_item;

                    desktop_item =
                        cJSON_GetArrayItem(desktops_array, (int) i);
                    if (desktop_item) {
                        cJSON *desktop_settings;
                        /* Load desktop 'count' and 'inaugural' */
                        s_json_load_uint(desktop_item, "count",
                                &config_base->screens[i].desktop_count);
                        s_json_load_uint(desktop_item, "inaugural",
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
                        desktop_settings =
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
                                cJSON *setting_item;

                                setting_item =
                                    cJSON_GetArrayItem(desktop_settings,
                                            (int) j);
                                if (setting_item) {
                                    s_config_load_desktop_entry(setting_item,
                                            config_base->screens[i]
                                            .desktops[j].name,
                                            &config_base->screens[i]
                                            .desktops[j].settings);
                                } /* ! if (setting_item) */
                            } /* ! for(j in 0..settings_count) */
                        } /* ! if (desktop_settings) */
                    } /* ! if (desktop_item) */
                } /* ! for (i in 0..desktop_count) */
            } /* ! if (!uses_nested_screen_layout) */
        } else {
            LOGGER_NOTICE("No 'desktops' array found under" \
                    " 'screens.settings' in '%s'; desktop" \
                    " settings, including background colors, will" \
                    " keep their default values", filename);
        } /* ! if (desktops_array) */
    } /* ! if (screen_settings) */

    /* Load default programs */
    programs = cJSON_GetObjectItem(json, "programs");
    if (programs) {
        s_json_load_string(programs, "terminal",
                config_base->programs.terminal,
                CONFIG_MAX_LENGTH_COMMAND);
        s_json_load_string(programs, "launcher",
                config_base->programs.launcher,
                CONFIG_MAX_LENGTH_COMMAND);
        s_json_load_string(programs, "file-manager",
                config_base->programs.file_manager,
                CONFIG_MAX_LENGTH_COMMAND);
        s_json_load_string(programs, "web-browser",
                config_base->programs.web_browser,
                CONFIG_MAX_LENGTH_COMMAND);
        s_json_load_string(programs, "editor",
                config_base->programs.editor,
                CONFIG_MAX_LENGTH_COMMAND);
    }

    /* Load window base configuration */
    windows = cJSON_GetObjectItem(json, "windows");
    if (windows) {
        cJSON *focus;
        cJSON *placement;

        s_json_load_uint(windows, "snap", &config_base->windows.snap);
        focus = cJSON_GetObjectItem(windows, "focus");
        if (focus) {
            s_json_load_bool(focus, "is-new-focused",
                    &config_base->windows.focus.is_new_focused);
            s_json_load_bool(focus, "is-raised-on-focus",
                    &config_base->windows.focus.is_raised_on_focus);
            s_json_load_string(focus, "focus-policy",
                    config_base->windows.focus.policy,
                    CONFIG_MAX_LENGTH_OPTION);
        }
        placement = cJSON_GetObjectItem(windows, "placement");
        if (placement) {
            s_json_load_string(placement, "policy",
                    config_base->windows.placement.policy,
                    CONFIG_MAX_LENGTH_OPTION);
            s_json_load_bool(placement, "is-centered",
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
    cJSON *modifiers;
    cJSON *keyboard;
    cJSON *mouse;

    LOGGER_TRACE("Parsing bindings configuration from file '%s'",
            filename);

    /* Load file or exit */
    if (s_json_load_config(filename, &json) != 0) {
        return 1;
    }

    /* Load keyboard modifiers */
    modifiers = cJSON_GetObjectItem(json, "modifiers");
    if (modifiers) {
        s_json_load_string(modifiers, "modc", config_bindings->modc,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(modifiers, "mods", config_bindings->mods,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(modifiers, "modl", config_bindings->modl,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(modifiers, "mod1", config_bindings->mod1,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(modifiers, "mod2", config_bindings->mod2,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(modifiers, "mod3", config_bindings->mod3,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(modifiers, "mod4", config_bindings->mod4,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(modifiers, "mod5", config_bindings->mod5,
                CONFIG_MAX_LENGTH_BINDING);
    }

    /* Load keybindings */
    keyboard = cJSON_GetObjectItem(json, "keyboard");
    if (keyboard != NULL) {
        cJSON *move;
        cJSON *resize;
        cJSON *desktop;

        s_json_load_string(keyboard, "terminal",
                config_bindings->keyboard.terminal,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "launcher",
                config_bindings->keyboard.launcher,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "file-manager",
                config_bindings->keyboard.file_manager,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "web-browser",
                config_bindings->keyboard.web_browser,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "editor",
                config_bindings->keyboard.editor,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "center",
                config_bindings->keyboard.center,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "maximize",
                config_bindings->keyboard.maximize,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "fullscreen",
                config_bindings->keyboard.fullscreen,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "shade",
                config_bindings->keyboard.shade,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "pin",
                config_bindings->keyboard.pin,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "iconify",
                config_bindings->keyboard.iconify,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "close",
                config_bindings->keyboard.close,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "kill",
                config_bindings->keyboard.kill,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "info",
                config_bindings->keyboard.info,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "cycle-prev",
                config_bindings->keyboard.cycle_prev,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "cycle-next",
                config_bindings->keyboard.cycle_next,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "hide",
                config_bindings->keyboard.hide,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(keyboard, "toggle-decoration",
                config_bindings->keyboard.toggle_decoration,
                CONFIG_MAX_LENGTH_BINDING);

        /* Keybindings for window movement */
        move = cJSON_GetObjectItem(keyboard, "move");
        if (move) {
            cJSON *relative;
            cJSON *absolute;

            relative = cJSON_GetObjectItem(move, "relative");
            if (relative) {
                s_json_load_string(relative, "right",
                        config_bindings->keyboard.move.relative.right,
                        CONFIG_MAX_LENGTH_BINDING);
                s_json_load_string(relative, "left",
                        config_bindings->keyboard.move.relative.left,
                        CONFIG_MAX_LENGTH_BINDING);
                s_json_load_string(relative, "up",
                        config_bindings->keyboard.move.relative.up,
                        CONFIG_MAX_LENGTH_BINDING);
                s_json_load_string(relative, "down",
                        config_bindings->keyboard.move.relative.down,
                        CONFIG_MAX_LENGTH_BINDING);
            }

            absolute = cJSON_GetObjectItem(move, "absolute");
            if (absolute) {
                s_json_load_string(absolute, "top-left",
                        config_bindings->keyboard.move.absolute.top_left,
                        CONFIG_MAX_LENGTH_BINDING);
                s_json_load_string(absolute, "top-right",
                        config_bindings->keyboard.move.absolute.top_right,
                        CONFIG_MAX_LENGTH_BINDING);
                s_json_load_string(absolute, "bottom-left",
                        config_bindings->keyboard.move.absolute.bottom_left,
                        CONFIG_MAX_LENGTH_BINDING);
                s_json_load_string(absolute, "bottom-right",
                        config_bindings->keyboard.move.absolute.bottom_right,
                        CONFIG_MAX_LENGTH_BINDING);
            }
        }

        /* Keybindings for window resizing */
        resize = cJSON_GetObjectItem(keyboard, "resize");
        if (resize) {
            s_json_load_string(resize, "right",
                    config_bindings->keyboard.resize.right,
                    CONFIG_MAX_LENGTH_BINDING);
            s_json_load_string(resize, "left",
                    config_bindings->keyboard.resize.left,
                    CONFIG_MAX_LENGTH_BINDING);
            s_json_load_string(resize, "up",
                    config_bindings->keyboard.resize.up,
                    CONFIG_MAX_LENGTH_BINDING);
            s_json_load_string(resize, "down",
                    config_bindings->keyboard.resize.down,
                    CONFIG_MAX_LENGTH_BINDING);
        }

        /* Keybindings for desktop cycling */
        desktop = cJSON_GetObjectItem(keyboard, "desktop");
        if (desktop) {
            s_json_load_string(desktop, "cycle-prev",
                    config_bindings->keyboard.desktop.cycle_prev,
                    CONFIG_MAX_LENGTH_BINDING);
            s_json_load_string(desktop, "cycle-next",
                    config_bindings->keyboard.desktop.cycle_next,
                    CONFIG_MAX_LENGTH_BINDING);
            s_json_load_string(desktop, "cycle-icon-prev",
                    config_bindings->keyboard.desktop.cycle_icon_prev,
                    CONFIG_MAX_LENGTH_BINDING);
            s_json_load_string(desktop, "cycle-icon-next",
                    config_bindings->keyboard.desktop.cycle_icon_next,
                    CONFIG_MAX_LENGTH_BINDING);
        }
    }

    /* Load mouse bindings.  Accept the 'mouse' section either at the
     * top level of the file, or nested inside 'keyboard', both layouts
     * have been observed in configuration files in the wild, and
     * silently ignoring one of them would leave the user's mouse
     * configuration inert without any indication why. */
    mouse = cJSON_GetObjectItem(json, "mouse");
    if (mouse == NULL && keyboard != NULL) {
        mouse = cJSON_GetObjectItem(keyboard, "mouse");
    }

    if (mouse) {
        cJSON *desktop;

        s_json_load_string(mouse, "move", config_bindings->mouse.move,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(mouse, "resize",
                config_bindings->mouse.resize,
                CONFIG_MAX_LENGTH_BINDING);
        s_json_load_string(mouse, "lower", config_bindings->mouse.lower,
                CONFIG_MAX_LENGTH_BINDING);

        /* Mouse bindings for desktop cycling */
        desktop = cJSON_GetObjectItem(mouse, "desktop");
        if (desktop) {
            s_json_load_string(desktop, "cycle-prev",
                    config_bindings->mouse.desktop.cycle_prev,
                    CONFIG_MAX_LENGTH_BINDING);
            s_json_load_string(desktop, "cycle-next",
                    config_bindings->mouse.desktop.cycle_next,
                    CONFIG_MAX_LENGTH_BINDING);
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
    cJSON *window;
    cJSON *icon;

    LOGGER_TRACE("Parsing theme configuration from file '%s'",
            filename);

    if (s_json_load_config(filename, &json) != 0) {
        return 1;
    }

    s_json_load_string(json, "name", config_theme->name,
            CONFIG_MAX_LENGTH_FONTNAME);

    window = cJSON_GetObjectItem(json, "window");
    if (window) {
        cJSON *general;
        cJSON *active;
        cJSON *inactive;

        general = cJSON_GetObjectItem(window, "general");
        if (general) {
            s_json_load_uint(general, "border-width",
                    &config_theme->window.general.border_width);
            s_json_load_bool(general, "is-decorated",
                    &config_theme->window.general.is_decorated);
        }

        active = cJSON_GetObjectItem(window, "active");
        if (active) {
            s_json_load_color(active, "background-color",
                    &config_theme->window.active.background_color);
            s_json_load_color(active, "foreground-color",
                    &config_theme->window.active.foreground_color);
            s_json_load_color(active, "border-color",
                    &config_theme->window.active.border_color);
            s_json_load_string(active, "font",
                    config_theme->window.active.font,
                    CONFIG_MAX_LENGTH_FONTNAME);
        }

        inactive = cJSON_GetObjectItem(window, "inactive");
        if (inactive) {
            s_json_load_color(inactive, "background-color",
                    &config_theme->window.inactive.background_color);
            s_json_load_color(inactive, "foreground-color",
                    &config_theme->window.inactive.foreground_color);
            s_json_load_color(inactive, "border-color",
                    &config_theme->window.inactive.border_color);
            s_json_load_string(inactive, "font",
                    config_theme->window.inactive.font,
                    CONFIG_MAX_LENGTH_FONTNAME);
        }
    }

    icon = cJSON_GetObjectItem(json, "icon");
    if (icon) {
        s_json_load_color(icon, "background-color",
                &config_theme->icon.background_color);
        s_json_load_color(icon, "foreground-color",
                &config_theme->icon.foreground_color);
        s_json_load_color(icon, "border-color",
                &config_theme->icon.border_color);
        s_json_load_uint(icon, "border-width",
                &config_theme->icon.border_width);
        s_json_load_bool(icon, "is-captioned",
                &config_theme->icon.is_captioned);
        s_json_load_string(icon, "font", config_theme->icon.font,
                CONFIG_MAX_LENGTH_FONTNAME);
    }

    cJSON_Delete(json);

    return 0;
}
