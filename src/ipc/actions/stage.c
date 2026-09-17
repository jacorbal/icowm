/**
 * @file ipc/actions/stage.c
 *
 * @brief IPC commands mirroring enact.h's
 *        enact_stage_desktop_switch* actions implementation
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

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <desktop.h>
#include <enact.h>
#include <enact/stage.h>
#include <memguard.h>
#include <stage.h>
#include <wm.h>

/* Local includes */
#include <ipc/resolve.h>
#include <ipc/response.h>
#include <ipc/actions/stage.h>


cJSON *ipc_action_goto_desktop(const wm_td *wm, const cJSON *args)
{
    stage_td *stage = NULL;
    const desktop_td *desktop;
    cJSON *error = NULL;

    desktop = ipc_resolve_desktop(wm, args, true, &stage, &error);
    if (desktop == NULL) {
        return error;
    }

    enact_stage_desktop_switch(stage, desktop->id);
    return ipc_response_ok();
}


cJSON *ipc_action_goto_north_desktop(const wm_td *wm, const cJSON *args)
{
    stage_td *const stage = ipc_resolve_stage(wm, args);

    if (stage == NULL) {
        return ipc_response_error("no such stage");
    }

    enact_stage_desktop_switch_north(stage);
    return ipc_response_ok();
}


cJSON *ipc_action_goto_south_desktop(const wm_td *wm, const cJSON *args)
{
    stage_td *const stage = ipc_resolve_stage(wm, args);

    if (stage == NULL) {
        return ipc_response_error("no such stage");
    }

    enact_stage_desktop_switch_south(stage);
    return ipc_response_ok();
}


cJSON *ipc_action_goto_east_desktop(const wm_td *wm, const cJSON *args)
{
    stage_td *const stage = ipc_resolve_stage(wm, args);

    if (stage == NULL) {
        return ipc_response_error("no such stage");
    }

    enact_stage_desktop_switch_east(stage);
    return ipc_response_ok();
}


cJSON *ipc_action_goto_west_desktop(const wm_td *wm, const cJSON *args)
{
    stage_td *const stage = ipc_resolve_stage(wm, args);

    if (stage == NULL) {
        return ipc_response_error("no such stage");
    }

    enact_stage_desktop_switch_west(stage);
    return ipc_response_ok();
}


cJSON *ipc_action_add_desktop(const wm_td *wm, const cJSON *args)
{
    stage_td *const stage = ipc_resolve_stage(wm, args);

    if (stage == NULL) {
        return ipc_response_error("no such stage");
    }

    if (memguard_max_clients() > 0u) {
        return ipc_response_error(
                "restricted-memory mode is locked to a single desktop");
    }

    if (stage->desktop_count >= (uint32_t) CONFIG_MAX_DESKTOPS) {
        return ipc_response_error(
                "already at the configured maximum number of desktops");
    }

    enact_stage_desktop_add(stage);
    return ipc_response_ok();
}


cJSON *ipc_action_remove_desktop(const wm_td *wm, const cJSON *args)
{
    stage_td *const stage = ipc_resolve_stage(wm, args);

    if (stage == NULL) {
        return ipc_response_error("no such stage");
    }

    if (stage->desktop_count <= 1) {
        return ipc_response_error("cannot remove the last desktop");
    }

    enact_stage_desktop_remove(stage);
    return ipc_response_ok();
}


cJSON *ipc_action_toggle_strutless_maximize(const wm_td *wm,
        const cJSON *args)
{
    stage_td *const stage = ipc_resolve_stage(wm, args);

    if (stage == NULL) {
        return ipc_response_error("no such stage");
    }

    enact_stage_toggle_strutless_maximize(stage);
    return ipc_response_ok();
}
