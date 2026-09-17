/**
 * @file stage/action.h
 *
 * @brief High-level, user-facing actions over a stage: growing or
 *        shrinking its desktop count, toggling strutless maximize, and
 *        applying or reverting RandR output profiles
 *
 * Split out of @c stage.h, alongside its sibling @c stage/
 * headers, so a file that only needs one of these actions does not
 * also pull in, and rebuild against, every other unrelated stage
 * concern (desktop, viewport, monitor, workarea, client) declared in
 * the same file.
 *
 * @see @p stage_s, in @c stage.h
 *
 * @defgroup stage_action Stage high-level actions
 * @ingroup stage
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef STAGE_ACTION_H
#define STAGE_ACTION_H


/* System includes */
#include <stdbool.h>

/* Project includes */
#include <stage.h>


/**
 * @brief Add a new desktop associated with the stage
 *
 * Grows @p stage's configured @c topology.screens.desktops layout by
 * one row or column first, whichever @c orientation treats as the
 * non-primary axis, when there is not already a desktop-less gap cell
 * in it for the new desktop to land on (see
 * @a s_stage_layout_grow_for's comment, in @c stage/switch.c, for
 * the fuller reasoning on why that one axis specifically).  Purely
 * a "create it" action either way: @p stage's current view never
 * switches to the new desktop, whether growing a new row or column
 * happened or not, and regardless of which desktop, if any, currently
 * has the view.
 *
 * Refused outright once @p stage's desktop count already reaches
 * @c CONFIG_MAX_DESKTOPS: @c config_base's
 * @c screens[screen_id].desktops array (@c config.h) is a fixed-size
 * array of exactly that many slots, indexed by the new desktop's ID, so
 * adding one more past that point would index past the end of it.
 *
 * Also refused outright, regardless of the current count, while
 * restricted-memory mode is active (@a memguard_max_clients), which is
 * deliberately locked to a single desktop always (see
 * @a config_set_default_values_memguard).
 *
 * @param stage Pointer to the stage to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action, including already being at
 *            @c CONFIG_MAX_DESKTOPS, or restricted-memory mode being
 *            active
 *
 * @note Complexity: @e O(1)
 */
int stage_action_desktop_add(stage_td *stage);

/**
 * @brief Remove the last desktop from the stage, moving any client
 *        still on it to the desktop immediately before it first
 *
 * Refuses outright when only one desktop remains (@c stage's desktop
 * count must stay at least @c 1).  Restricted-memory mode always has
 * exactly one desktop and no way to reach a second one (see
 * @a stage_action_desktop_add's comment), so that same guard alone
 * already refuses this call every time it runs under that mode too,
 * with no separate check of its own needed here.
 *
 * Every client still on the desktop being removed, pinned or not, is
 * moved onto what becomes the new last desktop before the old one is
 * destroyed.  Destroying a desktop that still holds clients would
 * otherwise destroy those clients' @c client_td structures right along
 * with it (see @a desktop_destroy, @c desktop.c), losing real, live
 * application windows rather than just the virtual desktop container.
 *
 * A moved client's EWMH @c _NET_WM_DESKTOP is brought in line with its
 * new desktop, except for a pinned one, whose property already holds
 * the EWMH "all desktops" sentinel and is left alone.  If the desktop
 * being removed is the current one, the view switches to the new last
 * desktop first.
 *
 * @param stage Pointer to the stage to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Refused (only one desktop left, no desktop to remove from,
 *            no fallback desktop available, or the underlying removal
 *            itself failed)
 * @retval -1 @p stage is @c NULL
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop being removed
 */
int stage_action_desktop_remove(stage_td *stage);

/**
 * @brief Toggle whether panel/tray struts are set aside when computing
 *        this stage's desktops' work areas
 *
 * Strutless maximization: while on, @a desktop_update_workarea
 * (@c desktop.h) folds in only @c desktops.margins from configuration,
 * never a panel's @c _NET_WM_STRUT_PARTIAL nor the systray's
 * reservation, so a maximized or smart-placed window can use the full
 * screen underneath wherever a panel would otherwise have reserved
 * space.  Every desktop's work area is recomputed immediately
 * (@a stage_workarea_refresh_all), not left for whatever unrelated
 * event happens to trigger that next.
 *
 * @param stage Pointer to the stage to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       @p stage
 */
int stage_action_maximize_toggle_strutless(stage_td *stage);

/**
 * @brief Clear every RandR snapshot left over from a previous config
 *        reload
 *
 * Called once, by @a wm_action_config_reload itself, before it calls
 * @a stage_action_randr_apply_profiles for each of its own stages
 * in turn: every one of those calls appends to the same snapshot rather
 * than resetting it, so a single reload that touches more than one
 * stage ends up with all of them, not only the first, covered by one
 * later @a stage_action_randr_revert_profiles call.
 *
 * @note Complexity: @e O(1)
 */
void stage_action_randr_snapshot_begin(void);

/**
 * @brief Apply every configured RandR output profile that matches
 *        a currently-connected output on this stage
 *
 * For each configured profile whose name matches a currently-connected
 * output.  An enabled profile's desired resolution (falling back to
 * whatever mode is already active, or the output's preferred mode, when
 * none is configured or none of the screen's modes matches it),
 * position, and rotation are compared against that output's CRTC's
 * actual current state first, and @a xcb_randr_set_crtc_config is
 * issued only when at least one of them actually differs (claiming
 * a free compatible CRTC first if the output had none, which always
 * counts as a difference, since it was driving nothing at all before);
 * @p is_primary is compared and, if needed, applied the same way as
 * a separate, independent step afterward.
 *
 * A disabled profile instead turns the output's CRTC off, but only if
 * it is not already off.  A configured profile whose name matches no
 * currently-connected output is skipped, logged at debug level only,
 * not as a warning.  This comparison is what keeps an unchanged
 * @c randr.json from writing anything to the X server at all on
 * a reload that touched some other file instead, or on a hotplug event
 * for an unrelated output.
 *
 * Meant to be called once at startup (after RandR is confirmed
 * available) and again whenever @c XCB_RANDR_NOTIFY_OUTPUT_CHANGE
 * fires, so a profile for an output that was not yet connected at
 * startup still gets applied once it is.
 *
 * @param stage Stage whose outputs to apply configured
 *                      profiles to
 * @param take_snapshot Whether to save each changed CRTC's prior state
 *                      first, appended to whatever @a stage_action_
 *                      randr_snapshot_begin last cleared rather than
 *                      replacing it, so a later
 *                      @a stage_action_randr_revert_profiles call can
 *                      put every one of them back, not only this
 *                      stage's own; pass @c false for a startup or
 *                      hotplug call, where there is nothing to revert
 *                      to (the newly-applied state @e is the intended
 *                      one), and @c true only when the caller means to
 *                      offer a user a chance to undo this specific call
 *
 * @return Status of the operation
 * @retval  true if at least one output's actual state was changed
 * @retval false if every configured profile already matched (or
 *               @p stage, RandR management, or every matching output
 *               could not be resolved at all)
 *
 * @note A no-op when @p stage, its connection, or its screen is
 *       @c NULL, or when RandR profile management is off altogether
 *       (@p config->randr.is_enabled is @c false)
 * @note @p config->randr.outputs is not scoped per screen
 * @note Complexity: @e O(p * (n + c)), where @e p is the number of
 *       configured profiles, @e n the number of outputs the screen
 *       currently reports, and @e c the number of CRTCs compatible with
 *       whichever output a profile matches (only when it has none
 *       active yet)
 *
 * @see @a wm_action_config_reload, and @p config_randr_s, loaded from
 *      @c randr.json
 */
bool stage_action_randr_apply_profiles(stage_td *stage,
        bool take_snapshot);

/**
 * @brief Undo every snapshotting @a stage_action_randr_apply_
 *        profiles call since the most recent
 *        @a stage_action_randr_snapshot_begin
 *
 * Restores every CRTC any of those calls actually changed to exactly
 * the state it captured first (mode, position, rotation, or off if it
 * was off), and each touched stage's own RandR primary output to
 * whichever one held it before, then clears the snapshot.  Every
 * stage a single config reload touched is covered, not only the
 * first, one stage's own screen resources failing to resolve never
 * blocking another's from still being reverted.  A safe, cheap no-op if
 * nothing is currently snapshotted, including when a stage it
 * belonged to no longer has a usable connection.
 *
 * @note Complexity: @e O(s), where @e s is the number of CRTCs
 *       snapshotted across every stage since the last
 *       @a stage_action_randr_snapshot_begin
 */
void stage_action_randr_revert_profiles(void);


#endif /* STAGE_ACTION_H */
