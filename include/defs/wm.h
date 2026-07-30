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
 * @brief Window-manager name exposed through EWMH
 *
 * Uses the build-time @c PROJECT_NAME_SHORT macro when available.
 * Falls back to a portable default when building without project
 * metadata injection.
 */
#ifdef PROJECT_NAME_SHORT
#define WM_EWMH_NAME PROJECT_NAME_SHORT
#else
#define WM_EWMH_NAME "IcoWM"
#endif  /* ! PROJECT_NAME_SHORT */

/** Length of @c WM_EWMH_NAME excluding the null terminator */
//#define WM_EWMH_NAME_LEN (sizeof(WM_EWMH_NAME) - 1u)

/**
 * @brief Initial capacity of windows for a desktop
 *
 * Number of windows that the desktop is initialized with.  A higher
 * initial capacity may reduce the need for resizing the underlying data
 * structure as windows are added to the open-addressed hash table.
 */
#define WM_DESKTOP_INITIAL_CAPACITY (256)  /* (512) ? */

/**
 * @brief Maximum number of characters allowed in the name of the
 *        desktop, including the null terminator
 */
#define WM_DESKTOP_MAX_LENGTH_NAME (64)

/** Desktop identifier when the client is pinned to all desktops */
#define WM_DESKTOP_ID_ALL (0xFFFFFFFFu)

/** Maximum number of supported key bindings */
#define WM_MAX_KEYBINDINGS (128)

/** Maximum number of supported mouse bindings */
#define WM_MAX_MOUSEBINDINGS (8)

/** Minimum supported client window dimension */
#define WM_MIN_WINDOW_DIMENSION (1u)

/** Keyboard move step (pixels) */
#define WM_KEYBOARD_MOVE_STEP (20)

/** Keyboard resize step (pixels) */
#define WM_KEYBOARD_RESIZE_STEP (20)

/** Pixels between baseline and the bottom of the titlebar */
#define WM_TITLEBAR_TEXT_BOTTOM_PAD (6)

/** Maximum length of each info popup text line */
#define WM_INFO_POPUP_LINE_MAX_LEN (256)

/** Maximum number of entries in the cycle menu */
#define WM_CYCLE_MENU_MAX_ENTRIES (64)

/** Maximum label length for a cycle menu entry */
#define WM_CYCLE_MENU_ENTRY_LEN (128)

/** Height of each row in the cycle menu, in pixels */
#define WM_CYCLE_MENU_ROW_HEIGHT (20)

/** Horizontal padding inside the cycle menu window */
#define WM_CYCLE_MENU_PAD_X (10)

/** Vertical padding (top/bottom) inside the cycle menu window */
#define WM_CYCLE_MENU_PAD_Y (6)

/** Width/height of icon square in pixels */
#define WM_ICON_SQUARE_SIZE (48u)

/** Caption area below icon in pixels */
#define WM_ICON_CAPTION_HEIGHT (14u)

/** Decoration button side pixels */
#define WM_DECOR_BTN_SIZE (12u)

/** Gap between buttons */
#define WM_DECOR_BTN_GAP (2u)

/** Padding from frame edge */
#define WM_DECOR_BTN_PAD (4u)

/** Threshold below which an icon drag is treated as a click */
#define WM_ICON_DRAG_THRESHOLD (16)     /* 4 (px) × 4 (px) = 16 (px^2) */

/** Maximum interval in milliseconds between two presses on the same
 *  titlebar that is recognised as a double-click. */
#define WM_DOUBLE_CLICK_MS (400u)


#endif  /* ! DEFS_WM_H */
