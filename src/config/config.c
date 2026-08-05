/**
 * @file config/config.c
 *
 * @brief Configuration init, destroy, defaults, and load dispatcher
 *        implementation
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
#include <stdlib.h>     /* NULL, calloc, free, getenv */

/* Utils includes */
#include <utils/config/json.h>
#include <utils/config/path.h>
#include <utils/safe/safestr.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


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
 * @note Buffer should be at least @c CONFIG_MAX_LENGTH_PATH_BASE
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
    safe_strncpy(config_dir_base, temp_path,
            CONFIG_MAX_LENGTH_PATH_BASE);
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
                = json_hex2uint32("#C0CCD8");
        }
    }

    LOGGER_TRACE("Setting default base programs", L_NARG);
    safe_strcpy(config->base.programs.terminal, "xterm");
    safe_strcpy(config->base.programs.launcher, "gmrun");
    safe_strcpy(config->base.programs.file_manager, "pcmanfm");
    safe_strcpy(config->base.programs.editor, "gvim");
    safe_strcpy(config->base.programs.web_browser, "firefox");
    config->base.windows.move_step = 10;
    config->base.windows.resize_step = 20;  /* usually overriden by hints */
    config->base.windows.snap = 4;
    config->base.windows.has_grips = false;
    config->base.windows.show_geom = true;
    config->base.windows.gravity = CONFIG_GRAVITY_NORTH_WEST;
    config->base.windows.focus_policy = CONFIG_FOCUS_POLICY_CLICK;
    config->base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_SMART;
    config->base.windows.focus.is_new_focused = true;
    config->base.windows.focus.is_raised_on_focus = false;
    config->base.icons.placement_policy = CONFIG_ICON_PLACEMENT_SMART;
    config->base.icons.show_geom = false;
    config->base.enable_emergency_shortcut = true;
    config->base.show_desktop_notify = true;

    /* Predetermined values for RandR output profile management */
    LOGGER_TRACE("Setting default RandR configuration", L_NARG);
    config->randr.is_enabled = false;
    config->randr.output_count = 0u;

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
    safe_strcpy(config->bindings.keyboard.launch.terminal,
            "modc+mod1+Return");
    safe_strcpy(config->bindings.keyboard.launch.launcher,
            "modc+mod1+r");
    safe_strcpy(config->bindings.keyboard.launch.file_manager,
            "modc+mod1+q");
    safe_strcpy(config->bindings.keyboard.launch.web_browser,
            "modc+mod1+w");
    safe_strcpy(config->bindings.keyboard.launch.editor,
            "modc+mod1+e");
    safe_strcpy(config->bindings.keyboard.window.close,
            "modc+mod1+c");
    safe_strcpy(config->bindings.keyboard.window.decorate,
            "modc+mod1+d");
    safe_strcpy(config->bindings.keyboard.window.fullscreen,
            "modc+mod1+f");
    safe_strcpy(config->bindings.keyboard.window.hide,
            "modc+mod1+mods+u");
    safe_strcpy(config->bindings.keyboard.window.iconify,
            "modc+mod1+i");
    safe_strcpy(config->bindings.keyboard.window.info,
            "modc+mod1+mods+i");
    safe_strcpy(config->bindings.keyboard.window.kill,
            "modc+mod1+mods+Escape");
    safe_strcpy(config->bindings.keyboard.window.maximize,
            "modc+mod1+m");
    safe_strcpy(config->bindings.keyboard.window.pin,
            "modc+mod1+p");
    safe_strcpy(config->bindings.keyboard.window.layer,
            "modc+mod1+mods+y");
    safe_strcpy(config->bindings.keyboard.window.shade,
            "modc+mod1+s");
    safe_strcpy(config->bindings.keyboard.cycle.desktop.prev,
            "modc+mod1+Left");
    safe_strcpy(config->bindings.keyboard.cycle.desktop.next,
            "modc+mod1+Right");
    safe_strcpy(config->bindings.keyboard.cycle.icon.prev,
            "modc+mod1+mods+Tab");
    safe_strcpy(config->bindings.keyboard.cycle.icon.next,
            "modc+mod1+Tab");
    safe_strcpy(config->bindings.keyboard.cycle.window.prev,
            "mod1+mods+Tab");
    safe_strcpy(config->bindings.keyboard.cycle.window.next,
            "mod1+Tab");
    safe_strcpy(config->bindings.keyboard.wm.redraw,
            "modc+mod1+mods+r");
    safe_strcpy(config->bindings.keyboard.wm.reload,
            "modc+mod1+mods+c");
    safe_strcpy(config->bindings.keyboard.wm.quit,
            "modc+mod1+mods+x");
    safe_strcpy(config->bindings.keyboard.wm.show_desktop,
            "modc+mod1+mods+d");

    /* Predetermined goto-desktop shortcuts for desktops 0-9 */
    LOGGER_TRACE("Setting default go-to keybindings", L_NARG);
    safe_strcpy(config->bindings.keyboard.wm.go_to.desktop[0],
            "modc+mod1+0");
    safe_strcpy(config->bindings.keyboard.wm.go_to.desktop[1],
            "modc+mod1+1");
    safe_strcpy(config->bindings.keyboard.wm.go_to.desktop[2],
            "modc+mod1+2");
    safe_strcpy(config->bindings.keyboard.wm.go_to.desktop[3],
            "modc+mod1+3");
    safe_strcpy(config->bindings.keyboard.wm.go_to.desktop[4],
            "modc+mod1+4");
    safe_strcpy(config->bindings.keyboard.wm.go_to.desktop[5],
            "modc+mod1+5");
    safe_strcpy(config->bindings.keyboard.wm.go_to.desktop[6],
            "modc+mod1+6");
    safe_strcpy(config->bindings.keyboard.wm.go_to.desktop[7],
            "modc+mod1+7");
    safe_strcpy(config->bindings.keyboard.wm.go_to.desktop[8],
            "modc+mod1+8");
    safe_strcpy(config->bindings.keyboard.wm.go_to.desktop[9],
            "modc+mod1+9");

    /* Predetermined configuration for movement with keyboard */
    LOGGER_TRACE("Setting default movement/resizing keybindings",
            L_NARG);
    safe_strcpy(config->bindings.keyboard.window.move.relative.right,
            "modc+mod1+l");
    safe_strcpy(config->bindings.keyboard.window.move.relative.left,
            "modc+mod1+h");
    safe_strcpy(config->bindings.keyboard.window.move.relative.up,
            "modc+mod1+k");
    safe_strcpy(config->bindings.keyboard.window.move.relative.down,
            "modc+mod1+j");
    safe_strcpy(config->bindings.keyboard.window.move.absolute.center,
            "modc+mod1+g");
    safe_strcpy(config->bindings.keyboard.window.move.absolute.top_left,
            "modc+mod1+y");
    safe_strcpy(config->bindings.keyboard.window.move.absolute.top_right,
            "modc+mod1+u");
    safe_strcpy(config->bindings.keyboard.window.move.absolute.bottom_left,
            "modc+mod1+b");
    safe_strcpy(config->bindings.keyboard.window.move.absolute.bottom_right,
            "modc+mod1+n");
    safe_strcpy(config->bindings.keyboard.window.resize.right,
            "modc+mod1+mods+l");
    safe_strcpy(config->bindings.keyboard.window.resize.left,
            "modc+mod1+mods+h");
    safe_strcpy(config->bindings.keyboard.window.resize.up,
            "modc+mod1+mods+k");
    safe_strcpy(config->bindings.keyboard.window.resize.down,
            "modc+mod1+mods+j");

    /* Predetermined configuration for mouse bindings */
    LOGGER_TRACE("Setting default mouse bindings", L_NARG);
    safe_strcpy(config->bindings.mouse.window.move, "mod1+button1");
    safe_strcpy(config->bindings.mouse.window.lower, "mod1+button2");
    safe_strcpy(config->bindings.mouse.window.resize, "mod1+button3");
    safe_strcpy(config->bindings.mouse.cycle.desktop.prev, "button4");
    safe_strcpy(config->bindings.mouse.cycle.desktop.next, "button5");

    /* Predetermined values for a default theme */
    LOGGER_TRACE("Setting default theme", L_NARG);
    safe_strcpy(config->theme.name, "Default (builtin)");
    config->theme.window.general.border_width = 2;
    config->theme.window.general.is_decorated = true;
    config->theme.window.active.background_color =
        json_hex2uint32("9AAEC8");
    config->theme.window.active.foreground_color =
        json_hex2uint32("253040");
    config->theme.window.active.border_color =
        json_hex2uint32("4A5566");
    config->theme.window.active.grip_color =
        json_hex2uint32("9AAEC8");
    safe_strcpy(config->theme.window.active.font, "fixed bold");
    config->theme.window.inactive.background_color =
        json_hex2uint32("D0D9E5");
    config->theme.window.inactive.foreground_color =
        json_hex2uint32("4A5566");
    config->theme.window.inactive.border_color =
        json_hex2uint32("7F9AB6");
    config->theme.window.inactive.grip_color =
        json_hex2uint32("4A5566");
    safe_strcpy(config->theme.window.inactive.font, "fixed");
    config->theme.icon.general.border_width = 2;
    config->theme.icon.general.is_captioned = true;
    config->theme.icon.active.background_color = json_hex2uint32("9AAEC8");
    config->theme.icon.active.foreground_color = json_hex2uint32("253040");
    config->theme.icon.active.border_color = json_hex2uint32("4A5566");
    safe_strcpy(config->theme.icon.active.font, "fixed");
    config->theme.icon.inactive.background_color = json_hex2uint32("D0D9E5");
    config->theme.icon.inactive.foreground_color = json_hex2uint32("4A5566");
    config->theme.icon.inactive.border_color = json_hex2uint32("7F9AB6");
    safe_strcpy(config->theme.icon.inactive.font, "fixed");
}


/* Load all the configuration */
int config_load(config_td *config, const char *config_prefix)
{
    char config_dir[CONFIG_MAX_LENGTH_PATH_BASE];
    char config_base_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_bindings_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_theme_file[CONFIG_MAX_LENGTH_PATH_THEME];
    char config_randr_file[CONFIG_MAX_LENGTH_PATH_CONFIG];

    s_config_dir_set(config_prefix, config_dir);

    LOGGER_DEBUG("Loading configuration from files on: '%s'",
            config_dir);

    /* Set main base configuration path */
    snprintf(config_base_file, sizeof(config_base_file),
            "%s/%s", config_dir, CONFIG_FILENAME_BASE);

    /* Load base configuration */
    if (config_load_base(config_base_file, &(config->base)) != 0) {
        LOGGER_WARNING("Base configuration could not be loaded;" \
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
            LOGGER_WARNING("Failed to load theme from:" \
                    " '%s'; default theme will be used",
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

    return 0;
}
