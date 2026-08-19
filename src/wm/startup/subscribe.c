/**
 * @file wm/startup/subscribe.c
 *
 * @brief Subscribing to X server events on every managed surface
 *
 * Split out of what used to be a single, flat @c startup.c.
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

/* ADT includes */
#include <adt/list.h>

/* Default initial values */
#include <defs/cursor.h>

/* Utils includes */
#include <utils/cursor.h>

/* Project includes */
#include <logger.h>
#include <surface.h>

/* Local includes */
#include <wm/startup/subscribe.h>


/* Subscribe to XRandR notifications on each managed root window */
int wm_startup_subscribe_randr_events(const wm_td *wm)
{
    xcb_connection_t *connection = wm_connection(wm);
    list_td *surfaces = wm_surfaces(wm);

    if (wm == NULL || surfaces == NULL || connection == NULL) {
        return -1;
    }

    if (!wm_randr_available(wm)) {
        return 0;
    }

    for (list_item_td *node = list_head(surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *const surface = (surface_td *) list_data(node);
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

        cookie = xcb_randr_select_input_checked(connection,
                surface->screen->root, mask);
        err = xcb_request_check(connection, cookie);
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

    xcb_flush(connection);
    return 0;
}


/* Subscribe to root window events on all managed surfaces */
int wm_startup_subscribe_root_events(const wm_td *wm)
{
    uint32_t values[1];
    xcb_cursor_t cur;
    uint32_t cur_val[1];
    surface_td *first_surface;
    xcb_connection_t *connection = wm_connection(wm);
    list_td *surfaces = wm_surfaces(wm);

    if (wm == NULL || surfaces == NULL || connection == NULL) {
        return -1;
    }

    values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY   |
                XCB_EVENT_MASK_KEY_PRESS             |
                XCB_EVENT_MASK_KEY_RELEASE           |
                XCB_EVENT_MASK_BUTTON_PRESS          |
                XCB_EVENT_MASK_BUTTON_RELEASE        |
                XCB_EVENT_MASK_PROPERTY_CHANGE;

    for (list_item_td *node = list_head(surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        xcb_void_cookie_t cookie;
        xcb_generic_error_t *err;

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        cookie = xcb_change_window_attributes_checked(
                connection, surface->screen->root,
                XCB_CW_EVENT_MASK, values);
        err = xcb_request_check(connection, cookie);
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
     * pointer.  Loaded from the active cursor theme via
     * 'util_cursor_load' (see 'utils/cursor.h'), falling back to the
     * X core cursor font automatically if the theme has no
     * "left_ptr" cursor; see 'defs/cursor.h' for that fallback
     * glyph's named constant. */
    first_surface = NULL;
    for (list_item_td *node = list_head(surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);

        if (surface != NULL && surface->screen != NULL) {
            first_surface = surface;
            break;
        }
    }

    cur = XCB_NONE;
    if (first_surface != NULL) {
        util_cursor_ctx_td *const ctx = util_cursor_ctx_new(connection,
                first_surface->screen);

        cur = util_cursor_load(ctx, "left_ptr", WM_CURSOR_LEFT_PTR_GLYPH);
        util_cursor_ctx_free(ctx);
    }
    if (cur == XCB_NONE) {
        xcb_flush(connection);
        return 0;
    }
    cur_val[0] = (uint32_t) cur;
    for (list_item_td *cn = list_head(surfaces);
            cn != NULL; cn = list_next(cn)) {
        surface_td *const sv = (surface_td *) list_data(cn);
        if (sv == NULL || sv->screen == NULL) {
            continue;
        }
        xcb_change_window_attributes(connection,
                sv->screen->root, XCB_CW_CURSOR, cur_val);
    }
    xcb_free_cursor(connection, cur);

    xcb_flush(connection);
    return 0;
}
