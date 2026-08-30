/**
 * @file client/icccm.h
 *
 * @brief ICCCM-sourced hints a client carries
 *
 * What @a client_init read off a window's ICCCM properties and the
 * window manager then has to remember: the size hints that constrain
 * how it may be resized, the protocols it takes part in, and the
 * window group it belongs to.
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

#ifndef CLIENT_ICCCM_H
#define CLIENT_ICCCM_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/pair.h>


/**
 * @brief ICCCM-sourced hints: @c WM_NORMAL_HINTS, @c WM_PROTOCOLS,
 *        @c WM_HINTS
 */
struct client_hints_icccm_s {
    /**
     * @brief ICCCM @c WM_NORMAL_HINTS size constraints
     */
    struct {
        bool is_valid;       /**< True when hints were read from
                                  server */
        bool has_position;   /**< True when the client itself
                                  requested a position
                                  (@c USPosition or @c PPosition)
                                  rather than leaving it to this
                                  window manager's own policy */
        struct position_s req_pos;  /**< Client-requested position,
                                         valid only when
                                         @p has_position is true */
        /** Minimum size, (0, 0) meaning unset */
        struct dimensions_s min;
        /** Maximum size, (0, 0) meaning unset */
        struct dimensions_s max;
        struct dimensions_s base;       /**< Base size for increment
                                             arithmetic */
        struct dimensions_s inc;        /**< Size increment
                                             (0 or 1 = no grid) */
        struct aspect_range_s aspect;   /**< Minimum/maximum w/h
                                             ratio (0,0 = unset) */
    } size;

    /**
     * @brief ICCCM @c WM_PROTOCOLS state
     */
    struct {
        xcb_atom_t delete_atom;     /**< Cached @c WM_DELETE_WINDOW
                                         atom */
        xcb_atom_t take_focus_atom; /**< Cached @c WM_TAKE_FOCUS
                                         atom */
        /** Supports @c WM_DELETE_WINDOW */
        bool has_delete;
        /** Supports @c WM_TAKE_FOCUS */
        bool has_take_focus;
    } protocols;

    /**
     * @brief ICCCM @c WM_HINTS fields
     */
    struct {
        /**
          * @brief Value of the @c input field of @c WM_HINTS
          *
          * True when the client asks the window manager to set
          * the input focus to its own toplevel for it, which
          * ICCCM §4.1.7 calls the Passive and Locally Active
          * models; false when it would rather do that itself on
          * receiving @c WM_TAKE_FOCUS, the No Input and Globally
          * Active models.  Defaults to true, as ICCCM says an
          * absent field does.
          *
          * Named for what it holds and not for whether the hint
          * was present: reading it as presence and testing the
          * flag instead would invert the very thing ICCCM asks
          * about here.
          */
        bool accepts_input;
        bool is_initial_iconic;   /**< Map iconic for @c WM_HINTS
                                       initial state */
        xcb_window_t group_leader; /**< Window group leader, or
                                        @c XCB_NONE */
        xcb_window_t client_leader; /**< ICCCM @c WM_CLIENT_LEADER
                                         window, or @c XCB_NONE if
                                         unset.  Used together with
                                         @p group_leader (see
                                         @a client_group_leader) to
                                         cluster windows belonging
                                         to the same application
                                         for placement */
    } hints;
};


#endif  /* ! CLIENT_ICCCM_H */
