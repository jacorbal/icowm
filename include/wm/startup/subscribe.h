/**
 * @file wm/startup/subscribe.h
 *
 * @brief Subscribing to X server events on every managed surface
 *
 * @ingroup startup
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_STARTUP_SUBSCRIBE_H
#define WM_STARTUP_SUBSCRIBE_H


/* Project includes */
#include <wm.h>


/**
 * @brief Subscribe to XRandR notifications on each managed root
 *        window
 *
 * @param wm Window manager state
 *
 * @return 0 on success (including when XRandR is unavailable), -1 if
 *         @p wm or its members are null
 */
int wm_startup_subscribe_randr_events(const wm_td *wm);

/**
 * @brief Subscribe to root window events on all managed surfaces
 *
 * Also sets a default left-pointer cursor on every root window; see
 * the implementation's comment for why that lives here.
 *
 * @param wm Window manager state
 *
 * @return 0 on success, -1 if @p wm or its members are null, or if
 *         another window manager already holds the root event
 *         subscription
 */
int wm_startup_subscribe_root_events(const wm_td *wm);


#endif  /* ! WM_STARTUP_SUBSCRIBE_H */
