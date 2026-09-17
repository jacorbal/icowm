/**
 * @file ipc/resolve.h
 *
 * @brief Turning a request's numeric IDs into real pointers
 *
 * Every command's arguments name a client, desktop, or stage by the
 * same numeric ID @c list_clients / @c list_desktops / @c get_focused
 * already report it by: a client's X window ID, a desktop's index on
 * its stage, a stage's screen index.  These turn one of those IDs
 * back into the real pointer it names, or build the appropriate error
 * response when it does not currently name anything, so every action
 * handler shares one place that does this instead of repeating the same
 * lookup and error wording on its own.
 *
 * @defgroup ipc_resolve IPC ID resolution
 * @ingroup ipc
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_RESOLVE_H
#define IPC_RESOLVE_H


/* System includes */
#include <stdbool.h>

/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>


/**
 * @brief Resolve which stage a request refers to
 *
 * Reads an optional numeric @c stage_id field from @p args; when
 * absent, falls back to the first stage in @p wm's list, the only
 * reasonable default on a single-monitor setup and still a usable one
 * on a multi-monitor one.
 *
 * @param wm   Window manager instance
 * @param args The request object
 *
 * @return The resolved stage, or @c NULL when @c stage_id was given
 *         but does not match any currently managed stage, or when
 *         there is no stage to fall back to at all
 *
 * @note Complexity: @e O(1)
 */
stage_td *ipc_resolve_stage(const wm_td *wm, const cJSON *args);

/**
 * @brief Resolve which desktop a request refers to, on top of
 *        @a ipc_resolve_stage
 *
 * @param wm                  Window manager instance
 * @param args                The request object
 * @param desktop_id_required When @c true, a missing or invalid
 *                            @c desktop_id is itself a failure;
 *                            when @c false, a missing one falls back
 *                            to the resolved stage's current
 *                            desktop instead (an out-of-range one
 *                            that was actually given is always a
 *                            failure either way)
 * @param out_stage Receives the resolved stage on
 *                            success; untouched on failure
 * @param out_error Receives a newly allocated error
 *                            response on failure (possibly null itself,
 *                            on an allocation failure building that
 *                            response); untouched on success
 *
 * @return The resolved desktop, or @c NULL on failure (see @p out_error
 *         for why if you are that interested)
 *
 * @note Complexity: @e O(1)
 */
desktop_td *ipc_resolve_desktop(const wm_td *wm, const cJSON *args,
        bool desktop_id_required, stage_td **out_stage,
        cJSON **out_error);

/**
 * @brief Resolve which client a request refers to
 *
 * @param wm   Window manager instance
 * @param args The request object; must carry a numeric
 *                    @c client_id
 * @param out_stage Receives the client's stage on success;
 *                    untouched on failure
 * @param out_desktop Receives the client's desktop on success;
 *                    untouched on failure
 * @param out_error Receives a newly allocated error response on
 *                    failure (possibly null itself, on an allocation
 *                    failure building that response); untouched on
 *                    success
 *
 * @return The resolved client, or @c NULL on failure (see @p out_error
 *         to know why)
 *
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       stages, @e d the number of desktops per stage, and @e c the
 *       hash-table lookup cost per desktop
 */
client_td *ipc_resolve_client(const wm_td *wm, const cJSON *args,
        stage_td **out_stage, desktop_td **out_desktop,
        cJSON **out_error);


#endif  /* ! IPC_RESOLVE_H */
