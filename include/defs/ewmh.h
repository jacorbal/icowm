/**
 * @file defs/ewmh.h
 *
 * @brief EWMH-related constants for the window manager
 *
 * Centralizes every constant that relates to the Extended Window Manager
 * Hints protocol, keeping them separate from the generic window manager
 * constants in @c defs/wm.h so that EWMH compliance can be audited and
 * maintained in one place.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_EWMH_H
#define DEFS_EWMH_H


/**
 * @brief Window-manager name exposed through EWMH
 *
 * Used as the value of @c _NET_WM_NAME on the supporting window and as
 * the label reported via @c _NET_SUPPORTING_WM_CHECK.  Uses the
 * build-time @c PROJECT_NAME_SHORT macro when available and falls back
 * to a portable default otherwise.
 */

#ifdef PROJECT_NAME_SHORT
#define WM_EWMH_NAME PROJECT_NAME_SHORT
#else
#define WM_EWMH_NAME "IcoWM"
#endif  /* ! PROJECT_NAME_SHORT */

/**
 * @brief Number of atoms listed in @c _NET_SUPPORTED
 *
 * Must equal the number of @c (supported_atoms[n_supported++]) lines in
 * @c wm_ewmh_init.  Update this constant whenever atoms are added to or
 * removed from the supported set.
 *
 * Current set (34 atoms):
 *   @c _NET_SUPPORTED, @c _NET_SUPPORTING_WM_CHECK,
 *   @c _NET_CLIENT_LIST, @c _NET_CLIENT_LIST_STACKING,
 *   @c _NET_NUMBER_OF_DESKTOPS, @c _NET_CURRENT_DESKTOP,
 *   @c _NET_DESKTOP_GEOMETRY, @c _NET_DESKTOP_VIEWPORT,
 *   @c _NET_DESKTOP_NAMES,
 *   @c _NET_WORKAREA, @c _NET_ACTIVE_WINDOW,
 *   @c _NET_WM_NAME, @c _NET_WM_ICON_NAME,
 *   @c _NET_WM_DESKTOP,
 *   @c _NET_WM_STRUT_PARTIAL, @c _NET_WM_STRUT,
 *   @c _NET_WM_STATE,
 *   @c _NET_WM_STATE_HIDDEN, @c _NET_WM_STATE_FULLSCREEN,
 *   @c _NET_WM_STATE_MAXIMIZED_VERT, @c _NET_WM_STATE_MAXIMIZED_HORZ,
 *   @c _NET_WM_STATE_ABOVE, @c _NET_WM_STATE_BELOW,
 *   @c _NET_WM_STATE_STICKY, @c _NET_WM_STATE_SHADED,
 *   @c _NET_WM_STATE_DEMANDS_ATTENTION,
 *   @c _NET_WM_STATE_SKIP_TASKBAR, @c _NET_WM_STATE_SKIP_PAGER,
 *   @c _NET_CLOSE_WINDOW,
 *   @c _NET_WM_WINDOW_TYPE, @c _NET_WM_WINDOW_TYPE_DOCK,
 *   @c _NET_WM_WINDOW_TYPE_NORMAL, @c _NET_WM_WINDOW_TYPE_DIALOG,
 *   @c _NET_MOVERESIZE_WINDOW
 */
#define WM_EWMH_SUPPORTED_COUNT (34)


#endif  /* ! DEFS_EWMH_H */
