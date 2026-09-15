/**
 * @file enact/client.h
 *
 * @brief Every action this window manager can carry out on a single
 *        client, one typed function per action
 *
 * Split out of @c enact.h, alongside @c enact/desktop.h and
 * @c enact/surface.h, so a file that only needs client actions does
 * not also pull in, and rebuild against, every desktop and surface
 * action declared alongside it.
 *
 * Each @a enact_client_* function below is the single place in the
 * whole project where its corresponding action actually happens.
 * A caller anywhere else (a keybinding handler, a menu callback, an
 * EWMH message handler, a rule) calls the matching @a enact_client_*
 * function directly, with its typed parameters, instead of reaching
 * into one of the @c cmds/client/ headers itself.  Searching for an
 * action's enum name always leads back to exactly one function here.
 *
 * @see @c action.h
 * @see @c enact.h
 *
 * @defgroup enact_client Client action execution
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef ENACT_CLIENT_H
#define ENACT_CLIENT_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>


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
 * lies to the north at all.  See
 * @a ccmd_client_move_to_monitor_north in @c cmds/client/geom.h for
 * the fuller
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
 *                 desktop's switch just restored on its own,
 *                 ends up genuinely focused there
 * @param config   Active configuration, passed through to
 *                 @c focus_apply
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's top parent's desktop
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
 *                 desktop's switch just restored on its own,
 *                 ends up genuinely focused there
 * @param config   Active configuration, passed through to
 *                 @c focus_apply
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's top parent's desktop
 */
void enact_client_send_to_desktop_south(client_td *client,
        list_td *surfaces, const config_td *config);

/**
 * @brief Carry the client to the desktop east of the current one,
 *        following it there
 *
 * A silent no-op when there is no different desktop to move to at
 * all (only one exists, or wrapping is disabled and this is already
 * the eastmost one in its row).  See
 * @a s_enact_client_send_to_desktop in @c enact/client.c for the
 * fuller
 * reasoning.
 *
 * @param client   Client to move
 * @param surfaces Full surface list, passed through to @c focus_apply
 *                 so this client, not whichever one the target
 *                 desktop's switch just restored on its own,
 *                 ends up genuinely focused there
 * @param config   Active configuration, passed through to
 *                 @c focus_apply
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's top parent's desktop
 */
void enact_client_send_to_desktop_east(client_td *client,
        list_td *surfaces, const config_td *config);

/**
 * @brief Carry the client to the desktop west of the current one,
 *        following it there
 *
 * A silent no-op when there is no different desktop to move to at
 * all (only one exists, or wrapping is disabled and this is already
 * the westmost one in its row).  See
 * @a s_enact_client_send_to_desktop in @c enact/client.c for the
 * fuller
 * reasoning.
 *
 * @param client   Client to move
 * @param surfaces Full surface list, passed through to @c focus_apply
 *                 so this client, not whichever one the target
 *                 desktop's switch just restored on its own,
 *                 ends up genuinely focused there
 * @param config   Active configuration, passed through to
 *                 @c focus_apply
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the client's top parent's desktop
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
 * @param client Client to unpin
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
 * @brief Set the client's sticky mode
 *
 * Not to be confused with @a enact_client_pin above; see
 * @c CLIENT_FLAG_STICKY's comment in @c client/state.h for the full
 * distinction between the two
 *
 * @param client Client to stick
 *
 * @note Does not broadcast an IPC event the way the pin, shade, and
 *       fullscreen setters above do; see @a enact_client_toggle_stick
 *       below for why
 * @note Complexity: @e O(1)
 */
void enact_client_stick(client_td *client);

/**
 * @brief Remove the client's sticky mode
 *
 * @param client Client to unstick
 *
 * @note Does not broadcast an IPC event; see @a enact_client_toggle_stick
 *       below for why
 * @note Complexity: @e O(1)
 */
void enact_client_unstick(client_td *client);

/**
 * @brief Toggle the client's sticky mode
 *
 * Not to be confused with @a enact_client_toggle_pin above; see
 * @c CLIENT_FLAG_STICKY's comment in @c client/state.h for the full
 * distinction between the two
 *
 * @param client Client to toggle
 *
 * @note Does not broadcast an IPC event the way the pin, shade, and
 *       fullscreen toggles above do: every bit of the 32-bit
 *       @c IPC_EVENT_* mask (@c ipc.h) is already spoken for, with
 *       none free for a new @c IPC_EVENT_STICK_SET/@c _CLEARED pair
 * @note Complexity: @e O(1)
 */
void enact_client_toggle_stick(client_td *client);

/**
 * @brief Move a client to a given page of its desktop's viewport
 *
 * Not to be confused with @a enact_client_send_to_desktop, which moves
 * a client to a different desktop entirely; this only moves it within
 * the current desktop's own pannable canvas.
 *
 * @param surface Surface whose current desktop owns @p client
 * @param client  Client to move
 * @param col     Zero-based destination column
 * @param row     Zero-based destination row
 *
 * @note A no-op on a sticky client, which is on screen from every
 *       origin and so belongs to no one page
 * @note Does not broadcast an IPC event, for the same reason
 *       @a enact_client_toggle_stick does not
 * @note Complexity: @e O(1)
 */
void enact_client_send_to_page(surface_td *surface, client_td *client,
        uint32_t col, uint32_t row);

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


#endif  /* ! ENACT_CLIENT_H */
