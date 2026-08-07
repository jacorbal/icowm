/**
 * @file desktop.h
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

/** Duration in milliseconds for the desktop-switch notification */
#define WM_DESKTOP_NOTIFY_TIMEOUT_MS (400)


#endif  /* ! DEFS_DESKTOP_H */
