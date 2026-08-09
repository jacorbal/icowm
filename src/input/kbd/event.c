/**
 * @file input/kbd/event.c
 *
 * @brief Key-press and key-release event dispatch
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <signal.h>     /* SIGTERM */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Render includes */
#include <render/surface.h>

/* Utils includes */
#include <utils/geom.h>

/* Windows & icons policy includes */
#include <policy/focus.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/cycle.h>
#include <menu/dialog.h>
#include <menu/dialog/info.h>
#include <menu/dialog/quit.h>
#include <menu/dialog/shortcuts.h>
#include <menu/popup.h>

/* Handler includes */
#include <handler/internal.h>

/* Command includes */
#include <cmds/ccmd.h>
#include <cmds/scmd.h>

/* Default initial values */

/* Project includes */
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <event.h>
#include <eventq.h>
#include <lifecycle.h>
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <input/kbd/bind.h>
#include <input/kbd/event.h>
#include <input/kbd/modal.h>


/* Surface lookup */

/**
 * @brief Look up a surface associated to a root window, with fallback
 *
 * Attempts to find a @c surface_td that corresponds to the given X11
 * @c root window by searching the @c surfaces list.  If no matching
 * surface is found, but the list is non-empty, this function falls back
 * to returning the first surface in the list.
 *
 * @param surfaces List of available surfaces to search in, or @c NULL
 * @param root     X11 root window identifier used as lookup key
 *
 * @return Pointer to the matching @c surface_td, or the first surface
 *         in the list if no match is found; returns @c NULL if
 *         @p surfaces is null or empty.
 *
 * @note Intended for use when a specific root surface may not exist
 *       yet, providing a reasonable default for callers.
 */
static surface_td *s_lookup_surface_fallback(list_td *surfaces,
        xcb_window_t root)
{
    surface_td *surface;

    surface = lookup_surface_for_root(surfaces, root);
    if (surface == NULL && surfaces != NULL &&
            !list_is_empty(surfaces)) {
        surface = (surface_td *) list_data(list_head(surfaces));
    }

    return surface;
}


/* Active-client lookup helper */

/**
 * @brief Resolve the currently focused client on a surface
 *
 * Looks up the current desktop for @p surface and returns the active
 * client on that desktop.  Optionally returns the owning surface and
 * desktop pointers through @p cs_out and @p cd_out.
 *
 * @param surface  Surface to query
 * @param surfaces Full surface list (for @c lookup_find_client)
 * @param cs_out   Receives the client's owning surface (may be null)
 * @param cd_out   Receives the client's owning desktop (may be null)
 *
 * @return Active client, or @c NULL when none is focused
 *
 * @note Complexity: @e O(n) for the client list walk
 */
static client_td *s_get_active_client(surface_td *surface,
        list_td *surfaces,
        surface_td **cs_out,
        desktop_td **cd_out)
{
    desktop_td *desktop;

    if (cs_out != NULL) { *cs_out = NULL; }
    if (cd_out != NULL) { *cd_out = NULL; }

    if (surface == NULL) {
        return NULL;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL || desktop->client_active_id == 0) {
        return NULL;
    }

    return lookup_find_client(surfaces, desktop->client_active_id,
            cs_out, cd_out);
}


/* Keyboard resize helpers */

/**
 * @brief Compute a keyboard resize target for one axis
 *
 * Calculates the next frame size for either the horizontal or vertical
 * axis when resizing a @c client_td via keyboard, taking into account
 * frame extents and window manager size hints such as base size,
 * minimum size and resize increment.  When valid size hints are
 * present, the inner size is snapped to the nearest increment starting
 * from the base (or minimum) size; otherwise a fixed keyboard resize
 * step is applied.
 *
 * @param client     Pointer to the client whose geometry is being
 *                   resized; may be null, in which case @p cur_frame is
 *                   returned
 * @param step       Amount of pixels to resize every step
 * @param horizontal @c true to operate on the horizontal axis (width),
 *                   @c false for the vertical axis (height)
 * @param cur_frame  Current outer frame size (including extents) for
 *                   the selected axis
 * @param grow       @c true to grow (increase) the size, @c false to
 *                   shrink (decrease) it
 *
 * @return The target outer frame size for the selected axis after
 *         applying keyboard resize semantics and clamping via
 *         @c geom_clamp_dim.
 *
 * @note With this, it's honored @c WM_NORMAL_HINTS increments when
 *       available, ensuring that keyboard resizing respects the
 *       client's preferred resize granularity.
 */
static uint32_t s_kb_resize_axis_target(const client_td *client,
        uint32_t step, bool horizontal, uint32_t cur_frame, bool grow)
{
    uint32_t ext_a;
    uint32_t ext_b;
    uint32_t cur_inner;
    int32_t base_i;
    int32_t min_i;
    int32_t inc_i;
    int32_t target;

    if (client == NULL) {
        return cur_frame;
    }

    if (horizontal) {
        ext_a = (uint32_t) client->layout.frame_extents.left;
        ext_b = (uint32_t) client->layout.frame_extents.right;
    } else {
        ext_a = (uint32_t) client->layout.frame_extents.top;
        ext_b = (uint32_t) client->layout.frame_extents.bottom;
    }

    cur_inner = (cur_frame > ext_a + ext_b)
        ? cur_frame - ext_a - ext_b : 0u;
    if (!client->size_hints.valid) {
        int32_t resize_step = (step > 0u) ? (int32_t) step : 1;
        target = grow
            ? (int32_t) cur_frame + resize_step
            : (int32_t) cur_frame - resize_step;
        return geom_clamp_dim(target);
    }

    if (horizontal) {
        base_i = client->size_hints.base_w;
        min_i = client->size_hints.min_w;
        inc_i = client->size_hints.inc_w;
    } else {
        base_i = client->size_hints.base_h;
        min_i = client->size_hints.min_h;
        inc_i = client->size_hints.inc_h;
    }

    if (inc_i > 1) {
        uint32_t base = (base_i > 0)
            ? (uint32_t) base_i
            : ((min_i > 0) ? (uint32_t) min_i : 0u);
        uint32_t inc = (uint32_t) inc_i;
        uint32_t over;
        uint32_t snapped;
        uint32_t target_inner;

        if (cur_inner < base) {
            cur_inner = base;
        }

        over = (cur_inner > base) ? (cur_inner - base) : 0u;
        snapped = base + (over / inc) * inc;

        if (grow) {
            target_inner = snapped + inc;
        } else {
            target_inner = (snapped > base) ? (snapped - inc) : base;
        }

        return geom_clamp_dim((int32_t) (target_inner + ext_a + ext_b));
    }

    target = (grow)
        ? (int32_t) cur_frame + (int32_t) ((step > 0u) ? step : 1u)
        : (int32_t) cur_frame - (int32_t) ((step > 0u) ? step : 1u);

    return geom_clamp_dim(target);
}


/**
 * @brief Directly apply a keyboard resize to a client
 *
 * Applies the resize synchronously without going through the event
 * queue.  This mirrors the interactive (mouse-drag) resize path so that
 * both input methods share identical behavior: the geometry is
 * constrained per-axis, applied to the correct X window (frame for
 * decorated clients, content window for undecorated clients), and
 * followed by a synthetic @c ConfigureNotify so the application learns
 * its new geometry immediately.
 *
 * @param client Pointer to the client to resize
 * @param new_x  New frame X position (screen-relative)
 * @param new_y  New frame Y position (screen-relative)
 * @param new_w  New frame width
 * @param new_h  New frame height
 *
 * @note This function flushes the XCB connection before returning.
 */
static void s_kbd_resize_apply(client_td *client,
        int32_t new_x, int32_t new_y, uint32_t new_w, uint32_t new_h)
{
    bool pos_changed;
    uint16_t mask;
    uint32_t values[4];
    xcb_window_t target_win;

    if (client == NULL) {
        return;
    }

    /* A shaded client shows only the titlebar; restore the full window
     * before applying the new dimensions */
    if (client_is_shaded(client)) {
        wcmd_client_unshade(client);
    }

    /* The caller ('s_handle_kbd_resize' via 's_kb_resize_axis_target')
     * already produced fully snapped, increment-aligned frame
     * dimensions.  Re-applying 'client_constrain_size' here would snap
     * the values a second time and could produce a size different from
     * what the position correction ('new_y += old_h - new_h') was
     * computed for, causing the top edge of the window to shift by the
     * wrong amount on 'RESIZE_UP'. */
    pos_changed = (new_x != client->layout.geometry.cur.pos.x ||
                   new_y != client->layout.geometry.cur.pos.y);

    /* Apply the new geometry to the correct X window.  Decorated
     * clients are reparented into a frame; undecorated clients are
     * direct children of the root. */
    target_win = (client->frame != 0 && client_is_decorated(client))
        ? client->frame : client->window;

    if (pos_changed) {
        mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
               XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        values[0] = (uint32_t) new_x;
        values[1] = (uint32_t) new_y;
        values[2] = new_w;
        values[3] = new_h;
    } else {
        mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        values[0] = new_w;
        values[1] = new_h;
    }
    xcb_configure_window(client->connection, target_win, mask, values);

    /* Update the stored geometry after configuring X so that
     * 'client_sync_decoration_layout' and the synthetic
     * 'ConfigureNotify' both see the final values */
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
    client->layout.geometry.cur.dim.w = new_w;
    client->layout.geometry.cur.dim.h = new_h;

    /* Reposition and resize the inner window and titlebar to match the
     * new frame dimensions (no-op for undecorated clients) */
    client_sync_decoration_layout(client);

    /* ICCCM §4.2.3: send a synthetic 'ConfigureNotify' with
     * screen-relative coordinates so the application always knows its
     * true on-screen position and content-area size, regardless of
     * reparenting. */
    client_send_synthetic_configure_notify(client->connection, client);

    /* Force a repaint AFTER the synthetic 'ConfigureNotify' so the
     * application draws at the correct screen-relative geometry.
     * Placing the 'Expose' here ensures it arrives in the client's
     * event queue after both the xcb_configure_window (from
     * 'client_sync_decoration_layout') and the synthetic
     * 'ConfigureNotify', giving programs that rely on size and position
     * before their 'Expose' handler runs the correct geometry. */
    xcb_clear_area(client->connection, 1, client->window, 0, 0, 0, 0);

    xcb_flush(client->connection);

    /* Mark the desktop as needing a repaint so frame decorations are
     * refreshed at the correct new dimensions */
    wm_request_client_redraw(client);
}


/* Cycle-menu key handling */

/**
 * @brief Handle a key press while the window-cycle menu is open
 *
 * Navigates the cycle menu with arrow keys, confirms with Enter,
 * cancels with @c Escape, and navigates with the configured
 * cycle-next/prev bindings.  Any other key closes the menu without
 * activating a client.
 *
 * @param keysym   Keysym of the pressed key
 * @param state    Stripped modifier state (lock modifiers removed)
 * @param surface  Surface for drawing and confirming (may be null)
 * @param surfaces Full surface list passed to @c cycle_confirm
 * @param config   Active configuration
 */
static void s_handle_cycle_key(xcb_keysym_t keysym, uint16_t state,
        surface_td *surface, list_td *surfaces,
        const config_td *config)
{
    xcb_connection_t *conn = (surface != NULL) ? surface->connection
                                               : NULL;

    /* Up arrow: go to previous entry */
    if (keysym == 0xff52u) {
        cycle_navigate_prev();
        if (conn != NULL) { cycle_draw(conn, config); }
        return;
    }

    /* Down arrow: go to next entry */
    if (keysym == 0xff54u) {
        cycle_navigate_next();
        if (conn != NULL) { cycle_draw(conn, config); }
        return;
    }

    /* Enter / KP_Enter: confirm selection */
    if (keysym == 0xff0du || keysym == 0xff8du) {
        if (conn != NULL) { cycle_confirm(conn, surfaces, config); }
        return;
    }

    /* Escape: cancel without activating */
    if (keysym == 0xff1bu) {
        if (conn != NULL) { cycle_close(conn); }
        return;
    }

    /* Configured cycle-next binding */
    if (cycle_next_keysym() != XCB_NO_SYMBOL &&
            keysym == cycle_next_keysym() &&
            state == cycle_next_modmask()) {
        cycle_navigate_next();
        if (conn != NULL) { cycle_draw(conn, config); }
        return;
    }

    /* Configured cycle-prev binding */
    if (cycle_prev_keysym() != XCB_NO_SYMBOL &&
            keysym == cycle_prev_keysym() &&
            state == cycle_prev_modmask()) {
        cycle_navigate_prev();
        if (conn != NULL) { cycle_draw(conn, config); }
        return;
    }

    /* Any other key: close the menu without action */
    if (conn != NULL) { cycle_close(conn); }
}


/* Confirmation dialog key handling */

/**
 * @brief Handle a key press while the quit-confirmation dialog is open
 *
 * @c Tab / @c Left / @c Right toggle the selected button; @c Enter
 * activates it; @c Escape cancels the dialog.
 *
 * @param keysym  Keysym of the pressed key
 * @param surface Surface for drawing (may be null)
 * @param config  Active configuration
 */
static void s_handle_dialog_quit_key(xcb_keysym_t keysym,
        surface_td *surface, const config_td *config)
{
    xcb_connection_t *conn = (surface != NULL) ? surface->connection
                                               : NULL;

    /* Tab, Left arrow, Right arrow: toggle selected button */
    if (keysym == 0xff09u || keysym == 0xff51u || keysym == 0xff53u) {
        dialog_quit_toggle_selection();
        if (conn != NULL) { dialog_quit_repaint(conn, config); }
        return;
    }

    /* Enter / KP_Enter / Space: confirm */
    if (keysym == 0xff0du || keysym == 0xff8du || keysym == 0x0020u) {
        if (conn != NULL) { dialog_quit_accept(conn); }
        return;
    }

    /* Escape: dismiss without action */
    if (keysym == 0xff1bu) {
        if (conn != NULL) { dialog_quit_close(conn); }
    }
}


/* Context-menu key handling */

/**
 * @brief Dispatch a key-press event to the currently open context menu
 *
 * Checks each of the three context menus (window menu, root menu,
 * window list) in order and forwards the key event to whichever is
 * currently visible.  Navigation (arrows), activation (@c Enter), and
 * cancellation (@c Escape) are all handled by
 * @c ctxmenu_handle_keypress via the per-menu wrapper.
 *
 * @param keysym     Keysym of the pressed key
 * @param connection XCB connection (for submenu creation)
 * @param surface    Surface on which the menu is displayed
 * @param config     Active configuration
 *
 * @return @c true if a menu was closed, @c false otherwise
 */
static bool s_dispatch_open_menu_key(xcb_keysym_t keysym,
        xcb_connection_t *connection, surface_td *surface,
        const config_td *config)
{
    if (wincmenu_is_open()) {
        wincmenu_handle_keypress(connection, surface, keysym, config);
        return true;
    }

    if (rootmenu_is_open()) {
        rootmenu_handle_keypress(connection, surface, keysym, config);
        return true;
    }

    if (winlist_is_open()) {
        winlist_handle_keypress(connection, surface, keysym, config);
        return true;
    }

    return false;
}


/* Client action dispatch */

/**
 * @brief Dispatch a single-client action triggered by a key binding
 *
 * Resolves the focused client and dispatches the action identified by
 * @p btype.  Each binding type maps to exactly one @c action_client_e
 * value.  Actions that require resize capability (maximize, fullscreen)
 * are silently dropped when the client is not resizable.
 *
 * @param btype    Keyboard binding type (one of the @c KEYBIND_CLIENT_*
 *                 constants)
 * @param surface  Current surface (used to resolve the active client)
 * @param surfaces Full surface list
 * @param bmm      Raw modifier mask of the matched binding (needed by
 *                 the info popup)
 * @param detail   Raw keycode detail from the event (needed by the info
 *                 popup)
 * @param config   Active configuration
 */
static void s_dispatch_client_action(enum wm_keybind_type_e btype,
        surface_td *surface, list_td *surfaces,
        uint16_t bmm, xcb_keycode_t detail,
        const config_td *config)
{
    desktop_td *desktop;
    client_td *client;

    if (surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    client = s_get_active_client(surface, surfaces, NULL, NULL);
    if (client == NULL) {
        return;
    }

    switch (btype) {
        /* To avoid warnings from the compiler, ALL cases must be here */
        case KEYBIND_NONE:
        case KEYBIND_DESKTOP_NEXT:
        case KEYBIND_DESKTOP_PREV:
        case KEYBIND_CLIENT_CYCLE_NEXT:
        case KEYBIND_CLIENT_CYCLE_PREV:
        case KEYBIND_DESKTOP_ICON_NEXT:
        case KEYBIND_DESKTOP_ICON_PREV:
        case KEYBIND_LAUNCH_TERMINAL:
        case KEYBIND_LAUNCH_LAUNCHER:
        case KEYBIND_LAUNCH_FILE_MANAGER:
        case KEYBIND_LAUNCH_WEB_BROWSER:
        case KEYBIND_LAUNCH_EDITOR:
        case KEYBIND_CLIENT_MOVE_LEFT:
        case KEYBIND_CLIENT_MOVE_RIGHT:
        case KEYBIND_CLIENT_MOVE_UP:
        case KEYBIND_CLIENT_MOVE_DOWN:
        case KEYBIND_CLIENT_MOVE_TOP_LEFT:
        case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
        case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
        case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
        case KEYBIND_CLIENT_RESIZE_LEFT:
        case KEYBIND_CLIENT_RESIZE_RIGHT:
        case KEYBIND_CLIENT_RESIZE_UP:
        case KEYBIND_CLIENT_RESIZE_DOWN:
        case KEYBIND_DESKTOP_SHOW:
        case KEYBIND_DESKTOP_GOTO_0:
        case KEYBIND_DESKTOP_GOTO_1:
        case KEYBIND_DESKTOP_GOTO_2:
        case KEYBIND_DESKTOP_GOTO_3:
        case KEYBIND_DESKTOP_GOTO_4:
        case KEYBIND_DESKTOP_GOTO_5:
        case KEYBIND_DESKTOP_GOTO_6:
        case KEYBIND_DESKTOP_GOTO_7:
        case KEYBIND_DESKTOP_GOTO_8:
        case KEYBIND_DESKTOP_GOTO_9:
        case KEYBIND_WM_ROOT_MENU:
        case KEYBIND_WM_WINDOWS_MENU:
        case KEYBIND_CLIENT_WINDOW_MENU:
        case KEYBIND_WM_REDRAW:
        case KEYBIND_WM_RELOAD:
        case KEYBIND_WM_QUIT:
        case KEYBIND_WM_SHORTCUTS_LIST:
        case KEYBIND_WM_EMERGENCY_EXIT:
        case KEYBIND_WM_FORTUNE:
            return;

        case KEYBIND_CLIENT_INFO:
            popup_show(surface->connection, surface, desktop, client,
                    bmm, detail, config);
            return;

        case KEYBIND_CLIENT_ICONIFY:
            client_send_event(client, ACTION_CLIENT_ICONIFY,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_HIDE:
            client_send_event(client, ACTION_CLIENT_HIDE,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_CLOSE:
            client_send_event(client, ACTION_CLIENT_CLOSE,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_KILL:
            client_send_event(client, ACTION_CLIENT_KILL,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_MAXIMIZE:
            if (!client_is_resizable(client)) { return; }
            client_send_event(client, ACTION_CLIENT_MAXIMIZE,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_CENTER:
            client_send_event(client, ACTION_CLIENT_CENTER,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_MOVE_NEXT_MONITOR:
            client_send_event(client, ACTION_CLIENT_MOVE_NEXT_MONITOR,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_SHADE:
            client_send_event(client, ACTION_CLIENT_TOGGLE_SHADE,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_FULLSCREEN:
            /* No 'client_is_resizable' gate, unlike maximize above;
             * see 'wcmd_client_fullscreen''s own comment for why
             * fullscreen is deliberately exempt from it. */
            client_send_event(client, ACTION_CLIENT_TOGGLE_FULLSCREEN,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_PIN:
            client_send_event(client, ACTION_CLIENT_TOGGLE_STICKY,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_TOGGLE_DECORATION:
            /* Unshade first: toggling decoration while shaded would
             * leave the window in an inconsistent visual state */
            if (client_is_shaded(client)) {
                wcmd_client_unshade(client);
            }
            client_send_event(client, ACTION_CLIENT_TOGGLE_DECORATION,
                    PRIORITY_NORMAL);
            return;

        case KEYBIND_CLIENT_CYCLE_LAYER:
            client_send_event(client, ACTION_CLIENT_CYCLE_LAYER,
                    PRIORITY_NORMAL);
            return;
    }
}


/* Program launch dispatch */

/**
 * @brief Launch a configured program for the given binding type
 *
 * Maps each @c KEYBIND_LAUNCH_* constant to its program string from the
 * configuration and calls @c lifecycle_dispatch_launch.
 *
 * @param btype   Keyboard binding type (one of the @c KEYBIND_LAUNCH_*
 *                constants)
 * @param surface Current surface passed to @c lifecycle_dispatch_launch
 * @param config  Active configuration holding the program paths
 */
static void s_handle_kbd_launch(enum wm_keybind_type_e btype,
        surface_td *surface, const config_td *config)
{
    const char *program = NULL;

    switch (btype) {
        /* To avoid warnings from the compiler, ALL cases must be here */
        case KEYBIND_NONE:
        case KEYBIND_DESKTOP_NEXT:
        case KEYBIND_DESKTOP_PREV:
        case KEYBIND_CLIENT_ICONIFY:
        case KEYBIND_CLIENT_HIDE:
        case KEYBIND_CLIENT_CLOSE:
        case KEYBIND_CLIENT_KILL:
        case KEYBIND_CLIENT_MAXIMIZE:
        case KEYBIND_CLIENT_CENTER:
        case KEYBIND_CLIENT_MOVE_NEXT_MONITOR:
        case KEYBIND_CLIENT_SHADE:
        case KEYBIND_CLIENT_FULLSCREEN:
        case KEYBIND_CLIENT_PIN:
        case KEYBIND_CLIENT_INFO:
        case KEYBIND_CLIENT_TOGGLE_DECORATION:
        case KEYBIND_CLIENT_CYCLE_LAYER:
        case KEYBIND_CLIENT_CYCLE_NEXT:
        case KEYBIND_CLIENT_CYCLE_PREV:
        case KEYBIND_DESKTOP_ICON_NEXT:
        case KEYBIND_DESKTOP_ICON_PREV:
        case KEYBIND_CLIENT_MOVE_LEFT:
        case KEYBIND_CLIENT_MOVE_RIGHT:
        case KEYBIND_CLIENT_MOVE_UP:
        case KEYBIND_CLIENT_MOVE_DOWN:
        case KEYBIND_CLIENT_MOVE_TOP_LEFT:
        case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
        case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
        case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
        case KEYBIND_CLIENT_RESIZE_LEFT:
        case KEYBIND_CLIENT_RESIZE_RIGHT:
        case KEYBIND_CLIENT_RESIZE_UP:
        case KEYBIND_CLIENT_RESIZE_DOWN:
        case KEYBIND_DESKTOP_SHOW:
        case KEYBIND_DESKTOP_GOTO_0:
        case KEYBIND_DESKTOP_GOTO_1:
        case KEYBIND_DESKTOP_GOTO_2:
        case KEYBIND_DESKTOP_GOTO_3:
        case KEYBIND_DESKTOP_GOTO_4:
        case KEYBIND_DESKTOP_GOTO_5:
        case KEYBIND_DESKTOP_GOTO_6:
        case KEYBIND_DESKTOP_GOTO_7:
        case KEYBIND_DESKTOP_GOTO_8:
        case KEYBIND_DESKTOP_GOTO_9:
        case KEYBIND_WM_ROOT_MENU:
        case KEYBIND_WM_WINDOWS_MENU:
        case KEYBIND_CLIENT_WINDOW_MENU:
        case KEYBIND_WM_REDRAW:
        case KEYBIND_WM_RELOAD:
        case KEYBIND_WM_QUIT:
        case KEYBIND_WM_SHORTCUTS_LIST:
        case KEYBIND_WM_EMERGENCY_EXIT:
        case KEYBIND_WM_FORTUNE:
            return;

        case KEYBIND_LAUNCH_TERMINAL:
            program = config->base.programs.terminal;
            break;
        case KEYBIND_LAUNCH_LAUNCHER:
            program = config->base.programs.launcher;
            break;
        case KEYBIND_LAUNCH_FILE_MANAGER:
            program = config->base.programs.file_manager;
            break;
        case KEYBIND_LAUNCH_WEB_BROWSER:
            program = config->base.programs.web_browser;
            break;
        case KEYBIND_LAUNCH_EDITOR:
            program = config->base.programs.editor;
            break;
    }

    lifecycle_dispatch_launch(surface, program, NULL);
}


/* Keyboard move dispatch */

/**
 * @brief Move the focused client by keyboard
 *
 * Resolves the active client and computes a new position based on
 * @p btype: relative steps (@c KEYBIND_CLIENT_MOVE_LEFT / @c RIGHT /
 * @c UP / @c DOWN) or absolute corner snaps
 * (@c KEYBIND_CLIENT_MOVE_TOP_LEFT...).
 *
 * @param btype    Keyboard binding type (one of the
 *                 @c KEYBIND_CLIENT_MOVE_*
 *                 constants)
 * @param surface  Current surface
 * @param surfaces Full surface list
 * @param config   Active configuration (for the move step size)
 */
static void s_handle_kbd_move(enum wm_keybind_type_e btype,
        surface_td *surface, list_td *surfaces,
        const config_td *config)
{
    surface_td *cs = NULL;
    client_td *client;
    int32_t move_step;
    int32_t new_x;
    int32_t new_y;
    int32_t max_x;
    int32_t max_y;

    client = s_get_active_client(surface, surfaces, &cs, NULL);
    if (client == NULL) {
        return;
    }

    move_step = (int32_t) ((config->base.windows.move_step > 0u)
            ? config->base.windows.move_step : 1u);
    new_x = client->layout.geometry.cur.pos.x;
    new_y = client->layout.geometry.cur.pos.y;
    max_x = (cs != NULL)
        ? (int32_t) cs->properties.dim.w -
          (int32_t) client->layout.geometry.cur.dim.w
        : new_x;
    max_y = (cs != NULL)
        ? (int32_t) cs->properties.dim.h -
          (int32_t) client->layout.geometry.cur.dim.h
        : new_y;

    switch (btype) {
        /* To avoid warnings from the compiler, ALL cases must be here */
        case KEYBIND_NONE:
        case KEYBIND_DESKTOP_NEXT:
        case KEYBIND_DESKTOP_PREV:
        case KEYBIND_CLIENT_ICONIFY:
        case KEYBIND_CLIENT_HIDE:
        case KEYBIND_CLIENT_CLOSE:
        case KEYBIND_CLIENT_KILL:
        case KEYBIND_CLIENT_MAXIMIZE:
        case KEYBIND_CLIENT_CENTER:
        case KEYBIND_CLIENT_MOVE_NEXT_MONITOR:
        case KEYBIND_CLIENT_SHADE:
        case KEYBIND_CLIENT_FULLSCREEN:
        case KEYBIND_CLIENT_PIN:
        case KEYBIND_CLIENT_INFO:
        case KEYBIND_CLIENT_TOGGLE_DECORATION:
        case KEYBIND_CLIENT_CYCLE_LAYER:
        case KEYBIND_CLIENT_CYCLE_NEXT:
        case KEYBIND_CLIENT_CYCLE_PREV:
        case KEYBIND_DESKTOP_ICON_NEXT:
        case KEYBIND_DESKTOP_ICON_PREV:
        case KEYBIND_LAUNCH_TERMINAL:
        case KEYBIND_LAUNCH_LAUNCHER:
        case KEYBIND_LAUNCH_FILE_MANAGER:
        case KEYBIND_LAUNCH_WEB_BROWSER:
        case KEYBIND_LAUNCH_EDITOR:
        case KEYBIND_CLIENT_RESIZE_LEFT:
        case KEYBIND_CLIENT_RESIZE_RIGHT:
        case KEYBIND_CLIENT_RESIZE_UP:
        case KEYBIND_CLIENT_RESIZE_DOWN:
        case KEYBIND_DESKTOP_SHOW:
        case KEYBIND_DESKTOP_GOTO_0:
        case KEYBIND_DESKTOP_GOTO_1:
        case KEYBIND_DESKTOP_GOTO_2:
        case KEYBIND_DESKTOP_GOTO_3:
        case KEYBIND_DESKTOP_GOTO_4:
        case KEYBIND_DESKTOP_GOTO_5:
        case KEYBIND_DESKTOP_GOTO_6:
        case KEYBIND_DESKTOP_GOTO_7:
        case KEYBIND_DESKTOP_GOTO_8:
        case KEYBIND_DESKTOP_GOTO_9:
        case KEYBIND_WM_ROOT_MENU:
        case KEYBIND_WM_WINDOWS_MENU:
        case KEYBIND_CLIENT_WINDOW_MENU:
        case KEYBIND_WM_REDRAW:
        case KEYBIND_WM_RELOAD:
        case KEYBIND_WM_QUIT:
        case KEYBIND_WM_SHORTCUTS_LIST:
        case KEYBIND_WM_EMERGENCY_EXIT:
        case KEYBIND_WM_FORTUNE:
            return;

        case KEYBIND_CLIENT_MOVE_LEFT:
            new_x -= move_step;
            break;
        case KEYBIND_CLIENT_MOVE_RIGHT:
            new_x += move_step;
            break;
        case KEYBIND_CLIENT_MOVE_UP:
            new_y -= move_step;
            break;
        case KEYBIND_CLIENT_MOVE_DOWN:
            new_y += move_step;
            break;
        case KEYBIND_CLIENT_MOVE_TOP_LEFT:
            new_x = 0;
            new_y = 0;
            break;
        case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
            new_x = max_x;
            new_y = 0;
            break;
        case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
            new_x = 0;
            new_y = max_y;
            break;
        case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
            new_x = max_x;
            new_y = max_y;
            break;
    }

    (void) client_send_event_move(client, new_x, new_y);
}


/* Keyboard resize dispatch */

/**
 * @brief Resize the focused client by keyboard
 *
 * Resolves the active client, checks that it is resizable and not in a
 * state that prevents resizing (fullscreen, maximized), and applies an
 * increment-aware size change in the direction indicated by @p btype.
 * @c Left / @c Up shrink from the right/bottom edge; @c Right / @c Down
 * grow that edge.
 *
 * @param btype    Keyboard binding type (one of the
 *                 @c KEYBIND_CLIENT_RESIZE_* constants)
 * @param surface  Current surface
 * @param surfaces Full surface list
 * @param config   Active configuration (for the resize step size)
 */
static void s_handle_kbd_resize(enum wm_keybind_type_e btype,
        surface_td *surface, list_td *surfaces,
        const config_td *config)
{
    client_td *client;
    uint32_t resize_step;
    int32_t new_x;
    int32_t new_y;
    uint32_t old_w;
    uint32_t old_h;
    int32_t new_w;
    int32_t new_h;

    client = s_get_active_client(surface, surfaces, NULL, NULL);
    if (client == NULL || !client_is_resizable(client)) {
        return;
    }

    /* Refuse to resize clients in a fixed-size state */
    if (client->properties.state == (uint16_t) CLIENT_STATE_FULLSCREEN ||
            client->properties.state == (uint16_t) CLIENT_STATE_MAXIMIZED ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ ||
            client->properties.state ==
                (uint16_t) CLIENT_STATE_MAXIMIZED_VERT) {
        return;
    }

    resize_step = (config->base.windows.resize_step > 0u)
        ? config->base.windows.resize_step : 1u;
    new_x = client->layout.geometry.cur.pos.x;
    new_y = client->layout.geometry.cur.pos.y;

    /* Operate in frame space (outer dimensions including decoration
     * extents).  's_kbd_resize_apply' converts to inner space
     * internally when applying size hints. */
    old_w = client->layout.geometry.cur.dim.w;
    old_h = client_is_shaded(client)
        ? client->layout.geometry.old.dim.h
        : client->layout.geometry.cur.dim.h;
    new_w = (int32_t) old_w;
    new_h = (int32_t) old_h;

    switch (btype) {
        /* To avoid warnings from the compiler, ALL cases must be here */
        case KEYBIND_NONE:
        case KEYBIND_DESKTOP_NEXT:
        case KEYBIND_DESKTOP_PREV:
        case KEYBIND_CLIENT_ICONIFY:
        case KEYBIND_CLIENT_HIDE:
        case KEYBIND_CLIENT_CLOSE:
        case KEYBIND_CLIENT_KILL:
        case KEYBIND_CLIENT_MAXIMIZE:
        case KEYBIND_CLIENT_CENTER:
        case KEYBIND_CLIENT_MOVE_NEXT_MONITOR:
        case KEYBIND_CLIENT_SHADE:
        case KEYBIND_CLIENT_FULLSCREEN:
        case KEYBIND_CLIENT_PIN:
        case KEYBIND_CLIENT_INFO:
        case KEYBIND_CLIENT_TOGGLE_DECORATION:
        case KEYBIND_CLIENT_CYCLE_LAYER:
        case KEYBIND_CLIENT_CYCLE_NEXT:
        case KEYBIND_CLIENT_CYCLE_PREV:
        case KEYBIND_DESKTOP_ICON_NEXT:
        case KEYBIND_DESKTOP_ICON_PREV:
        case KEYBIND_LAUNCH_TERMINAL:
        case KEYBIND_LAUNCH_LAUNCHER:
        case KEYBIND_LAUNCH_FILE_MANAGER:
        case KEYBIND_LAUNCH_WEB_BROWSER:
        case KEYBIND_LAUNCH_EDITOR:
        case KEYBIND_CLIENT_MOVE_LEFT:
        case KEYBIND_CLIENT_MOVE_RIGHT:
        case KEYBIND_CLIENT_MOVE_UP:
        case KEYBIND_CLIENT_MOVE_DOWN:
        case KEYBIND_CLIENT_MOVE_TOP_LEFT:
        case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
        case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
        case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
        case KEYBIND_DESKTOP_SHOW:
        case KEYBIND_DESKTOP_GOTO_0:
        case KEYBIND_DESKTOP_GOTO_1:
        case KEYBIND_DESKTOP_GOTO_2:
        case KEYBIND_DESKTOP_GOTO_3:
        case KEYBIND_DESKTOP_GOTO_4:
        case KEYBIND_DESKTOP_GOTO_5:
        case KEYBIND_DESKTOP_GOTO_6:
        case KEYBIND_DESKTOP_GOTO_7:
        case KEYBIND_DESKTOP_GOTO_8:
        case KEYBIND_DESKTOP_GOTO_9:
        case KEYBIND_WM_ROOT_MENU:
        case KEYBIND_WM_WINDOWS_MENU:
        case KEYBIND_CLIENT_WINDOW_MENU:
        case KEYBIND_WM_REDRAW:
        case KEYBIND_WM_RELOAD:
        case KEYBIND_WM_QUIT:
        case KEYBIND_WM_SHORTCUTS_LIST:
        case KEYBIND_WM_EMERGENCY_EXIT:
        case KEYBIND_WM_FORTUNE:
            return;

        case KEYBIND_CLIENT_RESIZE_LEFT:
            new_w = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, true, old_w, false);
            new_x += (int32_t) old_w - new_w;
            break;
        case KEYBIND_CLIENT_RESIZE_RIGHT:
            new_w = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, true, old_w, true);
            break;
        case KEYBIND_CLIENT_RESIZE_UP:
            new_h = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, false, old_h, false);
            new_y += (int32_t) old_h - new_h;
            break;
        case KEYBIND_CLIENT_RESIZE_DOWN:
            new_h = (int32_t) s_kb_resize_axis_target(client,
                    resize_step, false, old_h, true);
            break;
    }

    s_kbd_resize_apply(client,
            new_x, new_y,
            geom_clamp_dim(new_w),
            geom_clamp_dim(new_h));
}


/* Public event handlers */

/* Handle a key-release event to auto-confirm the cycle menu */
void keyboard_handle_release(xcb_key_symbols_t *keysyms,
        xcb_key_release_event_t *event, list_td *surfaces,
        const config_td *config)
{
    xcb_keysym_t keysym;

    if (keysyms == NULL || event == NULL) {
        return;
    }

    keysym = xcb_key_symbols_get_keysym(keysyms, event->detail, 0);

    /* Auto-confirm cycle menu when its modifier is released */
    if (cycle_is_open() && cycle_modifier() != 0 &&
            keyboard_is_modifier_for_mask(keysym, cycle_modifier())) {
        surface_td *surface = s_lookup_surface_fallback(surfaces,
                event->root);
        if (surface != NULL) {
            cycle_confirm(surface->connection, surfaces, config);
        }
        return;
    }
}


/* Translate a key-press event into an action and dispatch it */
void keyboard_handle_press(xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event, list_td *surfaces,
        const config_td *config)
{
    xcb_keysym_t keysym;
    uint16_t state;
    surface_td *surface;

    if (keysyms == NULL || event == NULL || config == NULL) {
        LOGGER_ERROR("Received null pointer in key press handler",
                L_NARG);
        return;
    }

    keysym = xcb_key_symbols_get_keysym(keysyms, event->detail, 0);
    state = (uint16_t) ((unsigned int) event->state &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));

    LOGGER_TRACE("Key press event (keysym=0x%x, state=0x%x)",
            keysym, state);

    surface = s_lookup_surface_fallback(surfaces, event->root);

    /* Keyboard modal move/resize intercepts all keys while active */
    if (kbd_modal_is_active()) {
        kbd_modal_handle_keypress(
                (surface != NULL) ? surface->connection : NULL,
                surface, keysym, config);
        return;
    }

    /* Cycle menu intercepts all keys while it is open */
    if (cycle_is_open()) {
        s_handle_cycle_key(keysym, state, surface, surfaces, config);
        return;
    }

    /* Quit-confirmation dialog intercepts all keys while open */
    if (dialog_quit_is_open()) {
        s_handle_dialog_quit_key(keysym, surface, config);
        return;
    }

    /* Info dialog: Up/Down scroll by one line, PageUp/PageDown by a
     * whole page (harmless no-ops when the message already fits
     * without scrolling; see 'menu_message_dialog_scroll'), and
     * Enter, Space, or Escape close it same as clicking "OK" would */
    if (dialog_info_is_open()) {
        if (surface != NULL && surface->connection != NULL) {
            if (keysym == 0xff52u) {          /* Up */
                menu_message_dialog_scroll(surface->connection,
                        config, -1);
                return;
            }
            if (keysym == 0xff54u) {          /* Down */
                menu_message_dialog_scroll(surface->connection,
                        config, 1);
                return;
            }
            if (keysym == 0xff55u) {          /* Page_Up */
                menu_message_dialog_scroll(surface->connection,
                        config, -(int32_t) DIALOG_MSG_MAX_LINES);
                return;
            }
            if (keysym == 0xff56u) {          /* Page_Down */
                menu_message_dialog_scroll(surface->connection,
                        config, (int32_t) DIALOG_MSG_MAX_LINES);
                return;
            }
        }
        if (keysym == 0xff0du || keysym == 0xff8du ||
                keysym == 0x0020u || keysym == 0xff1bu) {
            if (surface != NULL && surface->connection != NULL) {
                dialog_info_close(surface->connection);
            }
        }
        return;
    }

    /* Message dialog (warnings, errors, info messages, the 'fortune'
     * easter egg): a single "OK" button, so Enter, Space, or Escape
     * all just dismiss it the same way clicking that button would */
    if (menu_message_dialog_is_open()) {
        if (keysym == 0xff0du || keysym == 0xff8du ||
                keysym == 0x0020u || keysym == 0xff1bu) {
            if (surface != NULL && surface->connection != NULL) {
                menu_message_dialog_close(surface->connection);
            }
        }
        return;
    }

    /* Context menus intercept all keys while any menu is open */
    if (s_dispatch_open_menu_key(keysym, (surface != NULL)
                ? surface->connection : NULL,
            surface, config)) {
        return;
    }

    /* Emergency exit 'Ctrl+Mod1+BackSpace' */
    if (config->base.enable_emergency_shortcut &&
            keysym == 0xff08u &&
            (event->state & XCB_MOD_MASK_CONTROL) &&
            (event->state & XCB_MOD_MASK_1)) {
        LOGGER_NOTICE("Emergency exit key combination detected", L_NARG);
        wm_enable_emergency_exit();
        raise(SIGTERM);
        return;
    }

    /* Walk the binding table and dispatch the first match */
    for (int i = 0; i < keyboard_binding_count(); ++i) {
        xcb_keysym_t bks;
        uint16_t bmm;
        enum wm_keybind_type_e btype;
        uint16_t bind_state;

        btype = keyboard_binding_at(i, &bks, &bmm);
        bind_state = (uint16_t) ((unsigned int) bmm &
                ~((unsigned int) XCB_MOD_MASK_LOCK |
                    (unsigned int) XCB_MOD_MASK_2));

        if (keysym != bks || state != bind_state) {
            continue;
        }

        switch (btype) {
            case KEYBIND_DESKTOP_NEXT: {
                if (surface != NULL) {
                    event_td *ev;
                    action_td action;
                    action.type = ACTION_TYPE_SURFACE;
                    action.object.surface =
                        ACTION_SURFACE_DESKTOP_SWITCH_NEXT;
                    ev = event_init((void *) surface, NULL, action,
                            PRIORITY_NORMAL);
                    if (ev != NULL) { eventq_add(ev); }
                }
                return;
            }

            case KEYBIND_DESKTOP_PREV: {
                if (surface != NULL) {
                    event_td *ev;
                    action_td action;
                    action.type = ACTION_TYPE_SURFACE;
                    action.object.surface =
                        ACTION_SURFACE_DESKTOP_SWITCH_PREV;
                    ev = event_init((void *) surface, NULL, action,
                            PRIORITY_NORMAL);
                    if (ev != NULL) { eventq_add(ev); }
                }
                return;
            }

            case KEYBIND_DESKTOP_SHOW:
                if (surface != NULL) {
                    hi_handle_net_showing_desktop(surface,
                            !surface->showing_desktop);
                }
                return;

            case KEYBIND_DESKTOP_GOTO_0:
            case KEYBIND_DESKTOP_GOTO_1:
            case KEYBIND_DESKTOP_GOTO_2:
            case KEYBIND_DESKTOP_GOTO_3:
            case KEYBIND_DESKTOP_GOTO_4:
            case KEYBIND_DESKTOP_GOTO_5:
            case KEYBIND_DESKTOP_GOTO_6:
            case KEYBIND_DESKTOP_GOTO_7:
            case KEYBIND_DESKTOP_GOTO_8:
            case KEYBIND_DESKTOP_GOTO_9:
                if (surface != NULL) {
                    action_data_surface_td sdata;
                    sdata.surface = surface;
                    sdata.action_surface = ACTION_SURFACE_DESKTOP_SWITCH;
                    sdata.new_data.uvalue = (uint32_t) (btype -
                            KEYBIND_DESKTOP_GOTO_0);
                    scmd_surface_desktop_switch(surface, &sdata);
                }
                return;

            case KEYBIND_CLIENT_CYCLE_NEXT:
            case KEYBIND_CLIENT_CYCLE_PREV:
                if (surface != NULL) {
                    desktop_td *desktop =
                        lookup_current_desktop(surface);
                    xcb_connection_t *conn = surface->connection;
                    if (desktop != NULL && conn != NULL) {
                        int dir = (btype == KEYBIND_CLIENT_CYCLE_NEXT)
                            ? 1 : -1;
                        cycle_open(surfaces, conn, surface, desktop,
                                false, dir, bmm, config);
                        cycle_draw(conn, config);
                    }
                }
                return;

            case KEYBIND_DESKTOP_ICON_NEXT:
            case KEYBIND_DESKTOP_ICON_PREV:
                if (surface != NULL) {
                    desktop_td *desktop =
                        lookup_current_desktop(surface);
                    xcb_connection_t *conn = surface->connection;
                    if (desktop != NULL && conn != NULL) {
                        int dir = (btype == KEYBIND_DESKTOP_ICON_NEXT)
                            ? 1 : -1;
                        cycle_open(surfaces, conn, surface, desktop,
                                true, dir, bmm, config);
                        cycle_draw(conn, config);
                    }
                }
                return;

            case KEYBIND_WM_EMERGENCY_EXIT:
                /* Already handled above via the hardcoded shortcut */
                return;

            case KEYBIND_WM_FORTUNE:
                if (config->base.enable_fortune_shortcut &&
                        surface != NULL && surface->connection != NULL) {
                    dialog_fortune_show(surface->connection, surface,
                            config);
                }
                return;

            case KEYBIND_WM_REDRAW:
                wm_request_full_redraw();
                return;

            case KEYBIND_WM_QUIT:
                if (surface != NULL && surface->connection != NULL) {
                    dialog_quit_show(surface->connection, surface,
                            config);
                }
                return;

            case KEYBIND_WM_SHORTCUTS_LIST:
                if (surface != NULL && surface->connection != NULL) {
                    dialog_shortcuts_show(surface->connection, surface,
                            config);
                }
                return;

            case KEYBIND_WM_RELOAD:
                (void) wm_action_config_reload();
                return;

            case KEYBIND_WM_ROOT_MENU:
                if (surface != NULL && surface->connection != NULL) {
                    int16_t mx = (int16_t) (surface->properties.dim.w / 2u);
                    int16_t my = (int16_t) (surface->properties.dim.h / 2u);

                    /* When configured to appear under the cursor
                     * instead of always centered, query the current
                     * pointer position and use it, falling back to the
                     * screen center if the query fails */
                    if (config != NULL &&
                            config->base.menus.root.position ==
                                CONFIG_MENU_POSITION_UNDER_MOUSE &&
                            surface->screen != NULL) {
                        xcb_query_pointer_cookie_t qc =
                            xcb_query_pointer(surface->connection,
                                    surface->screen->root);
                        xcb_query_pointer_reply_t *qr =
                            xcb_query_pointer_reply(surface->connection,
                                    qc, NULL);
                        if (qr != NULL) {
                            mx = qr->root_x;
                            my = qr->root_y;
                            free(qr);
                        }
                    }

                    rootmenu_show(surface->connection, surface,
                            mx, my, config, wm_get_config_dir());
                }
                return;

            case KEYBIND_WM_WINDOWS_MENU:
                if (surface != NULL && surface->connection != NULL) {
                    int16_t mx = (int16_t) (surface->properties.dim.w / 2u);
                    int16_t my = (int16_t) (surface->properties.dim.h / 2u);

                    /* Same "under the cursor instead of a fixed point"
                     * behavior as the root menu (see
                     * 'KEYBIND_WM_ROOT_MENU' above), just governed by
                     * its own 'menus.windows.position' setting */
                    if (config != NULL &&
                            config->base.menus.windows.position ==
                                CONFIG_MENU_POSITION_UNDER_MOUSE &&
                            surface->screen != NULL) {
                        xcb_query_pointer_cookie_t qc =
                            xcb_query_pointer(surface->connection,
                                    surface->screen->root);
                        xcb_query_pointer_reply_t *qr =
                            xcb_query_pointer_reply(surface->connection,
                                    qc, NULL);
                        if (qr != NULL) {
                            mx = qr->root_x;
                            my = qr->root_y;
                            free(qr);
                        }
                    }

                    winlist_show(surface->connection, surface,
                            mx, my, config);
                }
                return;

            case KEYBIND_CLIENT_WINDOW_MENU: {
                /* Hardcoded 'Alt+Space': opens the context menu of the
                 * currently active client, anchored at its own position
                 * (unrelated to 'KEYBIND_WM_WINDOWS_MENU') */
                client_td *client = s_get_active_client(surface,
                        surfaces, NULL, NULL);
                if (client != NULL && surface != NULL &&
                        surface->connection != NULL) {
                    desktop_td *desktop =
                        lookup_current_desktop(surface);
                    int16_t mx = (int16_t)
                        client->layout.geometry.cur.pos.x;
                    int16_t my = (int16_t)
                        client->layout.geometry.cur.pos.y;
                    wincmenu_show(surface->connection, surface,
                            desktop, client, mx, my, config);
                }
                return;
            }

            case KEYBIND_CLIENT_ICONIFY:
            case KEYBIND_CLIENT_HIDE:
            case KEYBIND_CLIENT_CLOSE:
            case KEYBIND_CLIENT_KILL:
            case KEYBIND_CLIENT_MAXIMIZE:
            case KEYBIND_CLIENT_CENTER:
            case KEYBIND_CLIENT_MOVE_NEXT_MONITOR:
            case KEYBIND_CLIENT_SHADE:
            case KEYBIND_CLIENT_FULLSCREEN:
            case KEYBIND_CLIENT_PIN:
            case KEYBIND_CLIENT_INFO:
            case KEYBIND_CLIENT_TOGGLE_DECORATION:
            case KEYBIND_CLIENT_CYCLE_LAYER:
                s_dispatch_client_action(btype, surface, surfaces,
                        bmm, event->detail, config);
                return;

            case KEYBIND_LAUNCH_TERMINAL:
            case KEYBIND_LAUNCH_LAUNCHER:
            case KEYBIND_LAUNCH_FILE_MANAGER:
            case KEYBIND_LAUNCH_WEB_BROWSER:
            case KEYBIND_LAUNCH_EDITOR:
                s_handle_kbd_launch(btype, surface, config);
                return;

            case KEYBIND_CLIENT_MOVE_LEFT:
            case KEYBIND_CLIENT_MOVE_RIGHT:
            case KEYBIND_CLIENT_MOVE_UP:
            case KEYBIND_CLIENT_MOVE_DOWN:
            case KEYBIND_CLIENT_MOVE_TOP_LEFT:
            case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
            case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
            case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
                s_handle_kbd_move(btype, surface, surfaces, config);
                return;

            case KEYBIND_CLIENT_RESIZE_LEFT:
            case KEYBIND_CLIENT_RESIZE_RIGHT:
            case KEYBIND_CLIENT_RESIZE_UP:
            case KEYBIND_CLIENT_RESIZE_DOWN:
                s_handle_kbd_resize(btype, surface, surfaces, config);
                return;

            case KEYBIND_NONE:
                LOGGER_TRACE("Ignoring 'KEYBIND_NONE' entry", L_NARG);
                return;
        }
    }
}
