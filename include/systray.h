/**
 * @file systray.h
 *
 * @brief Built-in systray dock declarations
 *
 * Implements the freedesktop.org "System Tray Protocol Specification"
 * (selection acquisition, dock requests) together with just enough of
 * XEMBED to host icon windows, gated by @p config.systray.is_enabled.
 * A single tray is created on the first managed stage; the
 * @p systray.position configuration key picks which screen corner it
 * docks in.
 *
 * @note Icons are reparented with the window manager's default visual
 *       rather than a negotiated 32-bit ARGB one, so an icon relying
 *       on real alpha transparency may render against a solid
 *       background instead of blending into the tray
 * @note Full visual negotiation was left out to keep this a contained,
 *       verifiable first implementation (vid.
 *       @c _NET_SYSTEM_TRAY_VISUAL in the specification)
 *
 * Holds the core lifecycle and query functions.  Event routing, an
 * icon's own resize/map requests, and the clock widget are each split
 * into their own sibling header instead, @c systray/handle.h,
 * @c systray/icon.h, and @c systray/clock.h, so a file that only needs
 * one of those does not also pull in, and rebuild against, every other
 * one declared alongside it.
 *
 * @see @c systray/handle.h, @c systray/icon.h, @c systray/clock.h
 *
 * @defgroup systray System tray
 * @ingroup stage
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

/* Type includes */
#include <types/handles.h>

/* Project includes */
#include <types/pair.h> /* strut_partial_s */

/* Default initial values */
#include <defs/systray.h> /* WM_SYSTRAY_MAX_ICONS */


/**
 * @brief Acquire the tray selection and create the dock window
 *
 * A no-op when @p wm->config->base.systray.is_enabled is @c false, when
 * @p wm has no managed stages yet, or when another systray manager
 * already owns the @c _NET_SYSTEM_TRAY_Sn selection on the first
 * stage's screen.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(1)
 */
void systray_init(const wm_td *wm);

/**
 * @brief Fully tear down the tray, i.e., release the selection and
 *        destroy the dock window
 *
 * Only meant for the window manager itself exiting.  Safe to call even
 * when @a systray_init was never called or did not acquire ownership.
 * Does not attempt to gracefully undock icons first, for destroying the
 * tray window implicitly reparents any still-docked icon windows back
 * to the root window, matching how every other systray implementation
 * behaves on exit.
 *
 * To toggle the tray off and on again during normal operation (e.g.,
 * a configuration reload flipping @p is-enabled) use @a systray_reload
 * instead, which keeps the window and any docked icons alive in the
 * background so they reappear immediately when re-enabled instead of
 * needing every application to re-dock itself.
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
 * @brief Return the tray window when it is visible and stacked in the
 *        'below' layer, or @c XCB_WINDOW_NONE otherwise
 *
 * Lets any code that needs to stack itself just below the tray target
 * it directly instead of competing with it for the absolute bottom of
 * the sibling stack via an unqualified @c XCB_STACK_MODE_BELOW: two
 * things both asking to be "as low as possible" with no window to stack
 * relative to just take turns displacing each other, so whichever
 * restacked more recently wins, leaving the other one wrong until it
 * happens to restack again.
 *
 * @return The tray's @c xcb_window_t, or @c XCB_WINDOW_NONE when
 *         the tray is not currently shown, or is shown in a layer
 *         other than 'below'
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ccmd_client_iconify, which stacks a newly iconified client's
 *      icon window here, since icons are meant to sit lower than the
 *      tray even within the @c below layer
 */
xcb_window_t systray_below_window(void);

/**
 * @brief Return the space this window manager's systray currently
 *        reserves for itself on @p stage, via
 *        @c _NET_WM_STRUT_PARTIAL / @c _NET_WM_STRUT published on its
 *        own dock window
 *
 * Lets @a desktop_update_workarea fold the tray's reservation into
 * a desktop's @p workarea the exact same way it already folds a real
 * client's published strut, i.e., maximized windows and initial
 * placement stay off the tray's area, the same protection any other
 * panel or dock gets by publishing a strut of its own, per the
 * specification's recommendation.
 *
 * The tray is a single, not-per-stage instance (@p config.systray is
 * one global setting; see @p systray_state_s's comment for why), docked
 * on exactly one stage at a time.  Tthis returns @c NULL for every
 * other stage, so a multi-screen setup never reserves the tray's
 * space on a screen it does not actually occupy.
 *
 * @param stage Stage to query the tray's reservation for
 *
 * @return A pointer to the tray's currently reserved strut when
 *         @p stage is the one it is docked on and it is currently
 *         visible (mapped, non-empty, owning the tray selection); on
 *         every other stage, or while unmapped, @c NULL rather than
 *         a strut whose sides are all zero, so a caller need not treat
 *         "no dice, not this stage" and "reserving nothing" as the
 *         same case
 *
 * @note Complexity: @e O(1)
 */
const struct strut_partial_s
    *systray_get_reserved_strut(const stage_td *stage);

/**
 * @brief Return the tray's current on-screen rectangle on @p stage
 *
 * A synchronous @a xcb_get_geometry round trip, unlike every other
 * accessor in this header.  Nothing about the tray's current position
 * and size is cached anywhere else in this module (only its configured
 * @p height is; the rest follows from wherever @a systray_layout_reflow
 * last placed the window itself), so this is the only way to answer
 * "where the bloody hell is the tray sitting right now?" precisely.
 *
 * Meant for infrequent, one-off checks (e.g., an icon settling into its
 * final dropped position after a drag), not anything called on every
 * frame of a render or drag loop.
 *
 * @param stage    Stage to query the tray's rectangle on
 * @param out_tray Receives the tray's current rectangle, root-relative
 *                 (same coordinate space every top-level window this
 *                 project creates, icon windows included, already
 *                 shares)
 *
 * @return @c true and @p out_tray filled in when @p stage is the one
 *         the tray is docked on and it is currently showing there; @c
 *         false otherwise, with @p out_tray left untouched
 *
 * @note Complexity: @e O(1), plus one synchronous round trip to the
 *       X server
 */
bool systray_get_geometry(const stage_td *stage,
        struct geometry_s *restrict out_tray);

/**
 * @brief React to a configuration reload
 *
 * Reconciles the live tray with the just-reloaded
 * @p wm->config->base.systray settings, so toggling @p is-enabled or
 * changing @p position takes effect immediately instead of only on the
 * next window manager restart:
 *
 * - Was enabled, now disabled: releases the selection right away, but
 *   keeps the dock window and every currently docked icon exactly as
 *   they are, just hidden.  Unlike @a systray_shutdown, nothing is
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
 * @param wm Window manager state, with @p wm->config already reloaded
 *
 * @note Complexity: @e O(1)
 */
void systray_reload(const wm_td *wm);

/**
 * @brief Re-apply the configured @c systray.layer stacking rule
 *
 * Meant to be called whenever something elsewhere in the window manager
 * changes in a way that could affect where the tray should sit relative
 * to other windows.  Currently just a client entering or leaving
 * fullscreen.
 *
 * Valid layer values are:
 *
 * - @c below (default): always behind normal windows
 * - @c above: above normal windows, but covered by a fullscreen window,
 *   the same way a taskbar or panel gets covered by a fullscreen window
 *   in most desktop environments
 * - @c overlay: above absolutely everything, including fullscreen
 *   windows
 *
 * @note A safe no-op when the tray is not currently active
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients (only when @p systray.layer is @c above;
 *       @e O(1) for @c below and @c overlay)
 *
 * @see @a ccmd_client_fullscreen and @a ccmd_client_unfullscreen
 */
void systray_restack(void);

/**
 * @brief Reposition the tray window and lay out its docked icons
 *
 * Unmaps the tray window while empty or while the selection is not
 * currently owned (e.g., disabled by configuration, or another tray
 * manager is active), so it never shows on screen in either case;
 * otherwise sizes and moves it to the configured corner of the stage
 * and arranges icons in a single horizontal row inside it.
 *
 * Redraws only from whatever text and icon state is already cached;
 * never recomputes the clock or re-polls the battery itself, so calling
 * this alone (e.g., in response to an @c Expose event revealing
 * a previously covered region) is always cheap regardless of how often
 * it happens.
 *
 * @note Complexity: @e O(n), where @e n is the number of docked icons
 */
void systray_layout_reflow(void);


#endif  /* ! SYSTRAY_H */
