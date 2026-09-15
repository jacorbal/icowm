/**
 * @file client/ewmh.h
 *
 * @brief EWMH-sourced state a client carries
 *
 * What @a client_init read off a window's EWMH properties and the
 * window manager then has to remember: the states it asked for before
 * it was ever mapped, and the two protocols it takes part in.
 *
 * One of four files named for this protocol, each with its
 * remit: @c defs/ewmh.h holds the constants, @c wm/ewmh.h what this
 * window manager publishes on the root window, @c cmds/client/ewmh.h
 * the functions that write a client's state back out, and this one
 * what a client stores.
 *
 * @ingroup client
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CLIENT_EWMH_H
#define CLIENT_EWMH_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>


/**
 * @brief EWMH-sourced hints: pre-existing @c _NET_WM_STATE,
 *        @c _NET_WM_PING, @c _NET_WM_SYNC_REQUEST
 */
struct client_hints_ewmh_s {
    /**
     * @brief Whether the client's pre-existing @c _NET_WM_STATE
     *        (read before this window was ever mapped) already
     *        included the matching state bit
     *
     * EWMH's correct way for a client to request one of these
     * states from the outset, distinct from
     * @p hints_icccm.hints.is_initial_iconic (ICCCM @c WM_HINTS,
     * not EWMH) though serving the exact same role:
     * @a handler_window_map_request consults this once the newly mapped
     * client's frame/decoration already exist, the same way it
     * already consults @p hints_icccm.hints.is_initial_iconic for
     * @c IconicState.  A client requesting both maximized axes at
     * once is maximized on both, rather than one call each.
     *
     * @see @a s_client_read_pre_existing_state (client.c)
     */
    struct {
        bool is_fullscreen;
        bool is_maximized_horz;
        bool is_maximized_vert;
    } initial_state;

    /**
     * @brief EWMH @c _NET_WM_PING state
     */
    struct {
        bool is_supported;        /**< Supports @c _NET_WM_PING
                                       protocol */
        uint32_t last_sent;       /**< X timestamp of last ping
                                       sent */
        uint32_t last_reply;      /**< X timestamp of last ping
                                       reply */
        bool is_waiting;      /**< @c true between sending a ping
                                   and receiving its reply (or
                                   giving up after @p pending_ticks) */
        uint8_t pending_ticks; /**< Consecutive probe rounds spent
                                    waiting for the current ping;
                                    past
                                    @c WM_EWMH_PING_TIMEOUT_SECONDS
                                    worth of them the client is
                                    marked unresponsive */
    } ping;

    /**
     * @brief EWMH @c _NET_WM_SYNC_REQUEST state
     *
     * @p counter and @p alarm hold plain XCB XIDs (an
     * @c xcb_sync_counter_t / @c xcb_sync_alarm_t are both a
     * @c uint32_t under the hood) rather than the XSync-typed
     * values, so this header does not need to pull in
     * @c xcb/sync.h.  Call sites that actually issue XSync
     * requests cast as needed.
     *
     * @see @a ccmd_client_resize (throttling) and
     *      @a handler_sync_event (acknowledgement) for how these
     *      fields are driven
     */
    struct {
        /**
         * @brief XSync counter XID the client created and
         *        advertised through its
         *        @c _NET_WM_SYNC_REQUEST_COUNTER property
         *
         * Read by @a client_init, never created by it.  Left at
         * 0 when unset.
         */
        uint32_t counter;
        uint32_t alarm;       /**< WM-owned alarm XID watching
                                   @p counter for positive
                                   transitions, or 0 */
        uint32_t value;       /**< Local shadow of the last
                                   counter value sent to the
                                   client (low 32 bits; a single
                                   resize session never comes
                                   close to wrapping) */
        struct geometry_s pending_geom; /**< Geometry to apply once
                                             the pending request is
                                             acknowledged or times
                                             out */
        /** Supports @c _NET_WM_SYNC_REQUEST */
        bool is_supported;
        bool is_waiting;      /**< @c true between sending a sync
                                   request and receiving the
                                   matching @c AlarmNotify (or
                                   giving up after @p wait_ticks) */
        uint8_t wait_ticks;   /**< Consecutive resize attempts
                                   spent waiting for the current
                                   request; past
                                   @c WM_SYNC_MAX_WAIT_TICKS the
                                   pending geometry is
                                   force-applied so an
                                   unresponsive client can never
                                   freeze interactive resize */
        bool has_pending;     /**< @c true when a newer geometry
                                   arrived while @p is_waiting
                                   and still needs to be applied */
    } sync;
};


#endif  /* ! CLIENT_EWMH_H */
