/**
 * @file loop/refresh.c
 *
 * @brief End-of-iteration repaint and property sync for the main loop
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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Render includes */
#include <render/surface.h>

/* Menu includes */
#include <menu/notify/desktop.h>
#include <menu/popup.h>

/* Project includes */
#include <logger.h>
#include <surface.h>
#include <wm.h>
#include <wm/ewmh.h>

/* Local includes */
#include <loop/refresh.h>
#include <utils/xcb/connection.h>


/**
 * @brief Re-render only the surfaces marked as outdated
 *
 * @param ctx Main loop context
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
static void s_loop_refresh_outdated(const loop_ctx_td *ctx)
{
    if (ctx == NULL || ctx->surfaces == NULL) {
        return;
    }

    for (list_item_td *node = list_head(ctx->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *const surface = (surface_td *) list_data(node);
        if (surface == NULL) {
            continue;
        }

        if (surface->is_outdated) {
            if (surface_render_all_desktops(surface) != 0) {
                LOGGER_ERROR("Failed to render surface %u",
                        surface->id);
            }
        }
    }
}


/**
 * @brief Close a single-instance overlay dialog and repaint whichever
 *        surface is first in the surface list
 *
 * Shared by the timed auto-close of the info popup and of the
 * desktop-switch notification: both close a dialog that, unlike a
 * per-client one, is not tied to any one particular surface, so any
 * surface's own current-desktop repaint is enough to clear its
 * remnants from the screen.
 *
 * @param ctx      Main loop context
 * @param close_fn The dialog's own @c X_close function
 *
 * @note Complexity: @e O(1), since only the first surface is needed
 */
static void s_loop_refresh_close_overlay(const loop_ctx_td *ctx,
        void (*close_fn)(xcb_connection_t *))
{
    surface_td *found = NULL;

    for (list_item_td *node = list_head(ctx->surfaces); node != NULL;
            node = list_next(node)) {
        surface_td *const s = (surface_td *) list_data(node);
        if (s != NULL) {
            found = s;
            break;
        }
    }

    close_fn(xcb_connection_get());
    if (found != NULL) {
        surface_render_current_desktop_repaint(found);
    }
}


/**
 * @brief Whether any surface is still marked outdated
 *
 * Asked before the render pass, since rendering is what clears the
 * flag it reads.
 *
 * @param ctx Main loop context
 *
 * @return @c true when at least one surface needs re-rendering
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
static bool s_loop_refresh_any_outdated(const loop_ctx_td *ctx)
{
    for (list_item_td *node = list_head(ctx->surfaces); node != NULL;
            node = list_next(node)) {
        const surface_td *const s = (surface_td *) list_data(node);
        if (s != NULL && s->is_outdated) {
            return true;
        }
    }

    return false;
}


/* Close whatever expired, repaint what changed, and resync */
void loop_refresh(const loop_ctx_td *ctx)
{
    bool any_outdated;

    if (ctx == NULL || ctx->surfaces == NULL) {
        return;
    }

    /* Closed before the render pass below, so that whatever visual
     * update the close itself triggers is handled in this same
     * iteration */
    if (popup_is_open() && popup_ms_remaining() == 0) {
        s_loop_refresh_close_overlay(ctx, popup_close);
    }

    if (notify_desktop_is_open() &&
            notify_desktop_ms_remaining() == 0) {
        s_loop_refresh_close_overlay(ctx, notify_desktop_close);
    }

    any_outdated = s_loop_refresh_any_outdated(ctx);

    s_loop_refresh_outdated(ctx);
    if (any_outdated) {
        wm_ewmh_sync(ctx->wm);
    }
}


/* Mark every surface outdated and render them all */
void loop_refresh_full(const loop_ctx_td *ctx)
{
    if (ctx == NULL || ctx->surfaces == NULL) {
        return;
    }

    LOGGER_TRACE("Fully updating window manager", L_NARG);

    for (list_item_td *node = list_head(ctx->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *const surface = (surface_td *) list_data(node);
        if (surface != NULL) {
            surface->is_outdated = true;
        }
    }

    s_loop_refresh_outdated(ctx);
}
