/**
 * @file wm.h
 *
 * @brief Definitions for all related to the window manager itself, as
 *        for screens/surfaces, desktops and windows/clients
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_WM_H
#define DEFS_WM_H


/**
 * @brief Initial capacity of windows for a desktop
 *
 * Number of windows that the desktop is initialized with.  A higher
 * initial capacity may reduce the need for resizing the underlying data
 * structure as windows are added to the open-addressed hash table.
 */
#define DESKTOP_INITIAL_CAPACITY (256)  /* (512) ? */

/**
 * @brief Maximum number of characters allowed in the name of the
 *        desktop, including the null terminator
 */
#define DESKTOP_MAX_LENGTH_NAME (64)

/**
 * @brief Desktop identifier when the client is pinned to all desktops
 */
#define DESKTOP_ID_ALL (0xFFFFFFFF)


#endif  /* ! DEFS_WM_H */
