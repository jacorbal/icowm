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
#include <stdlib.h>     /* free, NULL */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Render includes */
#include <render/surface.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/cycle.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/fortune.h>
#include <menu/dialog/info.h>
#include <menu/dialog/message.h>
#include <menu/dialog/quit.h>
#include <menu/dialog/shortcuts.h>
#include <menu/popup.h>
#include <menu/dialog/run.h>
#include <menu/search.h>

/* Handler includes */
#include <handler/internal.h>

/* Command includes */
#include <cmds/client/basic.h>
#include <cmds/surface.h>

/* Default initial values */
#include <defs/dialog.h>
#include <defs/kbd.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <scratchpad.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <input/kbd/bind.h>
#include <input/kbd/event.h>
#include <input/kbd/internal.h>
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


/* Cycle-menu key handling */

/**
 * @brief Handle a key press while the window-cycle menu is open
 *
 * Navigates the cycle menu with arrow keys, confirms with Enter,
 * cancels with @c Escape, and navigates with the configured
 * cycle-next/prev bindings.  A bare modifier key-press (@c Shift,
 * @c Control, and so on, pressed on its own) is ignored outright,
 * since it carries no navigation intent by itself; any other key
 * closes the menu without activating a client.
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
    xcb_connection_t *const conn = (surface != NULL) ? surface->connection
                                               : NULL;

    /* Up arrow: go to previous entry */
    if (keysym == KS_UP) {
        cycle_navigate_prev();
        if (conn != NULL) { cycle_draw(conn, config); }
        return;
    }

    /* Down arrow: go to next entry */
    if (keysym == KS_DOWN) {
        cycle_navigate_next();
        if (conn != NULL) { cycle_draw(conn, config); }
        return;
    }

    /* Enter / KP_Enter: confirm selection */
    if (keysym == KS_RETURN || keysym == KS_KP_ENTER) {
        if (conn != NULL) { cycle_confirm(conn, surfaces, config); }
        return;
    }

    /* Escape: cancel without activating */
    if (keysym == KS_ESCAPE) {
        if (conn != NULL) { cycle_destroy(conn); }
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

    /* A bare modifier key-press (e.g. tapping Shift on its own while
     * Alt is still held, to switch cycling direction before the next
     * cycle-next/prev key comes back down) generates its own KeyPress
     * for that modifier's own keysym, which matches none of the cases
     * above; without this, it would fall through to the catch-all
     * below and close the menu the instant a modifier is pressed,
     * before the person ever gets a chance to press the direction key
     * again with the now-changed modifier state. */
    if (keyboard_keysym_is_modifier(keysym)) {
        return;
    }

    /* Any other key: close the menu without action */
    if (conn != NULL) { cycle_destroy(conn); }
}


/* Confirmation dialog key handling */

/**
 * @brief Handle a key press while the generic confirm dialog is open
 *
 * Backs both the quit-confirmation dialog and any other two-button
 * confirm dialog built on @c menu/dialog/confirm.h, e.g.,
 * @c menu/dialog/rrsafe.h.
 *
 * @c Tab / @c Left / @c Right toggle the selected button; @c Enter
 * activates it; @c Escape always cancels the dialog, regardless of
 * which button happens to be selected at the time.
 *
 * @param keysym  Keysym of the pressed key
 * @param surface Surface for drawing (may be null)
 * @param config  Active configuration
 *
 * @see @a menu_confirm_dialog_cancel
 */
static void s_handle_menu_confirm_dialog_key(xcb_keysym_t keysym,
        surface_td *surface, const config_td *config)
{
    xcb_connection_t *const conn = (surface != NULL) ? surface->connection
                                               : NULL;

    /* Tab, Left arrow, Right arrow: toggle selected button */
    if (keysym == KS_TAB || keysym == KS_LEFT || keysym == KS_RIGHT) {
        menu_confirm_dialog_toggle_selection();
        if (conn != NULL) { menu_confirm_dialog_repaint(conn, config); }
        return;
    }

    /* Enter / KP_Enter / Space: activate the currently selected button */
    if (keysym == KS_RETURN || keysym == KS_KP_ENTER ||
            keysym == KS_SPACE) {
        if (conn != NULL) { menu_confirm_dialog_accept(conn); }
        return;
    }

    /* Escape: always cancels, whichever button is currently selected */
    if (keysym == KS_ESCAPE) {
        if (conn != NULL) { menu_confirm_dialog_cancel(conn); }
    }
}


/* Context-menu key handling */

/**
 * @brief Resolve where a keyboard-triggered root/window-list menu
 *        should open: centered on the surface, or under the current
 *        pointer position
 *
 * Shared by @c KEYBIND_WM_ROOT_MENU and @c KEYBIND_WM_WINDOWS_MENU in
 * @c keyboard_handle_press, which only differ in which configuration
 * field selects "under mouse" and which function they go on to call
 * with the resolved position.  Falls back to the surface center if
 * @p under_mouse is @c true but the pointer query itself fails.
 *
 * @param surface     Surface the menu will open on
 * @param under_mouse Whether to query the pointer at all, rather than
 *                    always using the surface center
 * @param out_pos     Receives the resolved position
 *
 * @note Complexity: @e O(1)
 */
static void s_menu_position_resolve(surface_td *surface, bool under_mouse,
        struct position_s *restrict out_pos)
{
    out_pos->x = (int32_t) (surface->properties.dim.w / 2u);
    out_pos->y = (int32_t) (surface->properties.dim.h / 2u);

    if (under_mouse && surface->screen != NULL) {
        xcb_query_pointer_cookie_t qc =
            xcb_query_pointer(surface->connection, surface->screen->root);
        xcb_query_pointer_reply_t *const qr =
            xcb_query_pointer_reply(surface->connection, qc, NULL);
        if (qr != NULL) {
            out_pos->x = qr->root_x;
            out_pos->y = qr->root_y;
            free(qr);
        }
    }
}


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
 * @p btype.  Each binding type maps to exactly one @c enact_client_*
 * function.  Actions that require resize capability (maximize, fullscreen)
 * are silently dropped when the client is not resizable, and
 * @c KEYBIND_CLIENT_CYCLE_LAYER is silently dropped when the client is
 * fullscreen (see @a ccmd_desktop_enforce_layers's own doc comment on
 * why changing its layer there would have no visible effect).
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
    const desktop_td *desktop;
    client_td *client;

    if (surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    client = ik_get_active_client(surface, surfaces, NULL, NULL);
    if (client == NULL) {
        return;
    }

    switch (btype) {
        /* To avoid warnings from the compiler, ALL cases must be here */
        case KEYBIND_NONE:
        case KEYBIND_DESKTOP_NORTH:
        case KEYBIND_DESKTOP_SOUTH:
        case KEYBIND_DESKTOP_EAST:
        case KEYBIND_DESKTOP_WEST:
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
        case KEYBIND_WM_SCRATCHPAD_TOGGLE:
        case KEYBIND_DESKTOP_CLIENTS_ICONIFY_ALL:
        case KEYBIND_DESKTOP_CLIENTS_DEICONIFY_ALL:
        case KEYBIND_DESKTOP_CLIENTS_REARRANGE:
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
        case KEYBIND_DESKTOP_ADD:
        case KEYBIND_DESKTOP_REMOVE:
        case KEYBIND_WM_TOGGLE_STRUTLESS_MAXIMIZE:
        case KEYBIND_WM_ROOT_MENU:
        case KEYBIND_WM_WINDOWS_MENU:
        case KEYBIND_WM_SEARCH_WINDOWS:
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
            enact_client_iconify(client);
            return;

        case KEYBIND_CLIENT_HIDE:
            enact_client_hide(client);
            return;

        case KEYBIND_CLIENT_CLOSE:
            enact_client_close(client);
            return;

        case KEYBIND_CLIENT_KILL:
            enact_client_kill(client);
            return;

        case KEYBIND_CLIENT_MAXIMIZE:
            if (!client_is_resizable(client)) { return; }
            enact_client_maximize(client);
            return;

        case KEYBIND_CLIENT_CENTER:
            enact_client_center(client);
            return;

        case KEYBIND_CLIENT_MOVE_MONITOR_NORTH:
            enact_client_move_monitor_north(client);
            return;

        case KEYBIND_CLIENT_MOVE_MONITOR_SOUTH:
            enact_client_move_monitor_south(client);
            return;

        case KEYBIND_CLIENT_MOVE_MONITOR_EAST:
            enact_client_move_monitor_east(client);
            return;

        case KEYBIND_CLIENT_MOVE_MONITOR_WEST:
            enact_client_move_monitor_west(client);
            return;

        case KEYBIND_CLIENT_SEND_TO_DESKTOP_NORTH:
            enact_client_send_to_desktop_north(client, surfaces, config);
            return;

        case KEYBIND_CLIENT_SEND_TO_DESKTOP_SOUTH:
            enact_client_send_to_desktop_south(client, surfaces, config);
            return;

        case KEYBIND_CLIENT_SEND_TO_DESKTOP_EAST:
            enact_client_send_to_desktop_east(client, surfaces, config);
            return;

        case KEYBIND_CLIENT_SEND_TO_DESKTOP_WEST:
            enact_client_send_to_desktop_west(client, surfaces, config);
            return;

        case KEYBIND_CLIENT_SHADE:
            enact_client_toggle_shade(client);
            return;

        case KEYBIND_CLIENT_FULLSCREEN:
            /* Blocks entering, the same as maximize above, but not
             * exiting: a client already fullscreen through its own
             * EWMH request stays exitable here regardless of its own
             * resizable flag, the one case 'ccmd_client_fullscreen'
             * itself (cmds/client/state.c) still leaves ungated on
             * purpose. */
            if (!client_is_resizable(client) &&
                    !client_is_fullscreen(client)) {
                return;
            }
            enact_client_toggle_fullscreen(client);
            return;

        case KEYBIND_CLIENT_PIN:
            enact_client_toggle_pin(client);
            return;

        case KEYBIND_CLIENT_TOGGLE_DECORATION:
            /* Unshade first: toggling decoration while shaded would
             * leave the window in an inconsistent visual state */
            if (client_is_shaded(client)) {
                ccmd_client_unshade(client);
            }
            enact_client_toggle_decorate(client);
            return;

        case KEYBIND_CLIENT_CYCLE_LAYER:
            /* A fullscreen client's own stacking is always forced
             * above everything else while it holds focus, regardless
             * of its own real layer (see 'ccmd_desktop_enforce_
             * layers''s own doc comment); cycling its layer here
             * would silently do nothing visible until it later
             * leaves fullscreen, the same reasoning the window
             * context menu's own 'Layer' submenu is disabled for
             * already. */
            if (!client_is_fullscreen(client)) {
                enact_client_cycle_layer(client);
            }
            return;
    }
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
        surface_td *const surface = s_lookup_surface_fallback(surfaces,
                event->root);
        if (surface != NULL) {
            cycle_confirm(surface->connection, surfaces, config);
        }
        return;
    }
}


/* Translate a key-press event into an action and dispatch it */
void keyboard_handle_press(wm_td *wm, xcb_key_symbols_t *keysyms,
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

    /* Fuzzy window-search widget intercepts all keys while open */
    if (search_is_open()) {
        search_handle_keypress(
                (surface != NULL) ? surface->connection : NULL,
                surfaces, keysym, state, config);
        return;
    }

    /* Built-in run-box intercepts all keys while open */
    if (run_is_open()) {
        run_handle_keypress(
                (surface != NULL) ? surface->connection : NULL,
                surface, keysym, config);
        return;
    }

    /* Generic confirm dialog intercepts all keys while open, whether
     * it is currently showing as the quit-confirmation dialog or
     * something else built on 'menu/dialog/confirm.h' (see
     * 's_handle_menu_confirm_dialog_key'); only one instance of it
     * can ever be open at a time, so which wrapper opened it does not
     * matter here. */
    if (menu_confirm_dialog_is_open()) {
        s_handle_menu_confirm_dialog_key(keysym, surface, config);
        return;
    }

    /* Info dialog: Up/Down scroll by one line, PageUp/PageDown by a
     * whole page (harmless no-ops when the message already fits
     * without scrolling; see 'menu_message_dialog_scroll'), and
     * Enter, Space, or Escape close it same as clicking "OK" would */
    if (dialog_info_is_open()) {
        if (surface != NULL && surface->connection != NULL) {
            if (keysym == KS_UP) {
                menu_message_dialog_scroll(surface->connection,
                        config, -1);
                return;
            }
            if (keysym == KS_DOWN) {
                menu_message_dialog_scroll(surface->connection,
                        config, 1);
                return;
            }
            if (keysym == KS_PAGE_UP) {
                menu_message_dialog_scroll(surface->connection,
                        config, -(int32_t) DIALOG_MSG_MAX_LINES);
                return;
            }
            if (keysym == KS_PAGE_DOWN) {
                menu_message_dialog_scroll(surface->connection,
                        config, (int32_t) DIALOG_MSG_MAX_LINES);
                return;
            }
            if (keysym == KS_TAB) {
                menu_message_dialog_select_ok(surface->connection,
                        config);
                return;
            }
        }
        /* Warning and error dialogs (see 'menu_message_dialog_
         * requires_selection') cannot be reflex-dismissed: Escape
         * does nothing at all, and Enter/Space only activate "OK"
         * once it has actually been selected (Tab, just above, or a
         * direct click; see 'menu_message_dialog_handle_click' in
         * input/mouse/event/press.c, which is not gated the same
         * way, since
         * a deliberate click already demonstrates the same intent
         * selecting first and then pressing Enter/Space would).
         * Every other level keeps the previous, quicker-to-dismiss
         * behavior, where all four keys always just close it. */
        if (keysym == KS_RETURN || keysym == KS_KP_ENTER ||
                keysym == KS_SPACE) {
            if (surface != NULL && surface->connection != NULL &&
                    (!menu_message_dialog_requires_selection() ||
                     menu_message_dialog_ok_selected())) {
                dialog_info_close(surface->connection);
            }
        } else if (keysym == KS_ESCAPE) {
            if (surface != NULL && surface->connection != NULL &&
                    !menu_message_dialog_requires_selection()) {
                dialog_info_close(surface->connection);
            }
        }
        return;
    }

    /* Message dialog (warnings, errors, info messages, the 'fortune'
     * easter egg).  Dead code today, since 'dialog_info_is_open()'
     * above already covers the exact same underlying state and always
     * returns first; kept in the same up-to-date shape as that block
     * regardless, rather than left to visibly rot, in case a future
     * change to the block above ever makes this one reachable again. */
    if (menu_message_dialog_is_open()) {
        if (keysym == KS_RETURN || keysym == KS_KP_ENTER ||
                keysym == KS_SPACE) {
            if (surface != NULL && surface->connection != NULL &&
                    (!menu_message_dialog_requires_selection() ||
                     menu_message_dialog_ok_selected())) {
                menu_message_dialog_close(surface->connection);
            }
        } else if (keysym == KS_ESCAPE) {
            if (surface != NULL && surface->connection != NULL &&
                    !menu_message_dialog_requires_selection()) {
                menu_message_dialog_close(surface->connection);
            }
        } else if (keysym == KS_TAB &&
                surface != NULL && surface->connection != NULL) {
            menu_message_dialog_select_ok(surface->connection, config);
        }
        return;
    }

    /* Context menus intercept all keys while any menu is open */
    if (s_dispatch_open_menu_key(keysym, (surface != NULL)
                ? surface->connection : NULL,
            surface, config)) {
        return;
    }

    /* Emergency exit 'Ctrl+Mod1+Backspace' */
    if (config->base.shutdown.enable_emergency_shortcut &&
            keysym == KS_BACKSPACE &&
            (event->state & XCB_MOD_MASK_CONTROL) &&
            (event->state & XCB_MOD_MASK_1)) {
        LOGGER_NOTICE("Emergency exit key combination detected", L_NARG);
        wm_emergency_exit_enable();
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
            case KEYBIND_DESKTOP_NORTH:
                if (surface != NULL) {
                    enact_surface_desktop_switch_north(surface);
                }
                return;

            case KEYBIND_DESKTOP_SOUTH:
                if (surface != NULL) {
                    enact_surface_desktop_switch_south(surface);
                }
                return;

            case KEYBIND_DESKTOP_EAST:
                if (surface != NULL) {
                    enact_surface_desktop_switch_east(surface);
                }
                return;

            case KEYBIND_DESKTOP_WEST:
                if (surface != NULL) {
                    enact_surface_desktop_switch_west(surface);
                }
                return;

            case KEYBIND_DESKTOP_SHOW:
                if (surface != NULL) {
                    enact_desktop_show(lookup_current_desktop(surface),
                            !surface->showing_desktop);
                }
                return;

            case KEYBIND_WM_SCRATCHPAD_TOGGLE:
                if (surface != NULL) {
                    scratchpad_toggle(wm, lookup_current_desktop(surface));
                }
                return;

            case KEYBIND_DESKTOP_CLIENTS_ICONIFY_ALL:
                if (surface != NULL) {
                    enact_desktop_clients_iconify_all(
                            lookup_current_desktop(surface));
                }
                return;

            case KEYBIND_DESKTOP_CLIENTS_DEICONIFY_ALL:
                if (surface != NULL) {
                    enact_desktop_clients_deiconify_all(
                            lookup_current_desktop(surface));
                }
                return;

            case KEYBIND_DESKTOP_CLIENTS_REARRANGE:
                if (surface != NULL) {
                    enact_desktop_clients_rearrange(wm, surface,
                            lookup_current_desktop(surface));
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
                    enact_surface_desktop_switch(surface,
                            (uint32_t) (btype - KEYBIND_DESKTOP_GOTO_0));
                }
                return;

            case KEYBIND_DESKTOP_ADD:
                if (surface != NULL) {
                    enact_surface_desktop_add(surface);
                }
                return;

            case KEYBIND_DESKTOP_REMOVE:
                if (surface != NULL) {
                    enact_surface_desktop_remove(surface);
                }
                return;

            case KEYBIND_WM_TOGGLE_STRUTLESS_MAXIMIZE:
                if (surface != NULL) {
                    enact_surface_toggle_strutless_maximize(surface);
                }
                return;

            case KEYBIND_CLIENT_CYCLE_NEXT:
            case KEYBIND_CLIENT_CYCLE_PREV:
                if (surface != NULL) {
                    desktop_td *desktop =
                        lookup_current_desktop(surface);
                    if (desktop != NULL) {
                        if (btype == KEYBIND_CLIENT_CYCLE_NEXT) {
                            enact_desktop_cycle_clients_active(
                                    surface->connection, surface,
                                    desktop, bmm, config);
                        } else {
                            enact_desktop_cycle_clients_prev(
                                    surface->connection, surface,
                                    desktop, bmm, config);
                        }
                    }
                }
                return;

            case KEYBIND_DESKTOP_ICON_NEXT:
            case KEYBIND_DESKTOP_ICON_PREV:
                if (surface != NULL) {
                    desktop_td *desktop =
                        lookup_current_desktop(surface);
                    if (desktop != NULL) {
                        if (btype == KEYBIND_DESKTOP_ICON_NEXT) {
                            enact_desktop_cycle_clients_icons_next(
                                    surface->connection,
                                    surface, desktop, bmm, config);
                        } else {
                            enact_desktop_cycle_clients_icons_prev(
                                    surface->connection,
                                    surface, desktop, bmm, config);
                        }
                    }
                }
                return;

            case KEYBIND_WM_EMERGENCY_EXIT:
                /* Already handled above via the hardcoded shortcut */
                return;

            case KEYBIND_WM_FORTUNE:
                if (config->base.fortune.is_enabled &&
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
                (void) wm_action_config_reload(wm);
                return;

            case KEYBIND_WM_ROOT_MENU:
                if (surface != NULL && surface->connection != NULL) {
                    struct position_s pos;

                    /* When configured to appear under the cursor
                     * instead of always centered, query the current
                     * pointer position and use it, falling back to the
                     * screen center if the query fails */
                    s_menu_position_resolve(surface,
                            config != NULL &&
                                config->base.menus.root.position ==
                                    CONFIG_MENU_POSITION_UNDER_MOUSE,
                            &pos);

                    rootmenu_show(wm, surface->connection, surface,
                            pos, config);
                }
                return;

            case KEYBIND_WM_WINDOWS_MENU:
                if (surface != NULL && surface->connection != NULL) {
                    struct position_s pos;

                    /* Same "under the cursor instead of a fixed point"
                     * behavior as the root menu (see
                     * 'KEYBIND_WM_ROOT_MENU' above), just governed by
                     * its own 'menus.windows.position' setting */
                    s_menu_position_resolve(surface,
                            config != NULL &&
                                config->base.menus.windows.position ==
                                    CONFIG_MENU_POSITION_UNDER_MOUSE,
                            &pos);

                    winlist_show(surface->connection, surface,
                            pos, config);
                }
                return;

            case KEYBIND_WM_SEARCH_WINDOWS:
                if (surface != NULL && surface->connection != NULL) {
                    search_init(surfaces, surface->connection,
                            surface, config);
                }
                return;

            case KEYBIND_CLIENT_WINDOW_MENU: {
                /* Hardcoded 'Alt+Space': opens the context menu of the
                 * currently active client, anchored at its own position
                 * (unrelated to 'KEYBIND_WM_WINDOWS_MENU') */
                client_td *const client = ik_get_active_client(surface,
                        surfaces, NULL, NULL);
                if (client != NULL && surface != NULL &&
                        surface->connection != NULL) {
                    desktop_td *desktop =
                        lookup_current_desktop(surface);
                    wincmenu_show(surface->connection, surface,
                            desktop, client,
                            client->layout.geometry.cur.pos, config);
                }
                return;
            }

            case KEYBIND_CLIENT_ICONIFY:
            case KEYBIND_CLIENT_HIDE:
            case KEYBIND_CLIENT_CLOSE:
            case KEYBIND_CLIENT_KILL:
            case KEYBIND_CLIENT_MAXIMIZE:
            case KEYBIND_CLIENT_CENTER:
            case KEYBIND_CLIENT_MOVE_MONITOR_NORTH:
            case KEYBIND_CLIENT_MOVE_MONITOR_SOUTH:
            case KEYBIND_CLIENT_MOVE_MONITOR_EAST:
            case KEYBIND_CLIENT_MOVE_MONITOR_WEST:
            case KEYBIND_CLIENT_SEND_TO_DESKTOP_NORTH:
            case KEYBIND_CLIENT_SEND_TO_DESKTOP_SOUTH:
            case KEYBIND_CLIENT_SEND_TO_DESKTOP_EAST:
            case KEYBIND_CLIENT_SEND_TO_DESKTOP_WEST:
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
                ik_handle_launch(btype, surface, config);
                return;

            case KEYBIND_CLIENT_MOVE_LEFT:
            case KEYBIND_CLIENT_MOVE_RIGHT:
            case KEYBIND_CLIENT_MOVE_UP:
            case KEYBIND_CLIENT_MOVE_DOWN:
            case KEYBIND_CLIENT_MOVE_TOP_LEFT:
            case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
            case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
            case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
                ik_handle_move(btype, surface, surfaces, config);
                return;

            case KEYBIND_CLIENT_RESIZE_LEFT:
            case KEYBIND_CLIENT_RESIZE_RIGHT:
            case KEYBIND_CLIENT_RESIZE_UP:
            case KEYBIND_CLIENT_RESIZE_DOWN:
                ik_handle_resize(btype, surface, surfaces, config);
                return;

            case KEYBIND_NONE:
                LOGGER_TRACE("Ignoring 'KEYBIND_NONE' entry", L_NARG);
                return;
        }
    }
}
