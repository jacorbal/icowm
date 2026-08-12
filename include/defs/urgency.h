/**
 * @file defs/urgency.h
 *
 * @brief Timing for the urgent-client attention blink
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

#ifndef DEFS_URGENCY_H
#define DEFS_URGENCY_H


/**
 * @brief Milliseconds between one blink phase and the next for an
 *        urgent client's titlebar and icon
 *
 * Applies equally to both, so a client that is both decorated and
 * has an iconified twin (unusual, but not impossible) never shows
 * the two visibly out of step with one another.
 */
#define WM_URGENCY_BLINK_INTERVAL_MS (600)


#endif  /* ! DEFS_URGENCY_H */
