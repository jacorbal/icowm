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

/** Height of the run-box's single text-entry bar, in pixels */
#define WM_RUN_BAR_HEIGHT (26)

/** Horizontal padding inside the run-box window */
#define WM_RUN_PAD_X (14)

/** Vertical padding (top/bottom) inside the run-box window */
#define WM_RUN_PAD_Y (10)

/**
 * @brief Fixed width of the run-box, in pixels
 *
 * Narrower than @c WM_SEARCH_WIDTH (defs/search.h).  A typed command is
 * ordinarily far shorter than a window title, and the two widgets
 * being visibly different sizes is one more cue, alongside the
 * "Run:" prompt itself, that they are not the same thing.
 */
#define WM_RUN_WIDTH (320)

/**
 * @brief Shown at the left edge when the text scrolled out of view
 *        that way
 *
 * Without it there is no telling a command that begins where it looks
 * to from one whose beginning has scrolled past the edge, which is
 * the difference between running what was typed and running the tail
 * of it.
 */
#define WM_RUN_MARK_LEFT "<"

/** Shown at the right edge when text continues past it, for the same
 *  reason @c WM_RUN_MARK_LEFT is shown at the other one */
#define WM_RUN_MARK_RIGHT ">"

/** Width of the insertion cursor, in pixels; a thin bar between two
 *  characters rather than a block over one, which is what a text
 *  field draws and what leaves the character under it legible */
#define WM_RUN_CURSOR_WIDTH (2)


#endif  /* ! DEFS_RUN_H */
