/**
 * @file menu/context/wincmenu.c
 *
 * @brief Window context menu implementation
 *
 * Builds and manages the right-click context menu for client windows.
 * Actions are dispatched via the standard event queue so they are
 * processed on the next event loop iteration.
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
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* malloc, free, calloc */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <event.h>
#include <eventq.h>
#include <lookup.h>
#include <priority.h>
#include <surface.h>
#include <wm.h>

/* CMD includes */
#include <cmds/ccmd.h>
#include <cmds/layer.h>
#include <cmds/state.h>

/* Input includes */
/* Keyboard modal move/resize */
#include <input/kbd/modal.h>
#include <input/mouse/drag.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>
#include <menu/context/wincmenu.h>


/**
 * @brief Maximum number of desktops shown in the "Send to desktop"
 *        submenu (capped to avoid oversized menus)
 */
#define WINCMENU_MAX_DESKTOPS (32)

/**
 * @brief Number of fixed entries in the "Layer" submenu
 */
#define WINCMENU_LAYER_COUNT (3)

/**
 * @brief Number of fixed top-level entries in the window context menu:
 *        TWO submenus (Send to desktop, Layer) + ONE separator + EIGHT
 *        commands (Restore, Move, Resize, Iconify, Hide, Maximize, Roll
 *        up/down, Un/decorate) + ONE separator + ONE command (Close)
 *        = THIRTEEN total
 */
#define WINCMENU_FIXED_ENTRIES (13)

/**
 * @brief Total top-level entry slots:
 *        @c (WINCMENU_FIXED_ENTRIES + 2) extra slots reserved for
 *        future or dynamic entries
 */
#define WINCMENU_TOTAL_ENTRIES (WINCMENU_FIXED_ENTRIES + 2)


/**
 * @brief Userdata structure passed to the "Send to desktop" callbacks
 */
typedef struct {
    client_td *client;      /**< Target client */
    desktop_td *src;        /**< Source desktop */
    desktop_td *dst;        /**< Destination desktop */
} wincmenu_send_data_td;


/** Singleton root menu state */
static ctxmenu_state_td s_root;

/** Entries for the top-level window context menu */
static ctxmenu_entry_td s_entries[WINCMENU_TOTAL_ENTRIES];

/** Entries for the "Send to desktop" submenu */
static ctxmenu_entry_td s_desk_entries[WINCMENU_MAX_DESKTOPS + 1];

/** State for the "Send to desktop" child menu */
static ctxmenu_state_td s_desk_state;

/** Entries for the "Layer" submenu */
static ctxmenu_entry_td s_layer_entries[WINCMENU_LAYER_COUNT];

/** State for the "Layer" child menu */
static ctxmenu_state_td s_layer_state;

/** Per-desktop userdata pool for "Send to desktop" callbacks */
static wincmenu_send_data_td s_send_data[WINCMENU_MAX_DESKTOPS + 1];

/** Pointer to the target client (valid while the menu is open) */
static client_td *s_target_client = NULL;

/** Pointer to the surface (valid while the menu is open) */
static surface_td *s_surface = NULL;

/** Pointer to the source desktop (valid while the menu is open) */
static desktop_td *s_desktop = NULL;

/** Active configuration (valid while the menu is open) */
static const config_td *s_config = NULL;


/**
 * @brief Callback: send client to a specific desktop
 *
 * @param connection XCB connection (unused; dispatch is via event queue)
 * @param userdata   Pointer to @c wincmenu_send_data_td
 */
static void s_cb_send_to_desktop(xcb_connection_t *connection,
        void *userdata)
{
    wincmenu_send_data_td *d;
    action_td action;
    action_data_desktop_td *data;
    event_td *event;

    (void) connection;

    if (userdata == NULL) {
        return;
    }
    d = (wincmenu_send_data_td *) userdata;
    if (d->client == NULL || d->src == NULL || d->dst == NULL) {
        return;
    }

    action.type = ACTION_TYPE_DESKTOP;
    action.object.desktop = ACTION_DESKTOP_CLIENT_SEND;

    data = action_data_desktop_init(d->src, action.object.desktop);
    if (data == NULL) {
        return;
    }
    data->client = d->client;
    data->target = d->dst;

    event = event_init((void *) d->src, (void *) data,
            action, PRIORITY_NORMAL);
    if (event == NULL) {
        action_data_desktop_destroy(data);
        return;
    }
    (void) eventq_add(event);
}


/**
 * @brief Callback: send client to all desktops (sticky)
 *
 * @param connection XCB connection
 * @param userdata   Unused
 */
static void s_cb_sticky(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_TOGGLE_STICKY, PRIORITY_NORMAL);
    }
}


/**
 * @brief Callback: set layer to @c above
 */
static void s_cb_layer_above(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_LAYER_ABOVE, PRIORITY_NORMAL);
    }
}


/**
 * @brief Callback: set layer to @c normal
 */
static void s_cb_layer_normal(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_LAYER_NORMAL, PRIORITY_NORMAL);
    }
}


/**
 * @brief Callback: set layer to @c below
 */
static void s_cb_layer_below(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_LAYER_BELOW, PRIORITY_NORMAL);
    }
}


/**
 * @brief Callback: restore the client
 */
static void s_cb_restore(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_RESTORE, PRIORITY_NORMAL);
    }
}


/**
 * @brief Callback: move the client
 *
 * Dispatches based on how the "Move" entry was activated:
 * - Activated from the keyboard (@c Return or a letter shortcut):
 *   enters keyboard modal move mode, exactly like the "Resize" entry
 *   does for keyboard activation.  Arrow keys move the window,
 *   @c Return confirms, @c Escape restores the original position.
 * - Activated with the mouse (a click on the entry): warps the pointer
 *   to the window's centre and starts a pointer-driven move drag, so
 *   the window then follows the mouse until the button is released.
 */
static void s_cb_move(xcb_connection_t *connection,
        void *userdata)
{
    xcb_window_t root_win;
    int32_t center_x;
    int32_t center_y;
    uint32_t snap;
    surface_td *surface;

    (void) userdata;

    if (s_target_client == NULL || connection == NULL || s_surface == NULL ||
            s_surface->screen == NULL) {
        return;
    }

    /* Cannot move a fully-maximized or fullscreen window */
    if (client_is_maximized(s_target_client) ||
            client_is_fullscreen(s_target_client)) {
        return;
    }

    if (ctxmenu_last_activation_was_keyboard()) {
        surface = wm_get_surface_by_id(s_target_client->screen_id);
        kbd_modal_move_start(connection, surface, s_target_client);
        return;
    }

    root_win = s_surface->screen->root;
    if (root_win == XCB_WINDOW_NONE) {
        return;
    }

    center_x = s_target_client->layout.geometry.cur.pos.x
        + (int32_t) (s_target_client->layout.geometry.cur.dim.w / 2u);
    center_y = s_target_client->layout.geometry.cur.pos.y
        + (int32_t) (s_target_client->layout.geometry.cur.dim.h / 2u);
    snap = (s_config != NULL) ? s_config->base.windows.snap : 0u;

    xcb_warp_pointer(connection, XCB_NONE, root_win,
            0, 0, 0, 0,
            (int16_t) center_x, (int16_t) center_y);
    xcb_flush(connection);

    drag_start(connection, root_win, s_target_client, s_desktop,
            CLIENT_OPERATION_MOVING,
            XCB_CURRENT_TIME,
            (int16_t) center_x, (int16_t) center_y,
            s_surface->properties.dim.w,
            s_surface->properties.dim.h,
            snap);
}


/**
 * @brief Callback: resize the client
 *
 * Dispatches based on how the "Resize" entry was activated:
 * - Activated from the keyboard: enters keyboard modal resize mode.
 *   The user presses arrow keys to grow or shrink along the chosen
 *   edge; @c Return confirms and @c Escape restores the original
 *   geometry.
 * - Activated with the mouse: warps the pointer to the window's
 *   bottom-right corner and starts a pointer-driven resize drag, so the
 *   window is resized from that corner as the mouse moves, exactly like
 *   dragging the visible bottom-right resize handle.
 */
static void s_cb_resize(xcb_connection_t *connection,
        void *userdata)
{
    surface_td *surface;
    xcb_window_t root_win;
    int32_t corner_x;
    int32_t corner_y;
    uint32_t snap;

    (void) userdata;

    if (s_target_client == NULL || connection == NULL) {
        return;
    }

    if (!client_is_resizable(s_target_client) ||
            client_is_maximized(s_target_client) ||
            client_is_fullscreen(s_target_client)) {
        return;
    }

    if (client_is_shaded(s_target_client)) {
        wcmd_client_unshade(s_target_client);
    }

    surface = wm_get_surface_by_id(s_target_client->screen_id);

    if (ctxmenu_last_activation_was_keyboard()) {
        kbd_modal_resize_start(connection, surface, s_target_client);
        return;
    }

    if (s_surface == NULL || s_surface->screen == NULL) {
        return;
    }

    root_win = s_surface->screen->root;
    if (root_win == XCB_WINDOW_NONE) {
        return;
    }

    /* Warp just inside the bottom-right corner so 'drag_start' picks it
     * up as a corner resize handle (both edges active) rather than the
     * single-edge fallback that applies further from the corner */
    corner_x = s_target_client->layout.geometry.cur.pos.x
        + (int32_t) s_target_client->layout.geometry.cur.dim.w - 1;
    corner_y = s_target_client->layout.geometry.cur.pos.y
        + (int32_t) s_target_client->layout.geometry.cur.dim.h - 1;
    snap = (s_config != NULL) ? s_config->base.windows.snap : 0u;

    xcb_warp_pointer(connection, XCB_NONE, root_win,
            0, 0, 0, 0,
            (int16_t) corner_x, (int16_t) corner_y);
    xcb_flush(connection);

    drag_start(connection, root_win, s_target_client, s_desktop,
            CLIENT_OPERATION_RESIZING,
            XCB_CURRENT_TIME,
            (int16_t) corner_x, (int16_t) corner_y,
            s_surface->properties.dim.w,
            s_surface->properties.dim.h,
            snap);
}


/**
 * @brief Callback: iconify the client
 */
static void s_cb_iconify(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_ICONIFY, PRIORITY_NORMAL);
    }
}


/**
 * @brief Callback: hide the client
 */
static void s_cb_hide(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_HIDE, PRIORITY_NORMAL);
    }
}


/**
 * @brief Callback: maximize the client
 */
static void s_cb_maximize(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_MAXIMIZE, PRIORITY_NORMAL);
    }
}


/**
 * @brief Callback: toggle shade (roll up/down)
 */
static void s_cb_shade(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_TOGGLE_SHADE, PRIORITY_NORMAL);
    }
}


/**
 * @brief Callback: toggle decoration
 *
 * A rolled-up (shaded) window must be unrolled before its decoration
 * can be toggled, because the decorated titlebar is what keeps the
 * shade state meaningful.
 */
static void s_cb_decorate(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        if (client_is_shaded(s_target_client)) {
            wcmd_client_unshade(s_target_client);
        }
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_TOGGLE_DECORATION, PRIORITY_NORMAL);
    }
}


/**
 * @brief Callback: close the client
 */
static void s_cb_close(xcb_connection_t *connection,
        void *userdata)
{
    (void) connection;
    (void) userdata;

    if (s_target_client != NULL) {
        (void) client_send_event(s_target_client,
                ACTION_CLIENT_CLOSE, PRIORITY_NORMAL);
    }
}


/**
 * @brief Fill an entry with a command entry and optional callback
 *
 * @param e           Pointer to entry to fill
 * @param label       Entry label text
 * @param cb          Callback to invoke on activation (may be null)
 * @param userdata    Passed to @p cb
 * @param is_disabled Whether the entry is grayed out
 */
static void s_entry_command(ctxmenu_entry_td *e, const char *label,
        void (*cb)(xcb_connection_t *, void *), void *userdata,
        bool is_disabled)
{
    memset(e, 0, sizeof(*e));
    e->type = CTXMENU_COMMAND;
    safe_strncpy(e->label, label, sizeof(e->label) - 1u);
    e->on_activate = cb;
    e->userdata = userdata;
    e->is_disabled = is_disabled;
}


/**
 * @brief Build the "Send to desktop" submenu entries
 *
 * @param surface Surface that owns the desktops
 * @param desktop Currently active desktop
 * @param client  Target client
 *
 * @return Number of entries filled in @a s_desk_entries
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
static int s_build_desk_entries(surface_td *surface,
        desktop_td *desktop, client_td *client)
{
    int n = 0;
    uint32_t d_idx;
    desktop_td *d;
    bool is_cur;
    bool is_sticky;

    is_sticky = (client->properties.flags & CLIENT_FLAG_STICKY) != 0u;

    for (d_idx = 0; d_idx < surface->desktop_count &&
            n < WINCMENU_MAX_DESKTOPS; ++d_idx) {
        d = surface_desktop_get(surface, d_idx);
        if (d == NULL) {
            continue;
        }
        is_cur = (d->id == desktop->id);

        if (d->name[0] != '\0') {
            (void) snprintf(s_desk_entries[n].label,
                    sizeof(s_desk_entries[n].label),
                    "%s [%u] -- %s %s",
                    MENU_CONTEXT_CTXMENU_LABEL_PREFIX,
                    d_idx, d->name,
                    MENU_CONTEXT_CTXMENU_LABEL_SUFFIX);
        } else {
            (void) snprintf(s_desk_entries[n].label,
                    sizeof(s_desk_entries[n].label),
                    "%s [%u] %s",
                    MENU_CONTEXT_CTXMENU_LABEL_PREFIX,
                    d_idx,
                    MENU_CONTEXT_CTXMENU_LABEL_SUFFIX);
        }

        s_desk_entries[n].type = CTXMENU_COMMAND;
        s_desk_entries[n].is_disabled = is_cur;
        s_send_data[n].client = client;
        s_send_data[n].src = desktop;
        s_send_data[n].dst = d;
        s_desk_entries[n].on_activate = s_cb_send_to_desktop;
        s_desk_entries[n].userdata = &s_send_data[n];
        ++n;
    }

    /* 'All desktops' entry for sticky support */
    safe_strncpy(s_desk_entries[n].label, "All desktops",
            sizeof(s_desk_entries[n].label) - 1u);
    s_desk_entries[n].type = CTXMENU_COMMAND;
    s_desk_entries[n].is_disabled = is_sticky;
    s_desk_entries[n].on_activate = s_cb_sticky;
    s_desk_entries[n].userdata = NULL;
    ++n;

    return n;
}


/**
 * @brief Build the @c Layer submenu entries
 *
 * @param client Target client (used to disable the current layer)
 */
static void s_build_layer_entries(const client_td *client)
{
    bool is_above;
    bool is_normal;
    bool is_below;

    is_above  = (client->properties.layer ==
            (uint16_t) CLIENT_LAYER_ABOVE);
    is_normal = (client->properties.layer ==
            (uint16_t) CLIENT_LAYER_NORMAL);
    is_below  = (client->properties.layer ==
            (uint16_t) CLIENT_LAYER_BELOW);

    s_entry_command(&s_layer_entries[0], "Always on top",
            s_cb_layer_above, NULL, is_above);
    s_entry_command(&s_layer_entries[1], "Normal",
            s_cb_layer_normal, NULL, is_normal);
    s_entry_command(&s_layer_entries[2], "Always on bottom",
            s_cb_layer_below, NULL, is_below);
}


/* Open the window context menu for a client */
void wincmenu_show(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, client_td *client,
        int16_t x, int16_t y, const config_td *config)
{
    int n;
    int desk_count;
    bool can_restore;
    bool can_move;
    bool can_resize;
    bool can_shade;

    if (connection == NULL || surface == NULL || desktop == NULL ||
            client == NULL || config == NULL) {
        return;
    }

    /* Close any previously open window context menu */
    wincmenu_close();

    s_target_client = client;
    s_surface = surface;
    s_desktop = desktop;
    s_config = config;

    /* Determine disabled states.
     * A window that is only partially maximized (horizontal or vertical
     * only) can still be moved, but fully-maximized and fullscreen
     * windows cannot be moved or resized at all. */
    can_restore = client_is_maximized(client)
        || client_is_fullscreen(client);
    can_move = !client_is_maximized(client)
        && !client_is_fullscreen(client);
    can_resize = client_is_resizable(client)
        && !client_is_maximized(client)
        && !client_is_fullscreen(client);
    can_shade = (client->properties.flags &
            CLIENT_FLAG_DECORATED) != 0u;

    /* Build 'Send to desktop' submenu */
    memset(s_desk_entries, 0, sizeof(s_desk_entries));
    desk_count = s_build_desk_entries(surface, desktop, client);

    memset(&s_desk_state, 0, sizeof(s_desk_state));
    s_desk_state.window = XCB_WINDOW_NONE;
    s_desk_state.entries = s_desk_entries;
    s_desk_state.entry_count = desk_count;

    /* Build "Layer" submenu */
    memset(s_layer_entries, 0, sizeof(s_layer_entries));
    s_build_layer_entries(client);

    memset(&s_layer_state, 0, sizeof(s_layer_state));
    s_layer_state.window = XCB_WINDOW_NONE;
    s_layer_state.entries = s_layer_entries;
    s_layer_state.entry_count = WINCMENU_LAYER_COUNT;

    /* Build top-level entries */
    memset(s_entries, 0, sizeof(s_entries));
    n = 0;

    /* Send to desktop (submenu) */
    s_entries[n].type = CTXMENU_SUBMENU;
    safe_strncpy(s_entries[n].label, "Send to desktop",
            sizeof(s_entries[n].label) - 1u);
    s_entries[n].items = s_desk_entries;
    s_entries[n].item_count = desk_count;
    s_entries[n].userdata = &s_desk_state;
    ++n;

    /* Layer (submenu) */
    s_entries[n].type = CTXMENU_SUBMENU;
    safe_strncpy(s_entries[n].label, "Layer",
            sizeof(s_entries[n].label) - 1u);
    s_entries[n].items = s_layer_entries;
    s_entries[n].item_count = WINCMENU_LAYER_COUNT;
    s_entries[n].userdata = &s_layer_state;
    ++n;

    /* Separator */
    s_entries[n].type = CTXMENU_SEPARATOR;
    ++n;

    s_entry_command(&s_entries[n], "Restore",
            s_cb_restore, NULL, !can_restore);
    ++n;

    s_entry_command(&s_entries[n], "Move",
            s_cb_move, NULL, !can_move);
    ++n;

    s_entry_command(&s_entries[n], "Resize",
            s_cb_resize, NULL, !can_resize);
    ++n;

    s_entry_command(&s_entries[n], "Iconify",
            s_cb_iconify, NULL, false);
    ++n;

    s_entry_command(&s_entries[n], "Hide",
            s_cb_hide, NULL, false);
    ++n;

    s_entry_command(&s_entries[n], "Maximize",
            s_cb_maximize, NULL,
            !client_is_resizable(client) || client_is_maximized(client));
    ++n;

    s_entry_command(&s_entries[n], "Roll up/down",
            s_cb_shade, NULL, !can_shade);
    ++n;

    s_entry_command(&s_entries[n], "Un/decorate",
            s_cb_decorate, NULL, false);
    ++n;

    /* Separator before Close */
    s_entries[n].type = CTXMENU_SEPARATOR;
    ++n;

    s_entry_command(&s_entries[n], "Close",
            s_cb_close, NULL, false);
    ++n;

    memset(&s_root, 0, sizeof(s_root));
    s_root.window = XCB_WINDOW_NONE;
    s_root.entries = s_entries;
    s_root.entry_count = n;

    ctxmenu_show(connection, surface, &s_root, x, y, config);
}


/* Close the window context menu */
void wincmenu_close(void)
{
    ctxmenu_close(&s_root);
    s_target_client = NULL;
    s_surface = NULL;
    s_desktop = NULL;
    s_config = NULL;
}


/* Repaint the window context menu */
void wincmenu_repaint(xcb_window_t win)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(&s_root, win);
    if (state != NULL) {
        ctxmenu_repaint(state);
    }
}


/* Handle a button-press event inside the window context menu */
bool wincmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int x, int y,
        const config_td *config)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(&s_root, win);
    if (state == NULL) {
        return false;
    }

    x -= state->origin_x;
    y -= state->origin_y;

    return ctxmenu_handle_click(connection, surface, state,
            x, y, config);
}


/* Query whether the window context menu is currently open */
bool wincmenu_is_open(void)
{
    return ctxmenu_is_open(&s_root);
}


/* Return the root window context menu XCB window */
xcb_window_t wincmenu_window(void)
{
    return s_root.window;
}


/* Check whether 'win' belongs to the window context menu hierarchy */
bool wincmenu_owns_window(xcb_window_t win)
{
    return ctxmenu_find_state_for_window(&s_root, win) != NULL;
}


/* Handle a key-press event while the window context menu is open */
bool wincmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config)
{
    ctxmenu_state_td *deepest;
    deepest = ctxmenu_find_state_for_window(&s_root,
            ctxmenu_deepest_window(&s_root));

    if (deepest == NULL) {
        deepest = &s_root;
    }

    return ctxmenu_handle_keypress(connection, surface, deepest,
            keysym, config);
}


/* Handle a pointer-motion event over the window context menu */
void wincmenu_handle_motion(xcb_window_t win, int x, int y)
{
    ctxmenu_state_td *state;

    state = ctxmenu_find_state_for_window(&s_root, win);
    if (state != NULL) {
        ctxmenu_handle_motion(state, x, y);
    }
}
