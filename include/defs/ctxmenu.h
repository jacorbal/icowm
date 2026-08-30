/**
 * @file defs/ctxmenu.h
 *
 * @brief Dimensions and capacity limits for the generic context menu
 *        implementation (root menu, window menu, window list, and their
 *        submenus)
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

#ifndef DEFS_CTXMENU_H
#define DEFS_CTXMENU_H


/** Maximum number of entries in a context menu (including submenus) */
#define WM_CTXMENU_MAX_ENTRIES (64)

/** Maximum number of nesting levels for context menu submenus */
#define WM_CTXMENU_MAX_DEPTH (4)

/** Maximum length of a context menu entry label */
#define WM_CTXMENU_LABEL_MAX_LENGTH (128)

/** Maximum length of a context menu entry command string */
#define WM_CTXMENU_CMD_MAX_LENGTH (256)

/** Row height in pixels for a context menu entry */
#define WM_CTXMENU_ROW_HEIGHT (20)

/** Horizontal padding inside the context menu window */
#define WM_CTXMENU_PAD_X (12)

/** Vertical padding (top/bottom) inside the context menu window */
#define WM_CTXMENU_PAD_Y (4)

/** Minimum width of a context menu window in pixels */
#define WM_CTXMENU_MIN_WIDTH (160)

/** Height of a separator row in pixels */
#define WM_CTXMENU_SEP_HEIGHT (8)

/**
 * @brief Margin, in pixels, kept between a menu row's icon square
 *        and the top/bottom edges of that row
 *
 * Shared with the @c Alt+Tab style cycle menu (see @c defs/cycle.h and
 * @c menu/cycle/draw.c), not just this file's context menu.  Both
 * size their per-row application icon (see @p theme.menu.show-pixmaps
 * in @c config.h) as @c (row_height @c - @c WM_MENU_ICON_INSET) square,
 * so the icon never quite touches the row's top and bottom edges.
 *
 * @note Every site computing that difference guards it with @c #if
 *       rather than a ternary, both operands being fixed compile-time
 *       constants that would leave one branch provably unreachable
 *       (@c -Wunreachable-code)
 * @note The guard still catches a later edit to either constant that
 *       would otherwise underflow in silence
 */
#define WM_MENU_ICON_INSET (4)


#endif  /* ! DEFS_CTXMENU_H */
