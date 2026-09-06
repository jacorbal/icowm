/**
 * @file defs/desktop.h
 *
 * @brief Capacity limits and identifiers for desktops, and timing for
 *        the desktop-switch notification
 *
 * @ingroup defs
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_DESKTOP_H
#define DEFS_DESKTOP_H


/**
 * @brief Initial capacity of windows for a desktop
 *
 * Number of windows that the desktop is initialized with.  A higher
 * initial capacity may reduce the need for resizing the underlying data
 * structure as windows are added to the open-addressed hash table.
 *
 * Only the fallback used when restricted-memory mode is not active at
 * all (see @a memguard_max_clients, which @a desktop_init prefers over
 * this whenever @c -M is given).  Smaller under @c COMPACT (for
 * a comment on this mode, see @c defs/compact.h) regardless, so
 * a compact build run without @c -M still starts with a more modest
 * initial allocation than an ordinary build would.
 */
#ifdef COMPACT
#define WM_DESKTOP_INITIAL_CAPACITY (48)
#else
#define WM_DESKTOP_INITIAL_CAPACITY (64)
#endif

/**
 * @brief Maximum number of characters allowed in the name of the
 *        desktop, including the null terminator
 */
#define WM_DESKTOP_MAX_LENGTH_NAME (64)

/**
 * @brief Desktop identifier when the client is pinned to all desktops
 */
#define WM_DESKTOP_ID_ALL (0xFFFFFFFFu)

/**
 * @brief Sentinel meaning a desktop's @c background-color
 *        (@c config.json) was never explicitly set
 *
 * Distinguishes "this desktop's entry did not set its color" from
 * "this desktop's entry explicitly set this exact color", so a desktop
 * with no override of its own correctly falls back to
 * @p theme.desktop.color.background (in @c theme.json) instead.  Any
 * real, explicitly configured 24-bit color always has its top byte
 * zero, so this reserved value (top byte @c 0xFF) can never collide
 * with one.
 *
 * @see @a desktop_init in @c desktop.c, where this fallback is applied
 */
#define WM_DESKTOP_BG_COLOR_UNSET (0xFF000000u)

/**
 * @brief Duration in milliseconds for the desktop-switch notification
 */
#define WM_DESKTOP_NOTIFY_TIMEOUT_MS (400)

/**
 * @brief Milliseconds a window or icon drag has to hold the pointer
 *        against a screen edge before switching to the adjacent desktop
 *
 * Milliseconds before @p desktops.warp_on_edge_drag (@c config.json)
 * switches to the adjacent desktop with the drag still held.  Long
 * enough that merely passing through the edge on the way to a normal
 * drop elsewhere does not trigger it.
 *
 * @see @c config_desktop_s
 */
#define WM_DESKTOP_WARP_DELAY_MS (500)

/**
 * @brief Milliseconds the pointer has to rest against a screen edge,
 *        with no drag in progress, before that edge's first
 *        viewport pan
 *
 * Milliseconds before @p viewport.pan_on_edge_hover (@c config.json)
 * pans the current desktop's viewport toward the held edge for the
 * first time.  Long enough that merely passing through the edge on
 * the way elsewhere does not trigger it.
 *
 * @see @c config_desktop_s
 */
#define WM_VIEWPORT_PAN_DELAY_MS (500)

/**
 * @brief Milliseconds between repeated viewport pans while the pointer
 *        stays held against the same screen edge
 *
 * Shorter than @c WM_VIEWPORT_PAN_DELAY_MS above, since once the first
 * pan already confirmed the pointer is deliberately resting there
 * rather than just passing through, further pans need not wait as
 * long, the same way a key held down auto-repeats faster than its
 * initial delay.
 *
 * @see @c config_desktop_s
 */
#define WM_VIEWPORT_PAN_REPEAT_MS (200)


#endif  /* ! DEFS_DESKTOP_H */
