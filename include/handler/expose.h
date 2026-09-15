/**
 * @file handler/expose.h
 *
 * @brief X @c Expose event handler
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

#ifndef HANDLER_EXPOSE_H
#define HANDLER_EXPOSE_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Handle an @c EXPOSE event for decoration repaints
 *
 * Repaints the info popup, cycle menu, icon window captions, and client
 * titlebars as needed.  Only the final event in a sequence
 * @c (count == 0) triggers a repaint.
 *
 * @param connection XCB connection
 * @param surfaces   All managed surfaces
 * @param event      Expose event
 * @param cfg        Active configuration
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces
 */
void handler_expose(xcb_connection_t *connection,
        list_td *surfaces, xcb_expose_event_t *event,
        const config_td *cfg);


#endif  /* ! HANDLER_EXPOSE_H */
