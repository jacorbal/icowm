/**
 * @file ipc/resolve.h
 *
 * @brief Turning a request's own numeric IDs into real pointers
 *
 * Every command's own arguments name a client, desktop, or surface by
 * the same numeric ID @c list_clients / @c list_desktops /
 * @c get_focused already report it by: a client's own X window ID,
 * a desktop's own index on its surface, a surface's own screen index.
 * These turn one of those IDs back into the real pointer it names, or
 * build the appropriate error response when it does not currently name
 * anything, so every action handler shares one place that does this
 * instead of repeating the same lookup and error wording on its own.
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

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <surface.h>
#include <wm.h>


/* Public interface */
/**
 * @brief Resolve which surface a request refers to
 *
 * Reads an optional numeric @c surface_id field from @p args; when
 * absent, falls back to the first surface in @p wm's own list, the only
 * reasonable default on a single-monitor setup and still a usable one
 * on a multi-monitor one.
 *
 * @param wm   Window manager instance
 * @param args The request object
 *
 * @return The resolved surface, or @c NULL when @c surface_id was given
 *         but does not match any currently managed surface, or when
 *         there is no surface to fall back to at all
 *
 * @note Complexity: @e O(1)
 */
surface_td *ipc_resolve_surface(const wm_td *wm, const cJSON *args);

/**
 * @brief Resolve which desktop a request refers to, on top of
 *        @a ipc_resolve_surface
 *
 * @param wm                  Window manager instance
 * @param args                The request object
 * @param desktop_id_required When @c true, a missing or invalid
 *                            @c desktop_id is itself a failure;
 *                            when @c false, a missing one falls back
 *                            to the resolved surface's own current
 *                            desktop instead (an out-of-range one
 *                            that was actually given is always a
 *                            failure either way)
 * @param out_surface         Receives the resolved surface on
 *                            success; untouched on failure
 * @param out_error           Receives a newly allocated error
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
        bool desktop_id_required, surface_td **out_surface,
        cJSON **out_error);

/**
 * @brief Resolve which client a request refers to
 *
 * @param wm          Window manager instance
 * @param args        The request object; must have a numeric @c client_id
 * @param out_surface Receives the client's own surface on success;
 *                    untouched on failure
 * @param out_desktop Receives the client's own desktop on success;
 *                    untouched on failure
 * @param out_error   Receives a newly allocated error response on
 *                    failure (possibly null itself, on an allocation
 *                    failure building that response); untouched on
 *                    success
 *
 * @return The resolved client, or @c NULL on failure (see @p out_error
 *         to know why)
 *
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e c the
 *       hash-table lookup cost per desktop
 */
client_td *ipc_resolve_client(const wm_td *wm, const cJSON *args,
        surface_td **out_surface, desktop_td **out_desktop,
        cJSON **out_error);


#endif  /* ! IPC_RESOLVE_H */
