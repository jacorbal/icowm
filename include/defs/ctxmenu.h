/**
 * @file defs/ctxmenu.h
 *
 * @brief Dimensions and capacity limits for the generic context menu
 *        implementation (root menu, window menu, window list, and
 *        their submenus)
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
#define WM_CTXMENU_LABEL_MAX_LEN (128)

/** Maximum length of a context menu entry command string */
#define WM_CTXMENU_CMD_MAX_LEN (256)

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


#endif  /* ! DEFS_CTXMENU_H */
