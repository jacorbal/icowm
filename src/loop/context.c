/**
 * @file loop/context.c
 *
 * @brief Cached per-run state shared by the main event loop
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

/* Project includes */
#include <wm.h>

/* Local includes */
#include <loop/context.h>


/* Resolve the window manager values the loop caches for a run */
bool loop_context_init(loop_ctx_td *ctx, wm_td *wm)
{
    if (ctx == NULL || wm == NULL) {
        return false;
    }

    ctx->wm = wm;
    ctx->connection = wm_connection(wm);
    ctx->surfaces = wm_surfaces(wm);
    ctx->config = wm_config(wm);
    ctx->keysyms = NULL;
    ctx->pending_event = NULL;
    ctx->restricted_memory_mib = wm_restricted_memory_mib(wm);
    ctx->randr_base_event = wm_randr_base_event(wm);
    ctx->sync_base_event = wm_sync_base_event(wm);
    ctx->is_randr_available = wm_randr_available(wm);
    ctx->is_sync_available = wm_sync_available(wm);

    return true;
}
