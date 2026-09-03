/**
 * @file input/kbd/resolve.c
 *
 * @brief Resolution of a key press against the binding table
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <input/kbd/bind.h>
#include <input/kbd/internal.h>


/**
 * @brief Strip the modifiers that must never take part in a match
 *
 * Caps Lock and Num Lock are states of the keyboard rather than intent
 * of the user, and the X server reports them in the same word as the
 * modifiers that do carry intent, so a binding on @c Mod4+d would
 * otherwise stop working the moment Num Lock is on.
 *
 * @param mask Raw modifier mask, as reported or as configured
 *
 * @return The same mask with the lock bits cleared
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_ik_resolve_strip_locks(uint16_t mask)
{
    return (uint16_t) ((unsigned int) mask &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));
}


/* Find which action a key and modifier combination is bound to */
enum wm_keybind_type_e ik_resolve_binding(xcb_keysym_t keysym,
        uint16_t state, uint16_t *out_modmask)
{
    const uint16_t wanted = s_ik_resolve_strip_locks(state);

    for (int i = 0; i < keyboard_binding_count(); ++i) {
        xcb_keysym_t bound_keysym;
        uint16_t bound_modmask;
        enum wm_keybind_type_e type;

        type = keyboard_binding_at(i, &bound_keysym, &bound_modmask);

        if (keysym != bound_keysym ||
                wanted != s_ik_resolve_strip_locks(bound_modmask)) {
            continue;
        }

        if (out_modmask != NULL) {
            *out_modmask = bound_modmask;
        }

        return type;
    }

    return KEYBIND_NONE;
}
