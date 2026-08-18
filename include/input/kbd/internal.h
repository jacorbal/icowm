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


/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <surface.h>

/* Local includes */
#include <input/kbd/bind.h>


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
 * @brief Launch a configured program for the given binding type
 *
 * Maps each @c KEYBIND_LAUNCH_* constant to its program string from the
 * configuration and calls @a lifecycle_launch_dispatch.
 *
 * @param btype   Keyboard binding type (one of the @c KEYBIND_LAUNCH_*
 *                constants)
 * @param surface Current surface passed to @a lifecycle_launch_dispatch
 * @param config  Active configuration holding the program paths
 */
void ik_handle_launch(enum wm_keybind_type_e btype,
        surface_td *surface, const config_td *config);

/**
 * @brief Handle a keyboard move binding for the active client
 *
 * @param btype    Keyboard binding type (one of the
 *                 @c KEYBIND_CLIENT_MOVE_* constants)
 * @param surface  Current surface
 * @param surfaces Full surface list (for @c ik_get_active_client)
 * @param config   Active configuration holding the move step
 */
void ik_handle_move(enum wm_keybind_type_e btype,
        surface_td *surface, list_td *surfaces,
        const config_td *config);

/**
 * @brief Handle a keyboard resize binding for the active client
 *
 * @param btype    Keyboard binding type (one of the
 *                 @c KEYBIND_CLIENT_RESIZE_* constants)
 * @param surface  Current surface
 * @param surfaces Full surface list (for @a ik_get_active_client)
 * @param config   Active configuration holding the resize step
 */
void ik_handle_resize(enum wm_keybind_type_e btype,
        surface_td *surface, list_td *surfaces,
        const config_td *config);


#endif  /* ! INPUT_KBD_INTERNAL_H */
