/**
 * @file config/bindings.c
 *
 * @brief Keyboard and mouse bindings configuration loader
 *        implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* JSON includes */
#include <cjson/cJSON.h>

/* Utils includes */
#include <utils/config/json.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


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
        cJSON *wm;
        cJSON *launch;
        cJSON *window;
        cJSON *cycle;
        cJSON *go_to;

        launch = cJSON_GetObjectItem(keyboard, "launch");
        if (launch) {
            json_load_string(launch, "terminal",
                    config_bindings->keyboard.launch.terminal,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(launch, "launcher",
                    config_bindings->keyboard.launch.launcher,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(launch, "file-manager",
                    config_bindings->keyboard.launch.file_manager,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(launch, "web-browser",
                    config_bindings->keyboard.launch.web_browser,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(launch, "editor",
                    config_bindings->keyboard.launch.editor,
                    CONFIG_MAX_LENGTH_BINDING);
        }

        window = cJSON_GetObjectItem(keyboard, "window");
        if (window) {
            cJSON *window_move;
            cJSON *window_resize;

            json_load_string(window, "close",
                    config_bindings->keyboard.window.close,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "decorate",
                    config_bindings->keyboard.window.decorate,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "fullscreen",
                    config_bindings->keyboard.window.fullscreen,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "hide",
                    config_bindings->keyboard.window.hide,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "iconify",
                    config_bindings->keyboard.window.iconify,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "info",
                    config_bindings->keyboard.window.info,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "kill",
                    config_bindings->keyboard.window.kill,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "layer",
                    config_bindings->keyboard.window.layer,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "maximize",
                    config_bindings->keyboard.window.maximize,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "pin",
                    config_bindings->keyboard.window.pin,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "shade",
                    config_bindings->keyboard.window.shade,
                    CONFIG_MAX_LENGTH_BINDING);

            window_move = cJSON_GetObjectItem(window, "move");
            if (window_move != NULL) {
                cJSON *relative;
                cJSON *absolute;
                relative = cJSON_GetObjectItem(window_move, "relative");
                if (relative) {
                    json_load_string(relative, "right",
                config_bindings->keyboard.window.move.relative.right,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(relative, "left",
                config_bindings->keyboard.window.move.relative.left,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(relative, "up",
                config_bindings->keyboard.window.move.relative.up,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(relative, "down",
                config_bindings->keyboard.window.move.relative.down,
                            CONFIG_MAX_LENGTH_BINDING);
                }
                absolute = cJSON_GetObjectItem(window_move, "absolute");
                if (absolute) {
                    json_load_string(absolute, "center",
                config_bindings->keyboard.window.move.absolute.center,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(absolute, "top-left",
                config_bindings->keyboard.window.move.absolute.top_left,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(absolute, "top-right",
                config_bindings->keyboard.window.move.absolute.top_right,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(absolute, "bottom-left",
                config_bindings->keyboard.window.move.absolute.bottom_left,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(absolute, "bottom-right",
                config_bindings->keyboard.window.move.absolute.bottom_right,
                            CONFIG_MAX_LENGTH_BINDING);
                }
            }

            window_resize = cJSON_GetObjectItem(window, "resize");
            if (window_resize != NULL) {
                json_load_string(window_resize, "right",
                        config_bindings->keyboard.window.resize.right,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(window_resize, "left",
                        config_bindings->keyboard.window.resize.left,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(window_resize, "up",
                        config_bindings->keyboard.window.resize.up,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(window_resize, "down",
                        config_bindings->keyboard.window.resize.down,
                        CONFIG_MAX_LENGTH_BINDING);
            }

            json_load_string(window, "show-desktop",
                    config_bindings->keyboard.wm.show_desktop,
                    CONFIG_MAX_LENGTH_BINDING);
            /* Direct go-to shortcuts 0-9 */
            go_to = cJSON_GetObjectItem(wm, "go-to");
            if (go_to != NULL) {
                static const char *keys[10] = {
                    "desktop0", "desktop1", "desktop2",
                    "desktop3", "desktop4", "desktop5",
                    "desktop6", "desktop7", "desktop8",
                    "desktop9"
                };
                for (int gi = 0; gi < 10; ++gi) {
                    json_load_string(go_to, keys[gi],
                            config_bindings->keyboard.wm.go_to.desktop[gi],
                            CONFIG_MAX_LENGTH_BINDING);
                }
            }
        }

        wm = cJSON_GetObjectItem(keyboard, "wm");
        if (wm) {
            json_load_string(wm, "redraw",
                    config_bindings->keyboard.wm.redraw,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(wm, "reload",
                    config_bindings->keyboard.wm.reload,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(wm, "quit",
                    config_bindings->keyboard.wm.quit,
                    CONFIG_MAX_LENGTH_BINDING);
        }

        /* Keybindings for cycling: desktop, icon, and window */
        cycle = cJSON_GetObjectItem(keyboard, "cycle");
        if (cycle) {
            cJSON *cdesktop;
            cJSON *cicon;
            cJSON *cwindow;

            cdesktop = cJSON_GetObjectItem(cycle, "desktop");
            if (cdesktop) {
                json_load_string(cdesktop, "prev",
                        config_bindings->keyboard.cycle.desktop.prev,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(cdesktop, "next",
                        config_bindings->keyboard.cycle.desktop.next,
                        CONFIG_MAX_LENGTH_BINDING);
            }

            cicon = cJSON_GetObjectItem(cycle, "icon");
            if (cicon) {
                json_load_string(cicon, "prev",
                        config_bindings->keyboard.cycle.icon.prev,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(cicon, "next",
                        config_bindings->keyboard.cycle.icon.next,
                        CONFIG_MAX_LENGTH_BINDING);
            }

            cwindow = cJSON_GetObjectItem(cycle, "window");
            if (cwindow) {
                json_load_string(cwindow, "prev",
                        config_bindings->keyboard.cycle.window.prev,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(cwindow, "next",
                        config_bindings->keyboard.cycle.window.next,
                        CONFIG_MAX_LENGTH_BINDING);
            }
        }
    }

    /* Load mouse bindings.  The 'mouse' section must be at the top
     * level of the file, separate from 'keyboard'. */
    mouse = cJSON_GetObjectItem(json, "mouse");

    if (mouse) {
        cJSON *mwindow;
        cJSON *mcycle;

        mwindow = cJSON_GetObjectItem(mouse, "window");
        if (mwindow) {
            json_load_string(mwindow, "move",
                    config_bindings->mouse.window.move,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(mwindow, "lower",
                    config_bindings->mouse.window.lower,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(mwindow, "resize",
                    config_bindings->mouse.window.resize,
                    CONFIG_MAX_LENGTH_BINDING);
        }

        /* Mouse bindings for desktop cycling */
        mcycle = cJSON_GetObjectItem(mouse, "cycle");
        if (mcycle) {
            cJSON *cdesktop = cJSON_GetObjectItem(mcycle, "desktop");
            if (cdesktop) {
                json_load_string(cdesktop, "prev",
                        config_bindings->mouse.cycle.desktop.prev,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(cdesktop, "next",
                        config_bindings->mouse.cycle.desktop.next,
                        CONFIG_MAX_LENGTH_BINDING);
            }
        }
    }

    /* Free memory */
    cJSON_Delete(json);

    return 0;
}
