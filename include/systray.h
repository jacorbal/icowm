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
 *
 * @defgroup systray System tray
 * @ingroup surface
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
#include <types/pair.h> /* strut_partial_s */
#include <wm.h>

/* Default initial values */
#include <defs/systray.h> /* WM_SYSTRAY_MAX_ICONS */


/**
 * XEMBED opcode sent to a newly docked icon (@c XEMBED_EMBEDDED_NOTIFY)
 */
#define SYSTRAY_XEMBED_EMBEDDED_NOTIFY (0u)

/**
 * @c _NET_SYSTEM_TRAY_OPCODE opcode requesting an icon be docked
 */
#define SYSTRAY_OPCODE_REQUEST_DOCK (0u)


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
 * operation (e.g., a configuration reload flipping @c is-enabled) use
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
 * @brief Return the tray window when it is visible and stacked in the
 *        'below' layer, or @c XCB_WINDOW_NONE otherwise
 *
 * Lets any code that needs to stack itself just below the tray (see
 * @c ccmd_client_iconify, which stacks a newly iconified client's icon
 * window here, since icons are meant to sit lower than the tray even
 * within the 'below' layer) target it directly instead of competing
 * with it for the absolute bottom of the sibling stack via an
 * unqualified @c XCB_STACK_MODE_BELOW: two things both asking to be
 * "as low as possible" with no window to stack relative to just take
 * turns displacing each other, so whichever restacked more recently
 * wins, leaving the other one wrong until it happens to restack
 * again.
 *
 * @return The tray's own @c xcb_window_t, or @c XCB_WINDOW_NONE when
 *         the tray is not currently shown, or is shown in a layer
 *         other than 'below'
 *
 * @note Complexity: @e O(1)
 */
xcb_window_t systray_below_window(void);

/**
 * @brief Return the space this window manager's own systray currently
 *        reserves for itself on @p surface, via
 *        @c _NET_WM_STRUT_PARTIAL/@c _NET_WM_STRUT published on its
 *        own dock window
 *
 * Lets @c desktop_update_workarea fold the tray's own reservation into
 * a desktop's @c workarea the exact same way it already folds a real
 * client's own published strut -- maximized windows and initial
 * placement stay off the tray's own area, the same protection any
 * other panel or dock gets by publishing a strut of its own, per the
 * specification's own recommendation.
 *
 * The tray is a single, not-per-surface instance (@c config.systray is
 * one global setting; see @c systray_state_s's own doc comment for
 * why), docked on exactly one surface at a time: this returns @c NULL
 * for every other surface, so a multi-screen setup never reserves the
 * tray's space on a screen it does not actually occupy.
 *
 * @param surface Surface to query the tray's reservation for
 *
 * @return A pointer to the tray's currently reserved strut when
 *         @p surface is the one it is docked on and it is currently
 *         visible (mapped, non-empty, owning the tray selection); on
 *         every other surface, or while unmapped, @c NULL rather than
 *         a strut with every side at zero, so a caller need not treat
 *         "not this surface" and "reserving nothing" as the same case
 *
 * @note Complexity: @e O(1)
 */
const struct strut_partial_s *systray_get_reserved_strut(
        const surface_td *surface);

/**
 * @brief Return the tray's own current on-screen rectangle on
 *        @p surface
 *
 * A synchronous @c xcb_get_geometry round trip, unlike every other
 * accessor in this header: nothing about the tray's own current
 * position and size is cached anywhere else in this module (only its
 * configured @c height is; the rest follows from wherever @c
 * systray_layout_reflow last placed the window itself), so this is
 * the only way to answer "where exactly is the tray sitting right
 * now" precisely.  Meant for infrequent, one-off checks (e.g. an icon
 * settling into its final dropped position after a drag), not
 * anything called on every frame of a render or drag loop.
 *
 * @param surface Surface to query the tray's rectangle on
 * @param out_x   Receives the rectangle's left edge, root-relative
 *                (same coordinate space every top-level window this
 *                project creates, icon windows included, already
 *                shares)
 * @param out_y   Receives the rectangle's top edge, root-relative
 * @param out_w   Receives the rectangle's width
 * @param out_h   Receives the rectangle's height
 *
 * @return @c true and the rectangle filled in when @p surface is the
 *         one the tray is docked on and it is currently showing
 *         there; @c false otherwise, with none of the output
 *         parameters touched
 *
 * @note Complexity: @e O(1), plus one synchronous round trip to the
 *       X server
 */
bool systray_get_geometry(const surface_td *surface, int32_t *out_x,
        int32_t *out_y, uint16_t *out_w, uint16_t *out_h);

/**
 * @brief Query whether @p window is a currently docked icon, and if
 *        so, force it back to the tray's configured icon size
 *
 * Meant to be called from the @c ConfigureRequest handler for any
 * window not otherwise recognized as a managed client: a docked
 * icon's own resize attempt on itself reaches the window manager as a
 * @c ConfigureRequest only because the tray window now sets
 * @c XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT; without this function
 * actively overriding it back to @c theme.systray.pixmap.size, that
 * redirect alone would just let the request through unchanged, which
 * is no better than not redirecting at all.
 *
 * @param window Window to test
 *
 * @return @c true if @p window was a docked icon (its size was just
 *         forced back, and the caller should treat the request as
 *         fully handled); @c false otherwise (the caller should fall
 *         through to its normal handling)
 *
 * @note Complexity: @e O(n), where @e n is the number of docked icons
 */
bool systray_enforce_icon_size(xcb_window_t window);

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
 *   they are, just hidden.  Unlike @c systray_shutdown, nothing is
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
 * - @c below (default): always behind normal windows
 * - @c above: above normal windows, but covered by a fullscreen
 *   window, the same way a taskbar or panel gets covered by a
 *   fullscreen window in most desktop environments
 * - @c overlay: above absolutely everything, including fullscreen
 *   windows
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients (only when @c systray.layer is @c above;
 *       @e O(1) for @c below and @c overlay)
 *
 * @see @c ccmd_client_fullscreen and @c ccmd_client_unfullscreen
 */
void systray_restack(void);

/**
 * @brief How many milliseconds until the systray clock needs its next
 *        redraw
 *
 * Meant for the main event loop's @c poll timeout: call this once per
 * iteration and use the result to shorten the timeout when it is
 * smaller, the same way the info popup and desktop-switch notification
 * already do, so the clock's displayed text advances promptly at each
 * wall-clock second instead of only when some unrelated X event
 * happens to wake the loop up.
 *
 * @return @c -1 when the clock is disabled or the tray does not
 *         currently own the systray selection (nothing to redraw);
 *         @c 0 when a redraw is due right now; otherwise a small
 *         positive number of milliseconds
 *
 * @note Complexity: @e O(1)
 */
int systray_clock_ms_remaining(void);

/**
 * @brief Redraw the systray clock if the wall-clock second has changed
 *        since it was last drawn
 *
 * Call this once per main-loop iteration, after @c poll returns,
 * regardless of whether it returned due to an X event or a timeout.  A
 * no-op when the clock is disabled, the tray does not own the systray
 * selection, or less than a second has passed since the last redraw.
 *
 * @note Complexity: @e O(1) plus whatever the tray's own reflow costs
 *       when a redraw actually happens (see its own complexity note)
 */
void systray_clock_tick(void);


#endif  /* ! SYSTRAY_H */
