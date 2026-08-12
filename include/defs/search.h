/**
 * @file defs/search.h
 *
 * @brief Dimensions and capacity limits for the fuzzy window-search
 *        widget
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

#ifndef DEFS_SEARCH_H
#define DEFS_SEARCH_H


/** Maximum number of matching entries the search widget can list */
#define WM_SEARCH_MAX_ENTRIES (128)

/** Maximum length of the typed query string, including the null
 *  terminator */
#define WM_SEARCH_QUERY_MAX_LENGTH (128)

/** Maximum length of one result row's rendered label (name, desktop
 *  name, and bracketed hints combined) */
#define WM_SEARCH_ENTRY_LENGTH (192)

/** Height of the text-entry bar at the top of the widget, in pixels */
#define WM_SEARCH_BAR_HEIGHT (26)

/** Height of each result row, in pixels */
#define WM_SEARCH_ROW_HEIGHT (20)

/** Horizontal padding inside the search widget window */
#define WM_SEARCH_PAD_X (14)

/** Vertical padding (top/bottom) inside the search widget window */
#define WM_SEARCH_PAD_Y (10)

/** Fixed width of the search widget, in pixels; wide enough for the
 *  tabular name/desktop/hints layout without measuring every result
 *  up front */
#define WM_SEARCH_WIDTH (480)

/**
 * @brief Maximum height of the results viewport as a percentage of
 *        screen height
 *
 * When the full match list would exceed this fraction of the screen,
 * the window is capped at this height and only the entries around the
 * current selection are shown, the same reasoning as
 * @c WM_CYCLE_MENU_MAX_HEIGHT_PERCENT in @c defs/cycle.h.
 */
#define WM_SEARCH_MAX_HEIGHT_PERCENT (70)

/** Column gap, in pixels, between a row's name and its desktop name */
#define WM_SEARCH_COLUMN_GAP (18)

/**
 * @brief Pixels reserved on a row's right edge for its bracketed
 *        hints, regardless of whether that row has any
 *
 * Wide enough for the longest possible combination (an exclusive
 * state letter plus every independent flag, e.g. "[f,s,p,!]"), so
 * the name and desktop-name columns always truncate against the same
 * boundary whether or not the hints they are making room for turn
 * out to be empty this row.
 */
#define WM_SEARCH_HINTS_RESERVED_WIDTH (60)

/**
 * @brief Maximum pixel width of a row's name column before it
 *        truncates
 *
 * Leaves room for the desktop-name column beside it even for a very
 * long window title, the same reasoning @c s_titlebar_draw_title
 * (render/desktop.c) truncates a titlebar's own text against the
 * space its buttons leave rather than letting it run underneath
 * them.
 */
#define WM_SEARCH_NAME_MAX_WIDTH (240)

/**
 * @brief Text shown at the top of the results viewport when there
 *        are more entries above the ones currently visible
 */
#define WM_SEARCH_MENU_SCROLL_UP_INDICATOR "---"

/**
 * @brief Text shown at the bottom of the results viewport when
 *        there are more entries below the ones currently visible
 */
#define WM_SEARCH_MENU_SCROLL_DOWN_INDICATOR "---"


#endif  /* ! DEFS_SEARCH_H */
