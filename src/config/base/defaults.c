/**
 * @file config/base/defaults.c
 *
 * @brief Compiled-in default values for the base and desktop-
 *        navigation configuration structures
 *
 * One of the files @c config/base/ is made of;
 * @c config_set_default_base_values is used both as the initial
 * process-wide default and, before applying @c config.json (or
 * @c memguard.json) found, as the known-good starting point that
 * file's fields then overlay.
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

/* Default initial values */
#include <defs/desktop.h>
#include <defs/loop.h>
#include <defs/sn.h>

/* Project includes */
#include <logger.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Local includes */
#include <config.h>


/* Populate default values for the base and desktop-navigation
 * configuration structures, used both as the initial process-wide
 * default and, before applying config.json (or 'memguard.json') found,
 * as the known-good starting point that file's fields then
 * overlay */
void config_set_default_base_values(struct config_base_s *config_base,
        struct config_desktop_s *config_desktop)
{
    LOGGER_TRACE("Setting default base configuration", L_NARG);
    config_base->theme[0] = '\0';
    config_base->screen_count = 1;

    /* Desktop-navigation and reserved-space behavior ('config.json''s
     * top-level 'desktop', a sibling of 'topology'; see
     * config_desktop_s's comment in 'config.h').  Meaningless with only
     * one desktop for 'warp_on_edge_drag'/'wrap_at_bounds', but set
     * regardless of how many desktops end up configured, the same as
     * every other default here. */
    config_desktop->notify_activity = true;
    config_desktop->warp_on_edge_drag = true;
    config_desktop->pan_on_edge_drag = true;
    config_desktop->pan_on_edge_hover = true;
    config_desktop->wrap_at_bounds = true;
    config_desktop->margins.top = 0u;
    config_desktop->margins.right = 0u;
    config_desktop->margins.bottom = 0u;
    config_desktop->margins.left = 0u;

    /* Every screen and desktop slot the fixed-size 'screens' and
     * 'desktops' arrays can ever hold gets the sentinel here, not just
     * the ones this function is about to treat as active by default
     * below: 'config_load_base' can fill in far more screens or
     * desktops than that default, straight into these same arrays, and
     * a slot it does not itself set a color for would otherwise still
     * be sitting at zero from this whole structure's initial 'calloc'
     * rather than at the sentinel, which reads as an opaque black
     * background instead of falling back to the theme's color the
     * way an genuinely unset one should. */
    LOGGER_TRACE("Setting background-color sentinel for every" \
            " possible screen and desktop slot", L_NARG);
    for (unsigned int i = 0; i < CONFIG_MAX_SCREENS; ++i) {
        for (unsigned int j = 0; j < CONFIG_MAX_DESKTOPS; ++j) {
            config_base->screens[i].desktops[j].settings.background.color
                = WM_DESKTOP_BG_COLOR_UNSET;
        }
    }

    LOGGER_TRACE("Setting configuration for each screen", L_NARG);
    for (unsigned int i = 0; i < config_base->screen_count; ++i) {
        /* 4 desktops by default, unless 'CONFIG_MAX_DESKTOPS' itself is
         * smaller than that.  Purely a fallback for when nothing else
         * specifies a count at all: a 'config.json' that specifies its
         * own 'desktops.count' always overrides this default, since
         * 'config_load_base' runs after this and simply replaces it;
         * nothing caps that value back down afterward. */
        uint32_t desktop_default = 4u;

        config_base->screens[i].desktop_count =
            (CONFIG_MAX_DESKTOPS < desktop_default)
                ? CONFIG_MAX_DESKTOPS : desktop_default;
        config_base->screens[i].desktop_inaugural = 0;

        /* The exact same reading order the desktop list itself
         * already had before layout existed at all: a single row,
         * one column per desktop, corner and orientation both
         * irrelevant at that point since there is only ever one
         * direction to read in.  A 'config.json' that specifies its
         * own 'topology.screens.desktops[].layout' always overrides
         * this default the same way 'desktop_count' above does (see
         * that field's comment); this is purely the starting
         * baseline before any JSON is read. */
        config_base->screens[i].desktop_layout.orientation =
            CONFIG_DESKTOP_ORIENTATION_HORIZONTAL;
        config_base->screens[i].desktop_layout.corner =
            CONFIG_DESKTOP_CORNER_TOP_LEFT;
        config_base->screens[i].desktop_layout.rows = 1u;
        config_base->screens[i].desktop_layout.columns =
            config_base->screens[i].desktop_count;

        /* A pannable area exactly the size of the physical screen,
         * i.e., panning disabled, unless 'config.json' names its own
         * 'topology.screens.desktops[].viewport' */
        config_base->screens[i].viewport.columns = 1u;
        config_base->screens[i].viewport.rows = 1u;

        /* All desktop settings */
        LOGGER_TRACE("Setting desktops configuration on screen %u", i);
        for (unsigned int j = 0;
                j < config_base->screens[i].desktop_count;
                ++j) {
            char desktop_name[CONFIG_MAX_LENGTH_NAME];
            (void) snprintf(desktop_name, sizeof(desktop_name),
                    "Desktop %u", j);
            safe_strncpy(config_base->screens[i].desktops[j].name,
                desktop_name, CONFIG_MAX_LENGTH_NAME);
        }
    }

    LOGGER_TRACE("Setting default base programs", L_NARG);
    safe_strncpy(config_base->programs.terminal,
            "xterm", sizeof(config_base->programs.terminal));
    safe_strncpy(config_base->programs.launcher,
            "gmrun", sizeof(config_base->programs.launcher));
    safe_strncpy(config_base->programs.file_manager,
            "pcmanfm", sizeof(config_base->programs.file_manager));
    safe_strncpy(config_base->programs.editor,
            "gvim", sizeof(config_base->programs.editor));
    safe_strncpy(config_base->programs.web_browser,
            "firefox", sizeof(config_base->programs.web_browser));

    LOGGER_TRACE("Setting default prompt configuration", L_NARG);
    config_base->prompt.is_enabled = false;

    LOGGER_TRACE("Setting default scratchpad configuration", L_NARG);
    config_base->scratchpad.is_enabled = true;
    safe_strncpy(config_base->scratchpad.command,
            "xterm -fg black -bg ivory -cr black",
            sizeof(config_base->scratchpad.command));
    config_base->scratchpad.edge = CONFIG_SCRATCHPAD_EDGE_TOP;
    config_base->scratchpad.width.mode = CONFIG_SCRATCHPAD_SIZE_MAX;
    config_base->scratchpad.width.pixels = 0;
    config_base->scratchpad.height.mode = CONFIG_SCRATCHPAD_SIZE_FIXED;
    config_base->scratchpad.height.pixels = 200;
    config_base->scratchpad.ignore_margins = false;

    config_base->windows.move_step = 10;
    /* usually overridden by hints */
    config_base->windows.resize_step = 20;

    config_base->overlay.on_desktop_switch = true;
    config_base->overlay.on_viewport_move = true;

    config_base->viewport.move_step = 40;
    /* Enabled by default: the mesh only ever appears where it has
     * something to report, on a surface whose viewport can actually
     * pan and whose root window the manager still owns, so leaving
     * it on costs a plain single-screen desktop nothing */
    config_base->viewport.mesh.is_enabled = true;
    config_base->viewport.mesh.spacing_horizontal =
        CONFIG_VIEWPORT_MESH_SPACING_DEFAULT;
    config_base->viewport.mesh.spacing_vertical =
        CONFIG_VIEWPORT_MESH_SPACING_DEFAULT;
    config_base->viewport.mesh.thickness =
        CONFIG_VIEWPORT_MESH_THICKNESS_DEFAULT;
    config_base->viewport.mesh.tone_shift =
        CONFIG_VIEWPORT_MESH_TONE_SHIFT_DEFAULT;
    config_base->windows.edges.snap.window = 6;
    config_base->windows.edges.snap.screen = 6;
    /* Matches Openbox's default for 'config_resist_edge'
     * (config.c), reused there for the identical purpose */
    config_base->windows.edges.resistance = 20;
    config_base->windows.show_geom = true;
    config_base->windows.solid_drag = true;
    config_base->windows.gravity = CONFIG_GRAVITY_NORTH_WEST;
    config_base->windows.focus_policy = CONFIG_FOCUS_POLICY_CLICK;
    config_base->windows.placement_policy =
        CONFIG_PLACEMENT_POLICY_SMART;
    config_base->windows.monitor_policy =
        CONFIG_PLACEMENT_MONITOR_POINTER;
    config_base->windows.monitor_index = 0;
    config_base->windows.group_related = false;
    config_base->windows.focus.focus_new = true;
    config_base->windows.focus.raise = false;
    config_base->windows.focus.delay_ms = 250;
    config_base->icons.placement_policy = CONFIG_ICON_PLACEMENT_SMART;
    config_base->icons.show_geom = false;
    config_base->icons.follow_viewport = false;
    config_base->shutdown.enable_emergency_shortcut = false;
    config_base->shutdown.timeout_seconds = 15u;
    config_base->fortune.is_enabled = true;
    safe_strncpy(config_base->fortune.command, "fortune",
            sizeof(config_base->fortune.command));
    config_base->startup_notification.is_enabled = true;
    config_base->startup_notification.timeout_seconds =
        (uint32_t) SN_TIMEOUT_SECONDS;
    config_base->menus.root.position = CONFIG_MENU_POSITION_UNDER_MOUSE;
    config_base->menus.windows.position =
        CONFIG_MENU_POSITION_UNDER_MOUSE;
    config_base->systray.is_enabled = true;
    config_base->systray.is_embedding_enabled = true;
    config_base->systray.reserve_space = false;
    config_base->systray.avoid_overlap = true;
    config_base->systray.margins.top = 0u;
    config_base->systray.margins.right = 0u;
    config_base->systray.margins.bottom = 0u;
    config_base->systray.margins.left = 0u;
    config_base->systray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    config_base->systray.monitor.anchor = CONFIG_SYSTRAY_MONITOR_SURFACE;
    config_base->systray.monitor.index = 0u;
    config_base->systray.order = CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT;
    config_base->systray.layer = CONFIG_SYSTRAY_LAYER_BELOW;
    config_base->systray.clock.is_enabled = true;
    safe_strncpy(config_base->systray.clock.format,
            "%a %R", sizeof(config_base->systray.clock.format));

    config_base->systray.battery.is_enabled = false;
    config_base->systray.battery.threshold.charged = 100u;
    config_base->systray.battery.threshold.low = 20u;
    config_base->systray.battery.threshold.critical = 5u;
    config_base->systray.battery.backend.type =
        CONFIG_BATTERY_BACKEND_ACPI;
    config_base->systray.battery.backend.number = 0u;
    config_base->systray.battery.poll_seconds =
        (uint32_t) WM_SYSTRAY_BATTERY_POLL_SECONDS;

    config_base->systray.text.order[0] = CONFIG_SYSTRAY_TEXT_CLOCK;
    config_base->systray.text.order[1] = CONFIG_SYSTRAY_TEXT_BATTERY;
    config_base->systray.text.order_count = 2u;
    config_base->systray.text.position = CONFIG_SYSTRAY_TEXT_LEFT;
}
