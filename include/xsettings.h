/**
 * @file xsettings.h
 *
 * @brief Built-in XSETTINGS manager declarations
 *
 * Implements the freedesktop.org "XSETTINGS" specification: acquiring
 * the @c _XSETTINGS_Sn manager selection on a window and publishing
 * a @c _XSETTINGS_SETTINGS property that GTK/Qt applications read (and
 * watch for changes on) to learn the current theme name, icon theme,
 * cursor theme, and display DPI; the same mechanism a standalone daemon
 * like @c xsettingsd provides, gated by @c config.xsettings.is_enabled.
 *
 * @defgroup xsettings XSETTINGS protocol
 * @ingroup surface
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef XSETTINGS_H
#define XSETTINGS_H


/* Project includes */
#include <types/handles.h>


/* XSETTINGS setting-value type codes, per the specification */
#define XS_TYPE_INTEGER (0u)    /**< XSETTINGS integer value code */
#define XS_TYPE_STRING (1u)     /**< XSETTINGS string vaue code */

/**
 * @brief Byte-order code for a little-endian-encoded property
 *
 * Used unconditionally in @c xsettings.c, since every byte there is
 * written explicitly least-significant-first, regardless of host
 * endianness.
 */
#define XS_BYTE_ORDER_LSB (0u)


/* Public interface */
/**
 * @brief Acquire the XSETTINGS selection and publish the settings
 *
 * A no-op when @c wm->config->theme.xsettings.is_enabled is @c false,
 * when @p wm has no managed surfaces yet, or when another settings
 * manager already owns the @c _XSETTINGS_Sn selection on the first
 * surface's screen.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(1)
 */
void xsettings_init(const wm_td *wm);

/**
 * @brief Fully tear down: release the selection and destroy the window
 *
 * Only meant for the window manager itself exiting; use
 * @c xsettings_reload to react to a configuration reload instead, so
 * a disable-then-re-enable cycle does not force every watching
 * application to re-discover the manager from scratch.  Safe to call
 * even when @c xsettings_init was never called or did not acquire
 * ownership.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(1)
 */
void xsettings_shutdown(wm_td *wm);

/**
 * @brief React to a configuration reload
 *
 * Reconciles the live manager with the just-reloaded
 * @c wm->config->theme.xsettings settings:
 *
 * - Was enabled, now disabled: releases the selection right away,
 *   keeping the window itself (nothing to preserve on it besides the
 *   property, which simply becomes stale and unread once no client
 *   can find an owner for the selection any more).
 * - Was disabled, now enabled: creates the window if this is the very
 *   first time, then acquires the selection and publishes the current
 *   settings.
 * - Was and remains enabled: rebuilds and republishes the settings
 *   property with an incremented serial number whenever any configured
 *   value differs from what is currently published, so applications
 *   watching for property changes pick up the new theme/DPI/etc.
 *   immediately without needing to restart.
 * - Was and remains disabled: a no-op.
 *
 * @param wm Window manager state, with @c wm->config already reloaded
 *
 * @note Complexity: @e O(1)
 */
void xsettings_reload(const wm_td *wm);


#endif  /* ! XSETTINGS_H */
