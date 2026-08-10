/**
 * @file systray.c
 *
 * @brief Built-in systray dock: shared state, public event
 *        dispatchers, and configuration-driven lifecycle
 *
 * The @c s_tray state every file @ *.c in @c src/systray/ shares is
 * defined here (see @c systray/internal.h), along with the public entry
 * points every other subsystem calls into (init/shutdown/reload, and
 * every event handler): the actual work of reading clock/battery
 * text, positioning and drawing the tray, and speaking the systray
 * protocol lives in @c systray/text.c, @c systray/layout.c, and
 * @c systray/protocol.c respectively.
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
#include <stdint.h>
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils include */
#include <utils/safe/safestr.h>

/* Project includes */
#include <logger.h>
#include <wm.h>

/* Local includes */
#include <systray/internal.h>


/* Module-level built-in systray state; see 'systray/internal.h' for
 * the full field-by-field documentation */
struct systray_state_s s_tray;


/**
 * @brief Copy every systray setting from @p wm->config into @c s_tray,
 *        and refresh the clock/battery text for the values just copied
 *
 * The part @c systray_init and @c systray_reload both need identically
 * (every field @c config.json's own @c systray object can set, plus
 * re-rendering the clock and battery text so a changed format or
 * threshold takes effect immediately rather than waiting for the next
 * scheduled tick); what differs between the two callers is only what
 * happens before and after this, not this copy itself.
 *
 * @param wm Window manager state, for its current @c config
 *
 * @note Complexity: @e O(1)
 */
static void s_systray_apply_config(wm_td *wm)
{
    s_tray.position = wm->config->base.systray.position;
    s_tray.monitor.anchor = wm->config->base.systray.monitor.anchor;
    s_tray.monitor.index = wm->config->base.systray.monitor.index;
    s_tray.height = (uint16_t) ((wm->config->theme.systray.height >
            SYSTRAY_ICON_SIZE)
        ? wm->config->theme.systray.height : SYSTRAY_ICON_SIZE);
    s_tray.order = wm->config->base.systray.order;
    s_tray.layer = wm->config->base.systray.layer;
    s_tray.clock_enabled = wm->config->base.systray.clock.is_enabled;
    safe_strncpy(s_tray.clock_format,
            wm->config->base.systray.clock.format,
            sizeof(s_tray.clock_format));
    s_tray.battery_enabled = wm->config->base.systray.battery.is_enabled;
    s_tray.battery_threshold_charged =
        wm->config->base.systray.battery.threshold.charged;
    s_tray.battery_threshold_low =
        wm->config->base.systray.battery.threshold.low;
    s_tray.battery_threshold_critical =
        wm->config->base.systray.battery.threshold.critical;
    s_tray.battery_backend_type =
        wm->config->base.systray.battery.backend.type;
    s_tray.battery_backend_number =
        wm->config->base.systray.battery.backend.number;
    s_tray.battery_poll_seconds =
        wm->config->base.systray.battery.poll_seconds;
    s_tray.text_position = wm->config->base.systray.text.position;
    s_tray.text_valign = wm->config->theme.systray.text.valign;
    s_tray.text_gap = (uint16_t) wm->config->theme.systray.text.gap;
    s_tray.text_order[0] = wm->config->base.systray.text.order[0];
    s_tray.text_order[1] = wm->config->base.systray.text.order[1];
    s_tray.text_order_count = wm->config->base.systray.text.order_count;
    s_tray.theme = &wm->config->theme;
    systray_text_refresh_clock();
    systray_text_refresh_battery();
}


/* Acquire the tray selection and create the dock window */
void systray_init(wm_td *wm)
{
    if (wm == NULL || wm->config == NULL ||
            !wm->config->base.systray.is_enabled) {
        return;
    }

    s_systray_apply_config(wm);

    if (!systray_protocol_ensure_window(wm)) {
        return;
    }

    (void) systray_protocol_acquire_selection();
}


/* Release the tray selection and destroy the dock window */
void systray_shutdown(wm_td *wm)
{
    (void) wm;

    systray_protocol_release_selection();

    if (s_tray.window_ready && s_tray.connection != NULL &&
            s_tray.window != XCB_WINDOW_NONE) {
        /* Destroying the tray window implicitly reparents any still-
         * docked icons back to the root window; each icon's own
         * application is responsible for re-docking if a tray reappears
         * later, exactly as with every other systray.  This full
         * teardown is only for the window manager itself exiting;
         * toggling 'is-enabled' off goes through 'systray_reload',
         * which keeps the window and icons alive via
         * 'systray_protocol_release_selection' instead. */
        xcb_destroy_window(s_tray.connection, s_tray.window);
        xcb_flush(s_tray.connection);
    }

    memset(&s_tray, 0, sizeof(s_tray));
}


/* Query whether 'window' is the tray dock window itself */
bool systray_owns_window(xcb_window_t window)
{
    return s_tray.window_ready && window != XCB_WINDOW_NONE &&
        window == s_tray.window;
}


/* Return the tray window when it is visible and in the 'below' layer,
 * or 'XCB_WINDOW_NONE' otherwise */
xcb_window_t systray_below_window(void)
{
    if (!s_tray.window_ready || !s_tray.selection_owned ||
            s_tray.layer != CONFIG_SYSTRAY_LAYER_BELOW) {
        return XCB_WINDOW_NONE;
    }
    return s_tray.window;
}


/* Query whether 'window' is a currently docked icon, and if so, force
 * it back to the tray's fixed icon size */
bool systray_enforce_icon_size(xcb_window_t window)
{
    if (window == XCB_WINDOW_NONE) {
        return false;
    }

    for (uint16_t i = 0u; i < s_tray.icon_count; ++i) {
        if (s_tray.icons[i].window == window) {
            xcb_configure_window(s_tray.connection, window,
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                    (const uint32_t[]) {
                        SYSTRAY_ICON_SIZE, SYSTRAY_ICON_SIZE
                    });
            xcb_flush(s_tray.connection);
            return true;
        }
    }

    return false;
}


/* Handle a 'ClientMessage' addressed to the tray window */
void systray_handle_client_message(wm_td *wm,
        const xcb_client_message_event_t *event)
{
    (void) wm;

    if (event == NULL || !s_tray.selection_owned) {
        return;
    }

    if (event->window != s_tray.window ||
            event->type != s_tray.opcode_atom) {
        return;
    }

    /* 'data32[1]' is the opcode; only 'SYSTEM_TRAY_REQUEST_DOCK' is
     * implemented; 'BEGIN_MESSAGE'/'CANCEL_MESSAGE' (balloon-style
     * messages) are silently acknowledged as ignored */
    if (event->data.data32[1] == SYSTRAY_OPCODE_REQUEST_DOCK) {
        systray_protocol_dock((xcb_window_t) event->data.data32[2]);
    }
}


/* Handle a docked icon window being destroyed */
void systray_handle_destroy(wm_td *wm, xcb_window_t window)
{
    (void) wm;

    if (!s_tray.window_ready) {
        return;
    }

    for (uint16_t i = 0u; i < s_tray.icon_count; ++i) {
        if (s_tray.icons[i].window != window) {
            continue;
        }

        for (uint16_t j = i; j + 1u < s_tray.icon_count; ++j) {
            s_tray.icons[j] = s_tray.icons[j + 1u];
        }
        s_tray.icon_count--;

        LOGGER_DEBUG("Systray icon 0x%x undocked (%u remaining)",
                window, (unsigned int) s_tray.icon_count);

        systray_layout_reflow();
        return;
    }
}


/* Reposition the tray dock window for its surface's current size */
void systray_handle_surface_resize(wm_td *wm)
{
    (void) wm;

    systray_layout_reflow();
}


/* Re-apply the configured stacking layer, e.g., after a fullscreen
 * change elsewhere */
void systray_restack(void)
{
    systray_layout_restack();
}


/* React to a configuration reload */
void systray_reload(wm_td *wm)
{
    bool should_be_enabled;

    if (wm == NULL || wm->config == NULL) {
        return;
    }

    should_be_enabled = wm->config->base.systray.is_enabled;
    s_systray_apply_config(wm);
    systray_protocol_apply_theme_style();

    if (s_tray.selection_owned && !should_be_enabled) {
        LOGGER_INFO("Systray disabled by configuration reload;" \
                " releasing the selection (icons stay docked" \
                " hidden in the background)", L_NARG);
        systray_protocol_release_selection();
        return;
    }

    if (!s_tray.selection_owned && should_be_enabled) {
        LOGGER_INFO("Systray enabled by configuration reload", L_NARG);
        if (systray_protocol_ensure_window(wm) &&
                systray_protocol_acquire_selection()) {
            systray_protocol_resort();
            systray_layout_reflow();
        }
        return;
    }

    if (s_tray.selection_owned) {
        /* Still enabled: pick up a possible 'position' change, and
         * re-sort already-docked icons for the alphabetical 'order'
         * policies (see 'systray_protocol_resort') without disturbing
         * anything for the two plain insertion-order policies. */
        systray_protocol_resort();
        systray_layout_reflow();
    }
}
