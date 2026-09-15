/**
 * @file systray/handle.h
 *
 * @brief Systray event routing: client messages, destroyed icons,
 *        property changes, and surface resizes
 *
 * Split out of @c systray.h, alongside @c systray/icon.h and
 * @c systray/clock.h, so a file that only needs one of these does not
 * also pull in, and rebuild against, every other unrelated concern
 * declared alongside it.
 *
 * @see @c systray.h
 *
 * @ingroup systray
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SYSTRAY_HANDLE_H
#define SYSTRAY_HANDLE_H


/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * XEMBED opcode sent to a newly docked icon (@c XEMBED_EMBEDDED_NOTIFY)
 */
#define SYSTRAY_XEMBED_EMBEDDED_NOTIFY (0u)

/**
 * @c _NET_SYSTEM_TRAY_OPCODE opcode requesting an icon be docked
 */
#define SYSTRAY_OPCODE_REQUEST_DOCK (0u)

/**
 * @c XEMBED_MAPPED bit of the @c flags field in @c _XEMBED_INFO,
 * signaling that the icon wants to be shown
 */
#define SYSTRAY_XEMBED_MAPPED (1u << 0)


/**
 * @brief Handle a @c ClientMessage addressed to the tray window
 *
 * Recognizes @c _NET_SYSTEM_TRAY_OPCODE messages with the
 * @c SYSTEM_TRAY_REQUEST_DOCK opcode and docks the requested icon
 * window.  Other opcodes (balloon messages) are acknowledged as
 * ignored.
 *
 * @param wm    Window manager state
 * @param event Incoming @c ClientMessage event
 *
 * @note Events for a window other than the tray's are ignored
 * @note Complexity: @e O(1)
 */
void systray_handle_client_message(wm_td *wm,
        const xcb_client_message_event_t *event);

/**
 * @brief Handle a docked icon window being destroyed
 *
 * Removes @p window from the tray's icon list, if present, and reflows
 * the remaining icons.  A no-op if @p window is not currently docked.
 *
 * @param wm     Window manager state
 * @param window Destroyed window
 *
 * @note Complexity: @e O(n), where @e n is the number of docked icons
 */
void systray_handle_destroy(wm_td *wm, xcb_window_t window);

/**
 * @brief Handle a property change on a docked icon window
 *
 * Only @c _XEMBED_INFO is of interest: when its @c XEMBED_MAPPED flag
 * bit changes after the icon was already docked, the icon is shown or
 * hidden to match.  A no-op for any other property, or for a window
 * that is not currently docked.
 *
 * @param wm    Window manager state
 * @param event Incoming @c PropertyNotify event
 *
 * @note Complexity: @e O(n), where @e n is the number of docked icons
 */
void systray_handle_property_notify(const wm_td *wm,
        const xcb_property_notify_event_t *event);

/**
 * @brief Reposition the tray dock window for its surface's current size
 *
 * Called after a surface resize (e.g., an XRandR screen-change) so the
 * tray stays pinned to its configured corner.
 *
 * @param wm Window manager state
 *
 * @note Complexity: @e O(1)
 */
void systray_handle_surface_resize(wm_td *wm);


#endif  /* ! SYSTRAY_HANDLE_H */
