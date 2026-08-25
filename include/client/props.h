/**
 * @file client/props.h
 *
 * @brief Client property block
 *
 * The state, layer, flags, type, operation and focusing words a
 * client carries, all of them read through the predicates in
 * @c client/predicates.h rather than directly.
 *
 * @ingroup client
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CLIENT_PROPS_H
#define CLIENT_PROPS_H


/* System includes */
#include <stdint.h>

/* Local includes */
#include <client/state.h>

/**
 * @brief Window properties
 *
 * Encapsulates various properties of a client: state, layering
 * behavior, and any applicable flags.
 */
struct client_properties_s {
    uint16_t state;      /**< State (maximized, iconified,...) */
    uint16_t layer;      /**< Layer (above, normal, below) */
    uint16_t flags;      /**< Flags (hidden, sticky, focusable,...) */
    uint16_t type;       /**< Type (normal, notification...) */
    uint16_t operation;  /**< Operation (moving, resizing...) */
    uint16_t focusing;   /**< Focusing (focused, unfocused) */
};


#endif  /* ! CLIENT_PROPS_H */
