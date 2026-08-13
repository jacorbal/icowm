/**
 * @file render/icon.c
 *
 * @brief Icon window rendering for iconified clients
 *
 * Implements @c ri_render_client_icon, which handles the visual
 * representation of iconified clients.  Extracted from
 * @c render/desktop.c to separate icon rendering from the broader
 * desktop rendering pipeline.
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

/* XCB includes */
#include <xcb/xcb.h>

/* Menu includes */
#include <menu/cycle.h>

/* Policy includes */
#include <policy/urgency.h>

/* Default initial values */
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <render/icon.h>
#include <render/text.h>
#include <render/wmicon.h>
#include <systray.h>

/* Local includes */
#include <render/internal.h>


/**
 * @brief Render the icon window for an iconified client
 *
 * Applies icon window attributes (background, border color and width,
 * stacking), optionally draws the client's own @c _NET_WM_ICON image
 * (see @c theme.icon.show-pixmaps), optionally draws a caption label,
 * and optionally draws the pinned/state-hint indicators (see
 * @c ri_draw_icon_hints and @c theme.icon.show-hints).  Called from
 * @c desktop_render_clients for clients that are both hidden and
 * iconified.
 *
 * @param desktop    Desktop whose rendering context and theme are used
 * @param client     The iconified client to render
 * @param is_current @c true when @p desktop is the currently visible one
 *
 * @note No-op when @p client has no icon window or is not icon-mapped
 * @note Complexity: @e O(1)
 */
void ri_render_client_icon(desktop_td *desktop, client_td *client,
        bool is_current)
{
    bool is_cycle_sel;
    bool has_extra_icon_border;
    uint32_t border_width;
    xcb_window_t tray_below;

    if (desktop == NULL || client == NULL) {
        return;
    }

    if (!is_current || !client->is_icon_mapped ||
            client->icon_window == 0) {
        return;
    }

    is_cycle_sel = cycle_is_open() &&
        cycle_get_selected_client() == client;

    /* Nothing about this icon changed since its own last render (no
     * geometry/decoration change on the client itself, and its
     * cycle-selection styling is unchanged), so this skips
     * re-sending every X request below.  This desktop's own outdated
     * flag can be set by an entirely unrelated client (see
     * 'wm_request_client_redraw' marking the whole desktop), so
     * without this check every iconified client on it would
     * otherwise repeat this same work on every such render pass
     * regardless of whether it, itself, changed at all.  This is the
     * same needless-repaint reasoning already applied to normal
     * windows in 's_desktop_render_one_client' (render/desktop.c),
     * just not previously extended to icons.  A genuinely damaged
     * icon (covered and uncovered by another window, say) still
     * repaints correctly on its own via 'handler_expose', independent
     * of this.  An urgent client is the one exception: its own
     * attention blink (see 'policy/urgency.h') alternates this
     * icon's own colors (and 'ri_draw_icon_hints''s own hint letter)
     * between active and inactive, nothing this function's own
     * skip-check tracks, so an urgent client always falls through
     * and repaints in full on every blink phase change regardless of
     * whether either tracked reason actually changed. */
    if (!client->is_outdated &&
            is_cycle_sel == client->icon_last_cycle_sel &&
            !client_is_urgent(client)) {
        return;
    }
    client->icon_last_cycle_sel = is_cycle_sel;

    {
        /* What to actually display this frame: the icon's own real
         * cycle-selection state, except during an urgent client's
         * "on" blink phase, which swaps it to the opposite of
         * whatever it would otherwise be -- the same active/inactive
         * swap 's_desktop_render_one_client' (render/desktop.c)
         * already applies to a titlebar for the same reason.  Kept
         * separate from 'is_cycle_sel' itself (used above for the
         * skip-check and 'icon_last_cycle_sel' tracking) so a
         * transient blink flip is never mistaken for a real change
         * in cycle-selection once the client stops being urgent. */
        bool display_active = is_cycle_sel;

        if (client_is_urgent(client) && urgency_blink_is_on()) {
            display_active = !display_active;
        }

        has_extra_icon_border =
            cycle_client_has_extra_border(client, true);
        xcb_change_window_attributes(desktop->connection,
                client->icon_window,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                (const uint32_t[]) {
            (display_active)
                ? desktop->config_theme->icon.active.color.background
                : desktop->config_theme->icon.inactive.color.background,
            (display_active)
                ? desktop->config_theme->icon.active.border.color
                : desktop->config_theme->icon.inactive.border.color
                });

        border_width = (display_active)
            ? client->theme->icon.active.border.width
            : client->theme->icon.inactive.border.width;
        if (has_extra_icon_border) {
            border_width += WM_ICON_CYCLE_SEL_BORDER_EXTRA;
        }
        xcb_configure_window(desktop->connection,
                client->icon_window,
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                &border_width);

        xcb_clear_area(desktop->connection, 0,
                client->icon_window, 0, 0, 0, 0);
        xcb_map_window(desktop->connection, client->icon_window);
        /* Icons stay lower than the tray even within the shared 'below'
         * layer, "stuck to the desktop"; see 'ccmd_client_iconify' for the
         * fuller explanation of why an unqualified 'below' with no sibling
         * is not enough to guarantee that on its own. */
        tray_below = systray_below_window();
        if (tray_below != XCB_WINDOW_NONE) {
            xcb_configure_window(desktop->connection, client->icon_window,
                    XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
                    (const uint32_t[]) {
                    tray_below, XCB_STACK_MODE_BELOW
                    });
        } else {
            xcb_configure_window(desktop->connection, client->icon_window,
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const uint32_t[]) { XCB_STACK_MODE_BELOW });
        }

        if (desktop->config_theme->icon.show_pixmaps) {
            wmicon_draw(desktop->connection, client->ewmh, client->window,
                    client->icon_window, WM_ICON_SQUARE_SIZE,
                    (display_active)
                        ? desktop->config_theme->icon.active.color.foreground
                        : desktop->config_theme->icon.inactive.color.foreground,
                    (display_active)
                        ? desktop->config_theme->icon.active.color.background
                        : desktop->config_theme->icon.inactive.color.background,
                    &client->icon_pixmap_cache);
        }

        if (desktop->config_theme->icon.is_captioned &&
                client->info.name != NULL) {
            const char *caption =
                (client->icon_info.visible_icon_name != NULL &&
                 client->icon_info.visible_icon_name[0] != '\0')
                    ? client->icon_info.visible_icon_name
                    : client->info.name;

            text_renderer_init(desktop->connection,
                    desktop->config_theme->icon.inactive.font);
            text_renderer_set_color(
            (display_active)
                ? desktop->config_theme->icon.active.color.foreground
                : desktop->config_theme->icon.inactive.color.foreground,
            (display_active)
                ? desktop->config_theme->icon.active.color.background
                : desktop->config_theme->icon.inactive.color.background);

            text_draw_string(desktop->connection,
                    client->icon_window, XCB_NONE,
                    2,
                    (int16_t) (WM_ICON_SQUARE_SIZE +
                        WM_ICON_CAPTION_HEIGHT - 2u),
                    caption);
        }

        ri_draw_icon_hints(desktop->connection, client, display_active,
                desktop->config_theme);
    }

    /* This is not reset anywhere else for a hidden/iconified client.
     * Only 's_desktop_render_one_client' (render/desktop.c) clears
     * this flag, and that function is never reached for one (see
     * 'desktop_render_clients', which routes a hidden client here
     * instead).  Left uncleared, it would stay 'true' forever once
     * set, permanently defeating the skip check above. */
    client->is_outdated = false;
}


/* Render an iconified client's icon window in its "currently
 * selected" state.  Active colors, its own caption, and its hint
 * indicators all stay visible.  Only the pixmap is left out. */
void ri_render_client_icon_selected(xcb_connection_t *connection,
        client_td *client)
{
    if (connection == NULL || client == NULL || client->theme == NULL ||
            !client->is_icon_mapped || client->icon_window == 0) {
        return;
    }

    /* Kept in sync with 'ri_render_client_icon''s own use of this
     * same field.  Left untouched here, a client selected through
     * this function (rather than a full 'ri_render_client_icon'
     * render) would still read as 'icon_last_cycle_sel == false' the
     * moment it is later deselected, matching the freshly computed
     * 'is_cycle_sel == false' there and wrongly tripping that
     * function's own skip-check, silently discarding the full render
     * (pixmap, caption, hint indicators) deselecting is supposed to
     * restore. */
    client->icon_last_cycle_sel = true;

    xcb_change_window_attributes(connection, client->icon_window,
            XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
            (const uint32_t[]) {
                client->theme->icon.active.color.background,
                client->theme->icon.active.border.color
            });
    xcb_clear_area(connection, 0, client->icon_window, 0, 0, 0, 0);

    if (client->theme->icon.is_captioned && client->info.name != NULL) {
        const char *caption =
            (client->icon_info.visible_icon_name != NULL &&
             client->icon_info.visible_icon_name[0] != '\0')
                ? client->icon_info.visible_icon_name
                : client->info.name;

        text_renderer_init(connection, client->theme->icon.active.font);
        text_renderer_set_color(
                client->theme->icon.active.color.foreground,
                client->theme->icon.active.color.background);
        text_draw_string(connection, client->icon_window, XCB_NONE,
                2,
                (int16_t) (WM_ICON_SQUARE_SIZE +
                    WM_ICON_CAPTION_HEIGHT - 2u),
                caption);
    }

    /* Only the pixmap is deliberately omitted here, which is the
     * entire point of this function as opposed to a full
     * 'ri_render_client_icon' render.  The caption above, and these
     * hint indicators, both stay exactly as visible as they would in
     * any ordinary render, just drawn against the plain active-color
     * background this function already cleared to instead of over
     * whatever pixmap would otherwise sit underneath them. */
    ri_draw_icon_hints(connection, client, true, client->theme);
}


/* Draw the state-hint indicators in an iconified client's own top
 * corners */
void ri_draw_icon_hints(xcb_connection_t *connection, client_td *client,
        bool is_cycle_sel, const struct config_theme_s *theme)
{
    char letter[2] = { 0, 0 };
    uint16_t letter_w;
    bool is_urgent;
    bool blink_on;

    if (connection == NULL || client == NULL || theme == NULL ||
            client->icon_window == 0) {
        return;
    }

    is_urgent = client_is_urgent(client);
    blink_on = is_urgent && urgency_blink_is_on();

    /* 'show-hints' off still hides the sticky pin and any state
     * letter as documented, with one exception: an urgent client's
     * attention blink (see 'policy/urgency.h') still gets the urgent
     * letter drawn during its own "on" phase, appearing and
     * disappearing in that corner every 'WM_URGENCY_BLINK_INTERVAL_
     * MS' regardless of this setting, since drawing the user's
     * attention to it is the entire point and should not be
     * silenceable by a setting aimed at the unrelated state-letter
     * feature. */
    if (!theme->icon.show_hints) {
        if (!blink_on) {
            return;
        }
        letter[0] = WM_ICON_HINT_URGENT;
    }

    if (theme->icon.show_hints &&
            (client->properties.flags & CLIENT_FLAG_PIN) != 0u) {
        xcb_gcontext_t gc = xcb_generate_id(connection);
        uint32_t color = theme->window.titlebar.buttons.color.on;

        /* Sized from 'WM_ICON_SQUARE_SIZE' and
         * 'WM_ICON_PIXMAP_SCALE_PERCENT' rather than picked by eye or
         * reusing 'WM_DECOR_BTN_SIZE' (the titlebar buttons' own
         * size, too large here relative to a 48px icon).  When
         * 'theme.icon.show-pixmaps' is on, the client's own pixmap is
         * centered and scaled to 'WM_ICON_PIXMAP_SCALE_PERCENT' of
         * the icon square, leaving an equal margin free on all four
         * sides, 6 pixels at the built-in theme's own defaults, and
         * that margin is exactly the space available in this corner
         * before the square would start covering the pixmap
         * itself. */
        uint16_t pin_size = (uint16_t)
            ((WM_ICON_SQUARE_SIZE *
              (100u - WM_ICON_PIXMAP_SCALE_PERCENT)) / 200u);
        xcb_rectangle_t rect = { 0, 0, pin_size, pin_size };

        xcb_create_gc(connection, gc, client->icon_window,
                XCB_GC_FOREGROUND, &color);
        xcb_poly_fill_rectangle(connection, client->icon_window, gc,
                1, &rect);
        xcb_free_gc(connection, gc);
    }

    if (theme->icon.show_hints) {
        /* An urgent client's "on" blink phase always shows the
         * urgent letter here, alternating with whatever this corner
         * would otherwise show (a state letter, or nothing at all);
         * see this function's own doc comment. */
        if (blink_on) {
            letter[0] = WM_ICON_HINT_URGENT;
        } else {
            switch (client->properties.pre_iconify_state) {
                case CLIENT_STATE_FULLSCREEN:
                    letter[0] = WM_ICON_HINT_FULLSCREEN;
                    break;
                case CLIENT_STATE_MAXIMIZED:
                    letter[0] = WM_ICON_HINT_MAXIMIZED;
                    break;
                case CLIENT_STATE_MAXIMIZED_HORZ:
                    letter[0] = WM_ICON_HINT_MAXIMIZED_HORZ;
                    break;
                case CLIENT_STATE_MAXIMIZED_VERT:
                    letter[0] = WM_ICON_HINT_MAXIMIZED_VERT;
                    break;
                default:
                    return; /* CLIENT_STATE_NORMAL, not blinking:
                               nothing more to draw */
            }
        }
    }

    /* Same font/color scheme as the caption text just below this
     * corner (see the 'is_captioned' block in 'ri_render_client_icon'
     * above), so both pieces of text on the icon read as one
     * consistent style rather than two different-looking labels. */
    text_renderer_init(connection, theme->icon.inactive.font);
    text_renderer_set_color(
            (is_cycle_sel)
                ? theme->icon.active.color.foreground
                : theme->icon.inactive.color.foreground,
            (is_cycle_sel)
                ? theme->icon.active.color.background
                : theme->icon.inactive.color.background);

    letter_w = text_measure_string(letter);
    text_draw_string(connection, client->icon_window, XCB_NONE,
            (int16_t) ((int32_t) WM_ICON_SQUARE_SIZE -
                (int32_t) letter_w - 2),
            (int16_t) (2 + text_font_ascent()),
            letter);
}
