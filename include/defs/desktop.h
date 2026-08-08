/**
 * @file defs/desktop.h
 *
 * @brief Capacity limits and identifiers for desktops, and timing for
 *        the desktop-switch notification
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
 */
#define WM_DESKTOP_INITIAL_CAPACITY (256)  /* (512) ? */

/**
 * @brief Maximum number of characters allowed in the name of the
 *        desktop, including the null terminator
 */
#define WM_DESKTOP_MAX_LENGTH_NAME (64)

/** Desktop identifier when the client is pinned to all desktops */
#define WM_DESKTOP_ID_ALL (0xFFFFFFFFu)

/**
 * @brief Sentinel meaning a desktop's own @c background-color
 *        (config.json) was never explicitly set
 *
 * Distinguishes "this desktop's entry did not set its own color" from
 * "this desktop's entry explicitly set this exact color", so a
 * desktop with no override of its own correctly falls back to
 * @c theme.desktop.color.background (theme.json) instead: any real,
 * explicitly configured 24-bit color always has its own top byte
 * zero, so this reserved value (top byte @c 0xFF) can never collide
 * with one.
 *
 * @see @c desktop_init in desktop.c, where this fallback is applied
 */
#define WM_DESKTOP_BG_COLOR_UNSET (0xFF000000u)

/** Duration in milliseconds for the desktop-switch notification */
#define WM_DESKTOP_NOTIFY_TIMEOUT_MS (400)


#endif  /* ! DEFS_DESKTOP_H */
