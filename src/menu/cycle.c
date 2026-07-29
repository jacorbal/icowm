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
    .prev_modmask = 0
};


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
    uint32_t values[3];
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

    (void) surfaces;    /* passed to cycle_confirm for focus_apply */

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

    /* Collect matching clients — iterate from tail (top of stack, most
     * recently raised) to head (bottom), so the list order matches the
     * MRU ordering used by openbox and evilwm. */
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

    if (s_menu.count == 0) {
        return;
    }

    /* Preselect: start from active client, step by preselect */
    if (active_idx >= 0) {
        s_menu.selected = (active_idx + preselect + s_menu.count) %
            s_menu.count;
    } else {
        s_menu.selected = s_menu.count - 1;
    }

    /* Compute dimensions */
    for (int i = 0; i < s_menu.count; ++i) {
        uint16_t w = menu_draw_measure(s_menu.labels[i]);
        if (w > max_w) {
            max_w = w;
        }
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
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.window.inactive.background_color;
    values[1] = cfg->theme.window.active.border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE    |
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
