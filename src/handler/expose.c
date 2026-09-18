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

/* Utils includes */
#include <utils/xcb/connection.h>

/* Input includes */
#include <input/mouse/drag/overlay.h>

/* Default initial values */
#include <defs/icon.h>

/* Render includes */
#include <render/client/decoration.h>
#include <render/client/titlebar.h>
#include <render/icon.h>

/* Menu includes */
#include <menu/context/iconmenu.h>
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

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <stage.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <handler.h>
#include <handler/expose.h>


/* Handle an 'EXPOSE' event for decoration repaints */
void handler_expose(xcb_connection_t *connection,
        list_td *stages, xcb_expose_event_t *event,
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

    /* Systray repaint: redraws whatever text/icons are already cached
     * (see 'systray_layout_reflow''s body), never recomputing the clock
     * or re-polling the battery, so a region revealed after being
     * covered reappears right away instead of staying blank until
     * 'systray_clock_tick''s next per-second update happens to redraw
     * it anyway. */
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

    /* Icon context menu repaint */
    if (iconmenu_owns_window(event->window)) {
        iconmenu_repaint(event->window);
        return;
    }

    client = lookup_find_client(stages, event->window,
            NULL, &desktop);
    if (client == NULL) {
        return;
    }

    /* Icon window: full repaint, via the same shared function every
     * other place an icon needs one already uses (drag start, a cycle
     * or icon-menu selection change, the routine per-desktop render
     * pass; see 'ri_render_client_icon''s own callers).  This one used
     * to carry a separate, inline reimplementation of the same
     * drawing instead, and that duplication is exactly how it drifted
     * out of sync with the shared one, missing the icon-menu-open
     * case until that gap was found and fixed by hand here without
     * ever fixing the copy this ran from.
     *
     * 'restack' is false: unlike every other caller, an 'Expose' here
     * is not itself a change in the icon's own selection state, only
     * possibly a redraw the window itself is asking for (uncovered
     * after passing behind another window, say), so restacking it
     * below the tray on every one of those would shuffle it against
     * unrelated sibling icons for no reason a user asked for. */
    if (client->icon_window == event->window) {
        ri_render_client_icon(client, true, true, false);
        return;
    }

    is_focused = (desktop != NULL &&
                  desktop->client_active_id == client->id);
    cycle_client = cycle_get_selected_client();
    is_active_visual = cycle_is_open() && cycle_client == client;
    use_active_style = is_focused || is_active_visual;

    /* Frame-only expose: repaint border and background */
    if (client->frame != 0 && client->frame == event->window) {
        /* A fullscreen client's frame can still receive an Expose
         * (e.g., a click landing on it while it happens to still exist
         * as an X window underneath, even though it is never shown
         * decorated), and repainting the theme's regular border onto it
         * unconditionally would show through.  Same condition
         * 's_desktop_render_one_client' ('render/desktop.c') already
         * uses for its 'hide_decoration'. */
        if (!(client_is_fullscreen(client) &&
                    client->was_decorated_fullscreen)) {
            render_client_decoration_repaint_frame(connection, client,
                    use_active_style, &cfg->theme);
        }
        return;
    }

    if (client->titlebar != event->window) {
        return;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    title_h = (uint16_t) client->title_height;
    inner_w = (client->layout.geometry.cur.dim.w > left + right)
        ? (uint16_t) (client->layout.geometry.cur.dim.w - left - right)
        : 1u;

    render_client_titlebar_repaint_content(connection, client,
            use_active_style, inner_w, title_h, &cfg->theme);
}
