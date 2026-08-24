/**
 * @file input/mouse/drag/resist.c
 *
 * @brief Resistance-threshold math for a maximized-axis mouse resize
 *
 * One of the files @c input/mouse/drag/ is made of; see
 * @c drag/internal.h for why, the same reasoning @c drag/snap.c
 * follows for its unrelated math.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* Project includes */
#include <client.h>

/* Command includes */
#include <cmds/client/maximize.h>

/* Local includes */
#include <input/mouse/drag/internal.h>
#include <input/mouse/drag/resist.h>


/* Recompute a maximized-on-one-axis client's resistance state
 * for the current motion event, live */
void drag_resist_axis_update(uint32_t drag_dist_w, uint32_t drag_dist_h,
        uint32_t resistance)
{
    if (s_drag.is_resist_axis_w) {
        bool was_resize_w = s_drag.is_resize_w;

        s_drag.is_resize_w = drag_dist_w >= resistance;
        if (s_drag.is_solid_drag && s_drag.is_resize_w != was_resize_w) {
            if (s_drag.is_resize_w) {
                ccmd_client_demote_axis_state(s_drag.client, 1);
            } else {
                ccmd_client_promote_axis_state(s_drag.client, 1);
            }
        }
    }
    if (s_drag.is_resist_axis_h) {
        bool was_resize_h = s_drag.is_resize_h;

        s_drag.is_resize_h = drag_dist_h >= resistance;
        if (s_drag.is_solid_drag && s_drag.is_resize_h != was_resize_h) {
            if (s_drag.is_resize_h) {
                ccmd_client_demote_axis_state(s_drag.client, 2);
            } else {
                ccmd_client_promote_axis_state(s_drag.client, 2);
            }
        }
    }
}


/* Settle a maximize-locked axis's final state once a resize
 * drag ends, for whichever case drag_resist_axis_update's live
 * sync could not already handle */
void drag_resist_axis_finalize(bool finalize_resize)
{
    if (!finalize_resize || s_drag.is_solid_drag) {
        return;
    }

    if (s_drag.is_resist_axis_w && s_drag.is_resize_w) {
        ccmd_client_demote_axis_state(s_drag.client, 1);
    }
    if (s_drag.is_resist_axis_h && s_drag.is_resize_h) {
        ccmd_client_demote_axis_state(s_drag.client, 2);
    }
}
