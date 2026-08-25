/**
 * @file wm/startup.h
 *
 * @brief Window manager startup helpers: extension probing
 *
 * Split by competency into @c wm/startup/install.h (registering signal
 * handlers), @c wm/startup/handle.h (the handlers themselves and the
 * flags they set, queried back by the main loop), and
 * @c wm/startup/subscribe.h, for the X server event subscriptions.
 * This header keeps only the two X extension probes that belong to
 * no single one
 * of those.
 *
 * @ingroup loop
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_STARTUP_H
#define WM_STARTUP_H


/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Initialize optional XRandR support
 *
 * Probes the XRandR extension, stores extension metadata in @p wm, and
 * negotiates a compatible protocol version when available.
 *
 * @param wm Window manager state
 *
 * @return Status of the operation
 * @retval  0 on success or when XRandR is unavailable
 * @retval -1 on fatal input
 */
int wm_startup_init_randr(wm_td *wm);

/**
 * @brief Probe XSync extension support and cache metadata in @p wm
 *
 * Used for @c _NET_WM_SYNC_REQUEST.  Caches the base event code so
 * @c AlarmNotify events can be recognized in the main loop.  No
 * per-root event subscription is required for the XSync extension
 * (unlike XRandR); alarms deliver their notifications directly to the
 * connection that created them.
 *
 * @param wm Window manager state
 *
 * @return Status of the operation
 * @retval  0 on success or when XSync is unavailable
 * @retval -1 on fatal input
 */
int wm_startup_init_sync(wm_td *wm);


#endif  /* ! WM_STARTUP_H */
