/**
 * @file menu/cycle.c
 *
 * @brief Window and icon cycle menu implementation
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
#include <render/icon.h>
#include <render/outline.h>
#include <render/surface.h>

/* Render includes */
#include <render/text.h>

/* Input includes */
#include <input/kbd/bind.h>

/* Focus includes */
#include <policy/focus.h>

/* Default initial values */
#include <defs/ctxmenu.h>
#include <defs/cycle.h>

/* Utils includes */
#include <utils/xcb/atom.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <surface.h>

/* Local includes */
#include <menu/draw.h>
#include <menu/cycle.h>
#include <menu/internal.h>


/** Private cycle menu state */
struct cycle_menu_state_s g_cycle_menu = {
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
    .config = NULL,
    .scroll_offset = 0,
    .viewport_rows = 0,
    .last_drawn_selected = 0,
    .last_drawn_scroll_offset = 0,
    .has_drawn_once = false,
    .outline_windows = {
        XCB_WINDOW_NONE, XCB_WINDOW_NONE,
        XCB_WINDOW_NONE, XCB_WINDOW_NONE
    }
};


/**
 * @brief Restore the preview style for all clients in the cycle menu
 *
 * Iterates over the clients listed in the global cycle menu and resets
 * their preview windows or icons to the normal border style, using the
 * active or inactive window/icon theme as appropriate.
 *
 * @param connection XCB connection used to update preview window
 *                   attributes
 *
 * @note Complexity: @e O(n), where @e n is the number of clients in the
 *       cycle menu
 */
static void s_cycle_preview_restore(xcb_connection_t *connection)
{
    xcb_window_t target;
    uint32_t border_color;
    bool is_active;

    if (connection == NULL || g_cycle_menu.config == NULL ||
            g_cycle_menu.desktop == NULL || g_cycle_menu.count <= 0) {
        return;
    }

    for (int i = 0; i < g_cycle_menu.count; ++i) {
        const client_td *client = g_cycle_menu.clients[i];
        if (client == NULL) {
            continue;
        }

        target =
            mi_cycle_preview_target(client, g_cycle_menu.is_icon_menu);
        if (target == XCB_WINDOW_NONE) {
            continue;
        }

        if (g_cycle_menu.is_icon_menu) {
            border_color =
                g_cycle_menu.config->theme.icon.inactive.border.color;
        } else {
            is_active =
                (g_cycle_menu.desktop->client_active_id == client->id);
            border_color = (is_active)
                ? g_cycle_menu.config->theme.window.active.border.color
                : g_cycle_menu.config->theme.window.inactive.border.color;
        }

        mi_cycle_preview_style_target(connection, target,
                client, g_cycle_menu.config, g_cycle_menu.is_icon_menu,
                border_color);
    }
}


/**
 * @brief Adjust scroll offset so the selected row stays in the viewport
 *
 * Ensures the currently selected row remains visible within the menu's
 * viewport by scrolling up when the selection is above the visible
 * range, or scrolling down when it falls below it.  If the viewport can
 * display every row, scrolling is disabled and the offset is reset to
 * zero.
 *
 * @note Operates on the global @c g_cycle_menu state
 * @note Complexity: @e O(1)
 */
static void s_cycle_scroll_to_selection(void)
{
    if (g_cycle_menu.viewport_rows >= g_cycle_menu.count) {
        g_cycle_menu.scroll_offset = 0;
        return;
    }

    if (g_cycle_menu.selected < g_cycle_menu.scroll_offset) {
        g_cycle_menu.scroll_offset = g_cycle_menu.selected;
    } else if (g_cycle_menu.selected >=
            g_cycle_menu.scroll_offset + g_cycle_menu.viewport_rows) {
        g_cycle_menu.scroll_offset =
            g_cycle_menu.selected - g_cycle_menu.viewport_rows + 1;
    }
}


/* Initialize the cycle menu for window or icon cycling */
void cycle_init(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        bool is_icon, int preselect, uint16_t modifier,
        const config_td *cfg)
{
    cdlist_item_td *node;
    const cdlist_item_td *initial;
    xcb_get_input_focus_cookie_t foc_cookie;
    xcb_get_input_focus_reply_t *foc_reply;
    uint32_t mask;
    uint32_t values[4];
    uint16_t menu_w;
    uint16_t menu_h;
    int16_t menu_x;
    int16_t menu_y;
    int active_idx = -1;
    int vp_rows;
    uint16_t max_w = 200u;
    xcb_keysym_t nks;
    xcb_keysym_t pks;
    uint16_t nmm;
    uint16_t pmm;
    enum wm_keybind_type_e nt;
    enum wm_keybind_type_e pt;
    uint32_t screen_h_pct;
    uint32_t pad2;
    uint32_t avail;
    const uint16_t lock_mask = (uint16_t) (
            (unsigned int) XCB_MOD_MASK_LOCK |
            (unsigned int) XCB_MOD_MASK_2);

    if (connection == NULL || surface == NULL || desktop == NULL ||
            desktop->stacking == NULL || cfg == NULL) {
        return;
    }

    foc_cookie = xcb_get_input_focus(connection);
    foc_reply = xcb_get_input_focus_reply(connection, foc_cookie, NULL);

    cycle_destroy(connection);

    /* 'modifier' (the raw modmask of whichever specific cycle-next or
     * cycle-prev binding was actually pressed to get here) is
     * deliberately NOT used to derive 'g_cycle_menu.modifier' below;
     * see the comment there for why. */
    (void) modifier;

    g_cycle_menu.count = 0;
    g_cycle_menu.surface = surface;
    g_cycle_menu.desktop = desktop;
    g_cycle_menu.is_icon_menu = is_icon;
    g_cycle_menu.prev_focus = (foc_reply != NULL &&
            foc_reply->focus != XCB_WINDOW_NONE &&
            foc_reply->focus != XCB_INPUT_FOCUS_POINTER_ROOT &&
            foc_reply->focus != XCB_INPUT_FOCUS_NONE)
        ? foc_reply->focus : XCB_WINDOW_NONE;

    if (foc_reply != NULL) {
        free(foc_reply);
    }

    nt = (is_icon)
        ? KEYBIND_DESKTOP_ICON_NEXT
        : KEYBIND_CLIENT_CYCLE_NEXT;
    pt = (is_icon)
        ? KEYBIND_DESKTOP_ICON_PREV
        : KEYBIND_CLIENT_CYCLE_PREV;
    nks = XCB_NO_SYMBOL; nmm = 0;
    pks = XCB_NO_SYMBOL; pmm = 0;
    (void) keyboard_find(nt, &nks, &nmm);
    (void) keyboard_find(pt, &pks, &pmm);
    g_cycle_menu.next_keysym = nks;
    g_cycle_menu.next_modmask =
        (uint16_t) ((unsigned int) nmm & ~(unsigned int) lock_mask);
    g_cycle_menu.prev_keysym = pks;
    g_cycle_menu.prev_modmask =
        (uint16_t) ((unsigned int) pmm & ~(unsigned int) lock_mask);

    /* Together with 'keyboard_keysym_is_modifier' and its use in
     * 's_handle_cycle_key' (both in 'input/kbd/event.c'), which ignores
     * a bare modifier key-press instead of closing the menu on it, this
     * is the other half of what makes standard 'Alt-Tab'-style cycling
     * work.  Switching direction with 'Shift', while 'Alt' stays held,
     * without the menu closing on either the Shift press or its
     * release.
     *
     * The modifier this menu stays open for, and auto-confirms on
     * release of (see 'keyboard_handle_release', 'input/kbd/event.c'),
     * is only the bits shared by BOTH the cycle-next and cycle-prev
     * bindings, e.g., just 'Alt' when "next" is 'Alt+Tab' and "prev" is
     * 'Alt+Shift+Tab'.  The bit that differs between the two (Shift, in
     * that example) is what lets a person switch direction while the
     * menu stays open, by pressing the direction key again with that
     * bit now toggled, so it must never itself be treated as part of
     * what has to stay held.  Using the full modmask of whichever
     * specific binding was pressed to get here, as this used to, would
     * include that differing bit, closing the menu the moment it alone
     * is released instead of only on 'Alt''s own release. */
    g_cycle_menu.modifier =
        (uint16_t) ((unsigned int) g_cycle_menu.next_modmask &
                (unsigned int) g_cycle_menu.prev_modmask);
    g_cycle_menu.preview_client = NULL;
    g_cycle_menu.config = cfg;

    /* Collect matching clients, i.e., iterate from tail (top of stack,
     * most recently raised) to head (bottom), so the list order matches
     * the MRU ordering used by openbox and evilwm. */
    node = cdlist_tail(desktop->stacking);
    initial = node;
    if (node != NULL) {
        do {
            client_td *const c = (client_td *) cdlist_data(node);
            if (c != NULL && client_is_focusable(c) &&
                    !(c->properties.flags & CLIENT_FLAG_SKIP_TASKBAR)) {
                bool want = (is_icon)
                    ? (bool) client_is_iconified(c)
                    : !client_is_iconified(c);
                if (want &&
                        g_cycle_menu.count < WM_CYCLE_MENU_MAX_ENTRIES) {
                    int idx = g_cycle_menu.count;
                    const char *name = (c->info.name != NULL &&
                            c->info.name[0] != '\0')
                        ? c->info.name : "(unnamed)";

                    g_cycle_menu.clients[idx] = c;

                    /* Encode window state directly in the menu label:
                     *  - icon   -> "(name)"
                     *  - hidden -> "<name>"
                     *  - normal ->  "name"
                     * 'CLIENT_FLAG_HIDDEN' is set for BOTH an
                     * iconified and a genuinely hidden client (see
                     * 'client_hide', called from both paths), so
                     * the more specific iconified state has to be
                     * checked first; the hidden flag is only checked
                     * once iconified has already been ruled out. */
                    if (c->properties.state ==
                            (uint16_t) CLIENT_STATE_ICONIFIED) {
                        snprintf(g_cycle_menu.labels[idx],
                                WM_CYCLE_MENU_ENTRY_LENGTH, "(%s)", name);
                    } else if (c->properties.flags & CLIENT_FLAG_HIDDEN) {
                        snprintf(g_cycle_menu.labels[idx],
                                WM_CYCLE_MENU_ENTRY_LENGTH, "<%s>", name);
                    } else {
                        snprintf(g_cycle_menu.labels[idx],
                                WM_CYCLE_MENU_ENTRY_LENGTH, "%s", name);
                    }

                    if (c->id == desktop->client_active_id) {
                        active_idx = idx;
                    }
                    g_cycle_menu.count++;
                }
            }
            node = cdlist_prev(node);
        } while (node != NULL && node != initial);
    }

    if (g_cycle_menu.count == 0) {
        return;
    }

    /* Preselect: start from active client, step by preselect */
    if (active_idx >= 0) {
        g_cycle_menu.selected =
            (active_idx + preselect + g_cycle_menu.count) %
            g_cycle_menu.count;
    } else {
        g_cycle_menu.selected =
            (preselect >= 0) ? 0 : g_cycle_menu.count - 1;
    }

    /* Compute dimensions.  Every label is measured in both fonts an
     * entry could actually be drawn in ('unselected' and 'selected',
     * bold by default), the same reasoning as 'ctxmenu_width_compute'
     * in ctxmenu/layout.c: sizing off only one leaves no room for the
     * wider one once the highlight lands on it.  Each individual
     * measurement is capped at 'WM_CYCLE_MENU_LABEL_MAX_WIDTH' so one
     * very long window title cannot stretch the whole menu; such
     * a label is truncated when actually drawn instead (see
     * 's_cycle_draw_row' in menu/cycledraw.c). */
    text_renderer_init(connection, cfg->theme.menu.unselected.font);
    for (int i = 0; i < g_cycle_menu.count; ++i) {
        uint16_t w = menu_draw_measure(g_cycle_menu.labels[i]);
        if (w > (uint16_t) WM_CYCLE_MENU_LABEL_MAX_WIDTH) {
            w = (uint16_t) WM_CYCLE_MENU_LABEL_MAX_WIDTH;
        }
        if (w > max_w) { max_w = w; }
    }
    text_renderer_init(connection, cfg->theme.menu.selected.font);
    for (int i = 0; i < g_cycle_menu.count; ++i) {
        uint16_t w = menu_draw_measure(g_cycle_menu.labels[i]);
        if (w > (uint16_t) WM_CYCLE_MENU_LABEL_MAX_WIDTH) {
            w = (uint16_t) WM_CYCLE_MENU_LABEL_MAX_WIDTH;
        }
        if (w > max_w) { max_w = w; }
    }

    menu_w = (uint16_t) (max_w +
            (uint16_t) (cfg->theme.menu.padding.horizontal * 2u));

    /* Widen for a row's own client icon, the same reservation
     * 'cycle_draw' makes per row; see 'theme.menu.show-pixmaps''s
     * comment in 'config.h'. */
    if (cfg->theme.menu.show_pixmaps) {
        /* '#if', not a runtime ternary: both operands are fixed
         * compile-time constants, so a ternary here left one branch
         * provably unreachable to the compiler ('-Wunreachable-code').
         * Still guards the arithmetic against a future edit to either
         * constant that would otherwise underflow silently. */
#if WM_CYCLE_MENU_ROW_HEIGHT > WM_MENU_ICON_INSET
        uint16_t icon_size = (uint16_t)
            (WM_CYCLE_MENU_ROW_HEIGHT - WM_MENU_ICON_INSET);
#else
        uint16_t icon_size = 0u;
#endif

        menu_w = (uint16_t) (menu_w + icon_size +
                cfg->theme.menu.padding.horizontal);
    }

    /* Cap visible height at 'WM_CYCLE_MENU_MAX_HEIGHT_PERCENT' of
     * screen */
    screen_h_pct = surface->properties.dim.h *
        (uint32_t) WM_CYCLE_MENU_MAX_HEIGHT_PERCENT / 100u;
    pad2 = cfg->theme.menu.padding.vertical * 2u;
    avail = (screen_h_pct > pad2) ? (screen_h_pct - pad2) : 0u;
    vp_rows = (int) (avail / (uint32_t) WM_CYCLE_MENU_ROW_HEIGHT);

    if (vp_rows < 1) {
        vp_rows = 1;
    }
    if (vp_rows > g_cycle_menu.count) {
        vp_rows = g_cycle_menu.count;
    }
    g_cycle_menu.viewport_rows = vp_rows;
    g_cycle_menu.scroll_offset = 0;
    g_cycle_menu.has_drawn_once = false;
    s_cycle_scroll_to_selection();

    menu_h = (uint16_t) (cfg->theme.menu.padding.vertical * 2u +
            (uint32_t) (g_cycle_menu.viewport_rows *
                WM_CYCLE_MENU_ROW_HEIGHT));

    g_cycle_menu.width = menu_w;

    menu_x = (int16_t) (((int32_t) surface->properties.dim.w -
                (int32_t) menu_w) / 2);
    menu_y = (int16_t) (((int32_t) surface->properties.dim.h -
                (int32_t) menu_h) / 2);
    if (menu_x < 0) { menu_x = 0; }
    if (menu_y < 0) { menu_y = 0; }

    g_cycle_menu.window = xcb_generate_id(connection);

    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
        XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    values[0] = cfg->theme.menu.unselected.color.background;
    values[1] = cfg->theme.menu.border.color;
    values[2] = 1;  /* override_redirect = true */
    values[3] = XCB_EVENT_MASK_EXPOSURE    |
        XCB_EVENT_MASK_KEY_PRESS    |
        XCB_EVENT_MASK_KEY_RELEASE  |
        XCB_EVENT_MASK_BUTTON_PRESS;

    xcb_create_window(connection,
            XCB_COPY_FROM_PARENT,
            g_cycle_menu.window,
            surface->screen->root,
            menu_x, menu_y,
            menu_w, menu_h,
            (uint16_t) cfg->theme.menu.border.width,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    /* Same window-level opacity 'ctxmenu.c''s own window publishes,
     * shared with it via 'config_theme_s.menu.opacity' ('config.h') */
    atom_set_window_opacity(connection, g_cycle_menu.window,
            config_theme_opacity_to_raw(cfg->theme.menu.opacity));

    xcb_map_window(connection, g_cycle_menu.window);
    xcb_set_input_focus(connection,
            XCB_INPUT_FOCUS_POINTER_ROOT,
            g_cycle_menu.window,
            XCB_CURRENT_TIME);

    /* Already applies the same "selected" icon render (active colors,
     * caption and hints, no pixmap) that every later navigation call
     * gets via 's_cycle_repaint_icon' (see 'mi_cycle_preview_apply''s
     * implementation in 'menu/cycledraw.c', which calls
     * 'ri_render_client_icon_selected' directly for exactly this
     * reason) so the cycle's own initial preselection needs no separate
     * call here to match it. */
    mi_cycle_preview_apply(connection, cfg);

    xcb_flush(connection);
}


/* Destroy the cycle menu and restore previous focus */
void cycle_destroy(xcb_connection_t *connection)
{
    xcb_window_t restore_focus;
    surface_td *surface;

    if (connection == NULL || g_cycle_menu.window == XCB_WINDOW_NONE) {
        return;
    }

    restore_focus = g_cycle_menu.prev_focus;
    surface = g_cycle_menu.surface;
    s_cycle_preview_restore(connection);
    render_outline_hide(connection, g_cycle_menu.outline_windows);

    xcb_destroy_window(connection, g_cycle_menu.window);
    g_cycle_menu.window = XCB_WINDOW_NONE;
    g_cycle_menu.count = 0;
    g_cycle_menu.selected = 0;
    g_cycle_menu.width = 0;
    g_cycle_menu.surface = NULL;
    g_cycle_menu.desktop = NULL;
    g_cycle_menu.modifier = 0;
    g_cycle_menu.prev_focus = XCB_WINDOW_NONE;
    g_cycle_menu.next_keysym = XCB_NO_SYMBOL;
    g_cycle_menu.next_modmask = 0;
    g_cycle_menu.prev_keysym = XCB_NO_SYMBOL;
    g_cycle_menu.prev_modmask = 0;
    g_cycle_menu.preview_client = NULL;
    g_cycle_menu.config = NULL;
    g_cycle_menu.scroll_offset = 0;
    g_cycle_menu.viewport_rows = 0;

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


/* Confirm the currently selected cycle menu entry */
void cycle_confirm(xcb_connection_t *connection, list_td *surfaces,
        const config_td *cfg)
{
    client_td *target;
    surface_td *surface;
    desktop_td *desktop;
    bool is_icon;

    if (g_cycle_menu.window == XCB_WINDOW_NONE ||
            g_cycle_menu.selected < 0 ||
            g_cycle_menu.selected >= g_cycle_menu.count) {
        return;
    }

    target = g_cycle_menu.clients[g_cycle_menu.selected];
    surface = g_cycle_menu.surface;
    desktop = g_cycle_menu.desktop;
    is_icon = g_cycle_menu.is_icon_menu;

    cycle_destroy(connection);

    if (target == NULL || surface == NULL || desktop == NULL) {
        return;
    }

    if (is_icon) {
        enact_client_restore(target);
    } else if (target->properties.flags & CLIENT_FLAG_HIDDEN) {
        /* Hidden (non-iconified) window: unhide before focusing */
        enact_client_unhide(target);
    }

    if (!is_icon && client_is_shaded(target)) {
        enact_client_unshade(target);
    }

    focus_apply(surfaces, surface, desktop, target, true, cfg);
}


/**
 * @brief Repaint a client's real desktop icon (not the cycle menu's own
 *        preview), so it reflects a just-changed cycle-selection state
 *        right away
 *
 * @a cycle_navigate_to / @a cycle_navigate_to_next /
 * @a cycle_navigate_prev only ever touch the floating cycle menu's own
 * selection state; nothing about the real icon window sitting on the
 * desktop underneath it is otherwise told to repaint when that
 * selection moves on, so a client that was highlighted and then passed
 * over stays visually stuck showing that highlight until something
 * unrelated (e.g., @a cycle_destroy) eventually forces a full desktop
 * repaint.
 *
 * Called for both the previously-selected and newly-selected client on
 * every navigation, this keeps their real icons in sync with the menu
 * immediately instead.
 *
 * @p is_selected picks which of the two very different renders that
 * sync actually needs: the client this cycle just selected gets
 * @a ri_render_client_icon_selected (active colors, its caption, and
 * its own hint indicators, but deliberately no pixmap) while the client
 * just passed over gets a full @a ri_render_client_icon render instead,
 * back to its ordinary inactive appearance, pixmap, caption, and hint
 * indicators all included.
 *
 * @param client      Client whose real desktop icon to repaint
 * @param is_selected Whether @p client is the cycle's own newly
 *                     selected entry (@c true), or the one just
 *                     passed over (@c false)
 *
 * @note A no-op for a client that is not actually an iconified icon (or
 *       @c NULL, or with no cycle menu open at all); both render
 *       functions already guard that safely on their own
 * @note The cycle's own initial preselection at @a cycle_init time
 *       needs no separate call here
 * @note Complexity: @e O(1)
 *
 * @see @a ri_render_client_icon_selected's own comment in
 *      @c render/icon.h
 * @ see @a mi_cycle_preview_apply in @c menu/cycledraw.c, which already
 *       applies the very same "selected" render this function itself
 *       calls below.
 */
static void s_cycle_repaint_icon(client_td *client, bool is_selected)
{
    if (client == NULL || g_cycle_menu.desktop == NULL) {
        return;
    }

    if (is_selected) {
        ri_render_client_icon_selected(g_cycle_menu.desktop->connection,
                client);
    } else {
        ri_render_client_icon(g_cycle_menu.desktop, client, true);
    }
    xcb_flush(g_cycle_menu.desktop->connection);
}


/* Set the selection directly to a given index */
void cycle_navigate_to(unsigned int idx)
{
    client_td *prev_client;

    if (g_cycle_menu.count <= 0) {
        return;
    }

    if ((int) idx >= g_cycle_menu.count) {
        idx = (unsigned int)(g_cycle_menu.count - 1);
    }

    prev_client = cycle_get_selected_client();
    g_cycle_menu.selected = (int) idx;
    s_cycle_scroll_to_selection();

    if (prev_client != cycle_get_selected_client()) {
        s_cycle_repaint_icon(prev_client, false);
        s_cycle_repaint_icon(cycle_get_selected_client(), true);
    }
}


/* Force the next 'cycle_draw' call to repaint the whole viewport
 * (see this function's own comment in 'menu/cycle.h') */
void cycle_force_full_repaint(void)
{
    g_cycle_menu.has_drawn_once = false;
}


/* Advance the selection by one entry */
void cycle_navigate_next(void)
{
    client_td *prev_client;

    if (g_cycle_menu.count <= 0) {
        return;
    }

    prev_client = cycle_get_selected_client();
    g_cycle_menu.selected =
        (g_cycle_menu.selected + 1) % g_cycle_menu.count;
    s_cycle_scroll_to_selection();

    s_cycle_repaint_icon(prev_client, false);
    s_cycle_repaint_icon(cycle_get_selected_client(), true);
}


/* Retreat the selection by one entry */
void cycle_navigate_prev(void)
{
    client_td *prev_client;

    if (g_cycle_menu.count <= 0) {
        return;
    }

    prev_client = cycle_get_selected_client();
    g_cycle_menu.selected =
        (g_cycle_menu.selected - 1 +
         g_cycle_menu.count) % g_cycle_menu.count;
    s_cycle_scroll_to_selection();

    s_cycle_repaint_icon(prev_client, false);
    s_cycle_repaint_icon(cycle_get_selected_client(), true);
}


/* Query whether the cycle menu is currently open */
bool cycle_is_open(void)
{
    return g_cycle_menu.window != XCB_WINDOW_NONE;
}


/* Return the cycle menu window identifier */
xcb_window_t cycle_window(void)
{
    return g_cycle_menu.window;
}


/* Return the currently highlighted client in the cycle menu */
client_td *cycle_get_selected_client(void)
{
    if (g_cycle_menu.count <= 0 ||
            g_cycle_menu.selected < 0 ||
            g_cycle_menu.selected >= g_cycle_menu.count) {
        return NULL;
    }

    return g_cycle_menu.clients[g_cycle_menu.selected];
}


/* Return the modifier mask that opened the cycle menu */
uint16_t cycle_modifier(void)
{
    return g_cycle_menu.modifier;
}


/* Return the keysym configured for cycle-next navigation */
xcb_keysym_t cycle_next_keysym(void)
{
    return g_cycle_menu.next_keysym;
}


/* Return the modifier mask for the cycle-next binding */
uint16_t cycle_next_modmask(void)
{
    return g_cycle_menu.next_modmask;
}


/* Return the keysym configured for cycle-prev navigation */
xcb_keysym_t cycle_prev_keysym(void)
{
    return g_cycle_menu.prev_keysym;
}


/* Return the modifier mask for the cycle-prev binding */
uint16_t cycle_prev_modmask(void)
{
    return g_cycle_menu.prev_modmask;
}
