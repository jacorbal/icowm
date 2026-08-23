/**
 * @file defs/client.h
 *
 * @brief Dimensions, timing, and other numeric limits for a managed
 *        client window and its decoration
 *
 * @defgroup defs Protocol and theme constants
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_CLIENT_H
#define DEFS_CLIENT_H


/**
 * @brief Absolute minimum a client's own content area (its window, not
 *        counting decoration) can ever be resized down to, in pixels,
 *        when it has no own resize-increment hint of its own
 *        (@c WM_NORMAL_HINTS's @c width_inc / @c height_inc, ICCCM
 *        §4.1.2.3) to measure itself in instead
 *
 * The one true floor every resize path is guaranteed to respect,
 * interactive (mouse or keyboard) or not, regardless of whether that
 * particular path also happens to know about a client's own size hints:
 * @a geom_dim_clamp (@c utils/geom.h), the lowest-level generic clamp
 * several of them share, floors to exactly this value on its own, with
 * no client or hints in scope to reason about a floor any more specific
 * than "always safe to show and still grab".
 *
 * @see @c WM_MIN_WINDOW_DIMENSION_UNITS below for windows with
 *      resize-increments hints
 */
#define WM_MIN_WINDOW_DIMENSION (4u)

/**
 * @brief Absolute minimum a client's own content area can be resized
 *        down to, in resize-increment units, for a client that provides
 *        one (@c WM_NORMAL_HINTS's @c width_inc/height_inc, ICCCM
 *        §4.1.2.3).  A terminal counting in character columns and rows,
 *        say, rather than raw pixels.
 *
 * Applied only by @a client_size_constrain (@c client/geom.c), the one
 * path that actually resolves a client's own hints, as the floor such
 * a client's own @c min_w / @c min_h defaults to when it does not
 * specify one itself; a real @c min_w / @c min_h the client does
 * specify always still wins if larger.  Left unscaled by the client's
 * own @c width_inc / @c height_inc on purpose (1 unit's own true pixel
 * size, whatever that happens to be, already exceeds
 * @c WM_MIN_WINDOW_DIMENSION above for every increment size any real
 * client is ever likely to use, so that floor is never actually the
 * binding one for a client of this kind in practice).
 */
#define WM_MIN_WINDOW_DIMENSION_UNITS (1u)

/**
 * @brief Fallback client width and height when geometry cannot be
 *        queried from the X server
 */
#define WM_CLIENT_DEFAULT_DIM (100u)

/**
 * @brief Default titlebar height in pixels
 */
#define WM_TITLEBAR_DEFAULT_HEIGHT (22u)

/**
 * @brief Decoration button side pixels
 */
#define WM_DECOR_BTN_SIZE (12u)

/**
 * @brief Gap between buttons
 */
#define WM_DECOR_BTN_GAP (2u)

/**
 * @brief Maximum consecutive interactive-resize steps to wait for
 *        a client's @c _NET_WM_SYNC_REQUEST acknowledgement
 *
 * Each unit is one resize attempt while the pointer is being dragged
 * (i.e., roughly one @c MotionNotify), not a fixed time interval, so
 * the effective wait scales with how fast the user is actually moving
 * the mouse instead of a wall-clock timeout the window manager would
 * have to track separately.  Past this many attempts without an
 * @c AlarmNotify, the pending geometry is force-applied so a slow or
 * unresponsive client can never freeze interactive resize.
 *
 * @note Kept small on purpose: this is the fallback for a client that
 *       never acknowledges at all (including one whose @c AlarmNotify
 *       never arrives due to some as-yet-undiscovered XSync protocol
 *       mismatch on the window manager's side), and a resize should
 *       still feel reasonably responsive even in that worst case rather
 *       than visibly stalling for several steps before each catch-up
 *       jump.
 */
#define WM_SYNC_MAX_WAIT_TICKS (2u)

/**
 * @brief Maximum number of hops walked up a 'WM_TRANSIENT_FOR' chain
 *        before giving up and treating the current client as the top
 *
 * Purely a cycle guard: a well-behaved client tree is never anywhere
 * near this deep, but a misbehaving or malicious client could set
 * 'WM_TRANSIENT_FOR' to point back at one of its own descendants,
 * which would otherwise turn the walk in
 * @a ccmd_client_transient_top_parent into an infinite loop.
 */
#define WM_TRANSIENT_CHAIN_MAX_DEPTH (64u)

/**
 * @brief Grace period in milliseconds after a shade or unshade during
 *        which a client's own geometry @c ConfigureRequest is ignored
 *
 * A shade/unshade transition briefly (and drastically) resizes the
 * client's own content window, which some clients react to with
 * a delayed @c ConfigureRequest of their own once they catch up
 * processing the resulting @c ConfigureNotify sequence; if that request
 * lands after the window manager has already restored the client's true
 * geometry, honoring it silently undoes the shade or unshade the user
 * just asked for.
 *
 * @see @a handler_configure_request
 */
#define WM_SHADE_CONFIGURE_COOLDOWN_MS (250)

/**
 * @brief Grace period in milliseconds after entering or leaving
 *        fullscreen during which a client's own geometry and border
 *        width @c ConfigureRequest is ignored
 *
 * The same reasoning as @c WM_SHADE_CONFIGURE_COOLDOWN_MS, for the
 * same underlying reason: a fullscreen transition also briefly (and
 * drastically) resizes the client's own content window, and its own
 * delayed @c ConfigureRequest reacting to that, once it catches up
 * processing the resulting @c ConfigureNotify sequence, is far more
 * likely to be a stale echo of whatever geometry or border width it
 * had a moment before than an independent request it actually wants
 * honored now.  Left as its own separate constant and its own
 * separate client field, rather than reusing the shade one outright,
 * since the two transitions are conceptually distinct even though
 * the mechanism guarding against a stale echo of either is the same
 * shape; a future maintainer tuning one is not thereby forced to
 * also retune the other.
 *
 * @see @a handler_configure_request
 */
#define WM_FULLSCREEN_CONFIGURE_COOLDOWN_MS (250)

/**
 * @brief How long, in milliseconds, @c client_init waits for the X
 *        server to have an answer ready before giving up on any one
 *        of its own blocking XCB reply calls
 *
 * A plain blocking XCB reply call waits for as long as it takes the
 * server to answer, with no way to give up if it never does; the
 * server, or the specific window or client a given call concerns,
 * going away mid-request leaves that call blocked forever, and this
 * window manager's whole event loop along with it.  An ordinary
 * round trip on a healthy local connection finishes in a small
 * fraction of a millisecond, so this leaves enormous margin above
 * that; it exists purely to bound how long a genuinely unresponsive
 * server can freeze this window manager for, not to react to
 * everyday jitter.
 *
 * @see @a xcb_wait_readable, in @c utils/xcb/wait.h
 */
#define WM_CLIENT_INIT_REPLY_TIMEOUT_MS (3000)


#endif  /* ! DEFS_CLIENT_H */
