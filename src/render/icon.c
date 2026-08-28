/**
 * @file render/icon.c
 *
 * @brief Icon window rendering for iconified clients
 *
 * Implements @a ri_render_client_icon, which handles the visual
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
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Menu includes */
#include <input/mouse/drag.h>
#include <input/mouse/drag/icon.h>
#include <menu/cycle.h>

/* Policy includes */
#include <policy/urgency.h>

/* Default initial values */
#include <defs/icon.h>

/* Utils includes */
#include <utils/xcb/atom.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <render/icon.h>
#include <render/text.h>
#include <render/wmicon.h>
#include <systray.h>
#include <utils/xcb/connection.h>
#include <utils/xcb/window.h>


/**
 * @brief Render the icon window for an iconified client
 *
 * Applies icon window attributes (background, border color and width,
 * stacking), optionally draws the client's own @c _NET_WM_ICON image,
 * optionally draws a caption label, and optionally draws the
 * pinned/state-hint indicators.  Called from @p desktop_render_clients
 * for clients that are both hidden and iconified.
 *
 * @param client     The iconified client to render; its own theme and
 *                   connection are what this draws with
 * @param is_current @c true when the client's own desktop is the one
 *                   currently visible
 * @param force      Render even when nothing about the icon changed
 *                   since its last one
 *
 * @note No-op when @p client has no icon window or is not icon-mapped
 * @note Complexity: @e O(1)
 *
 * @see @a ri_icon_hints_draw and @p theme.icon.show-hints, also
 *      @p theme.icon.show-pixmaps
 */
void ri_render_client_icon(client_td *client, bool is_current,
        bool force)
{
    bool is_cycle_sel;
    uint32_t border_width;
    xcb_window_t tray_below;
    bool display_active;

    if (client == NULL || xcb_connection_get() == NULL ||
            client->config == NULL) {
        return;
    }

    if (!is_current || !client->is_icon_mapped ||
            client->icon_window == 0) {
        return;
    }

    /* An icon draws in its selected colors while it is the one the
     * cycle menu has picked, and equally while it is the one being
     * dragged: in both the person has hold of it and expects it to
     * look that way.
     *
     * Asked here rather than repainted from the drag itself, which is
     * what once happened and did not hold: this render pass runs
     * after a warp finishes, so anything the warp drew was painted
     * over a moment later by the ordinary path drawing the icon
     * unselected.  Deciding it here means every repaint agrees,
     * whatever triggered it. */
    is_cycle_sel = (cycle_is_open() &&
            cycle_get_selected_client() == client) ||
        (drag_is_icon_drag() && drag_client() == client);

    /* Nothing about this icon changed since its own last render (no
     * geometry/decoration change on the client itself, and its
     * cycle-selection styling is unchanged), so this skips
     * re-sending every X request below.  This desktop's own outdated
     * flag can be set by an entirely unrelated client (cfr.
     * 'wm_request_client_redraw' marking the whole desktop), so
     * without this check every iconified client on it would
     * otherwise repeat this same work on every such render pass
     * regardless of whether it, itself, changed at all.
     *
     * This is the same needless-repaint reasoning already applied to
     * normal windows in 's_desktop_render_one_client'
     * (render/desktop.c), just not previously extended to icons.
     * A genuinely damaged icon (covered and uncovered by another
     * window, say) still repaints correctly on its own via
     * 'handler_expose', independent of this.  An urgent client is the
     * one exception.  Its own attention blink (cfr. 'policy/urgency.h')
     * alternates this icon's own colors (and 'ri_icon_hints_draw''s own
     * hint letter) between active and inactive, nothing this function's
     * own skip-check tracks, so an urgent client always falls through
     * and repaints in full on every blink phase change regardless of
     * whether either tracked reason actually changed. */
    if (!force && !client->is_outdated &&
            is_cycle_sel == client->was_icon_cycle_selected &&
            !client_is_urgent(client)) {
        return;
    }
    client->was_icon_cycle_selected = is_cycle_sel;

    /* What to actually display this frame: the icon's own real
     * cycle-selection state, except during an urgent client's "on"
     * blink phase, which swaps it to the opposite of whatever it would
     * otherwise be; the same active/inactive swap
     * 's_desktop_render_one_client' (in 'render/desktop.c') already
     * applies to a titlebar for the same reason.  Kept separate from
     * 'is_cycle_sel' itself (used above for the skip-check and
     * 'was_icon_cycle_selected' tracking) so a transient blink flip is
     * never mistaken for a real change in cycle-selection once the
     * client stops being urgent. */
    display_active = is_cycle_sel;

    if (client_is_urgent(client) && urgency_blink_is_on()) {
        display_active = !display_active;
    }

    xcb_change_window_attributes(xcb_connection_get(),
            client->icon_window,
            XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
            (const uint32_t[]) {
        (display_active)
            ? client->config->theme.icon.active.color.background
            : client->config->theme.icon.inactive.color.background,
        (display_active)
            ? client->config->theme.icon.active.border.color
            : client->config->theme.icon.inactive.border.color
            });

    border_width = (display_active)
        ? client->config->theme.icon.active.border.width
        : client->config->theme.icon.inactive.border.width;
    xcb_window_set_border(client->icon_window, border_width);
    atom_set_window_opacity(xcb_connection_get(),
            client->icon_window,
            config_theme_opacity_to_raw((display_active)
                ? client->config->theme.icon.active.opacity
                : client->config->theme.icon.inactive.opacity));

    xcb_clear_area(xcb_connection_get(), 0,
            client->icon_window, 0, 0, 0, 0);
    xcb_window_show(client->icon_window);

    /* Icons stay lower than the tray even within the shared 'below'
     * layer, "stuck to the desktop".
     *
     * See 'ccmd_client_iconify' for the fuller explanation of why an
     * unqualified 'below' with no sibling is not enough to guarantee
     * that on its own. */
    tray_below = systray_below_window();
    if (tray_below != XCB_WINDOW_NONE) {
        xcb_window_stack_below(client->icon_window, tray_below);
    } else {
        xcb_window_lower(client->icon_window);
    }

    /* The pixmap is left out while this icon is the picked one: the
     * caption and the hint letters read against the plain selected
     * background rather than over whatever image would otherwise sit
     * under them.
     *
     * Tested against 'is_cycle_sel' and not 'display_active': the
     * latter carries the urgency blink's swap, and a blinking client
     * would otherwise have its pixmap appear and vanish on every
     * phase rather than simply changing color. */
    if (client->config->theme.icon.show_pixmaps && !is_cycle_sel) {
        wmicon_draw(xcb_connection_get(), xcb_ewmh_connection_get(), client->window,
                client->icon_window, WM_ICON_SQUARE_SIZE,
                (display_active)
                    ? client->config->theme.icon.active.color.foreground
                    : client->config->theme.icon.inactive.color.foreground,
                (display_active)
                    ? client->config->theme.icon.active.color.background
                    : client->config->theme.icon.inactive.color.background,
                &client->icon_pixmap_cache);
    }

    if (client->config->theme.icon.is_captioned &&
            client->info.name != NULL) {
        char caption[CONFIG_MAX_LENGTH_NAME];

        /* The picked-up icon takes the active font as well as the
         * active colors: drawn in the inactive one it read as a
         * different icon from the one the person had hold of. */
        (void) text_renderer_use_font(xcb_connection_get(),
                (is_cycle_sel)
                    ? client->config->theme.icon.active.font
                    : client->config->theme.icon.inactive.font);
        text_renderer_set_color(
                (display_active)
                    ? client->config->theme.icon.active.color.foreground
                    : client->config->theme.icon.inactive.color.foreground,
                (display_active)
                    ? client->config->theme.icon.active.color.background
                    : client->config->theme.icon.inactive.color.background);

        text_truncate_to_width(caption, sizeof(caption),
                client->info.name, WM_ICON_SQUARE_SIZE);

        if (xcb_ewmh_connection_get() != NULL) {
            client_sync_visible_name(client,
                    client->icon_info.visible_icon_name,
                    client->info.name, caption,
                    xcb_ewmh_set_wm_visible_icon_name_checked,
                    xcb_ewmh_connection_get()->_NET_WM_VISIBLE_ICON_NAME);
        }

        if (caption[0] != '\0') {
            text_draw_string(xcb_connection_get(),
                    client->icon_window, XCB_NONE,
                    (struct position_s) { 2,
                        WM_ICON_SQUARE_SIZE + WM_ICON_CAPTION_HEIGHT -
                            2u },
                    caption);
        }
    }

    ri_icon_hints_draw(xcb_connection_get(), client, display_active,
            &client->config->theme);

    /* This is not reset anywhere else for a hidden/iconified client.
     * Only 's_desktop_render_one_client' ('render/desktop.c') clears
     * this flag, and that function is never reached for one (see
     * 'desktop_render_clients', which routes a hidden client here
     * instead).  Left uncleared, it would stay 'true' forever once set,
     * permanently defeating the skip check above. */
    client->is_outdated = false;
}


/* Draw the state-hint indicators in an iconified client's own top
 * corners */
void ri_icon_hints_draw(xcb_connection_t *connection, client_td *client,
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

    /* 'show-hints' off still hides the sticky pin and any state letter
     * as documented, with one exception: an urgent client's attention
     * blink (see 'policy/urgency.h') still gets the urgent letter drawn
     * during its own "on" phase, appearing and disappearing in that
     * corner every 'WM_URGENCY_BLINK_INTERVAL_ MS' regardless of this
     * setting, since drawing the user's attention to it is the entire
     * point and should not be silenceable by a setting aimed at the
     * unrelated state-letter feature. */
    if (!theme->icon.show_hints) {
        if (!blink_on) {
            return;
        }
        letter[0] = WM_ICON_HINT_URGENT;
    }

    if (theme->icon.show_hints &&
            (client->properties.flags & CLIENT_FLAG_PIN) != 0u) {
        xcb_gcontext_t gc = xcb_generate_id(connection);
        /* The same foreground the state letter in the opposite corner
         * is drawn in, and for the same reason it uses that one: this
         * square is a state hint like 'f', 'm' or 'v', only shaped
         * rather than lettered, so it has to read as one of them
         * rather than as a stray piece of titlebar borrowed onto the
         * icon.  It used the titlebar buttons' own color before,
         * which is a different palette answering a different question
         * and left the two hints on one icon looking unrelated. */
        uint32_t color = (is_cycle_sel)
            ? theme->icon.active.color.foreground
            : theme->icon.inactive.color.foreground;

        /* Sized from 'WM_ICON_SQUARE_SIZE' and
         * 'WM_ICON_PIXMAP_SCALE_PERCENT' rather than picked by eye or
         * reusing 'WM_DECOR_BTN_SIZE' (the titlebar buttons' own size,
         * too large here relative to a 48px icon).  When
         * 'theme.icon.show-pixmaps' is on, the client's own pixmap is
         * centered and scaled to 'WM_ICON_PIXMAP_SCALE_PERCENT' of the
         * icon square, leaving an equal margin free on all four sides,
         * 6 pixels at the built-in theme's own defaults, and that
         * margin is exactly the space available in this corner before
         * the square would start covering the pixmap itself. */
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
        /* An urgent client's "on" blink phase always shows the urgent
         * letter here, alternating with whatever this corner would
         * otherwise show (a state letter, or nothing at all) */
        if (blink_on) {
            letter[0] = WM_ICON_HINT_URGENT;
        } else if (client_is_fullscreen(client)) {
            letter[0] = WM_ICON_HINT_FULLSCREEN;
        } else if (client_is_maximized(client)) {
            letter[0] = WM_ICON_HINT_MAXIMIZED;
        } else if (client_is_maximized_horz(client)) {
            letter[0] = WM_ICON_HINT_MAXIMIZED_HORZ;
        } else if (client_is_maximized_vert(client)) {
            letter[0] = WM_ICON_HINT_MAXIMIZED_VERT;
        } else {
            /* Holding no state bit but the iconified one, and not
             * blinking: nothing more to draw */
            return;
        }
    }

    /* Same font/color scheme as the caption text just below this corner
     * (see the 'is_captioned' block in 'ri_render_client_icon' above),
     * so both pieces of text on the icon read as one consistent style
     * rather than two different-looking labels. */
    (void) text_renderer_use_font(connection,
            (is_cycle_sel)
                ? theme->icon.active.font
                : theme->icon.inactive.font);
    text_renderer_set_color(
            (is_cycle_sel)
                ? theme->icon.active.color.foreground
                : theme->icon.inactive.color.foreground,
            (is_cycle_sel)
                ? theme->icon.active.color.background
                : theme->icon.inactive.color.background);

    letter_w = text_string_measure(letter);
    text_draw_string(connection, client->icon_window, XCB_NONE,
            (struct position_s) {
                (int32_t) WM_ICON_SQUARE_SIZE - (int32_t) letter_w - 2,
                2 + text_font_ascent() },
            letter);
}
