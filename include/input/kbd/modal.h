/**
 * @file input/kbd/modal.h
 *
 * @brief Keyboard modal move and resize declarations
 *
 * @ingroup input Input subsystem
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_KBD_MODAL_H
#define INPUT_KBD_MODAL_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <render/surface.h>


/* Keysym constants (X11 keyboard symbols) */
#define KS_LEFT 0xff51u
#define KS_UP 0xff52u
#define KS_RIGHT 0xff53u
#define KS_DOWN 0xff54u
#define KS_RETURN 0xff0du
#define KS_KP_ENTER 0xff8du
#define KS_ESCAPE 0xff1bu


/* Public interface */

/**
 * @brief Check whether a keyboard modal mode (move or resize) is active
 *
 * @return @c true when the WM is in an interactive keyboard move or
 *         resize session, @c false otherwise
 *
 * @note Complexity: @e O(1)
 */
bool kbd_modal_is_active(void);

/**
 * @brief Enter keyboard modal move mode for the given client
 *
 * Saves the client's current position, grabs the keyboard on the root
 * window, and enters move mode.  While active, arrow keys move the
 * window, @c Return confirms, and @c Escape cancels and restores the
 * original position.
 *
 * @param connection XCB connection
 * @param surface    Surface the client belongs to (used for root window)
 * @param client     Client to move
 *
 * @note Complexity: @e O(1)
 */
void kbd_modal_move_start(xcb_connection_t *connection,
        surface_td *surface, client_td *client);

/**
 * @brief Enter keyboard modal resize mode for the given client
 *
 * Saves the client's current geometry, grabs the keyboard on the root
 * window, and enters resize mode.  The first arrow key press determines
 * the active edge; subsequent presses grow or shrink along that edge.
 * @c Return confirms and @c Escape cancels, restoring the original size.
 *
 * @param connection XCB connection
 * @param surface    Surface the client belongs to (used for root window)
 * @param client     Client to resize
 *
 * @note Complexity: @e O(1)
 */
void kbd_modal_resize_start(xcb_connection_t *connection,
        surface_td *surface, client_td *client);

/**
 * @brief Dispatch a key press while a keyboard modal session is active
 *
 * Applies the appropriate move or resize step for arrow keys, confirms
 * the operation on @c Return or @c KP_Enter, and cancels it on
 * @c Escape.  All keys are consumed while modal mode is active.
 *
 * @param connection XCB connection (may be @c NULL; saved connection is
 *                   used as fallback)
 * @param surface    Current surface (used for move / resize step config)
 * @param keysym     X keysym of the pressed key
 * @param config     Active configuration (for move and resize step sizes)
 *
 * @return @c true (the key is always consumed while modal is active)
 *
 * @note Complexity: @e O(1)
 */
bool kbd_modal_handle_keypress(xcb_connection_t *connection,
        surface_td *surface, xcb_keysym_t keysym,
        const config_td *config);


#endif  /* ! INPUT_KBD_MODAL_H */
