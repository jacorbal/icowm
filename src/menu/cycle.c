/**
 * @file menu/cycle.c
 *
 * @brief Window/icon cycle menu implementation
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Render includes */
#include <render/surface.h>

/* Render includes */
#include <render/text.h>

/* Input includes */
#include <input/keyboard.h>

/* Focus includes */
#include <policy/focus.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <surface.h>

/* Local includes */
#include <menu/draw.h>
#include <menu/cycle.h>


/** Private cycle menu state */
static struct {
    xcb_window_t window;
    client_td *clients[WM_CYCLE_MENU_MAX_ENTRIES];
    char labels[WM_CYCLE_MENU_MAX_ENTRIES][WM_CYCLE_MENU_ENTRY_LEN];
    int count;
    int selected;
    uint16_t width;
    bool is_icon_menu;
    surface_td *surface;
    desktop_td *desktop;
    uint16_t modifier;
    xcb_window_t prev_focus;
    xcb_keysym_t next_keysym;
    uint16_t next_modmask;
    xcb_keysym_t prev_keysym;
    uint16_t prev_modmask;
    client_td *preview_client;
    const config_td *config;
} s_menu = {
    .window = XCB_WINDOW_NONE,
    .count = 0,
    .selected = 0,
    .width = 0,
    .is_icon_menu = false,
    .surface = NULL,
    .desktop = NULL,
    .modifier = 0,
    .prev_focus = XCB_WINDOW_NONE,
    .next_keysym = XCB_NO_SYMBOL,
    .next_modmask = 0,
    .prev_keysym = XCB_NO_SYMBOL,
    .prev_modmask = 0,
    .preview_client = NULL,
    .config = NULL
};


/**
 * @brief Resolve the X window used as the visual target for cycle
 *        preview
 *
 * Determines which X window should be used to represent a client during
 * cycle preview operations.  The function accounts for icon menu mode,
 * hidden clients, and window decorations to select the appropriate
 * drawable target.
 *
 * @param client       Pointer to the client to evaluate
 * @param is_icon_menu Whether the cycle preview is operating in icon
 *                     menu mode
 *
 * @return The X window ID to use as preview target, or
 *         @c XCB_WINDOW_NONE if no valid target is available
 *
 * @note Returns @c XCB_WINDOW_NONE if @p client is null, hidden, or
 *       lacks a valid drawable target
 * @note Prefers @c client->icon_window in icon menu mode when available
 * @note Uses the frame window when the client is decorated
 * @note Complexity: @e O(1)
 */
static xcb_window_t s_cycle_preview_target(const client_td *client,
        bool is_icon_menu)
{
    if (client == NULL) {
        return XCB_WINDOW_NONE;
    }

    if (is_icon_menu) {
        return (client->icon_window != 0)
            ? client->icon_window
            : XCB_WINDOW_NONE;
    }

    if (client->properties.flags & CLIENT_FLAG_HIDDEN) {
        return XCB_WINDOW_NONE;
    }

    if (client_is_decorated(client) && client->frame != 0) {
        return client->frame;
    }

    return client->window;
}


/* Return the border width for a cycle-preview target */
static uint32_t s_cycle_preview_border_width(const client_td *client,
        const config_td *cfg, bool is_icon_menu, bool is_highlighted)
{
    uint32_t border_width;

    if (cfg == NULL) {
        return 0u;
    }

    if (is_icon_menu) {
        border_width = cfg->theme.icon.general.border_width;
    } else if (client != NULL &&
            client_is_decorated(client) &&
            client->frame != 0) {
        border_width = 0u;
    } else {
        border_width = cfg->theme.window.general.border_width;
    }

    if (is_highlighted) {
        border_width += WM_ICON_CYCLE_SEL_BORDER_EXTRA;
    }

    return border_width;
}


/* Apply preview border color and width to the target window */
static void s_cycle_preview_style_target(xcb_connection_t *connection,
        xcb_window_t target, const client_td *client,
        const config_td *cfg, bool is_icon_menu,
        uint32_t border_color, bool is_highlighted)
{
    uint32_t border_width;
    uint32_t frame_values[2];

    if (connection == NULL || target == XCB_WINDOW_NONE ||
            cfg == NULL) {
        return;
    }

    border_width = s_cycle_preview_border_width(client, cfg,
            is_icon_menu, is_highlighted);
    xcb_configure_window(connection, target,
            XCB_CONFIG_WINDOW_BORDER_WIDTH, &border_width);

    if (!is_icon_menu &&
            client != NULL &&
            client_is_decorated(client) &&
            client->frame == target) {
        frame_values[0] = border_color;
        frame_values[1] = border_color;
        xcb_change_window_attributes(connection, target,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                frame_values);
        xcb_clear_area(connection, 0, target, 0, 0, 0, 0);
    } else {
        xcb_change_window_attributes(connection, target,
                XCB_CW_BORDER_PIXEL, &border_color);
    }
}


/**
 * @brief Apply cycle preview highlighting and stacking for the selected
 *        client
 *
 * Updates the visual state of the currently selected client in the
 * cycle preview by adjusting its border color and ensuring it is
 * stacked above its peers.  Also restores the previous preview client's
 * border color according to its active or inactive state.
 *
 * @param connection Pointer to the XCB connection
 * @param cfg        Pointer to the configuration containing theme data
 *
 * @note No-op if required state (connection, config, menu, or
 *       selection) is invalid or incomplete
 * @note Restores the previous preview client's border color before
 *       applying the new selection highlight
 * @note Ensures the selected target window is raised above others
 * @note Updates @c s_menu.preview_client to track the current preview
 * @note Complexity: @e O(1)
 */
static void s_cycle_preview_apply(xcb_connection_t *connection,
        const config_td *cfg)
{
    client_td *selected;
    client_td *previous;
    xcb_window_t selected_target;
    xcb_window_t previous_target;
    uint32_t values[2];
    uint32_t selected_border;
    uint32_t previous_border;
    bool prev_is_active;

    if (connection == NULL || cfg == NULL ||
            s_menu.window == XCB_WINDOW_NONE ||
            s_menu.surface == NULL || s_menu.desktop == NULL ||
            s_menu.selected < 0 || s_menu.selected >= s_menu.count) {
        return;
    }

    selected = s_menu.clients[s_menu.selected];
    selected_target = s_cycle_preview_target(selected, s_menu.is_icon_menu);
    if (selected == NULL || selected_target == XCB_WINDOW_NONE) {
        return;
    }

    previous = s_menu.preview_client;
    if (previous != NULL && previous != selected) {
        previous_target = s_cycle_preview_target(previous,
                s_menu.is_icon_menu);

        if (previous_target != XCB_WINDOW_NONE) {
            prev_is_active =
                (s_menu.desktop->client_active_id == previous->id);

            if (s_menu.is_icon_menu) {
                previous_border = cfg->theme.icon.inactive.border_color;
            } else if (prev_is_active) {
                previous_border = cfg->theme.window.active.border_color;
            } else {
                previous_border = cfg->theme.window.inactive.border_color;
            }

            s_cycle_preview_style_target(connection, previous_target,
                    previous, cfg, s_menu.is_icon_menu,
                    previous_border, false);

            if (s_menu.is_icon_menu) {
                xcb_configure_window(connection, previous_target,
                        XCB_CONFIG_WINDOW_STACK_MODE,
                        (const uint32_t[]) { XCB_STACK_MODE_BELOW });

                values[0] = cfg->theme.icon.inactive.background_color;
                values[1] = cfg->theme.icon.inactive.border_color;

                xcb_change_window_attributes(connection, previous_target,
                        XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL, values);
                xcb_clear_area(connection, 0, previous_target, 0, 0, 0, 0);
                if (cfg->theme.icon.general.is_captioned &&
                        previous->info.name != NULL) {
                    text_renderer_init(connection,
                            cfg->theme.icon.inactive.font);
                    text_renderer_set_color(
                            cfg->theme.icon.inactive.foreground_color,
                            cfg->theme.icon.inactive.background_color);
                    text_draw_string(connection, previous_target, XCB_NONE,
                            2,
                            (int16_t) (WM_ICON_SQUARE_SIZE +
                                WM_ICON_CAPTION_HEIGHT - 2u),
                            previous->info.name);
                }
            } /* ! if (s_menu.is_icon_menu) */
        } /* ! if (previous_target) */
    }

    selected_border = (s_menu.is_icon_menu)
        ? cfg->theme.icon.active.border_color
        : cfg->theme.window.active.border_color;
    s_cycle_preview_style_target(connection, selected_target,
            selected, cfg, s_menu.is_icon_menu,
            selected_border, true);

    if (s_menu.is_icon_menu) {
        values[0] = cfg->theme.icon.active.background_color;
        values[1] = cfg->theme.icon.active.border_color;

        xcb_change_window_attributes(connection, selected_target,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL, values);
        xcb_clear_area(connection, 0, selected_target, 0, 0, 0, 0);
        if (cfg->theme.icon.general.is_captioned &&
                selected->info.name != NULL) {
            text_renderer_init(connection,
                    cfg->theme.icon.active.font);
            text_renderer_set_color(
                    cfg->theme.icon.active.foreground_color,
                    cfg->theme.icon.active.background_color);
            text_draw_string(connection, selected_target, XCB_NONE,
                    2,
                    (int16_t) (WM_ICON_SQUARE_SIZE +
                        WM_ICON_CAPTION_HEIGHT - 2u),
                    selected->info.name);
        }
    }

    values[0] = s_menu.window;
    values[1] = XCB_STACK_MODE_BELOW;
    xcb_configure_window(connection, selected_target,
            XCB_CONFIG_WINDOW_SIBLING |
            XCB_CONFIG_WINDOW_STACK_MODE,
            values);

    s_menu.preview_client = selected;
    xcb_flush(connection);
}


/* Restore preview border style for all cycle entries */
static void s_cycle_preview_restore(xcb_connection_t *connection)
{
    client_td *client;
    xcb_window_t target;
    uint32_t border_color;
    bool is_active;

    if (connection == NULL || s_menu.config == NULL ||
            s_menu.desktop == NULL || s_menu.count <= 0) {
        return;
    }

    for (int i = 0; i < s_menu.count; ++i) {
        client = s_menu.clients[i];
        if (client == NULL) {
            continue;
        }

        target = s_cycle_preview_target(client, s_menu.is_icon_menu);
        if (target == XCB_WINDOW_NONE) {
            continue;
        }

        if (s_menu.is_icon_menu) {
            border_color = s_menu.config->theme.icon.inactive.border_color;
        } else {
            is_active = (s_menu.desktop->client_active_id == client->id);
            border_color = is_active
                ? s_menu.config->theme.window.active.border_color
                : s_menu.config->theme.window.inactive.border_color;
        }
        s_cycle_preview_style_target(connection, target,
                client, s_menu.config, s_menu.is_icon_menu,
                border_color, false);
    }
}


/* Open the cycle menu for window or icon cycling */
void cycle_open(list_td *surfaces,
        xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        bool is_icon, int preselect, uint16_t modifier,
        const config_td *cfg)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    xcb_get_input_focus_cookie_t foc_cookie;
    xcb_get_input_focus_reply_t *foc_reply;
    uint32_t mask;
    uint32_t values[4];
    uint16_t menu_w;
    uint16_t menu_h;
    int16_t menu_x;
    int16_t menu_y;
    int active_idx = -1;
    uint16_t max_w = 200u;
    xcb_keysym_t nks;
    xcb_keysym_t pks;
    uint16_t nmm;
    uint16_t pmm;
    enum wm_keybind_type_e nt;
    enum wm_keybind_type_e pt;
    const uint16_t lock_mask = (uint16_t) (
            (unsigned int) XCB_MOD_MASK_LOCK |
            (unsigned int) XCB_MOD_MASK_2);

    if (connection == NULL || surface == NULL || desktop == NULL ||
            desktop->stacking == NULL || cfg == NULL) {
        return;
    }

    foc_cookie = xcb_get_input_focus(connection);
    foc_reply = xcb_get_input_focus_reply(connection, foc_cookie, NULL);

    cycle_close(connection);

    s_menu.count = 0;
    s_menu.surface = surface;
    s_menu.desktop = desktop;
    s_menu.is_icon_menu = is_icon;
    s_menu.modifier =
        (uint16_t) ((unsigned int) modifier & ~(unsigned int) lock_mask);
    s_menu.prev_focus = (foc_reply != NULL &&
            foc_reply->focus != XCB_WINDOW_NONE &&
            foc_reply->focus != XCB_INPUT_FOCUS_POINTER_ROOT &&
            foc_reply->focus != XCB_INPUT_FOCUS_NONE)
        ? foc_reply->focus : XCB_WINDOW_NONE;

    if (foc_reply != NULL) {
        free(foc_reply);
    }

    nt = (is_icon) ? KEYBIND_DESKTOP_ICON_NEXT : KEYBIND_CLIENT_CYCLE_NEXT;
    pt = (is_icon) ? KEYBIND_DESKTOP_ICON_PREV : KEYBIND_CLIENT_CYCLE_PREV;
    nks = XCB_NO_SYMBOL; nmm = 0;
    pks = XCB_NO_SYMBOL; pmm = 0;
    (void) keyboard_find(nt, &nks, &nmm);
    (void) keyboard_find(pt, &pks, &pmm);
    s_menu.next_keysym = nks;
    s_menu.next_modmask =
        (uint16_t) ((unsigned int) nmm & ~(unsigned int) lock_mask);
    s_menu.prev_keysym = pks;
    s_menu.prev_modmask =
        (uint16_t) ((unsigned int) pmm & ~(unsigned int) lock_mask);
    s_menu.preview_client = NULL;
    s_menu.config = cfg;

    /* Collect matching clients, i.e., iterate from tail (top of stack,
     * most recently raised) to head (bottom), so the list order matches
     * the MRU ordering used by openbox and evilwm. */
    node = cdlist_tail(desktop->stacking);
    initial = node;
    if (node != NULL) {
        do {
            client_td *c = (client_td *) cdlist_data(node);
            if (c != NULL && client_is_focusable(c)) {
                bool want = is_icon
                    ? (bool) client_is_iconified(c)
                    : !client_is_iconified(c);
                if (want && s_menu.count < WM_CYCLE_MENU_MAX_ENTRIES) {
                    int idx = s_menu.count;
                    const char *name = (c->info.name != NULL &&
                            c->info.name[0] != '\0')
                        ? c->info.name : "(unnamed)";

                    s_menu.clients[idx] = c;

                    /* Mark hidden windows with brackets so they stand
                     * out visually in the cycle menu. */
                    if (c->properties.flags & CLIENT_FLAG_HIDDEN) {
                        snprintf(s_menu.labels[idx],
                                WM_CYCLE_MENU_ENTRY_LEN, "(%s)", name);
                    } else {
                        snprintf(s_menu.labels[idx],
                                WM_CYCLE_MENU_ENTRY_LEN, "%s", name);
                    }

                    if (c->id == desktop->client_active_id) {
                        active_idx = idx;
                    }
                    s_menu.count++;
                }
            }
            node = cdlist_prev(node);
        } while (node != NULL && node != initial);
    }

    /* Also include sticky (pinned) clients that live on other desktops:
     * they remain visible on every desktop but are only in their home
     * desktop's stacking list.  A linear walk over all desktops on all
     * surfaces is needed; duplicates are avoided by checking whether
     * the client pointer is already in 's_menu.clients[]'. */
    if (!is_icon && surfaces != NULL) {
        list_item_td *snode;
        for (snode = list_head(surfaces);
                snode != NULL; snode = list_next(snode)) {
            surface_td *sv = (surface_td *) list_data(snode);
            cdlist_item_td *dnode;
            cdlist_item_td *dinitial;
            if (sv == NULL || sv->desktops == NULL) {
                continue;
            }

            dnode = cdlist_head(sv->desktops);
            dinitial = dnode;
            if (dnode == NULL) {
                continue;
            }

            do {
                desktop_td *od = (desktop_td *) cdlist_data(dnode);
                cdlist_item_td *cn;
                cdlist_item_td *ci;
                if (od == NULL || od == desktop ||
                        od->stacking == NULL) {
                    dnode = cdlist_next(dnode);
                    continue;
                }

                cn = cdlist_head(od->stacking);
                ci = cn;
                if (cn != NULL) {
                    do {
                        client_td *c = (client_td *) cdlist_data(cn);
                        if (c != NULL &&
                                client_is_focusable(c) &&
                                client_is_sticky(c) &&
                                !client_is_iconified(c) &&
                                s_menu.count < WM_CYCLE_MENU_MAX_ENTRIES) {
                            /* Skip if already in list */
                            int found = 0;
                            for (int k = 0; k < s_menu.count; ++k) {
                                if (s_menu.clients[k] == c) {
                                    found = 1;
                                    break;
                                }
                            }
                            if (!found) {
                                int idx = s_menu.count;
                                const char *nm = (c->info.name != NULL
                                        && c->info.name[0] != '\0')
                                    ? c->info.name : "(unnamed)";
                                s_menu.clients[idx] = c;
                                snprintf(s_menu.labels[idx],
                                        WM_CYCLE_MENU_ENTRY_LEN,
                                        "%s", nm);
                                s_menu.count++;
                            }
                        }
                        cn = cdlist_next(cn);
                    } while (cn != NULL && cn != ci);
                }

                dnode = cdlist_next(dnode);
            } while (dnode != NULL && dnode != dinitial);
        }
    }

    if (s_menu.count == 0) {
        return;
    }

    /* Preselect: start from active client, step by preselect */
    if (active_idx >= 0) {
        s_menu.selected = (active_idx + preselect + s_menu.count) %
            s_menu.count;
    } else {
        s_menu.selected = (preselect >= 0) ? 0 : s_menu.count - 1;
    }

    /* Compute dimensions */
    for (int i = 0; i < s_menu.count; ++i) {
        uint16_t w = menu_draw_measure(s_menu.labels[i]);
        if (w > max_w) { max_w = w; }
    }

    menu_w = (uint16_t) (max_w + (uint16_t) (WM_CYCLE_MENU_PAD_X * 2));
    menu_h = (uint16_t) (WM_CYCLE_MENU_PAD_Y * 2 +
            s_menu.count * WM_CYCLE_MENU_ROW_HEIGHT);

    s_menu.width = menu_w;

    menu_x = (int16_t) (((int32_t) surface->properties.dim.w -
                (int32_t) menu_w) / 2);
    menu_y = (int16_t) (((int32_t) surface->properties.dim.h -
                (int32_t) menu_h) / 2);
    if (menu_x < 0) { menu_x = 0; }
    if (menu_y < 0) { menu_y = 0; }

    s_menu.window = xcb_generate_id(connection);

    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
        XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.window.inactive.background_color;
    values[1] = cfg->theme.window.active.border_color;
    values[2] = 1;  /* override_redirect = true */
    values[3] = XCB_EVENT_MASK_EXPOSURE    |
        XCB_EVENT_MASK_KEY_PRESS    |
        XCB_EVENT_MASK_KEY_RELEASE  |
        XCB_EVENT_MASK_BUTTON_PRESS;

    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            s_menu.window,
            surface->screen->root,
            menu_x, menu_y,
            menu_w, menu_h,
            1,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    xcb_map_window(connection, s_menu.window);
    xcb_set_input_focus(connection,
            XCB_INPUT_FOCUS_POINTER_ROOT,
            s_menu.window,
            XCB_CURRENT_TIME);

    s_cycle_preview_apply(connection, cfg);
    xcb_flush(connection);
}


/* Close the cycle menu and restore previous focus */
void cycle_close(xcb_connection_t *connection)
{
    xcb_window_t restore_focus;
    surface_td *surface;

    if (connection == NULL || s_menu.window == XCB_WINDOW_NONE) {
        return;
    }

    restore_focus = s_menu.prev_focus;
    surface = s_menu.surface;
    s_cycle_preview_restore(connection);

    xcb_destroy_window(connection, s_menu.window);
    s_menu.window = XCB_WINDOW_NONE;
    s_menu.count = 0;
    s_menu.selected = 0;
    s_menu.width = 0;
    s_menu.surface = NULL;
    s_menu.desktop = NULL;
    s_menu.modifier = 0;
    s_menu.prev_focus = XCB_WINDOW_NONE;
    s_menu.next_keysym = XCB_NO_SYMBOL;
    s_menu.next_modmask = 0;
    s_menu.prev_keysym = XCB_NO_SYMBOL;
    s_menu.prev_modmask = 0;
    s_menu.preview_client = NULL;
    s_menu.config = NULL;

    if (restore_focus != XCB_WINDOW_NONE) {
        xcb_set_input_focus(connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                restore_focus,
                XCB_CURRENT_TIME);
    }

    if (surface != NULL) {
        surface_render_current_desktop_repaint(surface);        
    } else {
        xcb_flush(connection);
    }
}


/* Repaint all menu entries */
void cycle_draw(xcb_connection_t *connection, const config_td *cfg)
{
    uint32_t fg_sel;
    uint32_t bg_sel;
    uint32_t fg_nor;
    uint32_t bg_nor;

    if (connection == NULL || cfg == NULL ||
            s_menu.window == XCB_WINDOW_NONE) {
        return;
    }

    fg_sel = cfg->theme.window.active.foreground_color;
    bg_sel = cfg->theme.window.active.background_color;
    fg_nor = cfg->theme.window.inactive.foreground_color;
    bg_nor = cfg->theme.window.inactive.background_color;

    text_renderer_init(connection, cfg->theme.window.active.font);

    for (int i = 0; i < s_menu.count; ++i) {
        int16_t row_y = (int16_t) (WM_CYCLE_MENU_PAD_Y +
                i * WM_CYCLE_MENU_ROW_HEIGHT);

        if (i == s_menu.selected) {
            menu_draw_row_bg(connection, s_menu.window, bg_sel,
                    row_y, (uint16_t) WM_CYCLE_MENU_ROW_HEIGHT,
                    s_menu.width);
            text_renderer_set_color(fg_sel, bg_sel);
        } else {
            menu_draw_row_bg(connection, s_menu.window, bg_nor,
                    row_y, (uint16_t) WM_CYCLE_MENU_ROW_HEIGHT,
                    s_menu.width);
            text_renderer_set_color(fg_nor, bg_nor);
        }

        menu_draw_label(connection, s_menu.window,
                (int16_t) WM_CYCLE_MENU_PAD_X,
                (int16_t) (row_y + WM_CYCLE_MENU_ROW_HEIGHT - 4),
                s_menu.labels[i]);
    }

    s_cycle_preview_apply(connection, cfg);
    xcb_flush(connection);
}


/* Confirm the currently selected cycle menu entry */
void cycle_confirm(xcb_connection_t *connection, list_td *surfaces,
        const config_td *cfg)
{
    client_td *target;
    surface_td *surface;
    desktop_td *desktop;
    bool is_icon;

    if (s_menu.window == XCB_WINDOW_NONE ||
            s_menu.selected < 0 ||
            s_menu.selected >= s_menu.count) {
        return;
    }

    target = s_menu.clients[s_menu.selected];
    surface = s_menu.surface;
    desktop = s_menu.desktop;
    is_icon = s_menu.is_icon_menu;

    cycle_close(connection);

    if (target == NULL || surface == NULL || desktop == NULL) {
        return;
    }

    if (is_icon) {
        (void) client_send_event_restore(target);
    } else if (target->properties.flags & CLIENT_FLAG_HIDDEN) {
        /* Hidden (non-iconified) window: unhide before focusing */
        (void) client_send_event(target, ACTION_CLIENT_UNHIDE,
                CLIENT_PRIORITY_DEFAULT);
    }

    if (!is_icon && client_is_shaded(target)) {
        (void) client_send_event(target, ACTION_CLIENT_UNSHADE,
                CLIENT_PRIORITY_DEFAULT);
    }

    focus_apply(surfaces, surface, desktop, target, true, cfg);
}


/* Set the selection directly to a given index */
void cycle_navigate_to(unsigned int idx)
{
    if (s_menu.count <= 0) {
        return;
    }

    if ((int) idx >= s_menu.count) {
        idx = (unsigned int)(s_menu.count - 1);
    }

    s_menu.selected = (int) idx;
}


/* Advance the selection by one entry */
void cycle_navigate_next(void)
{
    if (s_menu.count <= 0) {
        return;
    }

    s_menu.selected = (s_menu.selected + 1) % s_menu.count;
}


/* Retreat the selection by one entry */
void cycle_navigate_prev(void)
{
    if (s_menu.count <= 0) {
        return;
    }

    s_menu.selected = (s_menu.selected - 1 + s_menu.count) % s_menu.count;
}


/* Query whether the cycle menu is currently open */
bool cycle_is_open(void)
{
    return s_menu.window != XCB_WINDOW_NONE;
}


/* Return the cycle menu window identifier */
xcb_window_t cycle_window(void)
{
    return s_menu.window;
}


/* Return the currently highlighted client in the cycle menu */
client_td *cycle_get_selected_client(void)
{
    if (s_menu.count <= 0 ||
            s_menu.selected < 0 ||
            s_menu.selected >= s_menu.count) {
        return NULL;
    }

    return s_menu.clients[s_menu.selected];
}


/* Return the modifier mask that opened the cycle menu */
uint16_t cycle_modifier(void)
{
    return s_menu.modifier;
}


/* Return the keysym configured for cycle-next navigation */
xcb_keysym_t cycle_next_keysym(void)
{
    return s_menu.next_keysym;
}


/* Return the modifier mask for the cycle-next binding */
uint16_t cycle_next_modmask(void)
{
    return s_menu.next_modmask;
}


/* Return the keysym configured for cycle-prev navigation */
xcb_keysym_t cycle_prev_keysym(void)
{
    return s_menu.prev_keysym;
}


/* Return the modifier mask for the cycle-prev binding */
uint16_t cycle_prev_modmask(void)
{
    return s_menu.prev_modmask;
}


/* Return whether a client must keep cycle extra border */
bool cycle_client_has_extra_border(const client_td *client,
        bool is_icon_menu)
{
    if (client == NULL) {
        return false;
    }

    if (cycle_is_open()) {
        return cycle_get_selected_client() == client &&
            s_menu.is_icon_menu == is_icon_menu;
    }

    return cycle_is_open() &&
        cycle_get_selected_client() == client &&
        s_menu.is_icon_menu == is_icon_menu;
}
