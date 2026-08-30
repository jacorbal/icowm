/**
 * @file handler/expose.c
 *
 * @brief X EXPOSE event handler, i.e., decoration and overlay repaints
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

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>

/* Render includes */
#include <render/desktop.h>
#include <render/icon.h>
#include <render/text.h>
#include <render/wmicon.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/cycle.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/info.h>
#include <menu/notify/desktop.h>
#include <menu/popup.h>
#include <menu/dialog/run.h>
#include <menu/search.h>

/* Systray includes */
#include <systray.h>

/* Input includes */
#include <input/mouse/drag.h>
#include <input/mouse/drag/icon.h>
#include <input/mouse/drag/overlay.h>

/* Default initial values */
#include <defs/icon.h>

/* Project includes */
#include <lookup.h>

/* Local includes */
#include <handler.h>
#include <utils/xcb/connection.h>


/* Handle an 'EXPOSE' event for decoration repaints */
void handler_expose(xcb_connection_t *connection,
        list_td *surfaces, xcb_expose_event_t *event,
        const config_td *cfg)
{
    client_td *client;
    desktop_td *desktop;
    bool is_focused;
    bool is_active_visual;
    bool use_active_style;
    client_td *cycle_client;
    uint16_t left;
    uint16_t right;
    uint16_t title_h;
    uint16_t inner_w;

    if (event == NULL || event->count != 0) {
        return;
    }

    if (connection == NULL || cfg == NULL) {
        return;
    }

    LOGGER_TRACE("Expose event (window=0x%x, region=%ux%u%+d%+d)",
            event->window, event->width, event->height,
            event->x, event->y);

    /* Drag overlay repaint */
    if (drag_is_overlay_window(event->window)) {
        drag_overlay_repaint(connection);
        return;
    }

    /* Systray repaint: redraws whatever text/icons are already
     * cached (see 'systray_layout_reflow''s body), never
     * recomputing the clock or re-polling the battery, so a region
     * revealed after being covered reappears right away instead of
     * staying blank until 'systray_clock_tick''s next per-second
     * update happens to redraw it anyway. */
    if (systray_owns_window(event->window)) {
        systray_layout_reflow();
        return;
    }

    /* Info popup repaint */
    if (popup_is_open() && event->window == popup_window()) {
        popup_repaint(connection, cfg);
        return;
    }

    /* Informational dialog repaint */
    if (dialog_info_is_open() && event->window == dialog_info_window()) {
        dialog_info_repaint(connection, cfg);
        return;
    }

    /* Desktop notify repaint */
    if (notify_desktop_is_open() &&
            event->window == notify_desktop_window()) {
        notify_desktop_repaint(connection, cfg);
        return;
    }

    /* Cycle menu repaint */
    if (cycle_is_open() && event->window == cycle_window()) {
        cycle_force_full_repaint();
        cycle_draw(connection, cfg);
        return;
    }

    /* Fuzzy window-search widget repaint */
    if (search_is_open() && event->window == search_window()) {
        search_draw(connection, cfg);
        return;
    }

    /* Built-in run-box repaint */
    if (run_owns_window(event->window)) {
        run_draw(connection, cfg);
        return;
    }

    /* Generic confirm dialog repaint (quit-confirmation or any other
     * dialog built on 'menu/dialog/confirm.h'; only one instance can
     * ever be open at a time, so which wrapper opened it does not
     * matter here) */
    if (menu_confirm_dialog_is_open() &&
            event->window == menu_confirm_dialog_window()) {
        menu_confirm_dialog_repaint(connection, cfg);
        return;
    }

    /* Window context menu repaint */
    if (wincmenu_owns_window(event->window)) {
        wincmenu_repaint(event->window);
        return;
    }

    /* Root desktop menu repaint */
    if (rootmenu_owns_window(event->window)) {
        rootmenu_repaint(event->window);
        return;
    }

    /* Window list menu repaint */
    if (winlist_owns_window(event->window)) {
        winlist_repaint(event->window);
        return;
    }

    client = lookup_find_client(surfaces, event->window,
            NULL, &desktop);
    if (client == NULL) {
        return;
    }

    /* Icon window: repaint caption */
    if (client->icon_window == event->window) {
        bool is_icon_dragging;

        cycle_client = cycle_get_selected_client();
        is_icon_dragging = drag_is_active() && drag_is_icon_drag() &&
            drag_client() == client;
        /* The icon's drag ('drag_icon_start' in
         * 'input/mouse/drag/icon.c') sets the active styling once,
         * at the start of the drag, and nothing re-applies it
         * afterward; an
         * icon passing behind another window mid-drag gets exposed
         * again once it re-emerges, and without this check that repaint
         * would fall back to the inactive styling for the rest of the
         * drag, well after it visually cleared whatever it had passed
         * behind, since being-dragged is not otherwise part of what
         * decides active vs. inactive here. */
        is_active_visual = (cycle_is_open() && cycle_client == client) ||
            is_icon_dragging;

        if (!(client->properties.flags & CLIENT_FLAG_HIDDEN)) {
            return;
        }
        xcb_change_window_attributes(connection, client->icon_window,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                (const uint32_t[]) {
                    (is_active_visual)
                        ? cfg->theme.icon.active.color.background
                        : cfg->theme.icon.inactive.color.background,
                    (is_active_visual)
                        ? cfg->theme.icon.active.border.color
                        : cfg->theme.icon.inactive.border.color
                });
        xcb_clear_area(connection, 0, client->icon_window, 0, 0, 0, 0);

        /* Deliberately skipped while this same icon is either being
         * dragged ('drag_icon_sync_active_visual' in
         * 'input/mouse/drag/icon.c' clears the icon window without
         * drawing its pixmap when the drag starts, on purpose) or
         * currently selected in the icon cycle menu
         * ('ri_render_client_icon' in 'render/icon.c' draws either
         * case the same way, asking about both itself), both
         * already folded into 'is_active_visual' above.
         *
         * Without this check, an 'Expose' from passing behind another
         * window (or the cycle menu's floating window happening to
         * overlap it) mid-drag or mid-selection would redraw the pixmap
         * this same repaint just cleared, bringing it back despite
         * neither one ever wanting it shown in the first place. */
        if (cfg->theme.icon.show_pixmaps && !is_active_visual) {
            /* Always 'inactive' here, never a ternary against
             * 'is_active_visual': this whole block is already gated on
             * '!is_active_visual' above, so it is always false by the
             * time this runs */
            wmicon_draw(connection, xcb_ewmh_connection_get(), client->window,
                    client->icon_window, WM_ICON_SQUARE_SIZE,
                    cfg->theme.icon.inactive.color.foreground,
                    cfg->theme.icon.inactive.color.background,
                    &client->icon_pixmap_cache);
        }

        if (cfg->theme.icon.is_captioned &&
                client->info.name != NULL) {
            const char *caption =
                (client->icon_info.visible_icon_name != NULL &&
                 client->icon_info.visible_icon_name[0] != '\0')
                    ? client->icon_info.visible_icon_name
                    : client->info.name;

            (void) text_renderer_use_font(connection,
                (is_active_visual)
                    ? cfg->theme.icon.active.font
                    : cfg->theme.icon.inactive.font);
            text_renderer_set_color(
                    (is_active_visual)
                        ? cfg->theme.icon.active.color.foreground
                        : cfg->theme.icon.inactive.color.foreground,
                    (is_active_visual)
                        ? cfg->theme.icon.active.color.background
                        : cfg->theme.icon.inactive.color.background);
            text_draw_string(connection, client->icon_window, XCB_NONE,
                    (struct position_s) { 2,
                        WM_ICON_SQUARE_SIZE + WM_ICON_CAPTION_HEIGHT -
                            2u },
                    caption);
        }

        ri_icon_hints_draw(connection, client, is_active_visual,
                &cfg->theme);

        return;
    }

    is_focused = (desktop != NULL &&
                  desktop->client_active_id == client->id);
    cycle_client = cycle_get_selected_client();
    is_active_visual = cycle_is_open() && cycle_client == client;
    use_active_style = is_focused || is_active_visual;

    /* Frame-only expose: repaint border and background */
    if (client->frame != 0 && client->frame == event->window) {
        /* A fullscreen client's frame can still receive an
         * Expose (e.g., a click landing on it while it happens to
         * still exist as an X window underneath, even though it is
         * never shown decorated), and this path used to repaint the
         * theme's regular border onto it unconditionally regardless.
         * Same condition 's_desktop_render_one_client'
         * ('render/desktop.c') already uses for its
         * 'hide_decoration'. */
        if (!(client_is_fullscreen(client) &&
                    client->was_decorated_fullscreen)) {
            desktop_repaint_frame_decoration(connection, client,
                    use_active_style, &cfg->theme);
        }
        return;
    }

    if (client->titlebar != event->window || client->info.name == NULL) {
        return;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    title_h = (uint16_t) client->title_height;
    inner_w = (client->layout.geometry.cur.dim.w > left + right)
        ? (uint16_t) (client->layout.geometry.cur.dim.w - left - right)
        : 1u;

    desktop_repaint_titlebar_content(connection, client,
            use_active_style, inner_w, title_h, &cfg->theme);

}
