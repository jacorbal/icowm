/**
 * @file render/desktop.h
 *
 * @brief Desktop rendering and drawing functions
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
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <types/handles.h>
#include <config.h>


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
 * @see @a desktop_background_pixmap_cache_invalidate
 */
int desktop_render_background(desktop_td *desktop);

/**
 * @brief Invalidate the cached root window background pixmap
 *
 * Call this whenever one of the (several, mutually exclusive)
 * conventions a wallpaper-setting tool might use to publish its own
 * background pixmap on the root window changes, so the next
 * @a desktop_render_background call re-resolves it instead of
 * continuing to draw whatever was cached from before the change.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a s_get_root_background_pixmap in @c render/desktop.c for the
 *      exact property names watched
 */
void desktop_background_pixmap_cache_invalidate(void);

/**
 * @brief Recognize whether an atom is one of the root window background
 *        pixmap properties this module watches
 *
 * For the @c PropertyNotify handler in handler/focus.c to check
 * a changed atom against, so it can call
 * @a desktop_background_pixmap_cache_invalidate only when the change is
 * actually relevant, rather than on every root window property change
 * regardless of which one it was (many of which, including ones icowm's
 * own EWMH state syncing writes to the root window itself, have nothing
 * to do with the background pixmap at all).
 *
 * @param connection XCB connection, used to intern the candidate atom
 *                   names the first time this or
 *                   @a desktop_render_background is called, whichever
 *                   comes first; a no-op on every call after that
 * @param atom       Atom to check
 *
 * @return @c true if @p atom is one of the candidate background pixmap
 *         properties
 *
 * @note Complexity: @e O(1)
 */
bool desktop_property_is_background_pixmap(xcb_connection_t *connection,
        xcb_atom_t atom);

/**
 * @brief Render, position, and decorate a single already-non-hidden
 *        client during a stacking-order render pass
 *
 * Applies the client's own border width (only when it actually changed,
 * to avoid needless server round trips), maps or unmaps its
 * frame/titlebar/content window as appropriate for whether @p desktop
 * is the surface's currently displayed one, and either reconfigures its
 * full geometry and decoration (when @c is_outdated) or, more cheaply,
 * only refreshes focus-sensitive decoration colors (when
 * only @p desktop's own @c is_focus_dirty changed).
 *
 * Meant to be called directly for one specific client outside of an
 * ordinary full @a desktop_render_clients pass, e.g., by
 * @c policy/urgency.c to repaint just the urgent client(s) on an urgent
 * client's own blink-phase change, without forcing every other client
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

/**
 * @brief Repaint a titlebar's background, text, and buttons
 *
 * The single place that does this: called from every titlebar repaint
 * path in the codebase (the render pass's "geometry changed" and "only
 * focus changed" branches, and the @c Expose-event handler), so none of
 * them can ever end up drawing the title or buttons differently from
 * one another.  Computes the button layout itself via
 * @a client_titlebar_layout and draws the title text through
 * @a s_titlebar_draw_title (alignment-aware & width-aware, so a title
 * too long for the space the buttons leave is truncated rather than
 * drawn underneath them) before calling
 * @a s_desktop_titlebar_buttons_draw.
 *
 * @param connection Active XCB connection
 * @param client     Client whose titlebar is to be repainted
 * @param is_focused Whether to paint with the active or inactive
 *                   font and colors.  Ordinarily reflects whether
 *                   @p client actually holds input focus, except
 *                   for an urgent client mid-blink (see @c policy/
 *                   urgency.h), where it is deliberately the
 *                   opposite of the real focus state for one half of
 *                   the blink cycle: font and colors swap together,
 *                   so the titlebar reads as clearly attention-
 *                   grabbing rather than merely looking like focus
 *                   flickered
 * @param inner_w    Width available for the titlebar (the frame's
 *                   width minus its left/right decoration extents)
 * @param title_h    Titlebar height in pixels
 * @param theme      Theme providing colors, font, and titlebar layout
 *
 * @note A no-op if @p client has no titlebar window
 * @note Complexity: @e O(n), where @e n is the number of configured
 *       titlebar buttons plus the length of the client's title
 */
void desktop_repaint_titlebar_content(xcb_connection_t *connection,
        client_td *client, bool is_focused, uint16_t inner_w,
        uint16_t title_h, const struct config_theme_s *theme);

/**
 * @brief Repaint the frame window decoration for a client
 *
 * Refreshes the frame background/border and redraws the corner resize
 * grips when they are supposed to be visible for the current client
 * state.
 *
 * @param connection       Active XCB connection
 * @param client           Client whose frame decoration will be
 *                         repainted
 * @param use_active_style Whether to use the active theme colors
 * @param theme            Theme providing frame and grip colors
 *
 * @note Complexity: @e O(1)
 */
void desktop_repaint_frame_decoration(xcb_connection_t *connection,
        const client_td *client, bool use_active_style,
        const struct config_theme_s *theme);


#endif  /* ! RENDER_DESKTOP_H */
