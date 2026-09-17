/**
 * @file loop/event.h
 *
 * @brief Event handlers the main loop implements itself
 *
 * Nearly every X event the loop receives is forwarded straight to
 * @c handler.h or to the input modules.  The five declared here are
 * the exceptions: those whose handling is bound to the loop's
 * machinery, either because they update the user-time bookkeeping
 * that focus-stealing prevention reads, or because they consume
 * further events from the connection before deciding what to do.
 *
 * All of them take the event by address, the signature the dispatch
 * table requires (see @c loop/dispatch.h), even though only the
 * motion handler ever writes through it.
 *
 * @ingroup loop
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef LOOP_EVENT_H
#define LOOP_EVENT_H


/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <loop/context.h>


/* Public interface */
/**
 * @brief Handle a @c KEY_PRESS event
 *
 * Notes the press as genuine user activity before dispatching it, so
 * that a client requesting @c _NET_ACTIVE_WINDOW later can be told
 * apart from one the user was actually typing into.
 *
 * @param ctx   Main loop context
 * @param event Raw event, a @c xcb_key_press_event_t
 *
 * @note Complexity: @e O(k + s * d * c), where @e k is the number of
 *       configured bindings, @e s the number of stages, @e d the
 *       desktops per stage, and @e c the per-desktop lookup cost
 */
void loop_event_key_press(loop_ctx_td *ctx,
        xcb_generic_event_t **event);

/**
 * @brief Handle a @c KEY_RELEASE event
 *
 * @param ctx   Main loop context
 * @param event Raw event, a @c xcb_key_release_event_t
 *
 * @note Complexity: @e O(k), where @e k is the number of configured
 *       bindings
 */
void loop_event_key_release(loop_ctx_td *ctx,
        xcb_generic_event_t **event);

/**
 * @brief Handle a @c BUTTON_PRESS event
 *
 * Notes the press as genuine user activity, exactly as
 * @a loop_event_key_press does, before dispatching it.
 *
 * @param ctx   Main loop context
 * @param event Raw event, a @c xcb_button_press_event_t
 *
 * @note Complexity: @e O(b + s * d * c), where @e b is the number of
 *       configured button bindings, @e s the number of stages,
 *       @e d the desktops per stage, and @e c the per-desktop
 *       lookup cost
 */
void loop_event_button_press(loop_ctx_td *ctx,
        xcb_generic_event_t **event);

/**
 * @brief Handle a @c BUTTON_RELEASE event
 *
 * @param ctx   Main loop context
 * @param event Raw event, a @c xcb_button_release_event_t
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients inspected while ending whatever the press started
 */
void loop_event_button_release(loop_ctx_td *ctx,
        xcb_generic_event_t **event);

/**
 * @brief Handle a @c MOTION_NOTIFY event
 *
 * Collapses a run of consecutive pending motion events into their
 * latest position before acting on it, then routes that position to
 * whichever of the drag, the open menus, the search widget, or the
 * plain hover tracking currently owns the pointer.
 *
 * @param ctx   Main loop context
 * @param event Raw event, a @c xcb_motion_notify_event_t; replaced
 *              in place by any newer motion event found while
 *              collapsing, with the superseded ones freed as they
 *              are passed over
 *
 * @note A non-motion event found while peeking ahead is left in the
 *       context for the next pass instead of being dropped
 * @note Complexity: @e O(q + n), where @e q is the number of queued
 *       motion events collapsed and @e n the number of managed
 *       clients the hover path inspects
 */
void loop_event_motion_notify(loop_ctx_td *ctx,
        xcb_generic_event_t **event);


#endif  /* ! LOOP_EVENT_H */
