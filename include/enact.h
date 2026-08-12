/**
 * @file enact.h
 *
 * @brief Every action this window manager can carry out, one typed
 *        function per action
 *
 * Each @c enact_* function below is the single place in the whole
 * project where its corresponding action actually happens.  A caller
 * anywhere else (a keybinding handler, a menu callback, an EWMH
 * message handler, a rule) calls the matching @c enact_* function
 * directly, with its own typed parameters, instead of reaching into
 * one of the @c cmds/client/ headers or @c cmds/surface.h itself.  Searching
 * for an action's enum name (see @c action.h) always leads back to
 * exactly one function here.
 *
 * @defgroup enact Action execution
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef ENACT_H
#define ENACT_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>


/* == action_client_e == */

/**
 * @brief Close the client's window
 *
 * @param client Client to close
 *
 * @note Complexity: @e O(1)
 */
void enact_client_close(client_td *client);

/**
 * @brief Forcibly kill the client's owning connection
 *
 * @param client Client to kill
 *
 * @note Complexity: @e O(1)
 */
void enact_client_kill(client_td *client);

/**
 * @brief Restore the client to its normal state
 *
 * @param client Client to restore
 *
 * @note Complexity: @e O(1)
 */
void enact_client_restore(client_td *client);

/**
 * @brief Give input focus to the client
 *
 * @param client Client to focus
 *
 * @note Complexity: @e O(1)
 */
void enact_client_focus(client_td *client);

/**
 * @brief Take input focus away from the client
 *
 * @param client Client to unfocus
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unfocus(client_td *client);

/**
 * @brief Resize the client to a specific frame geometry
 *
 * @param client Client to resize
 * @param x      New frame X position
 * @param y      New frame Y position
 * @param w      New frame width
 * @param h      New frame height
 *
 * @note Complexity: @e O(1)
 */
void enact_client_resize(client_td *client, int32_t x, int32_t y,
        uint32_t w, uint32_t h);

/**
 * @brief Move the client to a specific position
 *
 * @param client Client to move
 * @param x      New X position
 * @param y      New Y position
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move(client_td *client, int32_t x, int32_t y);

/**
 * @brief Center the client on its current screen
 *
 * @param client Client to center
 *
 * @note Complexity: @e O(1)
 */
void enact_client_center(client_td *client);

/**
 * @brief Move the client to the next monitor on its surface
 *
 * @param client Client to move
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move_next_monitor(client_td *client);

/**
 * @brief Move the client to a specific monitor index
 *
 * @param client        Client to move
 * @param monitor_index Target monitor index on the client's surface
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move_to_monitor(client_td *client,
        uint32_t monitor_index);

/**
 * @brief Change the client's 'WM_CLASS' class and instance names
 *
 * @param client        Client to reclassify
 * @param class_name    New 'WM_CLASS' class name
 * @param instance_name New 'WM_CLASS' instance name
 *
 * @note Complexity: @e O(1)
 */
void enact_client_reclass(client_td *client, const char *class_name,
        const char *instance_name);

/**
 * @brief Change the client's 'WM_WINDOW_ROLE'
 *
 * @param client Client to change
 * @param role   New role string
 *
 * @note Complexity: @e O(1)
 */
void enact_client_rerole(client_td *client, const char *role);

/**
 * @brief Rename the client's window title
 *
 * @param client Client to rename
 * @param name   New title
 *
 * @note Complexity: @e O(1)
 */
void enact_client_rename(client_td *client, const char *name);

/**
 * @brief Maximize the client both horizontally and vertically
 *
 * @param client Client to maximize
 *
 * @note Complexity: @e O(1)
 */
void enact_client_maximize(client_td *client);

/**
 * @brief Maximize the client horizontally only
 *
 * @param client Client to maximize
 *
 * @note Complexity: @e O(1)
 */
void enact_client_maximize_horz(client_td *client);

/**
 * @brief Maximize the client vertically only
 *
 * @param client Client to maximize
 *
 * @note Complexity: @e O(1)
 */
void enact_client_maximize_vert(client_td *client);

/**
 * @brief Iconify the client
 *
 * @param client Client to iconify
 *
 * @note Complexity: @e O(1)
 */
void enact_client_iconify(client_td *client);

/**
 * @brief Hide the client's window
 *
 * @param client Client to hide
 *
 * @note Complexity: @e O(1)
 */
void enact_client_hide(client_td *client);

/**
 * @brief Show a previously hidden client
 *
 * @param client Client to show
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unhide(client_td *client);

/**
 * @brief Shade (roll up) the client
 *
 * @param client Client to shade
 *
 * @note Complexity: @e O(1)
 */
void enact_client_shade(client_td *client);

/**
 * @brief Unshade (roll down) the client
 *
 * @param client Client to unshade
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unshade(client_td *client);

/**
 * @brief Toggle the client's shade status
 *
 * @param client Client to toggle
 *
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_shade(client_td *client);

/**
 * @brief Set the client's sticky mode
 *
 * @param client Client to set sticky
 *
 * @note Complexity: @e O(1)
 */
void enact_client_sticky(client_td *client);

/**
 * @brief Remove the client's sticky mode
 *
 * @param client Client to unstick
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unsticky(client_td *client);

/**
 * @brief Toggle the client's sticky mode
 *
 * @param client Client to toggle
 *
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_sticky(client_td *client);

/**
 * @brief Set the client to full screen mode
 *
 * @param client Client to make full screen
 *
 * @note Complexity: @e O(1)
 */
void enact_client_fullscreen(client_td *client);

/**
 * @brief Remove the client's full screen mode
 *
 * @param client Client to restore
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unfullscreen(client_td *client);

/**
 * @brief Toggle the client's full screen mode
 *
 * @param client Client to toggle
 *
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_fullscreen(client_td *client);

/**
 * @brief Raise the client to the top of its layer
 *
 * @param client Client to raise
 *
 * @note Complexity: @e O(1)
 */
void enact_client_raise(client_td *client);

/**
 * @brief Lower the client to the bottom of its layer
 *
 * @param client Client to lower
 *
 * @note Complexity: @e O(1)
 */
void enact_client_lower(client_td *client);

/**
 * @brief Move the client to the always-on-top layer
 *
 * @param client Client to move
 *
 * @note Complexity: @e O(1)
 */
void enact_client_layer_above(client_td *client);

/**
 * @brief Move the client to the normal layer
 *
 * @param client Client to move
 *
 * @note Complexity: @e O(1)
 */
void enact_client_layer_normal(client_td *client);

/**
 * @brief Move the client to the always-on-bottom layer
 *
 * @param client Client to move
 *
 * @note Complexity: @e O(1)
 */
void enact_client_layer_below(client_td *client);

/**
 * @brief Cycle the client through the normal/above/below layers
 *
 * @param client Client to cycle
 *
 * @note Complexity: @e O(1)
 */
void enact_client_cycle_layer(client_td *client);

/**
 * @brief Mark the client as urgent
 *
 * @param client Client to mark
 *
 * @note Complexity: @e O(1)
 */
void enact_client_set_urgent(client_td *client);

/**
 * @brief Clear the client's urgency level
 *
 * @param client Client to clear
 *
 * @note Complexity: @e O(1)
 */
void enact_client_clear_urgent(client_td *client);

/**
 * @brief Set the client's icon name
 *
 * @param client    Client to update
 * @param icon_name New icon name
 *
 * @note Complexity: @e O(1)
 */
void enact_client_set_icon(client_td *client, const char *icon_name);

/**
 * @brief Toggle the client's decoration
 *
 * @param client Client to toggle
 *
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_decoration(client_td *client);


/* == action_desktop_e == */

/**
 * @brief Set the desktop's background color
 *
 * @param desktop Desktop to update
 * @param color   New background color, as a packed pixel value
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_set_background(desktop_td *desktop, uint32_t color);

/**
 * @brief Toggle whether the desktop's own surface shows the desktop
 *
 * Hides every mapped client on the surface so the desktop background
 * becomes visible, or restores them, mirroring @c _NET_SHOWING_DESKTOP
 *
 * @param desktop Desktop whose surface is toggled
 * @param show    @c true to hide clients and show the desktop,
 *                @c false to restore them
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the surface
 */
void enact_desktop_show(desktop_td *desktop, bool show);

/**
 * @brief Add a client to the desktop
 *
 * @param desktop Desktop to add to
 * @param client  Client to add
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_client_add(desktop_td *desktop, client_td *client);

/**
 * @brief Remove a client from the desktop
 *
 * @param desktop Desktop to remove from
 * @param client  Client to remove
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_client_remove(desktop_td *desktop, client_td *client);

/**
 * @brief Send a client from one desktop to another
 *
 * @param desktop Desktop the client currently lives on
 * @param client  Client to send
 * @param target  Destination desktop
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_client_send(desktop_td *desktop, client_td *client,
        desktop_td *target);

/**
 * @brief Send a client to the front of the desktop's window stack
 *
 * @param desktop Desktop the client lives on
 * @param client  Client to send to the front
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_client_send_front(desktop_td *desktop,
        client_td *client);

/**
 * @brief Send a client to the back of the desktop's window stack
 *
 * @param desktop Desktop the client lives on
 * @param client  Client to send to the back
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_client_send_back(desktop_td *desktop,
        client_td *client);

/**
 * @brief Re-apply the configured placement policy to every client on
 *        the desktop
 *
 * @param wm      Window manager instance
 * @param surface Surface the desktop belongs to
 * @param desktop Desktop whose clients are rearranged
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
void enact_desktop_clients_rearrange(wm_td *wm, surface_td *surface,
        desktop_td *desktop);

/**
 * @brief Iconify every client on the desktop
 *
 * @param desktop Desktop whose clients are iconified
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
void enact_desktop_clients_iconify_all(desktop_td *desktop);

/**
 * @brief Restore every iconified client on a desktop
 *
 * @param desktop Desktop whose iconified clients are restored
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
void enact_desktop_clients_deiconify_all(desktop_td *desktop);

/**
 * @brief Cycle input focus to the next non-iconified client
 *
 * Opens the cycle menu preselecting the next entry, and repaints it
 *
 * @param surfaces   All managed surfaces
 * @param connection XCB connection
 * @param surface    Surface on which to center the menu
 * @param desktop    Desktop whose client list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
void enact_desktop_cycle_clients_active(list_td *surfaces,
        xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, uint16_t modifier, const config_td *cfg);

/**
 * @brief Cycle input focus to the previous non-iconified client
 *
 * Opens the cycle menu preselecting the previous entry, and repaints it
 *
 * @param surfaces   All managed surfaces
 * @param connection XCB connection
 * @param surface    Surface on which to center the menu
 * @param desktop    Desktop whose client list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
void enact_desktop_cycle_clients_prev(list_td *surfaces,
        xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, uint16_t modifier, const config_td *cfg);

/**
 * @brief Cycle input focus to the next iconified client
 *
 * Opens the cycle menu listing icons and preselecting the next entry,
 * and repaints it
 *
 * @param surfaces   All managed surfaces
 * @param connection XCB connection
 * @param surface    Surface on which to center the menu
 * @param desktop    Desktop whose icon list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
void enact_desktop_cycle_clients_icons_next(list_td *surfaces,
        xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, uint16_t modifier, const config_td *cfg);

/**
 * @brief Cycle input focus to the previous iconified client
 *
 * Opens the cycle menu listing icons and preselecting the previous
 * entry, and repaints it
 *
 * @param surfaces   All managed surfaces
 * @param connection XCB connection
 * @param surface    Surface on which to center the menu
 * @param desktop    Desktop whose icon list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
void enact_desktop_cycle_clients_icons_prev(list_td *surfaces,
        xcb_connection_t *connection, surface_td *surface,
        desktop_td *desktop, uint16_t modifier, const config_td *cfg);

/**
 * @brief Launch a program associated with the desktop
 *
 * @param desktop Desktop the program is launched for
 * @param command Shell command to launch
 *
 * @return Process ID of the launched command, or @c -1 on failure
 *
 * @note Complexity: @e O(1)
 */
pid_t enact_desktop_command_launch(desktop_td *desktop,
        const char *command);


/* == action_surface_e == */

/**
 * @brief Switch the surface to a specific desktop
 *
 * @param surface    Surface to switch
 * @param desktop_id Target desktop index
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktops involved
 */
void enact_surface_desktop_switch(surface_td *surface,
        uint32_t desktop_id);

/**
 * @brief Switch the surface to the next desktop, in cyclic order
 *
 * @param surface Surface to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktops involved
 */
void enact_surface_desktop_switch_next(surface_td *surface);

/**
 * @brief Switch the surface to the previous desktop, in cyclic order
 *
 * @param surface Surface to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktops involved
 */
void enact_surface_desktop_switch_prev(surface_td *surface);


/* == action_wm_e == */

/**
 * @brief Request that the window manager stop and exit
 *
 * @return @c 0 on success, non-zero otherwise
 *
 * @note Complexity: @e O(1)
 */
int enact_wm_exit(void);

/**
 * @brief Reload the window manager's configuration
 *
 * @return @c 0 on success, non-zero otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the
 *       configuration being reloaded
 */
int enact_wm_configuration_reload(void);


#endif  /* ! ENACT_H */
