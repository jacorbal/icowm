/**
 * @file loop/dispatch.h
 *
 * @brief Event dispatch for the main loop
 *
 * Routes one X event to whoever handles it, through a table indexed by
 * the event's response type rather than a chain of comparisons, so the
 * cost of dispatching does not depend on how far down a list the event
 * happens to sit.
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

#ifndef LOOP_DISPATCH_H
#define LOOP_DISPATCH_H


/* XCB includes */
#include <xcb/xcb.h>

/* Local includes */
#include <loop/context.h>


/**
 * @brief How many entries the dispatch table holds
 *
 * An X event's type occupies the low seven bits of its response type,
 * the eighth being the synthetic marker, so this covers every value one
 * can carry.
 */
#define LOOP_DISPATCH_TABLE_SIZE (128)


/* Public interface */
/**
 * @brief Hand one X event to its handler
 *
 * Extension events are recognized first, since their response types
 * are only known at run time, and everything else is looked up by
 * index.  An event nobody handles is logged and discarded.
 *
 * @param ctx   Main loop context
 * @param event Event to dispatch, taken by address because the
 *              motion handler replaces it with the newest queued
 *              motion event and frees the ones it supersedes; the
 *              caller must free whatever is left here afterward,
 *              not the pointer it passed in
 *
 * @note Complexity: @e O(1), plus whatever the chosen handler costs
 */
void loop_dispatch_event(loop_ctx_td *ctx, xcb_generic_event_t **event);


#endif  /* ! LOOP_DISPATCH_H */
