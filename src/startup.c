/**
 * @file startup.c
 *
 * @brief Window manager startup helpers implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L  /* sigaction, sigemptyset */


/* System includes */
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* free */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <startup.h>


/**
 * @brief Flag written by the signal handler to request a graceful
 *        shutdown
 */
static volatile sig_atomic_t s_stop_signal_received = 0;


/**
 * @brief Flag written by the @c SIGHUP handler to request
 *        a configuration reload
 */
static volatile sig_atomic_t s_reload_signal_received = 0;


/**
 * @brief Signal handler for termination signals
 *
 * Records the signal number; the actual shutdown is handled from the
 * normal execution context in the main loop via
 * @c startup_stop_requested.
 *
 * @param signum Number of the received signal
 */
static void s_startup_handle_signal(int signum)
{
    s_stop_signal_received = signum;
}


/**
 * @brief Signal handler for @c SIGHUP (configuration reload)
 *
 * Sets a flag consumed by @c startup_reload_requested.  The actual
 * reload is deferred to the main loop so that it runs in a safe context
 * without async-signal-safety constraints.
 *
 * @param signum Number of the received signal (always @c SIGHUP)
 */
static void s_startup_handle_reload(int signum)
{
    (void) signum;
    s_reload_signal_received = 1;
}


/* Subscribe to root window events on all managed surfaces */
int startup_subscribe_root_events(wm_td *wm)
{
    uint32_t values[1];

    if (wm == NULL || wm->surfaces == NULL || wm->connection == NULL) {
        return -1;
    }

    values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY   |
                XCB_EVENT_MASK_KEY_PRESS             |
                XCB_EVENT_MASK_KEY_RELEASE           |
                XCB_EVENT_MASK_BUTTON_PRESS          |
                XCB_EVENT_MASK_BUTTON_RELEASE        |
                XCB_EVENT_MASK_PROPERTY_CHANGE;

    for (list_item_td *node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        xcb_void_cookie_t cookie;
        xcb_generic_error_t *err;

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        cookie = xcb_change_window_attributes_checked(
                wm->connection, surface->screen->root,
                XCB_CW_EVENT_MASK, values);
        err = xcb_request_check(wm->connection, cookie);
        if (err != NULL) {
            LOGGER_FATAL("Cannot subscribe to root events on" \
                    " surface %u; another window manager may be" \
                    " running (XCB error code %d)",
                    surface->id, err->error_code);
            free(err);
            return -1;
        }

        LOGGER_DEBUG("Subscribed to root events on surface %u"
                " (root %#x)", surface->id, surface->screen->root);
    }

    xcb_flush(wm->connection);
    return 0;
}


/* Install POSIX signal handlers for graceful termination */
int startup_install_signals(void)
{
    struct sigaction sa;
    struct sigaction sa_hup;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = s_startup_handle_signal;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    memset(&sa_hup, 0, sizeof(sa_hup));
    sa_hup.sa_handler = s_startup_handle_reload;
    sa_hup.sa_flags = 0;
    sigemptyset(&sa_hup.sa_mask);
    if (sigaction(SIGHUP, &sa_hup, NULL) != 0 ||
            sigaction(SIGINT, &sa, NULL) != 0 ||
            sigaction(SIGQUIT, &sa, NULL) != 0 ||
            sigaction(SIGTERM, &sa, NULL) != 0) {
        LOGGER_ERROR("Failed to install termination signal handlers",
                L_NARG);
        return -1;
    }

    return 0;
}


/* Query whether a termination signal has been received */
bool startup_stop_requested(void)
{
    return s_stop_signal_received != 0;
}


/* Query whether a 'SIGHUP' configuration-reload request was received */
bool startup_reload_requested(void)
{
    if (s_reload_signal_received != 0) {
        s_reload_signal_received = 0;
        return true;
    }

    return false;
}

