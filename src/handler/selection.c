/**
 * @file handler/selection.c
 *
 * @brief X @c SELECTION_CLEAR event handler
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <logger.h>
#include <wm.h>

/* Local includes */
#include <handler.h>
#include <handler/selection.h>
#include <wm/shutdown.h>


/* Handle a 'SELECTION_CLEAR' event */
void handler_selection_clear(wm_td *wm,
        const xcb_selection_clear_event_t *event)
{
    if (wm == NULL || event == NULL) {
        return;
    }

    if (event->owner != wm_ewmh_support_win(wm)) {
        return;
    }

    LOGGER_NOTICE("Manager selection ownership was taken over by" \
            " another window manager (window 0x%08x); shutting down",
            event->owner);
    wm_shutdown_begin(wm);
}
