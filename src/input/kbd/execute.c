/**
 * @file input/kbd/execute.c
 *
 * @brief Execution of a resolved keyboard binding
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
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Render includes */
#include <render/surface.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/context/wincmenu.h>
#include <menu/context/winlist.h>
#include <menu/cycle.h>
#include <menu/dialog/fortune.h>
#include <menu/dialog/quit.h>
#include <menu/dialog/run.h>
#include <menu/dialog/shortcuts.h>
#include <menu/popup.h>
#include <menu/search.h>

/* Handler includes */
#include <handler/internal.h>

/* Command includes */
#include <cmds/client/state.h>
#include <cmds/surface.h>

/* Default initial values */
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
#include <input/kbd/internal.h>

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
 * @brief Dispatch a single-client action triggered by a key binding
 *
 * Resolves the focused client and dispatches the action identified by
 * @p btype.  Each binding type maps to exactly one @c enact_client_*
 * function.  Actions that require resize capability (maximize, fullscreen)
 * are silently dropped when the client is not resizable, and
 * @c KEYBIND_CLIENT_CYCLE_LAYER is silently dropped when the client is
 * fullscreen (see @a ccmd_desktop_enforce_layers's comment on
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
        /* Every case must be listed, so the compiler keeps
         * checking this switch against the whole enumeration */
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
             * layers''s comment); cycling its layer here
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


/* Carry out the action a resolved binding names */
void ik_execute_binding(wm_td *wm, enum wm_keybind_type_e btype,
        uint16_t modmask, xcb_keycode_t keycode,
        surface_td *surface, list_td *surfaces,
        const config_td *config)
{
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
                        !surface->is_showing_desktop);
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
                                desktop, modmask, config);
                    } else {
                        enact_desktop_cycle_clients_prev(
                                surface->connection, surface,
                                desktop, modmask, config);
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
                                surface, desktop, modmask, config);
                    } else {
                        enact_desktop_cycle_clients_icons_prev(
                                surface->connection,
                                surface, desktop, modmask, config);
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
                    modmask, keycode, config);
            return;

        case KEYBIND_LAUNCH_TERMINAL:
            ik_handle_launch(IK_LAUNCH_TERMINAL, surface, config);
            return;
        case KEYBIND_LAUNCH_LAUNCHER:
            ik_handle_launch(IK_LAUNCH_LAUNCHER, surface, config);
            return;
        case KEYBIND_LAUNCH_FILE_MANAGER:
            ik_handle_launch(IK_LAUNCH_FILE_MANAGER, surface, config);
            return;
        case KEYBIND_LAUNCH_WEB_BROWSER:
            ik_handle_launch(IK_LAUNCH_WEB_BROWSER, surface, config);
            return;
        case KEYBIND_LAUNCH_EDITOR:
            ik_handle_launch(IK_LAUNCH_EDITOR, surface, config);
            return;

        case KEYBIND_CLIENT_MOVE_LEFT:
            ik_handle_move(IK_MOVE_LEFT, surface, surfaces, config);
            return;
        case KEYBIND_CLIENT_MOVE_RIGHT:
            ik_handle_move(IK_MOVE_RIGHT, surface, surfaces, config);
            return;
        case KEYBIND_CLIENT_MOVE_UP:
            ik_handle_move(IK_MOVE_UP, surface, surfaces, config);
            return;
        case KEYBIND_CLIENT_MOVE_DOWN:
            ik_handle_move(IK_MOVE_DOWN, surface, surfaces, config);
            return;
        case KEYBIND_CLIENT_MOVE_TOP_LEFT:
            ik_handle_move(IK_MOVE_TOP_LEFT, surface, surfaces,
                    config);
            return;
        case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
            ik_handle_move(IK_MOVE_TOP_RIGHT, surface, surfaces,
                    config);
            return;
        case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
            ik_handle_move(IK_MOVE_BOTTOM_LEFT, surface, surfaces,
                    config);
            return;
        case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
            ik_handle_move(IK_MOVE_BOTTOM_RIGHT, surface, surfaces,
                    config);
            return;

        case KEYBIND_CLIENT_RESIZE_LEFT:
            ik_handle_resize(IK_RESIZE_LEFT, surface, surfaces,
                    config);
            return;
        case KEYBIND_CLIENT_RESIZE_RIGHT:
            ik_handle_resize(IK_RESIZE_RIGHT, surface, surfaces,
                    config);
            return;
        case KEYBIND_CLIENT_RESIZE_UP:
            ik_handle_resize(IK_RESIZE_UP, surface, surfaces, config);
            return;
        case KEYBIND_CLIENT_RESIZE_DOWN:
            ik_handle_resize(IK_RESIZE_DOWN, surface, surfaces,
                    config);
            return;

        case KEYBIND_NONE:
            LOGGER_TRACE("Ignoring 'KEYBIND_NONE' entry", L_NARG);
            return;
    }
}
