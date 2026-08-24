/**
 * @file config/a11y.h
 *
 * @brief Accessibility oriented timing and feedback configuration
 *
 * Timing and visual feedback adjustments loaded from the entirely
 * optional @c a11y.json.
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

#ifndef CONFIG_A11Y_H
#define CONFIG_A11Y_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Default initial values */
#include <defs/config.h>


/**
 * @brief Accessibility (a11y) oriented adjustments to timing and visual
 *        feedback
 *
 * Loaded from its own @c a11y.json file, entirely optional: a missing
 * file, or any field it does not specify, keeps every value here at the
 * same built-in default the window manager already used before this
 * file existed, so nobody who never creates one sees any behavior
 * change at all.  Every field here does take effect on a configuration
 * reload, the same as @p config_desktop_s above.
 *
 * @p is_enabled (default @c false, mirroring @p config_randr_s's own)
 * gates every other field here at once: @c false leaves all of them at
 * their own built-in defaults regardless of what @c a11y.json otherwise
 * specifies, the same way @a config_load_a11y behaves when the file is
 * absent entirely.  A user keeps an @c a11y.json around (to reference,
 * or to have it ready) without it taking effect until they flip this
 * on, the same opt-in @c randr.json's own @p is-enabled already
 * provides for XRandR output profiles.
 */
struct config_a11y_s {
    /**
     * @brief Gates every field below at once
     *
     * @see This struct's comment above
     */
    bool is_enabled;

    struct {
        /**
         * @brief Milliseconds between two clicks for them to count as
         *        a double-click (on a titlebar, to toggle shade, &c.)
         *
         * @see @c WM_DOUBLE_CLICK_MS (@c defs/input.h) for the built-in
         *      default this overrides
         */
        uint32_t double_click_ms;
    } interaction;

    struct {
        /**
         * @brief Minimum border width, in pixels, enforced on every
         *        decorated or undecorated window
         *
         * This width is measured regardless of what the active theme's
         * own @p window.active.border.width or
         * @p window.inactive.border.width (@c theme.json) specify;
         * raising this keeps the focus indicator visible even for
         * a theme that sets an unusually thin border.
         *
         * @note A value of @c 0 never raises anything, deferring
         *       entirely to the theme
         */
        uint32_t min_border_width;
    } focus_indicator;

    struct {
        /**
         * @brief Sound an audible bell (@c xcb_bell) every time
         *        a client first becomes urgent, alongside the visual
         *        blink it already gets regardless of this setting
         */
        bool sound_bell;

        /**
         * @brief Milliseconds between one blink phase and the next for
         *        an urgent client's own visual indicator

         * @see @c WM_URGENCY_BLINK_INTERVAL_MS (@c defs/urgency.h) for
         *      the built-in default this overrides */
        uint32_t blink_interval_ms;
    } urgency;
};


#endif  /* ! CONFIG_A11Y_H */
