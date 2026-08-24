/**
 * @file enact.h
 *
 * @brief Every action this window manager can carry out, one typed
 *        function per action
 *
 * Each @a enact_* function below is the single place in the whole
 * project where its corresponding action actually happens.  A caller
 * anywhere else (a keybinding handler, a menu callback, an EWMH message
 * handler, a rule) calls the matching @a enact_* function directly,
 * with its own typed parameters, instead of reaching into one of the
 * @c cmds/client/ headers or @c cmds/surface.h itself.  Searching for
 * an action's enum name always leads back to exactly one function here.
 *
 * @see @c action.h
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
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>

/* 'action_client_e' */

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
 * @param geom   New frame position and dimensions
 *
 * @note Complexity: @e O(1)
 */
void enact_client_resize(client_td *client, struct geometry_s geom);

/**
 * @brief Resize the client to a specific frame geometry immediately,
 *        bypassing any in-flight sync throttling
 *
 * @see @a ccmd_client_resize_force for when this, rather than
 *      @a enact_client_resize, is the right call to make
 *
 * @param client Client to resize
 * @param geom   New frame position and dimensions
 *
 * @note Complexity: @e O(1)
 */
void enact_client_resize_force(client_td *client, struct geometry_s geom);

/**
 * @brief Move the client to a specific position
 *
 * @param client Client to move
 * @param pos    New position
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move(client_td *client, struct position_s pos);

/**
 * @brief Center the client on its current screen
 *
 * @param client Client to center
 *
 * @note Complexity: @e O(1)
 */
void enact_client_center(client_td *client);

/**
 * @brief Move the client to the monitor north of the current one on
 *        its surface
 *
 * A no-op on a surface with one monitor or none, or when no monitor
 * lies to the north at all; see @a ccmd_client_move_to_monitor_
 * north's comment, cmds/client/geom.h, for the fuller
 * reasoning, including why this never wraps around either.
 *
 * @param client Client to move
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move_monitor_north(client_td *client);

/**
 * @brief Move the client to the monitor south of the current one on
 *        its surface
 *
 * See @a enact_client_move_monitor_north's comment for the
 * fuller reasoning.
 *
 * @param client Client to move
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move_monitor_south(client_td *client);

/**
 * @brief Move the client to the monitor east of the current one on
 *        its surface
 *
 * See @a enact_client_move_monitor_north's comment for the
 * fuller reasoning.
 *
 * @param client Client to move
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move_monitor_east(client_td *client);

/**
 * @brief Move the client to the monitor west of the current one on
 *        its surface
 *
 * See @a enact_client_move_monitor_north's comment for the
 * fuller reasoning.
 *
 * @param client Client to move
 *
 * @note Complexity: @e O(1)
 */
void enact_client_move_monitor_west(client_td *client);

/**
 * @brief Carry the client to the desktop north of the current one,
 *        following it there
 *
 * A silent no-op when there is no different desktop to move to at
 * all (only one exists, wrapping is disabled and this is already
 * the topmost row, or no @c topology.screens.desktops layout is
 * configured at all, so there is no second row in the first place);
 * see @c s_enact_client_send_to_desktop's comment, enact/
 * client.c, for the fuller reasoning.
 *
 * @param client   Client to move
 * @param surfaces Full surface list, passed through to @c focus_apply
 *                 so this client, not whichever one the target
 *                 desktop's own switch just restored on its own,
 *                 ends up genuinely focused there
 * @param config   Active configuration, passed through to
 *                 @c focus_apply
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's own top parent's own desktop
 */
void enact_client_send_to_desktop_north(client_td *client,
        list_td *surfaces, const config_td *config);

/**
 * @brief Carry the client to the desktop south of the current one,
 *        following it there
 *
 * A silent no-op when there is no different desktop to move to at
 * all (only one exists, wrapping is disabled and this is already
 * the bottommost row, or no @c topology.screens.desktops layout is
 * configured at all, so there is no second row in the first place);
 * see @c s_enact_client_send_to_desktop's comment, enact/
 * client.c, for the fuller reasoning.
 *
 * @param client   Client to move
 * @param surfaces Full surface list, passed through to @c focus_apply
 *                 so this client, not whichever one the target
 *                 desktop's own switch just restored on its own,
 *                 ends up genuinely focused there
 * @param config   Active configuration, passed through to
 *                 @c focus_apply
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's own top parent's own desktop
 */
void enact_client_send_to_desktop_south(client_td *client,
        list_td *surfaces, const config_td *config);

/**
 * @brief Carry the client to the desktop east of the current one,
 *        following it there
 *
 * A silent no-op when there is no different desktop to move to at
 * all (only one exists, or wrapping is disabled and this is already
 * the eastmost one in its own row); see @c s_enact_client_send_to_
 * desktop's comment, enact/client.c, for the fuller
 * reasoning.
 *
 * @param client   Client to move
 * @param surfaces Full surface list, passed through to @c focus_apply
 *                 so this client, not whichever one the target
 *                 desktop's own switch just restored on its own,
 *                 ends up genuinely focused there
 * @param config   Active configuration, passed through to
 *                 @c focus_apply
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's own top parent's own desktop
 */
void enact_client_send_to_desktop_east(client_td *client,
        list_td *surfaces, const config_td *config);

/**
 * @brief Carry the client to the desktop west of the current one,
 *        following it there
 *
 * A silent no-op when there is no different desktop to move to at
 * all (only one exists, or wrapping is disabled and this is already
 * the westmost one in its own row); see @c s_enact_client_send_to_
 * desktop's comment, enact/client.c, for the fuller
 * reasoning.
 *
 * @param client   Client to move
 * @param surfaces Full surface list, passed through to @c focus_apply
 *                 so this client, not whichever one the target
 *                 desktop's own switch just restored on its own,
 *                 ends up genuinely focused there
 * @param config   Active configuration, passed through to
 *                 @c focus_apply
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's own top parent's own desktop
 */
void enact_client_send_to_desktop_west(client_td *client,
        list_td *surfaces, const config_td *config);

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
 * @brief Change the client's @c WM_CLASS class and instance names
 *
 * @param client        Client to reclassify
 * @param class_name    New @c WM_CLASS class name
 * @param instance_name New @c WM_CLASS instance name
 *
 * @note Complexity: @e O(1)
 */
void enact_client_reclass(client_td *client,
        const char *restrict class_name,
        const char *restrict instance_name);

/**
 * @brief Change the client's @c WM_WINDOW_ROLE
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
 * @brief Set the client's pin mode
 *
 * @param client Client to pin
 *
 * @note Complexity: @e O(1)
 */
void enact_client_pin(client_td *client);

/**
 * @brief Remove the client's pin mode
 *
 * @param client Client to unstick
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unpin(client_td *client);

/**
 * @brief Toggle the client's pin mode
 *
 * @param client Client to toggle
 *
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_pin(client_td *client);

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
void enact_client_urge(client_td *client);

/**
 * @brief Clear the client's urgency level
 *
 * @param client Client to clear
 *
 * @note Complexity: @e O(1)
 */
void enact_client_unurge(client_td *client);

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
void enact_client_toggle_decorate(client_td *client);

/* 'action_desktop_e' */

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
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       surface
 */
void enact_desktop_show(desktop_td *desktop, bool show);

/**
 * @brief Send a client from one desktop to another
 *
 * Never switches the surface's own currently viewed desktop, nor
 * forces real keyboard focus onto @p client immediately: a menu- or
 * keybind-driven "send to desktop" that does not also follow is a
 * "file this away" gesture, not "take me there", matching Openbox's
 * own equivalent (@c client_set_desktop, client.c).  @p client does
 * become @p target's own remembered active client when focusable,
 * though (see @a s_enact_desktop_client_send_one's comment,
 * enact/desktop.c, for the fuller reasoning), so it is what greets
 * whoever visits @p target next, rather than requiring @p target to
 * have already had some other active client remembered on it before
 * this arrived, or for this to somehow already be the one currently
 * viewed, to be found there.
 *
 * @param desktop Desktop the client currently lives on
 * @param client  Client to send
 * @param target  Destination desktop
 *
 * @note Complexity: @e O(1)
 */
void enact_desktop_client_send(const desktop_td *desktop,
        client_td *client, desktop_td *target);

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
 * A transient dialog among them is the one exception.  It is
 * re-centered over its own parent per ICCCM §4.1.2.6 instead of being
 * run through the configured policy, since every client goes through
 * @a place_window_apply itself, the same general placement engine
 * a window is run through when first mapped, not a simplified
 * rearrange-only routine.
 *
 * @param wm      Window manager instance (needed to locate a
 *                transient's parent, which can live on a different
 *                surface, via @p wm->surfaces)
 * @param surface Surface the desktop belongs to
 * @param desktop Desktop whose clients are rearranged
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 *
 * @see @a place_window_apply
 */
void enact_desktop_clients_rearrange(const wm_td *wm,
        surface_td *surface, desktop_td *desktop);

/**
 * @brief Iconify every client on the desktop
 *
 * @param desktop Desktop whose clients are iconified
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_clients_iconify_all(desktop_td *desktop);

/**
 * @brief Restore every iconified client on a desktop
 *
 * @param desktop Desktop whose iconified clients are restored
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_clients_deiconify_all(desktop_td *desktop);

/**
 * @brief Cycle input focus to the next non-iconified client
 *
 * Opens the cycle menu preselecting the next entry, and repaints it
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the menu
 * @param desktop    Desktop whose client list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_cycle_clients_active(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg);

/**
 * @brief Cycle input focus to the previous non-iconified client
 *
 * Opens the cycle menu preselecting the previous entry, and repaints it
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the menu
 * @param desktop    Desktop whose client list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_cycle_clients_prev(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg);

/**
 * @brief Cycle input focus to the next iconified client
 *
 * Opens the cycle menu listing icons and preselecting the next entry,
 * and repaints it
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the menu
 * @param desktop    Desktop whose icon list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_cycle_clients_icons_next(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg);

/**
 * @brief Cycle input focus to the previous iconified client
 *
 * Opens the cycle menu listing icons and preselecting the previous
 * entry, and repaints it
 *
 * @param connection XCB connection
 * @param surface    Surface on which to center the menu
 * @param desktop    Desktop whose icon list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_cycle_clients_icons_prev(xcb_connection_t *connection,
        surface_td *surface, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg);

/* 'action_surface_e' */

/**
 * @brief Switch the surface to a specific desktop
 *
 * @param surface    Surface to switch
 * @param desktop_id Target desktop index
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_surface_desktop_switch(surface_td *surface,
        uint32_t desktop_id);

/**
 * @brief Switch the surface to the desktop north of the current
 *        one, in cyclic order
 *
 * @param surface Surface to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_surface_desktop_switch_north(surface_td *surface);

/**
 * @brief Switch the surface to the desktop south of the current
 *        one, in cyclic order
 *
 * @param surface Surface to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_surface_desktop_switch_south(surface_td *surface);

/**
 * @brief Switch the surface to the desktop east of the current
 *        one, in cyclic order
 *
 * @param surface Surface to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_surface_desktop_switch_east(surface_td *surface);

/**
 * @brief Switch the surface to the desktop west of the current
 *        one, in cyclic order
 *
 * @param surface Surface to switch
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktops involved
 */
void enact_surface_desktop_switch_west(surface_td *surface);

/**
 * @brief Add a new, empty desktop to the end of the surface's own
 *        desktop list
 *
 * @param surface Surface to add a desktop to
 *
 * @note Complexity: @e O(1)
 */
void enact_surface_desktop_add(surface_td *surface);

/**
 * @brief Remove the surface's own last desktop, moving any client
 *        still on it to the new last desktop first
 *
 * A no-op, silently, when only one desktop remains: see
 * @a surface_action_desktop_remove (surface.h) for the exact refusal
 * conditions.
 *
 * @param surface Surface to remove the last desktop from
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop being removed
 */
void enact_surface_desktop_remove(surface_td *surface);

/**
 * @brief Toggle whether panel/tray struts are set aside when
 *        computing this surface's own desktops' work areas
 *
 * @param surface Surface to toggle strutless-maximization mode on
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       @p surface
 */
void enact_surface_toggle_strutless_maximize(surface_td *surface);

/* 'action_wm_e' */

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
 * @param wm Window manager instance
 *
 * @return @c 0 on success, non-zero otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the
 *       configuration being reloaded
 */
int enact_wm_configuration_reload(const wm_td *wm);


#endif  /* ! ENACT_H */
