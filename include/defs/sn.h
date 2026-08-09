/**
 * @file defs/sn.h
 *
 * @brief Default values for the startup-notification subsystem
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
 *        being expired automatically, overridable via
 *        @c startup-notification.timeout-seconds in @c config.json
 *        (see @c sn_set_timeout_seconds)
 *
 * Not every launched application is startup-notification aware, so
 * this is what keeps the busy cursor from staying on indefinitely
 * when nothing ever broadcasts a "remove:" message.
 */
#define SN_TIMEOUT_SECONDS (15)


#endif  /* ! DEFS_SN_H */
