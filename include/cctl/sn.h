/**
 * @file cctl/sn.h
 *
 * @brief freedesktop.org Startup Notification protocol
 *
 * Implements the launcher side of the "Startup Notification Protocol"
 * (as used by @c libstartup-notification and every major desktop
 * environment).  When the window manager launches a process on the
 * user's behalf, it generates a unique startup ID, hands it to the
 * child as the @c DESKTOP_STARTUP_ID environment variable, and
 * broadcasts a @c _NET_STARTUP_INFO_BEGIN message so that
 * startup-notification-aware applications and any other tool watching
 * (a taskbar, a dock) know a launch is in progress.
 *
 * While any launch is pending, every managed root window shows a busy
 * (watch) cursor.  The sequence ends, and the cursor is restored, as
 * soon as either the launched application itself broadcasts a
 * @c ("remove:" message) (most GTK and Qt applications do this
 * automatically once their main window is ready), or a fixed timeout
 * elapses, whichever comes first; not every application is
 * startup-notification aware, so the timeout is what keeps
 * a non-conforming one from leaving the busy cursor on indefinitely.
 *
 * @note Only the launcher side is implemented here, and not window
 *       association, which is the matching of a newly mapped window
 *       back to the startup sequence that produced it, for placement
 *       or focus decisions
 * @note Ending the busy cursor is the only user-visible behavior that
 *       currently depends on that association
 *
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CCTL_SN_H
#define CCTL_SN_H

/* System includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>

/* Default initial values */
#include <defs/sn.h>


/**
 * @brief Override how many seconds a startup-notification sequence
 *        waits before being expired automatically
 *
 * Called once after loading or reloading configuration; every sequence
 * already pending keeps whichever timeout was in effect when it began,
 * only sequences started after this call use the new value.
 *
 * @param seconds New timeout in seconds; @c 0 is ignored and leaves the
 *                previous value (or the @c SN_TIMEOUT_SECONDS built-in
 *                default, if this is never called) in effect
 *
 * @note Complexity: @e O(1)
 */
void cctl_sn_set_timeout_seconds(uint32_t seconds);

/**
 * @brief Begin a startup-notification sequence for a process about to
 *        be launched
 *
 * Generates a unique startup ID, broadcasts @c _NET_STARTUP_INFO_BEGIN
 * on every managed root window, shows the busy cursor, and registers
 * the sequence so it can be expired by @a cctl_sn_tick if nothing ever
 * completes it.
 *
 * @param connection XCB connection
 * @param surfaces   Managed surfaces, one root window per screen
 * @param name       Human-readable application name to publish in the
 *                   message (e.g., the command being launched); may be
 *                   null
 * @param out_id     Buffer to receive the generated startup ID,
 *                   suitable for passing to the child process as
 *                   @c DESKTOP_STARTUP_ID
 * @param out_id_size Size of @p out_id in bytes
 *
 * @return Status of the operation
 * @retval  true on success
 * @retval false if @p out_id is too small or a connection error
 *               prevented broadcasting the message
 *
 * @note Complexity: @e O(s), where @e s is the number of managed
 *       surfaces
 */
bool cctl_sn_begin(xcb_connection_t *connection, list_td *surfaces,
        const char *restrict name, char *restrict out_id,
        size_t out_id_size);

/**
 * @brief Handle a @c _NET_STARTUP_INFO_BEGIN or @c _NET_STARTUP_INFO
 *        @c ClientMessage
 *
 * Reassembles the chunked text these messages carry (the protocol
 * splits any message longer than one @c ClientMessage's 20-byte payload
 * across several consecutive messages) and, once a complete
 * @c ("remove:" message) naming a pending sequence is received,
 * completes that sequence and restores the normal cursor if no other
 * sequence is still pending.
 *
 * @param connection XCB connection
 * @param surfaces   Managed surfaces, one root window per screen
 * @param event      The @c ClientMessage event
 *
 * @note Complexity: @e O(p), where @e p is the number of currently
 *       pending sequences
 */
void cctl_sn_handle_client_message(xcb_connection_t *connection,
        list_td *surfaces, const xcb_client_message_event_t *event);

/**
 * @brief Milliseconds until the next pending sequence times out
 *
 * @return Milliseconds until the soonest pending sequence's timeout, or
 *         @c -1 when no sequence is pending
 *
 * @note Complexity: @e O(p), where @e p is the number of currently
 *       pending sequences
 */
int cctl_sn_ms_remaining(void);

/**
 * @brief Cancel a pending sequence immediately, e.g., because the
 *        process it was started for failed to actually launch
 *
 * Restores the normal cursor if no other sequence remains pending.
 *
 * @param connection XCB connection
 * @param surfaces   Managed surfaces, one root window per screen
 * @param id         Startup ID previously returned by @a cctl_sn_begin
 *
 * @note A no-op if @p id does not name a currently pending sequence
 * @note Complexity: @e O(p), where @e p is the number of currently
 *       pending sequences
 */
void cctl_sn_cancel(xcb_connection_t *connection, list_td *surfaces,
        const char *id);

/**
 * @brief Expire any pending sequence whose timeout has elapsed
 *
 * Restores the normal cursor on every managed root window once no
 * sequence remains pending, whether it ended by timing out here or by
 * an earlier call to @a cctl_sn_handle_client_message.
 *
 * @param connection XCB connection
 * @param surfaces   Managed surfaces, one root window per screen
 *
 * @note Complexity: @e O(s + p), where @e s is the number of managed
 *       surfaces and @e p is the number of currently pending sequences
 */
void cctl_sn_tick(xcb_connection_t *connection, list_td *surfaces);


#endif  /* ! CCTL_SN_H */
