/**
 * @file config.c
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
#include <stdlib.h>     /* NULL, calloc, free */
#include <string.h>     /* memcpy */

/* Utils includes */
#include <utils/config/json.h>
#include <utils/config/path.h>
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/ctxmenu.h>
#include <defs/desktop.h>
#include <defs/loop.h>
#include <defs/sn.h>

/* Project includes */
#include <logger.h>

#include <sn.h>

/* Local includes */
#include <config.h>
#include <config/internal.h>


/* Resolve the configuration directory from a prefix, or environment
 * variables when none is given (see config.h for the fallback order) */
void config_resolve_dir(const char *config_dir_prefix,
        char *config_dir_base)
{
    char temp_path[CONFIG_MAX_LENGTH_PATH_BASE];

    if (config_dir_prefix) {
        snprintf(temp_path, CONFIG_MAX_LENGTH_PATH_BASE,
                "%s", config_dir_prefix);
        path_simplify(temp_path);
        safe_strncpy(config_dir_base, temp_path,
                CONFIG_MAX_LENGTH_PATH_BASE);
        return;
    }

    xdg_resolve_dir(XDG_DIR_CONFIG, "./" CONFIG_DIR_BASE,
            config_dir_base, CONFIG_MAX_LENGTH_PATH_BASE);
}


/* Settle a theme's own final display name, once it is known whether
 * a theme file was actually loaded and whether that file itself set
 * its own "name" */
void ci_config_resolve_theme_name(struct config_theme_s *theme,
        const char *theme_file_name, bool theme_loaded)
{
    char combined[sizeof(theme->name)];
    size_t name_len;
    size_t file_len;
    size_t pos;
    size_t avail;
    size_t fit;

    if (theme == NULL) {
        return;
    }

    if (theme_file_name == NULL || theme_file_name[0] == '\0' ||
            !theme_loaded) {
        safe_strncpy(theme->name, "Default (builtin)",
                sizeof(theme->name));
        return;
    }

    if (theme->name[0] == '\0') {
        /* The loaded file set no "name" of its own: falls back to
         * the file's own short name (the same string "theme":
         * "<this>" in memguard.json/config.json names, not a path or
         * the ".json" extension), same as if that had been its
         * "name" all along. */
        safe_strncpy(theme->name, theme_file_name, sizeof(theme->name));
        return;
    }

    /* Built with explicit, provably bounded 'memcpy' calls rather
     * than 'snprintf' with two '%s' arguments of a priori unknown
     * length: GCC's own '-Wformat-truncation' analysis cannot trace
     * that the combined length here can never exceed 'combined''s own
     * size through reasoning this indirect, and warns as if the call
     * could write past it even though it provably cannot -- the same
     * issue, and the same fix, already applied in
     * 's_message_wrap_text' (menu/dialog/message.c).  Truncates
     * either string in turn (theme name first, then the file name)
     * rather than failing outright when the two together would not
     * fit: this is a display label, not something anything else
     * parses back apart, so a truncated one is a fully acceptable
     * outcome here, unlike it would be for, say, a file path. */
    name_len = safe_strlen(theme->name);
    file_len = safe_strlen(theme_file_name);

    /* Reserves the 4 bytes " (" + ")" + '\0' need beyond whatever
     * room the two strings themselves take up. */
    avail = (sizeof(combined) > 4u) ? sizeof(combined) - 4u : 0u;
    fit = (name_len > avail) ? avail : name_len;
    memcpy(combined, theme->name, fit);
    pos = fit;

    combined[pos] = ' ';
    ++pos;
    combined[pos] = '(';
    ++pos;

    /* Reserves the 2 bytes ')' + '\0' still need beyond 'pos'. */
    avail = (pos < sizeof(combined) - 2u) ? sizeof(combined) - 2u - pos
        : 0u;
    fit = (file_len > avail) ? avail : file_len;
    memcpy(combined + pos, theme_file_name, fit);
    pos += fit;

    combined[pos] = ')';
    ++pos;
    combined[pos] = '\0';

    safe_strncpy(theme->name, combined, sizeof(theme->name));
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


/* Populate default values for one theme structure, used both as the
 * compiled-in fallback theme and, before applying any theme file
 * found, as the known-good starting point that file's own fields
 * then overlay */
void config_set_default_theme_values(struct config_theme_s *theme)
{
    LOGGER_TRACE("Setting default theme", L_NARG);
    /* Left empty here on purpose, rather than a name like "Default
     * theme" outright: 'ci_config_resolve_theme_name' (config.c)
     * settles on the final name afterward, once it knows whether a
     * theme file was actually loaded and whether that file set its
     * own "name" (empty here means it did not), and this field
     * staying empty is exactly the signal it checks for that. */
    theme->name[0] = '\0';

    theme->window.is_decorated = true;
    theme->window.titlebar.height = 22u;
    theme->window.titlebar.alignment = CONFIG_TITLEBAR_ALIGN_CENTER;
    theme->window.titlebar.padding.horizontal = 2u;
    theme->window.titlebar.padding.vertical = 2u;

    theme->window.titlebar.buttons.left[0] =
        CONFIG_TITLEBAR_BUTTON_PIN;
    theme->window.titlebar.buttons.left[1] =
        CONFIG_TITLEBAR_BUTTON_LAYER;
    theme->window.titlebar.buttons.left_count = 2u;

    theme->window.titlebar.buttons.right[0] =
        CONFIG_TITLEBAR_BUTTON_CLOSE;
    theme->window.titlebar.buttons.right[1] =
        CONFIG_TITLEBAR_BUTTON_MAXIMIZE;
    theme->window.titlebar.buttons.right[2] =
        CONFIG_TITLEBAR_BUTTON_SHADE;
    theme->window.titlebar.buttons.right[3] =
        CONFIG_TITLEBAR_BUTTON_ICONIZE;
    theme->window.titlebar.buttons.right_count = 4u;

    theme->window.titlebar.buttons.color.on =
        json_hex2uint32("253F60");
    theme->window.titlebar.buttons.color.off =
        json_hex2uint32("7086A0");

    safe_strncpy(theme->window.active.font,
            "fixed bold", sizeof(theme->window.active.font));
    theme->window.active.color.background =
        json_hex2uint32("9AAEC8");
    theme->window.active.color.foreground =
        json_hex2uint32("253040");
    theme->window.active.border.color = json_hex2uint32("4A5566");
    theme->window.active.border.width = 2u;

    safe_strncpy(theme->window.inactive.font,
            "fixed", sizeof(theme->window.inactive.font));
    theme->window.inactive.color.background =
        json_hex2uint32("D0D9E5");
    theme->window.inactive.color.foreground =
        json_hex2uint32("4A5566");
    theme->window.inactive.border.color = json_hex2uint32("7F9AB6");
    theme->window.inactive.border.width = 2u;

    theme->icon.is_captioned = true;
    theme->icon.show_pixmaps = true;
    theme->icon.show_hints = true;

    safe_strncpy(theme->icon.active.font,
            "fixed bold", sizeof(theme->icon.active.font));
    theme->icon.active.color.background =
        json_hex2uint32("9AAEC8");
    theme->icon.active.color.foreground =
        json_hex2uint32("253040");
    theme->icon.active.border.color = json_hex2uint32("4A5566");
    theme->icon.active.border.width = 1u;

    safe_strncpy(theme->icon.inactive.font,
            "fixed", sizeof(theme->icon.inactive.font));
    theme->icon.inactive.color.background =
        json_hex2uint32("D0D9E5");
    theme->icon.inactive.color.foreground =
        json_hex2uint32("4A5566");
    theme->icon.inactive.border.color = json_hex2uint32("7F9AB6");
    theme->icon.inactive.border.width = 1u;

    safe_strncpy(theme->systray.style.font,
            "fixed bold", sizeof(theme->systray.style.font));
    theme->systray.style.color.background =
        json_hex2uint32("D0D9E5");
    theme->systray.style.color.foreground =
        json_hex2uint32("4A5566");
    theme->systray.style.border.color = json_hex2uint32("7F9AB6");
    theme->systray.style.border.width = 1u;
    theme->systray.height = 22u;
    theme->systray.pixmap.size = 24u;
    theme->systray.pixmap.padding = 4u;
    /* 'height' (22) actually sits below 'pixmap.size' (24) here: per
     * 's_systray_apply_config' in systray.c, the tray's own real,
     * effective height is clamped up to whichever of the two is
     * larger, so 24 -- not 22 -- is what actually applies, leaving no
     * room for 'systray.text.valign' to have any visible effect. */
    theme->systray.text.gap = 12u;
    theme->systray.text.valign = CONFIG_SYSTRAY_TEXT_VALIGN_CENTER;

    /* Same hue family (~213 degrees) as the rest of the theme's
     * D0D9E5/4A5566-family colors, but deliberately darker than the
     * UI chrome: a desktop background is a large, full-screen area
     * rather than a small UI element, so it wants a more neutral,
     * less attention-grabbing tone, and staying darker gives windows
     * placed on top of it more contrast to stand out against than a
     * light background would.  Landed on this specific value (rather
     * than an even darker one first tried) so it does not sit almost
     * as dark as the theme's own text/border colors, which left it
     * feeling heavier than a full-screen area calls for. */
    theme->desktop.color.background = json_hex2uint32("5F7187");

    safe_strncpy(theme->menu.unselected.font,
            "fixed", sizeof(theme->menu.unselected.font));
    theme->menu.unselected.color.background =
        json_hex2uint32("D0D9E5");
    theme->menu.unselected.color.foreground =
        json_hex2uint32("4A5566");
    theme->menu.unselected.border.color = json_hex2uint32("7F9AB6");
    theme->menu.unselected.border.width = 0u;

    safe_strncpy(theme->menu.selected.font,
            "fixed", sizeof(theme->menu.selected.font));
    theme->menu.selected.color.background =
        json_hex2uint32("9AAEC8");
    theme->menu.selected.color.foreground =
        json_hex2uint32("253040");
    theme->menu.selected.border.color = json_hex2uint32("4A5566");
    theme->menu.selected.border.width = 0u;

    safe_strncpy(theme->menu.label.font,
            "fixed", sizeof(theme->menu.label.font));
    theme->menu.label.color.background =
        json_hex2uint32("48607F");
    /* Picked for a WCAG contrast ratio of ~4.5:1 against this
     * background (the same bar as any other normal-weight text in
     * the theme): the border color this foreground used to reuse
     * only reached ~2:1 against a light background, too low for
     * text meant to be read normally rather than treated as a
     * de-emphasized secondary state; the ratio itself is the same
     * either way around, since contrast between two colors does not
     * depend on which one is foreground and which is background. */
    theme->menu.label.color.foreground =
        json_hex2uint32("D0D9E5");
    theme->menu.label.border.color = json_hex2uint32("7F9AB6");
    theme->menu.label.border.width = 0u;

    /* Picked for a WCAG contrast ratio of ~3:1 against the menu's own
     * background: low enough to still read as visibly de-emphasized
     * (this is disabled, secondary text, not meant to compete with
     * normal menu text), but not the ~1.7:1 the previous color gave,
     * which was too low to reliably read as text at all. */
    theme->menu.disabled_foreground = json_hex2uint32("717B88");
    theme->menu.separator_color = json_hex2uint32("7F9AB6");
    theme->menu.border.color = json_hex2uint32("7F9AB6");
    theme->menu.border.width = 2u;
    theme->menu.padding.horizontal = (uint32_t) WM_CTXMENU_PAD_X;
    theme->menu.padding.vertical = (uint32_t) WM_CTXMENU_PAD_Y;
    theme->menu.show_pixmaps = true;

    theme->dialog.background = json_hex2uint32("D0D9E5");
    theme->dialog.border.color = json_hex2uint32("7F9AB6");
    theme->dialog.border.width = 2u;

    safe_strncpy(theme->dialog.label.font,
            "fixed bold", sizeof(theme->dialog.label.font));
    theme->dialog.label.foreground = json_hex2uint32("4A5566");
    theme->dialog.label.padding.horizontal = 12u;
    theme->dialog.label.padding.vertical = 12u;

    safe_strncpy(theme->dialog.button.unselected.font,
            "fixed", sizeof(theme->dialog.button.unselected.font));
    theme->dialog.button.unselected.color.background =
        json_hex2uint32("D0D9E5");
    theme->dialog.button.unselected.color.foreground =
        json_hex2uint32("4A5566");
    theme->dialog.button.unselected.border.color =
        json_hex2uint32("7F9AB6");
    theme->dialog.button.unselected.border.width = 1u;

    safe_strncpy(theme->dialog.button.selected.font,
            "fixed bold", sizeof(theme->dialog.button.selected.font));
    theme->dialog.button.selected.color.background =
        json_hex2uint32("9AAEC8");
    theme->dialog.button.selected.color.foreground =
        json_hex2uint32("253040");
    theme->dialog.button.selected.border.color =
        json_hex2uint32("4A5566");
    theme->dialog.button.selected.border.width = 1u;

    theme->dialog.button.gap = 24u;
    theme->dialog.button.padding.horizontal = 12u;
    theme->dialog.button.padding.vertical = 6u;

    safe_strncpy(theme->overlay.font, "fixed", sizeof(theme->overlay.font));
    theme->overlay.color.background = json_hex2uint32("D0D9E5");
    theme->overlay.color.foreground = json_hex2uint32("4A5566");
    theme->overlay.border.color = json_hex2uint32("7F9AB6");
    theme->overlay.border.width = 1u;

    theme->xsettings.is_enabled = false;
    theme->xsettings.dpi = 96u;
    safe_strncpy(theme->xsettings.theme.gtk_theme_name, "Adwaita",
            CONFIG_MAX_LENGTH_NAME);
    safe_strncpy(theme->xsettings.theme.icon_theme_name, "Adwaita",
            CONFIG_MAX_LENGTH_NAME);
    safe_strncpy(theme->xsettings.theme.cursor_theme_name,
            "Adwaita", CONFIG_MAX_LENGTH_NAME);
    theme->xsettings.theme.cursor_theme_size = 24u;
}


/* Populate the configuration structure with default values */
void config_set_default_values(config_td *config)
{
    /* Assign predetermined values for base configuration */
    config->base.theme[0] = '\0';
    config->base.screen_count = 1;

    /* Desktop-navigation and reserved-space behavior (config.json's
     * own top-level 'desktop', a sibling of 'topology'; see config_
     * desktop_s's own doc comment in config.h) -- meaningless with
     * only one desktop for 'warp'/'cycle', but set regardless of how
     * many desktops end up configured, the same as every other
     * default here. */
    config->desktops.warp = true;
    config->desktops.cycle = true;
    config->desktops.margins.top = 0u;
    config->desktops.margins.right = 0u;
    config->desktops.margins.bottom = 0u;
    config->desktops.margins.left = 0u;

    /* Every screen and desktop slot the fixed-size 'screens' and
     * 'desktops' arrays can ever hold gets the sentinel here, not
     * just the ones this function is about to treat as active by
     * default below: 'config_load_base' can fill in far more screens
     * or desktops than that default, straight into these same
     * arrays, and a slot it does not itself set a color for would
     * otherwise still be sitting at zero from this whole structure's
     * initial 'calloc' rather than at the sentinel, which reads as an
     * opaque black background instead of falling back to the theme's
     * own color the way an genuinely unset one should. */
    LOGGER_TRACE("Setting background-color sentinel for every" \
            " possible screen and desktop slot", L_NARG);
    for (unsigned int i = 0; i < CONFIG_MAX_SCREENS; ++i) {
        for (unsigned int j = 0; j < CONFIG_MAX_DESKTOPS; ++j) {
            config->base.screens[i].desktops[j].settings.background.color
                = WM_DESKTOP_BG_COLOR_UNSET;
        }
    }

    LOGGER_TRACE("Setting configuration for each screen", L_NARG);
    for (unsigned int i = 0; i < config->base.screen_count; ++i) {
        /* 4 desktops by default, unless 'CONFIG_MAX_DESKTOPS' itself
         * is smaller than that.  Purely a fallback for when nothing
         * else specifies a count at all: a 'config.json' that
         * specifies its own 'desktops.count' always overrides this
         * default, since 'config_load_base' runs after this and
         * simply replaces it; nothing caps that value back down
         * afterward. */
        uint32_t desktop_default = 4u;

        config->base.screens[i].desktop_count =
            (CONFIG_MAX_DESKTOPS < desktop_default)
                ? CONFIG_MAX_DESKTOPS : desktop_default;
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
        }
    }

    LOGGER_TRACE("Setting default base programs", L_NARG);
    safe_strncpy(config->base.programs.terminal,
            "xterm", sizeof(config->base.programs.terminal));
    safe_strncpy(config->base.programs.launcher,
            "gmrun", sizeof(config->base.programs.launcher));
    safe_strncpy(config->base.programs.file_manager,
            "pcmanfm", sizeof(config->base.programs.file_manager));
    safe_strncpy(config->base.programs.editor,
            "gvim", sizeof(config->base.programs.editor));
    safe_strncpy(config->base.programs.web_browser,
            "firefox", sizeof(config->base.programs.web_browser));
    config->base.windows.move_step = 10;
    /* usually overridden by hints */
    config->base.windows.resize_step = 20;
    config->base.windows.snap = 4;
    config->base.windows.show_geom = true;
    config->base.windows.gravity = CONFIG_GRAVITY_NORTH_WEST;
    config->base.windows.focus_policy = CONFIG_FOCUS_POLICY_CLICK;
    config->base.windows.placement_policy = CONFIG_PLACEMENT_POLICY_SMART;
    config->base.windows.monitor_policy = CONFIG_PLACEMENT_MONITOR_POINTER;
    config->base.windows.group_related = true;
    config->base.windows.focus.is_new_focused = true;
    config->base.windows.focus.is_raised_on_focus = false;
    config->base.icons.placement_policy = CONFIG_ICON_PLACEMENT_SMART;
    config->base.icons.show_geom = false;
    config->base.enable_emergency_shortcut = false;
    config->base.enable_fortune_shortcut = true;
    config->base.startup_notification.is_enabled = true;
    config->base.startup_notification.timeout_seconds =
        (uint32_t) SN_TIMEOUT_SECONDS;
    config->base.show_desktop_overlay = true;
    config->base.menus.root.position = CONFIG_MENU_POSITION_UNDER_MOUSE;
    config->base.menus.windows.position = CONFIG_MENU_POSITION_UNDER_MOUSE;
    config->base.systray.is_enabled = true;
    config->base.systray.is_embedding_enabled = true;
    config->base.systray.reserve_space = false;
    config->base.systray.margins.top = 0u;
    config->base.systray.margins.right = 0u;
    config->base.systray.margins.bottom = 0u;
    config->base.systray.margins.left = 0u;
    config->base.systray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    config->base.systray.monitor.anchor = CONFIG_SYSTRAY_MONITOR_SURFACE;
    config->base.systray.monitor.index = 0u;
    config->base.systray.order = CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT;
    config->base.systray.layer = CONFIG_SYSTRAY_LAYER_BELOW;
    config->base.systray.clock.is_enabled = true;
    safe_strncpy(config->base.systray.clock.format,
            "%a %R", sizeof(config->base.systray.clock.format));

    config->base.systray.battery.is_enabled = false;
    config->base.systray.battery.threshold.charged = 100u;
    config->base.systray.battery.threshold.low = 20u;
    config->base.systray.battery.threshold.critical = 5u;
    config->base.systray.battery.backend.type = CONFIG_BATTERY_BACKEND_ACPI;
    config->base.systray.battery.backend.number = 0u;
    config->base.systray.battery.poll_seconds =
        (uint32_t) WM_SYSTRAY_BATTERY_POLL_SECONDS;

    config->base.systray.text.order[0] = CONFIG_SYSTRAY_TEXT_CLOCK;
    config->base.systray.text.order[1] = CONFIG_SYSTRAY_TEXT_BATTERY;
    config->base.systray.text.order_count = 2u;
    config->base.systray.text.position = CONFIG_SYSTRAY_TEXT_LEFT;

    /* Predetermined values for RandR output profile management */
    LOGGER_TRACE("Setting default RandR configuration", L_NARG);
    config->randr.is_enabled = false;
    config->randr.output_count = 0u;

    /* Assign predetermined values for bindings modifiers */
    LOGGER_TRACE("Setting default bindings modifiers", L_NARG);
    safe_strncpy(config->bindings.modc,
            "Control", sizeof(config->bindings.modc));
    safe_strncpy(config->bindings.mods,
            "Shift", sizeof(config->bindings.mods));
    safe_strncpy(config->bindings.modl,
            "Caps_Lock", sizeof(config->bindings.modl));
    safe_strncpy(config->bindings.mod1, "Alt", sizeof(config->bindings.mod1));
    safe_strncpy(config->bindings.mod2,
            "Num_Lock", sizeof(config->bindings.mod2));
    safe_strncpy(config->bindings.mod3, "", sizeof(config->bindings.mod3));
    safe_strncpy(config->bindings.mod4,
            "Super", sizeof(config->bindings.mod4));
    safe_strncpy(config->bindings.mod5,
            "Hyper", sizeof(config->bindings.mod5));

    /* Predetermined configuration for keybindings */
    LOGGER_TRACE("Setting default keybindings", L_NARG);
    safe_strncpy(config->bindings.keyboard.launch.terminal,
            "modc+mod1+Return",
            sizeof(config->bindings.keyboard.launch.terminal));
    safe_strncpy(config->bindings.keyboard.launch.launcher,
            "modc+mod1+r", sizeof(config->bindings.keyboard.launch.launcher));
    safe_strncpy(config->bindings.keyboard.launch.file_manager,
            "modc+mod1+q",
            sizeof(config->bindings.keyboard.launch.file_manager));
    safe_strncpy(config->bindings.keyboard.launch.web_browser,
            "modc+mod1+w",
            sizeof(config->bindings.keyboard.launch.web_browser));
    safe_strncpy(config->bindings.keyboard.launch.editor,
            "modc+mod1+e", sizeof(config->bindings.keyboard.launch.editor));
    safe_strncpy(config->bindings.keyboard.wm.menus.root,
            "modc+mod1+mods+m",
            sizeof(config->bindings.keyboard.wm.menus.root));
    safe_strncpy(config->bindings.keyboard.wm.menus.windows,
            "modc+mod1+mods+w",
            sizeof(config->bindings.keyboard.wm.menus.windows));
    safe_strncpy(config->bindings.keyboard.wm.search,
            "modc+mod4+mods+s",
            sizeof(config->bindings.keyboard.wm.search));
    safe_strncpy(config->bindings.keyboard.window.close,
            "modc+mod1+c", sizeof(config->bindings.keyboard.window.close));
    safe_strncpy(config->bindings.keyboard.window.decorate,
            "modc+mod1+d", sizeof(config->bindings.keyboard.window.decorate));
    safe_strncpy(config->bindings.keyboard.window.fullscreen,
            "modc+mod1+f",
            sizeof(config->bindings.keyboard.window.fullscreen));
    safe_strncpy(config->bindings.keyboard.window.hide,
            "modc+mod1+mods+u", sizeof(config->bindings.keyboard.window.hide));
    safe_strncpy(config->bindings.keyboard.window.iconify,
            "modc+mod1+i", sizeof(config->bindings.keyboard.window.iconify));
    safe_strncpy(config->bindings.keyboard.window.iconify_all,
            "modc+mod4+mods+i",
            sizeof(config->bindings.keyboard.window.iconify_all));
    safe_strncpy(config->bindings.keyboard.window.deiconify_all,
            "modc+mod4+mods+d",
            sizeof(config->bindings.keyboard.window.deiconify_all));
    safe_strncpy(config->bindings.keyboard.window.arrange,
            "modc+mod1+mods+a",
            sizeof(config->bindings.keyboard.window.arrange));
    safe_strncpy(config->bindings.keyboard.window.info,
            "modc+mod1+mods+i", sizeof(config->bindings.keyboard.window.info));
    safe_strncpy(config->bindings.keyboard.window.kill,
            "modc+mod1+mods+Escape",
            sizeof(config->bindings.keyboard.window.kill));
    safe_strncpy(config->bindings.keyboard.window.maximize,
            "modc+mod1+m", sizeof(config->bindings.keyboard.window.maximize));
    safe_strncpy(config->bindings.keyboard.window.next_monitor,
            "modc+mod1+mods+n",
            sizeof(config->bindings.keyboard.window.next_monitor));
    safe_strncpy(config->bindings.keyboard.window.pin,
            "modc+mod1+p", sizeof(config->bindings.keyboard.window.pin));
    safe_strncpy(config->bindings.keyboard.window.layer,
            "modc+mod1+mods+y",
            sizeof(config->bindings.keyboard.window.layer));
    safe_strncpy(config->bindings.keyboard.window.shade,
            "modc+mod1+s", sizeof(config->bindings.keyboard.window.shade));
    safe_strncpy(config->bindings.keyboard.cycle.desktop.prev,
            "modc+mod1+Left",
            sizeof(config->bindings.keyboard.cycle.desktop.prev));
    safe_strncpy(config->bindings.keyboard.cycle.desktop.next,
            "modc+mod1+Right",
            sizeof(config->bindings.keyboard.cycle.desktop.next));
    safe_strncpy(config->bindings.keyboard.cycle.icon.prev,
            "modc+mod1+mods+Tab",
            sizeof(config->bindings.keyboard.cycle.icon.prev));
    safe_strncpy(config->bindings.keyboard.cycle.icon.next,
            "modc+mod1+Tab",
            sizeof(config->bindings.keyboard.cycle.icon.next));
    safe_strncpy(config->bindings.keyboard.cycle.window.prev,
            "mod1+mods+Tab",
            sizeof(config->bindings.keyboard.cycle.window.prev));
    safe_strncpy(config->bindings.keyboard.cycle.window.next,
            "mod1+Tab", sizeof(config->bindings.keyboard.cycle.window.next));
    safe_strncpy(config->bindings.keyboard.wm.redraw,
            "modc+mod1+mods+r", sizeof(config->bindings.keyboard.wm.redraw));
    safe_strncpy(config->bindings.keyboard.wm.reload,
            "modc+mod1+mods+c", sizeof(config->bindings.keyboard.wm.reload));
    safe_strncpy(config->bindings.keyboard.wm.quit,
            "modc+mod1+mods+x", sizeof(config->bindings.keyboard.wm.quit));
    safe_strncpy(config->bindings.keyboard.wm.shortcuts,
            "modc+mod4+F1", sizeof(config->bindings.keyboard.wm.shortcuts));
    safe_strncpy(config->bindings.keyboard.wm.show_desktop,
            "modc+mod1+mods+d",
            sizeof(config->bindings.keyboard.wm.show_desktop));

    /* Predetermined goto-desktop shortcuts for desktops 0-9 */
    LOGGER_TRACE("Setting default go-to keybindings", L_NARG);
    safe_strncpy(config->bindings.keyboard.wm.go_to.desktop[0],
            "modc+mod1+0",
            sizeof(config->bindings.keyboard.wm.go_to.desktop[0]));
    safe_strncpy(config->bindings.keyboard.wm.go_to.desktop[1],
            "modc+mod1+1",
            sizeof(config->bindings.keyboard.wm.go_to.desktop[1]));
    safe_strncpy(config->bindings.keyboard.wm.go_to.desktop[2],
            "modc+mod1+2",
            sizeof(config->bindings.keyboard.wm.go_to.desktop[2]));
    safe_strncpy(config->bindings.keyboard.wm.go_to.desktop[3],
            "modc+mod1+3",
            sizeof(config->bindings.keyboard.wm.go_to.desktop[3]));
    safe_strncpy(config->bindings.keyboard.wm.go_to.desktop[4],
            "modc+mod1+4",
            sizeof(config->bindings.keyboard.wm.go_to.desktop[4]));
    safe_strncpy(config->bindings.keyboard.wm.go_to.desktop[5],
            "modc+mod1+5",
            sizeof(config->bindings.keyboard.wm.go_to.desktop[5]));
    safe_strncpy(config->bindings.keyboard.wm.go_to.desktop[6],
            "modc+mod1+6",
            sizeof(config->bindings.keyboard.wm.go_to.desktop[6]));
    safe_strncpy(config->bindings.keyboard.wm.go_to.desktop[7],
            "modc+mod1+7",
            sizeof(config->bindings.keyboard.wm.go_to.desktop[7]));
    safe_strncpy(config->bindings.keyboard.wm.go_to.desktop[8],
            "modc+mod1+8",
            sizeof(config->bindings.keyboard.wm.go_to.desktop[8]));
    safe_strncpy(config->bindings.keyboard.wm.go_to.desktop[9],
            "modc+mod1+9",
            sizeof(config->bindings.keyboard.wm.go_to.desktop[9]));

    /* Predetermined configuration for movement with keyboard */
    LOGGER_TRACE("Setting default movement/resizing keybindings",
            L_NARG);
    safe_strncpy(config->bindings.keyboard.window.move.relative.right,
            "modc+mod1+l",
            sizeof(config->bindings.keyboard.window.move.relative.right));
    safe_strncpy(config->bindings.keyboard.window.move.relative.left,
            "modc+mod1+h",
            sizeof(config->bindings.keyboard.window.move.relative.left));
    safe_strncpy(config->bindings.keyboard.window.move.relative.up,
            "modc+mod1+k",
            sizeof(config->bindings.keyboard.window.move.relative.up));
    safe_strncpy(config->bindings.keyboard.window.move.relative.down,
            "modc+mod1+j",
            sizeof(config->bindings.keyboard.window.move.relative.down));
    safe_strncpy(config->bindings.keyboard.window.move.absolute.center,
            "modc+mod1+g",
            sizeof(config->bindings.keyboard.window.move.absolute.center));
    safe_strncpy(config->bindings.keyboard.window.move.absolute.top_left,
            "modc+mod1+y",
            sizeof(config->bindings.keyboard.window.move.absolute.top_left));
    safe_strncpy(config->bindings.keyboard.window.move.absolute.top_right,
            "modc+mod1+u",
            sizeof(config->bindings.keyboard.window.move.absolute.top_right));
    safe_strncpy(config->bindings.keyboard.window.move.absolute.bottom_left,
            "modc+mod1+b",
            sizeof(config->bindings.keyboard.window.move.absolute.
                    bottom_left));
    safe_strncpy(config->bindings.keyboard.window.move.absolute.bottom_right,
            "modc+mod1+n",
            sizeof(config->bindings.keyboard.window.move.absolute.
                    bottom_right));
    safe_strncpy(config->bindings.keyboard.window.resize.right,
            "modc+mod1+mods+l",
            sizeof(config->bindings.keyboard.window.resize.right));
    safe_strncpy(config->bindings.keyboard.window.resize.left,
            "modc+mod1+mods+h",
            sizeof(config->bindings.keyboard.window.resize.left));
    safe_strncpy(config->bindings.keyboard.window.resize.up,
            "modc+mod1+mods+k",
            sizeof(config->bindings.keyboard.window.resize.up));
    safe_strncpy(config->bindings.keyboard.window.resize.down,
            "modc+mod1+mods+j",
            sizeof(config->bindings.keyboard.window.resize.down));

    /* Predetermined configuration for mouse bindings */
    LOGGER_TRACE("Setting default mouse bindings", L_NARG);
    safe_strncpy(config->bindings.mouse.window.move,
            "mod1+button1", sizeof(config->bindings.mouse.window.move));
    safe_strncpy(config->bindings.mouse.window.lower,
            "mod1+button2", sizeof(config->bindings.mouse.window.lower));
    safe_strncpy(config->bindings.mouse.window.resize,
            "mod1+button3", sizeof(config->bindings.mouse.window.resize));
    safe_strncpy(config->bindings.mouse.cycle.desktop.prev,
            "button4", sizeof(config->bindings.mouse.cycle.desktop.prev));
    safe_strncpy(config->bindings.mouse.cycle.desktop.next,
            "button5", sizeof(config->bindings.mouse.cycle.desktop.next));

    config_set_default_theme_values(&config->theme);
}


/** Path of the theme file 'config_load' most recently found specified
 *  by 'config.json' but missing; empty when none is currently
 *  missing */
static char s_missing_theme_file[CONFIG_MAX_LENGTH_PATH_THEME] = "";


/* Clear whichever theme file config_load last recorded as specified
 * but not actually found */
void config_missing_theme_reset(void)
{
    s_missing_theme_file[0] = '\0';
}


/* The theme file path config_load most recently found specified but
 * missing, if any */
const char *config_missing_theme_get(void)
{
    return (s_missing_theme_file[0] != '\0') ? s_missing_theme_file : NULL;
}


/* Load all the configuration */
int config_load(config_td *config, const char *config_prefix)
{
    char config_dir[CONFIG_MAX_LENGTH_PATH_BASE];
    char config_base_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_bindings_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    char config_theme_file[CONFIG_MAX_LENGTH_PATH_THEME];
    char config_randr_file[CONFIG_MAX_LENGTH_PATH_CONFIG];

    config_resolve_dir(config_prefix, config_dir);

    LOGGER_DEBUG("Loading configuration from files on: '%s'",
            config_dir);

    /* Set main base configuration path */
    snprintf(config_base_file, sizeof(config_base_file),
            "%s/%s", config_dir, CONFIG_FILENAME_BASE);

    /* Load base configuration */
    if (config_load_base(config_base_file, &(config->base),
                &(config->desktops)) != 0) {
        LOGGER_WARNING("Base configuration could not be loaded;" \
                " default values will be used", L_NARG);
        return 1;
    }
    LOGGER_DEBUG("Loaded base configuration from '%s'", config_base_file);

    /* Set bindings configuration path */
    snprintf(config_bindings_file, sizeof(config_bindings_file),
            "%s/%s", config_dir, CONFIG_FILENAME_BINDINGS);

    /* Load bindings */
    if (config_load_bindings(config_bindings_file,
                &(config->bindings)) != 0) {
        LOGGER_ERROR("Failed to load bindings from:" \
                " '%s'; default bindings will be used",
                config_bindings_file);
    } else {
        LOGGER_DEBUG("Loaded key/mouse bindings from '%s'",
                config_bindings_file);
    }

    /* Set theme file path */
    snprintf(config_theme_file,
            sizeof(config_theme_file),
            "%s/%s/%s.json", config_dir, CONFIG_DIR_THEMES,
            config->base.theme);

    /* Reset every field back to its own known default first, on every
     * call here, not just the first: 'config_load_theme' below only
     * ever overwrites whichever fields the theme file itself
     * specifies, so without this a reload that switched to a theme
     * missing some field the previous one did specify would leave
     * that field stuck at the old theme's own value instead of
     * falling back to this default. */
    config_set_default_theme_values(&config->theme);

    /* Load theme if it's specified, i.e., not empty string */
    if (safe_strlen(config->base.theme) == 0) {
        LOGGER_NOTICE("No theme specified in base configuration;" \
                " default will be used", L_NARG);
        ci_config_resolve_theme_name(&config->theme, config->base.theme,
                false);
    } else {
        /* Snapshot the syntax-error count before attempting the load,
         * so a load failure can be told apart from one that
         * 'json_load_config' (via 'config_load_theme') already
         * recorded there itself: a theme file that exists but fails
         * to parse is a syntax error like any other JSON file's, and
         * already covered that way; a theme file that simply is not
         * there at all is a different, narrower case, worth its own
         * note (see 'config_missing_theme_get') precisely because
         * that one, unlike a syntax error, is otherwise silent by
         * design. */
        uint32_t syntax_errors_before = json_syntax_errors_count();
        bool theme_loaded;

        LOGGER_DEBUG("Loading theme '%s' from '%s'",
                config->base.theme, config_theme_file);
        theme_loaded = (config_load_theme(config_theme_file,
                    &(config->theme)) == 0);
        if (!theme_loaded) {
            LOGGER_WARNING("Failed to load theme from:" \
                    " '%s'; default theme will be used",
                    config_theme_file);
            if (json_syntax_errors_count() == syntax_errors_before) {
                safe_strncpy(s_missing_theme_file, config_theme_file,
                        sizeof(s_missing_theme_file));
            }
        }
        ci_config_resolve_theme_name(&config->theme, config->base.theme,
                theme_loaded);
        if (theme_loaded) {
            LOGGER_DEBUG("Loaded theme '%s' (\"%s\") from '%s'",
                    config->base.theme, config->theme.name,
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
