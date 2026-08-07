/**
 * @file systray.h
 *
 * @brief Built-in systray dock declarations
 *
 * Implements the freedesktop.org "System Tray Protocol Specification"
 * (selection acquisition, dock requests) together with just enough of
 * XEMBED to host icon windows, gated by @c config.systray.is_enabled.
 * A single tray is created on the first managed surface; the
 * @c systray.position configuration key picks which screen corner it
 * docks in.
 *
 * @note Icons are reparented with the window manager's default visual,
 *       not a negotiated 32-bit ARGB visual, so icons that rely on real
 *       alpha transparency may render with a solid background instead
 *       of blending into the tray.  Full visual negotiation was left
 *       out to keep this a contained, verifiable first implementation
 *       (vid. @c _NET_SYSTEM_TRAY_VISUAL in the specification).
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SYSTRAY_H
#define SYSTRAY_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <wm.h>


/**
 * @brief Acquire the tray selection and create the dock window
 *
 * A no-op when @c wm->config->base.systray.is_enabled is @c false, when
 * @p wm has no managed surfaces yet, or when another systray manager
 * already owns the @c _NET_SYSTEM_TRAY_Sn selection on the first
 * surface's screen.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(1)
 */
void systray_init(wm_td *wm);

/**
 * @brief Fully tear down the tray: release the selection and destroy
 *        the dock window
 *
 * Only meant for the window manager itself exiting.  Safe to call even
 * when @c systray_init was never called or did not acquire ownership.
 * Does not attempt to gracefully undock icons first: destroying the
 * tray window implicitly reparents any still-docked icon windows back
 * to the root window, matching how every other systray implementation
 * behaves on exit.  To toggle the tray off and on again during normal
 * operation (e.g. a configuration reload flipping @c is-enabled) use
 * @c systray_reload instead, which keeps the window and any docked
 * icons alive in the background so they reappear immediately when
 * re-enabled instead of needing every application to re-dock itself.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(1)
 */
void systray_shutdown(wm_td *wm);

/**
 * @brief Query whether @p window is the tray dock window itself
 *
 * Used by the main loop and the client-message dispatcher to route
 * events addressed to the tray window here instead of through the
 * normal managed-client handling path, since the tray window is never
 * a managed client.
 *
 * @param window Window to test
 *
 * @return @c true if @p window is the current tray dock window
 *
 * @note Complexity: @e O(1)
 */
bool systray_owns_window(xcb_window_t window);

/**
 * @brief Handle a @c ClientMessage addressed to the tray window
 *
 * Recognizes @c _NET_SYSTEM_TRAY_OPCODE messages with the
 * @c SYSTEM_TRAY_REQUEST_DOCK opcode and docks the requested icon
 * window; other opcodes (balloon messages) are acknowledged as ignored.
 * Events for a window other than the tray's are ignored.
 *
 * @param wm    Window manager state
 * @param event Incoming @c ClientMessage event
 *
 * @note Complexity: @e O(1)
 */
void systray_handle_client_message(wm_td *wm,
        const xcb_client_message_event_t *event);

/**
 * @brief Handle a docked icon window being destroyed
 *
 * Removes @p window from the tray's icon list, if present, and reflows
 * the remaining icons.  A no-op if @p window is not currently docked.
 *
 * @param wm     Window manager state
 * @param window Destroyed window
 *
 * @note Complexity: @e O(n), where @e n is the number of docked icons
 */
void systray_handle_destroy(wm_td *wm, xcb_window_t window);

/**
 * @brief Reposition the tray dock window for its surface's current size
 *
 * Called after a surface resize (e.g., an XRandR screen-change) so the
 * tray stays pinned to its configured corner.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(1)
 */
void systray_handle_surface_resize(wm_td *wm);


/**
 * @brief React to a configuration reload
 *
 * Reconciles the live tray with the just-reloaded
 * @c wm->config->base.systray settings, so toggling @c is-enabled or
 * changing @c position takes effect immediately instead of only on the
 * next window manager restart:
 * - Was enabled, now disabled: releases the selection right away, but
 *   keeps the dock window and every currently docked icon exactly as
 *   they are, just hidden -- unlike @c systray_shutdown, nothing is
 *   destroyed or reparented away.
 * - Was disabled, now enabled: creates the dock window if this is the
 *   very first time (nothing to do otherwise), then re-acquires the
 *   selection.  Any icons that were docked before a previous disable
 *   are still attached and reappear immediately, with no need for their
 *   applications to re-request docking.
 * - Was and remains enabled: updates the configured corner and reflows
 *   without disturbing already-docked icons.
 * - Was and remains disabled: a no-op.
 *
 * @param wm Window manager state, with @c wm->config already reloaded
 *
 * @note Complexity: @e O(1)
 */
void systray_reload(wm_td *wm);

/**
 * @brief Re-apply the configured @c systray.layer stacking rule
 *
 * Meant to be called whenever something elsewhere in the window manager
 * changes in a way that could affect where the tray should sit relative
 * to other windows.  Currently just a client entering or leaving
 * fullscreen.  A safe no-op when the tray is not currently active.
 *
 * Valid layer values are:
 * - @c below: always behind normal windows
 * - @c above (default): above normal windows, but covered by
 *   a fullscreen window (the correct/expected behavior)
 * - @c above-all: above absolutely everything, including fullscreen
 *   windows (the current behavior, now optional instead of fixed)
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients (only when @c systray.layer is @c above;
 *       @e O(1) for @c below and @c above-all)
 *
 * @see @c wcmd_client_fullscreen and @c wcmd_client_unfullscreen
 */
void systray_restack(void);


#endif  /* ! SYSTRAY_H */
