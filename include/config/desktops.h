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
     * @brief Whether dragging a window past a screen edge switches
     *        desktops
     *
     * Held there past @c WM_DESKTOP_WARP_DELAY_MS (@c defs/desktop.h),
     * switches to the adjacent desktop with the drag still held.
     * Deferred to entirely whenever @c viewport.pan_on_edge_drag
     * still have room to pan the current desktop's viewport toward
     * that same edge instead: a desktop switch is only ever what a
     * held edge means once the viewport itself has nowhere left to
     * go, never before.
     *
     * @note Meaningless with only one desktop
     */
    bool warp_on_edge_drag;

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
