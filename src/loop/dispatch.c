/**
 * @file loop/dispatch.c
 *
 * @brief Event dispatch for the main loop
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
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/sync.h>

/* Utils includes */
#include <utils/xcb/connection.h>

/* Input includes */
#include <input/mouse/event.h>

/* Project includes */
#include <handler.h>
#include <logger.h>

/* Local includes */
#include <loop/dispatch.h>
#include <loop/event.h>


/**
 * @brief Signature every entry of the dispatch table has
 */
typedef void (*s_loop_dispatch_fn)(loop_ctx_td *ctx,
        xcb_generic_event_t **event);


/**
 * @brief Handle an event that is deliberately ignored
 *
 * Reparenting is the window manager's doing, and its spurious
 * @c UnmapNotify is already absorbed through @c ignore_unmap; a created
 * window, in turn, is adopted on @c MAP_REQUEST rather than on
 * creation, since one may well never be mapped at all.  Both are routed
 * here rather than left out of the table, so that neither is reported
 * as unhandled.
 *
 * @param ctx   Main loop context, unused
 * @param event Event to ignore
 *
 * @note Complexity: @e O(1)
 */
static void s_loop_dispatch_ignore(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    (void) ctx;
    (void) event;
}


/**
 * @brief Adapt @a handler_protocol_error to the table signature
 *
 * @param ctx   Main loop context, unused
 * @param event Event received with response type @c 0
 *
 * @note Complexity: @e O(1)
 */
static void s_loop_dispatch_protocol_error(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    (void) ctx;

    handler_protocol_error(*event);
}


/**
 * @brief Adapt @a mouse_handle_enter to the table signature
 *
 * @param ctx   Main loop context
 * @param event Enter-notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_enter_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    mouse_handle_enter(xcb_connection_get(), ctx->surfaces,
            (xcb_enter_notify_event_t *) *event, ctx->config);
}


/**
 * @brief Adapt @a handler_leave_notify to the table signature
 *
 * @param ctx   Main loop context
 * @param event Leave-notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected
 */
static void s_loop_dispatch_leave_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_leave_notify(ctx->wm, (xcb_leave_notify_event_t *) *event);
}


/**
 * @brief Adapt @a handler_focus_in to the table signature
 *
 * @param ctx   Main loop context
 * @param event Focus-in event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_focus_in(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_focus_in(xcb_connection_get(), ctx->surfaces,
            (xcb_focus_in_event_t *) *event);
}


/**
 * @brief Adapt @a handler_focus_out to the table signature
 *
 * @param ctx   Main loop context
 * @param event Focus-out event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected
 */
static void s_loop_dispatch_focus_out(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_focus_out(ctx->wm, (xcb_focus_out_event_t *) *event);
}


/**
 * @brief Adapt @a handler_configure_notify to the table signature
 *
 * @param ctx   Main loop context
 * @param event Configure-notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_configure_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_configure_notify(xcb_connection_get(), ctx->surfaces,
            (xcb_configure_notify_event_t *) *event);
}


/**
 * @brief Adapt @a handler_configure_request to the table signature
 *
 * @param ctx   Main loop context
 * @param event Configure-request event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_configure_request(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_configure_request(xcb_connection_get(), ctx->surfaces,
            (xcb_configure_request_event_t *) *event);
}


/**
 * @brief Adapt @a handler_map_request to the table signature
 *
 * @param ctx   Main loop context
 * @param event Map-request event
 *
 * @note Complexity: @e O(1)
 */
static void s_loop_dispatch_map_request(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_map_request(ctx->wm, (xcb_map_request_event_t *) *event);
}


/**
 * @brief Adapt @a handler_map_notify to the table signature
 *
 * @param ctx   Main loop context
 * @param event Map-notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_map_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_map_notify(xcb_connection_get(), ctx->surfaces,
            (xcb_map_notify_event_t *) *event);
}


/**
 * @brief Adapt @a handler_unmap_notify to the table signature
 *
 * @param ctx   Main loop context
 * @param event Unmap-notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_unmap_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_unmap_notify(xcb_connection_get(), ctx->surfaces,
            (xcb_unmap_notify_event_t *) *event);
}


/**
 * @brief Adapt @a handler_destroy_notify to the table signature
 *
 * @param ctx   Main loop context
 * @param event Destroy-notify event
 *
 * @note Complexity: @e O(n + t), where @e n is the number of managed
 *       surfaces and @e t the number of docked systray icons
 */
static void s_loop_dispatch_destroy_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_destroy_notify(ctx->wm, xcb_connection_get(), ctx->surfaces,
            (xcb_destroy_notify_event_t *) *event);
}


/**
 * @brief Adapt @a handler_property_notify to the table signature
 *
 * @param ctx   Main loop context
 * @param event Property-notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_property_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_property_notify(ctx->wm, xcb_connection_get(), ctx->surfaces,
            (xcb_property_notify_event_t *) *event);
}


/**
 * @brief Adapt @a handler_colormap_notify to the table signature
 *
 * @param ctx   Main loop context
 * @param event Colormap-notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_colormap_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_colormap_notify(xcb_connection_get(), ctx->surfaces,
            (xcb_colormap_notify_event_t *) *event);
}


/**
 * @brief Adapt @a handler_expose to the table signature
 *
 * @param ctx   Main loop context
 * @param event Expose event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_expose(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_expose(xcb_connection_get(), ctx->surfaces,
            (xcb_expose_event_t *) *event, ctx->config);
}


/**
 * @brief Adapt @a handler_client_message to the table signature
 *
 * @param ctx   Main loop context
 * @param event Client-message event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_client_message(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_client_message(ctx->wm,
            (xcb_client_message_event_t *) *event);
}


/**
 * @brief Adapt @a handler_selection_clear to the table signature
 *
 * @param ctx   Main loop context
 * @param event Selection-clear event
 *
 * @note Complexity: @e O(1), aside from @a wm_shutdown_begin's own
 *       cost when it is reached
 */
static void s_loop_dispatch_selection_clear(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_selection_clear(ctx->wm,
            (xcb_selection_clear_event_t *) *event);
}


/**
 * @brief Adapt @a handler_mapping_notify to the table signature
 *
 * @param ctx   Main loop context
 * @param event Mapping-notify event
 *
 * @note Complexity: @e O(k * s), where @e k is the number of bindings
 *       and @e s the number of managed surfaces
 */
static void s_loop_dispatch_mapping_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_mapping_notify(ctx->keysyms, ctx->surfaces,
            (xcb_mapping_notify_event_t *) *event, ctx->config);
}


/**
 * @brief Adapt @a handler_gravity_notify to the table signature
 *
 * @param ctx   Main loop context
 * @param event Gravity-notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_gravity_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_gravity_notify(xcb_connection_get(), ctx->surfaces,
            (xcb_gravity_notify_event_t *) *event);
}


/**
 * @brief Adapt @a handler_circulate_notify to the table signature
 *
 * @param ctx   Main loop context
 * @param event Circulate-notify event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_circulate_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_circulate_notify(xcb_connection_get(), ctx->surfaces,
            (xcb_circulate_notify_event_t *) *event);
}


/**
 * @brief Adapt @a handler_circulate_request to the table signature
 *
 * @param ctx   Main loop context
 * @param event Circulate-request event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
static void s_loop_dispatch_circulate_request(loop_ctx_td *ctx,
        xcb_generic_event_t **event)
{
    handler_circulate_request(xcb_connection_get(), ctx->surfaces,
            (xcb_circulate_request_event_t *) *event);
}


/**
 * @brief Route an extension event to its handler, if it is one
 *
 * Neither XRandR nor XSync has a response type known at compile time:
 * the server assigns each extension a base at connection time and its
 * events sit at fixed offsets from there, so these cannot be table
 * entries and have to be compared for.
 *
 * @param ctx   Main loop context
 * @param event Event to test
 *
 * @return @c true when the event belonged to an extension and has
 *         been handled
 *
 * @note Complexity: @e O(1)
 */
static bool s_loop_dispatch_extension(loop_ctx_td *ctx,
        xcb_generic_event_t *event)
{
    const uint8_t type = (uint8_t) (event->response_type & ~0x80u);

    if (ctx->is_randr_available &&
            (type == (uint8_t) (ctx->randr_base_event +
                    XCB_RANDR_SCREEN_CHANGE_NOTIFY) ||
             type == (uint8_t) (ctx->randr_base_event +
                    XCB_RANDR_NOTIFY))) {
        handler_randr_event(ctx->wm, event);
        return true;
    }

    if (ctx->is_sync_available &&
            type == (uint8_t) (ctx->sync_base_event +
                    XCB_SYNC_ALARM_NOTIFY)) {
        handler_sync_event(ctx->wm, event);
        return true;
    }

    return false;
}


/**
 * @brief Every handler, indexed by the event type that reaches it
 *
 * A type nobody handles is left @c NULL, which the dispatcher reports
 * rather than acts on.  Response type @c 0 is not an event type at all
 * but a protocol error, which is why it has an entry of its own.
 */
static const s_loop_dispatch_fn
        s_loop_dispatch_table[LOOP_DISPATCH_TABLE_SIZE] = {
    [0] = s_loop_dispatch_protocol_error,
    [XCB_KEY_PRESS] = loop_event_key_press,
    [XCB_KEY_RELEASE] = loop_event_key_release,
    [XCB_BUTTON_PRESS] = loop_event_button_press,
    [XCB_BUTTON_RELEASE] = loop_event_button_release,
    [XCB_MOTION_NOTIFY] = loop_event_motion_notify,
    [XCB_ENTER_NOTIFY] = s_loop_dispatch_enter_notify,
    [XCB_LEAVE_NOTIFY] = s_loop_dispatch_leave_notify,
    [XCB_FOCUS_IN] = s_loop_dispatch_focus_in,
    [XCB_FOCUS_OUT] = s_loop_dispatch_focus_out,
    [XCB_EXPOSE] = s_loop_dispatch_expose,
    [XCB_CREATE_NOTIFY] = s_loop_dispatch_ignore,
    [XCB_DESTROY_NOTIFY] = s_loop_dispatch_destroy_notify,
    [XCB_UNMAP_NOTIFY] = s_loop_dispatch_unmap_notify,
    [XCB_MAP_NOTIFY] = s_loop_dispatch_map_notify,
    [XCB_MAP_REQUEST] = s_loop_dispatch_map_request,
    [XCB_REPARENT_NOTIFY] = s_loop_dispatch_ignore,
    [XCB_CONFIGURE_NOTIFY] = s_loop_dispatch_configure_notify,
    [XCB_CONFIGURE_REQUEST] = s_loop_dispatch_configure_request,
    [XCB_GRAVITY_NOTIFY] = s_loop_dispatch_gravity_notify,
    [XCB_CIRCULATE_NOTIFY] = s_loop_dispatch_circulate_notify,
    [XCB_CIRCULATE_REQUEST] = s_loop_dispatch_circulate_request,
    [XCB_PROPERTY_NOTIFY] = s_loop_dispatch_property_notify,
    [XCB_COLORMAP_NOTIFY] = s_loop_dispatch_colormap_notify,
    [XCB_CLIENT_MESSAGE] = s_loop_dispatch_client_message,
    [XCB_SELECTION_CLEAR] = s_loop_dispatch_selection_clear,
    [XCB_MAPPING_NOTIFY] = s_loop_dispatch_mapping_notify
};



/* Hand one X event to its handler */
void loop_dispatch_event(loop_ctx_td *ctx, xcb_generic_event_t **event)
{
    uint8_t type;
    s_loop_dispatch_fn handler;

    if (ctx == NULL || event == NULL || *event == NULL) {
        return;
    }

    if (s_loop_dispatch_extension(ctx, *event)) {
        return;
    }

    type = (uint8_t) ((*event)->response_type & ~0x80u);
    handler = s_loop_dispatch_table[type];
    if (handler == NULL) {
        LOGGER_TRACE("Unhandled X event type: %d", (int) type);
        return;
    }

    handler(ctx, event);
}
