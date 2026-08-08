/**
 * @file defs/client.h
 *
 * @brief Dimensions, timing, and other numeric limits for a managed
 *        client window and its decoration
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


/** Minimum supported client window dimension */
#define WM_MIN_WINDOW_DIMENSION (1u)

/**
 * @brief Fallback client width and height when geometry cannot be
 *        queried from the X server
 */
#define WM_CLIENT_DEFAULT_DIM (100u)

/** Default titlebar height in pixels */
#define WM_TITLEBAR_DEFAULT_HEIGHT (22u)

/** Pixels between baseline and the bottom of the titlebar */
#define WM_TITLEBAR_TEXT_BOTTOM_PAD (6)

/** Decoration button side pixels */
#define WM_DECOR_BTN_SIZE (12u)

/** Gap between buttons */
#define WM_DECOR_BTN_GAP (2u)

/**
 * @brief Maximum consecutive interactive-resize steps to wait for a
 *        client's @c _NET_WM_SYNC_REQUEST acknowledgement
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
 *       never acknowledges at all (including one whose 'AlarmNotify'
 *       never arrives due to some as-yet-undiscovered XSync protocol
 *       mismatch on the window manager's side), and a resize should
 *       still feel reasonably responsive even in that worst case rather
 *       than visibly stalling for several steps before each catch-up
 *       jump.
 */
#define WM_SYNC_MAX_WAIT_TICKS (2u)


#endif  /* ! DEFS_CLIENT_H */
