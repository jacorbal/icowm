/**
 * @file wm/startup.c
 *
 * @brief Window manager startup helpers: extension probing
 *
 * Split by competency into @c wm/startup/install.c (registering signal
 * handlers), @c wm/startup/handle.c (the handlers themselves and the
 * flags they set), and @c wm/startup/subscribe.c (X server event
 * subscriptions), leaving this file with the two X extension probes
 * that belong to no single one of those.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdint.h>
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/sync.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/xcb/reply.h>

/* Project includes */
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <wm/startup.h>


/* Probe XRandR support and cache extension metadata in 'wm' */
int wm_startup_init_randr(wm_td *wm)
{
    const xcb_query_extension_reply_t *ext;
    xcb_randr_query_version_reply_t *ver_reply;
    xcb_generic_error_t *ver_error = NULL;
    xcb_randr_query_version_cookie_t ver_cookie;
    xcb_connection_t *connection = wm_connection(wm);
    list_td *surfaces = wm_surfaces(wm);

    if (wm == NULL || connection == NULL) {
        return -1;
    }

    wm_set_randr(wm, false, 0u);

    ext = xcb_get_extension_data(connection, &xcb_randr_id);
    if (ext == NULL || !ext->present) {
        LOGGER_NOTICE("XRandR extension is unavailable on this X server",
                L_NARG);
        return 0;
    }

    ver_cookie = xcb_randr_query_version(connection, 1u, 5u);
    ver_reply = xcb_randr_query_version_reply(connection,
            ver_cookie, &ver_error);
    if (ver_reply == NULL) {
        xcb_reply_log_error(ver_error, "the XRandR version");
        LOGGER_WARNING("Failed to query XRandR version;" \
                " disabling XRandR", L_NARG);
        return 0;
    }

    wm_set_randr(wm, true, ext->first_event);
    LOGGER_INFO("XRandR enabled (server version %u.%u, base event=%u)",
            (unsigned int) ver_reply->major_version,
            (unsigned int) ver_reply->minor_version,
            (unsigned int) ext->first_event);
    free(ver_reply);

    /* Query initial CRTC/output state for each managed surface so that
     * 'surface->randr' fields are populated before the first RandR
     * event arrives (needed by set_orientation/set_resolution) */
    for (list_item_td *node = list_head(surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *const surface = (surface_td *) list_data(node);
        xcb_randr_get_screen_resources_current_cookie_t res_cookie;
        xcb_randr_get_screen_resources_current_reply_t *res_reply;
        xcb_generic_error_t *res_error = NULL;
        xcb_randr_crtc_t *crtcs;
        int crtc_count;

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        res_cookie = xcb_randr_get_screen_resources_current(
                connection, surface->screen->root);
        res_reply = xcb_randr_get_screen_resources_current_reply(
                connection, res_cookie, &res_error);
        if (res_reply == NULL) {
            xcb_reply_log_error(res_error,
                    "a surface's XRandR screen resources");
            continue;
        }

        crtc_count =
            xcb_randr_get_screen_resources_current_crtcs_length(
                    res_reply);
        crtcs =
            xcb_randr_get_screen_resources_current_crtcs(res_reply);

        for (int ci = 0; ci < crtc_count; ++ci) {
            xcb_randr_get_crtc_info_cookie_t ci_cookie;
            xcb_randr_get_crtc_info_reply_t *crtc_info;
            xcb_generic_error_t *ci_error = NULL;

            ci_cookie = xcb_randr_get_crtc_info(connection,
                    crtcs[ci], res_reply->config_timestamp);
            crtc_info = xcb_randr_get_crtc_info_reply(
                    connection, ci_cookie, &ci_error);

            if (crtc_info == NULL) {
                xcb_reply_log_error(ci_error, "an XRandR CRTC");
                continue;
            }

            if (crtc_info->mode != XCB_NONE &&
                    crtc_info->num_outputs > 0) {
                const xcb_randr_output_t *out_ids =
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

        /* Apply any configured RandR output profile ('randr.json')
         * matching an output already connected at startup; a profile
         * for one that connects later is instead applied when
         * 'handler_randr_event' sees its own 'OUTPUT_CHANGE' */
        (void) surface_action_apply_randr_profiles(surface, false);
    }

    return 0;
}


/* Probe XSync extension support and cache metadata in 'wm' */
int wm_startup_init_sync(wm_td *wm)
{
    const xcb_query_extension_reply_t *ext;
    xcb_sync_initialize_reply_t *ver_reply;
    xcb_sync_initialize_cookie_t ver_cookie;
    xcb_generic_error_t *ver_error = NULL;
    xcb_connection_t *connection = wm_connection(wm);

    if (wm == NULL || connection == NULL) {
        return -1;
    }

    wm_set_sync(wm, false, 0u);

    ext = xcb_get_extension_data(connection, &xcb_sync_id);
    if (ext == NULL || !ext->present) {
        LOGGER_NOTICE("XSync extension is unavailable on this X server;" \
                " '_NET_WM_SYNC_REQUEST' will not be offered", L_NARG);
        return 0;
    }

    ver_cookie = xcb_sync_initialize(connection, 3u, 0u);
    ver_reply = xcb_sync_initialize_reply(connection, ver_cookie,
            &ver_error);
    if (ver_reply == NULL) {
        xcb_reply_log_error(ver_error, "the XSync version");
        LOGGER_WARNING("Failed to query XSync version;" \
                " disabling '_NET_WM_SYNC_REQUEST'", L_NARG);
        return 0;
    }

    wm_set_sync(wm, true, ext->first_event);
    LOGGER_INFO("XSync enabled (server version %u.%u, base event=%u)",
            (unsigned int) ver_reply->major_version,
            (unsigned int) ver_reply->minor_version,
            (unsigned int) ext->first_event);
    free(ver_reply);

    return 0;
}
