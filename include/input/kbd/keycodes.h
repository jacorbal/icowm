/**
 * @file input/kbd/keycodes.h
 *
 * @brief Common key and modifier constants for input handling
 *
 * Provides the @c INPUT_STRIP_LOCK_MASK macro (used whenever a modifier
 * mask must be compared against a configured binding, so that
 * @c Caps_Lock and @c Num_Lock do not interfere) and convenient aliases
 * for the most common XCB modifier masks.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef INPUT_KBD_KEYCODES_H
#define INPUT_KBD_KEYCODES_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Strip locking modifier bits from a modifier mask
 *
 * Removes @c XCB_MOD_MASK_LOCK (@c Caps_Lock) and @c XCB_MOD_MASK_2
 * (@c Num_Lock) from @p m so that comparisons against configured
 * bindings are not affected by the state of these locking keys.
 *
 * @param m Modifier mask to strip (any integer type)
 *
 * @return A @c uint16_t with locking bits cleared
 */
#define INPUT_STRIP_LOCK_MASK(m) \
    ((uint16_t) ((unsigned int)(m) & \
        ~((unsigned int) XCB_MOD_MASK_LOCK | \
            (unsigned int) XCB_MOD_MASK_2)))

/* Readable aliases for common modifier masks */
#define MOD_SHIFT    XCB_MOD_MASK_SHIFT    /**< Shift modifier */
#define MOD_CTRL     XCB_MOD_MASK_CONTROL  /**< Control modifier */
#define MOD_ALT      XCB_MOD_MASK_1        /**< Alt/Mod1 modifier */
#define MOD_SUPER    XCB_MOD_MASK_4        /**< Super/Win/Mod4 modifier */
#define MOD_HYPER    XCB_MOD_MASK_5        /**< Hyper/Mod5 modifier */
#define MOD_NUMLOCK  XCB_MOD_MASK_2        /**< Num_Lock/Mod2 (locking) */
#define MOD_CAPSLOCK XCB_MOD_MASK_LOCK     /**< Caps_Lock (locking) */


#endif  /* ! INPUT_KBD_KEYCODES_H */
