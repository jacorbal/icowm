/**
 * @file enact/desktop.h
 *
 * @brief Every action this window manager can carry out on a single
 *        desktop, one typed function per action
 *
 * Split out of @c enact.h, alongside @c enact/client.h and
 * @c enact/stage.h, so a file that only needs desktop actions does
 * not also pull in, and rebuild against, every client and stage
 * action declared alongside it.
 *
 * Each @a enact_desktop_* function below is the single place in the
 * whole project where its corresponding action actually happens.
 * A caller anywhere else (a keybinding handler, a menu callback, an
 * EWMH message handler, a rule) calls the matching @a enact_desktop_*
 * function directly, with its typed parameters, instead of reaching
 * into @c cmds/stage.h itself.  Searching for an action's enum name
 * always leads back to exactly one function here.
 *
 * @see @c action.h
 * @see @c enact.h
 *
 * @defgroup enact_desktop Desktop action execution
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef ENACT_DESKTOP_H
#define ENACT_DESKTOP_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


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
 * @brief Toggle whether the desktop's stage shows the desktop
 *
 * Hides every mapped client on the stage so the desktop background
 * becomes visible, or restores them, mirroring @c _NET_SHOWING_DESKTOP.
 *
 * @param desktop Desktop whose stage is toggled
 * @param show    @c true to hide clients and show the desktop,
 *                @c false to restore them
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       stage
 */
void enact_desktop_show(desktop_td *desktop, bool show);

/**
 * @brief Send a client from one desktop to another
 *
 * Never switches the stage's currently viewed desktop, nor
 * forces real keyboard focus onto @p client immediately: a menu- or
 * keybind-driven "send to desktop" that does not also follow is a
 * "file this away" gesture, not "take me there", matching Openbox's
 * own equivalent (@c client_set_desktop, client.c).
 *
 * @p client does become @p target's remembered active client when
 * focusable, though (see @a s_enact_desktop_client_send_one's comment
 * in @c enact/desktop.c, for the fuller reasoning), so it is what
 * greets whoever visits @p target next, rather than requiring @p target
 * to have already had some other active client remembered on it before
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
 * re-centered over its parent per ICCCM §4.1.2.6 instead of being
 * run through the configured policy, since every client goes through
 * @a place_window_apply itself, the same general placement engine
 * a window is run through when first mapped, not a simplified
 * rearrange-only routine.
 *
 * @param wm Window manager instance (needed to locate a
 *                transient's parent, which can live on a different
 *                stage, via @p wm->stages)
 * @param stage   Stage the desktop belongs to
 * @param desktop Desktop whose clients are rearranged
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 *
 * @see @a place_window_apply
 */
void enact_desktop_client_rearrange_all(const wm_td *wm,
        stage_td *stage, const desktop_td *desktop);

/**
 * @brief Iconify every client on the desktop
 *
 * @param desktop Desktop whose clients are iconified
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_client_iconify_all(desktop_td *desktop);

/**
 * @brief Restore every iconified client on a desktop
 *
 * @param desktop Desktop whose iconified clients are restored
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_client_deiconify_all(desktop_td *desktop);

/**
 * @brief Cycle input focus to the next non-iconified client
 *
 * Opens the cycle menu preselecting the next entry, and repaints it.
 *
 * @param connection XCB connection
 * @param stage      Stage on which to center the menu
 * @param desktop    Desktop whose client list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_cycle_clients_active(xcb_connection_t *connection,
        stage_td *stage, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg);

/**
 * @brief Cycle input focus to the previous non-iconified client
 *
 * Opens the cycle menu preselecting the previous entry, and repaints
 * it.
 *
 * @param connection XCB connection
 * @param stage      Stage on which to center the menu
 * @param desktop    Desktop whose client list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_cycle_clients_prev(xcb_connection_t *connection,
        stage_td *stage, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg);

/**
 * @brief Cycle input focus to the next iconified client
 *
 * Opens the cycle menu listing icons and preselecting the next entry,
 * and repaints it.
 *
 * @param connection XCB connection
 * @param stage      Stage on which to center the menu
 * @param desktop    Desktop whose icon list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_cycle_clients_icons_next(xcb_connection_t *connection,
        stage_td *stage, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg);

/**
 * @brief Cycle input focus to the previous iconified client
 *
 * Opens the cycle menu listing icons and preselecting the previous
 * entry, and repaints it.
 *
 * @param connection XCB connection
 * @param stage      Stage on which to center the menu
 * @param desktop    Desktop whose icon list will be shown
 * @param modifier   Modifier mask of the opening key binding
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
void enact_desktop_cycle_clients_icons_prev(xcb_connection_t *connection,
        stage_td *stage, desktop_td *desktop,
        uint16_t modifier, const config_td *cfg);


#endif  /* ! ENACT_DESKTOP_H */
