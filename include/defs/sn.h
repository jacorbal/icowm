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
 * @see @p sn_set_timeout_seconds
 */
#define SN_TIMEOUT_SECONDS (15)


#endif  /* ! DEFS_SN_H */
