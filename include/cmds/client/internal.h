/**
 * @file cmds/client/internal.h
 *
 * @brief Private declarations shared across the client-command modules
 *
 * Low-level XCB, EWMH, and ICCCM plumbing used by more than one
 * client-command module, but never called from outside @c cmds/
 * client/ itself; @c ccmd_target_win, @c ccmd_client_monitor,
 * @c ccmd_client_grab_buttons, @c ccmd_set_wm_state, and @c ccmd_
 * clear_wm_state moved out to the genuinely public @c cmds/client/
 * basic.h once every one of those turned out to already be called
 * from outside this directory too (@c client.c, @c handler/map.c,
 * @c handler/ewmhmsg.c, and @c menu/popup.c among them), which this
 * header's own "must not be included outside of it" promise never
 * actually held for them.
 *
 * @note This header is private to @c cmds/client/ and must not be
 *       included outside of it.
 *
 * @ingroup cmds
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_CLIENT_INTERNAL_H
#define CMDS_CLIENT_INTERNAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Project includes */
#include <client.h>
#include <surface.h>


/* Internal interface */
/**
 * @brief Intern an atom name in the X11 system
 *
 * @param connection Pointer to the X11 connection
 * @param name       Name of the atom
 *
 * @return Interned atom ID, or @c XCB_ATOM_NONE on failure
 *
 * @note Complexity: @e O(n), where @e n is the length of @p name
 */
xcb_atom_t ccmd_intern_atom(xcb_connection_t *connection,
        const char *name);

/**
 * @brief Retrieve the pixel dimensions of the client's current screen
 *
 * Either @p out_w or @p out_h may be null but not both.
 *
 * @param client Pointer to the client whose screen is queried
 * @param out_w  Destination for the screen width in pixels, or null
 * @param out_h  Destination for the screen height in pixels, or null
 *
 * @return @c true on success, @c false on failure
 *
 * @note Complexity: @e O(n), where @e n is the screen index
 */
bool ccmd_screen_dim(client_td *client,
        uint16_t *restrict out_w, uint16_t *restrict out_h);

/**
 * @brief Remove passive button grabs from an undecorated client
 *
 * Releases the passive grab installed by @c ccmd_client_grab_buttons so
 * mouse input flows directly to the client again.
 *
 * @param client Pointer to the client
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_ungrab_buttons(client_td *client);

/**
 * @brief Publish @c _NET_FRAME_EXTENTS on the client window
 *
 * Writes the EWMH @c _NET_FRAME_EXTENTS cardinal property so that
 * taskbars and other clients know the exact size of the decoration
 * added around the content window.  No-op when @p client or its
 * @c ewmh connection is null.
 *
 * @param client Pointer to the client
 * @param left   Left frame extent in pixels
 * @param right  Right frame extent in pixels
 * @param top    Top frame extent in pixels (includes titlebar height)
 * @param bottom Bottom frame extent in pixels
 *
 * @note Complexity: @e O(1)
 */
void ccmd_publish_frame_extents(client_td *client,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom);


/**
 * @brief Transfer focus away from a client that is leaving the current
 *        visible focus chain
 *
 * Thin wrapper resolving @p client's own surface/desktop before
 * deferring to @a client_focus_fallback itself; a no-op unless
 * @p client is genuinely this desktop's own current active client,
 * since some other, already-unfocused client being hidden or
 * iconified has no focus of its own to hand off in the first place.
 *
 * @param client Client that is being hidden or iconified
 *
 * @note Implemented in @c cmds/client/focus.c
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       current desktop
 */
void ccmd_client_focus_fallback(const client_td *client);

/**
 * @brief Create the client's icon window if it does not exist yet, or
 *        reposition the existing one at its saved coordinates
 *
 * A new window is placed either at the client's own remembered
 * @c icon_x/icon_y (if any, and not since claimed by another icon;
 * see @c s_icon_slot_is_taken, private to @c cmds/client/icon.c) or
 * via @c place_icon_apply otherwise, then created with the theme's
 * inactive icon colors.  An already-existing icon window is simply
 * re-configured to its saved position, which may have changed since
 * if the user dragged it.
 *
 * @param client     Client whose icon window to create or reposition
 * @param icon_h_out Icon window height, including the caption band if
 *                   the theme captions icons
 *
 * @note Implemented in @c cmds/client/icon.c
 * @note Complexity: @e O(n), where @e n is the number of already-
 *       iconified clients on the same desktop
 */
void ccmd_client_ensure_icon_window(client_td *client,
        uint16_t icon_h_out);


#endif  /* ! CMDS_CLIENT_INTERNAL_H */
