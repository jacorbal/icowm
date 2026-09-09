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
 * soon as any one of three things happens, whichever comes first: the
 * launched application itself broadcasts a @c ("remove:" message)
 * (most GTK and Qt applications do this automatically once their main
 * window is ready); a newly mapped window's own @c _NET_WM_PID names
 * the very process this window manager launched
 * (@a cctl_sn_complete_for_pid), which most X11 applications publish
 * whether or not they know anything about this protocol, xterm among
 * them; or a fixed timeout elapses, which is what keeps an application
 * answering to neither of those two from leaving the busy cursor on
 * indefinitely.
 *
 * @note Window association exists for initial desktop placement:
 *       @a cctl_sn_desktop_for_window matches a newly mapped window's
 *       own @c _NET_STARTUP_ID back to the pending sequence that
 *       produced it, so a slow-starting application lands on the
 *       desktop it was launched from rather than whichever one happens
 *       to be current once it finally maps
 * @note That association does not follow @c WM_CLIENT_LEADER the way
 *       the specification allows for a group's other windows, and it
 *       plays no part in any focus decision
 * @note Ending the busy cursor by @c _NET_WM_PID
 *       (@a cctl_sn_complete_for_pid) needs no @c _NET_STARTUP_ID
 *       association at all, unlike desktop placement above; it only
 *       needs @a cctl_sn_associate_pid to have recorded the launched
 *       process's own PID first, which
 *       @a desktop_action_process_launch_with_class
 *       (@c desktop/dclient.c) does right after a successful spawn
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
#include <sys/types.h>  /* pid_t */

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
 * completes it.  @p origin_desktop is recorded alongside the sequence
 * purely so a later @a cctl_sn_desktop_for_window can recover it; it
 * has no other effect on the sequence itself.
 *
 * @param connection     XCB connection
 * @param surfaces       Managed surfaces, one root window per screen
 * @param name           Human-readable application name to publish in
 *                       the message (e.g., the command being launched);
 *                       may be null
 * @param origin_desktop Desktop the launch was requested from
 * @param out_id         Buffer to receive the generated startup ID,
 *                       suitable for passing to the child process as
 *                       @c DESKTOP_STARTUP_ID
 * @param out_id_size    Size of @p out_id in bytes
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
        const char *restrict name, uint32_t origin_desktop,
        char *restrict out_id, size_t out_id_size);

/**
 * @brief Record the PID of the process a pending sequence actually
 *        launched
 *
 * Called once, right after a successful spawn;
 * @a cctl_sn_complete_for_pid is what later reads this back, matching
 * it against a newly mapped window's own @c _NET_WM_PID to end the
 * sequence even when the launched application never broadcasts a
 * @c ("remove:" message) of its own.
 *
 * @param id  Startup ID @a cctl_sn_begin returned for this sequence
 * @param pid PID of the process actually launched for it
 *
 * @note A no-op if @p id no longer names a pending sequence (already
 *       completed, canceled, or timed out) by the time the spawn
 *       finishes
 * @note Complexity: @e O(p), where @e p is the number of currently
 *       pending sequences
 */
void cctl_sn_associate_pid(const char *id, pid_t pid);

/**
 * @brief Look up the desktop a newly mapped window's startup sequence
 *        was launched from
 *
 * Reads @p window's own @c _NET_STARTUP_ID property (set by
 * a startup-notification-aware toolkit from the @c DESKTOP_STARTUP_ID
 * environment variable this window manager itself hands its children)
 * and, if it names a sequence still pending, recovers the desktop that
 * @a cctl_sn_begin recorded for it.
 *
 * A peek, not a consuming lookup: the pending sequence itself is left
 * untouched, since ending its busy cursor depends only on the
 * @c ("remove:" message) or the timeout, and a single sequence could in
 * theory still go on to map more than one window.
 *
 * @param connection  XCB connection
 * @param window      Newly mapped window to check
 * @param out_desktop Set to the origin desktop on a match; left
 *                    untouched otherwise
 *
 * @return Status of the lookup
 * @retval  true @p window carries a @c _NET_STARTUP_ID naming
 *               a still-pending sequence, and @p out_desktop was set
 * @retval false @p window has no such property, or it names no
 *               currently pending sequence
 *
 * @note Complexity: @e O(p), where @e p is the number of currently
 *       pending sequences
 */
bool cctl_sn_desktop_for_window(xcb_connection_t *connection,
        xcb_window_t window, uint32_t *out_desktop);

/**
 * @brief End the pending sequence, if any, that launched the process
 *        owning a newly mapped window
 *
 * A fallback completion path for an application that never broadcasts
 * a @c ("remove:" message) of its own (xterm and most other classic X11
 * applications, as opposed to most GTK and Qt ones): @p window's own
 * @c _NET_WM_PID, which the X server or the toolkit publishes
 * independently of any startup-notification awareness, is matched
 * against whichever PID @a cctl_sn_associate_pid recorded for each
 * still-pending sequence.
 *
 * Unlike @a cctl_sn_desktop_for_window, this call is consuming: a match
 * ends the sequence outright (the same as the @c ("remove:" message)
 * would), clearing the busy cursor once no sequence remains pending.
 *
 * @param connection XCB connection
 * @param surfaces   Every managed surface, to clear the busy cursor on
 * @param window     Newly mapped window to check
 *
 * @return Whether a pending sequence was matched and ended
 *
 * @note Complexity: @e O(p), where @e p is the number of currently
 *       pending sequences
 */
bool cctl_sn_complete_for_pid(xcb_connection_t *connection,
        list_td *surfaces, xcb_window_t window);

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
