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
#define WM_MAX_KEYBINDINGS (160)

/** Maximum number of supported mouse bindings */
#define WM_MAX_MOUSEBINDINGS (8)

/** Minimum supported client window dimension */
#define WM_MIN_WINDOW_DIMENSION (1u)

/** Default titlebar height in pixels */
#define WM_TITLEBAR_DEFAULT_HEIGHT (22u)

/**
 * @brief Fallback client width and height when geometry cannot be
 *        queried from the X server
 */
#define WM_CLIENT_DEFAULT_DIM (100u)

/**
 * @brief Glyph index of @c XC_left_ptr (source) in the X cursor font
 *
 * The X cursor font stores source and mask glyphs in pairs.  The left
 * pointer cursor uses glyph 68 as the source shape and glyph 69 as its
 * transparency mask.
 */
#define WM_CURSOR_LEFT_PTR_GLYPH      (68u)

/** Glyph index of the mask for @c XC_left_ptr in the X cursor font */
#define WM_CURSOR_LEFT_PTR_MASK_GLYPH (69u)

/** Pixels between baseline and the bottom of the titlebar */
#define WM_TITLEBAR_TEXT_BOTTOM_PAD (6)

/** Maximum length of each info popup text line */
#define WM_INFO_POPUP_LINE_MAX_LEN (256)

/** Maximum number of entries in the cycle menu */
#define WM_CYCLE_MENU_MAX_ENTRIES (128)

/** Maximum label length for a cycle menu entry */
#define WM_CYCLE_MENU_ENTRY_LEN (128)

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
#define WM_CYCLE_MENU_MAX_HEIGHT_PERC (80)

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

/**
 * @brief Size in pixels of the corner zone used for edge/corner resize
 *
 * Clicks on the frame border within this many pixels of a frame corner
 * are treated as corner-resize initiation events rather than plain
 * border clicks.
 */
#define WM_RESIZE_CORNER_SIZE (12)
/**
 * @brief Duration in milliseconds before the info popup auto-closes
 *
 * The popup opened by the client-info key binding stays visible for
 * this long after being shown and then closes automatically.
 */
#define WM_INFO_POPUP_TIMEOUT_MS (1000)

/** Extra border pixels added to the selected icon in the cycle menu */
#define WM_ICON_CYCLE_SEL_BORDER_EXTRA (1u)

/** Maximum interval in milliseconds between two presses on the same
 *  titlebar that is recognized as a double-click */
#define WM_DOUBLE_CLICK_MS (400u)

/** Poll timeout (ms) for one main-loop iteration */
#define WM_EVENT_POLL_TIMEOUT_MS (1000)

/** Duration in milliseconds for the desktop-switch notification */
#define WM_DESKTOP_NOTIFY_TIMEOUT_MS (400)


#endif  /* ! DEFS_WM_H */
