/**
 * @file input/kbd/intercept.c
 *
 * @brief Key-press interception by whatever owns the keyboard
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
#include <stddef.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/cycle.h>
#include <menu/dialog/confirm.h>
#include <menu/dialog/info.h>
#include <menu/dialog/message.h>
#include <menu/dialog/quit.h>
#include <menu/dialog/run.h>
#include <menu/search.h>

/* Command includes */
#include <cmds/surface.h>

/* Default initial values */
#include <defs/dialog.h>
#include <defs/kbd.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <surface.h>

/* Local includes */
#include <input/kbd/internal.h>
#include <input/kbd/modal.h>

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
    xcb_connection_t *const conn = (surface != NULL)
        ? surface->connection : NULL;

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

    /* A bare modifier key-press (e.g., tapping Shift on its own while
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
    xcb_connection_t *const conn = (surface != NULL)
        ? surface->connection : NULL;

    /* Tab, Left arrow, Right arrow: toggle selected button */
    if (keysym == KS_TAB || keysym == KS_LEFT || keysym == KS_RIGHT) {
        menu_confirm_dialog_toggle_selection();
        if (conn != NULL) { menu_confirm_dialog_repaint(conn, config); }
        return;
    }

    /* Enter, KP_Enter and Space activate the currently selected
     * button */
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


/* Let whatever currently owns the keyboard consume the key */
bool ik_intercept_keypress(xcb_keysym_t keysym, uint16_t state,
        surface_td *surface, list_td *surfaces,
        const config_td *config)
{
    /* Keyboard modal move/resize intercepts all keys while active */
    if (kbd_modal_is_active()) {
        kbd_modal_handle_keypress(
                (surface != NULL) ? surface->connection : NULL,
                surface, keysym, config);
        return true;
    }

    /* Cycle menu intercepts all keys while it is open */
    if (cycle_is_open()) {
        s_handle_cycle_key(keysym, state, surface, surfaces, config);
        return true;
    }

    /* Fuzzy window-search widget intercepts all keys while open */
    if (search_is_open()) {
        search_handle_keypress(
                (surface != NULL) ? surface->connection : NULL,
                surfaces, keysym, state, config);
        return true;
    }

    /* Built-in run-box intercepts all keys while open */
    if (run_is_open()) {
        run_handle_keypress(
                (surface != NULL) ? surface->connection : NULL,
                surface, keysym, config);
        return true;
    }

    /* Generic confirm dialog intercepts all keys while open, whether
     * it is currently showing as the quit-confirmation dialog or
     * something else built on 'menu/dialog/confirm.h' (see
     * 's_handle_menu_confirm_dialog_key'); only one instance of it
     * can ever be open at a time, so which wrapper opened it does not
     * matter here. */
    if (menu_confirm_dialog_is_open()) {
        s_handle_menu_confirm_dialog_key(keysym, surface, config);
        return true;
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
                return true;
            }
            if (keysym == KS_DOWN) {
                menu_message_dialog_scroll(surface->connection,
                        config, 1);
                return true;
            }
            if (keysym == KS_PAGE_UP) {
                menu_message_dialog_scroll(surface->connection,
                        config, -(int32_t) DIALOG_MSG_MAX_LINES);
                return true;
            }
            if (keysym == KS_PAGE_DOWN) {
                menu_message_dialog_scroll(surface->connection,
                        config, (int32_t) DIALOG_MSG_MAX_LINES);
                return true;
            }
            if (keysym == KS_TAB) {
                menu_message_dialog_select_ok(surface->connection,
                        config);
                return true;
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
        return true;
    }

    /* Message dialog (warnings, errors, info messages, the 'fortune'
     * easter egg): dead code today, since 'dialog_info_is_open()'
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
        return true;
    }

    /* Context menus intercept all keys while any menu is open */
    if (s_dispatch_open_menu_key(keysym, (surface != NULL)
                ? surface->connection : NULL,
            surface, config)) {
        return true;
    }

    return false;
}
