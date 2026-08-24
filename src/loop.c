/**
 * @file loop.c
 *
 * @brief Main event loop
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
#include <stdlib.h>     /* free, NULL */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Input includes */
#include <input/kbd/bind.h>
#include <input/mouse/bind.h>

/* Project includes */
#include <cctl/adopt.h>
#include <logger.h>
#include <wm/startup/install.h>
#include <wm.h>

/* Local includes */
#include <loop.h>
#include <loop/context.h>
#include <loop/dispatch.h>
#include <loop/pollset.h>
#include <loop/refresh.h>
#include <loop/signals.h>
#include <loop/timers.h>


/* Run the main event loop until the window manager is stopped */
void loop_run(wm_td *wm)
{
    loop_ctx_td ctx;

    if (wm == NULL || !wm_is_running(wm)) {
        LOGGER_TRACE("Window manager is not initialized or" \
                " set to not run", L_NARG);
        return;
    }

    if (!loop_context_init(&ctx, wm)) {
        LOGGER_ERROR("Failed to resolve main loop context", L_NARG);
        return;
    }

    if (wm_startup_install_signals() != 0) {
        LOGGER_WARNING("Continuing without termination signal handling",
                L_NARG);
    }

    if (wm_startup_install_crash_handlers() != 0) {
        LOGGER_WARNING("Continuing without fatal-signal diagnostics",
                L_NARG);
    }

    ctx.keysyms = xcb_key_symbols_alloc(ctx.connection);
    if (ctx.keysyms == NULL) {
        LOGGER_ERROR("Failed to allocate key symbols table", L_NARG);
        return;
    }
    wm_set_keysyms(wm, ctx.keysyms);

    keyboard_load(ctx.surfaces, ctx.keysyms, ctx.config);
    mouse_load(ctx.surfaces, ctx.config);

    cctl_adopt_scan(wm);
    loop_refresh_full(&ctx);

    /* Synchronize EWMH root properties after the initial scan so that
     * taskbars reading '_NET_CLIENT_LIST' see the windows that were
     * adopted by 'cctl_adopt_scan'.  The earlier 'wm_ewmh_sync'
     * call in 'wm_init' ran before any clients were managed, leaving
     * the list empty; 'loop_refresh_full' then cleared 'is_outdated',
     * so the first main-loop iteration would never trigger a sync on
     * its own. */
    wm_ewmh_sync(wm);

    LOGGER_DEBUG("Entering main event loop", L_NARG);

    while (wm_is_running(wm)) {
        xcb_generic_event_t *event;

        if (!loop_signals_process(&ctx)) {
            break;
        }

        if (!loop_pollset_wait(&ctx, loop_timers_timeout(&ctx))) {
            break;
        }

        loop_timers_tick(&ctx);

        while ((event = (ctx.pending_event != NULL)
                    ? ctx.pending_event
                    : xcb_poll_for_event(ctx.connection)) != NULL) {
            ctx.pending_event = NULL;
            loop_dispatch_event(&ctx, &event);
            free(event);
        }

        loop_refresh(&ctx);
    }

    LOGGER_DEBUG("Exiting event loop", L_NARG);
    xcb_key_symbols_free(ctx.keysyms);
    wm_set_keysyms(wm, NULL);
}
