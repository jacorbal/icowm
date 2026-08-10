/**
 * @file defs/loop.h
 *
 * @brief Timing constants for the main event loop's @c poll wait
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

#ifndef DEFS_LOOP_H
#define DEFS_LOOP_H


/** Poll timeout (ms) for one main-loop iteration */
#define WM_EVENT_POLL_TIMEOUT_MS (1000)

/** Fallback poll timeout (ms) used to wait out the rest of the current
 *  wall-clock second before the systray clock is due its next redraw */
#define WM_SYSTRAY_CLOCK_POLL_MS (250)

/**
 * @brief Default seconds between re-reading the systray battery
 *        status, overridable via @c systray.battery.poll-seconds in
 *        @c config.json
 *
 * Far coarser than the clock's own per-second redraw: a percentage
 * does not need per-second freshness, and every poll costs a handful
 * of file reads that a plain wall-clock display has no reason to pay
 * for that often.
 */
#define WM_SYSTRAY_BATTERY_POLL_SECONDS (30)


#endif  /* ! DEFS_LOOP_H */
