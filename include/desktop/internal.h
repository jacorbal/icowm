/**
 * @file desktop/internal.h
 *
 * @brief Private helpers shared across desktop implementation modules
 *
 * Declares functions that are used by more than one of the desktop
 * translation units (@c desktop/desktop.c, @c desktop/dclient.c) but
 * must not be exposed as part of the public desktop API declared in
 * @c desktop.h.
 *
 * @note This header is private to the desktop subsystem and must not be
 *       included outside of @c src/desktop/
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DESKTOP_INTERNAL_H
#define DESKTOP_INTERNAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Project includes */
#include <desktop.h>


/**
 * @brief Enable or disable all clients on a desktop
 *
 * Iterates over the desktop's client collection and updates each
 * client's enabled and focusable state according to @p enabled.
 *
 * @param desktop Target desktop containing the clients
 * @param enabled If @c true, clients are enabled and focusable;
 *                otherwise, they are disabled and not focusable
 *
 * @return Status code
 * @retval  0 Success
 * @retval  1 Invalid input (@p desktop or its client list is null)
 *
 * @note Implemented in @c desktop/dclient.c
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
int di_set_clients_enabled(desktop_td *desktop, bool enabled);


#endif  /* ! DESKTOP_INTERNAL_H */
