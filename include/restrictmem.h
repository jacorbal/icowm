/**
 * @file restrictmem.h
 *
 * @brief Restricted-memory mode's runtime watchdog
 *
 * The startup-time half of restricted-memory mode (refusing to start
 * at all when system memory is already too tight) lives directly in
 * @c wm_start; this module is only the part that keeps watching this
 * process's own memory usage once running, warning through a message
 * dialog if it climbs to the configured ceiling.
 *
 * @see @c -M in @c main.c, @c wm_start in @c wm.h
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RESTRICTMEM_H
#define RESTRICTMEM_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/** How often the watchdog actually re-reads this process's own
 *  memory usage, in seconds; checking on every single main-loop
 *  iteration would mean a file read many times a second for no
 *  benefit, since usage cannot realistically climb meaningfully
 *  faster than this */
#define RESTRICTMEM_CHECK_INTERVAL_SECONDS (5)

/** Once a warning has been shown for climbing at or above the
 *  configured ceiling, usage has to drop back below this fraction of
 *  it before a renewed climb can warn again; without this hysteresis,
 *  usage hovering right at the ceiling would show the same dialog
 *  repeatedly every check interval */
#define RESTRICTMEM_HYSTERESIS_PERCENT (90u)

/**
 * @brief Maximum number of clients (managed windows) any one desktop
 *        will hold in restricted-memory mode
 *
 * Each managed client (see @c client_td) carries real, ongoing memory
 * of its own regardless of anything else this mode restricts: window
 * geometry and hint tracking, a decoration frame, an icon window if
 * iconified, and its own entry in the desktop's client hash table
 * (see @c adt/ohtbl.h) and stacking list.  Unlike @c desktop.count or
 * @c screens.count (see @c config_load), which bound state this mode
 * already forces to its smallest possible value, nothing else caps
 * how many of these a person can accumulate simply by opening more
 * applications, so this is the one restriction in the whole mode that
 * is enforced procedurally (refusing to manage another client past
 * it; see @c handler/map.c) rather than by forcing a configuration
 * value down to begin with.
 *
 * A @c #define rather than a run-time setting since it is meant as a
 * hard safety valve, not something to tune per session; edit and
 * recompile to change it.
 */
#define RESTRICTMEM_MAX_CLIENTS (16u)


/**
 * @brief Set the ceiling the runtime watchdog checks this process's
 *        own memory usage against
 *
 * @param ceiling_mib Restricted-memory mode's ceiling in mebibytes,
 *                    or @c 0 to turn the watchdog off entirely
 *
 * @note Complexity: @e O(1)
 */
void restrictmem_init(uint32_t ceiling_mib);

/**
 * @brief Periodically check this process's own memory usage against
 *        the configured ceiling, warning through a message dialog
 *        once it is reached
 *
 * A no-op, cheap to call every main-loop iteration, when the watchdog
 * is off (see @c restrictmem_init), when fewer than @c
 * RESTRICTMEM_CHECK_INTERVAL_SECONDS have passed since the last
 * actual check, or when a warning is already showing (the message
 * dialog only allows one instance at a time; see @c
 * menu_message_dialog_is_open).
 *
 * @param connection XCB connection, for the warning dialog and to
 *                   read this process's own memory usage
 * @param surface    Surface to center the warning dialog on
 * @param config     Active configuration, for the warning dialog
 *
 * @note Complexity: @e O(1)
 */
void restrictmem_tick(xcb_connection_t *connection,
        surface_td *surface, const config_td *config);

/**
 * @brief Warn through a message dialog that @c RESTRICTMEM_MAX_CLIENTS
 *        has been reached on a desktop
 *
 * The caller (@c handler/map.c) is the one that actually checks the
 * client count against @c RESTRICTMEM_MAX_CLIENTS and decides whether
 * to manage a newly mapped window at all; this only shows the
 * explanation once that decision has already been made.  A no-op if a
 * message dialog is already showing for any other reason (only one
 * instance at a time; see @c menu_message_dialog_is_open), so a burst
 * of several windows opening in quick succession while already at the
 * cap does not stack up a dialog per window.
 *
 * @param connection XCB connection, for the warning dialog
 * @param surface    Surface to center the warning dialog on
 * @param config     Active configuration, for the warning dialog
 *
 * @note Complexity: @e O(1)
 */
void restrictmem_warn_client_cap(xcb_connection_t *connection,
        surface_td *surface, const config_td *config);


#endif  /* ! RESTRICTMEM_H */
