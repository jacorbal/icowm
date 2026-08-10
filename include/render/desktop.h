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
#include <client.h>
#include <config.h>
#include <desktop.h>


/**
 * @brief Draw the background of a desktop
 *
 * @param desktop Pointer to the desktop to draw
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to draw background
 *
 * @note The root window's background pixmap (see
 *       @c desktop_invalidate_background_pixmap_cache) is resolved
 *       once and cached from then on, so this is @e O(1) after the
 *       first call rather than the handful of round trips to the X
 *       server a naive re-resolve on every call would cost
 * @note Complexity: @e O(1)
 */
int desktop_render_background(desktop_td *desktop);

/**
 * @brief Invalidate the cached root window background pixmap
 *
 * Call this whenever one of the (several, mutually exclusive)
 * conventions a wallpaper-setting tool might use to publish its own
 * background pixmap on the root window changes (see
 * @c s_get_root_background_pixmap in render/desktop.c for the exact
 * property names watched), so the next @c desktop_render_background
 * call re-resolves it instead of continuing to draw whatever was
 * cached from before the change.
 *
 * @note Complexity: @e O(1)
 */
void desktop_invalidate_background_pixmap_cache(void);

/**
 * @brief Recognize whether an atom is one of the root window
 *        background pixmap properties this module watches
 *
 * For the @c PropertyNotify handler in handler/focus.c to check a
 * changed atom against, so it can call
 * @c desktop_invalidate_background_pixmap_cache only when the change
 * is actually relevant, rather than on every root window property
 * change regardless of which one it was (many of which, including
 * ones icowm's own EWMH state syncing writes to the root window
 * itself, have nothing to do with the background pixmap at all).
 *
 * @param connection XCB connection, used to intern the candidate atom
 *                   names the first time this or
 *                   @c desktop_render_background is called, whichever
 *                   comes first; a no-op on every call after that
 * @param atom       Atom to check
 *
 * @return @c true if @p atom is one of the candidate background
 *         pixmap properties
 *
 * @note Complexity: @e O(1)
 */
bool desktop_property_is_background_pixmap(xcb_connection_t *connection,
        xcb_atom_t atom);

/**
 * @brief Draw all clients on a desktop
 *
 * Iterates through all clients in the desktop's stacking list and
 * configures their geometry.  Windows are only mapped (made visible)
 * when @p is_current is @c true; for a desktop that is not the one
 * currently displayed on its surface, only geometry/stacking is updated
 * so that a stale full-render pass (triggered by an unrelated
 * @p is_outdated flag, e.g., after moving/resizing a client) cannot
 * undo an explicit @a surface_clients_hide and make a client reappear
 * on top of the desktop the user actually switched to.
 *
 * @param desktop    Pointer to the desktop to draw
 * @param is_current Whether @p desktop is the surface's currently
 *                   displayed desktop; when @c false, clients are not
 *                   (re-)mapped, only their geometry is updated
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to draw clients
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 */
int desktop_render_clients(desktop_td *desktop, bool is_current);

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
 * @brief Flush drawing operations to the X server
 *
 * Sends all accumulated drawing commands to the X server to make the
 * changes visible on screen.
 *
 * @param desktop Pointer to the desktop
 *
 * @note Complexity: @e O(1)
 */
void desktop_render_flush(desktop_td *desktop);

/**
 * @brief Draw the buttons configured in @c window.titlebar.buttons on
 *        a titlebar window
 *
 * Draws exactly the buttons in @p left (before the window title) and
 * @p right (after the window title), at the positions
 * @c client_titlebar_layout already computed for them.
 *
 * The position is never recomputed on by this function on its own, so
 * it can never disagree with the click hit-test, which uses the same
 * computed layout.  The fill color for most buttons is taken from
 * @p theme: @c window.active.color.foreground when @p is_focused is
 * @c true, @c window.inactive.color.foreground otherwise; the pin and
 * layer buttons instead reflect their own state (sticky/non-normal
 * layer) regardless of focus; maximize and fullscreen fall back to the
 * background color when @p can_maximize is @c false.
 *
 * @param connection   Active XCB connection
 * @param titlebar     XCB window identifier of the titlebar
 * @param btn_y        Y position every button shares, from
 *                     @c client_titlebar_layout
 * @param left         Left-side button layout from
 *                     @c client_titlebar_layout
 * @param left_n       Number of entries in @p left
 * @param right        Right-side button layout from
 *                     @c client_titlebar_layout
 * @param right_n      Number of entries in @p right
 * @param is_focused   Whether the owning client is currently focused
 * @param is_sticky    Whether the owning client has the sticky flag set
 * @param is_layered   Whether the client layer is above or below normal
 * @param can_maximize Whether the maximize button is enabled
 * @param theme        Pointer to the theme providing button colors
 *
 * @note Complexity: @e O(n), where @e n is @p left_n + @p right_n
 */
void desktop_draw_titlebar_buttons(xcb_connection_t *connection,
        xcb_window_t titlebar, int16_t btn_y,
        const struct titlebar_button_layout_s *left,
        uint8_t left_n,
        const struct titlebar_button_layout_s *right,
        uint8_t right_n,
        bool is_focused, bool is_sticky, bool is_layered,
        bool can_maximize, const struct config_theme_s *theme);

/**
 * @brief Repaint a titlebar's background, text, and buttons
 *
 * The single place that does this: called from every titlebar repaint
 * path in the codebase (the render pass's "geometry changed" and "only
 * focus changed" branches, and the @c Expose-event handler), so none of
 * them can ever end up drawing the title or buttons differently from
 * one another. Computes the button layout itself via
 * @c client_titlebar_layout and draws the title text through
 * @c s_titlebar_draw_title (alignment- and width-aware, so a title too
 * long for the space the buttons leave is truncated rather than drawn
 * underneath them) before calling
 * @c desktop_draw_titlebar_buttons.  A no-op if @p client has no
 * titlebar window.
 *
 * @param connection Active XCB connection
 * @param client     Client whose titlebar is to be repainted
 * @param is_focused Whether @p client is currently focused (selects
 *                   active vs inactive colors and font)
 * @param inner_w    Width available for the titlebar (the frame's
 *                   width minus its left/right decoration extents)
 * @param title_h    Titlebar height in pixels
 * @param theme      Theme providing colors, font, and titlebar layout
 *
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
