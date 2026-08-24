/**
 * @file loop/event/input.c
 *
 * @brief Key and pointer button event handlers for the main loop
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Input includes */
#include <input/kbd/event.h>
#include <input/mouse/event.h>

/* Project includes */
#include <client.h>
#include <lookup.h>

/* Local includes */
#include <loop/event.h>


/**
 * @brief Refresh the client behind a genuine input event's own
 *        @c user_time, for @c _NET_ACTIVE_WINDOW focus-stealing
 *        prevention to compare against later
 *
 * @param ctx Main loop context, for its own surface list
 * @param window Window a real @c KeyPress or @c ButtonPress named as
 *               its own @c event field, i.e., the one that actually
 *               received it
 * @param response_type The raw, unmasked @c response_type off the
 *                       event itself, top bit included
 * @param time X server timestamp of the event
 *
 * @note A no-op when the top bit of @p response_type marks the event
 *       synthetic, i.e., sent by an application itself via
 *       @c XSendEvent rather than genuinely delivered by the X
 *       server: trusting a synthetic one here would let any client
 *       fake recent activity right before requesting
 *       @c _NET_ACTIVE_WINDOW, defeating the whole point of the
 *       comparison this feeds
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e c
 *       the hash-table lookup cost per desktop, the same as
 *       @a lookup_find_client itself, which this wraps
 */
static void s_loop_event_note_real_input(const loop_ctx_td *ctx,
        xcb_window_t window, uint8_t response_type, uint32_t time)
{
    client_td *client;

    if ((response_type & 0x80u) != 0u) {
        return;
    }

    client = lookup_find_client(ctx->surfaces, window, NULL, NULL);
    client_update_user_time(client, time);
}


/* Handle a 'KEY_PRESS' event */
void loop_event_key_press(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    xcb_key_press_event_t *kp;

    if (ctx == NULL || event == NULL || *event == NULL) {
        return;
    }

    kp = (xcb_key_press_event_t *) *event;
    s_loop_event_note_real_input(ctx, kp->event,
            (*event)->response_type, kp->time);
    keyboard_handle_press(ctx->wm, ctx->keysyms, kp, ctx->surfaces,
            ctx->config);
}


/* Handle a 'KEY_RELEASE' event */
void loop_event_key_release(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    if (ctx == NULL || event == NULL || *event == NULL) {
        return;
    }

    keyboard_handle_release(ctx->keysyms,
            (xcb_key_release_event_t *) *event, ctx->surfaces,
            ctx->config);
}


/* Handle a 'BUTTON_PRESS' event */
void loop_event_button_press(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    xcb_button_press_event_t *bp;

    if (ctx == NULL || event == NULL || *event == NULL) {
        return;
    }

    bp = (xcb_button_press_event_t *) *event;
    s_loop_event_note_real_input(ctx, bp->event,
            (*event)->response_type, bp->time);
    mouse_handle_press(ctx->wm, ctx->connection, ctx->surfaces, bp,
            ctx->config);
}


/* Handle a 'BUTTON_RELEASE' event */
void loop_event_button_release(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    if (ctx == NULL || event == NULL || *event == NULL) {
        return;
    }

    mouse_handle_release(ctx->connection, ctx->surfaces,
            (xcb_button_release_event_t *) *event, ctx->config);
}
