/**
 * @file defs/popup.h
 *
 * @brief Dimensions and timing for the client-info popup
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

#ifndef DEFS_POPUP_H
#define DEFS_POPUP_H


/**
 * @brief Maximum length of each info popup text line
 */
#define WM_INFO_POPUP_LINE_MAX_LENGTH (256)

/**
 * @brief Duration in milliseconds before the info popup auto-closes
 *
 * The popup opened by the client-info key binding stays visible for
 * this long after being shown and then closes automatically.
 */
#define WM_INFO_POPUP_TIMEOUT_MS (1000)


#endif  /* ! DEFS_POPUP_H */
