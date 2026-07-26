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
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* NULL, getenv */

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/json.h>
#include <utils/path.h>
#include <utils/safestr.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/**
 * @brief Parse focus policy text into configuration enumeration
 *
 * @param value Focus policy string from configuration
 *
 * @return Parsed focus policy enumeration value
 *
 * @note Supported values are @c click and @c follow-mouse
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_focus_policy_e s_config_parse_focus_policy(
        const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_FOCUS_POLICY_CLICK;
    }

    if (safe_strcmp(value_norm, "follow-mouse") == 0) {
        return CONFIG_FOCUS_POLICY_FOLLOW_MOUSE;
    }

    return CONFIG_FOCUS_POLICY_CLICK;
}


/**
 * @brief Parse placement policy text into configuration enumeration
 *
 * @param value Placement policy string from configuration
 *
 * @return Parsed placement policy enumeration value
 *
 * @note Supported values are @c smart, @c cascade,
 *       @c centered, and @c under-mouse
 * @note Complexity: @e O(n), where @e n is the length of @p value
 */
static enum config_placement_policy_e s_config_parse_placement_policy(
        const char *value)
{
    char value_norm[CONFIG_MAX_LENGTH_OPTION];

    if (!json_field_normalize(value, value_norm, sizeof(value_norm))) {
        return CONFIG_PLACEMENT_POLICY_SMART;
    }

    if (safe_strcmp(value_norm, "cascade") == 0) {
        return CONFIG_PLACEMENT_POLICY_CASCADE;
    }
    if (safe_strcmp(value_norm, "centered") == 0) {
        return CONFIG_PLACEMENT_POLICY_CENTERED;
    }
    if (safe_strcmp(value_norm, "under-mouse") == 0) {
        return CONFIG_PLACEMENT_POLICY_UNDER_MOUSE;
    }

    return CONFIG_PLACEMENT_POLICY_SMART;
}


/**
 * @brief Load a desktop entry from a JSON object
 *
 * Loads the desktop name and its background color from a JSON object
 * into the provided output fields.
 *
 * @param desktop_json JSON object with desktop settings
 * @param name_out     Destination desktop name
 * @param settings_out Destination desktop settings
 *
 * @note Complexity: @e O(n + m), where @e n is the length of the field
 *       names processed and @e m is the length of the desktop name
 */
static void s_config_load_desktop_entry(cJSON *desktop_json,
        char *name_out, struct desktop_settings_s *settings_out)
{
    if (desktop_json == NULL || name_out == NULL ||
            settings_out == NULL) {
        return;
    }

    json_load_string(desktop_json, "name", name_out,
            CONFIG_MAX_LENGTH_NAME);

    if (json_load_color(desktop_json, "background-color",
                &settings_out->background.color) != 0) {
        LOGGER_NOTICE("Failed to load JSON string:"
                " 'background-color'; desktop '%s' keeps its"
                " default background color", name_out);
    }
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
                = json_hex2uint32("#000000");
        }
    }

    LOGGER_TRACE("Setting default base programs", L_NARG);
    safe_strcpy(config->base.programs.terminal, "xterm");
    safe_strcpy(config->base.programs.launcher, "gmrun");
    safe_strcpy(config->base.programs.file_manager, "spacefm");
    safe_strcpy(config->base.programs.web_browser, "firefox");
    safe_strcpy(config->base.programs.editor, "gvim");
    config->base.windows.snap = 4;
    config->base.windows.focus_policy = CONFIG_FOCUS_POLICY_CLICK;
    config->base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_SMART;
    config->base.windows.focus.is_new_focused = true;
    config->base.windows.focus.is_raised_on_focus = false;
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
    safe_strcpy(config->bindings.mouse.lower, "button2");
    safe_strcpy(config->bindings.mouse.resize, "button3");
    safe_strcpy(config->bindings.mouse.desktop.cycle_prev, "button4");
    safe_strcpy(config->bindings.mouse.desktop.cycle_next, "button5");

    /* Predetermined values for a default theme */
    LOGGER_TRACE("Setting default theme", L_NARG);
    safe_strcpy(config->theme.name, "Default (builtin)");
    config->theme.window.general.border_width = 2;
    config->theme.window.general.is_decorated = true;
    config->theme.window.active.background_color = json_hex2uint32("FFFFFF");
    config->theme.window.active.foreground_color = json_hex2uint32("000000");
    config->theme.window.active.border_color = json_hex2uint32("222222");
    safe_strcpy(config->theme.window.active.font, "monospace bold 9");
    config->theme.window.inactive.background_color = json_hex2uint32("000000");
    config->theme.window.inactive.foreground_color = json_hex2uint32("FFFFFF");
    config->theme.window.inactive.border_color = json_hex2uint32("999999");
    safe_strcpy(config->theme.window.inactive.font, "monospace 9");
    config->theme.icon.background_color = json_hex2uint32("FFFFFF");
    config->theme.icon.foreground_color = json_hex2uint32("000000");
    config->theme.icon.border_color = json_hex2uint32("000000");
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
    if (json_load_config(filename, &json) != 0) {
        return 1;
    }

    /* Theme name */
    if (json_load_string(json, "theme", config_base->theme,
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
        json_load_uint(screen_settings, "count",
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
                        json_load_uint(desktop_item, "count",
                                &config_base->screens[i].desktop_count);
                        json_load_uint(desktop_item, "inaugural",
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
        json_load_string(programs, "terminal",
                config_base->programs.terminal,
                CONFIG_MAX_LENGTH_COMMAND);
        json_load_string(programs, "launcher",
                config_base->programs.launcher,
                CONFIG_MAX_LENGTH_COMMAND);
        json_load_string(programs, "file-manager",
                config_base->programs.file_manager,
                CONFIG_MAX_LENGTH_COMMAND);
        json_load_string(programs, "web-browser",
                config_base->programs.web_browser,
                CONFIG_MAX_LENGTH_COMMAND);
        json_load_string(programs, "editor",
                config_base->programs.editor,
                CONFIG_MAX_LENGTH_COMMAND);
    }

    /* Load window base configuration */
    windows = cJSON_GetObjectItem(json, "windows");
    if (windows) {
        cJSON *focus;
        cJSON *placement;

        json_load_uint(windows, "snap", &config_base->windows.snap);
        focus = cJSON_GetObjectItem(windows, "focus");
        if (focus) {
            cJSON *focus_policy_item;

            json_load_bool(focus, "is-new-focused",
                    &config_base->windows.focus.is_new_focused);
            json_load_bool(focus, "is-raised-on-focus",
                    &config_base->windows.focus.is_raised_on_focus);
            focus_policy_item = json_get_item(focus,
                    "policy");
            if (focus_policy_item != NULL &&
                    cJSON_IsString(focus_policy_item)) {
                config_base->windows.focus_policy =
                    s_config_parse_focus_policy(
                            focus_policy_item->valuestring);
            }
        }
        placement = cJSON_GetObjectItem(windows, "placement");
        if (placement) {
            cJSON *placement_policy_item;

            placement_policy_item = json_get_item(
                    placement, "policy");
            if (placement_policy_item != NULL &&
                    cJSON_IsString(placement_policy_item)) {
                config_base->windows.placement_policy =
                    s_config_parse_placement_policy(
                            placement_policy_item->valuestring);
            }
            json_load_bool(placement, "is-centered",
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
    if (json_load_config(filename, &json) != 0) {
        return 1;
    }

    /* Load keyboard modifiers */
    modifiers = cJSON_GetObjectItem(json, "modifiers");
    if (modifiers) {
        json_load_string(modifiers, "modc", config_bindings->modc,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(modifiers, "mods", config_bindings->mods,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(modifiers, "modl", config_bindings->modl,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(modifiers, "mod1", config_bindings->mod1,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(modifiers, "mod2", config_bindings->mod2,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(modifiers, "mod3", config_bindings->mod3,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(modifiers, "mod4", config_bindings->mod4,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(modifiers, "mod5", config_bindings->mod5,
                CONFIG_MAX_LENGTH_BINDING);
    }

    /* Load keybindings */
    keyboard = cJSON_GetObjectItem(json, "keyboard");
    if (keyboard != NULL) {
        cJSON *move;
        cJSON *resize;
        cJSON *desktop;

        json_load_string(keyboard, "terminal",
                config_bindings->keyboard.terminal,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "launcher",
                config_bindings->keyboard.launcher,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "file-manager",
                config_bindings->keyboard.file_manager,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "web-browser",
                config_bindings->keyboard.web_browser,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "editor",
                config_bindings->keyboard.editor,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "center",
                config_bindings->keyboard.center,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "maximize",
                config_bindings->keyboard.maximize,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "fullscreen",
                config_bindings->keyboard.fullscreen,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "shade",
                config_bindings->keyboard.shade,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "pin",
                config_bindings->keyboard.pin,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "iconify",
                config_bindings->keyboard.iconify,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "close",
                config_bindings->keyboard.close,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "kill",
                config_bindings->keyboard.kill,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "info",
                config_bindings->keyboard.info,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "cycle-prev",
                config_bindings->keyboard.cycle_prev,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "cycle-next",
                config_bindings->keyboard.cycle_next,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "hide",
                config_bindings->keyboard.hide,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(keyboard, "toggle-decoration",
                config_bindings->keyboard.toggle_decoration,
                CONFIG_MAX_LENGTH_BINDING);

        /* Keybindings for window movement */
        move = cJSON_GetObjectItem(keyboard, "move");
        if (move) {
            cJSON *relative;
            cJSON *absolute;

            relative = cJSON_GetObjectItem(move, "relative");
            if (relative) {
                json_load_string(relative, "right",
                        config_bindings->keyboard.move.relative.right,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(relative, "left",
                        config_bindings->keyboard.move.relative.left,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(relative, "up",
                        config_bindings->keyboard.move.relative.up,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(relative, "down",
                        config_bindings->keyboard.move.relative.down,
                        CONFIG_MAX_LENGTH_BINDING);
            }

            absolute = cJSON_GetObjectItem(move, "absolute");
            if (absolute) {
                json_load_string(absolute, "top-left",
                        config_bindings->keyboard.move.absolute.top_left,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(absolute, "top-right",
                        config_bindings->keyboard.move.absolute.top_right,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(absolute, "bottom-left",
                        config_bindings->keyboard.move.absolute.bottom_left,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(absolute, "bottom-right",
                        config_bindings->keyboard.move.absolute.bottom_right,
                        CONFIG_MAX_LENGTH_BINDING);
            }
        }

        /* Keybindings for window resizing */
        resize = cJSON_GetObjectItem(keyboard, "resize");
        if (resize) {
            json_load_string(resize, "right",
                    config_bindings->keyboard.resize.right,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(resize, "left",
                    config_bindings->keyboard.resize.left,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(resize, "up",
                    config_bindings->keyboard.resize.up,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(resize, "down",
                    config_bindings->keyboard.resize.down,
                    CONFIG_MAX_LENGTH_BINDING);
        }

        /* Keybindings for desktop cycling */
        desktop = cJSON_GetObjectItem(keyboard, "desktop");
        if (desktop) {
            json_load_string(desktop, "cycle-prev",
                    config_bindings->keyboard.desktop.cycle_prev,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(desktop, "cycle-next",
                    config_bindings->keyboard.desktop.cycle_next,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(desktop, "cycle-icon-prev",
                    config_bindings->keyboard.desktop.cycle_icon_prev,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(desktop, "cycle-icon-next",
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

        json_load_string(mouse, "move", config_bindings->mouse.move,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(mouse, "resize",
                config_bindings->mouse.resize,
                CONFIG_MAX_LENGTH_BINDING);
        json_load_string(mouse, "lower", config_bindings->mouse.lower,
                CONFIG_MAX_LENGTH_BINDING);

        /* Mouse bindings for desktop cycling */
        desktop = cJSON_GetObjectItem(mouse, "desktop");
        if (desktop) {
            json_load_string(desktop, "cycle-prev",
                    config_bindings->mouse.desktop.cycle_prev,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(desktop, "cycle-next",
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

    if (json_load_config(filename, &json) != 0) {
        return 1;
    }

    json_load_string(json, "name", config_theme->name,
            CONFIG_MAX_LENGTH_FONTNAME);

    window = cJSON_GetObjectItem(json, "window");
    if (window) {
        cJSON *general;
        cJSON *active;
        cJSON *inactive;

        general = cJSON_GetObjectItem(window, "general");
        if (general) {
            json_load_uint(general, "border-width",
                    &config_theme->window.general.border_width);
            json_load_bool(general, "is-decorated",
                    &config_theme->window.general.is_decorated);
        }

        active = cJSON_GetObjectItem(window, "active");
        if (active) {
            json_load_color(active, "background-color",
                    &config_theme->window.active.background_color);
            json_load_color(active, "foreground-color",
                    &config_theme->window.active.foreground_color);
            json_load_color(active, "border-color",
                    &config_theme->window.active.border_color);
            json_load_string(active, "font",
                    config_theme->window.active.font,
                    CONFIG_MAX_LENGTH_FONTNAME);
        }

        inactive = cJSON_GetObjectItem(window, "inactive");
        if (inactive) {
            json_load_color(inactive, "background-color",
                    &config_theme->window.inactive.background_color);
            json_load_color(inactive, "foreground-color",
                    &config_theme->window.inactive.foreground_color);
            json_load_color(inactive, "border-color",
                    &config_theme->window.inactive.border_color);
            json_load_string(inactive, "font",
                    config_theme->window.inactive.font,
                    CONFIG_MAX_LENGTH_FONTNAME);
        }
    }

    icon = cJSON_GetObjectItem(json, "icon");
    if (icon) {
        json_load_color(icon, "background-color",
                &config_theme->icon.background_color);
        json_load_color(icon, "foreground-color",
                &config_theme->icon.foreground_color);
        json_load_color(icon, "border-color",
                &config_theme->icon.border_color);
        json_load_uint(icon, "border-width",
                &config_theme->icon.border_width);
        json_load_bool(icon, "is-captioned",
                &config_theme->icon.is_captioned);
        json_load_string(icon, "font", config_theme->icon.font,
                CONFIG_MAX_LENGTH_FONTNAME);
    }

    cJSON_Delete(json);

    return 0;
}
