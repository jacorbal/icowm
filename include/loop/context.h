/**
 * @file loop/context.h
 *
 * @brief Cached per-run state shared by the main event loop
 *
 * Gathers the window-manager values the main loop reads on every
 * iteration, resolved once at loop entry instead of through an
 * accessor call per use, plus the two pieces of state the loop itself
 * owns for as long as it runs: the key symbols table and the
 * one-event lookahead used to coalesce pointer motion.
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

#ifndef LOOP_CONTEXT_H
#define LOOP_CONTEXT_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */

/* Project includes */
#include <types/handles.h>


/** Cached state the main event loop carries across one whole run */
typedef struct loop_ctx_s {
    /** Window-manager singleton this loop runs for */
    wm_td *wm;

    /** All managed stages, i.e., @a wm_stages of @p wm */
    list_td *stages;

    /** Active configuration, i.e., @a wm_config of @p wm */
    const config_td *config;

    /**
     * @brief Key symbols table owned by the loop
     *
     * Allocated at loop entry and freed on exit; @c NULL until
     * @a loop_run has allocated it.
     */
    xcb_key_symbols_t *keysyms;

    /**
     * @brief One-event lookahead
     *
     * Holds the first non-motion event found while coalescing a run
     * of consecutive @c MotionNotify events, so that it is handled on
     * the next pass rather than dropped.  @c NULL when no event is
     * being held.
     */
    xcb_generic_event_t *pending_event;

    /** Memory cap for managed clients, @c 0 when unrestricted */
    uint32_t restricted_memory_mib;

    /**
     * @brief XRandR base event code
     *
     * Meaningless unless @p is_randr_available says so.
     */
    uint8_t randr_base_event;

    /**
     * @brief XSync base event code
     *
     * Meaningless unless @p is_sync_available says so.
     */
    uint8_t sync_base_event;

    /** Whether the XRandR extension is present */
    bool is_randr_available;

    /** Whether the XSync extension is present */
    bool is_sync_available;
} loop_ctx_td;


/* Public interface */
/**
 * @brief Resolve the window-manager values the loop caches for a run
 *
 * Fills @p ctx from @p wm, leaving @c keysyms and @c pending_event
 * empty for @a loop_run to fill in as it goes.
 *
 * @param ctx Context to fill in
 * @param wm  Window-manager singleton to read the cached values from
 *
 * @return @c true when @p ctx was filled in, @c false when either
 *         argument is @c NULL
 *
 * @note Complexity: @e O(1)
 */
bool loop_context_init(loop_ctx_td *ctx, wm_td *wm);


#endif  /* ! LOOP_CONTEXT_H */
