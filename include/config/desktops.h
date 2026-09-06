/**
 * @file config/desktops.h
 *
 * @brief Desktop navigation and reserved-space configuration
 *
 * How navigation between the desktops @c config/base.h defines
 * behaves, and how much of each desktop's area stays reserved
 * regardless of what any client publishes.
 *
 * @ingroup config
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_DESKTOPS_H
#define CONFIG_DESKTOPS_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Default initial values */
#include <defs/config.h>


/**
 * @brief Global desktop-navigation and reserved-space behavior
 *
 * Unlike @p config_base_s (screen and desktop topology.  How many
 * screens and desktops exist, and their names/colors), none of
 * this describes topology at all, only how navigation between
 * whatever desktops @p config_base_s already defines behaves, and how
 * much of each desktop's area stays reserved regardless of what any
 * client itself publishes via @c _NET_WM_STRUT_PARTIAL.
 *
 * Loaded from @c config.json's top-level @c desktops object,
 * a sibling of @c topology, not nested inside it.  Unlike topology,
 * every field here does take effect on a configuration reload.
 *
 * @see @a desktop_update_workarea
 */
struct config_desktop_s {
    /**
     * @brief Whether a client becoming urgent on a desktop other than
     *        the one currently visible on its own surface shows an
     *        informational popup naming that desktop, so the user is
     *        not left unaware that something needs attention off-screen
     *
     * A client urgent on the currently visible desktop already gets its
     * own titlebar blink instead, which this never duplicates.
     *
     * @see @c policy/urgency.h
     */
    bool notify_activity;

    /**
     * @brief Whether dragging a window past a screen edge switches
     *        desktops
     *
     * Held there past @c WM_DESKTOP_WARP_DELAY_MS (@c defs/desktop.h),
     * switches to the adjacent desktop with the drag still held.
     * Deferred to entirely whenever @c pan_on_edge_drag below would
     * still have room to pan the current desktop's viewport toward
     * that same edge instead: a desktop switch is only ever what a
     * held edge means once the viewport itself has nowhere left to
     * go, never before.
     *
     * @note Meaningless with only one desktop
     */
    bool warp_on_edge_drag;

    /**
     * @brief Whether dragging a window past a screen edge pans the
     *        current desktop's viewport toward that edge, carrying
     *        the dragged window along
     *
     * Held there past @c WM_VIEWPORT_PAN_DELAY_MS (@c defs/desktop.h),
     * pans one screen toward the held edge, then repeats every
     * @c WM_VIEWPORT_PAN_REPEAT_MS for as long as the drag stays held
     * there, exactly like @c pan_on_edge_hover below except triggered
     * by a drag rather than a plain hover.  Takes priority over
     * @c warp_on_edge_drag above for as long as the viewport still has
     * room to pan that way; once it does not (or the screen's
     * @c viewport is @c 1x1), a held edge falls through to that one
     * instead.
     *
     * @note Meaningless on a screen whose @c viewport is @c 1x1 (no
     *       panning configured)
     */
    bool pan_on_edge_drag;

    /**
     * @brief Whether resting the pointer against a screen edge, with
     *        no drag in progress, pans the current desktop's viewport
     *        toward that edge
     *
     * Held there past @c WM_VIEWPORT_PAN_DELAY_MS (@c defs/desktop.h),
     * pans one screen toward the held edge, then repeats every
     * @c WM_VIEWPORT_PAN_REPEAT_MS for as long as the pointer stays
     * held there.
     *
     * @note Meaningless on a screen whose @c viewport is @c 1x1 (no
     *       panning configured), or while a window or icon is being
     *       dragged: an edge held during a drag is @c pan_on_edge_drag
     *       and @c warp_on_edge_drag above's to answer instead, never
     *       this one's
     */
    bool pan_on_edge_hover;

    /** Whether switching past the first or last desktop wraps around to
     *  the other end, rather than stopping there.  Meaningless with
     *  only one desktop. */
    bool wrap_at_bounds;

    /**
     * @brief Extra space reserved on each edge of every desktop's
     *        workarea, on top of whatever @c _NET_WM_STRUT_PARTIAL
     *        clients already reserve there
     *
     * Useful for a program that does not publish that property itself
     * (such as, say, Conky).  Applies identically to every desktop on
     * every screen; there is no per-desktop or per-screen override.
     *
     * @see @a desktop_update_workarea
     */
    struct {
        uint32_t top;
        uint32_t right;
        uint32_t bottom;
        uint32_t left;
    } margins;
};


#endif  /* ! CONFIG_DESKTOPS_H */
