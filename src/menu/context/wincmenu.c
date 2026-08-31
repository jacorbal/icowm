/**
 * @file menu/context/wincmenu.c
 *
 * @brief Window context menu implementation
 *
 * Builds and manages the right-click context menu for client windows.
 * Actions are dispatched by calling the matching @c enact function
 * directly, taking effect at once
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
#include <string.h>     /* memset */

/* ADT includes */
#include <adt/cdlist.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/input.h>
#include <defs/uistr.h>
#include <i18n.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* CMD includes */
#include <cmds/client/focus.h>
#include <cmds/client/layer.h>
#include <cmds/client/state.h>

/* Input includes */
/* Keyboard modal move/resize */
#include <input/kbd/modal.h>
#include <input/mouse/drag.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/select.h>
#include <menu/context/ctxmenu/tree.h>
#include <menu/dialog/inspect.h>
#include <menu/context/wincmenu.h>


/**
 * @brief Userdata structure passed to the "Send to desktop" callbacks
 */
typedef struct {
    client_td *client;      /**< Target client */
    desktop_td *src;        /**< Source desktop */
    desktop_td *dst;        /**< Destination desktop */
} wincmenu_send_data_td;


/**
 * @brief Userdata structure passed to the "Send to monitor" callbacks
 */
typedef struct {
    client_td *client;      /**< Target client */
    uint32_t monitor_index; /**< Destination monitor index */
} wincmenu_send_monitor_data_td;


/** Singleton root menu state */
static ctxmenu_state_td s_root;

/** Entries for the top-level window context menu */
static ctxmenu_entry_td s_entries[WINCMENU_TOTAL_ENTRIES];

/** Entries for the "Send to desktop" submenu */
static ctxmenu_entry_td s_desk_entries[WINCMENU_MAX_DESKTOPS + 2];

/** State for the "Send to desktop" child menu */
static ctxmenu_state_td s_desk_state;

/** Entries for the "Send to monitor" submenu */
static ctxmenu_entry_td s_monitor_entries[WINCMENU_MAX_MONITORS];

/** State for the "Send to monitor" child menu */
static ctxmenu_state_td s_monitor_state;

/** Entries for the "Layer" submenu */
static ctxmenu_entry_td s_layer_entries[WINCMENU_LAYER_COUNT];

/** State for the "Layer" child menu */
static ctxmenu_state_td s_layer_state;

/** Per-desktop userdata pool for "Send to desktop" callbacks */
static wincmenu_send_data_td s_send_data[WINCMENU_MAX_DESKTOPS + 1];

/** Per-monitor userdata pool for "Send to monitor" callbacks */
static wincmenu_send_monitor_data_td
    s_send_monitor_data[WINCMENU_MAX_MONITORS];

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
 * @param connection XCB connection (unused)
 * @param userdata   Pointer to @c wincmenu_send_data_td
 */
static void s_cb_send_to_desktop(xcb_connection_t *connection,
        void *userdata)
{
    wincmenu_send_data_td *d;

    (void) connection;

    if (userdata == NULL) {
        return;
    }
    d = (wincmenu_send_data_td *) userdata;
    if (d->client == NULL || d->src == NULL || d->dst == NULL) {
        return;
    }

    enact_desktop_client_send(d->src, d->client, d->dst);
}


/**
 * @brief Callback: send client to a specific monitor
 *
 * @param connection XCB connection (unused)
 * @param userdata   Pointer to @c wincmenu_send_monitor_data_td
 */
static void s_cb_send_to_monitor(xcb_connection_t *connection,
        void *userdata)
{
    wincmenu_send_monitor_data_td *d;

    (void) connection;

    if (userdata == NULL) {
        return;
    }
    d = (wincmenu_send_monitor_data_td *) userdata;
    if (d->client == NULL) {
        return;
    }

    enact_client_move_to_monitor(d->client, d->monitor_index);
}


/**
 * @brief Callback: move the client
 *
 * Dispatches based on how the "Move" entry was activated:
 * - Activated from the keyboard (@c Return or a letter shortcut):
 *   enters keyboard modal move mode, exactly like the "Resize" entry
 *   does for keyboard activation.  Arrow keys move the window,
 *   @c Return confirms, @c Escape restores the original position.
 * - Activated with the mouse (a click on the entry).  Warps the pointer
 *   to the window's center and starts a pointer-driven move drag, so
 *   the window then follows the mouse until the button is released.
 */
static void s_cb_move(xcb_connection_t *connection,
        void *userdata)
{
    xcb_window_t root_win;
    struct position_s center_pos;
    struct dimensions_s screen_dim;
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

    center_pos.x = s_target_client->layout.geometry.cur.pos.x
        + (int32_t) (s_target_client->layout.geometry.cur.dim.w / 2u);
    center_pos.y = s_target_client->layout.geometry.cur.pos.y
        + (int32_t) (s_target_client->layout.geometry.cur.dim.h / 2u);

    xcb_warp_pointer(connection, XCB_NONE, root_win,
            0, 0, 0, 0,
            (int16_t) center_pos.x, (int16_t) center_pos.y);

    screen_dim.w = s_surface->properties.dim.w;
    screen_dim.h = s_surface->properties.dim.h;
    drag_start(connection, root_win, s_target_client, s_desktop,
            CLIENT_OPERATION_MOVING,
            XCB_CURRENT_TIME,
            center_pos, screen_dim);
}


/**
 * @brief Pick the resize-drag corner diagonally opposite the window's
 *        screen quadrant
 *
 * Determines which screen quadrant the window's center falls in and
 * returns the coordinates of the opposite corner of the window frame,
 * one pixel inside each edge so @c drag_start recognizes it as
 * a corner handle (see @c im_bounds_resize in
 * input/mouse/bounds.h):
 * - Window in the top-left quadrant    -> bottom-right corner.
 * - Window in the bottom-left quadrant -> top-right corner.
 * - Window in the bottom-right quadrant -> top-left corner.
 * - Window in the top-right quadrant   -> bottom-left corner.
 *
 * This keeps the resize handle on the side of the window that is
 * furthest from the screen edge it is closest to, so the pointer never
 * has to be warped off-screen (or right against a screen edge) to
 * start the drag.
 *
 * @param client Client to resize
 * @param surface Surface the client is on (for screen dimensions)
 * @param out_x  Output: root-relative X of the chosen corner
 * @param out_y  Output: root-relative Y of the chosen corner
 *
 * @note Complexity: @e O(1)
 */
static void s_resize_corner_grab(const client_td *client,
        const surface_td *surface, int32_t *restrict out_x,
        int32_t *restrict out_y)
{
    int32_t win_center_x;
    int32_t win_center_y;
    int32_t screen_center_x;
    int32_t screen_center_y;
    bool is_left;
    bool is_top;
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;

    left = client->layout.geometry.cur.pos.x;
    top = client->layout.geometry.cur.pos.y;
    right = left + (int32_t) client->layout.geometry.cur.dim.w - 1;
    bottom = top + (int32_t) client->layout.geometry.cur.dim.h - 1;

    win_center_x = left + (int32_t) (client->layout.geometry.cur.dim.w / 2u);
    win_center_y = top + (int32_t) (client->layout.geometry.cur.dim.h / 2u);
    screen_center_x = (int32_t) (surface->properties.dim.w / 2u);
    screen_center_y = (int32_t) (surface->properties.dim.h / 2u);

    is_left = win_center_x < screen_center_x;
    is_top = win_center_y < screen_center_y;

    *out_x = (is_left) ? right : left;
    *out_y = (is_top) ? bottom : top;
}


/**
 * @brief Callback: resize the client
 *
 * Dispatches based on how the "Resize" entry was activated:
 * - Activated from the keyboard: enters keyboard modal resize mode.
 *   The user presses arrow keys to grow or shrink along the chosen
 *   edge; @c Return confirms and @c Escape restores the original
 *   geometry.
 * - Activated with the mouse: warps the pointer to whichever corner of
 *   the window is diagonally opposite its screen quadrant and starts
 *   a pointer-driven resize drag from there, so the resize handle is
 *   always the corner furthest from the screen edge the window is
 *   closest to (and thus always reachable without the pointer having
 *   to leave the screen).  See @c s_resize_corner_grab.
 */
static void s_cb_resize(xcb_connection_t *connection,
        void *userdata)
{
    surface_td *surface;
    xcb_window_t root_win;
    struct position_s corner_pos;
    struct dimensions_s screen_dim;

    (void) userdata;

    if (s_target_client == NULL || connection == NULL) {
        return;
    }

    if (!client_is_maximizable(s_target_client) ||
            client_is_maximized(s_target_client) ||
            client_is_fullscreen(s_target_client)) {
        return;
    }

    if (client_is_shaded(s_target_client)) {
        ccmd_client_unshade(s_target_client);
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

    s_resize_corner_grab(s_target_client, s_surface,
            &corner_pos.x, &corner_pos.y);

    xcb_warp_pointer(connection, XCB_NONE, root_win,
            0, 0, 0, 0,
            (int16_t) corner_pos.x, (int16_t) corner_pos.y);

    screen_dim.w = s_surface->properties.dim.w;
    screen_dim.h = s_surface->properties.dim.h;
    drag_start(connection, root_win, s_target_client, s_desktop,
            CLIENT_OPERATION_RESIZING,
            XCB_CURRENT_TIME,
            corner_pos, screen_dim);
}


/**
 * @brief Callback: send the client action encoded in @p userdata
 *
 * Shared by every entry below whose activation is nothing more than
 * "send this one @c action_client_e to the target client": iconify,
 * hide, maximize, fullscreen, shade, and close.  The action itself
 * travels through @p userdata (see @c s_entry_command's callers
 * for each, cast through @c intptr_t the same way any small integer
 * value is conventionally threaded through a @c void* callback
 * parameter), rather than each action needing its near-identical
 * one-line wrapper.  @c s_cb_decorate stays separate below since it
 * has an extra unshade step first, not just a different action
 * constant.
 *
 * @param connection Unused; kept for the callback's required signature
 * @param userdata   The @c enum @c action_client_e to send, cast to
 *                   @c void*
 */
static void s_cb_send_action(xcb_connection_t *connection, void *userdata)
{
    enum action_client_e action = (enum action_client_e) (intptr_t) userdata;

    (void) connection;

    if (s_target_client == NULL) {
        return;
    }

    switch (action) {
        case ACTION_CLIENT_CLOSE:
            enact_client_close(s_target_client);
            break;
        case ACTION_CLIENT_HIDE:
            enact_client_hide(s_target_client);
            break;
        case ACTION_CLIENT_ICONIFY:
            enact_client_iconify(s_target_client);
            break;
        case ACTION_CLIENT_LAYER_ABOVE:
            enact_client_layer_above(s_target_client);
            break;
        case ACTION_CLIENT_LAYER_BELOW:
            enact_client_layer_below(s_target_client);
            break;
        case ACTION_CLIENT_LAYER_NORMAL:
            enact_client_layer_normal(s_target_client);
            break;
        case ACTION_CLIENT_MAXIMIZE:
            enact_client_maximize(s_target_client);
            break;
        case ACTION_CLIENT_RESTORE:
            enact_client_restore(s_target_client);
            break;
        case ACTION_CLIENT_TOGGLE_DECORATION:
            enact_client_toggle_decorate(s_target_client);
            break;
        case ACTION_CLIENT_TOGGLE_FULLSCREEN:
            enact_client_toggle_fullscreen(s_target_client);
            break;
        case ACTION_CLIENT_TOGGLE_SHADE:
            enact_client_toggle_shade(s_target_client);
            break;
        case ACTION_CLIENT_TOGGLE_PIN:
            enact_client_toggle_pin(s_target_client);
            break;
        case ACTION_CLIENT_KILL:
        case ACTION_CLIENT_FOCUS:
        case ACTION_CLIENT_UNFOCUS:
        case ACTION_CLIENT_RESIZE:
        case ACTION_CLIENT_MOVE:
        case ACTION_CLIENT_CENTER:
        case ACTION_CLIENT_MOVE_MONITOR_NORTH:
        case ACTION_CLIENT_MOVE_MONITOR_SOUTH:
        case ACTION_CLIENT_MOVE_MONITOR_EAST:
        case ACTION_CLIENT_MOVE_MONITOR_WEST:
        case ACTION_CLIENT_MOVE_TO_MONITOR:
        case ACTION_CLIENT_RECLASS:
        case ACTION_CLIENT_REROLE:
        case ACTION_CLIENT_RENAME:
        case ACTION_CLIENT_MAXIMIZE_HORZ:
        case ACTION_CLIENT_MAXIMIZE_VERT:
        case ACTION_CLIENT_UNHIDE:
        case ACTION_CLIENT_SHADE:
        case ACTION_CLIENT_UNSHADE:
        case ACTION_CLIENT_PIN:
        case ACTION_CLIENT_UNPIN:
        case ACTION_CLIENT_FULLSCREEN:
        case ACTION_CLIENT_UNFULLSCREEN:
        case ACTION_CLIENT_RAISE:
        case ACTION_CLIENT_LOWER:
        case ACTION_CLIENT_CYCLE_LAYER:
        case ACTION_CLIENT_SET_URGENT:
        case ACTION_CLIENT_CLEAR_URGENT:
        case ACTION_CLIENT_SET_ICON:
            LOGGER_WARNING("Unexpected action %d routed through" \
                    " 's_cb_send_action'", action);
            break;
    }
}


/**
 * @brief Open the inspector for the client this menu was raised over
 *
 * @param connection XCB connection
 * @param userdata   Unused
 *
 * @note Complexity: @e O(t), where @e t is the client's number of
 *       transient children, which the inspector counts
 */
static void s_cb_inspect(xcb_connection_t *connection, void *userdata)
{
    (void) userdata;

    if (s_target_client != NULL) {
        dialog_inspect_show(connection, s_surface, s_config,
                s_target_client);
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
            ccmd_client_unshade(s_target_client);
        }
        enact_client_toggle_decorate(s_target_client);
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
 * @brief What @a s_desktop_entry_visit is building
 */
struct s_desk_entry_ctx_s {
    client_td *client;              /**< Client the entries send */
    desktop_td *current;            /**< Desktop it is on already */
    const surface_td *surface;      /**< Surface being offered */
    uint32_t count;                 /**< Entries built so far */
    uint32_t index;                 /**< Desktop index reached */
};


/**
 * @brief Build one desktop's "send there" entry
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_desk_entry_ctx_s being built
 *
 * @note Stops building once the menu is full, the whole walk still
 *       running: a visitor has no way to end one
 * @note Complexity: @e O(1)
 */
static void s_desktop_entry_visit(desktop_td *desktop, void *data)
{
    struct s_desk_entry_ctx_s *const ctx = data;
    char label[WM_DESKTOP_MAX_LENGTH_NAME + 64];
    uint32_t n;

    if (ctx == NULL || ctx->count >= WINCMENU_MAX_DESKTOPS) {
        return;
    }

    n = ctx->count;
    surface_desktop_label(ctx->surface, ctx->index, desktop->name,
            false, true, label, sizeof(label));
    (void) snprintf(s_desk_entries[n].label,
            sizeof(s_desk_entries[n].label), "%s%s%s",
            MENU_CONTEXT_CTXMENU_LABEL_PREFIX, label,
            MENU_CONTEXT_CTXMENU_LABEL_SUFFIX);

    s_desk_entries[n].type = CTXMENU_COMMAND;
    s_desk_entries[n].is_disabled = (desktop->id == ctx->current->id);
    s_send_data[n].client = ctx->client;
    s_send_data[n].src = ctx->current;
    s_send_data[n].dst = desktop;
    s_desk_entries[n].on_activate = s_cb_send_to_desktop;
    s_desk_entries[n].userdata = &s_send_data[n];
    ctx->count++;
    ctx->index++;
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
 *       (a single walk of the surface's circular desktop list,
 *       not one lookup per index)
 */
static int s_build_desk_entries(surface_td *surface,
        desktop_td *desktop, client_td *client)
{
    struct s_desk_entry_ctx_s desk_ctx;
    int n;
    bool is_sticky;

    is_sticky = (client->properties.flags & CLIENT_FLAG_PIN) != 0u;

    desk_ctx.client = client;
    desk_ctx.current = desktop;
    desk_ctx.surface = surface;
    desk_ctx.count = 0u;
    desk_ctx.index = 0u;
    surface_desktops_walk(surface, s_desktop_entry_visit, &desk_ctx);
    n = (int) desk_ctx.count;

    /* Separates the numbered-desktop entries above from the pin/unpin
     * one below, only when there actually are any.  With none (an
     * empty or single-surface edge case), a bare separator would lead
     * nowhere. */
    if (n > 0) {
        s_desk_entries[n].type = CTXMENU_SEPARATOR;
        ++n;
    }

    /* "All desktops" entry for sticky support.  When the client is
     * already sticky, relabel it as an active un-pin action instead of
     * disabling it, since toggling stickiness on this entry already
     * works both ways and there is otherwise no menu entry to remove
     * a pin once set */
    if (is_sticky) {
        safe_strncpy(s_desk_entries[n].label,
                _(STR_WINCMENU_THIS_DESKTOP_UNPIN),
                sizeof(s_desk_entries[n].label) - 1u);
    } else {
        safe_strncpy(s_desk_entries[n].label,
                _(STR_WINCMENU_ALL_DESKTOPS_PIN),
                sizeof(s_desk_entries[n].label) - 1u);
    }

    /* 'All desktops' entry for sticky support */
    s_desk_entries[n].type = CTXMENU_COMMAND;
    s_desk_entries[n].is_disabled = false/*is_sticky*/;
    s_desk_entries[n].on_activate = s_cb_send_action;
    s_desk_entries[n].userdata =
        (void *) (intptr_t) ACTION_CLIENT_TOGGLE_PIN;
    ++n;

    return n;
}


/**
 * @brief Build the "Send to monitor" submenu entries
 *
 * @param surface Surface that owns the monitors
 * @param client  Target client
 *
 * @return Number of entries filled in @a s_monitor_entries
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors
 */
static int s_build_monitor_entries(surface_td *surface, client_td *client)
{
    int n = 0;
    monitor_td cur_monitor;
    struct position_s center_pos;

    center_pos.x = client->layout.geometry.cur.pos.x +
        (int32_t) (client->layout.geometry.cur.dim.w / 2u);
    center_pos.y = client->layout.geometry.cur.pos.y +
        (int32_t) (client->layout.geometry.cur.dim.h / 2u);
    cur_monitor = surface_monitor_for_point(surface, center_pos);

    for (uint32_t m_idx = 0; m_idx < surface->monitor_count &&
            n < WINCMENU_MAX_MONITORS; ++m_idx) {
        bool is_cur;
        const monitor_td *m = &surface->monitors[m_idx];

        is_cur = (m->x == cur_monitor.x &&
                m->y == cur_monitor.y);

        (void) snprintf(s_monitor_entries[n].label,
                sizeof(s_monitor_entries[n].label),
                "%s[%u] %ux%u @ %d,%d%s%s",
                MENU_CONTEXT_CTXMENU_LABEL_PREFIX,
                m_idx, m->w, m->h, m->x, m->y,
                (m_idx == surface->primary_monitor_index)
                    ? " (primary)" : "",
                MENU_CONTEXT_CTXMENU_LABEL_SUFFIX);

        s_monitor_entries[n].type = CTXMENU_COMMAND;
        s_monitor_entries[n].is_disabled = is_cur;
        s_send_monitor_data[n].client = client;
        s_send_monitor_data[n].monitor_index = m_idx;
        s_monitor_entries[n].on_activate = s_cb_send_to_monitor;
        s_monitor_entries[n].userdata = &s_send_monitor_data[n];
        ++n;
    }

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

    is_above = (client->properties.layer ==
            (uint16_t) CLIENT_LAYER_ABOVE);
    is_normal = (client->properties.layer ==
            (uint16_t) CLIENT_LAYER_NORMAL);
    is_below = (client->properties.layer ==
            (uint16_t) CLIENT_LAYER_BELOW);

    s_entry_command(&s_layer_entries[0], _(STR_WINCMENU_LAYER_ALWAYS_ON_TOP),
            s_cb_send_action,
            (void *) (intptr_t) ACTION_CLIENT_LAYER_ABOVE, is_above);
    s_entry_command(&s_layer_entries[1], _(STR_WINCMENU_LAYER_NORMAL),
            s_cb_send_action,
            (void *) (intptr_t) ACTION_CLIENT_LAYER_NORMAL, is_normal);
    s_entry_command(&s_layer_entries[2],
            _(STR_WINCMENU_LAYER_ALWAYS_ON_BOTTOM),
            s_cb_send_action,
            (void *) (intptr_t) ACTION_CLIENT_LAYER_BELOW, is_below);
}


/* Open the window context menu for a client */
void wincmenu_show(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop, client_td *client,
        struct position_s pos, const config_td *config)
{
    int n;
    int desk_count;
    int monitor_count;
    bool can_restore;
    bool can_move;
    bool can_resize;
    bool can_shade;

    if (connection == NULL || surface == NULL || desktop == NULL ||
            client == NULL || config == NULL || client_is_locked(client)) {
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
     * windows cannot be moved or resized at all.  Restore, on the other
     * hand, is available from any maximized state (full, horizontal, or
     * vertical) as well as fullscreen, since 'ccmd_client_restore'
     * already handles all of them. */
    can_restore = client_is_maximized_any(client)
        || client_is_fullscreen(client);
    can_move = !client_is_maximized(client)
        && !client_is_fullscreen(client)
        && !client_is_locked(client);
    can_resize = client_is_resizable(client)
        && !client_is_maximized(client)
        && !client_is_fullscreen(client)
        && !client_is_locked(client);
    can_shade = (client->properties.flags &
            CLIENT_FLAG_DECORATED) != 0u
        && !client_is_fullscreen(client);

    /* Build 'Send to desktop' submenu, only meaningful (and only
     * shown at all, see below) on a surface with more than one
     * desktop; this is also where the "all desktops" sticky toggle
     * lives, so hiding the whole submenu on a single-desktop surface
     * (as in restricted-memory mode; see 'memguard.h') correctly
     * hides that too, since sticking to every desktop means nothing
     * when there is only the one. */
    desk_count = 0;
    if (surface->desktop_count > 1u) {
        memset(s_desk_entries, 0, sizeof(s_desk_entries));
        desk_count = s_build_desk_entries(surface, desktop, client);

        memset(&s_desk_state, 0, sizeof(s_desk_state));
        s_desk_state.window = XCB_WINDOW_NONE;
        s_desk_state.entries = s_desk_entries;
        s_desk_state.entry_count = desk_count;
    }

    /* Build "Send to monitor" submenu, only meaningful (and only
     * shown at all, see below) on a surface with more than one
     * monitor */
    monitor_count = 0;
    if (surface->monitor_count > 1u) {
        memset(s_monitor_entries, 0, sizeof(s_monitor_entries));
        monitor_count = s_build_monitor_entries(surface, client);

        memset(&s_monitor_state, 0, sizeof(s_monitor_state));
        s_monitor_state.window = XCB_WINDOW_NONE;
        s_monitor_state.entries = s_monitor_entries;
        s_monitor_state.entry_count = monitor_count;
    }

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

    /* Send to desktop (submenu); omitted entirely, not just disabled,
     * on a surface with only one desktop */
    if (desk_count > 0) {
        s_entries[n].type = CTXMENU_SUBMENU;
        safe_strncpy(s_entries[n].label, _(STR_WINCMENU_SEND_TO_DESKTOP),
                sizeof(s_entries[n].label) - 1u);
        s_entries[n].items = s_desk_entries;
        s_entries[n].item_count = desk_count;
        s_entries[n].userdata = &s_desk_state;
        ++n;
    }

    /* Send to monitor (submenu); omitted entirely, not just disabled,
     * on a surface with only one monitor */
    if (monitor_count > 0) {
        s_entries[n].type = CTXMENU_SUBMENU;
        safe_strncpy(s_entries[n].label, _(STR_WINCMENU_SEND_TO_MONITOR),
                sizeof(s_entries[n].label) - 1u);
        s_entries[n].items = s_monitor_entries;
        s_entries[n].item_count = monitor_count;
        s_entries[n].userdata = &s_monitor_state;
        ++n;
    }

    /* Layer (submenu): disabled while fullscreen, since a focused
     * fullscreen client's stacking is always forced above
     * everything else regardless of its own real layer (see
     * 'ccmd_desktop_enforce_layers''s comment); choosing a
     * layer here would silently do nothing visible until the client
     * later leaves fullscreen, which reads as broken rather than
     * merely deferred. */
    s_entries[n].type = CTXMENU_SUBMENU;
    safe_strncpy(s_entries[n].label, _(STR_WINCMENU_LAYER),
            sizeof(s_entries[n].label) - 1u);
    s_entries[n].items = s_layer_entries;
    s_entries[n].item_count = WINCMENU_LAYER_COUNT;
    s_entries[n].userdata = &s_layer_state;
    s_entries[n].is_disabled = client_is_fullscreen(client);
    ++n;

    /* Separator */
    s_entries[n].type = CTXMENU_SEPARATOR;
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_RESTORE),
            s_cb_send_action,
            (void *) (intptr_t) ACTION_CLIENT_RESTORE, !can_restore);
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_MOVE),
            s_cb_move, NULL, !can_move);
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_RESIZE),
            s_cb_resize, NULL, !can_resize);
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_ICONIFY),
            s_cb_send_action,
            (void *) (intptr_t) ACTION_CLIENT_ICONIFY, false);
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_HIDE),
            s_cb_send_action,
            (void *) (intptr_t) ACTION_CLIENT_HIDE, false);
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_MAXIMIZE),
            s_cb_send_action,
            (void *) (intptr_t) ACTION_CLIENT_MAXIMIZE,
            !client_is_maximizable(client) ||
                client_is_maximized(client) ||
                client_is_fullscreen(client));
    ++n;

    /* Disabled under the exact same condition as maximize above,
     * unlike a client's EWMH request to enter fullscreen itself
     * (see 'ccmd_client_fullscreen''s comment, cmds/client/
     * state.c, for why that path stays unconditional.  A fixed-size
     * DOS-emulation or retro-game window legitimately requests
     * fullscreen via alt+enter regardless of its own resizable
     * flag).  This is a different question.  Whether the window
     * manager's user-facing fullscreen offer, this very menu
     * entry, matched by every keybinding and decoration button that
     * also call 'ccmd_client_fullscreen' directly, makes any sense
     * to present at all for a client with no legitimate reason to
     * ever cover the whole screen, a fixed-size confirmation dialog
     * ("Are you sure you want to delete this file?") foremost among
     * them: nothing about entering fullscreen from here overrides
     * 'client_is_resizable' the way the client's EWMH request
     * does, so a client that can never resize itself gains nothing
     * from it either way. */
    s_entry_command(&s_entries[n],
            (client_is_fullscreen(client))
                ? _(STR_WINCMENU_UNFULLSCREEN)
                : _(STR_WINCMENU_FULLSCREEN_ENTER),
            s_cb_send_action,
            (void *) (intptr_t) ACTION_CLIENT_TOGGLE_FULLSCREEN,
            (!client_is_resizable(client) || client_is_modal(client)) &&
                !client_is_fullscreen(client));
    ++n;

    s_entry_command(&s_entries[n],
            (client_is_shaded(client)) ? _(STR_WINCMENU_UNSHADE)
                : _(STR_WINCMENU_SHADE),
            s_cb_send_action,
            (void *) (intptr_t) ACTION_CLIENT_TOGGLE_SHADE, !can_shade);
    ++n;

    s_entry_command(&s_entries[n],
            (client_is_decorated(client)) ? _(STR_WINCMENU_UNDECORATE)
                : _(STR_WINCMENU_DECORATE),
            s_cb_decorate, NULL, client_is_fullscreen(client));
    ++n;

    /* Separator before the two entries that end the menu */
    s_entries[n].type = CTXMENU_SEPARATOR;
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_INSPECT),
            s_cb_inspect, NULL, false);
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_CLOSE),
            s_cb_send_action,
            (void *) (intptr_t) ACTION_CLIENT_CLOSE, false);
    ++n;

    memset(&s_root, 0, sizeof(s_root));
    s_root.window = XCB_WINDOW_NONE;
    s_root.entries = s_entries;
    s_root.entry_count = n;

    ctxmenu_show(connection, surface, &s_root, pos, config);
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
    ctxmenu_tree_redraw_window(&s_root, win);
}


/* Handle a button-press event inside the window context menu */
bool wincmenu_handle_click(xcb_connection_t *connection,
        surface_td *surface, xcb_window_t win, int y,
        const config_td *config)
{
    return ctxmenu_tree_handle_click_window(connection, surface, &s_root,
            win, y, config);
}


/* Query whether the window context menu is currently open */
bool wincmenu_is_open(void)
{
    return ctxmenu_is_open(&s_root);
}


/* Check whether 'win' belongs to the window context menu hierarchy */
bool wincmenu_owns_window(xcb_window_t win)
{
    return ctxmenu_tree_state_find_for_window(&s_root, win) != NULL;
}


/* Handle a key-press event while the window context menu is open */
bool wincmenu_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config)
{
    return ctxmenu_tree_handle_keypress_deepest(connection, surface,
            &s_root, keysym, config);
}


/* Handle a pointer-motion event over the window context menu */
void wincmenu_handle_motion(xcb_window_t win, int x, int y)
{
    ctxmenu_tree_handle_motion_window(&s_root, win, x, y);
}
