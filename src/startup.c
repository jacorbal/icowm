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

#define _POSIX_C_SOURCE 200112L /* sigaction, sigemptyset */


/* System includes */
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* free */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/sync.h>

/* ADT includes */
#include <adt/list.h>

/* Default initial values */
#include <defs/wm.h>

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
 * @brief Flag written by @c SIGCONT (VT resume) to re-establish input
 * grabs
 */
static volatile sig_atomic_t s_resume_signal_received = 0;

/**
 * @brief Flag written by the @c SIGHUP handler to request
 *        a configuration reload
 */
static volatile sig_atomic_t s_reload_signal_received = 0;

/**
 * @brief Flag written by @c SIGCHLD so the main loop can reap children
 */
static volatile sig_atomic_t s_child_reap_requested = 0;


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


/**
 * @brief Signal handler for @c SIGCONT (VT resume)
 *
 * Sets a flag consumed by @c startup_resume_requested so that the main
 * loop can re-establish keyboard and mouse grabs after returning from
 * a virtual-terminal switch.
 *
 * @param signum Number of the received signal (always @c SIGCONT)
 */
static void s_startup_handle_resume(int signum)
{
    (void) signum;
    s_resume_signal_received = 1;
}


/**
 * @brief Signal handler for @c SIGCHLD
 *
 * Defers child reaping to the main loop so @c waitpid is only called in
 * normal execution context.
 *
 * @param signum Number of the received signal (always @c SIGCHLD)
 */
static void s_startup_handle_child(int signum)
{
    (void) signum;
    s_child_reap_requested = 1;
}


/**
 * @brief Install a single POSIX signal handler
 *
 * @param signum Signal number to configure
 * @param handler Function to invoke when the signal arrives
 * @param flags   Extra @c sigaction flags for the registration
 *
 * @return 0 on success, -1 if @c sigaction fails
 */
static int s_startup_install_handler(int signum,
        void (*handler)(int), int flags)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sa.sa_flags = flags;
    sigemptyset(&sa.sa_mask);

    return sigaction(signum, &sa, NULL);
}


/* Probe XRandR support and cache extension metadata in 'wm' */
int startup_randr_init(wm_td *wm)
{
    const xcb_query_extension_reply_t *ext;
    xcb_randr_query_version_reply_t *ver_reply;
    xcb_randr_query_version_cookie_t ver_cookie;

    if (wm == NULL || wm->connection == NULL) {
        return -1;
    }

    wm->randr_available = false;
    wm->randr_base_event = 0u;

    ext = xcb_get_extension_data(wm->connection, &xcb_randr_id);
    if (ext == NULL || !ext->present) {
        LOGGER_NOTICE("XRandR extension is unavailable on this X server",
                L_NARG);
        return 0;
    }

    ver_cookie = xcb_randr_query_version(wm->connection, 1u, 5u);
    ver_reply = xcb_randr_query_version_reply(wm->connection,
            ver_cookie, NULL);
    if (ver_reply == NULL) {
        LOGGER_WARNING("Failed to query XRandR version;" \
                " disabling XRandR", L_NARG);
        return 0;
    }

    wm->randr_available = true;
    wm->randr_base_event = ext->first_event;
    LOGGER_INFO("XRandR enabled (server version %u.%u, base event=%u)",
            (unsigned int) ver_reply->major_version,
            (unsigned int) ver_reply->minor_version,
            (unsigned int) wm->randr_base_event);
    free(ver_reply);

    /* Query initial CRTC/output state for each managed surface so that
     * 'surface->randr' fields are populated before the first RandR
     * event arrives (needed by set_orientation/set_resolution) */
    for (list_item_td *node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        xcb_randr_get_screen_resources_current_cookie_t res_cookie;
        xcb_randr_get_screen_resources_current_reply_t *res_reply;
        xcb_randr_crtc_t *crtcs;
        int crtc_count;

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        res_cookie = xcb_randr_get_screen_resources_current(
                wm->connection, surface->screen->root);
        res_reply = xcb_randr_get_screen_resources_current_reply(
                wm->connection, res_cookie, NULL);
        if (res_reply == NULL) {
            continue;
        }

        crtc_count =
            xcb_randr_get_screen_resources_current_crtcs_length(
                    res_reply);
        crtcs =
            xcb_randr_get_screen_resources_current_crtcs(res_reply);

        for (int ci = 0; ci < crtc_count; ci++) {
            xcb_randr_get_crtc_info_cookie_t ci_cookie;
            xcb_randr_get_crtc_info_reply_t *crtc_info;

            ci_cookie = xcb_randr_get_crtc_info(wm->connection,
                    crtcs[ci], res_reply->config_timestamp);
            crtc_info = xcb_randr_get_crtc_info_reply(
                    wm->connection, ci_cookie, NULL);

            if (crtc_info == NULL) {
                continue;
            }

            if (crtc_info->mode != XCB_NONE &&
                    crtc_info->num_outputs > 0) {
                xcb_randr_output_t *out_ids =
                    xcb_randr_get_crtc_info_outputs(crtc_info);

                surface->randr.is_known = true;
                surface->randr.crtc_id = (uint32_t) crtcs[ci];
                surface->randr.mode_id = (uint32_t) crtc_info->mode;
                surface->randr.rotation = crtc_info->rotation;
                surface->randr.output_id = (uint32_t) out_ids[0];

                LOGGER_DEBUG("Surface %u: initial CRTC %u, mode %u,"
                        " output %u, rotation %u",
                        surface->id,
                        surface->randr.crtc_id,
                        surface->randr.mode_id,
                        surface->randr.output_id,
                        (unsigned int) surface->randr.rotation);

                free(crtc_info);
                break;  /* Take the first active CRTC */
            }
            free(crtc_info);
        }

        free(res_reply);
    }

    return 0;
}


/* Probe XSync extension support and cache metadata in 'wm' */
int startup_sync_init(wm_td *wm)
{
    const xcb_query_extension_reply_t *ext;
    xcb_sync_initialize_reply_t *ver_reply;
    xcb_sync_initialize_cookie_t ver_cookie;

    if (wm == NULL || wm->connection == NULL) {
        return -1;
    }

    wm->sync_available = false;
    wm->sync_base_event = 0u;

    ext = xcb_get_extension_data(wm->connection, &xcb_sync_id);
    if (ext == NULL || !ext->present) {
        LOGGER_NOTICE("XSync extension is unavailable on this X server;" \
                " '_NET_WM_SYNC_REQUEST' will not be offered", L_NARG);
        return 0;
    }

    ver_cookie = xcb_sync_initialize(wm->connection, 3u, 0u);
    ver_reply = xcb_sync_initialize_reply(wm->connection, ver_cookie, NULL);
    if (ver_reply == NULL) {
        LOGGER_WARNING("Failed to query XSync version;" \
                " disabling '_NET_WM_SYNC_REQUEST'", L_NARG);
        return 0;
    }

    wm->sync_available = true;
    wm->sync_base_event = ext->first_event;
    LOGGER_INFO("XSync enabled (server version %u.%u, base event=%u)",
            (unsigned int) ver_reply->major_version,
            (unsigned int) ver_reply->minor_version,
            (unsigned int) wm->sync_base_event);
    free(ver_reply);

    return 0;
}


/* Subscribe to XRandR notifications on each managed root window */
int startup_subscribe_randr_events(wm_td *wm)
{
    if (wm == NULL || wm->surfaces == NULL || wm->connection == NULL) {
        return -1;
    }

    if (!wm->randr_available) {
        return 0;
    }

    for (list_item_td *node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        xcb_void_cookie_t cookie;
        xcb_generic_error_t *err;
        uint16_t mask;

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        mask = XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
               XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE   |
               XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
               XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY;

        cookie = xcb_randr_select_input_checked(wm->connection,
                surface->screen->root, mask);
        err = xcb_request_check(wm->connection, cookie);
        if (err != NULL) {
            LOGGER_WARNING("Failed to subscribe XRandR events on"
                    " surface %u (XCB error code %u)",
                    surface->id, (unsigned int) err->error_code);
            free(err);
            continue;
        }

        LOGGER_DEBUG("Subscribed XRandR events on surface %u"
                " (root %#x)", surface->id, surface->screen->root);
    }

    xcb_flush(wm->connection);
    return 0;
}


/* Subscribe to root window events on all managed surfaces */
int startup_subscribe_root_events(wm_td *wm)
{
    uint32_t values[1];
    xcb_font_t fnt;
    xcb_cursor_t cur;
    uint32_t cur_val[1];

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

    /* Set a default left-pointer cursor on every root window so the
     * cursor is visible even when no client window is under the
     * pointer.  The cursor font stores glyphs in pairs; see 'defs/wm.h'
     * for the named constants. */
    fnt = xcb_generate_id(wm->connection);
    cur = xcb_generate_id(wm->connection);
    xcb_open_font(wm->connection, fnt,
            (uint16_t) strlen("cursor"), "cursor");
    xcb_create_glyph_cursor(wm->connection, cur, fnt, fnt,
            WM_CURSOR_LEFT_PTR_GLYPH, WM_CURSOR_LEFT_PTR_MASK_GLYPH,
            0u, 0u, 0u,
            0xffffu, 0xffffu, 0xffffu);
    cur_val[0] = (uint32_t) cur;
    for (list_item_td *cn = list_head(wm->surfaces);
            cn != NULL; cn = list_next(cn)) {
        surface_td *sv = (surface_td *) list_data(cn);
        if (sv == NULL || sv->screen == NULL) {
            continue;
        }
        xcb_change_window_attributes(wm->connection,
                sv->screen->root, XCB_CW_CURSOR, cur_val);
    }
    xcb_free_cursor(wm->connection, cur);
    xcb_close_font(wm->connection, fnt);

    xcb_flush(wm->connection);
    return 0;
}


/* Install POSIX signal handlers for graceful termination */
int startup_install_signals(void)
{
    if (s_startup_install_handler(SIGHUP,
                s_startup_handle_reload, 0) != 0 ||
            s_startup_install_handler(SIGINT,
                s_startup_handle_signal, 0) != 0 ||
            s_startup_install_handler(SIGQUIT,
                s_startup_handle_signal, 0) != 0 ||
            s_startup_install_handler(SIGTERM,
                s_startup_handle_signal, 0) != 0 ||
            s_startup_install_handler(SIGCONT,
                s_startup_handle_resume, 0) != 0 ||
            s_startup_install_handler(SIGCHLD,
                s_startup_handle_child, SA_NOCLDSTOP) != 0) {
        LOGGER_ERROR("Failed to install startup signal handlers",
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


/* Query whether a 'SIGCONT' (VT resume) was received */
bool startup_resume_requested(void)
{
    if (s_resume_signal_received != 0) {
        s_resume_signal_received = 0;
        return true;
    }

    return false;
}


/* Query whether a pending child-reap request was received */
bool startup_child_reap_requested(void)
{
    if (s_child_reap_requested != 0) {
        s_child_reap_requested = 0;
        return true;
    }

    return false;
}
