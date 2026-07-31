/**
 * @file wm/internal.h
 *
 * @brief Private singleton accessor shared across wm sub-modules
 *
 * Declares the singleton @c wm_td pointer that is defined in
 * @c wm/wm.c and shared with @c wm/ewmh.c and @c wm/action.c.
 *
 * @note This header is private to the wm subsystem and must not be
 *       included outside of @c src/wm/, for it is NOT part of the
 *       public API
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_INTERNAL_H
#define WM_INTERNAL_H


/* Project includes */
#include <wm.h>


/**
 * @brief Singleton window manager instance
 *
 * @note Defined in @c wm/wm.c
 * @note All wm sub-modules access it through this declaration
 */
extern wm_td *wm;


#endif  /* ! WM_INTERNAL_H */
