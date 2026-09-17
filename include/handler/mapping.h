/**
 * @file handler/mapping.h
 *
 * @brief X @c MappingNotify (keyboard mapping change) event handler
 *
 * Split out of @c handler.h, alongside its sibling @c handler headers,
 * so a file that only needs this one event's handler does not also pull
 * in, and rebuild against, every other unrelated one declared alongside
 * it.
 *
 * @see @c handler.h
 *
 * @ingroup handler
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef HANDLER_MAPPING_H
#define HANDLER_MAPPING_H


/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Handle a @c MAPPING_NOTIFY event
 *
 * Refreshes the cached keyboard-mapping table and re-establishes all
 * passive key and button grabs.
 *
 * @param keysyms XCB key-symbols table to refresh
 * @param stages  All managed stages
 * @param event   Mapping notify event
 * @param cfg     Active configuration
 *
 * @note Complexity: @e O(k * s), where @e k is the number of bindings
 *       and @e s is the number of stages
 */
void handler_mapping_notify(xcb_key_symbols_t *keysyms,
        list_td *stages, xcb_mapping_notify_event_t *event,
        const config_td *cfg);


#endif  /* ! HANDLER_MAPPING_H */
