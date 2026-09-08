/**
 * @file render/desktop.h
 *
 * @brief Desktop render pass orchestration functions
 *
 * @defgroup render Rendering and repaint
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_DESKTOP_H
#define RENDER_DESKTOP_H


/* System includes */
#include <stdbool.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Render, position, and decorate a single already-non-hidden
 *        client during a stacking-order render pass
 *
 * Applies the client's border width (only when it actually changed, to
 * avoid needless server round trips), maps or unmaps its
 * frame/titlebar/content window as appropriate for whether @p desktop
 * is the surface's currently displayed one, and either reconfigures its
 * full geometry and decoration (when @c is_outdated) or, more cheaply,
 * only refreshes focus-sensitive decoration colors (when only
 * @p desktop's @c is_focus_dirty changed).
 *
 * Meant to be called directly for one specific client outside of an
 * ordinary full @a desktop_render_clients pass, e.g., by
 * @c policy/urgency.c to repaint just the urgent client(s) on an urgent
 * client's blink-phase change, without forcing every other client
 * on the same desktop to repaint along with it.
 *
 * @param desktop    Desktop the client belongs to
 * @param client     Client to render; assumed non-null and not
 *                   currently hidden
 * @param is_current Whether @p desktop is the surface's currently
 *                   displayed desktop
 *
 * @note Complexity: @e O(1)
 */
void desktop_render_one_client(desktop_td *desktop,
        client_td *client, bool is_current);

/**
 * @brief Full desktop render
 *
 * Clears the desktop and redraws everything, i.e., background and
 * clients.  Called when the desktop needs a complete refresh.
 *
 * @param desktop    Pointer to the desktop to render
 * @param is_current Whether @p desktop is the surface's currently
 *                   displayed desktop; forwarded to
 *                   @a desktop_render_clients so that clients on
 *                   a desktop that is not currently shown are never
 *                   (re-)mapped by this general refresh path
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to render desktop
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
int desktop_render_full(desktop_td *desktop, bool is_current);


#endif  /* ! RENDER_DESKTOP_H */
