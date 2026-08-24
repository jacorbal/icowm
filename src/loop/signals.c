/**
 * @file loop/signals.c
 *
 * @brief Deferred signal handling for the main event loop
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

/* Session includes */
#include <session.h>

/* Input includes */
#include <input/kbd/bind.h>
#include <input/mouse/bind.h>

/* Project includes */
#include <logger.h>
#include <wm.h>
#include <wm/startup/handle.h>

/* Local includes */
#include <loop/signals.h>


/* Act on whichever signal flags were raised since last checked */
bool loop_signals_process(const loop_ctx_td *ctx)
{
    if (ctx == NULL) {
        return false;
    }

    if (wm_startup_requested_stop()) {
        LOGGER_INFO("Termination signal received;" \
                " requesting shutdown", L_NARG);
        wm_request_stop();
        return false;
    }

    if (wm_startup_requested_reload()) {
        LOGGER_INFO("'SIGHUP' received; reloading configuration",
                L_NARG);
        (void) wm_action_config_reload(ctx->wm);
    }

    if (wm_startup_requested_resume()) {
        LOGGER_INFO("'SIGCONT' received; re-establishing" \
                " input grabs", L_NARG);
        keyboard_load(ctx->surfaces, ctx->keysyms, ctx->config);
        mouse_load(ctx->surfaces, ctx->config);
    }

    if (wm_startup_requested_child_reap()) {
        session_reap_children();
    }

    return true;
}
