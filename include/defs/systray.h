/**
 * @file defs/systray.h
 *
 * @brief Dimensions and capacity limits for the built-in systray dock
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


/** Side length in pixels of each docked icon's embed window */
#define WM_SYSTRAY_ICON_SIZE (24u)

/** Padding in pixels around and between icons */
#define WM_SYSTRAY_ICON_PAD (4u)

/**
 * @brief Upper bound on simultaneously docked icons
 *
 * A plain fixed array is enough for a systray and keeps this module
 * allocation-free.
 */
#define WM_SYSTRAY_MAX_ICONS (32u)


#endif  /* ! DEFS_SYSTRAY_H */
