/**
 * @file config/randr.h
 *
 * @brief XRandR per-output profile configuration
 *
 * The resolution, position and rotation wanted for each physical
 * output, as loaded from @c randr.json.
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

#ifndef CONFIG_RANDR_H
#define CONFIG_RANDR_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Type includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/config.h>


/**
 * @brief Per-output RandR profile configuration
 *
 * Stores the user-defined settings for a single physical output,
 * applied via @a surface_action_randr_apply_profiles at startup and
 * whenever that output is (re)connected.
 *
 * @see @c surface.h
 */
struct config_randr_output_s {
    /**
     * @brief Preferred resolution
     *
     * Matched against the screen's mode list, falling back to
     * whatever mode the output's CRTC already has (or its first
     * preferred mode, if none) when left at @c 0 or when no mode
     * matches exactly.
     *
     * @note Only applied when @p is_enabled is @c true
     */
    struct dimensions_s preferred_res;

    /**
     * @brief Output position @e (x, y) in the virtual screen
     *
     * @note Only applied when @p is_enabled is @c true
     */
    struct position_s position;

    uint16_t rotation;  /**< Preferred rotation (XRandR mask);
                             see @p is_enabled */

    /**
     * @brief Whether this output is used at all
     *
     * - @c true applies @p preferred_res, @p position, and @p rotation
     *   below to the output's CRTC, and also lets IcoWM manage
     *   windows on it;
     * - @c false instead turns the output's CRTC off if it has one,
     *   blanking it, and excludes it from window management entirely,
     *   as if physically disconnected, which is useful for
     *   a permanently-connected output (a projector for mirroring, say)
     *   that should never receive windows.
     *
     * @see @a surface_action-apply_randr_profiles and
     *      @a surface_monitor_refresh_all
     */
    bool is_enabled;

    /**
     * @brief Mark this output as RandR's primary one, applied as
     *        a separate request right after the rest of this profile
     *
     * @note Meaningful only where @p is_enabled is @c true
     */
    bool is_primary;

    /** Output name, such as "HDMI-1" or "VESA-1" */
    char name[CONFIG_RANDR_OUTPUT_NAME_LENGTH];
};


/**
 * @brief XRandR layout configuration
 *
 * Holds a list of per-output profiles and a global on/off switch.
 *
 * @note One instance per @c config_td, shared by every managed X
 *       screen (@c surface_td), not scoped per-screen
 * @note Matching in @a surface_action_randr_apply_profiles is by
 *       @a config_randr_output_s.name alone, queried independently
 *       against the RandR resources of each screen
 * @note On a multi-GPU setup with two X screens exposing an output of
 *       the same name, the matching profile applies to both alike
 */
struct config_randr_s {
    struct config_randr_output_s outputs[CONFIG_RANDR_MAX_OUTPUTS];
    uint32_t output_count;  /**< Number of populated output profiles */

    /**
     * @brief Master switch for the whole per-output profile system
     *
     * @c false ignores every profile in @p outputs and every detected
     * RandR output is used, same as before this system existed.
     *
     * @see @p config_randr_output_s
     */
    bool is_enabled;

};


#endif  /* ! CONFIG_RANDR_H */
