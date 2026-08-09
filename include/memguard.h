/**
 * @file memguard.h
 *
 * @brief Restricted-memory mode's runtime watchdog and dynamic
 *        client-count cap
 *
 * The startup-time half of restricted-memory mode (refusing to start
 * at all when system memory is already too tight) lives directly in
 * @c wm_start; this module is the part that keeps watching this
 * process's own memory usage once running, computes how many clients
 * a given @c -M ceiling can actually afford, and warns through
 * message dialogs when either limit is reached.
 *
 * Every tunable number this module uses lives in @c defs/memguard.h,
 * not here.
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

#ifndef MEMGUARD_H
#define MEMGUARD_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <config.h>
#include <surface.h>


/**
 * @brief Set the ceiling this module checks against, and compute the
 *        client cap that ceiling affords
 *
 * @param ceiling_mib Restricted-memory mode's ceiling in mebibytes,
 *                    or @c 0 to turn the whole module off (both the
 *                    runtime memory watchdog and the client cap)
 *
 * @note Complexity: @e O(1)
 */
void memguard_init(uint32_t ceiling_mib);

/**
 * @brief The maximum number of clients (managed windows) any one
 *        desktop should hold under the ceiling set by the last
 *        @c memguard_init call
 *
 * Computed once, when @c memguard_init sets the ceiling, from
 * @c defs/memguard.h's own @c MEMGUARD_BASELINE_MIB, @c MEMGUARD_KIB_
 * PER_CLIENT, @c MEMGUARD_MIN_CLIENTS, and @c MEMGUARD_ABSOLUTE_MAX_
 * CLIENTS, not read live off any actual measurement: restricted-
 * memory mode has no way to measure any one client's own real memory
 * cost, so this is an estimate, deliberately a conservative
 * (generous) one; see @c MEMGUARD_KIB_PER_CLIENT's own documentation.
 *
 * Used both to size a desktop's own client hash table up front (see
 * @c desktop_init) and to decide when @c handler/map.c should refuse
 * to manage another client.
 *
 * @return The client cap, or @c 0 when the module is off (see
 *         @c memguard_init); callers should treat @c 0 as "no cap",
 *         not as "cap of zero clients"
 *
 * @note Complexity: @e O(1)
 */
uint32_t memguard_max_clients(void);

/**
 * @brief Periodically check this process's own memory usage against
 *        the configured ceiling, warning through a message dialog
 *        once it is reached
 *
 * A no-op, cheap to call every main-loop iteration, when the watchdog
 * is off (see @c memguard_init), when fewer than @c MEMGUARD_CHECK_
 * INTERVAL_SECONDS have passed since the last actual check, or when a
 * warning is already showing (the message dialog only allows one
 * instance at a time; see @c menu_message_dialog_is_open).
 *
 * @param connection XCB connection, for the warning dialog and to
 *                   read this process's own memory usage
 * @param surface    Surface to center the warning dialog on
 * @param config     Active configuration, for the warning dialog
 *
 * @note Complexity: @e O(1)
 */
void memguard_tick(xcb_connection_t *connection,
        surface_td *surface, const config_td *config);

/**
 * @brief Warn through a message dialog that @c memguard_max_clients
 *        has been reached on a desktop
 *
 * The caller (@c handler/map.c) is the one that actually checks the
 * client count against @c memguard_max_clients and decides whether to
 * manage a newly requested window at all (leaving it unmapped
 * entirely when it declines, rather than leaving it broken and
 * unmanaged; see the caller's own comment for why); this only shows
 * the explanation once that decision has already been made.  A no-op if a
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
void memguard_warn_client_cap(xcb_connection_t *connection,
        surface_td *surface, const config_td *config);


#endif  /* ! MEMGUARD_H */
