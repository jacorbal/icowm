/**
 * @file config/bindings.c
 *
 * @brief Keyboard and mouse bindings configuration loader and
 *        defaults implementation
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
#include <utils/safe/safestr.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <config.h>


/* Populate default values for the keyboard and mouse bindings
 * configuration structure, used both as the initial process-wide
 * default and, before applying any bindings.json found, as the
 * known-good starting point that file's own fields then overlay */
void config_set_default_bindings_values(
        struct config_bindings_s *config_bindings)
{
    /* Assign predetermined values for bindings modifiers */
    LOGGER_TRACE("Setting default bindings modifiers", L_NARG);
    safe_strncpy(config_bindings->modc,
            "Control", sizeof(config_bindings->modc));
    safe_strncpy(config_bindings->mods,
            "Shift", sizeof(config_bindings->mods));
    safe_strncpy(config_bindings->modl,
            "Caps_Lock", sizeof(config_bindings->modl));
    safe_strncpy(config_bindings->mod1, "Alt", sizeof(config_bindings->mod1));
    safe_strncpy(config_bindings->mod2,
            "Num_Lock", sizeof(config_bindings->mod2));
    safe_strncpy(config_bindings->mod3, "", sizeof(config_bindings->mod3));
    safe_strncpy(config_bindings->mod4,
            "Super", sizeof(config_bindings->mod4));
    safe_strncpy(config_bindings->mod5,
            "Hyper", sizeof(config_bindings->mod5));

    /* Predetermined configuration for keybindings */
    LOGGER_TRACE("Setting default keybindings", L_NARG);
    safe_strncpy(config_bindings->keyboard.launch.terminal,
            "modc+mod1+Return",
            sizeof(config_bindings->keyboard.launch.terminal));
    safe_strncpy(config_bindings->keyboard.launch.launcher,
            "modc+mod1+r", sizeof(config_bindings->keyboard.launch.launcher));
    safe_strncpy(config_bindings->keyboard.launch.file_manager,
            "modc+mod1+q",
            sizeof(config_bindings->keyboard.launch.file_manager));
    safe_strncpy(config_bindings->keyboard.launch.web_browser,
            "modc+mod1+w",
            sizeof(config_bindings->keyboard.launch.web_browser));
    safe_strncpy(config_bindings->keyboard.launch.editor,
            "modc+mod1+e", sizeof(config_bindings->keyboard.launch.editor));
    safe_strncpy(config_bindings->keyboard.wm.menus.root,
            "modc+mod1+mods+m",
            sizeof(config_bindings->keyboard.wm.menus.root));
    safe_strncpy(config_bindings->keyboard.wm.menus.windows,
            "modc+mod1+mods+w",
            sizeof(config_bindings->keyboard.wm.menus.windows));
    safe_strncpy(config_bindings->keyboard.wm.search,
            "modc+mod4+mods+s",
            sizeof(config_bindings->keyboard.wm.search));
    safe_strncpy(config_bindings->keyboard.window.close,
            "modc+mod1+c", sizeof(config_bindings->keyboard.window.close));
    safe_strncpy(config_bindings->keyboard.window.decorate,
            "modc+mod1+d", sizeof(config_bindings->keyboard.window.decorate));
    safe_strncpy(config_bindings->keyboard.window.fullscreen,
            "modc+mod1+f",
            sizeof(config_bindings->keyboard.window.fullscreen));
    safe_strncpy(config_bindings->keyboard.window.hide,
            "modc+mod1+mods+u", sizeof(config_bindings->keyboard.window.hide));
    safe_strncpy(config_bindings->keyboard.window.iconify,
            "modc+mod1+i", sizeof(config_bindings->keyboard.window.iconify));
    safe_strncpy(config_bindings->keyboard.window.iconify_all,
            "modc+mod1+mods+i",
            sizeof(config_bindings->keyboard.window.iconify_all));
    safe_strncpy(config_bindings->keyboard.window.deiconify_all,
            "modc+mod1+mods+d",
            sizeof(config_bindings->keyboard.window.deiconify_all));
    safe_strncpy(config_bindings->keyboard.window.arrange,
            "modc+mod1+mods+a",
            sizeof(config_bindings->keyboard.window.arrange));
    safe_strncpy(config_bindings->keyboard.window.info,
            "modc+mod4+mods+i", sizeof(config_bindings->keyboard.window.info));
    safe_strncpy(config_bindings->keyboard.window.kill,
            "modc+mod1+mods+Escape",
            sizeof(config_bindings->keyboard.window.kill));
    safe_strncpy(config_bindings->keyboard.window.maximize,
            "modc+mod1+m", sizeof(config_bindings->keyboard.window.maximize));
    safe_strncpy(config_bindings->keyboard.window.pin,
            "modc+mod1+p", sizeof(config_bindings->keyboard.window.pin));
    safe_strncpy(config_bindings->keyboard.window.layer,
            "modc+mod1+mods+y",
            sizeof(config_bindings->keyboard.window.layer));
    safe_strncpy(config_bindings->keyboard.window.shade,
            "modc+mod1+s", sizeof(config_bindings->keyboard.window.shade));
    safe_strncpy(config_bindings->keyboard.cycle.desktop.north,
            "modc+mod1+Up",
            sizeof(config_bindings->keyboard.cycle.desktop.north));
    safe_strncpy(config_bindings->keyboard.cycle.desktop.south,
            "modc+mod1+Down",
            sizeof(config_bindings->keyboard.cycle.desktop.south));
    safe_strncpy(config_bindings->keyboard.cycle.desktop.east,
            "modc+mod1+Right",
            sizeof(config_bindings->keyboard.cycle.desktop.east));
    safe_strncpy(config_bindings->keyboard.cycle.desktop.west,
            "modc+mod1+Left",
            sizeof(config_bindings->keyboard.cycle.desktop.west));
    safe_strncpy(config_bindings->keyboard.cycle.icon.prev,
            "modc+mod1+mods+Tab",
            sizeof(config_bindings->keyboard.cycle.icon.prev));
    safe_strncpy(config_bindings->keyboard.cycle.icon.next,
            "modc+mod1+Tab",
            sizeof(config_bindings->keyboard.cycle.icon.next));
    safe_strncpy(config_bindings->keyboard.cycle.window.prev,
            "mod1+mods+Tab",
            sizeof(config_bindings->keyboard.cycle.window.prev));
    safe_strncpy(config_bindings->keyboard.cycle.window.next,
            "mod1+Tab", sizeof(config_bindings->keyboard.cycle.window.next));
    safe_strncpy(config_bindings->keyboard.wm.redraw,
            "modc+mod1+mods+r", sizeof(config_bindings->keyboard.wm.redraw));
    safe_strncpy(config_bindings->keyboard.wm.reload,
            "modc+mod1+mods+c", sizeof(config_bindings->keyboard.wm.reload));
    safe_strncpy(config_bindings->keyboard.wm.quit,
            "modc+mod1+mods+x", sizeof(config_bindings->keyboard.wm.quit));
    safe_strncpy(config_bindings->keyboard.wm.shortcuts,
            "modc+mod4+F1", sizeof(config_bindings->keyboard.wm.shortcuts));
    safe_strncpy(config_bindings->keyboard.wm.fortune,
            "modc+mod4+Backspace",
            sizeof(config_bindings->keyboard.wm.fortune));
    safe_strncpy(config_bindings->keyboard.wm.scratchpad,
            "modc+mod1+mods+F12",
            sizeof(config_bindings->keyboard.wm.scratchpad));

    /* Deliberately empty: no key is bound to this out of the box,
     * unlike every other binding above.  Explicit here, the same way
     * every other default in this function is explicit, so this
     * reads as an intentional choice rather than a forgotten one. */
    safe_strncpy(config_bindings->keyboard.wm.toggle_strutless_maximize,
            "", sizeof(config_bindings->keyboard.wm.toggle_strutless_maximize));

    safe_strncpy(config_bindings->keyboard.desktop.add,
            "modc+mod4+mods+Right",
            sizeof(config_bindings->keyboard.desktop.add));
    safe_strncpy(config_bindings->keyboard.desktop.remove,
            "modc+mod4+mods+Left",
            sizeof(config_bindings->keyboard.desktop.remove));
    safe_strncpy(config_bindings->keyboard.desktop.show,
            "modc+mod4+mods+d",
            sizeof(config_bindings->keyboard.desktop.show));

    /* Predetermined goto-desktop shortcuts for desktops 0-9 */
    LOGGER_TRACE("Setting default go-to keybindings", L_NARG);
    safe_strncpy(config_bindings->keyboard.desktop.go_to.desktop[0],
            "modc+mod1+0",
            sizeof(config_bindings->keyboard.desktop.go_to.desktop[0]));
    safe_strncpy(config_bindings->keyboard.desktop.go_to.desktop[1],
            "modc+mod1+1",
            sizeof(config_bindings->keyboard.desktop.go_to.desktop[1]));
    safe_strncpy(config_bindings->keyboard.desktop.go_to.desktop[2],
            "modc+mod1+2",
            sizeof(config_bindings->keyboard.desktop.go_to.desktop[2]));
    safe_strncpy(config_bindings->keyboard.desktop.go_to.desktop[3],
            "modc+mod1+3",
            sizeof(config_bindings->keyboard.desktop.go_to.desktop[3]));
    safe_strncpy(config_bindings->keyboard.desktop.go_to.desktop[4],
            "modc+mod1+4",
            sizeof(config_bindings->keyboard.desktop.go_to.desktop[4]));
    safe_strncpy(config_bindings->keyboard.desktop.go_to.desktop[5],
            "modc+mod1+5",
            sizeof(config_bindings->keyboard.desktop.go_to.desktop[5]));
    safe_strncpy(config_bindings->keyboard.desktop.go_to.desktop[6],
            "modc+mod1+6",
            sizeof(config_bindings->keyboard.desktop.go_to.desktop[6]));
    safe_strncpy(config_bindings->keyboard.desktop.go_to.desktop[7],
            "modc+mod1+7",
            sizeof(config_bindings->keyboard.desktop.go_to.desktop[7]));
    safe_strncpy(config_bindings->keyboard.desktop.go_to.desktop[8],
            "modc+mod1+8",
            sizeof(config_bindings->keyboard.desktop.go_to.desktop[8]));
    safe_strncpy(config_bindings->keyboard.desktop.go_to.desktop[9],
            "modc+mod1+9",
            sizeof(config_bindings->keyboard.desktop.go_to.desktop[9]));

    /* Predetermined configuration for movement with keyboard */
    LOGGER_TRACE("Setting default movement/resizing keybindings",
            L_NARG);
    safe_strncpy(config_bindings->keyboard.window.move.relative.right,
            "modc+mod1+l",
            sizeof(config_bindings->keyboard.window.move.relative.right));
    safe_strncpy(config_bindings->keyboard.window.move.relative.left,
            "modc+mod1+h",
            sizeof(config_bindings->keyboard.window.move.relative.left));
    safe_strncpy(config_bindings->keyboard.window.move.relative.up,
            "modc+mod1+k",
            sizeof(config_bindings->keyboard.window.move.relative.up));
    safe_strncpy(config_bindings->keyboard.window.move.relative.down,
            "modc+mod1+j",
            sizeof(config_bindings->keyboard.window.move.relative.down));
    safe_strncpy(config_bindings->keyboard.window.move.absolute.center,
            "modc+mod1+g",
            sizeof(config_bindings->keyboard.window.move.absolute.center));
    safe_strncpy(config_bindings->keyboard.window.move.absolute.top_left,
            "modc+mod1+y",
            sizeof(config_bindings->keyboard.window.move.absolute.top_left));
    safe_strncpy(config_bindings->keyboard.window.move.absolute.top_right,
            "modc+mod1+u",
            sizeof(config_bindings->keyboard.window.move.absolute.top_right));
    safe_strncpy(config_bindings->keyboard.window.move.absolute.bottom_left,
            "modc+mod1+b",
            sizeof(config_bindings->keyboard.window.move.absolute.
                    bottom_left));
    safe_strncpy(config_bindings->keyboard.window.move.absolute.bottom_right,
            "modc+mod1+n",
            sizeof(config_bindings->keyboard.window.move.absolute.
                    bottom_right));
    safe_strncpy(config_bindings->keyboard.window.resize.right,
            "modc+mod1+mods+l",
            sizeof(config_bindings->keyboard.window.resize.right));
    safe_strncpy(config_bindings->keyboard.window.resize.left,
            "modc+mod1+mods+h",
            sizeof(config_bindings->keyboard.window.resize.left));
    safe_strncpy(config_bindings->keyboard.window.resize.up,
            "modc+mod1+mods+k",
            sizeof(config_bindings->keyboard.window.resize.up));
    safe_strncpy(config_bindings->keyboard.window.resize.down,
            "modc+mod1+mods+j",
            sizeof(config_bindings->keyboard.window.resize.down));
    safe_strncpy(config_bindings->keyboard.window.send_to.desktop.north,
            "modc+mod1+mods+Up",
            sizeof(config_bindings->keyboard.window.send_to.
                    desktop.north));
    safe_strncpy(config_bindings->keyboard.window.send_to.desktop.south,
            "modc+mod1+mods+Down",
            sizeof(config_bindings->keyboard.window.send_to.
                    desktop.south));
    safe_strncpy(config_bindings->keyboard.window.send_to.desktop.east,
            "modc+mod1+mods+Right",
            sizeof(config_bindings->keyboard.window.send_to.
                    desktop.east));
    safe_strncpy(config_bindings->keyboard.window.send_to.desktop.west,
            "modc+mod1+mods+Left",
            sizeof(config_bindings->keyboard.window.send_to.
                    desktop.west));
    safe_strncpy(config_bindings->keyboard.window.send_to.monitor.north,
            "modc+mod1+mod4+mods+Up",
            sizeof(config_bindings->keyboard.window.send_to.
                    monitor.north));
    safe_strncpy(config_bindings->keyboard.window.send_to.monitor.south,
            "modc+mod1+mod4+mods+Down",
            sizeof(config_bindings->keyboard.window.send_to.
                    monitor.south));
    safe_strncpy(config_bindings->keyboard.window.send_to.monitor.east,
            "modc+mod1+mod4+mods+Right",
            sizeof(config_bindings->keyboard.window.send_to.
                    monitor.east));
    safe_strncpy(config_bindings->keyboard.window.send_to.monitor.west,
            "modc+mod1+mod4+mods+Left",
            sizeof(config_bindings->keyboard.window.send_to.
                    monitor.west));

    /* Predetermined configuration for mouse bindings */
    LOGGER_TRACE("Setting default mouse bindings", L_NARG);
    safe_strncpy(config_bindings->mouse.window.move,
            "mod1+button1", sizeof(config_bindings->mouse.window.move));
    safe_strncpy(config_bindings->mouse.window.lower,
            "mod1+button2", sizeof(config_bindings->mouse.window.lower));
    safe_strncpy(config_bindings->mouse.window.resize,
            "mod1+button3", sizeof(config_bindings->mouse.window.resize));
    safe_strncpy(config_bindings->mouse.cycle.desktop.north,
            "mods+button4",
            sizeof(config_bindings->mouse.cycle.desktop.north));
    safe_strncpy(config_bindings->mouse.cycle.desktop.south,
            "mods+button5",
            sizeof(config_bindings->mouse.cycle.desktop.south));
    safe_strncpy(config_bindings->mouse.cycle.desktop.east,
            "button5", sizeof(config_bindings->mouse.cycle.desktop.east));
    safe_strncpy(config_bindings->mouse.cycle.desktop.west,
            "button4", sizeof(config_bindings->mouse.cycle.desktop.west));
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
        cJSON *wm;
        cJSON *desktop;
        cJSON *launch;
        cJSON *window;
        cJSON *cycle;

        wm = cJSON_GetObjectItem(keyboard, "wm");
        if (wm) {
            cJSON *wm_menus;

            wm_menus = cJSON_GetObjectItem(wm, "menus");
            if (wm_menus) {
                json_load_string(wm_menus, "root",
                        config_bindings->keyboard.wm.menus.root,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(wm_menus, "windows",
                        config_bindings->keyboard.wm.menus.windows,
                        CONFIG_MAX_LENGTH_BINDING);
            }
            json_load_string(wm, "search",
                    config_bindings->keyboard.wm.search,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(wm, "scratchpad",
                    config_bindings->keyboard.wm.scratchpad,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(wm, "redraw",
                    config_bindings->keyboard.wm.redraw,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(wm, "reload",
                    config_bindings->keyboard.wm.reload,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(wm, "quit",
                    config_bindings->keyboard.wm.quit,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(wm, "shortcuts",
                    config_bindings->keyboard.wm.shortcuts,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(wm, "fortune",
                    config_bindings->keyboard.wm.fortune,
                    CONFIG_MAX_LENGTH_BINDING);

            json_load_string(wm, "toggle-strutless-maximization",
                    config_bindings->keyboard.wm.toggle_strutless_maximize,
                    CONFIG_MAX_LENGTH_BINDING);
        }

        /* Desktop-level actions: switching, adding/removing, and the
         * show-desktop toggle; its own top-level sibling of 'wm'
         * above, not nested under it, matching 'struct keyboard_s'
         * itself (config.h). */
        desktop = cJSON_GetObjectItem(keyboard, "desktop");
        if (desktop) {
            cJSON *go_to;

            json_load_string(desktop, "add",
                    config_bindings->keyboard.desktop.add,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(desktop, "remove",
                    config_bindings->keyboard.desktop.remove,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(desktop, "show",
                    config_bindings->keyboard.desktop.show,
                    CONFIG_MAX_LENGTH_BINDING);

            /* Direct go-to shortcuts 0-9 */
            go_to = cJSON_GetObjectItem(desktop, "go-to");
            if (go_to != NULL) {
                static const char *keys[10] = {
                    "desktop0", "desktop1", "desktop2",
                    "desktop3", "desktop4", "desktop5",
                    "desktop6", "desktop7", "desktop8",
                    "desktop9"
                };
                for (int gi = 0; gi < 10; ++gi) {
                    json_load_string(go_to, keys[gi],
                        config_bindings->keyboard.desktop.go_to.
                            desktop[gi],
                            CONFIG_MAX_LENGTH_BINDING);
                }
            }
        }

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
            cJSON *window_send_to;

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
            json_load_string(window, "iconify-all",
                    config_bindings->keyboard.window.iconify_all,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "deiconify-all",
                    config_bindings->keyboard.window.deiconify_all,
                    CONFIG_MAX_LENGTH_BINDING);
            json_load_string(window, "arrange",
                    config_bindings->keyboard.window.arrange,
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

            window_send_to = cJSON_GetObjectItem(window, "send-to");
            if (window_send_to != NULL) {
                cJSON *send_to_desktop;
                cJSON *send_to_monitor;

                send_to_desktop = cJSON_GetObjectItem(window_send_to,
                        "desktop");
                if (send_to_desktop != NULL) {
                    json_load_string(send_to_desktop, "north",
                        config_bindings->keyboard.window.send_to.
                            desktop.north,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(send_to_desktop, "south",
                        config_bindings->keyboard.window.send_to.
                            desktop.south,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(send_to_desktop, "east",
                        config_bindings->keyboard.window.send_to.
                            desktop.east,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(send_to_desktop, "west",
                        config_bindings->keyboard.window.send_to.
                            desktop.west,
                            CONFIG_MAX_LENGTH_BINDING);
                }

                send_to_monitor = cJSON_GetObjectItem(window_send_to,
                        "monitor");
                if (send_to_monitor != NULL) {
                    json_load_string(send_to_monitor, "north",
                        config_bindings->keyboard.window.send_to.
                            monitor.north,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(send_to_monitor, "south",
                        config_bindings->keyboard.window.send_to.
                            monitor.south,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(send_to_monitor, "east",
                        config_bindings->keyboard.window.send_to.
                            monitor.east,
                            CONFIG_MAX_LENGTH_BINDING);
                    json_load_string(send_to_monitor, "west",
                        config_bindings->keyboard.window.send_to.
                            monitor.west,
                            CONFIG_MAX_LENGTH_BINDING);
                }
            }
        }

        /* Keybindings for cycling: desktop, icon, and window */
        cycle = cJSON_GetObjectItem(keyboard, "cycle");
        if (cycle) {
            cJSON *cdesktop;
            cJSON *cicon;
            cJSON *cwindow;

            cdesktop = cJSON_GetObjectItem(cycle, "desktop");
            if (cdesktop) {
                json_load_string(cdesktop, "north",
                        config_bindings->keyboard.cycle.desktop.north,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(cdesktop, "south",
                        config_bindings->keyboard.cycle.desktop.south,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(cdesktop, "east",
                        config_bindings->keyboard.cycle.desktop.east,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(cdesktop, "west",
                        config_bindings->keyboard.cycle.desktop.west,
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
                json_load_string(cdesktop, "north",
                        config_bindings->mouse.cycle.desktop.north,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(cdesktop, "south",
                        config_bindings->mouse.cycle.desktop.south,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(cdesktop, "east",
                        config_bindings->mouse.cycle.desktop.east,
                        CONFIG_MAX_LENGTH_BINDING);
                json_load_string(cdesktop, "west",
                        config_bindings->mouse.cycle.desktop.west,
                        CONFIG_MAX_LENGTH_BINDING);
            }
        }
    }

    /* Free memory */
    cJSON_Delete(json);

    return 0;
}
