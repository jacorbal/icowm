/**
 * @file loop/refresh.h
 *
 * @brief End-of-iteration repaint and property sync for the main loop
 *
 * Whatever an iteration did, handling events or letting a countdown
 * elapse, ends up recorded as an outdated flag on a surface rather
 * than painted on the spot.  This is where those flags are turned
 * into actual redraws, and where the root window's EWMH properties
 * are brought back in line with what changed.
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

#ifndef LOOP_REFRESH_H
#define LOOP_REFRESH_H


/* Local includes */
#include <loop/context.h>


/* Public interface */
/**
 * @brief Close whatever expired, repaint what changed, and resync
 *
 * Runs once at the end of every loop iteration.  Closes the two
 * single-instance overlays whose display timeout has just elapsed,
 * re-renders the surfaces marked outdated, and writes the root
 * window's EWMH properties again only if something actually was
 * outdated.
 *
 * @param ctx Main loop context
 *
 * @note Syncing unconditionally would keep @c poll permanently
 *       readable: the root window has @c PROPERTY_CHANGE selected, so
 *       every write comes back as a @c PropertyNotify of its own,
 *       waking the loop up to do nothing
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       and @e m the number of desktops on the outdated ones
 */
void loop_refresh(const loop_ctx_td *ctx);

/**
 * @brief Mark every surface outdated and render them all
 *
 * Called once before entering the event loop, so that windows already
 * on screen when the window manager starts are drawn from scratch
 * rather than waiting for an event to touch them.
 *
 * @param ctx Main loop context
 *
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       and @e m the number of desktops
 */
void loop_refresh_full(const loop_ctx_td *ctx);


#endif  /* ! LOOP_REFRESH_H */
