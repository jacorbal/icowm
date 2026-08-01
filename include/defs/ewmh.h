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
 * Current set (32 atoms):
 *   _NET_SUPPORTED, _NET_SUPPORTING_WM_CHECK,
 *   _NET_CLIENT_LIST, _NET_CLIENT_LIST_STACKING,
 *   _NET_NUMBER_OF_DESKTOPS, _NET_CURRENT_DESKTOP,
 *   _NET_DESKTOP_GEOMETRY, _NET_DESKTOP_VIEWPORT,
 *   _NET_DESKTOP_NAMES,
 *   _NET_WORKAREA, _NET_ACTIVE_WINDOW,
 *   _NET_WM_NAME, _NET_WM_ICON_NAME,
 *   _NET_WM_DESKTOP,
 *   _NET_WM_STRUT_PARTIAL, _NET_WM_STRUT,
 *   _NET_WM_STATE,
 *   _NET_WM_STATE_HIDDEN, _NET_WM_STATE_FULLSCREEN,
 *   _NET_WM_STATE_MAXIMIZED_VERT, _NET_WM_STATE_MAXIMIZED_HORZ,
 *   _NET_WM_STATE_ABOVE, _NET_WM_STATE_BELOW,
 *   _NET_WM_STATE_STICKY, _NET_WM_STATE_SHADED,
 *   _NET_WM_STATE_DEMANDS_ATTENTION,
 *   _NET_CLOSE_WINDOW,
 *   _NET_WM_WINDOW_TYPE, _NET_WM_WINDOW_TYPE_DOCK,
 *   _NET_WM_WINDOW_TYPE_NORMAL, _NET_WM_WINDOW_TYPE_DIALOG,
 *   _NET_MOVERESIZE_WINDOW
 */
#define WM_EWMH_SUPPORTED_COUNT (32)


#endif  /* ! DEFS_EWMH_H */
