/**
 * @file systray.c
 *
 * @brief Built-in systray dock: shared state, public event
 *        dispatchers, and configuration-driven lifecycle
 *
 * The @p s_tray state every file @ *.c in @c src/systray/ shares is
 * defined here (see @c systray/internal.h), along with the public entry
 * points every other subsystem calls into (init/shutdown/reload, and
 * every event handler).   The actual work of reading clock/battery
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
#include <stdlib.h>     /* free */
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
 * @brief Copy every systray setting from @p wm->config into @p s_tray,
 *        and refresh the clock/battery text for the values just copied
 *
 * The part @a systray_init and @a systray_reload both need identically
 * (every field @c config.json's own @p systray object can set, plus
 * re-rendering the clock and battery text so a changed format or
 * threshold takes effect immediately rather than waiting for the next
 * scheduled tick).  What differs between the two callers is only what
 * happens before and after this, not this copy itself.
 *
 * @param wm Window manager state, for its current @p config
 *
 * @note Complexity: @e O(1)
 */
static void s_systray_apply_config(wm_td *wm)
{
    s_tray.position = wm->config->base.systray.position;
    s_tray.reserve_space = wm->config->base.systray.reserve_space;
    s_tray.strut_margins.top = wm->config->base.systray.margins.top;
    s_tray.strut_margins.right = wm->config->base.systray.margins.right;
    s_tray.strut_margins.bottom =
        wm->config->base.systray.margins.bottom;
    s_tray.strut_margins.left = wm->config->base.systray.margins.left;
    s_tray.monitor.anchor = wm->config->base.systray.monitor.anchor;
    s_tray.monitor.index = wm->config->base.systray.monitor.index;

    /* A configured '0' (or anything absurdly small) would otherwise
     * make every docked icon invisible, or divide the icon row's own
     * layout math by a near-zero step; clamped to '1' at the very
     * least, the same defensive floor 'height' below already applied
     * against the old fixed 'WM_SYSTRAY_ICON_SIZE' constant. */
    s_tray.pixmap_size = (uint16_t)
        ((wm->config->theme.systray.pixmap.size > 0u)
            ? wm->config->theme.systray.pixmap.size : 1u);
    s_tray.pixmap_pad = (uint16_t) wm->config->theme.systray.pixmap.padding;

    s_tray.height = (uint16_t) ((wm->config->theme.systray.height >
            s_tray.pixmap_size)
        ? wm->config->theme.systray.height : s_tray.pixmap_size);
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

    s_tray.is_active = true;

    /* Never acquired at all when 'is_embedding_enabled' is false, own
     * clock/battery text still shown regardless via the explicit
     * 'systray_layout_reflow' call below, since
     * 'systray_protocol_acquire_selection' does not trigger one on its
     * own: see 'is_embedding_enabled''s comment in 'config.h' for why
     * restricted-memory mode is the one profile that always leaves it
     * 'false' */
    if (wm->config->base.systray.is_embedding_enabled) {
        (void) systray_protocol_acquire_selection();
    }

    systray_layout_reflow();
}


/* Release the tray selection and destroy the dock window */
void systray_shutdown(wm_td *wm)
{
    (void) wm;

    systray_protocol_release_selection();

    if (s_tray.window_ready && s_tray.connection != NULL &&
            s_tray.window != XCB_WINDOW_NONE) {
        /* Destroying the tray window implicitly reparents any
         * still-docked icons back to the root window; each icon's own
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
    if (!s_tray.window_ready || !s_tray.is_active ||
            s_tray.layer != CONFIG_SYSTRAY_LAYER_BELOW) {
        return XCB_WINDOW_NONE;
    }
    return s_tray.window;
}


/* Return the space the tray currently reserves for itself on
 * 'surface', or 'NULL' when 'surface' is not the one it is docked
 * on */
const struct strut_partial_s *systray_get_reserved_strut(
        const surface_td *surface)
{
    if (surface == NULL || !s_tray.window_ready ||
            s_tray.surface != surface) {
        return NULL;
    }

    /* 'reserved_strut' is kept at all-zero sides by
     * 'systray_layout_reflow' itself whenever the tray is unmapped
     * (disabled, empty, or another tray manager owns the selection), so
     * no separate check for that is needed here: a caller adding an
     * all-zero strut to a workarea calculation is a no-op either way */
    return &s_tray.reserved_strut;
}


/* Return the tray's own current on-screen rectangle on 'surface', or
 * 'false' when it is not currently showing there at all */
bool systray_get_geometry(const surface_td *surface,
        int32_t *restrict out_x, int32_t *restrict out_y,
        uint16_t *restrict out_w, uint16_t *restrict out_h)
{
    xcb_get_geometry_cookie_t cookie;
    xcb_get_geometry_reply_t *reply;

    if (surface == NULL || out_x == NULL || out_y == NULL ||
            out_w == NULL || out_h == NULL ||
            !s_tray.window_ready || !s_tray.is_active ||
            s_tray.surface != surface) {
        return false;
    }

    cookie = xcb_get_geometry(s_tray.connection, s_tray.window);
    reply = xcb_get_geometry_reply(s_tray.connection, cookie, NULL);
    if (reply == NULL) {
        return false;
    }

    /* 'reply->x'/'reply->y' are relative to the tray window's own
     * parent, the same root every other top-level window this project
     * creates (icon windows included) shares, so directly comparable
     * against an icon's own root-relative position with no extra
     * translation needed */
    *out_x = (int32_t) reply->x;
    *out_y = (int32_t) reply->y;
    *out_w = reply->width;
    *out_h = reply->height;

    free(reply);
    return true;
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
                        s_tray.pixmap_size, s_tray.pixmap_size
                    });
            xcb_flush(s_tray.connection);
            return true;
        }
    }

    return false;
}


/**
 * @brief Force every already-docked icon back to the tray's current
 *        @p pixmap.size
 *
 * A docked icon's own size is otherwise only ever set once, at dock
 * time (see @a systray_protocol_dock in @c systray/protocol.c).
 * Reparent, resize, only then map, the icon is never actually visible
 * at its old size in the first place, so it never needs to redraw
 * itself to fit a new one either.  Nothing about reloading the
 * configuration on its own revisits an icon that was already docked
 * (and already mapped, already painted once) under a previous, possibly
 * different @p pixmap.size.
 *
 * @a systray_layout_reflow, called separately, does reposition every
 * icon using the newly reloaded size and padding for its own spacing
 * math, but repositioning is not resizing.
 *
 * Unmapping first, then resizing, then remapping mirrors that same
 * dock-time sequence as closely as possible, rather than resizing the
 * icon in place while still mapped and already painted: many minimal
 * XEmbed tray-icon implementations paint themselves once at whatever
 * size they were first mapped at and never repaint in response to a
 * later live @c ConfigureNotify the way a full GTK/Qt widget would;
 * an in-place resize left the icon showing as a blank square in at
 * least one real client, not a correctly rescaled one, since nothing
 * in that client ever repainted it.  Briefly unmapping first, so the
 * icon is invisible precisely while it does not yet have its new
 * size, and only remapping once it does, gives it the same "resized
 * before ever visible at the new size" situation dock time already
 * relies on, though, without XEmbed guaranteeing this, an individual
 * client could still fail to repaint correctly here too.
 *
 * @note Complexity: @e O(n), where @e n is the number of docked icons
 */
static void s_systray_resize_docked_icons(void)
{
    for (uint16_t i = 0u; i < s_tray.icon_count; ++i) {
        xcb_window_t icon = s_tray.icons[i].window;

        xcb_unmap_window(s_tray.connection, icon);
        xcb_configure_window(s_tray.connection, icon,
                XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                (const uint32_t[]) {
                    s_tray.pixmap_size, s_tray.pixmap_size
                });
        xcb_map_window(s_tray.connection, icon);
    }
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

    /* Scans the whole array rather than stopping at the first match:
     * defense in depth against ever ending up with more than one
     * entry for the same window (the dock path itself now refuses a
     * duplicate outright, see 'systray_protocol_dock', but nothing
     * about this cleanup path should have to assume that always
     * holds to stay safe) */
    for (uint16_t i = 0u; i < s_tray.icon_count; /* incremented below */) {
        if (s_tray.icons[i].window != window) {
            ++i;
            continue;
        }

        for (uint16_t j = i; j + 1u < s_tray.icon_count; ++j) {
            s_tray.icons[j] = s_tray.icons[j + 1u];
        }
        s_tray.icon_count--;

        LOGGER_DEBUG("Systray icon 0x%x undocked (%u remaining)",
                window, (unsigned int) s_tray.icon_count);

        /* Do not advance 'i': the entry that just shifted into this
         * same index has not been checked against 'window' yet */
    }

    systray_layout_reflow();
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
    bool was_active;

    if (wm == NULL || wm->config == NULL) {
        return;
    }

    should_be_enabled = wm->config->base.systray.is_enabled;
    was_active = s_tray.is_active;
    s_systray_apply_config(wm);
    systray_protocol_apply_theme_style();

    /* Unconditional, before the enabled/disabled branches below: an
     * icon already docked before this reload keeps whatever size it was
     * forced to at dock time otherwise (see this function's own doc
     * comment), regardless of whether the tray ends up enabled,
     * disabled, or unchanged by this same reload, so a size picked up
     * while momentarily disabled is still correct the next time the
     * tray is shown again, without needing every application to re-dock
     * itself. */
    s_systray_resize_docked_icons();

    if (was_active && !should_be_enabled) {
        LOGGER_INFO("Systray disabled by configuration reload;" \
                " releasing the selection (icons stay docked" \
                " hidden in the background)", L_NARG);
        s_tray.is_active = false;

        /* 'systray_protocol_release_selection' already triggers
         * 'systray_layout_reflow' itself once it releases the
         * selection, but only when 'selection_owned' was actually
         * 'true' to begin with.  With embedding disabled, it was never
         * acquired at all, so this still needs to unmap the window
         * itself directly instead. */
        if (s_tray.selection_owned) {
            systray_protocol_release_selection();
        } else {
            systray_layout_reflow();
        }
        return;
    }

    if (!was_active && should_be_enabled) {
        bool ready = systray_protocol_ensure_window(wm);

        s_tray.is_active = ready;

        /* Selection acquisition only even attempted, let alone required
         * for success here, when embedding is actually allowed; with it
         * disabled the window alone (already showing its own
         * clock/battery text via 's_systray_apply_config' above) is
         * enough on its own */
        if (ready && wm->config->base.systray.is_embedding_enabled) {
            (void) systray_protocol_acquire_selection();
        }

        LOGGER_INFO("Systray enabled by configuration reload", L_NARG);
        if (ready) {
            systray_protocol_resort();
            systray_layout_reflow();
        }
        return;
    }

    if (was_active) {
        /* Still active: pick up a possible 'position' change, and
         * re-sort already-docked icons for the alphabetical 'order'
         * policies (see 'systray_protocol_resort') without disturbing
         * anything for the two plain insertion-order policies. */
        systray_protocol_resort();
        systray_layout_reflow();
    }
}
