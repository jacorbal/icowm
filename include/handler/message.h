/**
 * @file handler/message.h
 *
 * @brief X @c ClientMessage event handler
 *
 * Split out of @c handler.h, alongside its sibling @c handler headers,
 * so a file that only needs this event's handler does not also pull in,
 * and rebuild against, every other unrelated one declared alongside it.
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

#ifndef HANDLER_MESSAGE_H
#define HANDLER_MESSAGE_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief ICCCM @c WM_CHANGE_STATE @c IconicState value
 */
#define ICCCM_ICONIC_STATE (3)


/**
 * @brief Handle a @c CLIENT_MESSAGE event
 *
 * Dispatches EWMH and ICCCM client-message requests from applications
 * (fullscreen, maximize, close, desktop switch, iconify, &c.) to the
 * appropriate command functions so that they are honored by the window
 * manager.
 *
 * @param wm    Window manager state
 * @param event Client message event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients
 */
void handler_message_client(wm_td *wm,
        xcb_client_message_event_t *event);


#endif  /* ! HANDLER_MESSAGE_H */
