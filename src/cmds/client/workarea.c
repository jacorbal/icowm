/**
 * @file cmds/client/workarea.c
 *
 * @brief Client-relative workarea resolution
 *
 * One of the files @c cmds/client/ is made of.  Its single function
 * today, kept separate from @c move.c/@c maximize.c regardless: shared
 * almost evenly by positioning (centering, moving to a corner) and
 * maximize/fullscreen sizing alike, it belongs to neither one on its
 * own.
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
#include <stdint.h>

/* Util includes */
#include <utils/geom.h>

/* Stage includes */
#include <stage/desktop.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <cmds/client/screen.h>
#include <cmds/client/workarea.h>


/* Resolve the on-screen workarea for whichever monitor a client
 * currently sits on */
bool ccmd_client_resolve_workarea(client_td *client,
        int32_t *restrict out_x, int32_t *restrict out_y,
        uint16_t *restrict out_w, uint16_t *restrict out_h)
{
    stage_td *stage = NULL;
    const desktop_td *desktop;
    monitor_td monitor;
    struct geometry_s clipped;
    uint32_t did;

    if (client == NULL || out_w == NULL || out_h == NULL) {
        return false;
    }

    if (!ccmd_client_monitor(client, &stage, &monitor)) {
        return false;
    }

    did = (client->desktop_id == WM_DESKTOP_ID_ALL)
        ? stage->desktop_cur
        : client->desktop_id;
    desktop = stage_desktop_get(stage, did);
    if (desktop == NULL || desktop->workarea.dim.w == 0u ||
            desktop->workarea.dim.h == 0u) {
        return false;
    }

    clipped = geom_intersect_rect(
            desktop->workarea.pos.x, desktop->workarea.pos.y,
            desktop->workarea.dim.w, desktop->workarea.dim.h,
            monitor.x, monitor.y,
            monitor.w, monitor.h);
    if (clipped.dim.w == 0u || clipped.dim.h == 0u) {
        return false;
    }

    if (out_x != NULL) {
        *out_x = clipped.pos.x;
    }
    if (out_y != NULL) {
        *out_y = clipped.pos.y;
    }
    *out_w = geom_dim_clamp((int32_t) clipped.dim.w);
    *out_h = geom_dim_clamp((int32_t) clipped.dim.h);

    return true;
}
