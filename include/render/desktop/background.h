/**
 * @file render/desktop/background.h
 *
 * @brief Desktop root window background rendering functions
 *
 * @ingroup render
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef RENDER_DESKTOP_BACKGROUND_H
#define RENDER_DESKTOP_BACKGROUND_H


/* System includes */
#include <stdbool.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Type includes */
#include <types/handles.h>


/**
 * @brief Draw the background of a desktop
 *
 * @param desktop Pointer to the desktop to draw
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to draw background
 *
 * @note The root window's background pixmap is resolved once and cached
 *       from then on, so this is @e O(1) after the first call rather
 *       than the handful of round trips to the X server a naive
 *       re-resolve on every call would cost
 * @note Complexity: @e O(1)
 *
 * @see @a render_desktop_background_cache_invalidate
 */
int render_desktop_background_render(desktop_td *desktop);

/**
 * @brief Invalidate the cached root window background pixmap
 *
 * Cheap and always correct, even though only one screen's own property
 * actually changed, since which one that was is not known at this call
 * site (@c handler/focus.c, a generic @c PropertyNotify handler not
 * otherwise concerned with which screen a client's root belongs to) and
 * this only ever runs on the comparatively rare event of an external
 * wallpaper tool actually changing something, not on every render pass.
 *
 * Call this whenever one of the (several, mutually exclusive)
 * conventions a wallpaper-setting tool might use to publish its
 * background pixmap on the root window changes, so the next
 * @a render_desktop_background_render call re-resolves it instead of
 * continuing to draw whatever was cached from before the change.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a s_get_root_background_pixmap in
 *      @c render/desktop/background.c for the exact property names
 *      watched
 */
void render_desktop_background_cache_invalidate(void);

/**
 * @brief Recognize whether an atom is one of the root window background
 *        pixmap properties this module watches
 *
 * For the @c PropertyNotify handler in handler/focus.c to check
 * a changed atom against, so it can call
 * @a render_desktop_background_cache_invalidate only when the change is
 * actually relevant, rather than on every root window property change
 * regardless of which one it was (many of which, including ones IcoWM's
 * own EWMH state syncing writes to the root window itself, have nothing
 * to do with the background pixmap at all).
 *
 * @param connection XCB connection, used to intern the candidate atom
 *                   names the first time this or
 *                   @a render_desktop_background_render is called,
 *                   whichever comes first; a no-op on every call after
 *                   that
 * @param atom       Atom to check
 *
 * @return @c true if @p atom is one of the candidate background pixmap
 *         properties
 *
 * @note Complexity: @e O(1)
 */
bool render_desktop_background_property_is_pixmap(
        xcb_connection_t *connection, xcb_atom_t atom);


#endif  /* ! RENDER_DESKTOP_BACKGROUND_H */
