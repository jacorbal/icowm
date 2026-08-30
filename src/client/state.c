/**
 * @file client/state.c
 *
 * @brief What a client's state means for its geometry
 *
 * Kept beside its declarations in @c client/state.h rather than
 * among the rest of @c client.c.
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
#include <stdlib.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb_icccm.h>

/* Project includes */
#include <client/state.h>


/* Adjust a frame position to keep a gravity anchor fixed across a
 * size change */
void client_gravity_adjust_pos(int32_t *restrict out_x,
        int32_t *restrict out_y,
        uint32_t old_w, uint32_t old_h,
        uint32_t new_w, uint32_t new_h,
        uint16_t gravity)
{
    int32_t dw = (int32_t) ((uint32_t) old_w - (uint32_t) new_w);
    int32_t dh = (int32_t) ((uint32_t) old_h - (uint32_t) new_h);

    if (gravity == (uint16_t) CLIENT_GRAVITY_NORTH_EAST ||
            gravity == (uint16_t) CLIENT_GRAVITY_EAST ||
            gravity == (uint16_t) CLIENT_GRAVITY_SOUTH_EAST) {
        *out_x = (int32_t) ((uint32_t) *out_x + (uint32_t) dw);
    } else if (gravity == (uint16_t) CLIENT_GRAVITY_NORTH ||
            gravity == (uint16_t) CLIENT_GRAVITY_CENTER ||
            gravity == (uint16_t) CLIENT_GRAVITY_SOUTH) {
        *out_x = (int32_t) ((uint32_t) *out_x + (uint32_t) (dw / 2));
    }

    if (gravity == (uint16_t) CLIENT_GRAVITY_SOUTH_EAST ||
            gravity == (uint16_t) CLIENT_GRAVITY_SOUTH ||
            gravity == (uint16_t) CLIENT_GRAVITY_SOUTH_WEST) {
        *out_y = (int32_t) ((uint32_t) *out_y + (uint32_t) dh);
    } else if (gravity == (uint16_t) CLIENT_GRAVITY_EAST ||
            gravity == (uint16_t) CLIENT_GRAVITY_CENTER ||
            gravity == (uint16_t) CLIENT_GRAVITY_WEST) {
        *out_y = (int32_t) ((uint32_t) *out_y + (uint32_t) (dh / 2));
    }
}
