/**
 * @file defs/cycle.h
 *
 * @brief Dimensions and capacity limits for the Alt+Tab-style client
 *        cycle menu
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

#ifndef DEFS_CYCLE_H
#define DEFS_CYCLE_H


/** Maximum number of entries in the cycle menu */
#define WM_CYCLE_MENU_MAX_ENTRIES (128)

/** Maximum label length for a cycle menu entry */
#define WM_CYCLE_MENU_ENTRY_LENGTH (128)

/**
 * @brief Maximum pixel width a single cycle-menu label may claim
 *        towards the menu's own width
 *
 * Caps how far one very long window title can stretch the whole menu;
 * a label wider than this truncates instead (see @a s_cycle_draw_row in
 * @c menu/cycledraw.c), the same reasoning @c WM_SEARCH_NAME_MAX_WIDTH
 * truncates a search result's own name (@c defs/search.h).
 */
#define WM_CYCLE_MENU_LABEL_MAX_WIDTH (280)

/** Height of each row in the cycle menu, in pixels */
#define WM_CYCLE_MENU_ROW_HEIGHT (20)

/** Horizontal padding inside the cycle menu window */
#define WM_CYCLE_MENU_PAD_X (14)

/** Vertical padding (top/bottom) inside the cycle menu window */
#define WM_CYCLE_MENU_PAD_Y (14)

/**
 * @brief Maximum height of the cycle menu as a percentage of screen
 *        height
 *
 * When the full entry list would exceed this fraction of the screen,
 * the menu window is capped at this height and a scroll viewport is
 * used so the user can reach every entry with the cycle keys.
 */
#define WM_CYCLE_MENU_MAX_HEIGHT_PERCENT (80)

/**
 * @brief Text shown at the top of the cycle-menu viewport when there
 *        are more entries above the ones currently visible
 */
#define WM_CYCLE_MENU_SCROLL_UP_INDICATOR "---"

/**
 * @brief Text shown at the bottom of the cycle-menu viewport when there
 *        are more entries below the ones currently visible
 */
#define WM_CYCLE_MENU_SCROLL_DOWN_INDICATOR "---"


#endif  /* ! DEFS_CYCLE_H */
