/**
 * @file systray/icon.h
 *
 * @brief Systray docked-icon self-requests: an icon resizing or
 *        mapping itself, reaching the window manager through the
 *        tray's substructure redirect
 *
 * Split out of @c systray.h, alongside @c systray/handle.h and
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

#ifndef SYSTRAY_ICON_H
#define SYSTRAY_ICON_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Query whether @p window is a currently docked icon, and if
 *        so, force it back to the tray's configured icon size
 *
 * Meant to be called from the @c ConfigureRequest handler for any
 * window not otherwise recognized as a managed client: a docked
 * icon's resize attempt on itself reaches the window manager as a
 * @c ConfigureRequest only because the tray window now sets
 * @c XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT; without this function
 * actively overriding it back to @p theme.systray.pixmap.size, that
 * redirect alone would just let the request through unchanged, which
 * is no better than not redirecting at all.
 *
 * @param window Window to test
 *
 * @return Status of the operation
 * @retval  true if @p window was a docked icon (its size was just
 *               forced back, and the caller should treat the request as
 *               fully handled)
 * @retval false otherwise (the caller should fall through to its normal
 *               handling)
 *
 * @note Complexity: @e O(n), where @e n is the number of docked icons
 */
bool systray_icon_size_enforce(xcb_window_t window);

/**
 * @brief Query whether @p window is a currently docked icon, and if
 *        so, grant or refuse its own request to map itself
 *
 * Meant to be called from the @c MapRequest handler for any window
 * not otherwise recognized as a managed client: a docked icon calling
 * @c XMapWindow on itself, rather than waiting for the embedder as
 * @c XEMBED intends, reaches the window manager as a @c MapRequest
 * only because the tray window now sets
 * @c XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT.  Without this function,
 * that redirect would leave the request unrecognized, the icon window
 * would fall through to the normal top-level adoption path, and end
 * up managed as a brand-new decorated client instead of staying
 * a plain docked icon.  The request is honored or refused according
 * to the icon's own @c _XEMBED_INFO, the same authority
 * @a systray_handle_property_notify already defers to.
 *
 * @param window Window to test
 *
 * @return Status of the operation
 * @retval  true if @p window was a docked icon (its map request was
 *               just granted or refused, and the caller should treat
 *               the request as fully handled)
 * @retval false otherwise (the caller should fall through to its normal
 *               handling)
 *
 * @note Complexity: @e O(n), where @e n is the number of docked icons
 */
bool systray_icon_map_request(xcb_window_t window);


#endif  /* ! SYSTRAY_ICON_H */
