/**
 * @file config/memguard/defaults.c
 *
 * @brief Restricted-memory mode's fixed default configuration
 *        profile implementation
 *
 * Kept apart from @c config/memguard.c so that file stays focused on
 * orchestrating restricted-memory mode's config loading, not on
 * the profile's long, mechanical list of fixed field values.
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/desktop.h>
#include <defs/input.h>
#include <defs/sn.h>
#include <defs/urgency.h>

/* Local includes */
#include <config.h>
#include <config/memguard.h>


/* Populate a configuration structure with restricted-memory mode's
 * own fixed profile */
void config_set_default_values_memguard(config_td *config)
{
    if (config == NULL) {
        return;
    }

    config->base.theme[0] = '\0';
    config->base.screen_count = 1u;

    /* A single screen, a single desktop: neither edge-warping nor
     * wrap-around navigation, nor the desktop-name overlay or the
     * cross-desktop activity notification, mean anything with only one
     * desktop to switch to. */
    config->base.overlay.on_desktop_switch = false;
    config->base.overlay.on_viewport_move = false;
    config->desktops.notify_activity = false;
    config->desktops.warp_on_edge_drag = false;
    config->base.viewport.pan_on_edge_drag = false;
    config->base.viewport.pan_on_edge_hover = false;
    config->desktops.wrap_at_bounds = false;
    config->desktops.margins.top = 0u;
    config->desktops.margins.right = 0u;
    config->desktops.margins.bottom = 0u;
    config->desktops.margins.left = 0u;

    config->base.screens[0].desktop_count = 1u;
    config->base.screens[0].desktop_inaugural = 0u;
    config->base.screens[0].desktop_layout.orientation =
        CONFIG_DESKTOP_ORIENTATION_HORIZONTAL;
    config->base.screens[0].desktop_layout.corner =
        CONFIG_DESKTOP_CORNER_TOP_LEFT;
    config->base.screens[0].desktop_layout.rows = 1u;
    config->base.screens[0].desktop_layout.columns = 1u;
    config->base.screens[0].viewport.columns = 1u;
    config->base.screens[0].viewport.rows = 1u;
    safe_strncpy(config->base.screens[0].desktops[0].name, "Desktop 0",
            CONFIG_MAX_LENGTH_NAME);
    config->base.screens[0].desktops[0].settings.background.color =
        WM_DESKTOP_BG_COLOR_UNSET;

    /* Launched-program defaults, in case memguard.json does not specify
     * its; identical to 'config_set_default_values''s defaults, since
     * restricted-memory mode has no particular reason to prefer
     * different programs. */
    safe_strncpy(config->base.programs.terminal, "xterm",
            sizeof(config->base.programs.terminal));
    safe_strncpy(config->base.programs.launcher, "gmrun",
            sizeof(config->base.programs.launcher));
    safe_strncpy(config->base.programs.file_manager, "pcmanfm",
            sizeof(config->base.programs.file_manager));
    safe_strncpy(config->base.programs.editor, "gvim",
            sizeof(config->base.programs.editor));
    safe_strncpy(config->base.programs.web_browser, "firefox",
            sizeof(config->base.programs.web_browser));

    /* Unlike every program name just above, this one genuinely does
     * differ from the normal-mode default (see base.c's): the
     * built-in run-box avoids spawning 'launcher' itself as a whole
     * extra process, even a minimal one such as this very mode's
     * default for it above, fitting this mode's whole reason for
     * existing the same way every other choice in this file already
     * does. */
    config->base.prompt.is_enabled = true;

    config->base.windows.move_step = 10u;
    config->base.windows.resize_step = 20u;
    config->base.windows.edges.snap.window = 6u;
    config->base.windows.edges.snap.screen = 6u;
    config->base.windows.edges.resistance = 20u;
    config->base.windows.show_geom = true;
    config->base.windows.solid_drag = false;
    config->base.windows.gravity = CONFIG_GRAVITY_NORTH_WEST;
    config->base.windows.focus_policy = CONFIG_FOCUS_POLICY_CLICK;
    config->base.windows.placement_policy =
        CONFIG_PLACEMENT_POLICY_SMART;
    config->base.windows.monitor_policy =
        CONFIG_PLACEMENT_MONITOR_POINTER;
    config->base.windows.monitor_index = 0u;
    config->base.windows.group_related = false;
    config->base.windows.focus.focus_new = true;
    config->base.windows.focus.raise = false;
    config->base.windows.focus.delay_ms = 250u;

    /* SMART's cost is bounded (256 candidate slots, each checked
     * against every already-docked icon, so O(256*n) at worst) and runs
     * once per icon placed, not on any hot path, so it costs nothing
     * meaningful to leave on by default here; overridable in
     * memguard.json (see 'ci_memguard_load_json') the same as an
     * ordinary session's 'icons.placement'. */
    config->base.icons.placement_policy = CONFIG_ICON_PLACEMENT_SMART;
    config->base.icons.show_geom = false;

    /* Moot here: this screen's viewport is permanently 1x1 (see just
     * above), so there is never a pan to follow, but the field still
     * exists and defaults the same way as an ordinary session's for
     * consistency and to avoid leaving it uninitialized. */
    config->base.viewport.pan_icons = false;

    /* Moot for the same reason, and switched off rather than left
     * holding an ordinary session's inherited values: with a
     * permanently 1x1 viewport there is no pan for 'move_step' to
     * size and nothing for the mesh to make visible, so 'memguard.json'
     * has no 'viewport' section at all and neither field is ever read
     * back from it. */
    config->base.viewport.pan_step = 0u;
    config->base.viewport.mesh.is_enabled = false;

    config->base.shutdown.enable_emergency_shortcut = true;
    config->base.shutdown.timeout_seconds = 15u;
    config->base.fortune.is_enabled = false;
    safe_strncpy(config->base.fortune.command, "fortune",
            sizeof(config->base.fortune.command));

    /* Accessibility (a11y): the exact same built-in defaults as an
     * ordinary session's (see 'config_set_default_a11y_values' in
     * 'config.c'), including the same 'is_enabled=false' opt-in
     * posture; restricted-memory mode never has a reason to change
     * these, saving memory is never a reason to also give up basic
     * accessibility accommodations */
    config_set_default_a11y_values(&config->a11y);

    config->base.startup_notification.is_enabled = false;
    config->base.startup_notification.timeout_seconds =
        (uint32_t) SN_TIMEOUT_SECONDS;

    config->base.menus.root.position =
        CONFIG_MENU_POSITION_UNDER_MOUSE;
    config->base.menus.windows.position =
        CONFIG_MENU_POSITION_UNDER_MOUSE;

    /* Systray defaults, in case memguard.json does not specify its.
     * A lower 'battery.poll_seconds' than an ordinary session's default
     * is the one deliberate difference here, both to check less often
     * and since a stale battery reading for a few extra seconds matters
     * little either way. */
    config->base.systray.is_enabled = true;
    /* Fixed false for this mode, deliberately not something
     * 'memguard.json' is allowed to configure; see
     * 'is_embedding_enabled''s comment in 'config.h' */
    config->base.systray.is_embedding_enabled = false;
    config->base.systray.reserve_space = false;
    config->base.systray.avoid_overlap = true;
    config->base.systray.margins.top = 0u;
    config->base.systray.margins.right = 0u;
    config->base.systray.margins.bottom = 0u;
    config->base.systray.margins.left = 0u;
    config->base.systray.position = CONFIG_SYSTRAY_POSITION_TOP_LEFT;
    config->base.systray.monitor.anchor =
        CONFIG_SYSTRAY_MONITOR_SURFACE;
    config->base.systray.monitor.index = 0u;
    config->base.systray.order = CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT;
    config->base.systray.layer = CONFIG_SYSTRAY_LAYER_BELOW;
    config->base.systray.clock.is_enabled = true;
    safe_strncpy(config->base.systray.clock.format, "%a %R",
            sizeof(config->base.systray.clock.format));

    config->base.systray.battery.is_enabled = false;
    config->base.systray.battery.threshold.charged = 100u;
    config->base.systray.battery.threshold.low = 20u;
    config->base.systray.battery.threshold.critical = 5u;
    config->base.systray.battery.backend.type =
        CONFIG_BATTERY_BACKEND_ACPI;
    config->base.systray.battery.backend.number = 0u;
    config->base.systray.battery.poll_seconds = 30u;

    config->base.systray.text.order[0] = CONFIG_SYSTRAY_TEXT_CLOCK;
    config->base.systray.text.order[1] = CONFIG_SYSTRAY_TEXT_BATTERY;
    config->base.systray.text.order_count = 2u;
    config->base.systray.text.position = CONFIG_SYSTRAY_TEXT_LEFT;

    /* RandR output-profile management: never consulted at all in
     * this mode, so this stays at its off/empty state regardless. */
    config->randr.is_enabled = false;
    config->randr.output_count = 0u;

    /* Same reasoning as 'config_load''s equivalent call.  Without this,
     * a session with no theme named in 'memguard.json' at all would
     * leave 'config->theme' entirely zeroed (every color black, every
     * font an empty string) rather than falling back to a sensible
     * compiled-in theme, and a reload that switched away from a theme
     * specifying some field to one that does not would leave that field
     * stuck at the old theme's value instead of this default. */
    config_set_default_theme_values(&config->theme);
}
