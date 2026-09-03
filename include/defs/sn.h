/**
 * @file defs/sn.h
 *
 * @brief Default values for the startup-notification subsystem
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

#ifndef DEFS_SN_H
#define DEFS_SN_H


/**
 * @brief Default seconds a startup-notification sequence waits before
 *        being expired automatically
 *
 * Not every launched application is startup-notification aware, so this
 * is what keeps the busy cursor from staying on indefinitely when
 * nothing ever broadcasts a "remove:" message.
 *
 * @note Overridable via @a startup-notification.timeout-seconds in
 *       @c config.json
 *
 * @see @p cctl_sn_set_timeout_seconds
 */
#define SN_TIMEOUT_SECONDS (15)

/** Maximum length of a generated or received startup ID */
#define SN_ID_MAX_LEN (128)

/** Maximum simultaneously pending (unacknowledged) launch sequences */
#define SN_MAX_PENDING (16)

/** Maximum reassembly length for one incoming chunked message */
#define SN_MSG_MAX_LEN (512)

/** Maximum simultaneous in-progress incoming reassemblies (one per
 *  sender window sending interleaved chunks) */
#define SN_MAX_REASSEMBLY (8)

/**
 * @brief Milliseconds an in-progress reassembly slot may sit idle
 *        before it is forcibly recycled
 *
 * The chunks of one message arrive back-to-back in the same burst, so
 * a slot that has not seen a fragment in this long belongs to a sender
 * that stopped mid-message (crashed, closed the window, or never sent
 * the final short chunk) rather than one that is merely running slow;
 * without this, such a slot would sit reserved forever and, with
 * enough of them, permanently starve @c SN_MAX_REASSEMBLY.
 */
#define SN_REASSEMBLY_TIMEOUT_MS (5000)

/** Bytes of text payload in one format-8 @c ClientMessage */
#define SN_CHUNK_LEN (20)


#endif  /* ! DEFS_SN_H */
