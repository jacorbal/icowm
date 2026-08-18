/**
 * @file defs/run.h
 *
 * @brief Dimensions and capacity limits for the built-in run-box
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

#ifndef DEFS_RUN_H
#define DEFS_RUN_H


/** Maximum length of the typed command string, including the null
 *  terminator */
#define WM_RUN_COMMAND_MAX_LENGTH (256)

/** Height of the run-box's own single text-entry bar, in pixels */
#define WM_RUN_BAR_HEIGHT (26)

/** Horizontal padding inside the run-box window */
#define WM_RUN_PAD_X (14)

/** Vertical padding (top/bottom) inside the run-box window */
#define WM_RUN_PAD_Y (10)

/**
 * @brief Fixed width of the run-box, in pixels
 *
 * Narrower than @c WM_SEARCH_WIDTH (defs/search.h): a typed command is
 * ordinarily far shorter than a window title, and the two widgets
 * being visibly different sizes is one more cue, alongside the
 * "Run:" prompt itself, that they are not the same thing.
 */
#define WM_RUN_WIDTH (320)


#endif  /* ! DEFS_RUN_H */
