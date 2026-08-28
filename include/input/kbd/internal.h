/**
 * @file input/kbd/internal.h
 *
 * @brief Private cross-file declarations shared across the keyboard
 *        input subsystem
 *
 * Splitting @c src/input/kbd/event.c into the main dispatch (cycle
 * menu, dialog, open-menu, and generic client-action key handling) and
 * the direct client-interaction handlers (program launch, keyboard
 * move, keyboard resize) still leaves one function the latter's own
 * file exposes for the former to call, and vice versa: both need the
 * currently focused client, which is genuinely shared lookup logic, not
 * duplicated per file.
 *
 * @note This header is private to the keyboard input subsystem and must
 *       not be included outside of @c src/input/kbd/, for it is NOT
 *       part of the public API in @c input/kbd/event.h
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_KBD_INTERNAL_H
#define INPUT_KBD_INTERNAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>

/* Local includes */
#include <input/kbd/bind.h>



/**
 * @brief Which program a launch binding starts
 *
 * Narrower than @c wm_keybind_type_e on purpose: @a ik_execute_binding
 * has already decided that the press was a launch before it calls
 * @a ik_handle_launch, so the handler only ever sees these five, and
 * its switch can cover them all without listing every other binding the
 * window manager has.
 */
enum ik_launch_e {
    IK_LAUNCH_TERMINAL,
    IK_LAUNCH_LAUNCHER,
    IK_LAUNCH_FILE_MANAGER,
    IK_LAUNCH_WEB_BROWSER,
    IK_LAUNCH_EDITOR
};

/**
 * @brief How a move binding displaces the focused client
 *
 * Narrower than @c wm_keybind_type_e for the same reason as
 * @c ik_launch_e.
 */
enum ik_move_e {
    IK_MOVE_LEFT,
    IK_MOVE_RIGHT,
    IK_MOVE_UP,
    IK_MOVE_DOWN,
    IK_MOVE_TOP_LEFT,
    IK_MOVE_TOP_RIGHT,
    IK_MOVE_BOTTOM_LEFT,
    IK_MOVE_BOTTOM_RIGHT
};

/**
 * @brief Which edge a resize binding moves
 *
 * Narrower than @c wm_keybind_type_e for the same reason as
 * @c ik_launch_e.
 */
enum ik_resize_e {
    IK_RESIZE_LEFT,
    IK_RESIZE_RIGHT,
    IK_RESIZE_UP,
    IK_RESIZE_DOWN
};

/**
 * @brief Resolve the currently focused client on a surface
 *
 * Looks up the current desktop for @p surface and returns the active
 * client on that desktop.  Optionally returns the owning surface and
 * desktop pointers through @p cs_out and @p cd_out.
 *
 * @param surface  Surface to query
 * @param surfaces Full surface list (for @a lookup_find_client)
 * @param cs_out   Receives the client's owning surface (may be null)
 * @param cd_out   Receives the client's owning desktop (may be null)
 *
 * @return Active client, or @c NULL when none is focused
 *
 * @note Complexity: @e O(n) for the client list walk
 */
client_td *ik_get_active_client(surface_td *surface,
        list_td *surfaces, surface_td **cs_out, desktop_td **cd_out);

/**
 * @brief Launch one of the configured programs
 *
 * Reads the program's path from the configuration and hands it to
 * @a cctl_launch_dispatch.
 *
 * @param program Which program to launch
 * @param surface Current surface, passed to @a cctl_launch_dispatch
 * @param config  Active configuration holding the program paths
 *
 * @note Complexity: @e O(1), aside from the launch itself
 */
void ik_handle_launch(enum ik_launch_e program,
        surface_td *surface, const config_td *config);

/**
 * @brief Move the active client by keyboard
 *
 * @param direction Where to move it, by a step or to a corner
 * @param surface   Current surface
 * @param surfaces  Full surface list, for @a ik_get_active_client
 * @param config    Active configuration holding the move step
 *
 * @note Complexity: @e O(1)
 */
void ik_handle_move(enum ik_move_e direction,
        surface_td *surface, list_td *surfaces,
        const config_td *config);

/**
 * @brief Resize the active client by keyboard
 *
 * @param edge     Which edge to move
 * @param surface  Current surface
 * @param surfaces Full surface list, for @a ik_get_active_client
 * @param config   Active configuration holding the resize step
 *
 * @note Complexity: @e O(1)
 */
void ik_handle_resize(enum ik_resize_e edge,
        surface_td *surface, list_td *surfaces,
        const config_td *config);

/**
 * @brief Let whatever currently owns the keyboard consume the key
 *
 * A modal move or resize, an open cycle menu, the search widget, the
 * run box, a confirm or message dialog, and any open context menu all
 * take every key while they last, in that order of priority.  Asked
 * before the binding table is consulted at all, so that a shortcut
 * cannot fire while one of them is up.
 *
 * @param keysym       Key symbol of the press as the bindings define
 *                     it: the key itself, with no modifier applied
 * @param typed_keysym Symbol that key and its modifiers actually
 *                     produce, which is what a dialog reading text
 *                     wants
 * @param state        Modifier mask of the press, lock bits already
 *                     cleared
 * @param surface      Surface the press belongs to, may be null
 * @param surfaces     Full surface list
 * @param config       Active configuration
 *
 * @return @c true when the key was consumed and must go no further
 *
 * @note Complexity: @e O(n), where @e n is the number of entries in
 *       whichever of them happens to be open
 */
bool ik_intercept_keypress(xcb_keysym_t keysym,
        xcb_keysym_t typed_keysym, uint16_t state,
        surface_td *surface, list_td *surfaces,
        const config_td *config);

/**
 * @brief Find which action a key and modifier combination is bound to
 *
 * Pure over the binding table: it reads no X state, touches nothing,
 * and answers the same for the same arguments, which is what makes the
 * binding logic testable on its own.
 *
 * @param keysym      Key symbol to match
 * @param state       Modifier mask to match, as reported by the X
 *                    server; its lock bits are ignored
 * @param out_modmask Receives the matched binding's own raw modifier
 *                    mask, which some actions need; may be null
 *
 * @return The action bound to the combination, or @c KEYBIND_NONE
 *         when no binding matches
 *
 * @note Complexity: @e O(k), where @e k is the number of configured
 *       bindings
 */
enum wm_keybind_type_e ik_resolve_binding(xcb_keysym_t keysym,
        uint16_t state, uint16_t *out_modmask);

/**
 * @brief Carry out the action a resolved binding names
 *
 * @param wm       Window-manager singleton, for the actions that
 *                 reach beyond one surface
 * @param btype    Action to carry out, as resolved by
 *                 @a ik_resolve_binding
 * @param modmask  Raw modifier mask of the matched binding, which the
 *                 cycle menu needs in order to know which modifier
 *                 release confirms it
 * @param keycode  Keycode of the press, for the same reason
 * @param surface  Surface the press belongs to, may be null
 * @param surfaces Full surface list
 * @param config   Active configuration
 *
 * @note Complexity: depends entirely on the action; @e O(1) for most,
 *       @e O(n) over managed clients for the ones that walk them
 */
void ik_execute_binding(wm_td *wm, enum wm_keybind_type_e btype,
        uint16_t modmask, xcb_keycode_t keycode,
        surface_td *surface, list_td *surfaces,
        const config_td *config);


#endif  /* ! INPUT_KBD_INTERNAL_H */
