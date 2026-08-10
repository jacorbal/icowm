/**
 * @file defs/systray.h
 *
 * @brief Capacity limit for the built-in systray dock
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

#ifndef DEFS_SYSTRAY_H
#define DEFS_SYSTRAY_H


/**
 * @brief Upper bound on simultaneously docked icons
 *
 * A plain fixed array is enough for a systray and keeps this module
 * allocation-free.  Unlike a docked icon's own pixel size and padding
 * (@c theme.systray.pixmap.size/padding, configurable per theme since
 * this array's own capacity is fixed at compile time), this stays a
 * compile-time constant on purpose.
 */
#define WM_SYSTRAY_MAX_ICONS (32u)


#endif  /* ! DEFS_SYSTRAY_H */
