/**
 * @file input/mouse/resolve.c
 *
 * @brief Resolution of a button press against the binding table
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <input/mouse/bind.h>
#include <input/mouse/internal.h>


/* Find which action a button and modifier combination is bound to */
enum wm_mousebind_type_e im_resolve_binding(xcb_button_index_t button,
        uint16_t state)
{
    /* Caps Lock and Num Lock are states of the keyboard rather than
     * intent of the user, and the X server reports them in the same
     * word as the modifiers that do carry intent */
    const uint16_t wanted = (uint16_t) ((unsigned int) state &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));

    for (int i = 0; i < mousebind_count(); ++i) {
        xcb_button_index_t bound_button;
        uint16_t required;
        enum wm_mousebind_type_e type;

        type = mousebind_at(i, &bound_button, &required);
        if (type == MOUSEBIND_NONE) {
            continue;
        }

        /* A binding with no modifiers of its own matches whatever the
         * press happened to carry; one that names some requires those
         * to be held, and tolerates others alongside them */
        if (button == bound_button &&
                (required == 0u || (wanted & required) == required)) {
            return type;
        }
    }

    return MOUSEBIND_NONE;
}
