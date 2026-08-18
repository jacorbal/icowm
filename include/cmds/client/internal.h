/**
 * @file cmds/client/internal.h
 *
 * @brief Private declarations shared across the client-command modules
 *
 * Low-level XCB, EWMH, and ICCCM plumbing used by the client-command
 * modules (@c basic, @c geom, @c layer, @c meta, @c state), implemented
 * across @c ewmh.c, @c screen.c, and @c grab.c.
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


#define CCMD_WM_STATE_WITHDRAWN (0u)
#define CCMD_WM_STATE_NORMAL (1u)
#define CCMD_WM_STATE_ICONIC (3u)


/* Internal interface */
/**
 * @brief Retrieve the ID of the currently active window for a screen
 *
 * @param ewmh      Pointer to the EWMH connection
 * @param screen_id Screen identifier
 *
 * @return ID of the active window, or @c XCB_WINDOW_NONE on error
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t ccmd_active_win(xcb_ewmh_connection_t *ewmh,
        uint32_t screen_id);

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
 * @brief Return the frame window when decorated, otherwise the client
 *        window
 *
 * @param client Pointer to the client to inspect
 *
 * @return Frame window when available, client window otherwise;
 *         @c XCB_WINDOW_NONE if @p client is @c NULL
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t ccmd_target_win(client_td *client);

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
 * @brief Find which monitor a client is currently on
 *
 * Resolves @p client's surface from the global @c wm singleton, then
 * finds whichever of that surface's monitors @p client's own center
 * point currently falls on.
 *
 * @param client      Client to resolve a monitor for
 * @param out_surface Receives the resolved surface (may be @c NULL)
 * @param out_monitor Receives the resolved monitor's raw geometry
 *                     (screen edges, not adjusted for panel/dock
 *                     struts)
 *
 * @return @c true on success, @c false if the client's surface could
 *         not be found; callers fall back to @c ccmd_screen_dim's raw
 *         screen size in that case
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
bool ccmd_client_monitor(client_td *client, surface_td **out_surface,
        monitor_td *out_monitor);

/**
 * @brief Passively grab all mouse buttons on an undecorated client
 *
 * Installs a synchronous passive grab on the client window so the
 * window manager can focus the client on click before replaying or
 * consuming the button event.
 *
 * @param client Pointer to the client
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_grab_buttons(client_td *client);

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
 * @brief Write the ICCCM @c WM_STATE property for a client
 *
 * Stores the client state and optional icon window in the legacy
 * @c WM_STATE property expected by pagers, taskbars, and older X11
 * clients.
 *
 * @param client      Pointer to the client
 * @param state       ICCCM window-manager state value
 * @param icon_window Icon window associated with @p state, or
 *                    @c XCB_NONE
 *
 * @note Complexity: @e O(n), where @e n is the length of @c WM_STATE
 */
void ccmd_set_wm_state(client_td *client,
        uint32_t state, xcb_window_t icon_window);

/**
 * @brief Remove the ICCCM @c WM_STATE property from a client
 *
 * Deletes the legacy @c WM_STATE property, typically when the client is
 * being withdrawn from window-manager control.
 *
 * @param client Pointer to the client
 *
 * @note Complexity: @e O(n), where @e n is the length of @c WM_STATE
 */
void ccmd_clear_wm_state(client_td *client);

/**
 * @brief Add multiple EWMH window states to a client
 *
 * @param client     Pointer to the client
 * @param num_states Number of state name strings that follow
 * @param ...        @c (const char*) state name arguments
 *
 * @note Complexity: @e O(n), where @e n is @p num_states
 */
void ccmd_add_states(client_td *client, uint32_t num_states, ...);

/**
 * @brief Remove multiple EWMH window states from a client
 *
 * @param client     Pointer to the client
 * @param num_states Number of state name strings that follow
 * @param ...        @c (const char*) state name arguments
 *
 * @note Complexity: @e O(n * m), where @e n is @p num_states and @e m
 *       is the current number of window states
 */
void ccmd_rem_states(client_td *client, uint32_t num_states, ...);

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
void ccmd_client_focus_fallback(client_td *client);

/**
 * @brief Create the client's icon window if it does not exist yet, or
 *        reposition the existing one at its saved coordinates
 *
 * A new window is placed either at the client's own remembered
 * @c icon_x/icon_y (if any, and not since claimed by another icon;
 * see @c s_icon_slot_is_taken, private to @c cmds/client/icon.c) or
 * via @c place_icon otherwise, then created with the theme's inactive
 * icon colors.  An already-existing icon window is simply
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
